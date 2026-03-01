#include "zb_manager_pairing.h"
#include "zb_manager_devices.h"
#include "zb_manager_tuya_dp.h"
#include "zb_manager_ncp_host.h"
//#include "zb_manager_devices_manufactory_table.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "web_server.h"
#include "ha_mqtt_publisher.h"
#include "esp_err.h"
#include "esp_check.h"
#include "zbm_dev_base.h"
#include "zbm_dev_base_utils.h"
#include "zbm_dev_polling.h"
#include "zbm_dev_append_sheduler.h"

static const char *TAG = "ZB_PAIRING_WORKER";

#define ZB_PAIRING_STACK_SIZE     4096
#define ZB_PAIRING_TASK_PRIORITY  20

static TaskHandle_t s_pairing_task_handle = NULL;
static QueueHandle_t s_pairing_queue = NULL;

// Internal message structure
typedef struct {
    int32_t event_id;
    void *event_data;
    size_t data_size;
} zb_pairing_msg_t;

static void appending_controll_timeout_CB(void *arg) {
    ESP_LOGW(TAG, "appending_controll_CB! TIMEOUT");
    if (arg != NULL)
    {
            device_appending_sheduler_t* shedule = (device_appending_sheduler_t*)arg;
            // Удаляем таймер
            if (shedule->appending_controll_timer) {
                esp_timer_stop(shedule->appending_controll_timer);
                esp_timer_delete(shedule->appending_controll_timer);
                shedule->appending_controll_timer = NULL;
            }
            // Отправляем событие в основную задачу
            //eventLoopPost(ZB_HANDLER_EVENTS, APPENDING_TIMEOUT_EVENT, shedule, 0, 0);
            ESP_LOGW(TAG, "APPENDING_TIMEOUT_EVENT sent to main task");
    }
}
static void StartUpdateDevices(device_appending_sheduler_t* shedule)
{
    if(shedule!= NULL)
    {
        shedule->tuya_magic_status = 0;
        shedule->tuya_magic_read_attr_cmd_param.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
        shedule->tuya_magic_read_attr_cmd_param.zcl_basic_cmd.src_endpoint = 1;
        shedule->tuya_magic_read_attr_cmd_param.zcl_basic_cmd.dst_endpoint = 1;
        shedule->tuya_magic_read_attr_cmd_param.zcl_basic_cmd.dst_addr_u.addr_short = shedule->appending_device->short_addr;
        shedule->tuya_magic_read_attr_cmd_param.clusterID = ESP_ZB_ZCL_CLUSTER_ID_BASIC;
        uint16_t attributes[] = {
                ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID,ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID,
                ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, 0xfffe,
        };
        shedule->tuya_magic_read_attr_cmd_param.attr_number = sizeof(attributes) / sizeof(attributes[0]);
        shedule->tuya_magic_read_attr_cmd_param.attr_field = calloc(1,sizeof(attributes[0]) * shedule->tuya_magic_read_attr_cmd_param.attr_number);
        memcpy(shedule->tuya_magic_read_attr_cmd_param.attr_field, attributes, sizeof(uint16_t) * shedule->tuya_magic_read_attr_cmd_param.attr_number);
        shedule->tuya_magic_req_tsn = 0xff;

        // запускаем контрольный таймер
        if (shedule->appending_controll_timer != NULL) esp_timer_delete(shedule->appending_controll_timer);
        const esp_timer_create_args_t appending_controll_timer_args = {
            .callback = &appending_controll_timeout_CB,
            .name = "one-shot",
            .arg = shedule,
        };
        ESP_ERROR_CHECK(esp_timer_create(&appending_controll_timer_args, &shedule->appending_controll_timer));
        ESP_ERROR_CHECK(esp_timer_start_once(shedule->appending_controll_timer, 60000 * 1000)); // запускаем на 10 секунд
        ESP_LOGW(TAG, "shedule->appending_controll_timer started");
        // отправляем запрос
        shedule->tuya_magic_req_tsn = 0xff;
        shedule->tuya_magic_req_tsn = zb_manager_zcl_read_attr_cmd_req(&shedule->tuya_magic_read_attr_cmd_param);
        if(shedule->tuya_magic_req_tsn == 0xff)
            {
                ESP_LOGW(TAG, "ERROR SENDING TUYA_MAGIC Break!!!");
                 if (shedule->appending_controll_timer) {
                    esp_timer_stop(shedule->appending_controll_timer);
                    esp_timer_delete(shedule->appending_controll_timer);
                    shedule->appending_controll_timer = NULL;
                }
                free(shedule->tuya_magic_read_attr_cmd_param.attr_field);
                zbm_DeviceAppendShedulerArray[shedule->index_in_sheduler_array] = NULL;
                free(shedule);
                shedule = NULL;
                return;
            } else 
            {
                ESP_LOGW(TAG, "TUYA_MAGIC SEND OK with tsn %d", shedule->tuya_magic_req_tsn);
                shedule->tuya_magic_status = 1;
                // Формируем запрос active_ep_req
                shedule->active_ep_req.addr_of_interest = shedule->appending_device->short_addr;
                shedule->active_ep_req_status = 0;
            }
    }
}

static void zb_pairing_worker_task(void *pvParameters)
{
    zb_pairing_msg_t msg;
    ESP_LOGI(TAG, "Pairing worker started");

    while (1) {
        if (xQueueReceive(s_pairing_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }

        if (!msg.event_data) {
            free(msg.event_data);
            continue;
        }

        //device_appending_sheduler_t *sheduler = NULL;
        device_custom_t *dev_info = NULL;
        zb_manager_cmd_read_attr_resp_message_t *read_resp = NULL;
        zb_manager_active_ep_resp_message_t *ep_resp = NULL;
        //zb_manager_simple_desc_resp_message_t *simple_desc_resp = NULL;
        //zb_manager_node_desc_resp_message_t* node_desc_resp = NULL;
        delayed_simple_desc_req_t* delayed_simple_desc_req = NULL;
        int64_t start = esp_timer_get_time();
        switch (msg.event_id) {
            case ZB_PAIRING_DEVICE_ASSOCIATED_EVENT: {
                ESP_LOGI(TAG, "✅ ZB_PAIRING_DEVICE_ASSOCIATED_EVENT: processing pairing");
                zb_pairing_device_associated_t* data = (zb_pairing_device_associated_t*)msg.event_data;
                if (data == NULL) {
                    ESP_LOGW(TAG, "ZB_PAIRING_DEVICE_ASSOCIATED_EVENT: data is NULL");
                    break;
                }
                ESP_LOGI(TAG, "ZB_PAIRING_DEVICE_ASSOCIATED_EVENT: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                    data->device_addr[7], data->device_addr[6], data->device_addr[5], data->device_addr[4],
                    data->device_addr[3], data->device_addr[2], data->device_addr[1], data->device_addr[0]);
                // Ищем устройство среди существующих, если не найдено, удаляем
                
                device_custom_t* device = NULL;
                device = zbm_dev_base_find_device_by_long_safe(&data->device_addr);
                esp_err_t delete_res = ESP_OK;
                if(device)
                {
                    ESP_LOGW(TAG, "ZB_PAIRING_DEVICE_ASSOCIATED_EVENT  устройство найдено по ieee удаляем старую запись");
                    esp_err_t delete_res = zbm_dev_base_dev_delete_safe(device);
                }
                if (delete_res != ESP_OK)
                {
                    ESP_LOGW(TAG, "ZB_PAIRING_DEVICE_ASSOCIATED_EVENT  устройство найдено по ieee но удалить старую запись не удалось");
                }
                // создаём временное устройство внутри шедулера для процесса сопряжения
                ESP_LOGI (TAG, "ZB_PAIRING_DEVICE_ASSOCIATED_EVENT: создаём временное устройство в шедулере");
                // готовим Шедулер для добавления устройства
                device_appending_sheduler_t* appending_ctx = calloc(1, sizeof(device_appending_sheduler_t));
                if (appending_ctx == NULL)
                {
                    ESP_LOGE(TAG, "ZB_PAIRING_DEVICE_ASSOCIATED_EVENT Failed to create appending_ctx");
                    break;
                }
                    
                appending_ctx->index_in_sheduler_array = 0xff;
                        
                // устанавливаем индекс задачи (ищем свободный индекс)
                for(int i = 0; i < zbm_DeviceAppendShedulerCount; i++)
                {
                    if (zbm_DeviceAppendShedulerArray[i] == NULL) 
                    {
                        zbm_DeviceAppendShedulerArray[i] = appending_ctx;
                        appending_ctx->index_in_sheduler_array = i;
                        break;
                    }
                }
                if(appending_ctx->index_in_sheduler_array == 0xff) 
                {
                    ESP_LOGW(TAG, "ZB_PAIRING_DEVICE_ASSOCIATED_EVENT: DeviceAppendShedulerArray is full!!!");
                    free(appending_ctx);
                    appending_ctx = NULL;
                    break;
                }
                //создаём временное устройство
                device = zbm_dev_base_create_new_device_obj(data->device_addr);
                if (device == NULL)
                {
                    ESP_LOGE(TAG, "ZB_PAIRING_DEVICE_ASSOCIATED_EVENT Failed to create device");
                    break;
                }
                device->is_in_build_status = 1;
                device->endpoints_count = 0;
                ESP_LOGI(TAG, "ZB_PAIRING_DEVICE_ASSOCIATED_EVENT Создано пустое устройство в Шедулере (mac: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)",device->ieee_addr[7], 
                        device->ieee_addr[6], device->ieee_addr[5], device->ieee_addr[4],
                        device->ieee_addr[3], device->ieee_addr[2], device->ieee_addr[1], device->ieee_addr[0]);
                appending_ctx->appending_device = device;
                appending_ctx->tuya_magic_try_count = 1;
                break;   
            }
            
            case ZB_PAIRING_DEVICE_UPDATE_EVENT:{
                ESP_LOGI(TAG, "✅ ZB_PAIRING_DEVICE_UPDATE_EVENT: processing pairing");
                zb_pairing_device_update_t* data = (zb_pairing_device_update_t*)msg.event_data;
                if (data == NULL) {
                    ESP_LOGW(TAG, "ZB_PAIRING_DEVICE_UPDATE_EVENT: data is NULL");
                    break;
                }
                
                // ищем в массиве Шедулера, если находим, то продолжаем добавление иначе просто сообщаем, что устройство на связи

                //1. ищем в шедулере
                device_appending_sheduler_t* appending_ctx = NULL;
                for(int i = 0; i < zbm_DeviceAppendShedulerCount; i++)
                {
                    if (zbm_DeviceAppendShedulerArray[i] == NULL) continue;
                    if (zbm_DeviceAppendShedulerArray[i]->appending_device == NULL) continue;
                    if (ieee_addr_compare(&zbm_DeviceAppendShedulerArray[i]->appending_device->ieee_addr, &data->params.long_addr) == 0)
                    {
                        appending_ctx = zbm_DeviceAppendShedulerArray[i];
                        appending_ctx->index_in_sheduler_array = i;
                        appending_ctx->appending_device->short_addr = data->params.short_addr;
                        appending_ctx->appending_device->parent_short_addr = data->params.parent_short;
                        break;
                    }else ESP_LOGW(TAG,"(ieee_addr_compare(&zbm_DeviceAppendShedulerArray[i]->appending_device->ieee_addr, &data->params.long_addr) != 0)");
                    
                }
                //2. Если не нашли
                if (appending_ctx == NULL)
                {
                    device_custom_t* device = NULL;
                    device = zbm_dev_base_find_device_by_short_safe (data->params.short_addr);
                    if (device != NULL)
                    {
                        ESP_LOGI(TAG, "ZB_PAIRING_DEVICE_UPDATE_EVENT: 0x%04x re-joined the network and parent is 0x%04x ", data->params.short_addr, data->params.parent_short);
                        device->parent_short_addr = data->params.parent_short;
                        device->last_seen_ms = esp_log_timestamp();
                        device->is_online = true;
                        //zb_manager_update_device_activity(device->short_addr, 10);
                    }
                    break;
                }
                //3. Если нашли, значит продолжаем добавление
                StartUpdateDevices(appending_ctx);
                break;
            }

            case ZB_PAIRING_DEVICE_ANNCE_EVENT:{
                
            break;
            }

            case ZB_PAIRING_DEVICE_AUTHORIZED_EVENT:{
                
            break;
            }

        case ZB_PAIRING_ATTR_READ_RESP: {
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

            // Поиск sheduler
            device_appending_sheduler_t *sheduler = NULL;
            if (zbm_DeviceAppendShedulerArray == NULL)
            {
                ESP_LOGW(TAG,"DeviceAppendShedulerArray == NULL");
            }
            for (int i = 0; i < zbm_DeviceAppendShedulerCount; i++) {
                if (zbm_DeviceAppendShedulerArray[i] == NULL)
                {
                    ESP_LOGW(TAG,"DeviceAppendShedulerArray[i] == NULL");
                }
                if (zbm_DeviceAppendShedulerArray[i]->appending_device == NULL)
                {
                    ESP_LOGW(TAG,"DeviceAppendShedulerArray[i]->appending_device == NULL");
                }
                if (zbm_DeviceAppendShedulerArray[i]->appending_device->short_addr == cmd_info->src_address.u.short_addr)
                {
                    sheduler = zbm_DeviceAppendShedulerArray[i];
                    break;
                }
            }

            if (!sheduler) {
                ESP_LOGE(TAG, "Sheduler not found");
                zb_manager_free_read_attr_resp_attr_array(read_resp);
                read_resp = NULL;
                //free(input_copy);
                break;
            }

            device_custom_t* device = NULL;
            device = sheduler->appending_device;
            if (device == NULL)
            {
                ESP_LOGE(TAG, "sheduler->appending_device not found");
                zb_manager_free_read_attr_resp_attr_array(read_resp);
                read_resp = NULL;
                //free(input_copy);
                break;
            }

            if (sheduler && sheduler->tuya_magic_status == 1) {
                ESP_LOGW(TAG, "TUYA_MAGIC response with TSN %d", read_resp->info.header.tsn);
                ESP_LOGW(TAG, "Device found in DeviceAppendShedulerArray !!! (short: 0x%04x)", device->short_addr);
                ESP_LOGW(TAG, "Продолжаем добавление устройства — переходим к active_ep_req");

                // PAIRING_READ_RESP может прислать ответ только от Basic кластера, это запрорс TuyaMagic
                if (device->server_BasicClusterObj == NULL) {
                        device->server_BasicClusterObj = calloc(1, sizeof(zigbee_manager_basic_cluster_t));
                        if (device->server_BasicClusterObj) {
                            zigbee_manager_basic_cluster_t cl = ZIGBEE_BASIC_CLUSTER_DEFAULT_INIT();
                            memcpy(device->server_BasicClusterObj, &cl, sizeof(zigbee_manager_basic_cluster_t));
                            ESP_LOGI(TAG, "Basic Cluster added to device");

                            for (int i = 0; i < read_resp->attr_count; i++) {
                                void *value = read_resp->attr_arr[i].attr_value;
                                zb_manager_basic_cluster_update_attribute(device->server_BasicClusterObj,
                                                                read_resp->attr_arr[i].attr_id, read_resp->attr_arr[i].attr_type,
                                                                value, read_resp->attr_arr[i].attr_len);
                                ESP_LOGI(TAG, "Basic Cluster attribute 0x%04x updated", read_resp->attr_arr[i].attr_id);
                            }

                            const char *text = get_power_source_string(device->server_BasicClusterObj->power_source);
                            ESP_LOGI(TAG, "Power Source: %u (%s)", device->server_BasicClusterObj->power_source, text);
                        }
                }
                sheduler->tuya_magic_status = 2;
                sheduler->active_ep_req_status = 1;
                sheduler->active_ep_req.addr_of_interest = device->short_addr;
                // Отправляем ActiveEp
                if(zb_manager_zdo_active_ep_req(&sheduler->active_ep_req, NULL, sheduler)==ESP_OK){
                        ESP_LOGI(TAG, "ZDO_ACTIVE_EP_REQ sent");
                }

                zb_manager_free_read_attr_resp_attr_array(read_resp);
                read_resp = NULL;
                break;
            }
            break;
        }
        case ZB_PAIRING_ATTR_REPORT_EVENT: {
            ESP_LOGI(TAG, "✅ ZB_PAIRING_ATTR_REPORT_EVENT: processing pairing");
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
            zb_manager_cmd_report_attr_resp_message_t *rep = calloc(1,sizeof(zb_manager_cmd_report_attr_resp_message_t));
            if (!rep) {
                ESP_LOGE(TAG, "Failed to calloc rep");
                break;
            }
            memset(rep, 0, sizeof(zb_manager_cmd_report_attr_resp_message_t));

            rep->status = report_attr->status;
            if (rep->status != ESP_ZB_ZCL_STATUS_SUCCESS) {
                ESP_LOGW(TAG, "ATTR_REPORT_EVENT: status 0x%02x", rep->status);
                free(rep);
                rep = NULL;
            }
            memcpy(&rep->src_address, &report_attr->src_address, sizeof(esp_zb_zcl_addr_t));
            rep->src_endpoint = report_attr->src_endpoint;
            rep->dst_endpoint = report_attr->dst_endpoint;
            rep->cluster = report_attr->cluster;
            rep->attr.attr_id = attr_data->id;
            rep->attr.attr_type = attr_data->type;
            rep->attr.attr_len = attr_data->size;

            const uint8_t *value_src = raw + sizeof(*report_attr) + sizeof(*attr_data);
            const uint8_t *buffer_end = raw + len;
            if(rep->attr.attr_len > 0)
            {
                rep->attr.attr_value = calloc(1,rep->attr.attr_len);
                memcpy(rep->attr.attr_value, value_src, rep->attr.attr_len);
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
            zb_manager_free_report_attr_resp(rep);
            rep = NULL;
            break;
        }
        case ZB_PAIRING_ACTIVE_EP_RESP: {
            ESP_LOGI(TAG, "✅ ZB_PAIRING_ACTIVE_EP_RESP: processing pairing");
            ep_resp = (zb_manager_active_ep_resp_message_t *)msg.event_data;
            if (ep_resp->status == ESP_ZB_ZDP_STATUS_SUCCESS && ep_resp->user_ctx) {
                device_appending_sheduler_t *sheduler = NULL;
                sheduler = (device_appending_sheduler_t *)ep_resp->user_ctx;
                ESP_LOGI(TAG, "ACTIVE_EP_RESP: sheduler=%p, appending_device=%p", sheduler, sheduler->appending_device);
                if(!sheduler)
                {
                    break;
                }

                device_custom_t* device = NULL;
                device = sheduler->appending_device;
                if (!device)
                {
                    break;
                }

                sheduler->active_ep_req_status = 2;
                // Запрос node descriptor отправляем сразу
                uint16_t node_desc_par = device->short_addr;
                ESP_LOGI(TAG, "📤 Queuing NODE_DESC event with ID=%d for device_short 0x%04x", ZB_ACTION_DELAYED_NODE_DESC_REQ, device->short_addr);
                if (zb_manager_post_to_pairing_worker(ZB_PAIRING_DELAYED_NODE_DESC_REQ, &node_desc_par, sizeof(uint16_t)) == true)
                {
                    ESP_LOGI(TAG, "✅ ZB_ACTION_DELAYED_NODE_DESC_REQ Posted to pairing worker");
                    //ESP_ERROR_CHECK(esp_timer_restart(sheduler->appending_controll_timer, 10000 * 1000));
                } else {
                        ESP_LOGE(TAG, "❌ Failed to post ZB_ACTION_DELAYED_NODE_DESC_REQ to pairing worker");
                }

                // готовим simple_desc_req
                sheduler->simple_desc_req_count = ep_resp->ep_count;
                sheduler->simple_desc_req_list = calloc(sheduler->simple_desc_req_count, sizeof(local_esp_zb_zdo_simple_desc_req_param_t));
                sheduler->simple_desc_req_simple_desc_req_list_status = calloc(sheduler->simple_desc_req_count, sizeof(uint8_t));

                device->endpoints_count = ep_resp->ep_count; // ep добавляем все!!!
                device->endpoints_array = calloc(device->endpoints_count, sizeof(endpoint_custom_t*));

                for (int i = 0; i < ep_resp->ep_count; i++) {
                    device->endpoints_array[i] = RemoteDeviceEndpointCreate(ep_resp->ep_list[i]);   
                    // заполняем шедулер кроме epid = 242 (0xf2), для неё сразу статус выстявляем 2
                    if (ep_resp->ep_list[i] != 0xf2) {
                        sheduler->simple_desc_req_list[i].addr_of_interest = device->short_addr;
                        sheduler->simple_desc_req_list[i].endpoint = ep_resp->ep_list[i];
                        sheduler->simple_desc_req_simple_desc_req_list_status[i] = 1;
                        delayed_simple_desc_req_t delayed_simple_desc_req;
                        delayed_simple_desc_req.short_addr = sheduler->simple_desc_req_list[i].addr_of_interest;
                        delayed_simple_desc_req.src_endpoint = sheduler->simple_desc_req_list[i].endpoint;
                        device_appending_sheduler_t * sheduler_p_copy;
                        sheduler_p_copy = sheduler;
                        delayed_simple_desc_req.ctx = sheduler_p_copy;
                        delayed_simple_desc_req.user_cb = NULL;
                        if (zb_manager_post_to_pairing_worker(ZB_PAIRING_DELAYED_SIMPLE_DESC_REQ, &delayed_simple_desc_req, sizeof(delayed_simple_desc_req_t)) == true)
                        {
                            ESP_LOGI(TAG, "✅ ZB_PAIRING_DELAYED_SIMPLE_DESC_REQ Posted to pairing worker for short 0x%04x ep 0x%2x ", delayed_simple_desc_req.short_addr, delayed_simple_desc_req.src_endpoint);
                            //ESP_ERROR_CHECK(esp_timer_restart(sheduler->appending_controll_timer, 10000 * 1000));
                        } else {
                            ESP_LOGE(TAG, "❌ Failed to post ZB_PAIRING_DELAYED_SIMPLE_DESC_REQ to pairing worker");
                        }
                    } else {
                        // сразу ставим статус выполнено, вдруг 242 будет не в конце Active_ep респонза
                        sheduler->simple_desc_req_simple_desc_req_list_status[i] = 2;
                    }
                }
            }
            break;
        }      
        
        case ZB_PAIRING_DELAYED_SIMPLE_DESC_REQ:{
            ESP_LOGI(TAG, "✅ ZB_PAIRING_DELAYED_SIMPLE_DESC_REQ: processing pairing");
            delayed_simple_desc_req = NULL;
            delayed_simple_desc_req = (delayed_simple_desc_req_t *)msg.event_data;
            device_appending_sheduler_t *sheduler = NULL;
            sheduler = (device_appending_sheduler_t *)delayed_simple_desc_req->ctx;
            device_appending_sheduler_t * sheduler_p_copy = sheduler;

            ESP_LOGW(TAG, "✅ ZB_PAIRING_DELAYED_SIMPLE_DESC_REQ: sheduler %p", sheduler);
            ESP_LOGW(TAG, "✅ ZB_PAIRING_DELAYED_SIMPLE_DESC_REQ: sheduler_p_copy %p", sheduler_p_copy);
            ESP_LOGI(TAG, "DELAYED_SIMPLE_DESC_REQ: sheduler=%p, appending_device=%p", sheduler, sheduler->appending_device);
            local_esp_zb_zdo_simple_desc_req_param_t simple_desc_req;
            simple_desc_req.addr_of_interest = delayed_simple_desc_req->short_addr;
            simple_desc_req.endpoint = delayed_simple_desc_req->src_endpoint;
                
            if (zb_manager_zdo_simple_desc_req(&simple_desc_req, NULL, sheduler_p_copy) == ESP_OK){
                ESP_LOGI(TAG, "✅ Simple desc req sent to device 0x%04x ep 0x%2x", simple_desc_req.addr_of_interest, simple_desc_req.endpoint);
                //ESP_ERROR_CHECK(esp_timer_restart(sheduler->appending_controll_timer, 10000 * 1000));
            }else {
                ESP_LOGW(TAG, "❌ Simple desc req failed to device 0x%04x ep 0x%2x", simple_desc_req.addr_of_interest, simple_desc_req.endpoint);
            }
            break;
        }
        case ZB_PAIRING_DELAYED_NODE_DESC_REQ:{
            ESP_LOGI(TAG, "✅ ZB_PAIRING_DELAYED_NODE_DESC_REQ: processing pairing");
            uint16_t short_addr; // = *(uint16_t *)msg.event_data;
            memcpy(&short_addr, msg.event_data, sizeof(uint16_t));
            if (zb_manager_zdo_node_desc_req(short_addr) == ESP_OK)
            {
                ESP_LOGI(TAG, "Node desc req sent to device 0x%04x", short_addr);
                //if (sheduler != NULL) ESP_ERROR_CHECK(esp_timer_restart(sheduler->appending_controll_timer, 10000 * 1000));
            }else {
                 ESP_LOGW(TAG, "Node desc req failed to device 0x%04x", short_addr);
            }
            break;
        }
        case ZB_PAIRING_NODE_DESC_RESP:{
            ESP_LOGI(TAG, "✅ ZB_PAIRING_DELAYED_NODE_DESC_REQ: processing pairing");
            zb_manager_node_desc_resp_message_t* node_desc_resp = NULL;
            node_desc_resp = (zb_manager_node_desc_resp_message_t *)msg.event_data;
            
            device_appending_sheduler_t *sheduler = NULL;
            device_custom_t *dev = NULL; 

            for (int i = 0; i < zbm_DeviceAppendShedulerCount; i++) {
                if (zbm_DeviceAppendShedulerArray[i] == NULL) continue;
                if (zbm_DeviceAppendShedulerArray[i]->appending_device == NULL) continue;
                if (zbm_DeviceAppendShedulerArray[i]->appending_device->short_addr == node_desc_resp->short_addr)
                {
                    sheduler = zbm_DeviceAppendShedulerArray[i];
                    dev = sheduler->appending_device;
                    break;
                }
            }

            if (!dev) {
                ESP_LOGW(TAG, "ZB_PAIRING_NODE_DESC_RESP: Device not found for short_addr 0x%04x", node_desc_resp->short_addr);
                break;
            }

            // выставляем manufactory_code
            uint16_t manuf_code = 0;

            if (node_desc_resp->status != ESP_ZB_ZDP_STATUS_SUCCESS) 
            {
                local_esp_zb_af_node_desc_t node_desc = {
                    .node_desc_flags = node_desc_resp->node_desc.node_desc_flags,
                    .mac_capability_flags = node_desc_resp->node_desc.mac_capability_flags,
                    .manufacturer_code = node_desc_resp->node_desc.manufacturer_code,
                    .max_buf_size = node_desc_resp->node_desc.max_buf_size,
                    .max_incoming_transfer_size = node_desc_resp->node_desc.max_incoming_transfer_size,
                    .server_mask = node_desc_resp->node_desc.server_mask,
                    .max_outgoing_transfer_size = node_desc_resp->node_desc.max_outgoing_transfer_size,
                    .desc_capability_field = node_desc_resp->node_desc.desc_capability_field,
                };
                manuf_code = zbm_base_get_manuf_code_by_priority(dev->ieee_addr, &node_desc, dev->server_BasicClusterObj ? dev->server_BasicClusterObj->manufacturer_name : NULL);
                ESP_LOGI(TAG, "Resolved manufacturer_code via node_desc: 0x%04x", manuf_code);
            }else{ // статус неудачный, не все устройства отвечают на NodeDesc, ставим manuf_code другим способом
                ESP_LOGW(TAG, "ZB_PAIRING_NODE_DESC_RESP failed for 0x%04x, status=0x%02x", node_desc_resp->short_addr, node_desc_resp->status);
                // Попробуем по Basic Cluster и OUI
                manuf_code = zbm_base_get_manuf_code_by_priority(dev->ieee_addr,NULL, dev->server_BasicClusterObj ? dev->server_BasicClusterObj->manufacturer_name : NULL);
                if (manuf_code == 0) {
                    ESP_LOGW(TAG, "Failed to resolve manufacturer_code for 0x%04x", node_desc_resp->short_addr);
                }
            }
            // Сохраняем результат
            if (manuf_code != 0) {
                dev->manufacturer_code = manuf_code;
            }
            break;
        }
        case ZB_PAIRING_SIMPLE_DESC_RESP:{
            ESP_LOGI(TAG, "✅ SIMPLE_DESC_RESP: processing pairing");
            zb_manager_simple_desc_resp_message_t * local_simple_desc_resp = (zb_manager_simple_desc_resp_message_t *)msg.event_data;
            if (local_simple_desc_resp->status == ESP_ZB_ZDP_STATUS_SUCCESS){   
                device_appending_sheduler_t *sheduler = NULL;
                local_esp_zb_af_simple_desc_1_1_t *desc = (local_esp_zb_af_simple_desc_1_1_t *)local_simple_desc_resp->simple_desc;
                // 🔐 Проверка: user_ctx — это валидный указатель?
                void *raw_ctx = local_simple_desc_resp->user_ctx;
                ESP_LOGI(TAG, "SIMPLE_DESC_RESP: raw_ctx=%p", raw_ctx);
                if (!raw_ctx) {
                    ESP_LOGE(TAG, "❌ user_ctx is NULL");
                    break;
                }
                //ESP_LOGW(TAG, "✅ SIMPLE_DESC_RESP: raw_ctx %p", raw_ctx);
 
                sheduler = (device_appending_sheduler_t *)raw_ctx;
                if (!sheduler->appending_device) {
                        ESP_LOGE(TAG, "❌ SIMPLE_DESC_RESP: sheduler is NULL");
                        break;
                    }
                ESP_LOGI(TAG, "SIMPLE_DESC_RESP: sheduler=%p, appending_device=%p", sheduler, sheduler ? sheduler->appending_device : NULL);
                ESP_LOGW(TAG, "SIMPLE_DESC_RESP: sheduler->simple_desc_req_count=%d", sheduler->simple_desc_req_count);
                ESP_LOGW(TAG, "SIMPLE_DESC_RESP: simple_desc_resp->simple_desc->endpoint %d", desc->endpoint);
                    // Найти и отметить выполнение
                    //ESP_ERROR_CHECK(esp_timer_restart(sheduler->appending_controll_timer, 10000 * 1000));
                for (int i = 0; i < sheduler->simple_desc_req_count; i++) {
                    
                    ESP_LOGW(TAG, "SIMPLE_DESC_RESP: endpoint %d", sheduler->simple_desc_req_list[i].endpoint);
                    //ESP_LOGW(TAG, "SIMPLE_DESC_RESP: simple_desc_resp->simple_desc->endpoint %d", desc->endpoint);
                    if (sheduler->simple_desc_req_list[i].endpoint == desc->endpoint) {
                        sheduler->simple_desc_req_simple_desc_req_list_status[i] = 2;
                        ESP_LOGW(TAG, "SIMPLE_DESC_RESP: endpoint %d marked as processed and build status set 2", desc->endpoint);
                        break;
                    }
                }
  
                // Добавить endpoint (эндпоинты надо создавать в active_ep_resp, чтобы избежать realloc)
                endpoint_custom_t* endpoint = NULL;
                if ((sheduler->appending_device->endpoints_count > 0)&&(sheduler->appending_device->endpoints_array != NULL))
                {
                    for (int i = 0; i < sheduler->appending_device->endpoints_count; i++)
                    {
                        endpoint_custom_t* temp_ep = NULL;
                        temp_ep = sheduler->appending_device->endpoints_array[i];
                        if (temp_ep->ep_id == desc->endpoint)
                        {
                            endpoint = temp_ep;
                            // === УСТАНОВКА ТАЙМАУТА ПРИ ПЕРВОМ ЭНДПОИНТЕ ===
                            if (i == 0)
                            {
                                uint32_t timeout_ms = zbm_dev_get_timeout_for_device_id(desc->app_device_id);
                                sheduler->appending_device->device_timeout_ms = timeout_ms;
                                ESP_LOGI(TAG, "Device 0x%04x: device_type=0x%04x timeout=%" PRIu32 " ms",sheduler->appending_device->short_addr, desc->app_device_id, timeout_ms);
                            }
                            break;
                        }
                    }
                }

                if (!endpoint)
                {
                    ESP_LOGW (TAG, "Device 0x%04x: endpoint object with ep_id 0x%02x not found", sheduler->appending_device->short_addr, desc->endpoint);
                    break;
                }

                // заполняем endpoint
                endpoint->is_use_on_device = 1;
                memset(endpoint->friendly_name,0,sizeof(endpoint->friendly_name));
                sprintf(endpoint->friendly_name, "[0x%4x] [0x%02x]",sheduler->appending_device->short_addr, desc->endpoint);
                endpoint->deviceId = desc->app_device_id;

                uint16_t *clusters = desc->app_cluster_list;
                if (desc->app_input_cluster_count > 0)
                {
                    for (int i = 0; i < desc->app_input_cluster_count; i++) 
                    {
                        ESP_LOGI(TAG, "  Input Cluster: 0x%04x", clusters[i]);
                        if (clusters[i] == 0x0003) 
                            {
                                if(endpoint->server_IdentifyClusterObj  == NULL)
                                {
                                    endpoint->is_use_identify_cluster = 1;
                                    endpoint->server_IdentifyClusterObj = calloc(1,sizeof(zb_manager_identify_cluster_t));
                                    zb_manager_identify_cluster_t cl = ZIGBEE_IDENTIFY_CLUSTER_DEFAULT_INIT();
                                    memcpy(endpoint->server_IdentifyClusterObj, &cl, sizeof(zb_manager_identify_cluster_t));
                                    ESP_LOGI(TAG, "  Identify Cluster added");
                                    continue;
                                }
                            } else if (clusters[i] == 0x0402) 
                            {
                                if (endpoint->server_TemperatureMeasurementClusterObj == NULL)
                                {
                                    endpoint->is_use_temperature_measurement_cluster = 1;
                                    endpoint->server_TemperatureMeasurementClusterObj = calloc(1,sizeof(zb_manager_temperature_measurement_cluster_t));
                                    zb_manager_temperature_measurement_cluster_t cl = ZIGBEE_TEMP_MEASUREMENT_CLUSTER_DEFAULT_INIT();
                                    memcpy(endpoint->server_TemperatureMeasurementClusterObj, &cl, sizeof(zb_manager_temperature_measurement_cluster_t));
                                    ESP_LOGI(TAG, "  Temperature Measurement Cluster added 0x0402");
                                    continue;
                                }
                            } else if (clusters[i] == 0x0405)
                            {
                                if (endpoint->server_HumidityMeasurementClusterObj == NULL)
                                {
                                    endpoint->is_use_humidity_measurement_cluster = true;
                                    endpoint->server_HumidityMeasurementClusterObj = calloc(1, sizeof(zb_manager_humidity_measurement_cluster_t));
                                    zb_manager_humidity_measurement_cluster_t cl = ZIGBEE_HUMIDITY_MEASUREMENT_CLUSTER_DEFAULT_INIT();
                                    memcpy(endpoint->server_HumidityMeasurementClusterObj, &cl, sizeof(zb_manager_humidity_measurement_cluster_t));
                                    ESP_LOGI(TAG, "  Humidity Measurement Cluster added 0x0405");
                                    continue;
                                }
                            } else if (clusters[i] == 0x0006)
                            {
                                if (endpoint->server_OnOffClusterObj == NULL)
                                    {
                                        endpoint->is_use_on_off_cluster = true;
                                        endpoint->server_OnOffClusterObj = calloc(1, sizeof(zb_manager_on_off_cluster_t));
                                        zb_manager_on_off_cluster_t cl = ZIGBEE_ON_OFF_CLUSTER_DEFAULT_INIT();
                                        memcpy(endpoint->server_OnOffClusterObj, &cl, sizeof(zb_manager_on_off_cluster_t));
                                        ESP_LOGI(TAG, "  On/Off Cluster added 0x0006");
                                        endpoint->on_off_apply_cb = NULL;
                                        endpoint->on_off_user_data = NULL;
                                            // Установика callback для примера, не используется
                                            //temp_endpoint->on_off_apply_cb = [](bool on, void* ctx) {
                                            //gpio_set_level((int)ctx, on ? 1 : 0);
                                            //};
                                            //temp_endpoint->on_off_user_data = (void*)GPIO_NUM_12;
                                        continue;
                                    }
                            } else if (clusters[i] == 0x0001)
                            {
                                if (sheduler->appending_device->server_PowerConfigurationClusterObj == NULL) 
                                {
                                    sheduler->appending_device->server_PowerConfigurationClusterObj = calloc(1, sizeof(zb_manager_power_config_cluster_t));
                                    zb_manager_power_config_cluster_t cl = ZIGBEE_POWER_CONFIG_CLUSTER_DEFAULT_INIT();
                                    memcpy(sheduler->appending_device->server_PowerConfigurationClusterObj, &cl, sizeof(zb_manager_power_config_cluster_t));
                                    ESP_LOGI(TAG, "Power Configuration Cluster created at device level");
                                    continue;
                                }
                            } else {
                                if (clusters[i] == 0x0000)
                                {
                                    continue;
                                }
                                else
                                {
                                    // Создаём новый cluster_custom_t
                                    cluster_custom_t *custom_cluster = calloc(1, sizeof(cluster_custom_t));
                                    if (!custom_cluster) continue;
                                    custom_cluster->id = clusters[i];
                                    snprintf(custom_cluster->cluster_id_text, sizeof(custom_cluster->cluster_id_text),
                                                    "%s", zb_manager_get_cluster_name(clusters[i]) ?: "Unknown");
                                    custom_cluster->role_mask = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE; // или CLIENT?
                                    custom_cluster->manuf_code = 0; // может быть из других источников
                                    custom_cluster->attr_count = 0;
                                    custom_cluster->attr_array = NULL;
                                    custom_cluster->is_use_on_device = 1;
                                    if (endpoint->UnKnowninputClusters_array == NULL)
                                    {
                                        endpoint->UnKnowninputClusters_array = calloc(1, sizeof(cluster_custom_t*));
                                        endpoint->UnKnowninputClusters_array[0] = custom_cluster;
                                        endpoint->UnKnowninputClusterCount = 1;
                                    }else
                                    {
                                        endpoint->UnKnowninputClusters_array = realloc(endpoint->UnKnowninputClusters_array,
                                                    (endpoint->UnKnowninputClusterCount + 1) * sizeof(cluster_custom_t*));
                                        endpoint->UnKnowninputClusters_array[endpoint->UnKnowninputClusterCount] = custom_cluster;
                                        endpoint->UnKnowninputClusterCount++;
                                    }
                                }

                            }
                    } 
                }
                if (desc->app_output_cluster_count > 0) 
                {
                    endpoint->output_clusters_count = desc->app_output_cluster_count;
                    endpoint->output_clusters_array = calloc(desc->app_output_cluster_count, sizeof(uint16_t));
                
                    for (int i = 0; i < desc->app_output_cluster_count; i++) 
                    {
                        ESP_LOGI(TAG, "  Output Cluster: 0x%04x", clusters[desc->app_input_cluster_count + i]);
                        endpoint->output_clusters_array[i] = clusters[desc->app_input_cluster_count + i];
                        ESP_LOGI(TAG, "  Output Cluster output_clusters_array[i]: 0x%04x", endpoint->output_clusters_array[i]);
                    }
                    ESP_LOGI(TAG, "Endpoint 0x%02x on short_addr 0x%04x clusters added", desc->endpoint, sheduler->appending_device->short_addr);
                } else ESP_LOGI(TAG, "No output clusters for endpoint 0x%02x", desc->endpoint);
                        
                // находим запрос simple_desc и помечаем его выполненным
                for (int i = 0; i < sheduler->simple_desc_req_count; i++) 
                    {
                        if (sheduler->simple_desc_req_list[i].endpoint == desc->endpoint) 
                        {
                            sheduler->simple_desc_req_simple_desc_req_list_status[i] = 2;
                            break;
                        }
                    }
                // Проверить завершение
                bool all_done = true;
                for (int i = 0; i < sheduler->simple_desc_req_count; i++) {
                    if (sheduler->simple_desc_req_simple_desc_req_list_status[i] != 2) 
                    {
                        all_done = false;
                        break;
                    }
                }

                if (all_done) 
                {
                    ESP_LOGW(TAG, "All simple desc req done for device 0x%04x", sheduler->appending_device->short_addr);
                    ESP_LOGI(TAG, "✅ Device build complete: 0x%04x", sheduler->appending_device->short_addr);

                    sheduler->appending_device->is_in_build_status = 2;
                    zbm_dev_configure_device_timeout(sheduler->appending_device);
                        
                    // 🔧 Исправляем power_source
                    zb_manager_apply_device_fixups(sheduler->appending_device);
                    uint16_t short_addr = sheduler->appending_device->short_addr;
                    if (zbm_dev_base_dev_obj_append_safe(sheduler->appending_device) == ESP_OK)
                    {
                        // ✅ Уведомляем веб-интерфейс с задержкой, пусть сохраниться успеет: устройство готово!
                        //vTaskDelay(pdMS_TO_TICKS(300));
                        ws_notify_device_update(short_addr);
                        //zb_manager_queue_save_request();
                        zbm_dev_base_queue_save_req_cmd();
                        
                        // ✅ ПУБЛИКУЕМ ПОЛНОЕ ПЕРЕОПРЕДЕЛЕНИЕ В HA
                        //ha_mqtt_republish_discovery_for_device(sheduler->appending_device);
                    }else {
                        ESP_LOGE(TAG,"ERROR APPEND SHEDULER_DEVICE TO BASE");
                    }
                   
                    ESP_ERROR_CHECK(esp_timer_stop(sheduler->appending_controll_timer));
                        ESP_ERROR_CHECK(esp_timer_delete(sheduler->appending_controll_timer));
                    zbm_dev_delete_appending_sheduler(sheduler);
                    //sheduler = NULL;
                }
            } // end if ESP_ZB_ZDP_STATUS_SUCCESS
                else {
                    ESP_LOGW(TAG, "SIMPLE_DESC_RESP Status FAIL or INVALID input DATA");
                }
                //zb_manager_free_simple_desc_resp(simple_desc_resp);
                //free(simple_desc_resp);
                break;
            }
            default:
                ESP_LOGW(TAG, "Unhandled pairing event: %ld", msg.event_id);
                break;
        }

        free(msg.event_data);
        int64_t end = esp_timer_get_time();
        if ((end - start) > 100000) {
            ESP_LOGW(TAG, "⚠️ Event handler took %lld ms!", (end - start) / 1000);
        }else ESP_LOGW(TAG, " Event handler took %lld ms!", (end - start) / 1000);
    }
}

esp_err_t zb_manager_start_pairing_worker(uint8_t core)
{
    if (s_pairing_task_handle) {
        return ESP_OK; // уже запущен
    }

    s_pairing_queue = xQueueCreate(32, sizeof(zb_pairing_msg_t));
    if (!s_pairing_queue) {
        return ESP_ERR_NO_MEM;
    }

    static StackType_t pairing_stack[ZB_PAIRING_STACK_SIZE];
    static StaticTask_t pairing_task_buffer;

    s_pairing_task_handle = xTaskCreateStatic(
        zb_pairing_worker_task,
        "zb_pairing",
        ZB_PAIRING_STACK_SIZE,
        NULL,
        ZB_PAIRING_TASK_PRIORITY,
        pairing_stack,
        &pairing_task_buffer
    );

    return s_pairing_task_handle ? ESP_OK : ESP_FAIL;
}

bool zb_manager_post_to_pairing_worker(int32_t id, void *data, size_t size)
{
    if (!s_pairing_queue || !data) return false;

    zb_pairing_msg_t msg = {
        .event_id = id,
        .data_size = size,
        .event_data = calloc(1, size)
    };

    if (!msg.event_data) return false;

    memcpy(msg.event_data, data, size);

    return xQueueSend(s_pairing_queue, &msg, pdMS_TO_TICKS(50)) == pdTRUE;
}
