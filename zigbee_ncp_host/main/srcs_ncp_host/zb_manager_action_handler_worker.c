#include "zb_manager_action_handler_worker.h"
#include "zb_manager_devices.h"
#include "zb_manager_tuya_dp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "zb_manager_ncp_host.h"
#include "esp_log.h"
#include "web_server.h"
#include "zb_manager_devices.h"
#include "quirks_storage.h"
#include "ha_mqtt_publisher.h"
#include "zb_manager_rules.h"

static const char *TAG = "ZB_ACTION_WORKER";
static uint32_t last_log_ms = 0; // для worker тест утечек

#define ZB_ACTION_STACK_SIZE     4096
#define ZB_ACTION_TASK_PRIORITY  8

static TaskHandle_t s_action_task_handle = NULL;
static QueueHandle_t s_action_queue = NULL;

typedef struct {
    int32_t event_id;
    void *event_data;
    size_t data_size;
} zb_action_msg_t;



// === POOL for report_attr_resp ===
#define REPORT_ATTR_POOL_SIZE 2  // Хватит на 2 одновременных report'ов

static zb_manager_cmd_report_attr_resp_message_t g_report_resp_pool[REPORT_ATTR_POOL_SIZE];
static bool g_report_resp_pool_used[REPORT_ATTR_POOL_SIZE] = {0};
static uint8_t g_report_attr_value_pool[REPORT_ATTR_POOL_SIZE][64]; // буферы до 32 байт (можно увеличить)
static const char *REPORT_POOL_TAG = "POOL_REPORT_ATTR";




/**
 * @brief Получить свободный объект из пула
 */
static zb_manager_cmd_report_attr_resp_message_t* report_attr_pool_get(void)
{
    for (int i = 0; i < REPORT_ATTR_POOL_SIZE; i++) {
        if (!g_report_resp_pool_used[i]) {
            g_report_resp_pool_used[i] = true;
            zb_manager_cmd_report_attr_resp_message_t *msg = &g_report_resp_pool[i];

            // Обнуляем основные поля
            memset(msg, 0, sizeof(zb_manager_cmd_report_attr_resp_message_t));

            // Указываем на выделенный буфер значения
            msg->attr.attr_value = g_report_attr_value_pool[i];

            ESP_LOGW(REPORT_POOL_TAG, "✅ Got item %d from pool", i);
            return msg;
        }
    }

    ESP_LOGW(REPORT_POOL_TAG, "❌ Pool FULL! Consider increasing REPORT_ATTR_POOL_SIZE");
    return NULL;
}

/**
 * @brief Вернуть объект в пул
 * (attr_value не копируется — он в статическом буфере)
 */
static void report_attr_pool_put(zb_manager_cmd_report_attr_resp_message_t* msg)
{
    if (!msg) return;

    for (int i = 0; i < REPORT_ATTR_POOL_SIZE; i++) {
        if (&g_report_resp_pool[i] == msg) {
            if (!g_report_resp_pool_used[i]) {
                ESP_LOGW(REPORT_POOL_TAG, "⚠️ Double free detected for pool item %d!", i);
                return;
            }

            // Не делаем free(attr_value) — он в пуле
            memset(&g_report_resp_pool[i], 0, sizeof(zb_manager_cmd_report_attr_resp_message_t));
            g_report_resp_pool[i].attr.attr_value = g_report_attr_value_pool[i]; // восстанавливаем указатель

            g_report_resp_pool_used[i] = false;
            ESP_LOGW(REPORT_POOL_TAG, "🗑 Returned item %d to pool", i);
            return;
        }
    }

    ESP_LOGE(REPORT_POOL_TAG, "❌ Attempt to free NON-POOL pointer!");
}


void zb_manager_process_custom_cluster_command(zb_action_msg_t *msg)
{
    zb_manager_custom_cluster_report_message_t *rep = 
        (zb_manager_custom_cluster_report_message_t *)msg->event_data;

    ESP_LOGI(TAG, "🔧 Processing custom cluster: short=0x%04x, cluster=0x%04x, cmd=0x%02x",
             rep->short_addr, rep->cluster_id, rep->command_id);

    // === 🔄 Проверяем: это TUYA-like кластер? ===
    /*if (rep->cluster_id != 0xEF00) {
        ESP_LOGD(TAG, "❌ Not 0xEF00 cluster → ignoring");
        return;
    }*/

    // === 🔍 Находим устройство ===
    device_custom_t *dev = zb_manager_find_device_by_short(rep->short_addr);
    if (!dev) {
        ESP_LOGW(TAG, "❌ Device 0x%04x not found", rep->short_addr);
        return;
    }

    // === 📦 Получаем model_id из Basic Cluster ===
    zigbee_manager_basic_cluster_t *basic = dev->server_BasicClusterObj;
    if (!basic) {
        ESP_LOGW(TAG, "❌ No Basic Cluster for 0x%04x", rep->short_addr);
        return;
    }

    const char *model_id = basic->model_identifier;
    if (!model_id || strlen(model_id) == 0) {
        ESP_LOGW(TAG, "❌ Empty model_id for 0x%04x", rep->short_addr);
        return;
    }

    ESP_LOGI(TAG, "🔍 Model ID: '%s'", model_id);

    // === 🧩 Ищем квирк ===
    cJSON *quirk = quirks_get_quirk_by_model(model_id);
    if (!quirk) {
        ESP_LOGW(TAG, "❌ No quirk for model_id='%s' → treating as raw", model_id);
        // Можно сохранить сырые данные
        return;
    }

    ESP_LOGI(TAG, "✅ Quirk found for '%s' → parsing DP", model_id);

    uint8_t *data = rep->data;
    uint16_t len = rep->data_len;

    ESP_LOG_BUFFER_HEX_LEVEL("DATA in zb_manager_process_custom_cluster_command", rep->data, rep->data_len, ESP_LOG_INFO);

    if (len < 4) {
        ESP_LOGW(TAG, "❌ Data too short: %u", len);
        return;
    }

    uint8_t dp_count = data[0];
    uint16_t offset = 1;

    for (int i = 0; i < dp_count && offset + 3 < len; i++) {
        uint8_t dp_id = data[offset + 0];
        uint8_t dp_type = data[offset + 1];
        uint16_t dp_len = data[offset + 2] | (data[offset + 3] << 8);
        offset += 4;

        if (offset + dp_len > len) {
            ESP_LOGW(TAG, "❌ DP %d: not enough data", dp_id);
            break;
        }

        uint8_t *dp_value = &data[offset];
        offset += dp_len;

        ESP_LOGI(TAG, "📄 DP[%d]: type=%d, len=%u", dp_id, dp_type, dp_len);
        ESP_LOG_BUFFER_HEX_LEVEL("DP Value", dp_value, dp_len, ESP_LOG_INFO);

        // === 🔎 Есть ли описание DP в квирке? ===
        cJSON *dp_info = quirks_get_dp_info(quirk, dp_id);
        if (!dp_info) {
            ESP_LOGW(TAG, "⚠️ No definition for DP %d", dp_id);
            continue;
        }

        const char *role = cJSON_GetStringValue(cJSON_GetObjectItem(dp_info, "role"));
        double scale = cJSON_GetNumberValue(cJSON_GetObjectItem(dp_info, "scale"));

        if (!role) continue;

        // === 🌡️ Температура (тип 0x02, 4 байта) ===
        if (strcmp(role, "temperature") == 0 && dp_type == 0x02 && dp_len == 4) {
            int32_t raw = dp_value[0] | (dp_value[1] << 8) | (dp_value[2] << 16) | (dp_value[3] << 24);
            float temp = (float)raw * scale;
            ESP_LOGI(TAG, "🌡️ Temperature: %.2f °C", temp);
            // Здесь можно: dev->temperature = temp;
        }
        // === 💧 Влажность ===
        else if (strcmp(role, "humidity") == 0 && dp_type == 0x02 && dp_len == 4) {
            int32_t raw = dp_value[0] | (dp_value[1] << 8) | (dp_value[2] << 16) | (dp_value[3] << 24);
            float hum = (float)raw * scale;
            ESP_LOGI(TAG, "💧 Humidity: %.2f %%", hum);
            // dev->humidity = hum;
        }
        // === 🔄 Другие роли можно добавить: switch, brightness, mode и т.д. ===
        else {
            ESP_LOGW(TAG, "⚠️ Unsupported role='%s' for DP %d", role, dp_id);
        }
    }

    // === 🌐 Уведомляем веб-интерфейс ===
    //ws_notify_device_update(dev->short_addr);
}


//ram control
static int report_count = 0;
static uint32_t min_ram = 0xFFFFFFFF;

static void zb_action_worker_task(void *pvParameters)
{
    zb_action_msg_t msg;
    ESP_LOGI(TAG, "Action worker started");
    

    while (1) {
        if (xQueueReceive(s_action_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }

        if (!msg.event_data) {
            free(msg.event_data);
            continue;
        }
        uint32_t ram = esp_get_free_heap_size();
        if (ram < min_ram) min_ram = ram;
        if (report_count == 0)
        {
            ESP_LOGW(TAG, "RAM control zb_action_worker_task on report_count == 0 %lu", min_ram);
        }
        report_count++;
        ESP_LOGW(TAG, "REPORT_COUNT %d", report_count);

        //ESP_LOGW(TAG, "RAM control zb_action_worker_task on BEGIN %lu", esp_get_free_heap_size());

        int64_t start = esp_timer_get_time();
        switch (msg.event_id) {
           

        case ZB_ACTION_ATTR_READ_RESP: {
            uint8_t *input_copy = (uint8_t *)msg.event_data;
            size_t total_len = msg.data_size;

            if (!input_copy || total_len < sizeof(esp_zb_zcl_cmd_info_t) + 1) {
                ESP_LOGE(TAG, "❌ Invalid input data in ZB_ACTION_ATTR_READ_RESP");
                break;
            }

            ESP_LOGI(TAG, "ZB_ACTION_ATTR_READ_RESP: processing raw buffer of size %zu", total_len);
            ESP_LOG_BUFFER_HEX_LEVEL("NCP RAW", input_copy, total_len, ESP_LOG_INFO);

            zb_manager_cmd_read_attr_resp_message_t* read_resp = NULL;
            read_resp = calloc(1, sizeof(zb_manager_cmd_read_attr_resp_message_t));
            if (!read_resp) {
                ESP_LOGE(TAG, "❌ Failed to allocate read_resp");
                break;
            }

            // Копируем info
            esp_zb_zcl_cmd_info_t* cmd_info = (esp_zb_zcl_cmd_info_t*)input_copy;
            memcpy(&read_resp->info, cmd_info, sizeof(esp_zb_zcl_cmd_info_t));
            ESP_LOGI(TAG, "ZB_ACTION_ATTR_READ_RESP sizeof(esp_zb_zcl_cmd_info_t) = %d",sizeof(esp_zb_zcl_cmd_info_t));

            // Читаем attr_count
            uint8_t attr_count = input_copy[sizeof(esp_zb_zcl_cmd_info_t)];
            read_resp->attr_count = attr_count;
            ESP_LOGW(TAG, "Attribute count: %u", attr_count);

            if (attr_count == 0) {
                read_resp->attr_arr = NULL;
            } else {
                read_resp->attr_arr = calloc(attr_count, sizeof(zb_manager_attr_t));
                if (!read_resp->attr_arr) {
                    ESP_LOGE(TAG, "❌ Failed to allocate attr_arr for %u attributes", attr_count);
                    free(read_resp);
                    break;
                }
            }

            // Указатель на начало данных атрибутов
            uint8_t *pointer = input_copy + sizeof(esp_zb_zcl_cmd_info_t) + 1; // пропускаем info + attr_count

            bool parse_failed = false;

            for (uint8_t i = 0; i < attr_count; i++) {
                zb_manager_read_resp_attr_t* attr = &read_resp->attr_arr[i];

                // Минимум: status(1) + id(2) + type(1) + len(1) = 5 байт
                if (pointer + 5 > input_copy + total_len) {
                    ESP_LOGE(TAG, "❌ Buffer too short for attribute header (index=%u)", i);
                    parse_failed = true;
                    break;
                }

                // === ЧИТАЕМ ПОРЯДОК ПО ЗАПИСИ В NCP ===
                attr->status    = *pointer++;                    // status
                attr->attr_id   = *(uint16_t*)pointer; pointer += 2;
                attr->attr_type = *pointer++;                    // attr_type
                attr->attr_len  = *pointer++;                    // data_size

                ESP_LOGI(TAG, "Parsing attr[%u]: id=0x%04x, status=0x%02x, type=0x%02x, len=%u",
                        i, attr->attr_id, attr->status, attr->attr_type, attr->attr_len);

                if (attr->status != ESP_ZB_ZCL_STATUS_SUCCESS) {
                    ESP_LOGW(TAG, "Attr 0x%04x read failed with status: 0x%02x", attr->attr_id, attr->status);
                    attr->attr_value = NULL;
                    continue; // не читаем значение
                }

                // Проверяем, хватает ли места для значения
                if (attr->attr_len > 0) {
                    if (pointer + attr->attr_len > input_copy + total_len) {
                        ESP_LOGE(TAG, "❌ Attr value overflows buffer: id=0x%04x, len=%u", attr->attr_id, attr->attr_len);
                        parse_failed = true;
                        break;
                    }

                    attr->attr_value = malloc(attr->attr_len);
                    if (!attr->attr_value) {
                        ESP_LOGE(TAG, "❌ Failed to allocate %u bytes for attr_value (id=0x%04x)", attr->attr_len, attr->attr_id);
                        parse_failed = true;
                        break;
                    }

                    memcpy(attr->attr_value, pointer, attr->attr_len);
                    pointer += attr->attr_len;

                    ESP_LOG_BUFFER_HEX_LEVEL("Attr Value", attr->attr_value, attr->attr_len, ESP_LOG_INFO);
                } else {
                    attr->attr_value = NULL;
                }
            }

            if (parse_failed) {
                ESP_LOGE(TAG, "Failed to parse attribute values → cleaning up");
                zb_manager_free_read_attr_resp_attr_array(read_resp);
                read_resp = NULL;
                break;
            }

            // Проверка адреса
            uint16_t short_addr = read_resp->info.src_address.u.short_addr;
            if (short_addr == 0xFFFF) {
                ESP_LOGW(TAG, "❌ Invalid short_addr: 0xFFFF in read response");
                zb_manager_free_read_attr_resp_attr_array(read_resp);
                read_resp = NULL;
                break;
            }

            // Общий статус команды (может быть успехом, даже если отдельные атрибуты упали)
            if (read_resp->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
                ESP_LOGW(TAG, "ZB_ACTION_ATTR_READ_RESP: overall status 0x%02x from 0x%04x", read_resp->info.status, short_addr);
                zb_manager_free_read_attr_resp_attr_array(read_resp);
                read_resp = NULL;
                break;
            }

            esp_err_t update_dev_data_result = ESP_FAIL;
            update_dev_data_result = zbm_dev_base_dev_update_from_read_response_safe(read_resp);

            //zb_manager_update_device_activity(short_addr, 10);
            
            ws_notify_device_update(read_resp->info.src_address.u.short_addr);

            zb_manager_free_read_attr_resp_attr_array(read_resp);
            read_resp = NULL;

            break;
        }

        case ZB_ACTION_ATTR_REPORT: {
            ESP_LOGI(TAG, "ZB_ACTION_ATTR_REPORT: processing");
            uint8_t *raw = (uint8_t *)msg.event_data;
            size_t len = msg.data_size;
            typedef struct {
                esp_zb_zcl_status_t status;
                esp_zb_zcl_addr_t src_address;
                uint8_t src_endpoint;
                uint8_t dst_endpoint;
                uint16_t cluster;
            } ESP_ZNSP_ZB_PACKED_STRUCT esp_ncp_zb_report_attr_t;

            typedef struct {
                uint16_t id;
                uint8_t  type;
                uint8_t  size;
            } ESP_ZNSP_ZB_PACKED_STRUCT esp_ncp_zb_attr_data_t;

            ESP_LOGI(TAG, "✅ ATTR_REPORT_EVENT: processing, len=%u", len);

            if (len < sizeof(esp_ncp_zb_report_attr_t) + sizeof(esp_ncp_zb_attr_data_t)) {
                ESP_LOGE(TAG, "ATTR_REPORT: invalid length %u", len);
                break;
            }
            
            esp_ncp_zb_report_attr_t *report_attr = (esp_ncp_zb_report_attr_t *)raw;
            esp_ncp_zb_attr_data_t *attr_data = (esp_ncp_zb_attr_data_t *)(raw + sizeof(*report_attr));

            // Проверка типа атрибута
            if (attr_data->type > 0xFF) {
                ESP_LOGE(TAG, "Invalid attr_type: 0x%04x", attr_data->type);
                break;
            }

            // Создаём resp_msg через пул
            zb_manager_cmd_report_attr_resp_message_t *rep = report_attr_pool_get();
            if (!rep) {
                ESP_LOGE(TAG, "Failed to get rep from pool");
                break;
            }
            /*zb_manager_cmd_report_attr_resp_message_t *rep = calloc(1, sizeof(zb_manager_cmd_report_attr_resp_message_t));
            if (!rep) {
                ESP_LOGE(TAG, "Failed to allocate rep");
                break;
            }*/

            rep->status = report_attr->status;
            memcpy(&rep->src_address, &report_attr->src_address, sizeof(esp_zb_zcl_addr_t));
            rep->src_endpoint = report_attr->src_endpoint;
            rep->dst_endpoint = report_attr->dst_endpoint;
            rep->cluster = report_attr->cluster;

            rep->attr.attr_id = attr_data->id;
            rep->attr.attr_type = attr_data->type;
            rep->attr.attr_len = attr_data->size;

            // Поиск устройства
            device_custom_t *dev = zbm_dev_base_find_device_by_short_safe(rep->src_address.u.short_addr);
            if (!dev) {
                ESP_LOGW(TAG, "ATTR_REPORT_EVENT: device 0x%04x not found", rep->src_address.u.short_addr);
                //zb_manager_free_report_attr_resp(rep);
                report_attr_pool_put(rep);
                rep = NULL;
                break;
            }

            if(dev->is_in_build_status !=2)
            {
                report_attr_pool_put(rep);
                rep = NULL;
                break;
            }
            
            // Проверка: не выходит ли значение за пределы буфера
            const uint8_t *value_src = raw + sizeof(*report_attr) + sizeof(*attr_data);
            const uint8_t *buffer_end = raw + len;

            if (rep->attr.attr_len > (uint32_t)(buffer_end - value_src)) {
                ESP_LOGE(TAG, "attr_len (%u) exceeds buffer", rep->attr.attr_len);
                //zb_manager_free_report_attr_resp(rep);
                report_attr_pool_put(rep);
                rep = NULL;
                break;
            }

           // Копируем значение атрибута в пул
           if (rep->attr.attr_len > 0) {
                if (rep->attr.attr_len > 32) {
                    ESP_LOGW(REPORT_POOL_TAG, "⚠️ Attr len %u > 32! Truncating", rep->attr.attr_len);
                    rep->attr.attr_len = 32;
                }
                memcpy(rep->attr.attr_value, value_src, rep->attr.attr_len);
            }

            

            if (rep->status != ESP_ZB_ZCL_STATUS_SUCCESS) {
                ESP_LOGW(TAG, "ATTR_REPORT_EVENT: status 0x%02x", rep->status);
                //zb_manager_free_report_attr_resp(rep);
                report_attr_pool_put(rep);
                rep = NULL;
                break;
            }

            // Логируем
            ESP_LOGI(TAG, "📊 ATTR_REPORT: short=0x%04x, ep=%d, cluster=0x%04x, attr_id=0x%04x, type=0x%02x, len=%d",
                    rep->src_address.u.short_addr,
                    rep->src_endpoint,
                    rep->cluster,
                    rep->attr.attr_id,
                    rep->attr.attr_type,
                    rep->attr.attr_len);

            if (rep->attr.attr_value && rep->attr.attr_len > 0) {
                ESP_LOG_BUFFER_HEX_LEVEL("Attr Value", rep->attr.attr_value, rep->attr.attr_len, ESP_LOG_INFO);
            }

            //rep заполнен, можно отправлять в update модуль
            if (zbm_dev_base_dev_update_from_report_notify_safe(rep) == ESP_OK)
            {
                ws_notify_device_update(rep->src_address.u.short_addr);
            }else {
                ESP_LOGW(TAG,"zbm_dev_base_dev_update_from_report_notify_safe(rep) != ESP_OK");
            }

            
            ESP_LOGI(TAG, "🔄 ZB_ACTION_REPORT_ATTR: free");
            //zb_manager_free_report_attr_resp(rep);
            //rep = NULL;
            report_attr_pool_put(rep);
            rep = NULL;

            break;
        }

            case ZB_ACTION_CUSTOM_CLUSTER_REPORT: {
            zb_manager_custom_cluster_report_message_t *custom_cl_rep = (zb_manager_custom_cluster_report_message_t *)msg.event_data;
            ESP_LOGI(TAG, "🔄 ZB_ACTION_CUSTOM_CLUSTER_REPORT: short=0x%04x, manuf=0x%04x, cmd=0x%02x, len=%u",
                    custom_cl_rep->short_addr, custom_cl_rep->manuf_code, custom_cl_rep->command_id, custom_cl_rep->data_len);

            // Передаём в общий обработчик
            //zb_manager_process_custom_cluster_command(&msg);

            //zb_manager_free_custom_cluster_report_message(custom_cl_rep);
            break;
        }


            case ZB_ACTION_DELAYED_NODE_DESC_REQ: {
                ESP_LOGI(TAG, "✅ DELAYED_NODE_DESC_REQ_LOCAL: processing node_desc");
                uint16_t short_addr; // = *(uint16_t *)msg.event_data;
                memcpy(&short_addr, msg.event_data, sizeof(uint16_t));
                device_custom_t *dev = NULL;      
                dev = zb_manager_find_device_by_short(short_addr);
                if (!dev) {
                    ESP_LOGW(TAG, "DELAYED_NODE_DESC_REQ_LOCAL: device 0x%04x not found", short_addr);
                    break;
                }
                //отправляем
                if (zb_manager_zdo_node_desc_req(short_addr) == ESP_OK)
                {
                    ESP_LOGI(TAG, "Node desc req sent to device 0x%04x", short_addr);
                    
                }else {
                    ESP_LOGW(TAG, "Node desc req failed to device 0x%04x", short_addr);
                }
                

                break;
            }

            case ZB_ACTION_DELAYED_BIND_REQ: {
                ESP_LOGI(TAG, "✅ ZB_ACTION_DELAYED_BIND_REQ: processing");
                delayed_bind_req_t *req = (delayed_bind_req_t *)msg.event_data;
                if (!req) {
                    ESP_LOGW(TAG, "❌ DELAYED_BIND_REQ: NULL request data");
                    break;
                }

                device_custom_t *dev = zb_manager_find_device_by_short(req->short_addr);
                if (!dev) {
                    ESP_LOGW(TAG, "❌ DELAYED_BIND_REQ: device 0x%04x not found", req->short_addr);
                    break;
                }

                // Подготавливаем параметры bind
                local_esp_zb_zdo_bind_req_param_t bind_req = {0};
                bind_req.cluster_id = req->cluster_id;
                bind_req.dst_addr_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
                memcpy(bind_req.dst_address_u.addr_long, LocalIeeeAdr, 8);
                memcpy(bind_req.src_address, dev->ieee_addr, 8);
                bind_req.src_endp = req->src_endpoint;
                bind_req.dst_endp = 0x01;  // координатор
                bind_req.req_dst_addr = req->short_addr;

                // Создаём КОПИЮ req для user_ctx (zb_manager освободит её в колбэке)
                delayed_bind_req_t *user_ctx_copy = calloc(1, sizeof(delayed_bind_req_t));
                if (!user_ctx_copy) {
                    ESP_LOGE(TAG, "❌ Failed to allocate user_ctx_copy for bind");
                    break;
                }
                memcpy(user_ctx_copy, req, sizeof(*user_ctx_copy));

                // Отправляем bind-запрос
                esp_err_t err = zb_manager_zdo_device_bind_req(&bind_req, NULL, user_ctx_copy);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "✅ BIND request sent: cluster=0x%04x, dev=0x%04x", req->cluster_id, req->short_addr);
                } else {
                    ESP_LOGE(TAG, "❌ zb_manager_zdo_device_bind_req failed: %s", esp_err_to_name(err));
                    free(user_ctx_copy);  // Освобождаем, если не отправился
                }

                // Не освобождаем `req` здесь — это делает `free(msg.event_data)` в конце
                break;
            }
            case ZB_ACTION_DELAYED_BIND_DEV_DEV_REQ: {

                ESP_LOGI(TAG, "✅ ZB_ACTION_DELAYED_BIND_DEV_DEV_REQ: processing");
                delayed_bind_dev_dev_req_t *req = (delayed_bind_dev_dev_req_t *)msg.event_data;
                if (!req) {
                    ESP_LOGW(TAG, "❌ ZB_ACTION_DELAYED_BIND_DEV_DEV_REQ: NULL request data");
                    break;
                }
                
                esp_zb_ieee_addr_t src_ieee;
                esp_zb_ieee_addr_t dst_ieee;
                if (req->src_short_addr != 0x0000)
                {
                    device_custom_t *src_dev = zbm_dev_base_find_device_by_short_safe(req->src_short_addr);
                    if (!src_dev) {
                        ESP_LOGW(TAG, "❌ ZB_ACTION_DELAYED_BIND_DEV_DEV_REQ: src_device 0x%04x not found", req->src_short_addr);
                        break;
                    }
                    memcpy(src_ieee, src_dev->ieee_addr, 8);
                }else memcpy(src_ieee, LocalIeeeAdr, 8);

                if (req->dst_short_addr != 0x0000)
                {
                    device_custom_t *dst_dev = zbm_dev_base_find_device_by_short_safe(req->dst_short_addr);
                    if (!dst_dev) {
                        ESP_LOGW(TAG, "❌ ZB_ACTION_DELAYED_BIND_DEV_DEV_REQ: dst_device 0x%04x not found", req->src_short_addr);
                        break;
                    }
                    memcpy(dst_ieee, dst_dev->ieee_addr, 8);
                }else memcpy(dst_ieee, LocalIeeeAdr, 8);

                // Подготавливаем параметры bind
                local_esp_zb_zdo_bind_req_param_t bind_req = {0};
                bind_req.cluster_id = req->cluster_id;
                bind_req.dst_addr_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
                memcpy(bind_req.dst_address_u.addr_long, dst_ieee, 8);
                memcpy(bind_req.src_address, src_ieee, 8);
                bind_req.src_endp = req->src_endpoint;
                bind_req.dst_endp = req->dst_endpoint; 
                bind_req.req_dst_addr = req->src_short_addr;

                // Создаём КОПИЮ req для user_ctx (zb_manager освободит её в колбэке)
                delayed_bind_req_t *user_ctx_copy = calloc(1, sizeof(delayed_bind_req_t));
                if (!user_ctx_copy) {
                    ESP_LOGE(TAG, "❌ Failed to allocate user_ctx_copy for bind");
                    break;
                }
                user_ctx_copy->cluster_id = req->cluster_id;
                user_ctx_copy->short_addr = req->src_short_addr;
                user_ctx_copy->src_endpoint = req->src_endpoint;
                //memcpy(user_ctx_copy, req, sizeof(*user_ctx_copy));

                // Отправляем bind-запрос
                esp_err_t err = zb_manager_zdo_device_bind_req(&bind_req, NULL, user_ctx_copy);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "✅ BIND request sent: cluster=0x%04x, to dev=0x%04x (src dev=0x%04x (ep=0x%02x) -bind-> dst dev=0x%04x (ep=0x%02x) )", req->cluster_id, req->src_short_addr, req->src_short_addr, req->src_endpoint, req->dst_short_addr, req->dst_endpoint);
                } else {
                    ESP_LOGE(TAG, "❌ zb_manager_zdo_device_bind_req failed: %s", esp_err_to_name(err));
                    free(user_ctx_copy);  // Освобождаем, если не отправился
                }

                // Не освобождаем `req` здесь — это делает `free(msg.event_data)` в конце
                break;
            }
            case ZB_ACTION_BIND_RESP: {
                ESP_LOGI(TAG, "✅ ZB_ACTION_BIND_RESP: processing");
                zb_manager_bind_resp_message_t *resp = (zb_manager_bind_resp_message_t *)msg.event_data;
                if (!resp) break;

                if (resp->status == ESP_ZB_ZDP_STATUS_SUCCESS) {
                    delayed_bind_req_t *delayed_ctx = (delayed_bind_req_t *)resp->user_ctx;
                    if (delayed_ctx) {
                        uint16_t dev_short = delayed_ctx->short_addr;
                        uint8_t ep = delayed_ctx->src_endpoint;
                        uint16_t cluster_id = delayed_ctx->cluster_id;

                        ESP_LOGI(TAG, "✅ BIND successful: cluster=0x%04x, dev=0x%04x", cluster_id, dev_short);

                        // После успешного bind — отправляем настройку reporting
                        /*delayed_configure_report_req_t *cfg_req = calloc(1, sizeof(delayed_configure_report_req_t));
                        if (cfg_req) {
                            cfg_req->short_addr = dev_short;
                            cfg_req->src_endpoint = ep;
                            cfg_req->cluster_id = cluster_id;

                            if (zb_manager_post_to_action_worker(ZB_ACTION_DELAYED_CONFIG_REPORT_REQ, cfg_req, sizeof(*cfg_req))) {
                                ESP_LOGI(TAG, "📤 Posted DELAYED_CONFIG_REPORT_REQ for cluster 0x%04x", cluster_id);
                            } else {
                                ESP_LOGE(TAG, "❌ Failed to post CONFIG_REPORT_REQ");
                                free(cfg_req);
                            }
                        }*/

                        // Освобождаем контекст bind
                        free(delayed_ctx);
                    }
                } else {
                    ESP_LOGW(TAG, "❌ BIND failed: status=0x%02x", resp->status);
                }

                // Освобождаем память ответа
                zb_manager_free_bind_resp(resp);
                break;
            }
            case ZB_ACTION_UNBIND_RESP: {
            zb_manager_bind_resp_message_t *resp = (zb_manager_bind_resp_message_t *)msg.event_data;
            if (!resp) break;

            ESP_LOGI(TAG, "UNBIND response: status=0x%02x", resp->status);

            // Можно обработать статус, если нужно
            if (resp->status == ESP_ZB_ZDP_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "✅ UNBIND successful");
            } else {
                ESP_LOGW(TAG, "❌ UNBIND failed: status=0x%02x", resp->status);
            }

            // Освобождаем
            zb_manager_free_bind_resp(resp);
            break;
        }

            case ZB_ACTION_DELAYED_CONFIG_REPORT_REQ: {
                ESP_LOGI(TAG, "✅ DELAYED_CONFIG_REPORT_REQ_LOCAL: processing");
                delayed_configure_report_req_t *req = (delayed_configure_report_req_t *)msg.event_data;
                device_custom_t *dev = zbm_dev_base_find_device_by_short_safe(req->short_addr);
                if (!dev) {
                    ESP_LOGW(TAG, "DELAYED_CONFIG_REPORT_REQ: device 0x%04x not found", req->short_addr);
                    break;
                }

                ESP_LOGI(TAG, "✅ CONFIGURE_REPORT: short=0x%04x, ep=%d, cluster=0x%04x, min=%d, max=%d, change=%d",
                        req->short_addr, req->src_endpoint, req->cluster_id,
                        req->min_interval, req->max_interval, req->reportable_change);

                // Вызов нужной функции в зависимости от кластера
                if (req->cluster_id == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT) {
                    esp_err_t err = ESP_FAIL;
                    err = zb_manager_configure_reporting_temperature_ext(
                        req->short_addr, req->src_endpoint,
                        req->min_interval, req->max_interval, req->reportable_change
                    );
                    if (err != ESP_OK) {
                        ESP_LOGW(TAG, "❌ Failed to configure reporting for Temperature");
                    }
                }
                else if (req->cluster_id == ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT) {
                    /*zb_manager_configure_reporting_humidity_ext(
                        req->short_addr, req->src_endpoint,
                        req->min_interval, req->max_interval, req->reportable_change
                    );*/
                }
                else if (req->cluster_id == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
                    esp_err_t err = zb_manager_configure_reporting_onoff_ext(
                        req->short_addr, req->src_endpoint,
                        req->min_interval, req->max_interval, req->reportable_change
                    );
                    if (err != ESP_OK) {
                        ESP_LOGW(TAG, "❌ Failed to configure reporting for On/Off cluster");
                    }
                }
                else if (req->cluster_id == ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG) {
                    /*zb_manager_configure_reporting_power_ext(
                        req->short_addr, req->src_endpoint,
                        req->min_interval, req->max_interval, req->reportable_change
                    );*/
                }
                else {
                    ESP_LOGW(TAG, "❌ Unsupported cluster for reporting: 0x%04x", req->cluster_id);
                }

                //free(req);
                break;
            }

            case ZB_ACTION_DELAYED_DISCOVER_ATTR_REQ: {
                ESP_LOGI(TAG, "✅ ZB_ACTION_DELAYED_DISCOVER_ATTR_REQ: processing");
                delayed_discovery_attr_req_t *req = (delayed_discovery_attr_req_t *)msg.event_data;
                device_custom_t *dev = zbm_dev_base_find_device_by_short_safe(req->short_addr);
                if (!dev) {
                    ESP_LOGW(TAG, "ZB_ACTION_DELAYED_DISCOVER_ATTR_REQ: device 0x%04x not found", req->short_addr);
                    break;
                }
                esp_zb_zcl_disc_attr_cmd_t cmd_req;
                cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
                cmd_req.cluster_id = req->cluster_id;
                cmd_req.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
                cmd_req.dis_defalut_resp = 0;
                //cmd_req.manuf_code = dev->manufacturer_code;
                //cmd_req.manuf_specific = 1;

                cmd_req.max_attr_number = req->max_attr_number;
                cmd_req.start_attr_id = req->start_attr_id;
                cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = req->short_addr;
                cmd_req.zcl_basic_cmd.dst_endpoint = req->endpoint;
                cmd_req.zcl_basic_cmd.src_endpoint = 1;
                uint8_t tsn = 0xff;
                tsn = zb_manager_disc_attr_cmd_req(&cmd_req);

                break;
            }
            case ZB_ACTION_DISCOVER_ATTR_RESP: {
                ESP_LOGI(TAG, "✅ ZB_ACTION_DISCOVER_ATTR_RESP: processing");
                const uint8_t *data = (uint8_t *)msg.event_data;
                uint16_t len = msg.data_size;

                esp_err_t update_dev_data_result = ESP_FAIL;
                
                esp_zb_zcl_cmd_info_t *info = (esp_zb_zcl_cmd_info_t *)data;
                if (info->status != ESP_ZB_ZCL_STATUS_SUCCESS) {
                    ESP_LOGW(TAG, "Discover attributes failed: status=0x%02x", info->status);
                    break;
                }
                update_dev_data_result = zbm_dev_base_dev_update_from_discovery_attr_notify_safe(data, len);

                uint16_t src_addr = info->src_address.u.short_addr;
                uint8_t ep = info->dst_endpoint;
                uint16_t cluster_id = info->cluster;

                /*uint8_t attr_count = data[sizeof(esp_zb_zcl_cmd_info_t)];
                const uint8_t *ptr = data + sizeof(esp_zb_zcl_cmd_info_t) + 1;

                ESP_LOGI(TAG, "Discovery result: short=0x%04x, ep=%d, cluster=0x%04x, count=%d",
                        src_addr, ep, cluster_id, attr_count);

                for (int i = 0; i < attr_count; i++) {
                    uint16_t attr_id = (ptr[1] << 8) | ptr[0];
                    esp_zb_zcl_attr_type_t attr_type = ptr[2];

                    ESP_LOGI(TAG, "  Attr[0x%04x] Type=0x%02x", attr_id, attr_type);

                    ptr += 3; // id (2) + type (1)
                }*/

                // Здесь можешь собрать в JSON и отправить через WebSocket в UI
                if (update_dev_data_result == ESP_OK)
                {
                    
                    ws_notify_device_update(src_addr);
                    zbm_dev_base_save_req_cmd();
                    //vTaskDelay( 
                    //zbm_dev_base_queue_save_req_cmd();
                    
                }
                
                //ws_notify_discovery_result(src_addr, ep, cluster_id, data, len);
                break;
            }
            case ZB_ACTION_NETWORK_IS_OPEN: {
                ESP_LOGI(TAG, "🌐 Zigbee network OPENED");
                //isZigbeeNetworkOpened = true;
                ws_notify_network_status(); // ✅ Рассылаем всем клиентам
                //

                break;
            }

            case ZB_ACTION_NETWORK_IS_CLOSE: {
                ESP_LOGI(TAG, "🚫 Zigbee network CLOSED");
                //isZigbeeNetworkOpened = false;
                ws_notify_network_status(); // ✅
                break;
            }
            default:
                ESP_LOGW(TAG, "Unhandled action event: %ld", msg.event_id);
                break;
        }

        // RAW-буфер будет освобождён здесь после обработки
        // Все парсеры (read_attr, report, bind) используют один free(msg.event_data) в конце
        free(msg.event_data);
        msg.event_data = NULL;
        int64_t end = esp_timer_get_time();
        if ((end - start) > 100000) {
            ESP_LOGW(TAG, "⚠️ Event handler took %lld ms!", (end - start) / 1000);
        }else ESP_LOGW(TAG, " Event handler took %lld ms!", (end - start) / 1000);

        if (report_count % 100 == 0) {
            ESP_LOGW(TAG, "✅ 100 reports: min RAM = %lu", min_ram);
            report_count = 0;
        }

        // Мониторинг RAM
        if (esp_log_timestamp() - last_log_ms > 10000) {
            size_t free = esp_get_free_heap_size();
            size_t min = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
            size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
            uint32_t frag = (free > 0) ? (100 - (largest * 100) / free) : 0;

            ESP_LOGI("RAM_MONITOR", "📊 RAM: %" PRIu32 "kB, min=%" PRIu32 "kB, largest=%" PRIu32 "kB, frag=%" PRIu32 "%%",
                    (uint32_t)(free / 1024),
                    (uint32_t)(min / 1024),
                    (uint32_t)(largest / 1024),
                    frag);
            last_log_ms = esp_log_timestamp();
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        //ESP_LOGW(TAG, "RAM control zb_action_worker_task on END %lu", esp_get_free_heap_size());
    }
}

esp_err_t zb_manager_start_action_worker(uint8_t core)
{
    if (s_action_task_handle) return ESP_OK;

    s_action_queue = xQueueCreate(20, sizeof(zb_action_msg_t));
    if (!s_action_queue) return ESP_ERR_NO_MEM;

    static StackType_t action_stack[ZB_ACTION_STACK_SIZE];
    static StaticTask_t action_task_buffer;

    s_action_task_handle = xTaskCreateStatic(
        zb_action_worker_task,
        "zb_action",
        ZB_ACTION_STACK_SIZE,
        NULL,
        ZB_ACTION_TASK_PRIORITY,
        action_stack,
        &action_task_buffer
    );

    return s_action_task_handle ? ESP_OK : ESP_FAIL;
}

bool zb_manager_post_to_action_worker(int32_t id, void *data, size_t size)
{
    if (!s_action_queue || !data) return false;

    zb_action_msg_t msg = {
        .event_id = id,
        .data_size = size,
        .event_data = calloc(1, size)
    };

    if (!msg.event_data) return false;

    memcpy(msg.event_data, data, size);

    return xQueueSend(s_action_queue, &msg, pdMS_TO_TICKS(10)) == pdTRUE;
}

// zb_manager_action_handler_worker.c
void zb_manager_start_powered_device_sync(void)
{
    ESP_LOGI("SYNC", "🔁 Starting sync with always-powered devices...");

    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) != pdTRUE) {
        ESP_LOGE("SYNC", "Failed to take mutex in powered_device_sync");
        return;
    }

    for (int i = 0; i < RemoteDevicesCount; i++) {
        device_custom_t *dev = RemoteDevicesArray[i];
        if (!dev) continue;

        // Пропускаем, если не всегда включено
        if (!zb_manager_is_device_always_powered(dev)) 
        {
            dev->is_online = false;
            continue;
        }

        // Пропускаем, если уже есть pending read
        if (dev->has_pending_read) {
            ESP_LOGD("SYNC", "Device 0x%04x: already has pending read", dev->short_addr);
            continue;
        }

        bool sent_any = false;

        // принудительно выставляем pending false
        dev->has_pending_read = false;
        dev->has_pending_response = false;

        // обнуляем online статус
        dev->is_online = false;
        // Перебираем все эндпоинты устройства
        for (int ep_idx = 0; ep_idx < dev->endpoints_count; ep_idx++) {
            endpoint_custom_t *ep = dev->endpoints_array[ep_idx];
            if (!ep || !ep->is_use_on_off_cluster) continue;

            // Пытаемся отправить ReadAttr для OnOff
            uint8_t tsn = zb_manager_read_on_off_attribute(dev->short_addr, ep->ep_id);
            if (tsn != 0xff) {
                // команда на чтение отправлена
                dev->has_pending_read = true;
                // фиксируем время отправки команды
                dev->last_pending_read_ms = esp_log_timestamp();
                ESP_LOGI("SYNC", "✅ Sent ReadAttr(OnOff) to 0x%04x (EP: %d)", dev->short_addr, ep->ep_id);
                sent_any = true;
                // Не выходим — можно опросить несколько эндпоинтов (например, реле с двумя клавишами)
            } else {
                ESP_LOGW("SYNC", "❌ Failed to send ReadAttr(OnOff) to 0x%04x (EP: %d)", dev->short_addr, ep->ep_id);
            }
        }

        // Если OnOff не найден — можно опросить Basic для other data (опционально)
        if (!sent_any) {
            // Например, прочитать Manufacturer Name
            uint16_t *attr_list = calloc(1, sizeof(uint16_t));
            if (attr_list) {
                attr_list[0] = ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID;

                esp_zb_zcl_read_attr_cmd_t read_attr_cmd = {
                    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                    .zcl_basic_cmd = {
                        .src_endpoint = 1,
                        .dst_endpoint = 1,
                        .dst_addr_u.addr_short = dev->short_addr,
                    },
                    .clusterID = ESP_ZB_ZCL_CLUSTER_ID_BASIC,
                    .attr_number = 1,
                    .attr_field = attr_list,
                };

                uint8_t tsn = zb_manager_zcl_read_attr_cmd_req(&read_attr_cmd);
                if (tsn == 0xff) {
                    ESP_LOGW("SYNC", "Failed to send ReadAttr(Basic) to 0x%04x", dev->short_addr);
                    free(attr_list);
                } else {
                    dev->has_pending_read = true;
                    ESP_LOGI("SYNC", "✅ Sent ReadAttr(Basic) to 0x%04x", dev->short_addr);
                }
            }
        }
    }

    DEVICE_ARRAY_UNLOCK();
}


