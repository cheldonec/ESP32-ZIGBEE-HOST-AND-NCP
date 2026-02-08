#include "zb_manager_pairing.h"
#include "zb_manager_devices.h"
#include "zb_manager_tuya_dp.h"
#include "zb_manager_ncp_host.h"
#include "zb_manager_devices_manufactory_table.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "web_server.h"
#include "ha_mqtt_publisher.h"
#include "esp_err.h"
#include "esp_check.h"

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
                DeviceAppendShedulerArray[shedule->index_in_sheduler_array] = NULL;
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

        device_appending_sheduler_t *sheduler = NULL;
        device_custom_t *dev_info = NULL;
        zb_manager_cmd_read_attr_resp_message_t *read_resp = NULL;
        zb_manager_active_ep_resp_message_t *ep_resp = NULL;
        //zb_manager_simple_desc_resp_message_t *simple_desc_resp = NULL;
        zb_manager_node_desc_resp_message_t* node_desc_resp = NULL;
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
                // Ищем устройство среди существующих, если не найдено, добавляем, если найдено, обновляем
                bool device_found = false;
                device_custom_t* device = NULL;
                if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                    for (int i = 0; i < RemoteDevicesCount; i++)
                    {
                        device = RemoteDevicesArray[i];
                        if (device != NULL)
                        {
                            if (ieee_addr_compare(&device->ieee_addr, &data->device_addr) == 0)
                            {
                                device_found = true;
                                //device->is_online = false;
                                device->is_in_build_status = 1;
                                ESP_LOGW(TAG, "ZB_PAIRING_DEVICE_ASSOCIATED_EVENT  device->build_status выставлен в 1 (режим добавления)");
                                if (device->server_BasicClusterObj != NULL) {
                                    free(device->server_BasicClusterObj);
                                    device->server_BasicClusterObj = NULL;
                                }
                                if (device->server_PowerConfigurationClusterObj != NULL) {
                                    free(device->server_PowerConfigurationClusterObj);
                                    device->server_PowerConfigurationClusterObj = NULL;
                                }
                                if (device->endpoints_count > 0) {
                                    for (int j = 0; j < device->endpoints_count; j++)
                                    {
                                        RemoteDeviceEndpointDelete(device->endpoints_array[j]);
                                    }
                                    device->endpoints_count = 0;
                                    free (device->endpoints_array);
                                    device->endpoints_array = NULL;
                                }
                                break;
                            }
                        }
                    }
                    if (device_found == false)
                    {
                        ESP_LOGI (TAG, "ZB_PAIRING_DEVICE_ASSOCIATED_EVENT: device not found, adding");
                        uint8_t free_index = REMOTE_DEVICES_COUNT + 1; // несуществующий индекс
                        for(int i = 0; i < REMOTE_DEVICES_COUNT; i++)
                        {  
                            if (RemoteDevicesArray[i] == NULL)
                            {
                                free_index = i;
                                break;
                            }
                        }
                        if (free_index < REMOTE_DEVICES_COUNT + 1)
                        {
                            RemoteDevicesArray[free_index] = RemoteDeviceCreate(data->device_addr);

                            device = (device_custom_t*)RemoteDevicesArray[free_index];
                            if (device != NULL)
                            {
                                device->is_in_build_status = 1;
                                device->endpoints_count = 0;
                                device->index_in_array = free_index; 
                                memset(device->friendly_name,0,sizeof(device->friendly_name));
                                sprintf(device->friendly_name, "Устройство [ %d ]", free_index + 1);
                                ESP_LOGI(TAG, "ZB_PAIRING_DEVICE_ASSOCIATED_EVENT Создано пустое устройство под индексом %d (mac: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)", free_index, device->ieee_addr[7], 
                                    device->ieee_addr[6], device->ieee_addr[5], device->ieee_addr[4],
                                    device->ieee_addr[3], device->ieee_addr[2], device->ieee_addr[1], device->ieee_addr[0]);
                            }else {
                                ESP_LOGE(TAG, "ZB_PAIRING_DEVICE_ASSOCIATED_EVENT Failed to create device");
                            }
                        }else {
                                ESP_LOGE(TAG, "ZB_PAIRING_DEVICE_ASSOCIATED_EVENT Failed to find free index");
                        }
                    }
                    DEVICE_ARRAY_UNLOCK();
                    } else {
                                ESP_LOGE(TAG, "CRITICAL: Failed to take device mutex in ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED! Device addition may fail.");
                                break;
                            }

            break;
            }
            
            case ZB_PAIRING_DEVICE_UPDATE_EVENT:{
                ESP_LOGI(TAG, "✅ ZB_PAIRING_DEVICE_UPDATE_EVENT: processing pairing");
                zb_pairing_device_update_t* data = (zb_pairing_device_update_t*)msg.event_data;
                if (data == NULL) {
                    ESP_LOGW(TAG, "ZB_PAIRING_DEVICE_UPDATE_EVENT: data is NULL");
                    break;
                }
                ESP_LOGI(TAG, "ZB_PAIRING_DEVICE_UPDATE_EVENT: 0x%04x re-joined the network", data->params.short_addr);
                // если устройство в статусе добавления is_in_build_status == 1, то продолжаем процесс добавления
                // если is_in_build_status == 2, то обновляем онлайн статус

                bool device_found_by_short = false;
                bool device_found_by_ieee = false;
                device_custom_t* device = NULL;
                if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                    for (int i = 0; i < RemoteDevicesCount; i++)
                    {
                        device = (device_custom_t*)RemoteDevicesArray[i];
                        if (device != NULL)
                        {
                            if (device->short_addr == data->params.short_addr)
                            {
                                device_found_by_short = true;
                                device->is_online = true;
                                ESP_LOGW(TAG, "ZB_PAIRING_DEVICE_UPDATE_EVENT  device_found_by_short = true device_short_in_data 0x%04x, device_short 0x%04x", data->params.short_addr, device->short_addr);
                                break; // выходим из for (int i = 0; i < RemoteDevicesCount; i++)
                            }
                        }
                    }
                    // если устройство не найдено по короткому адресу, ищем по ieee
                    if (device_found_by_short == false)
                    {
                        for (int i = 0; i < RemoteDevicesCount; i++)
                        {
                            device = (device_custom_t*)RemoteDevicesArray[i];
                            if (device != NULL)
                            {
                                if (ieee_addr_compare(&device->ieee_addr, &data->params.long_addr) == 0)
                                {
                                    device_found_by_ieee = true;
                                    device->short_addr = data->params.short_addr;
                                    break; // выходим из for (int i = 0; i < RemoteDevicesCount; i++) 
                                }
                            }
                        }
                    }

                    DEVICE_ARRAY_UNLOCK();   
                } else
                    {
                        ESP_LOGE(TAG, "CRITICAL: Failed to take device mutex in ZB_PAIRING_DEVICE_UPDATE_EVENT! Device addition may fail.");
                        break;
                    }
                // обновляем активность, значит устройство просто вернулось в сеть
                if ((device_found_by_short == true)&&(device != NULL))
                {
                    zb_manager_update_device_activity(device->short_addr, 10);
                    break; // выходим из switch (msg.event_id)
                }
                // если по короткому не найдено, но найдено по ieee, значит оно создалось или начало обновляться в ZB_PAIRING_DEVICE_ASSOCIATED_EVENT
                // требуется полный опрос
                if ((device_found_by_ieee == true)&&(device != NULL))
                {
                    //проверяем ещё раз build_status
                    if (device->is_in_build_status == 1)
                    {
                        // готовим Шедулер для добавления устройства
                        device_appending_sheduler_t* appending_ctx = calloc(1, sizeof(device_appending_sheduler_t));
                        if (appending_ctx != NULL)
                        {
                            appending_ctx->index_in_sheduler_array = 0xff;
                            appending_ctx->appending_device = device;
                            // устанавливаем индекс задачи
                            for(int i = 0; i < DeviceAppendShedulerCount; i++)
                            {
                                if (DeviceAppendShedulerArray[i] == NULL) 
                                {
                                    DeviceAppendShedulerArray[i] = appending_ctx;
                                    appending_ctx->index_in_sheduler_array = i;
                                    break;
                                }
                            }
                            if(appending_ctx->index_in_sheduler_array == 0xff) 
                            {
                                ESP_LOGW(TAG, "ZB_PAIRING_DEVICE_UPDATE_EVENT DeviceAppendShedulerArray is full!!!");
                                free(appending_ctx);
                                appending_ctx = NULL;
                                break;
                            }
                            appending_ctx->tuya_magic_try_count = 1;
                            StartUpdateDevices(appending_ctx);
                            break;
                        }
                    }
                }
            
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

            if (total_len < sizeof(esp_zb_zcl_cmd_info_t) + 1) {
                ESP_LOGE(TAG, "Invalid data size in ZB_PAIRING_ATTR_READ_RESP: %zu", total_len);
                //free(input_copy);
                break;
            }

            ESP_LOGW(TAG, "zb_manager_read_attr_response_fn from EVENT ATTR_READ_RESP from short 0x%04x and tsn %d",
                    ((esp_zb_zcl_cmd_info_t*)input_copy)->src_address.u.short_addr,
                    ((esp_zb_zcl_cmd_info_t*)input_copy)->header.tsn);

            // === 🔧 Парсим как в zb_manager_read_attr_resp_fn_old ===
            zb_manager_cmd_read_attr_resp_message_t* read_resp = NULL;
            read_resp = calloc(1, sizeof(zb_manager_cmd_read_attr_resp_message_t));
            if (!read_resp) {
                ESP_LOGE(TAG, "Failed to allocate read_resp in pairing");
                //free(input_copy);
                break;
            }

            esp_zb_zcl_cmd_info_t* cmd_info = (esp_zb_zcl_cmd_info_t*)input_copy;
            memcpy(&read_resp->info, cmd_info, sizeof(esp_zb_zcl_cmd_info_t));

            uint8_t attr_count = *(uint8_t*)(input_copy + sizeof(esp_zb_zcl_cmd_info_t));
            read_resp->attr_count = attr_count;
            ESP_LOGW(TAG, "Attribute count: %d", attr_count);

            read_resp->attr_arr = calloc(attr_count, sizeof(zb_manager_attr_t));
            if (!read_resp->attr_arr) {
                ESP_LOGE(TAG, "Failed to allocate attr_arr in pairing");
                free(read_resp);
                //free(input_copy);
                break;
            }

            uint8_t* pointer = (uint8_t*)(input_copy + sizeof(esp_zb_zcl_cmd_info_t) + sizeof(uint8_t));
            bool alloc_failed = false;

            for (uint8_t i = 0; i < attr_count; i++) {
                zb_manager_attr_t* attr = &read_resp->attr_arr[i];

                attr->attr_id  = *((uint16_t*)pointer);
                pointer += sizeof(uint16_t);
                attr->attr_type = *((esp_zb_zcl_attr_type_t*)pointer);
                pointer += sizeof(esp_zb_zcl_attr_type_t);
                attr->attr_len = *(uint8_t*)pointer;
                pointer += sizeof(uint8_t);

                if (attr->attr_len > 0) {
                    attr->attr_value = calloc(1, attr->attr_len);
                    if (!attr->attr_value) {
                        ESP_LOGE(TAG, "Failed to allocate attr_value for attr_id=0x%04x", attr->attr_id);
                        alloc_failed = true;
                        break;
                    }
                    memcpy(attr->attr_value, pointer, attr->attr_len);
                    pointer += attr->attr_len;
                } else {
                    attr->attr_value = NULL;
                }
            }

            if (alloc_failed) {
                zb_manager_free_read_attr_resp_attr_array(read_resp);
                free(read_resp);
                //free(input_copy);
                break;
            }

            uint16_t short_addr = read_resp->info.src_address.u.short_addr;
            device_custom_t *dev_info = zb_manager_find_device_by_short(short_addr);
            if (!dev_info) {
                ESP_LOGW(TAG, "ATTR_READ_RESP: device 0x%04x not found", short_addr);
                zb_manager_free_read_attr_resp_attr_array(read_resp);
                free(read_resp);
                //free(input_copy);
                break;
            }

            //zb_manager_update_device_activity(short_addr, 10);

            // Логируем атрибуты
            if (read_resp->attr_count > 0) {
                for (int i = 0; i < read_resp->attr_count; i++) {
                    zb_manager_attr_t attr = read_resp->attr_arr[i];
                    ESP_LOGW(TAG, "zb_manager_read_attr_response_fn attr_id 0x%04x attr_type 0x%02x attr_len %d", 
                            attr.attr_id, attr.attr_type, attr.attr_len);
                }
            }

            if (dev_info->is_in_build_status != 1) {
                ESP_LOGW(TAG, "Device 0x%04x: is_in_build_status != 1", short_addr);
                zb_manager_free_read_attr_resp_attr_array(read_resp);
                free(read_resp);
                //free(input_copy);
                break;
            }

            // Поиск sheduler
            device_appending_sheduler_t *sheduler = NULL;
            for (int i = 0; i < DeviceAppendShedulerCount; i++) {
                if (DeviceAppendShedulerArray[i] && DeviceAppendShedulerArray[i]->appending_device == dev_info) {
                    sheduler = DeviceAppendShedulerArray[i];
                    break;
                }
            }

            if (!sheduler) {
                ESP_LOGE(TAG, "Sheduler not found");
                zb_manager_free_read_attr_resp_attr_array(read_resp);
                free(read_resp);
                //free(input_copy);
                break;
            }

            if (sheduler && sheduler->tuya_magic_status == 1) {
                ESP_LOGW(TAG, "TUYA_MAGIC response with TSN %d", read_resp->info.header.tsn);
                ESP_LOGW(TAG, "Device found in RemoteDevList with status is_in_build_status!!! (short: 0x%04x)", dev_info->short_addr);
                ESP_LOGW(TAG, "Продолжаем добавление устройства — переходим к active_ep_req");

                //ESP_ERROR_CHECK(esp_timer_restart(sheduler->appending_controll_timer, 10000 * 1000));

                if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                    if (dev_info->server_BasicClusterObj == NULL) {
                        dev_info->server_BasicClusterObj = calloc(1, sizeof(zigbee_manager_basic_cluster_t));
                        if (dev_info->server_BasicClusterObj) {
                            zigbee_manager_basic_cluster_t cl = ZIGBEE_BASIC_CLUSTER_DEFAULT_INIT();
                            memcpy(dev_info->server_BasicClusterObj, &cl, sizeof(zigbee_manager_basic_cluster_t));
                            ESP_LOGI(TAG, "Basic Cluster added to device");
                        }
                    }

                    for (int i = 0; i < read_resp->attr_count; i++) {
                        zb_manager_basic_cluster_update_attribute(dev_info->server_BasicClusterObj,
                                                                read_resp->attr_arr[i].attr_id,
                                                                read_resp->attr_arr[i].attr_value);
                        ESP_LOGI(TAG, "Basic Cluster attribute 0x%04x updated", read_resp->attr_arr[i].attr_id);
                    }

                    const char *text = get_power_source_string(dev_info->server_BasicClusterObj->power_source);
                    ESP_LOGI(TAG, "Power Source: %u (%s)", dev_info->server_BasicClusterObj->power_source, text);                  
                }
                DEVICE_ARRAY_UNLOCK();
                sheduler->tuya_magic_status = 2;
                    sheduler->active_ep_req_status = 1;
                    sheduler->active_ep_req.addr_of_interest = dev_info->short_addr;
                    //ESP_ERROR_CHECK(esp_timer_restart(sheduler->appending_controll_timer, 10000 * 1000));
                    if(zb_manager_zdo_active_ep_req(&sheduler->active_ep_req, NULL, sheduler)==ESP_OK){
                        ESP_LOGI(TAG, "ZDO_ACTIVE_EP_REQ sent");
                    }
            }else {
                ESP_LOGW(TAG, "MUTEX UNHAVAILIBLE line 293");
            }

            // Освобождаем
            zb_manager_free_read_attr_resp_attr_array(read_resp);
            free(read_resp);
            //free(input_copy);  // ← парный free

            break;
        }

            case ZB_PAIRING_ACTIVE_EP_RESP: {
                ESP_LOGI(TAG, "✅ ZB_PAIRING_ACTIVE_EP_RESP: processing pairing");
                ep_resp = (zb_manager_active_ep_resp_message_t *)msg.event_data;

                if (ep_resp->status == ESP_ZB_ZDP_STATUS_SUCCESS && ep_resp->user_ctx) {
                    sheduler = (device_appending_sheduler_t *)ep_resp->user_ctx;
                    ESP_LOGI(TAG, "ACTIVE_EP_RESP: sheduler=%p, appending_device=%p", sheduler, sheduler->appending_device);
                    if (sheduler->appending_device->is_in_build_status == 1) {
                        sheduler->active_ep_req_status = 2;
                        //ESP_ERROR_CHECK(esp_timer_restart(sheduler->appending_controll_timer, 10000 * 1000));

                        // Запрос node descriptor отправляем сразу
                        uint16_t node_desc_par = sheduler->appending_device->short_addr;
                        ESP_LOGI(TAG, "📤 Queuing NODE_DESC event with ID=%d for device_short 0x%04x", ZB_ACTION_DELAYED_NODE_DESC_REQ, sheduler->appending_device->short_addr);
                        //eventLoopPost(ZB_HANDLER_EVENTS, DELAYED_NODE_DESC_REQ_LOCAL, short_addr_for_node_req, sizeof(uint16_t), portMAX_DELAY);
                        //eventLoopPost(ZB_HANDLER_EVENTS, DELAYED_NODE_DESC_REQ_LOCAL, &sheduler->appending_device->short_addr, sizeof(uint16_t), portMAX_DELAY);

                        if (zb_manager_post_to_pairing_worker(ZB_PAIRING_DELAYED_NODE_DESC_REQ, &node_desc_par, sizeof(uint16_t)) == true)
                        {
                            ESP_LOGI(TAG, "✅ ZB_ACTION_DELAYED_NODE_DESC_REQ Posted to pairing worker");
                            //ESP_ERROR_CHECK(esp_timer_restart(sheduler->appending_controll_timer, 10000 * 1000));
                        } else {
                            ESP_LOGE(TAG, "❌ Failed to post ZB_ACTION_DELAYED_NODE_DESC_REQ to pairing worker");
                        }

                        // готовим simple_desc_req
                        
                        if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                        
                        
                        sheduler->simple_desc_req_count = ep_resp->ep_count;
                        sheduler->simple_desc_req_list = calloc(sheduler->simple_desc_req_count, sizeof(local_esp_zb_zdo_simple_desc_req_param_t));
                        sheduler->simple_desc_req_simple_desc_req_list_status = calloc(sheduler->simple_desc_req_count, sizeof(uint8_t));
                         DEVICE_ARRAY_UNLOCK();
                            } else {
                                //zb_manager_free_active_ep_resp_ep_array(ep_resp);
                                ESP_LOGE(TAG, "CRITICAL: Failed to take device mutex in ACTIVE_EP_RESP! .");
                                break;
                            }
                        
                        // так как if (sheduler->appending_device->is_in_build_status == 1) то проверяем эндпоинты, вдруг они не удалились
                        if (sheduler->appending_device->endpoints_array != NULL)
                        {
                                //zb_manager_free_active_ep_resp_ep_array(ep_resp);
                                ESP_LOGE(TAG, "CRITICAL: sheduler->appending_device->endpoints_array != NULL in ACTIVE_EP_RESP! УДАЛЯЕМ .");
                                
                        }
                        // создаём пустые эндпоинты сразу
                        if (sheduler->appending_device->endpoints_array == NULL)
                        {
                            if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                            sheduler->appending_device->endpoints_count = ep_resp->ep_count; // ep добавляем все!!!
                            sheduler->appending_device->endpoints_array = calloc(sheduler->appending_device->endpoints_count, sizeof(endpoint_custom_t*));
                            
                            DEVICE_ARRAY_UNLOCK();
                            } else {
                                //zb_manager_free_active_ep_resp_ep_array(ep_resp);
                                ESP_LOGE(TAG, "CRITICAL: Failed to take device mutex in ACTIVE_EP_RESP! .");
                                break;
                            }
                        }

                        if (sheduler->appending_device->endpoints_array == NULL)
                        {
                            ESP_LOGE(TAG, "Error allocating memory for endpoints_array");
                            //zb_manager_free_active_ep_resp_ep_array(ep_resp);
                            break;
                        }
                        // берём мьютекс, так как RemoteDeviceEndpointCreate создаёт эндпоинт calloc
                        if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                        for (int i = 0; i < ep_resp->ep_count; i++) {
                            sheduler->appending_device->endpoints_array[i] = RemoteDeviceEndpointCreate(ep_resp->ep_list[i]);
                            
                            // заполняем шедулер кроме epid = 242 (0xf2), для неё сразу статус выстявляем 2
                            if (ep_resp->ep_list[i] != 0xf2) {
                                
                            
                                sheduler->simple_desc_req_list[i].addr_of_interest = sheduler->appending_device->short_addr;
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
                            } // if (ep_resp->ep_list[i] != 0xf2) {}
                            else {
                                // сразу ставим статус выполнено, вдруг 242 будет не в конце Active_ep респонза
                                sheduler->simple_desc_req_simple_desc_req_list_status[i] = 2;
                            }
                            //zb_manager_zdo_simple_desc_req(&sheduler->simple_desc_req_list[i], NULL, sheduler);
                        }
                        DEVICE_ARRAY_UNLOCK();
                            } else {
                                //zb_manager_free_active_ep_resp_ep_array(ep_resp);
                                ESP_LOGE(TAG, "CRITICAL: Failed to take device mutex in ACTIVE_EP_RESP! .");
                                break;
                            }
                    }
                }
                //zb_manager_free_active_ep_resp_ep_array(ep_resp);
                break;
            }
            case ZB_PAIRING_DELAYED_SIMPLE_DESC_REQ:{
                ESP_LOGI(TAG, "✅ ZB_PAIRING_DELAYED_SIMPLE_DESC_REQ: processing pairing");
                delayed_simple_desc_req = NULL;
                delayed_simple_desc_req = (delayed_simple_desc_req_t *)msg.event_data;
                sheduler = (device_appending_sheduler_t *)delayed_simple_desc_req->ctx;
                device_appending_sheduler_t * sheduler_p_copy = sheduler;
                ESP_LOGW(TAG, "❌ ZB_PAIRING_DELAYED_SIMPLE_DESC_REQ: sheduler %p", sheduler);
                ESP_LOGW(TAG, "❌ ZB_PAIRING_DELAYED_SIMPLE_DESC_REQ: sheduler_p_copy %p", sheduler_p_copy);
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
                device_custom_t *dev = NULL;      
                dev = zb_manager_find_device_by_short(short_addr);
                if (!dev) {
                    ESP_LOGW(TAG, "DELAYED_NODE_DESC_REQ_LOCAL: device 0x%04x not found", short_addr);
                    break;
                }
                
                if (dev != NULL) {
                    device_appending_sheduler_t *sheduler = NULL;
                    for (int i = 0; i < DeviceAppendShedulerCount; i++) {
                        if (DeviceAppendShedulerArray[i] && DeviceAppendShedulerArray[i]->appending_device == dev_info) {
                            sheduler = DeviceAppendShedulerArray[i];
                            break;
                        }
                    }
                    //отправляем
                    if (zb_manager_zdo_node_desc_req(short_addr) == ESP_OK)
                    {
                        ESP_LOGI(TAG, "Node desc req sent to device 0x%04x", short_addr);
                        //if (sheduler != NULL) ESP_ERROR_CHECK(esp_timer_restart(sheduler->appending_controll_timer, 10000 * 1000));
                    }else {
                        ESP_LOGW(TAG, "Node desc req failed to device 0x%04x", short_addr);
                    }
                }
                

                break;
            }
            case ZB_PAIRING_NODE_DESC_RESP:{
               ESP_LOGI(TAG, "✅ ZB_PAIRING_DELAYED_NODE_DESC_REQ: processing pairing");
               node_desc_resp = (zb_manager_node_desc_resp_message_t *)msg.event_data;
               dev_info = NULL;
               dev_info = zb_manager_find_device_by_short(node_desc_resp->short_addr);
                if (!dev_info) {
                    ESP_LOGW(TAG, "ZB_PAIRING_NODE_DESC_RESP: Device not found for short_addr 0x%04x", node_desc_resp->short_addr);
                    break;
                }
                if (dev_info != NULL) {
                    device_appending_sheduler_t *sheduler = NULL;
                    for (int i = 0; i < DeviceAppendShedulerCount; i++) {
                        if (DeviceAppendShedulerArray[i] && DeviceAppendShedulerArray[i]->appending_device == dev_info) {
                            sheduler = DeviceAppendShedulerArray[i];
                            break;
                        }
                    }
                    //if (sheduler != NULL) ESP_ERROR_CHECK(esp_timer_restart(sheduler->appending_controll_timer, 10000 * 1000));
                }
                
                uint16_t manuf_code = 0;
                if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                if (node_desc_resp->status == ESP_ZB_ZDP_STATUS_SUCCESS) {
                    // Формируем структуру node_desc для передачи в резолвер
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
                    // Используем полный резолвер
                    
                    manuf_code = zb_manager_resolve_manufacturer_code(
                        dev_info->ieee_addr,
                        &node_desc,
                        dev_info->server_BasicClusterObj ? dev_info->server_BasicClusterObj->manufacturer_name : NULL
                    );

                    ESP_LOGI(TAG, "Resolved manufacturer_code via node_desc: 0x%04x", manuf_code);
                } // end status success
                else {
                    ESP_LOGW(TAG, "ZB_PAIRING_NODE_DESC_RESP failed for 0x%04x, status=0x%02x", node_desc_resp->short_addr, node_desc_resp->status);
                    // Попробуем по Basic Cluster и OUI
                    manuf_code = zb_manager_resolve_manufacturer_code(
                        dev_info->ieee_addr,
                        NULL, // node_desc недоступен
                        dev_info->server_BasicClusterObj ? dev_info->server_BasicClusterObj->manufacturer_name : NULL
                    );

                    if (manuf_code == 0) {
                        ESP_LOGW(TAG, "Failed to resolve manufacturer_code for 0x%04x", node_desc_resp->short_addr);
                    }
                }
                // Сохраняем результат
                if (manuf_code != 0) {
                    dev_info->manufacturer_code = manuf_code;
                }
                DEVICE_ARRAY_UNLOCK();
                } // end mutex
                else
                    {
                        ESP_LOGE(TAG, "CRITICAL: Failed to take device mutex in ZB_PAIRING_NODE_DESC_RESP! Device addition may fail.");
                        break;
                    }
                break;
            }
            case ZB_PAIRING_SIMPLE_DESC_RESP:{
                ESP_LOGI(TAG, "✅ SIMPLE_DESC_RESP: processing pairing");
                //simple_desc_resp = NULL;
                //simple_desc_resp = (zb_manager_simple_desc_resp_message_t *)msg.event_data;
                zb_manager_simple_desc_resp_message_t * local_simple_desc_resp = (zb_manager_simple_desc_resp_message_t *)msg.event_data;
                //if (simple_desc_resp->status == ESP_ZB_ZDP_STATUS_SUCCESS && simple_desc_resp->simple_desc && simple_desc_resp->user_ctx) {
                if (local_simple_desc_resp->status == ESP_ZB_ZDP_STATUS_SUCCESS){
                    
                    //ESP_LOGW(TAG, "❌ ZB_PAIRING_DELAYED_SIMPLE_DESC_RESP: sheduler %p", sheduler);
                    local_esp_zb_af_simple_desc_1_1_t *desc = (local_esp_zb_af_simple_desc_1_1_t *)local_simple_desc_resp->simple_desc;
                    // 🔐 Проверка: user_ctx — это валидный указатель?
                    void *raw_ctx = local_simple_desc_resp->user_ctx;
                    ESP_LOGI(TAG, "SIMPLE_DESC_RESP: raw_ctx=%p", raw_ctx);
                    if (!raw_ctx) {
                        ESP_LOGE(TAG, "❌ user_ctx is NULL");
                        break;
                    }

                ESP_LOGW(TAG, "❌ SIMPLE_DESC_RESP: raw_ctx %p", raw_ctx);
                //ESP_LOGW(TAG, "❌ ZB_PAIRING_DELAYED_SIMPLE_DESC_REQ: sheduler_p_copy %p", sheduler_p_copy);
                    /*uint32_t addr = (uint32_t)raw_ctx;
                    if (addr < 0x20000000 || addr > 0x40000000) {
                        ESP_LOGE(TAG, "❌ Invalid user_ctx: %p — not a valid RAM pointer!", addr);
                        
                        break;
                    }*/

                    //device_appending_sheduler_t *sheduler = (device_appending_sheduler_t *)raw_ctx;
                    sheduler = (device_appending_sheduler_t *)raw_ctx;
                    ESP_LOGI(TAG, "SIMPLE_DESC_RESP: sheduler=%p, appending_device=%p", sheduler, sheduler ? sheduler->appending_device : NULL);
                    ESP_LOGW(TAG, "❌ SIMPLE_DESC_RESP: sheduler %p", sheduler);

                    if (!sheduler->appending_device) {
                        ESP_LOGE(TAG, "❌ SIMPLE_DESC_RESP: sheduler is NULL");
                        break;
                    }

                    //ESP_LOGW(TAG, "✅ sheduler validated: %p, device=0x%04x, ep=%d", sheduler, sheduler->appending_device->short_addr, desc->endpoint);

                    ESP_LOGW(TAG, "SIMPLE_DESC_RESP: sheduler->simple_desc_req_count=%d", sheduler->simple_desc_req_count);
                    ESP_LOGW(TAG, "SIMPLE_DESC_RESP: simple_desc_resp->simple_desc->endpoint %d", desc->endpoint);
                    // Найти и отметить выполнение
                    //ESP_ERROR_CHECK(esp_timer_restart(sheduler->appending_controll_timer, 10000 * 1000));
                    for (int i = 0; i < sheduler->simple_desc_req_count; i++) {
                        // надо проверить sheduler->simple_desc_req_list, похоже они не создались((()))
                        ESP_LOGW(TAG, "SIMPLE_DESC_RESP: endpoint %d", sheduler->simple_desc_req_list[i].endpoint);
                        //ESP_LOGW(TAG, "SIMPLE_DESC_RESP: simple_desc_resp->simple_desc->endpoint %d", desc->endpoint);
                        if (sheduler->simple_desc_req_list[i].endpoint == desc->endpoint) {
                            sheduler->simple_desc_req_simple_desc_req_list_status[i] = 2;
                            ESP_LOGW(TAG, "SIMPLE_DESC_RESP: endpoint %d marked as processed and build status set 2", desc->endpoint);
                            break;
                        }
                    }

                    

                    if (sheduler->appending_device->is_in_build_status != 1)
                    {
                        ESP_LOGW (TAG, "Device 0x%04x: is_in_build_status != 1", sheduler->appending_device->short_addr);
                        //zb_manager_free_simple_desc_resp(simple_desc_resp);
                        break;
                    }
                  
                    // Добавить endpoint (эндпоинты надо создавать в active_ep_resp, чтобы избежать realloc)
                    endpoint_custom_t* temp_endpoint = NULL;
                    temp_endpoint = zb_manager_find_endpoint(sheduler->appending_device->short_addr, desc->endpoint);
                    ESP_LOGI(TAG, "🔍 zb_manager_find_endpoint(%04x, %02x) -> %p", sheduler->appending_device->short_addr, desc->endpoint, temp_endpoint);
                    if (temp_endpoint == NULL) {
                        ESP_LOGW (TAG, "Device 0x%04x: endpoint not found", sheduler->appending_device->short_addr);
                        //zb_manager_free_simple_desc_resp(simple_desc_resp);
                        break;
                    }
                    
                    // === УСТАНОВКА ТАЙМАУТА ПРИ ПЕРВОМ ЭНДПОИНТЕ ===
                    if (temp_endpoint == sheduler->appending_device->endpoints_array[0]) {
                        uint32_t timeout_ms = get_timeout_for_device_id(desc->app_device_id);
                        sheduler->appending_device->device_timeout_ms = timeout_ms;
                        ESP_LOGI(TAG, "Device 0x%04x: device_type=0x%04x timeout=%" PRIu32 " ms",sheduler->appending_device->short_addr, desc->app_device_id, timeout_ms);}

                    // добавляем кластеры
                    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                        

                        temp_endpoint->is_use_on_device = 1;
                        memset(temp_endpoint->friendly_name,0,sizeof(temp_endpoint->friendly_name));
                        sprintf(temp_endpoint->friendly_name, "[0x%4x] [0x%02x]",sheduler->appending_device->short_addr, desc->endpoint);
                        temp_endpoint->deviceId = desc->app_device_id;

                        uint16_t *clusters = desc->app_cluster_list;
                        for (int i = 0; i < desc->app_input_cluster_count; i++) 
                                {
                                    ESP_LOGI(TAG, "  Input Cluster: 0x%04x", clusters[i]);
                                    if (clusters[i] == 0x0003) 
                                    {
                                    if(temp_endpoint->server_IdentifyClusterObj  == NULL)
                                        {
                                            temp_endpoint->is_use_identify_cluster = 1;
                                            temp_endpoint->server_IdentifyClusterObj = calloc(1,sizeof(zb_manager_identify_cluster_t));
                                            zb_manager_identify_cluster_t cl = ZIGBEE_IDENTIFY_CLUSTER_DEFAULT_INIT();
                                            memcpy(temp_endpoint->server_IdentifyClusterObj, &cl, sizeof(zb_manager_identify_cluster_t));
                                            ESP_LOGI(TAG, "  Identify Cluster added");
                                        }
                                    } // end if 0x0003
                                    else
                                    if (clusters[i] == 0x0402) 
                                    {
                                    if (temp_endpoint->server_TemperatureMeasurementClusterObj == NULL)
                                    {
                                        temp_endpoint->is_use_temperature_measurement_cluster = 1;
                                        temp_endpoint->server_TemperatureMeasurementClusterObj = calloc(1,sizeof(zb_manager_temperature_measurement_cluster_t));
                                        zb_manager_temperature_measurement_cluster_t cl = ZIGBEE_TEMP_MEASUREMENT_CLUSTER_DEFAULT_INIT();
                                        memcpy(temp_endpoint->server_TemperatureMeasurementClusterObj, &cl, sizeof(zb_manager_temperature_measurement_cluster_t));
                                        ESP_LOGI(TAG, "  Temperature Measurement Cluster added 0x0402");
                                    }
                                    } // end if 0x0402
                                    else // if (clusters[i] == 0x0402) 
                                    if (clusters[i] == 0x0405)
                                    {
                                    if (temp_endpoint->server_HumidityMeasurementClusterObj == NULL)
                                    {
                                        temp_endpoint->is_use_humidity_measurement_cluster = true;
                                        temp_endpoint->server_HumidityMeasurementClusterObj = calloc(1, sizeof(zb_manager_humidity_measurement_cluster_t));
                                        zb_manager_humidity_measurement_cluster_t cl = ZIGBEE_HUMIDITY_MEASUREMENT_CLUSTER_DEFAULT_INIT();
                                        memcpy(temp_endpoint->server_HumidityMeasurementClusterObj, &cl, sizeof(zb_manager_humidity_measurement_cluster_t));
                                        ESP_LOGI(TAG, "  Humidity Measurement Cluster added 0x0405");
                                    }
                                    } // end if 0x0405
                                    else
                                    if (clusters[i] == 0x0006)
                                    {
                                    if (temp_endpoint->server_OnOffClusterObj == NULL)
                                    {
                                        temp_endpoint->is_use_on_off_cluster = true;
                                        temp_endpoint->server_OnOffClusterObj = calloc(1, sizeof(zb_manager_on_off_cluster_t));
                                        zb_manager_on_off_cluster_t cl = ZIGBEE_ON_OFF_CLUSTER_DEFAULT_INIT();
                                        memcpy(temp_endpoint->server_OnOffClusterObj, &cl, sizeof(zb_manager_on_off_cluster_t));
                                        ESP_LOGI(TAG, "  On/Off Cluster added 0x0006");
                                        temp_endpoint->on_off_apply_cb = NULL;
                                        temp_endpoint->on_off_user_data = NULL;
                                        // Установика callback для примера, не используется
                                        //temp_endpoint->on_off_apply_cb = [](bool on, void* ctx) {
                                        //gpio_set_level((int)ctx, on ? 1 : 0);
                                        //};
                                        //temp_endpoint->on_off_user_data = (void*)GPIO_NUM_12;
                                    }
                                    }// end if 0x0006
                                    else
                                    if (clusters[i] == 0x0001)
                                    {
                                    if (sheduler->appending_device->server_PowerConfigurationClusterObj == NULL) 
                                    {
                                        sheduler->appending_device->server_PowerConfigurationClusterObj = calloc(1, sizeof(zb_manager_power_config_cluster_t));
                                        zb_manager_power_config_cluster_t cl = ZIGBEE_POWER_CONFIG_CLUSTER_DEFAULT_INIT();
                                        memcpy(sheduler->appending_device->server_PowerConfigurationClusterObj, &cl, sizeof(zb_manager_power_config_cluster_t));
                                        ESP_LOGI(TAG, "Power Configuration Cluster created at device level");
                                    }
                                    }// end if 0x0001


                                } // end for input clusters

                        // output clusters
                        if (desc->app_output_cluster_count > 0) 
                        {
                            temp_endpoint->output_clusters_count = desc->app_output_cluster_count;
                            temp_endpoint->output_clusters_array = calloc(desc->app_output_cluster_count, sizeof(uint16_t));
                        } else ESP_LOGI(TAG, "No output clusters for endpoint 0x%02x", desc->endpoint);
                        for (int i = 0; i < desc->app_output_cluster_count; i++) 
                        {
                            ESP_LOGI(TAG, "  Output Cluster: 0x%04x", clusters[desc->app_input_cluster_count + i]);
                            temp_endpoint->output_clusters_array[i] = clusters[desc->app_input_cluster_count + i];
                            ESP_LOGI(TAG, "  Output Cluster output_clusters_array[i]: 0x%04x", temp_endpoint->output_clusters_array[i]);
                        }
                        ESP_LOGI(TAG, "Endpoint 0x%02x on short_addr 0x%04x clusters added", desc->endpoint, sheduler->appending_device->short_addr);
                        // находим запрос simple_desc и помечаем его выполненным
                        for (int i = 0; i < sheduler->simple_desc_req_count; i++) 
                        {
                                if (sheduler->simple_desc_req_list[i].endpoint == desc->endpoint) 
                                {
                                    sheduler->simple_desc_req_simple_desc_req_list_status[i] = 2;
                                    break;
                                }
                        }

                        DEVICE_ARRAY_UNLOCK();
                            } else
                                {
                                    ESP_LOGE(TAG, "CRITICAL: Failed to take device mutex in SIMPLE_DESC_RESP! Device addition may fail.");
                                    zb_manager_free_simple_desc_resp(local_simple_desc_resp);
                                    break;
                                }
                    // Проверить завершение
                    bool all_done = true;
                    for (int i = 0; i < sheduler->simple_desc_req_count; i++) {
                        if (sheduler->simple_desc_req_simple_desc_req_list_status[i] != 2) {
                            all_done = false;
                            break;
                        }
                    }

                    if (all_done) {
                        ESP_LOGW(TAG, "All simple desc req done for device 0x%04x", sheduler->appending_device->short_addr);
                        ESP_LOGI(TAG, "✅ Device build complete: 0x%04x", sheduler->appending_device->short_addr);
                        sheduler->appending_device->is_in_build_status = 2;
                        zb_manager_configure_device_timeout(sheduler->appending_device);
                        
                        // 🔧 Исправляем power_source
                        zb_manager_apply_device_fixups(sheduler->appending_device);
                        zb_manager_queue_save_request();

                        // ✅ Уведомляем веб-интерфейс: устройство готово!
                        ws_notify_device_update(sheduler->appending_device->short_addr);

                        // ✅ ПУБЛИКУЕМ ПОЛНОЕ ПЕРЕОПРЕДЕЛЕНИЕ В HA
                        ha_mqtt_republish_discovery_for_device(sheduler->appending_device);

                        ESP_ERROR_CHECK(esp_timer_stop(sheduler->appending_controll_timer));
                        ESP_ERROR_CHECK(esp_timer_delete(sheduler->appending_controll_timer));
                        zb_manager_delete_appending_sheduler(sheduler);
                        sheduler = NULL;
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
