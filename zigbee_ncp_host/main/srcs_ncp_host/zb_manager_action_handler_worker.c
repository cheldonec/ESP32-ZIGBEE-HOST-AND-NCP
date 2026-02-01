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

#define ZB_ACTION_STACK_SIZE     6144
#define ZB_ACTION_TASK_PRIORITY  8

static TaskHandle_t s_action_task_handle = NULL;
static QueueHandle_t s_action_queue = NULL;

typedef struct {
    int32_t event_id;
    void *event_data;
    size_t data_size;
} zb_action_msg_t;

// === POOL for read_attr_resp ===
#define READ_ATTR_POOL_SIZE 2  // Хватит на 2 одновременных ответов
#define MAX_ATTR_COUNT 16
#define MAX_ATTR_LEN   64

// === РАСШИРЕННЫЙ пул для read_attr_resp ===
typedef struct {
    zb_manager_cmd_read_attr_resp_message_t msg;
    zb_manager_attr_t attr_arr[MAX_ATTR_COUNT];
    bool used;
} read_attr_pool_item_t;

static zb_manager_cmd_read_attr_resp_message_t g_read_resp_pool[READ_ATTR_POOL_SIZE];

static read_attr_pool_item_t g_read_resp_pool_extended[READ_ATTR_POOL_SIZE];

static bool g_read_resp_pool_used[READ_ATTR_POOL_SIZE] = {0};
static const char *POOL_TAG = "POOL_READ_ATTR";

// === POOL for report_attr_resp ===
#define REPORT_ATTR_POOL_SIZE 2  // Хватит на 2 одновременных report'ов

static zb_manager_cmd_report_attr_resp_message_t g_report_resp_pool[REPORT_ATTR_POOL_SIZE];
static bool g_report_resp_pool_used[REPORT_ATTR_POOL_SIZE] = {0};
static uint8_t g_report_attr_value_pool[REPORT_ATTR_POOL_SIZE][64]; // буферы до 32 байт (можно увеличить)
static const char *REPORT_POOL_TAG = "POOL_REPORT_ATTR";

/**
 * @brief Получить свободный объект из пула
 */
static zb_manager_cmd_read_attr_resp_message_t* read_attr_pool_get(void)
{
    for (int i = 0; i < READ_ATTR_POOL_SIZE; i++) {
        if (!g_read_resp_pool_extended[i].used) {
            g_read_resp_pool_extended[i].used = true;
            read_attr_pool_item_t *item = &g_read_resp_pool_extended[i];

            memset(&item->msg, 0, sizeof(zb_manager_cmd_read_attr_resp_message_t));
            item->msg.attr_arr = item->attr_arr;

            // Подключаем attr_value к attr_value_buf
            for (int j = 0; j < MAX_ATTR_COUNT; j++) {
                item->attr_arr[j].attr_value = item->attr_arr[j].attr_value_buf;
            }

            ESP_LOGW(POOL_TAG, "✅ Got item %d from pool", i);
            return &item->msg;
        }
    }

    ESP_LOGW(POOL_TAG, "❌ Pool FULL! Consider increasing READ_ATTR_POOL_SIZE");
    return NULL;
}


/**
 * @brief Вернуть объект в пул
 */
static void read_attr_pool_put(zb_manager_cmd_read_attr_resp_message_t* msg)
{
    if (!msg) return;

    for (int i = 0; i < READ_ATTR_POOL_SIZE; i++) {
        if (&g_read_resp_pool_extended[i].msg == msg) {
            if (!g_read_resp_pool_extended[i].used) {
                ESP_LOGW(POOL_TAG, "⚠️ Double free detected for pool item %d!", i);
                return;
            }

            // Не делаем free(attr_arr) — всё в стеке пула

            g_read_resp_pool_extended[i].used = false;
            ESP_LOGW(POOL_TAG, "🗑 Returned item %d to pool", i);
            return;
        }
    }

    ESP_LOGE(POOL_TAG, "❌ Attempt to free NON-POOL pointer!");
}


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
    ws_notify_device_update(dev->short_addr);
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
           // working copy
            /*case ZB_ACTION_ATTR_READ_RESP: {
            ESP_LOGD(TAG, "ZB_ACTION_ATTR_READ_RESP: event_data=%p, data_size=%zu", msg.event_data, msg.data_size);

            if (!msg.event_data) {
                ESP_LOGE(TAG, "❌ event_data is NULL in ZB_ACTION_ATTR_READ_RESP");
                break;
            }

            if (msg.data_size < sizeof(zb_manager_cmd_read_attr_resp_message_t)) {
                ESP_LOGE(TAG, "❌ Invalid data size: %zu, expected >= %zu", msg.data_size, sizeof(zb_manager_cmd_read_attr_resp_message_t));
                break;
            }

            zb_manager_cmd_read_attr_resp_message_t* read_resp = (zb_manager_cmd_read_attr_resp_message_t *)msg.event_data;

            // Проверка на валидность short_addr
            uint16_t short_addr = read_resp->info.src_address.u.short_addr;
            if (short_addr == 0xFFFF) {
                ESP_LOGW(TAG, "❌ Invalid short_addr: 0xFFFF in read response");
                zb_manager_free_read_attr_resp_attr_array(read_resp);
                break;
            }

            // Проверка на корректный статус
            if (read_resp->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
                ESP_LOGW(TAG, "ZB_ACTION_ATTR_READ_RESP: status 0x%02x from 0x%04x", read_resp->info.status, short_addr);
                zb_manager_free_read_attr_resp_attr_array(read_resp);
                break;
            }

            // Поиск устройства
            device_custom_t *dev_info = zb_manager_find_device_by_short(short_addr);
            if (!dev_info) {
                ESP_LOGW(TAG, "ZB_ACTION_ATTR_READ_RESP: device 0x%04x not found", short_addr);
                zb_manager_free_read_attr_resp_attr_array(read_resp);
                break;
            }

            // Обновляем активность, выставляется online статус dev->is_online
            zb_manager_update_device_activity(short_addr, 10);

            ESP_LOGI(TAG, "ZB_ACTION_ATTR_READ_RESP: short=0x%04x, ep=%d, cluster=0x%04x, attr_count=%d",
                    short_addr,
                    read_resp->info.src_endpoint,
                    read_resp->info.cluster,
                    read_resp->attr_count);

            // Если устройство в режиме сопряжения — передаём в pairing_worker
            if (dev_info->is_in_build_status == 1) {
                ESP_LOGI(TAG, "ZB_ACTION_ATTR_READ_RESP: device 0x%04x is in pairing mode → forwarding to pairing_worker", short_addr);

                size_t total_size = sizeof(zb_manager_cmd_read_attr_resp_message_t) +
                                    (read_resp->attr_count - 1) * sizeof(zb_manager_attr_t);

                // Выделяем память для всей структуры
                zb_manager_cmd_read_attr_resp_message_t *copy = calloc(1, total_size);
                if (!copy) {
                    ESP_LOGE(TAG, "❌ Failed to allocate memory for read_attr_resp copy");
                    //zb_manager_free_read_attr_resp_attr_array(read_resp);
                    break;
                }

                // Копируем основную часть
                memcpy(copy, read_resp, total_size);

                // Копируем каждый attr_value
                for (int i = 0; i < read_resp->attr_count; i++) {
                    if (read_resp->attr_arr[i].attr_value && read_resp->attr_arr[i].attr_len > 0) {
                        uint8_t *val_copy = malloc(read_resp->attr_arr[i].attr_len);
                        if (val_copy) {
                            memcpy(val_copy, read_resp->attr_arr[i].attr_value, read_resp->attr_arr[i].attr_len);
                            copy->attr_arr[i].attr_value = val_copy;
                        } else {
                            ESP_LOGE(TAG, "❌ Failed to allocate attr_value copy for attr_id=0x%04x", read_resp->attr_arr[i].attr_id);
                        }
                    }
                }

                // Отправляем копию
                if (!zb_manager_post_to_pairing_worker(ZB_PAIRING_ATTR_READ_RESP, copy, total_size)) {
                    ESP_LOGE(TAG, "❌ Failed to forward ATTR_READ_RESP to pairing_worker");
                    // Освобождаем копию в случае ошибки
                    for (int i = 0; i < copy->attr_count; i++) {
                        if (copy->attr_arr[i].attr_value) free(copy->attr_arr[i].attr_value);
                    }
                    free(copy);
                }
                break;
            }

            // Успешный ответ на ReadAttr → устройство живое!
            // Только для питаемых устройств
            if (zb_manager_is_device_always_powered(dev_info)) {
                dev_info->is_online = true;
                dev_info->has_pending_response = true;
                ESP_LOGI(TAG, "✅ Mains-powered device 0x%04x is ONLINE (responded to ReadAttr)", short_addr);
            }
            // если устройство уже сопряжено,- продолжаем обновление атрибутов
            // 🔹 Обработка On/Off кластера
            if (read_resp->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
                endpoint_custom_t *ep = zb_manager_find_endpoint(short_addr, read_resp->info.src_endpoint);
                if (ep && ep->is_use_on_off_cluster && ep->server_OnOffClusterObj) {
                    bool updated = false;

                    // Проверка на валидный attr_count
                    if (read_resp->attr_count == 0) {
                        ESP_LOGW(TAG, "OnOff ReadAttr: attr_count is 0");
                    //} else if (read_resp->attr_count > ZB_MANAGER_MAX_ATTRIBUTES) {
                        //ESP_LOGE(TAG, "OnOff ReadAttr: attr_count too large: %d", read_resp->attr_count);
                    } else {
                        for (int i = 0; i < read_resp->attr_count; i++) {
                            if (read_resp->attr_arr[i].attr_id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
                                if (read_resp->attr_arr[i].attr_value == NULL) {
                                    ESP_LOGW(TAG, "OnOff ReadAttr: attr_value is NULL for attr_id=0x%04x", read_resp->attr_arr[i].attr_id);
                                    continue;
                                }

                                bool new_state = *(bool*)read_resp->attr_arr[i].attr_value;
                                bool old_state = ep->server_OnOffClusterObj->on_off;

                                if (old_state != new_state) {
                                    zb_manager_on_off_cluster_update_attribute(ep->server_OnOffClusterObj,
                                                                            read_resp->attr_arr[i].attr_id,
                                                                            read_resp->attr_arr[i].attr_value);
                                    ESP_LOGI(TAG, "✅ OnOff updated via ReadAttr: 0x%04x (ep: %d) → %s (was: %s)",
                                            dev_info->short_addr, ep->ep_id,
                                            new_state ? "ON" : "OFF",
                                            old_state ? "ON" : "OFF");
                                    ws_notify_device_update(dev_info->short_addr);
                                    updated = true;
                                } else {
                                    ESP_LOGD(TAG, "OnOff state unchanged: 0x%04x (ep: %d) → %s",
                                            dev_info->short_addr, ep->ep_id, new_state ? "ON" : "OFF");
                                }
                                break;
                            }
                        }
                    }

                    if (!updated && read_resp->attr_count > 0) {
                        ESP_LOGW(TAG, "OnOff ReadAttr: no matching attr_id in response");
                    }
                } else {
                    ESP_LOGW(TAG, "OnOff ReadAttr: endpoint or cluster not found for 0x%04x (ep: %d)",
                            dev_info->short_addr, read_resp->info.src_endpoint);
                }
            }

            // Обновляем флаги устройства
            dev_info->has_pending_read = false;
            dev_info->has_pending_response = true;
            //обновляем  web в любом случае
            ws_notify_device_update(dev_info->short_addr);
            // Освобождаем только внутренние данные, не саму структуру
            zb_manager_free_read_attr_resp_attr_array(read_resp);

            break;
        }*/

        case ZB_ACTION_ATTR_READ_RESP: {
            uint8_t *input_copy = (uint8_t *)msg.event_data;
            size_t total_len = msg.data_size;

            if (!input_copy || total_len < sizeof(esp_zb_zcl_cmd_info_t) + 1) {
                ESP_LOGE(TAG, "❌ Invalid input data in ZB_ACTION_ATTR_READ_RESP");
                break;
            }

            ESP_LOGI(TAG, "ZB_ACTION_ATTR_READ_RESP: processing raw buffer of size %zu", total_len);
            ESP_LOG_BUFFER_HEX_LEVEL("NCP RAW", input_copy, total_len, ESP_LOG_INFO);

            // === ИСПОЛЬЗУЕМ ПУЛ ===
            zb_manager_cmd_read_attr_resp_message_t* read_resp = read_attr_pool_get();
            if (!read_resp) {
                ESP_LOGE(TAG, "Failed to get read_resp from pool");
                break;
            }

            // Копируем общую информацию
            esp_zb_zcl_cmd_info_t* cmd_info = (esp_zb_zcl_cmd_info_t*)input_copy;
            memcpy(&read_resp->info, cmd_info, sizeof(esp_zb_zcl_cmd_info_t));

            // Читаем attr_count
            uint8_t attr_count = *(uint8_t*)(input_copy + sizeof(esp_zb_zcl_cmd_info_t));
            read_resp->attr_count = attr_count;
            ESP_LOGW(TAG, "Attribute count: %d", attr_count);

            // Проверка: не больше ли, чем MAX_ATTR_COUNT
            if (attr_count > MAX_ATTR_COUNT) {
                ESP_LOGW(TAG, "Attribute count %u > MAX_ATTR_COUNT %u → truncating", attr_count, MAX_ATTR_COUNT);
                attr_count = MAX_ATTR_COUNT;
                read_resp->attr_count = attr_count;
            }

            uint8_t* pointer = (uint8_t*)(input_copy + sizeof(esp_zb_zcl_cmd_info_t) + sizeof(uint8_t));
            bool parse_failed = false;

            for (uint8_t i = 0; i < attr_count; i++) {
                zb_manager_attr_t* attr = &read_resp->attr_arr[i];

                if (pointer + sizeof(uint16_t) + sizeof(esp_zb_zcl_attr_type_t) + sizeof(uint8_t) > input_copy + total_len) {
                    ESP_LOGE(TAG, "Buffer overflow while parsing attr header");
                    parse_failed = true;
                    break;
                }

                attr->attr_id  = *((uint16_t*)pointer); pointer += sizeof(uint16_t);
                attr->attr_type = *((esp_zb_zcl_attr_type_t*)pointer); pointer += sizeof(esp_zb_zcl_attr_type_t);
                attr->attr_len = *(uint8_t*)pointer; pointer += sizeof(uint8_t);

                if (attr->attr_len > 0) {
                    // Проверка: не выходит ли за пределы буфера
                    if (pointer + attr->attr_len > input_copy + total_len) {
                        ESP_LOGE(TAG, "Attr value overflows buffer: id=0x%04x, len=%u", attr->attr_id, attr->attr_len);
                        parse_failed = true;
                        break;
                    }

                    // Копируем в attr_value_buf (уже подключён в read_attr_pool_get)
                    memcpy(attr->attr_value, pointer, attr->attr_len);
                    pointer += attr->attr_len;
                } else {
                    attr->attr_value = NULL;
                }

                ESP_LOGI(TAG, "Parsed attr[%d]: id=0x%04x, type=0x%02x, len=%u", i, attr->attr_id, attr->attr_type, attr->attr_len);
                if (attr->attr_len > 0) {
                    ESP_LOG_BUFFER_HEX_LEVEL("Attr Value", attr->attr_value, attr->attr_len, ESP_LOG_INFO);
                }
            }

            if (parse_failed) {
                ESP_LOGE(TAG, "Failed to parse attribute values");
                read_attr_pool_put(read_resp);
                read_resp = NULL;
                break;
            }

            uint16_t short_addr = read_resp->info.src_address.u.short_addr;

            // Проверка на broadcast
            if (short_addr == 0xFFFF) {
                ESP_LOGW(TAG, "❌ Invalid short_addr: 0xFFFF in read response");
                read_attr_pool_put(read_resp);
                read_resp = NULL;
                break;
            }

            // Проверка статуса
            if (read_resp->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
                ESP_LOGW(TAG, "ZB_ACTION_ATTR_READ_RESP: status 0x%02x from 0x%04x", read_resp->info.status, short_addr);
                read_attr_pool_put(read_resp);
                read_resp = NULL;
                break;
            }

            // Поиск устройства
            device_custom_t *dev_info = zb_manager_find_device_by_short(short_addr);
            if (!dev_info) {
                ESP_LOGW(TAG, "ZB_ACTION_ATTR_READ_RESP: device 0x%04x not found", short_addr);
                read_attr_pool_put(read_resp);
                read_resp = NULL;
                break;
            }

            // Обновляем активность
            zb_manager_update_device_activity(short_addr, 10);

            // === 🔁 Если устройство в режиме сопряжения → отправляем в pairing_worker ===
            if (dev_info->is_in_build_status == 1) {
                ESP_LOGI(TAG, "ZB_ACTION_ATTR_READ_RESP: device 0x%04x is in pairing mode → forwarding raw buffer", short_addr);

                uint8_t *pairing_copy = malloc(total_len);
                if (pairing_copy) {
                    memcpy(pairing_copy, input_copy, total_len);
                    bool post_ok = zb_manager_post_to_pairing_worker(ZB_PAIRING_ATTR_READ_RESP, pairing_copy, total_len);
                    if (!post_ok) {
                        ESP_LOGE(TAG, "❌ Failed to forward to pairing_worker");
                        free(pairing_copy);
                    } else {
                        free(pairing_copy);
                    }
                } else {
                    ESP_LOGE(TAG, "❌ Failed to allocate pairing_copy");
                }
                // Возвращаем в пул
                ESP_LOGI(TAG, "🔄 Returning read_resp to pool");
                read_attr_pool_put(read_resp);
                break;
            }

            // 🔹 Обработка On/Off кластера
            if (read_resp->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
                endpoint_custom_t *ep = zb_manager_find_endpoint(short_addr, read_resp->info.src_endpoint);
                if (ep && ep->is_use_on_off_cluster && ep->server_OnOffClusterObj) {
                    bool updated = false;

                    if (read_resp->attr_count == 0) {
                        ESP_LOGW(TAG, "OnOff ReadAttr: attr_count is 0");
                    } else {
                        for (int i = 0; i < read_resp->attr_count; i++) {
                            // отправляем в обработчик сценариев
                            zb_rule_trigger_state_update(
                                read_resp->info.src_address.u.short_addr,
                                read_resp->info.cluster,
                                read_resp->attr_arr[i].attr_id,
                                read_resp->attr_arr[i].attr_value,
                                read_resp->attr_arr[i].attr_len,
                                read_resp->attr_arr[i].attr_type
                            );
                            if (read_resp->attr_arr[i].attr_id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
                                if (read_resp->attr_arr[i].attr_value == NULL) {
                                    ESP_LOGW(TAG, "OnOff ReadAttr: attr_value is NULL for attr_id=0x%04x", read_resp->attr_arr[i].attr_id);
                                    continue;
                                }

                                bool new_state = *(bool*)read_resp->attr_arr[i].attr_value;
                                bool old_state = ep->server_OnOffClusterObj->on_off;

                                if (old_state != new_state) {
                                    zb_manager_on_off_cluster_update_attribute(ep->server_OnOffClusterObj,
                                                                            read_resp->attr_arr[i].attr_id,
                                                                            read_resp->attr_arr[i].attr_value);
                                    ESP_LOGI(TAG, "✅ OnOff updated via ReadAttr: 0x%04x (ep: %d) → %s (was: %s)",
                                            dev_info->short_addr, ep->ep_id,
                                            new_state ? "ON" : "OFF",
                                            old_state ? "ON" : "OFF");

                                
                                    ws_notify_device_update(dev_info->short_addr);
                                    // ✅ Уведомляем HA
                                    ha_device_updated(dev_info, ep);
                                    updated = true;
                                } else {
                                    ESP_LOGD(TAG, "OnOff state unchanged: 0x%04x (ep: %d) → %s",
                                            dev_info->short_addr, ep->ep_id, new_state ? "ON" : "OFF");
                                }
                                break;
                            }
                        }
                    }

                    if (!updated && read_resp->attr_count > 0) {
                        ESP_LOGI(TAG, "OnOff ReadAttr: no matching attr_id in response");
                    }
                } else {
                    ESP_LOGW(TAG, "OnOff ReadAttr: endpoint or cluster not found for 0x%04x (ep: %d)",
                            dev_info->short_addr, read_resp->info.src_endpoint);
                }
            }

            // Обновляем флаги
            dev_info->has_pending_read = false;
            dev_info->has_pending_response = true;
            ws_notify_device_update(dev_info->short_addr);

            // Возвращаем в пул
            ESP_LOGI(TAG, "🔄 Returning read_resp to pool");
            read_attr_pool_put(read_resp);
            read_resp = NULL;

            break;
        }


// working code для восстановления
            /*case ZB_ACTION_ATTR_REPORT: {
            ESP_LOGI(TAG, "✅ ATTR_REPORT_EVENT: processing");
            zb_manager_cmd_report_attr_resp_message_t *rep = (zb_manager_cmd_report_attr_resp_message_t *)msg.event_data;

            if (rep->status != ESP_ZB_ZCL_STATUS_SUCCESS) {
                ESP_LOGW(TAG, "ATTR_REPORT_EVENT: status 0x%02x", rep->status);
                zb_manager_free_report_attr_resp(rep);
                break;
            }

            // логируем
            ESP_LOGI(TAG, "📊 ATTR_REPORT: short=0x%04x, ep=%d, cluster=0x%04x, attr_id=0x%04x, type=0x%02x, len=%d",
             rep->src_address.u.short_addr,
             rep->src_endpoint,
             rep->cluster,
             rep->attr.attr_id,
             rep->attr.attr_type,
             rep->attr.attr_len);

            // Печать значений в hex
            if (rep->attr.attr_value && rep->attr.attr_len > 0) {
                ESP_LOG_BUFFER_HEX_LEVEL("Attr Value", rep->attr.attr_value, rep->attr.attr_len, ESP_LOG_INFO);
            }

            // Поиск устройства
            device_custom_t *dev = zb_manager_find_device_by_short(rep->src_address.u.short_addr);
            if (!dev) {
                ESP_LOGW(TAG, "ATTR_REPORT_EVENT: device 0x%04x not found", rep->src_address.u.short_addr);
                zb_manager_free_report_attr_resp(rep);
                break;
            }

            // Обновляем активность
            // если устройство отвечает на read_attr для контроля состояния, то обновляем, иначе устройство может просто спамить или поменяло родителя,
            // активность надо будет обновлять только если устройство отвечает на read_attr
            if (zb_manager_is_device_always_powered(dev))
            {
                if((dev->has_pending_response == true))
                {
                    zb_manager_update_device_activity(rep->src_address.u.short_addr, 10);
                }
            }else 
            zb_manager_update_device_activity(rep->src_address.u.short_addr, 10);
            //ws_notify_device_update(dev->short_addr);

            // 🔹 Basic Cluster (0x0000)
            if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_BASIC) {
                if (dev->server_BasicClusterObj == NULL) {
                    dev->server_BasicClusterObj = calloc(1, sizeof(zigbee_manager_basic_cluster_t));
                    if (dev->server_BasicClusterObj) {
                        zigbee_manager_basic_cluster_t cl = ZIGBEE_BASIC_CLUSTER_DEFAULT_INIT();
                        memcpy(dev->server_BasicClusterObj, &cl, sizeof(zigbee_manager_basic_cluster_t));
                    } else {
                        ESP_LOGE(TAG, "Failed to allocate Basic Cluster for 0x%04x", dev->short_addr);
                        zb_manager_free_report_attr_resp(rep);
                        break;
                    }
                }
                if (dev->server_BasicClusterObj) {
                    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                    zb_manager_basic_cluster_update_attribute(dev->server_BasicClusterObj, rep->attr.attr_id, rep->attr.attr_value);
                    DEVICE_ARRAY_UNLOCK();
                    // ✅ Логируем обновление атрибута
                    log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);
                    // ✅ Отправляем полное обновление устройства на web-сокет
                            ws_notify_device_update(rep->src_address.u.short_addr);
                    } else {
                        ESP_LOGE(TAG, "Failed to take mutex for ESP_ZB_ZCL_CLUSTER_ID_BASIC update");
                        zb_manager_free_report_attr_resp(rep);
                        break;
                    }
                }
            }
            // 🔹 Power Configuration Cluster (0x0001)
            else if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG) {
                if (dev->server_PowerConfigurationClusterObj == NULL) {
                    dev->server_PowerConfigurationClusterObj = calloc(1, sizeof(zb_manager_power_config_cluster_t));
                    if (dev->server_PowerConfigurationClusterObj) {
                        zb_manager_power_config_cluster_t cl = ZIGBEE_POWER_CONFIG_CLUSTER_DEFAULT_INIT();
                        memcpy(dev->server_PowerConfigurationClusterObj, &cl, sizeof(zb_manager_power_config_cluster_t));
                    } else {
                        ESP_LOGE(TAG, "Failed to allocate Power Config Cluster for 0x%04x", dev->short_addr);
                        zb_manager_free_report_attr_resp(rep);
                        break;
                    }
                }
                if (dev->server_PowerConfigurationClusterObj) {
                    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                        zb_manager_power_config_cluster_update_attribute(dev->server_PowerConfigurationClusterObj, rep->attr.attr_id, rep->attr.attr_value);
                        DEVICE_ARRAY_UNLOCK();
                        // ✅ Логируем обновление атрибута
                        log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);
                        // ✅ Отправляем полное обновление устройства на web-сокет
                            ws_notify_device_update(rep->src_address.u.short_addr);
                    } else {
                        ESP_LOGE(TAG, "Failed to take mutex for Power Config update");
                        zb_manager_free_report_attr_resp(rep);
                        break;
                    }
                }
            }
            // 🔹 Cluster на уровне endpoint'а
            else {
                endpoint_custom_t *ep = zb_manager_find_endpoint(rep->src_address.u.short_addr, rep->src_endpoint);
                if (!ep) {
                    ESP_LOGW(TAG, "ATTR_REPORT_EVENT: endpoint not found: short=0x%04x, ep=%d", rep->src_address.u.short_addr, rep->src_endpoint);
                    zb_manager_free_report_attr_resp(rep);
                    break;
                }

                // 🔹 Temperature Measurement (0x0402)
                if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT) {
                    if (ep->server_TemperatureMeasurementClusterObj == NULL) {
                        ep->server_TemperatureMeasurementClusterObj = calloc(1, sizeof(zb_manager_temperature_measurement_cluster_t));
                        if (ep->server_TemperatureMeasurementClusterObj) {
                            zb_manager_temperature_measurement_cluster_t cl = ZIGBEE_TEMP_MEASUREMENT_CLUSTER_DEFAULT_INIT();
                            memcpy(ep->server_TemperatureMeasurementClusterObj, &cl, sizeof(zb_manager_temperature_measurement_cluster_t));
                        } else {
                            ESP_LOGE(TAG, "Failed to allocate Temp Meas Cluster for 0x%04x", rep->src_address.u.short_addr);
                            zb_manager_free_report_attr_resp(rep);
                            break;
                        }
                    }
                    if (ep->server_TemperatureMeasurementClusterObj) {
                        if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                            zb_manager_temp_meas_cluster_update_attribute(ep->server_TemperatureMeasurementClusterObj, rep->attr.attr_id, rep->attr.attr_value);
                            DEVICE_ARRAY_UNLOCK();
                            // ✅ Логируем обновление атрибута
                            log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);
                            // ✅ Отправляем полное обновление устройства на web-сокет
                            ws_notify_device_update(rep->src_address.u.short_addr);
                        } else {
                            ESP_LOGE(TAG, "Failed to take mutex for Temp Meas update");
                            zb_manager_free_report_attr_resp(rep);
                            break;
                        }
                    }
                }
                // 🔹 Humidity Measurement (0x0405)
                else if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT) {
                    if (ep->server_HumidityMeasurementClusterObj == NULL) {
                        ep->server_HumidityMeasurementClusterObj = calloc(1, sizeof(zb_manager_humidity_measurement_cluster_t));
                        if (ep->server_HumidityMeasurementClusterObj) {
                            zb_manager_humidity_measurement_cluster_t cl = ZIGBEE_HUMIDITY_MEASUREMENT_CLUSTER_DEFAULT_INIT();
                            memcpy(ep->server_HumidityMeasurementClusterObj, &cl, sizeof(zb_manager_humidity_measurement_cluster_t));
                        } else {
                            ESP_LOGE(TAG, "Failed to allocate Humidity Meas Cluster for 0x%04x", rep->src_address.u.short_addr);
                            zb_manager_free_report_attr_resp(rep);
                            break;
                        }
                    }
                    if (ep->server_HumidityMeasurementClusterObj) {
                        if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                            zb_manager_humidity_meas_cluster_update_attribute(ep->server_HumidityMeasurementClusterObj, rep->attr.attr_id, rep->attr.attr_value);
                            DEVICE_ARRAY_UNLOCK();
                            // ✅ Логируем обновление атрибута
                            log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);
                            // ✅ Отправляем полное обновление устройства на web-сокет
                            ws_notify_device_update(rep->src_address.u.short_addr);
                        } else {
                            ESP_LOGE(TAG, "Failed to take mutex for Humidity Meas update");
                            zb_manager_free_report_attr_resp(rep);
                            break;
                        }
                    }
                }
                // 🔹 On/Off Cluster (0x0006)
                else if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
                    if (ep->server_OnOffClusterObj == NULL) {
                        ep->server_OnOffClusterObj = calloc(1, sizeof(zb_manager_on_off_cluster_t));
                        if (ep->server_OnOffClusterObj) {
                            zb_manager_on_off_cluster_t cl = ZIGBEE_ON_OFF_CLUSTER_DEFAULT_INIT();
                            memcpy(ep->server_OnOffClusterObj, &cl, sizeof(zb_manager_on_off_cluster_t));
                        } else {
                            ESP_LOGE(TAG, "Failed to allocate On/Off Cluster for 0x%04x", rep->src_address.u.short_addr);
                            zb_manager_free_report_attr_resp(rep);
                            break;
                        }
                    }
                    if (ep->server_OnOffClusterObj) {
                        if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                            zb_manager_on_off_cluster_update_attribute(ep->server_OnOffClusterObj, rep->attr.attr_id, rep->attr.attr_value);

                            DEVICE_ARRAY_UNLOCK();
                            // ✅ Логируем обновление атрибута
                            log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);
                            // ✅ Отправляем полное обновление устройства на web-сокет
                            ws_notify_device_update(rep->src_address.u.short_addr);
                        } else {
                            ESP_LOGE(TAG, "Failed to take mutex for On/Off update");
                            zb_manager_free_report_attr_resp(rep);
                            break;
                        }
                    }
                }
                // 🔹 Неизвестный кластер
                else {
                    ESP_LOGW(TAG, "Unhandled cluster ID: 0x%04x", rep->cluster);
                    // логируем, даже если кластер не поддерживается
                }
            }

            // ✅ Единый вывод лога
            //log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);
            zb_manager_free_report_attr_resp(rep);
            break;
        }*/

        case ZB_ACTION_ATTR_REPORT: {
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

            /*if (rep->attr.attr_len > 0) {
                rep->attr.attr_value = malloc(rep->attr.attr_len);
                if (!rep->attr.attr_value) {
                    ESP_LOGE(TAG, "Failed to allocate attr_value");
                    zb_manager_free_report_attr_resp(rep);
                    break;
                }
                memcpy(rep->attr.attr_value, value_src, rep->attr.attr_len);
            } else {
                rep->attr.attr_value = NULL;
            }*/
           // Копируем значение атрибута в пул
           if (rep->attr.attr_len > 0) {
                if (rep->attr.attr_len > 32) {
                    ESP_LOGW(REPORT_POOL_TAG, "⚠️ Attr len %u > 32! Truncating", rep->attr.attr_len);
                    rep->attr.attr_len = 32;
                }
                memcpy(rep->attr.attr_value, value_src, rep->attr.attr_len);
            }

            // === Дальше — вся ваша старая логика, БЕЗ ИЗМЕНЕНИЙ ===

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

            // Поиск устройства
            device_custom_t *dev = zb_manager_find_device_by_short(rep->src_address.u.short_addr);
            if (!dev) {
                ESP_LOGW(TAG, "ATTR_REPORT_EVENT: device 0x%04x not found", rep->src_address.u.short_addr);
                //zb_manager_free_report_attr_resp(rep);
                report_attr_pool_put(rep);
                rep = NULL;
                break;
            }

            // Обновляем активность
            if (zb_manager_is_device_always_powered(dev)) {
                if (dev->has_pending_response == true) {
                    zb_manager_update_device_activity(rep->src_address.u.short_addr, 10);
                }
            } else {
                zb_manager_update_device_activity(rep->src_address.u.short_addr, 10);
            }

            // 🔹 Basic Cluster (0x0000)
            if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_BASIC) {
                if (dev->server_BasicClusterObj == NULL) {
                    dev->server_BasicClusterObj = calloc(1, sizeof(zigbee_manager_basic_cluster_t));
                    if (dev->server_BasicClusterObj) {
                        zigbee_manager_basic_cluster_t cl = ZIGBEE_BASIC_CLUSTER_DEFAULT_INIT();
                        memcpy(dev->server_BasicClusterObj, &cl, sizeof(zigbee_manager_basic_cluster_t));
                    } else {
                        ESP_LOGE(TAG, "Failed to allocate Basic Cluster for 0x%04x", dev->short_addr);
                        //zb_manager_free_report_attr_resp(rep);
                        report_attr_pool_put(rep);
                        rep = NULL;
                        break;
                    }
                }
                if (dev->server_BasicClusterObj) {
                    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                        zb_manager_basic_cluster_update_attribute(dev->server_BasicClusterObj, rep->attr.attr_id, rep->attr.attr_value);
                        DEVICE_ARRAY_UNLOCK();
                        log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);
                        ws_notify_device_update(rep->src_address.u.short_addr);
                    } else {
                        ESP_LOGE(TAG, "Failed to take mutex for ESP_ZB_ZCL_CLUSTER_ID_BASIC update");
                        //zb_manager_free_report_attr_resp(rep);
                        report_attr_pool_put(rep);
                        rep = NULL;
                        break;
                    }
                }
            }
            // 🔹 Power Configuration (0x0001)
            else if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG) {
                if (dev->server_PowerConfigurationClusterObj == NULL) {
                    dev->server_PowerConfigurationClusterObj = calloc(1, sizeof(zb_manager_power_config_cluster_t));
                    if (dev->server_PowerConfigurationClusterObj) {
                        zb_manager_power_config_cluster_t cl = ZIGBEE_POWER_CONFIG_CLUSTER_DEFAULT_INIT();
                        memcpy(dev->server_PowerConfigurationClusterObj, &cl, sizeof(zb_manager_power_config_cluster_t));
                    } else {
                        ESP_LOGE(TAG, "Failed to allocate Power Config Cluster for 0x%04x", dev->short_addr);
                        //zb_manager_free_report_attr_resp(rep);
                        report_attr_pool_put(rep);
                        rep = NULL;
                        break;
                    }
                }
                if (dev->server_PowerConfigurationClusterObj) {
                    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                        zb_manager_power_config_cluster_update_attribute(dev->server_PowerConfigurationClusterObj, rep->attr.attr_id, rep->attr.attr_value);
                        DEVICE_ARRAY_UNLOCK();
                        zb_rule_trigger_state_update(
                                rep->src_address.u.short_addr,
                                rep->cluster,
                                rep->attr.attr_id,
                                rep->attr.attr_value,
                                rep->attr.attr_len,
                                rep->attr.attr_type
                            );
                        log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);
                        ws_notify_device_update(rep->src_address.u.short_addr);
                    } else {
                        ESP_LOGE(TAG, "Failed to take mutex for Power Config update");
                        //zb_manager_free_report_attr_resp(rep);
                        report_attr_pool_put(rep);
                        rep = NULL;
                        break;
                    }
                }
            }
            // 🔹 Cluster на уровне endpoint'а
            else {
                endpoint_custom_t *ep = zb_manager_find_endpoint(rep->src_address.u.short_addr, rep->src_endpoint);
                if (!ep) {
                    ESP_LOGW(TAG, "ATTR_REPORT_EVENT: endpoint not found: short=0x%04x, ep=%d", rep->src_address.u.short_addr, rep->src_endpoint);
                    //zb_manager_free_report_attr_resp(rep);
                    report_attr_pool_put(rep);
                    rep = NULL;
                    break;
                }

                // 🔹 Temperature (0x0402)
                if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT) {
                    if (ep->server_TemperatureMeasurementClusterObj == NULL) {
                        ep->server_TemperatureMeasurementClusterObj = calloc(1, sizeof(zb_manager_temperature_measurement_cluster_t));
                        if (ep->server_TemperatureMeasurementClusterObj) {
                            zb_manager_temperature_measurement_cluster_t cl = ZIGBEE_TEMP_MEASUREMENT_CLUSTER_DEFAULT_INIT();
                            memcpy(ep->server_TemperatureMeasurementClusterObj, &cl, sizeof(zb_manager_temperature_measurement_cluster_t));
                        } else {
                            ESP_LOGE(TAG, "Failed to allocate Temp Meas Cluster");
                            rep = NULL;
                            break;
                        }
                    }
                    if (ep->server_TemperatureMeasurementClusterObj) {
                        if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                            zb_manager_temp_meas_cluster_update_attribute(ep->server_TemperatureMeasurementClusterObj, rep->attr.attr_id, rep->attr.attr_value);
                            DEVICE_ARRAY_UNLOCK();
                            // отправляем в обработчик сценариев
                            zb_rule_trigger_state_update(
                                rep->src_address.u.short_addr,
                                rep->cluster,
                                rep->attr.attr_id,
                                rep->attr.attr_value,
                                rep->attr.attr_len,
                                rep->attr.attr_type
                            );
                            log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);
                            ws_notify_device_update(rep->src_address.u.short_addr);
                            // ✅ Уведомляем HA
                            ha_device_updated(dev, ep);
                            //report_attr_pool_put(rep);
                        } else {
                            ESP_LOGE(TAG, "Failed to take mutex for Temp Meas update");
                            //zb_manager_free_report_attr_resp(rep);
                            report_attr_pool_put(rep);
                            rep = NULL;
                            break;
                        }
                    }
                }
                // 🔹 Humidity (0x0405)
                else if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT) {
                    if (ep->server_HumidityMeasurementClusterObj == NULL) {
                        ep->server_HumidityMeasurementClusterObj = calloc(1, sizeof(zb_manager_humidity_measurement_cluster_t));
                        if (ep->server_HumidityMeasurementClusterObj) {
                            zb_manager_humidity_measurement_cluster_t cl = ZIGBEE_HUMIDITY_MEASUREMENT_CLUSTER_DEFAULT_INIT();
                            memcpy(ep->server_HumidityMeasurementClusterObj, &cl, sizeof(zb_manager_humidity_measurement_cluster_t));
                        } else {
                            ESP_LOGE(TAG, "Failed to allocate Humidity Meas Cluster");
                            //zb_manager_free_report_attr_resp(rep);
                            report_attr_pool_put(rep);
                            rep = NULL;
                            break;
                        }
                    }
                    if (ep->server_HumidityMeasurementClusterObj) {
                        if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                            zb_manager_humidity_meas_cluster_update_attribute(ep->server_HumidityMeasurementClusterObj, rep->attr.attr_id, rep->attr.attr_value);
                            DEVICE_ARRAY_UNLOCK();
                            zb_rule_trigger_state_update(
                                rep->src_address.u.short_addr,
                                rep->cluster,
                                rep->attr.attr_id,
                                rep->attr.attr_value,
                                rep->attr.attr_len,
                                rep->attr.attr_type
                            );
                            log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);
                            ws_notify_device_update(rep->src_address.u.short_addr);
                            // ✅ Уведомляем HA
                            ha_device_updated(dev, ep);
                        } else {
                            ESP_LOGE(TAG, "Failed to take mutex for Humidity Meas update");
                            //zb_manager_free_report_attr_resp(rep);
                            report_attr_pool_put(rep);
                            rep = NULL;
                            break;
                        }
                    }
                }
                // 🔹 On/Off (0x0006)
                else if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
                    if (ep->server_OnOffClusterObj == NULL) {
                        ep->server_OnOffClusterObj = calloc(1, sizeof(zb_manager_on_off_cluster_t));
                        if (ep->server_OnOffClusterObj) {
                            zb_manager_on_off_cluster_t cl = ZIGBEE_ON_OFF_CLUSTER_DEFAULT_INIT();
                            memcpy(ep->server_OnOffClusterObj, &cl, sizeof(zb_manager_on_off_cluster_t));
                        } else {
                            ESP_LOGE(TAG, "Failed to allocate On/Off Cluster");
                            //zb_manager_free_report_attr_resp(rep);
                            report_attr_pool_put(rep);
                            rep = NULL;
                            break;
                        }
                    }
                    if (ep->server_OnOffClusterObj) {
                        if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                            zb_manager_on_off_cluster_update_attribute(ep->server_OnOffClusterObj, rep->attr.attr_id, rep->attr.attr_value);
                            DEVICE_ARRAY_UNLOCK();
                            zb_rule_trigger_state_update(
                                rep->src_address.u.short_addr,
                                rep->cluster,
                                rep->attr.attr_id,
                                rep->attr.attr_value,
                                rep->attr.attr_len,
                                rep->attr.attr_type
                            );
                            log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);
                            ws_notify_device_update(rep->src_address.u.short_addr);
                            // ✅ Уведомляем HA
                            ha_device_updated(dev, ep);
                        } else {
                            ESP_LOGE(TAG, "Failed to take mutex for On/Off update");
                            //zb_manager_free_report_attr_resp(rep);
                            report_attr_pool_put(rep);
                            rep = NULL;
                            break;
                        }
                    }
                }
                // 🔹 Неизвестный кластер
                else {
                    ESP_LOGW(TAG, "Unhandled cluster ID: 0x%04x", rep->cluster);
                }
            }

            // Освобождаем
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
                    device_custom_t *src_dev = zb_manager_find_device_by_short(req->src_short_addr);
                    if (!src_dev) {
                        ESP_LOGW(TAG, "❌ ZB_ACTION_DELAYED_BIND_DEV_DEV_REQ: src_device 0x%04x not found", req->src_short_addr);
                        break;
                    }
                    memcpy(src_ieee, src_dev->ieee_addr, 8);
                }else memcpy(src_ieee, LocalIeeeAdr, 8);

                if (req->dst_short_addr != 0x0000)
                {
                    device_custom_t *dst_dev = zb_manager_find_device_by_short(req->dst_short_addr);
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
                    ESP_LOGI(TAG, "✅ BIND request sent: cluster=0x%04x, to dev=0x%04x (src dev=0x%04x -bind-> dst dev=0x%04x  )", req->cluster_id, req->src_short_addr, req->src_short_addr, req->dst_short_addr);
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
                device_custom_t *dev = NULL;      
                dev = zb_manager_find_device_by_short(req->short_addr);
                if (!dev) {
                    ESP_LOGW(TAG, "DELAYED_CONFIG_REPORT_REQ_LOCAL: device 0x%04x not found", req->short_addr);
                    break;
                }

                ESP_LOGI(TAG, "✅ CONFIGURE_REPORT: short=0x%04x, ep=%d, cluster=0x%04x", req->short_addr, req->src_endpoint, req->cluster_id);
                // вызываем configure_reporting в зависимости от кластера
                if (req->src_endpoint == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT) 
                {
                    ESP_LOGI(TAG, "🌡️ Queued CONFIG_REPORT request for cluster ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT (0x0402)  (ep=%d)", req->src_endpoint);
                    zb_manager_configure_reporting_temperature(req->short_addr, req->src_endpoint);
                }
                else if (req->cluster_id == ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT) 
                {
                    ESP_LOGI(TAG, "🌡️ Queued CONFIG_REPORT request for cluster ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT (0x0405)  (ep=%d)", req->src_endpoint);
                    zb_manager_configure_reporting_humidity(req->short_addr, req->src_endpoint);
                }
                else if (req->cluster_id  == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) 
                {
                    ESP_LOGI(TAG, "🌡️ Queued CONFIG_REPORT request for cluster ESP_ZB_ZCL_CLUSTER_ID_ON_OFF (0x0006)  (ep=%d)", req->src_endpoint);
                    zb_manager_configure_reporting_onoff(req->short_addr, req->src_endpoint);
                }
                else if (req->cluster_id == ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG) 
                {
                    ESP_LOGI(TAG, "🌡️ Queued CONFIG_REPORT request for cluster ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG (0x0001)  (ep=%d)", req->src_endpoint);
                    zb_manager_configure_reporting_power(req->short_addr, req->src_endpoint);
                }
                //zb_manager_configure_reporting(req->short_addr, req->src_endpoint, req->cluster_id);
                free(req);
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


