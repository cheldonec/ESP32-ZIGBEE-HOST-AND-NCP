#include "zb_manager_ncp_host.h"
#include "esp_log.h"
#include "esp_check.h"
#include "string.h"
#include "zb_manager_devices.h"
#include "freertos/FreeRTOS.h"
#include "zb_manager_devices_manufactory_table.h"
#include "zb_manager_tuya_dp.h"
#include "ncp_host_bus.h"
#include "ncp_host.h"


static const char *TAG = "ZB_MANAGER_NCP_HOST";
//extern const uint8_t REMOTE_DEVICES_COUNT;
esp_zb_ieee_addr_t LocalIeeeAdr;
zigbee_ncp_module_state_e zigbee_ncp_module_state = NOT_INIT;
bool g_zigbee_restarting = false;

bool isZigbeeNetworkOpened = false;
static esp_host_zb_endpoint_t* saved_endpoint1 = NULL; // для рестарта NCP
static esp_host_zb_endpoint_t* saved_endpoint2 = NULL; // для рестарта NCP
static esp_host_zb_endpoint_t* saved_endpoint3 = NULL; // для рестарта NCP
static esp_host_zb_endpoint_t* saved_endpoint4 = NULL; // для рестарта NCP

//#define REMOTE_DEVICES_COUNT (32) /************* !!!! zb_manager_devices.h */
// создаётся через esp_err_t zb_manager_devices_init(void);
// происходит подгрузка из файла конфигурации
//uint8_t local_RemoteDevicesCount = 0;
//device_custom_t** local_RemoteDevicesArray = NULL;

// массив для добавления устройств, содержит временные данные и освобождается при завершении сопряжения
//#define DEVICE_APPEND_SHEDULER_COUNT (5)  /************* !!!! zb_manager_devices.h */
//uint8_t local_DeviceAppendShedulerCount = 0;
//device_appending_sheduler_t** local_DeviceAppendShedulerArray = NULL;

 // Буфер под служебные данные задачи
    static StaticTask_t xZB_TaskBuffer;
    // Буфер под стек задачи
    static StackType_t xZB_Stack[ZIGBEE_STACK_SIZE];

    static TaskHandle_t xZB_Handle;
    TaskHandle_t xZB_TaskHandle = NULL; // для управления xZB_Handle

static void esp_zb_task(void *pvParameters)
{
    
    ESP_LOGI(TAG, "Zigbee main Task started");

    esp_zb_stack_main_loop();
    vTaskDelete(NULL);
}

esp_err_t zb_manager_start_main_task(uint8_t core)
{
        //zb_manager_register_event_action_handler(&zb_manager_event_action_handler, NULL);

        xZB_Handle = xTaskCreateStaticPinnedToCore(esp_zb_task, "esp_zb_task", ZIGBEE_STACK_SIZE, NULL, ZIGBEE_TASK_PRIORITY, xZB_Stack, &xZB_TaskBuffer, core);
        if (xZB_Handle == NULL)
        {
            ESP_LOGE(TAG, "Failed to create Zigbee task");
            return ESP_FAIL;
        }else
        {
            xZB_TaskHandle = xZB_Handle; // сохраняем указатель для управления
            return ESP_OK;
        }
}

esp_err_t zb_manager_init_devices_base(void)
{
    // создаём список устройств
    esp_err_t err = zb_manager_devices_init();      // !!!! zb_manager_devices.h
    //local_RemoteDevicesCount = RemoteDevicesCount; //!!!! zb_manager_devices.h */
    //local_RemoteDevicesArray = RemoteDevicesArray;
    //local_DeviceAppendShedulerCount = DeviceAppendShedulerCount;
    //local_DeviceAppendShedulerArray = DeviceAppendShedulerArray;
    if (err == ESP_OK) ESP_LOGI(TAG, "Zigbee stack zb_manager_init_devices_base"); else
    {
        ESP_LOGW(TAG, "Zigbee stack zb_manager_init_devices_base FAIL");
    }
    return err;
}



esp_err_t zb_manager_ncp_host_fast_start(esp_host_zb_endpoint_t *endpoint1, esp_host_zb_endpoint_t *endpoint2, esp_host_zb_endpoint_t *endpoint3, esp_host_zb_endpoint_t *endpoint4)
{
    esp_err_t err = ESP_FAIL;
    //1.
    
    // Запуск worker'ов
    zb_manager_start_pairing_worker(1);
    zb_manager_start_action_worker(1);
    
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    //2.
    err = zb_manager_start_main_task(0);
    
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    //3.
    err = zb_manager_init(); // 2 попытки
    if (err == ESP_OK) ESP_LOGI(TAG, "Zigbee stack zb_manager_init"); else
    {
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        err = zb_manager_init();
        if (err == ESP_OK) ESP_LOGI(TAG, "Zigbee stack zb_manager_init"); else
            {
                ESP_LOGW(TAG, "Zigbee stack zb_manager_init FAIL");
            }
    }
    
    
    //4.
    if (endpoint1 != NULL) 
    {
        err = esp_host_zb_ep_create(endpoint1);
         saved_endpoint1 = endpoint1;
        //free (endpoint1);
        //endpoint1 = NULL;
        if (err == ESP_OK) ESP_LOGI(TAG, "Zigbee stack esp_host_zb_ep_create EP1"); else ESP_LOGW(TAG, "Zigbee stack esp_host_zb_ep_create EP1 FAIL");
    } else ESP_LOGW(TAG, "zb_manager_ncp_host_fast_start Zigbee stack endpoint1 == NULL");
    if (endpoint2 != NULL) 
    {
        err = esp_host_zb_ep_create(endpoint2);
         saved_endpoint2 = endpoint2;
         //free (endpoint2);
        //endpoint2 = NULL;
        if (err == ESP_OK) ESP_LOGI(TAG, "Zigbee stack esp_host_zb_ep_create EP2"); else ESP_LOGW(TAG, "Zigbee stack esp_host_zb_ep_create EP2 FAIL");
    } else ESP_LOGW(TAG, "zb_manager_ncp_host_fast_start Zigbee stack endpoint2 == NULL");

    if (endpoint3 != NULL) 
    {
        err = esp_host_zb_ep_create(endpoint3);
         saved_endpoint3 = endpoint3;
         //free (endpoint3);
          //endpoint3 = NULL;
          if (err == ESP_OK) ESP_LOGI(TAG, "Zigbee stack esp_host_zb_ep_create EP3"); else ESP_LOGW(TAG, "Zigbee stack esp_host_zb_ep_create EP3 FAIL");
    }else ESP_LOGW(TAG, "zb_manager_ncp_host_fast_start Zigbee stack endpoint3 == NULL");

    if (endpoint4 != NULL) 
    {
        err = esp_host_zb_ep_create(endpoint4);
         saved_endpoint4 = endpoint4;
        //free (endpoint4);
        //endpoint4 = NULL;
        if (err == ESP_OK) ESP_LOGI(TAG, "Zigbee stack esp_host_zb_ep_create EP4"); else ESP_LOGW(TAG, "Zigbee stack esp_host_zb_ep_create EP4 FAIL");
    }else ESP_LOGW(TAG, "zb_manager_ncp_host_fast_start Zigbee stack endpoint4 == NULL");

    //5.
    memset(LocalIeeeAdr,0,8);
    err = zb_manager_start();
    if (err == ESP_OK) 
    {
        ESP_LOGI(TAG, "Zigbee stack zb_manager_start");
        zigbee_ncp_module_state = STARTED;
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        //ESP_LOGI(TAG, "Zigbee test func zb_manager_open_network");
        esp_zb_get_long_address(LocalIeeeAdr);
        ESP_LOGI(TAG,  "LocalIeeeAdr: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", LocalIeeeAdr[0], LocalIeeeAdr[1], LocalIeeeAdr[2],
                LocalIeeeAdr[3], LocalIeeeAdr[4], LocalIeeeAdr[5], LocalIeeeAdr[6], LocalIeeeAdr[7]);

        zigbee_ncp_module_state = WORKING;
    } else
    {
        ESP_LOGW(TAG, "Zigbee stack zb_manager_start FAIL");
    }
    return err;
}



// zb_manager_ncp_host.c

esp_err_t zb_manager_ncp_host_restart_on_ncp_foulture(void)
{
    ESP_LOGW(TAG, "🔄 Full Zigbee NCP restart initiated");

    zigbee_ncp_module_state = RESTARTING;

    // === 1. Останавливаем Zigbee стек (очереди, семафор) ===
    esp_zb_stack_shutdown();  // ← отправит shutdown event в host_task

    // === 2. Ждём, пока host_task завершится ===
    while (s_host_dev.run) {
        ESP_LOGD(TAG, "Waiting for host task to stop...");
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    // === 3. Перезапуск ===
    esp_err_t err = ESP_FAIL;

    // Инициализируем шину заново
    /*err = esp_host_init(HOST_CONNECTION_MODE_UART);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to re-init host bus");
        goto fail;
    }

    err = esp_host_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to start host task");
        goto fail;
    }*/

    vTaskDelay(pdMS_TO_TICKS(500));

    // Пересоздаём платформу Zigbee
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    err = esp_zb_platform_config(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to re-config Zigbee platform");
        goto fail;
    }

    vTaskDelay(pdMS_TO_TICKS(5000));

    err = zb_manager_init(); // 2 попытки
    if (err == ESP_OK) ESP_LOGI(TAG, "Zigbee stack zb_manager_init"); else
    {
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        err = zb_manager_init();
        if (err == ESP_OK) ESP_LOGI(TAG, "Zigbee stack zb_manager_init"); else
            {
                ESP_LOGW(TAG, "Zigbee stack zb_manager_init FAIL");
            }
    }
    
    // Восстанавливаем endpoints
    if (saved_endpoint1) esp_host_zb_ep_create(saved_endpoint1);
    if (saved_endpoint2) esp_host_zb_ep_create(saved_endpoint2);
    if (saved_endpoint3) esp_host_zb_ep_create(saved_endpoint3);
    if (saved_endpoint4) esp_host_zb_ep_create(saved_endpoint4);

    // Запускаем сеть
    memset(LocalIeeeAdr, 0, 8);
    err = zb_manager_start();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✅ Zigbee stack restarted successfully");
        esp_zb_get_long_address(LocalIeeeAdr);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, LocalIeeeAdr, 8, ESP_LOG_INFO);
        zigbee_ncp_module_state = WORKING;
    } else {
        ESP_LOGE(TAG, "❌ zb_manager_start failed after restart");
        zigbee_ncp_module_state = FOULTED;
    }

    return err;

fail:
    zigbee_ncp_module_state = FOULTED;
    return err;
}



//***************************************************** Колбэк на таймер appending_controll_timer ***********************************************************/
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
        ESP_ERROR_CHECK(esp_timer_start_once(shedule->appending_controll_timer, 5000 * 1000)); // запускаем на 5 секунд
        ESP_LOGW(TAG, "shedule->appending_controll_timer stated");
        // отправляем запрос 
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

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG, "Failed to start Zigbee bdb commissioning");
}

bool zb_manager_app_signal_handler(local_esp_zb_app_signal_t *signal_struct)
{
     uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    local_esp_zb_app_signal_type_t sig_type = *p_sg_p;
   
    esp_zb_nwk_signal_device_associated_params_t*  dev_assoc_params = NULL;
    esp_zb_zdo_signal_device_annce_params_t*       dev_annce_params = NULL;
    esp_zb_zdo_signal_device_authorized_params_t*  dev_auth_params = NULL;
    esp_zb_zdo_signal_device_update_params_t*      dev_update_params = NULL;
    esp_zb_zdo_signal_leave_indication_params_t*   leave_ind_params = NULL;
    esp_zb_zdo_device_unavailable_params_t*        dev_unavalible = NULL;
    
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            //ESP_LOGI(TAG, "Deferred driver initialization %s", deferred_driver_init() ? "failed" : "successful");
            ESP_LOGI(TAG, "Start network formation");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
        } else {
            ESP_LOGW(TAG, "%s failed with status: %s, retrying", esp_zb_zdo_signal_to_string(sig_type),
                     esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_FORMATION:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Formed network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            ESP_LOGI(TAG, "Restart network formation (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
        }
        break;
    case ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED:
       
        if(err_status == ESP_OK)
            {
                ESP_LOGI(TAG, "ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED:");
                dev_assoc_params = (esp_zb_nwk_signal_device_associated_params_t*)esp_zb_app_signal_get_params(p_sg_p);
                //  создаём устройство !!! Проверено
                // 1. ищем по списку
                if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                bool device_found = false;         // если будет найдено, то меняем статус is_in_build_status, а значит после dev_annce надо будет его опросить как новое
                    for (int i = 0; i < RemoteDevicesCount; i++)
                    {
                        if (RemoteDevicesArray[i]!=NULL) 
                        {
                            if (ieee_addr_compare(&RemoteDevicesArray[i]->ieee_addr, &dev_assoc_params->device_addr) == 0)
                            {
                                device_found = true;
                                ESP_LOGW(TAG, "ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED  device found in RemoteDevList FLAG Update set enable!!!");
                                RemoteDevicesArray[i]->is_in_build_status = 1; // переопределяем статус добавления и очищаем
                                if (RemoteDevicesArray[i]->endpoints_count > 0)
                                {
                                    for (int j = 0; j < RemoteDevicesArray[i]->endpoints_count; j++)
                                    {
                                        RemoteDeviceEndpointDelete(RemoteDevicesArray[i]->endpoints_array[j]);
                                    }
                                    RemoteDevicesArray[i]->endpoints_count = 0;
                                    free (RemoteDevicesArray[i]->endpoints_array);
                                    RemoteDevicesArray[i]->endpoints_array = NULL;
                                    RemoteDevicesArray[i]->is_online = false;
                                }
                                ESP_LOGW(TAG, "ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED  device found in RemoteDevList, надо очистить связанную графику!!!");
                                break;
                            }
                        }
                    }
                    // 2 если не нашли, то надо найти свободную ячейку и создать новое
                    if (device_found == false)
                    {
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
                            esp_zb_ieee_addr_t temp_ieee;
                            memcpy(temp_ieee, dev_assoc_params->device_addr, sizeof(esp_zb_ieee_addr_t));
                            RemoteDevicesArray[free_index] = RemoteDeviceCreate(temp_ieee);
                            if (RemoteDevicesArray[free_index] != NULL) 
                            {
                                RemoteDevicesArray[free_index]->is_in_build_status = 1;
                                RemoteDevicesArray[free_index]->endpoints_count = 0;
                                RemoteDevicesArray[free_index]->index_in_array = free_index; 
                                memset(RemoteDevicesArray[free_index]->friendly_name,0,sizeof(RemoteDevicesArray[free_index]->friendly_name));
                                sprintf(RemoteDevicesArray[free_index]->friendly_name, "Устройство [ %d ]", free_index + 1);

                                ESP_LOGI(TAG, "Создано пустое устройство под индексом %d (mac: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)", free_index, dev_assoc_params->device_addr[7], 
                                dev_assoc_params->device_addr[6], dev_assoc_params->device_addr[5], dev_assoc_params->device_addr[4],
                                dev_assoc_params->device_addr[3], dev_assoc_params->device_addr[2], dev_assoc_params->device_addr[1], dev_assoc_params->device_addr[0]);
                            }
                        } else ESP_LOGW(TAG, "ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED Список устройств занят, для добавления надо расширить список");
                    }
                     DEVICE_ARRAY_UNLOCK();} else
                                                {
                                                    ESP_LOGE(TAG, "CRITICAL: Failed to take device mutex in ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED! Device addition may fail.");
                                                    
                                                    break;
                                                }
            }
        break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE:
       
        if(err_status == ESP_OK)
            {
                ESP_LOGI(TAG, "ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE:");
                dev_update_params = (esp_zb_zdo_signal_device_update_params_t *)esp_zb_app_signal_get_params(p_sg_p);
                // Обновляем короткий адрес
                      // 1. ищем по списку
                bool device_found = false;         // если будет найдено, то меняем статус is_in_build_status
                bool need_full_update = false;

                if (!RemoteDevicesArray) {
                    ESP_LOGE(TAG, "Remote devices array not initialized");
                    break;
                }
                
                // === Сначала обновляем short_addr под мьютексом ===
                if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {

                    for (int i = 0; i < RemoteDevicesCount; i++)
                    {
                        if (RemoteDevicesArray[i]!=NULL) 
                        {
                            if (ieee_addr_compare(&RemoteDevicesArray[i]->ieee_addr, &dev_update_params->long_addr) == 0)
                            {
                                device_found = true;
                                ESP_LOGW(TAG, "ESP_ZB_NWK_SIGNAL_DEVICE_UPDATE  device found and SHORT_ADDR Update!!!");
                                //zb_manager_obj.RemoteDevicesArray[i]->is_in_build_status = true; // меняем статус на переподключение
                                RemoteDevicesArray[i]->short_addr = dev_update_params->short_addr;
                                if (RemoteDevicesArray[i]->is_in_build_status == 1)
                                {
                                    need_full_update = true;
                                    // Send TuyaMagicPacket
                                    device_appending_sheduler_t* appending_ctx = calloc(1, sizeof(device_appending_sheduler_t));

                                    if (appending_ctx != NULL)
                                    {
                                        appending_ctx->index_in_sheduler_array = 0xff;
                                        appending_ctx->appending_device = RemoteDevicesArray[i];
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
                                            ESP_LOGW(TAG, "DeviceAppendShedulerArray is full!!!");
                                            free(appending_ctx);
                                            appending_ctx = NULL;
                                            break;
                                        }
                                        //Запускаем шедулер
                                        appending_ctx->tuya_magic_try_count = 1;
                                        StartUpdateDevices(appending_ctx);
                                        
                                    }
                                }
                                //memcpy(temp_ieee, zb_manager_obj.RemoteDevicesArray[i], 8);
                                break;
                            }
                        }
                    }
                    DEVICE_ARRAY_UNLOCK();
                } else
                    {
                        ESP_LOGE(TAG, "CRITICAL: Failed to take device mutex in ESP_ZB_NWK_SIGNAL_DEVICE_UPDATE! Device addition may fail.");
                        break;
                    }

                // === Теперь ОБНОВЛЯЕМ АКТИВНОСТЬ — ПОСЛЕ мьютекса (вне его) ===
                // Это безопасно: функция сама возьмёт мьютекс
                uint16_t short_addr = dev_update_params->short_addr;
                uint8_t lqi = 80; // можно улучшить позже, когда LQI придёт от NCP
                zb_manager_update_device_activity(dev_update_params->short_addr, lqi);
                ESP_LOGI(TAG, "DEVICE_UPDATE: 0x%04x re-joined the network, activity updated", dev_update_params->short_addr);
            }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Network steering started");
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
    if(err_status == ESP_OK)
       {
       dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGI(TAG, "ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:");
        ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)", dev_annce_params->device_short_addr);
                if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                    for (int i = 0; i < RemoteDevicesCount; i++)
                    {
                        if (RemoteDevicesArray[i]!=NULL) 
                        {
                            if (ieee_addr_compare(&RemoteDevicesArray[i]->ieee_addr, &dev_annce_params->ieee_addr) == 0)
                            {
                                
                                if (RemoteDevicesArray[i]->is_in_build_status == 1)
                                {
                                    // обновляем short_addr
                                    RemoteDevicesArray[i]->short_addr = dev_annce_params->device_short_addr;
                                    ESP_LOGW(TAG, "ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE  device found in RemoteDevList with status is_in_build_status!!! (short: 0x%04x)", RemoteDevicesArray[i]->short_addr); 
                                    break;
                                }
            
                            }
                        }
                    }
                   
                    DEVICE_ARRAY_UNLOCK();} else
                    {
                        ESP_LOGE(TAG, "CRITICAL: Failed to take device mutex in ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE! Device addition may fail.");
                        break;
                    }
       }
        break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED:
        if(err_status == ESP_OK)
            {    
                dev_auth_params = (esp_zb_zdo_signal_device_authorized_params_t *)esp_zb_app_signal_get_params(p_sg_p);
                ESP_LOGI(TAG, "ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED:");
                /*sprintf(buff, "DEVICE_AUTHORIZED : %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x (short: 0x%04x) (auth_type: 0x%02x) (auth_status: 0x%02x)", 
                dev_auth_params->long_addr[7], dev_auth_params->long_addr[6], dev_auth_params->long_addr[5], dev_auth_params->long_addr[4],
                     dev_auth_params->long_addr[3], dev_auth_params->long_addr[2], dev_auth_params->long_addr[1], dev_auth_params->long_addr[0], 
                     dev_auth_params->short_addr, dev_auth_params->authorization_type, dev_auth_params->authorization_status);
                //zb_manager_obj.utility_functions_callbacks.print_log_to_screen_callback(buff, 0xA7BFC1);

                ESP_LOGI(TAG, "device authorized (short: 0x%04x) (auth_type: 0x%02x) (auth_status: 0x%02x) (mac: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)", 
                dev_auth_params->short_addr, dev_auth_params->authorization_type, dev_auth_params->authorization_status, dev_auth_params->long_addr[7], dev_auth_params->long_addr[6], dev_auth_params->long_addr[5], dev_auth_params->long_addr[4],
                     dev_auth_params->long_addr[3], dev_auth_params->long_addr[2], dev_auth_params->long_addr[1], dev_auth_params->long_addr[0]);
                */
            }

        break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        if (err_status == ESP_OK) {
            if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
                isZigbeeNetworkOpened = true;
                ESP_LOGW(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
                zb_manager_post_to_action_worker(ZB_ACTION_NETWORK_IS_OPEN, &isZigbeeNetworkOpened, sizeof(isZigbeeNetworkOpened));
                
            } else {
                isZigbeeNetworkOpened = false;
                ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
                zb_manager_post_to_action_worker(ZB_ACTION_NETWORK_IS_CLOSE, &isZigbeeNetworkOpened, sizeof(isZigbeeNetworkOpened));
                
            }
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
    if (zb_manager_user_app_signal_handler != NULL) return false; else return true; // false значит надо вызвать пользовательскую функцию
}

