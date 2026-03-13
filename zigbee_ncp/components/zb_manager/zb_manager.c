#include <stdio.h>
#include "zb_manager.h"
#include "zb_manager_config.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "nvs_flash.h"
//#include "zcl/esp_zigbee_zcl_common.h"
//#include "nwk/esp_zigbee_nwk.h"
#include "esp_zigbee_core.h"
#include "zb_config.h"
#include "zb_aps_handler.h"
#include "zb_raw_action_handle.h"
#include "zb_handlers.h"
//#include "light_driver.h"
//#include "platform/esp_zigbee_platform.h"

// настройка ключей безопасности zboss
// Глобальный link key — ZBOSS использует его автоматически
__attribute__((section(".rodata"))) const zb_uint8_t g_zb_secur_preconfigured_link_key[16] = {
    0x5a, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6c,
    0x6c, 0x69, 0x61, 0x6e, 0x63, 0x65, 0x30, 0x39
};
//extern void zb_secur_setup_preconfigured_key(zb_uint8_t *key);
//extern void zb_secur_setup_preconfigured_key(zb_uint8_t *key);
//extern void zb_bdb_set_legacy_default_link_key(zb_uint8_t *key);
//extern void zb_bdb_set_legacy_device_support(zb_uint8_t state);
//extern void zb_set_installcode_policy(zb_bool_t allow_ic_only);

static const char *TAG = "zb_event_post_send_module";

zb_manager_t zb_manager_obj;

static TaskHandle_t xZBTaskHandle; // задача зигбии
static TaskHandle_t xZBEventsTaskHandle; // задача обработки сообщений и команд от внешних модулей

// параметры для очереди, которая будет крутиться внутри xZBEventsTaskHandle
static QueueHandle_t xZBEventsQueueHandle; //
static StaticQueue_t xZBEventsQueueBuffer;
static uint8_t ZB_QueueStorage[ZB_EVENTS_QUEUE_SIZE * ZB_EVENTS_QUEUE_ITEM_SIZE]; 
//static StaticQueue_t ZB_HandleDataQueueBuffer;

//******************************************** send msg to other processes *****************************************/
bool zb_event_msg_post(esp_event_base_t event_base, int32_t event_id, void* event_data, size_t event_data_size, TickType_t ticks_to_wait)
{
     esp_err_t err = ESP_OK;
     ESP_LOGI(TAG, "SENDING EVENT FROM GROUP: %s MESSAGE: %d", event_base, (int)event_id);
    do {
      esp_err_t err = esp_event_post(event_base, event_id, event_data, event_data_size, ticks_to_wait == portMAX_DELAY ? EVENT_LOOP_POST_DELAY : ticks_to_wait);
      if (err != ESP_OK) {
        //ESP_LOGE(EVENT_POST_SEND_TAG, "Failed to post event to \"%s\" #%d: %li (%s)", event_base, event_id, err, esp_err_to_name(err));
        if (ticks_to_wait == portMAX_DELAY) {
          vTaskDelay(EVENT_LOOP_POST_DELAY);
        };
      };
    } while ((ticks_to_wait == portMAX_DELAY) && (err != ESP_OK));
  return err == ESP_OK;
}




static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG, "Failed to start Zigbee bdb commissioning");
}

//void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct); // описывается далее, в зависимости от типа устройства

/*void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_leave_indication_params_t *leave_ind_params = NULL;
    esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;
    switch (sig_type) {
     case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:// добавлено для роутера
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:// добавлено для роутера
     if (err_status == ESP_OK) {
            //ESP_LOGI(TAG, "Deferred driver initialization %s", deferred_driver_init() ? "failed" : "successful");
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status: %s, retrying", esp_zb_zdo_signal_to_string(sig_type),
                     esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
   
    case ESP_ZB_BDB_SIGNAL_STEERING: 
    if (err_status != ESP_OK) {
            ESP_LOGW(TAG, "Stack %s failure with %s status, steering",esp_zb_zdo_signal_to_string(sig_type), esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        } else {
           
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            esp_zb_zdo_match_desc_req_param_t  find_req;
            find_req.addr_of_interest = 0xfffd;
            find_req.dst_nwk_addr = 0xffff;
            
            //esp_zb_zdo_find_on_off_light(&find_req, user_find_cb, NULL);
        }
        break;
   
    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)", dev_annce_params->device_short_addr);
        break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION:
        leave_ind_params = (esp_zb_zdo_signal_leave_indication_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        if (!leave_ind_params->rejoin) {
            esp_zb_ieee_addr_t leave_addr;
            memcpy(leave_addr, leave_ind_params->device_addr, sizeof(esp_zb_ieee_addr_t));
            ESP_LOGI(TAG, "Zigbee Node is leaving network: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)",
                     leave_addr[7], leave_addr[6], leave_addr[5], leave_addr[4],
                     leave_addr[3], leave_addr[2], leave_addr[1], leave_addr[0]);
        }
        break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:// добавлено для роутера
        if (err_status == ESP_OK) {
            if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
                ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
            } else {
                ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
            }
        }
        break;
    case ESP_ZB_NLME_STATUS_INDICATION:
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}*/

/*static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
        //ret = zb_attribute_reporting_handler((esp_zb_zcl_report_attr_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
        //ret = zb_read_attr_resp_handler((esp_zb_zcl_cmd_read_attr_resp_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID:
        //ret = zb_configure_report_resp_handler((esp_zb_zcl_cmd_config_report_resp_message_t *)message);
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}*/

#if(CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP)
static void esp_zb_gateway_update_rcp(void)
{
    /* Deinit uart to transfer UART to the serial loader */
    esp_zb_rcp_deinit();
    if (esp_rcp_update() != ESP_OK) {
        esp_rcp_mark_image_verified(false);
    }
    esp_restart();
}

static void esp_zb_gateway_board_try_update(const char *rcp_version_str)
{
    char version_str[RCP_VERSION_MAX_SIZE];
    if (esp_rcp_load_version_in_storage(version_str, sizeof(version_str)) == ESP_OK) {
        ESP_LOGI(TAG, "Storage RCP Version: %s", version_str);
        if (strcmp(version_str, rcp_version_str)) {
            ESP_LOGI(TAG, "*** NOT MATCH VERSION! ***");
            esp_zb_gateway_update_rcp();
        } else {
            ESP_LOGI(TAG, "*** MATCH VERSION! ***");
            esp_rcp_mark_image_verified(true);
        }
    } else {
        ESP_LOGI(TAG, "RCP firmware not found in storage, will reboot to try next image");
        esp_rcp_mark_image_verified(false);
        esp_restart();
    }
}

static esp_err_t init_spiffs(void)
{
    esp_vfs_spiffs_conf_t rcp_fw_conf = {
        .base_path = "/rcp_fw", .partition_label = "rcp_fw", .max_files = 10, .format_if_mount_failed = false
    };
    esp_vfs_spiffs_register(&rcp_fw_conf);
    return ESP_OK;
}
#endif

void rcp_error_handler(void)
{
#if(CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP)
    ESP_LOGI(TAG, "Re-flashing RCP");
    esp_zb_gateway_update_rcp();
#endif
    esp_restart();
}

#if CONFIG_OPENTHREAD_SPINEL_ONLY
static esp_err_t check_ot_rcp_version(void)
{
    char internal_rcp_version[RCP_VERSION_MAX_SIZE];
    ESP_RETURN_ON_ERROR(esp_radio_spinel_rcp_version_get(internal_rcp_version, ESP_RADIO_SPINEL_ZIGBEE), TAG, "Fail to get rcp version from radio spinel");
    ESP_LOGI(TAG, "Running RCP Version: %s", internal_rcp_version);
#if(CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP)
    esp_zb_gateway_board_try_update(internal_rcp_version);
#endif
    return ESP_OK;
}
#endif

static void ESP_ZIGBEE_Task(void *pvParameters)
{

    //zb_manager_init_t *init_ctx = (zb_manager_init_t *)pvParameters;
    zb_manager_t *init_ctx = (zb_manager_t *)pvParameters;
    
    #if CONFIG_OPENTHREAD_SPINEL_ONLY
    esp_radio_spinel_register_rcp_failure_handler(rcp_error_handler, ESP_RADIO_SPINEL_ZIGBEE);
    #endif

        #ifdef CONFIG_ZB_ZCZR
        #ifdef CONFIG_ZB_MANAGER_ZC
            esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG(); // coordinator 
            esp_zb_init(&zb_nwk_cfg);
        #elif CONFIG_ZB_MANAGER_ZR
           esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZCR_CONFIG(); // router
           esp_zb_init(&zb_nwk_cfg);
        #endif
        #elif CONFIG_ZB_ZED
            esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG(); // enddevice config
            esp_zb_init(&zb_nwk_cfg);
        #endif
   
    
    #if CONFIG_OPENTHREAD_SPINEL_ONLY
    ESP_ERROR_CHECK(check_ot_rcp_version());
    #endif
    
    //esp_zb_ep_list_t *esp_zb_ep_list = generate_local_endpoints_desc_for_sample();
    if (init_ctx->esp_local_zb_ep_list != NULL) {
        ESP_ERROR_CHECK(esp_zb_device_register(init_ctx->esp_local_zb_ep_list));
    } else ESP_LOGW(TAG, "Отсутствует список локальных эндпоинтов");
    //ESP_ERROR_CHECK(esp_zb_device_register(init_ctx->esp_local_zb_ep_list));
    

    //esp_zb_zcl_attr_t * testattr = esp_zb_zcl_get_attribute(HA_REMOTE_DEVICE_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID);
    //ESP_LOGW(TAG, "testattr = %s", (char*)testattr->data_p);

    //ESP_LOGI(TAG, "devid %x",esp_zb_ep_list->next->endpoint.simple_desc->app_device_id);
    //if (init_ctx->action_handler != NULL) {
        //esp_zb_core_action_handler_register(init_ctx->action_handler);
    //} else 
    //esp_zb_core_action_handler_register(zb_action_handler); // если без NCP
    if (esp_zb_aps_src_binding_table_size_set(64) != ESP_OK) // таблица записей когда нас забиндили
    {
        ESP_LOGW(TAG, "esp_zb_aps_src_binding_table_size_set failed");
    }
    if (esp_zb_aps_dst_binding_table_size_set(16) != ESP_OK) // bind_req->req_dst_addr = 0000 таблица когда мы биндим
    {
        ESP_LOGW(TAG, "esp_zb_aps_dst_binding_table_size_set failed");
    }
    if (esp_zb_io_buffer_size_set(160) != ESP_OK)
    {
        ESP_LOGW(TAG, "esp_zb_io_buffer_size_set failed");
    }
    if (esp_zb_scheduler_queue_size_set(160)!= ESP_OK)
    {
        ESP_LOGW(TAG, "esp_zb_scheduler_queue_size_set failed");
    }
    esp_zb_core_action_handler_register(esp_ncp_zb_action_handler); // NCP
    
    // временно отключаем регистрацию RAW команды
    /*if (init_ctx->raw_command_handler != NULL) {
        esp_zb_raw_command_handler_register(init_ctx->raw_command_handler);
    }else esp_zb_raw_command_handler_register(raw_command_handler);*/
    
        esp_zb_raw_command_handler_register(raw_command_handler);
        esp_zb_aps_data_indication_handler_register(esp_zb_aps_data_indication_handler);
        esp_zb_aps_data_confirm_handler_register(esp_zb_aps_data_confirm_handler);

    #ifdef CONFIG_ZB_ZCZR
        #ifdef CONFIG_ZB_MANAGER_ZC
            esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
            esp_zb_set_secondary_network_channel_set(ESP_ZB_SECONDARY_CHANNEL_MASK);
        #endif
    #endif
        esp_zb_zll_identify_req_handler_register(identify_handler_req);
        //esp_zb_identify_notify_handler_register(HA_REMOTE_DEVICE_ENDPOINT, identify_handler);
        
        esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
        esp_zb_set_secondary_network_channel_set(ESP_ZB_SECONDARY_CHANNEL_MASK);

       // ✅ Включаем поддержку устройств с link key
        zb_bdb_set_legacy_device_support(1);

        // ✅ Отключаем политику install code
        zb_set_installcode_policy(0);

        extern const zb_uint8_t g_zb_secur_preconfigured_link_key[16];
        ESP_LOGW(TAG, "TCLinkKey[0] = 0x%02x", g_zb_secur_preconfigured_link_key[0]);
        // ✅ УСТАНОВКА LINK KEY — КОНЕЦ

    ESP_ERROR_CHECK(esp_zb_start(false));
    ESP_LOGW(TAG, "RAM control after esp_zb_start(false) in zb_manager zb_task %lu", esp_get_free_heap_size());
        for(;;)
        {
         esp_zb_stack_main_loop_iteration();
         //vTaskDelay(20/ portTICK_PERIOD_MS);
        }
        vTaskDelete(NULL);
        //esp_zb_stack_main_loop(); 
}

/*static void ESP_ZIGBEE_EventsTask(void *pvParameters)
{
    zb_event_msg_t event_msg;
    ESP_LOGI(TAG, "Start ESP_ZIGBEE_EventsTask");
        for(;;)
        {
         if (xQueueReceive(xZBEventsQueueHandle, (void *)&event_msg, (TickType_t)ZB_EVENTS_QUEUE_READ_TICS)) {
          vTaskDelay(20/ portTICK_PERIOD_MS);

        }
        }
        vTaskDelete(NULL); 

    }*/

///////////////////////////////////////////////////////////
/*Инициализация модуля
Входной параметр init_zb_event_manager_t initdata
typedef struct init_zb_event_manager_s {
    esp_zb_platform_config_t* esp_zb_config;
    esp_zb_cfg_t* zb_nwk_cfg;
    esp_zb_ep_list_t *esp_zb_ep_list;
}
init_zb_event_manager_t;
всё должно быть создано перед вызовом
если всё хорошо, то 
1. Запускается ESP_ZIGBEE_Task - просто цикл ZIGBEE
2. Запускается очередь сообщений xZBEventsQueueHandle, которая будет крутиться внутри ESP_ZIGBEE_EventsTask
3. Запускается ESP_ZIGBEE_EventsTask
*/
/*esp_err_t zb_event_manager_init(zb_manager_init_t init_params)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };

    ret = esp_zb_platform_config(&config);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Ошибка %s во время esp_zb_platform_config(initdata.esp_zb_config)", esp_err_to_name(ret));
        return ret;
    } 
    
    
    //---------------------------------- Запуск очереди сообщений для ESP_ZIGBEE_EventsTask
    xZBEventsQueueHandle = NULL;
    xZBEventsQueueHandle = xQueueCreateStatic(ZB_EVENTS_QUEUE_SIZE, ZB_EVENTS_QUEUE_ITEM_SIZE, &ZB_QueueStorage[0], &xZBEventsQueueBuffer);
    if (xZBEventsQueueHandle == NULL) return ESP_FAIL;

    //-----------------------------------Запуск цикла обработки сообщений
    static StaticTask_t xZBEventsTaskBuffer;
    static StackType_t xZBEventsStack[ZIGBEE_EVENTS_STACK_SIZE];
    xZBEventsTaskHandle = NULL;
    xZBEventsTaskHandle = xTaskCreateStatic(ESP_ZIGBEE_EventsTask, "Zigbee_Events_Task", ZIGBEE_EVENTS_STACK_SIZE, NULL, 10, xZBEventsStack, &xZBEventsTaskBuffer);
    if (xZBEventsTaskHandle == NULL) return ESP_FAIL;
    //assert(xZBEventsTaskHandle);

    //----------------------------------- Запуск основного цикла Zigbee
    zb_manager_init_t *init_ctx = calloc(1, sizeof(zb_manager_init_t));
   
    init_ctx->esp_local_zb_ep_list = init_params.esp_local_zb_ep_list;
    init_ctx->action_handler = init_params.action_handler;

    static StaticTask_t xZBTaskBuffer; 
    static StackType_t xZBStack[ZIGBEE_STACK_SIZE];
    xZBTaskHandle = NULL;
    xZBTaskHandle = xTaskCreateStatic(ESP_ZIGBEE_Task, "Zigbee_Task", ZIGBEE_STACK_SIZE, init_ctx, 10, xZBStack, &xZBTaskBuffer);
    if (xZBTaskHandle == NULL) return ESP_FAIL;
    //assert(xZBTaskHandle);
    return ret;
}*/



esp_err_t zb_manager_init()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    #if(CONFIG_ZIGBEE_GW_AUTO_UPDATE_RCP)
    esp_rcp_update_config_t rcp_update_config = ESP_ZB_RCP_UPDATE_CONFIG();
    ESP_ERROR_CHECK(init_spiffs());
    ESP_ERROR_CHECK(esp_rcp_update_init(&rcp_update_config));
    #endif

    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };

    ret = esp_zb_platform_config(&config);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Ошибка %s во время esp_zb_platform_config(initdata.esp_zb_config)", esp_err_to_name(ret));
        return ret;
    } 

    // values
    zb_manager_devices_init(); //in zb_manager_devices
    zb_manager_obj.RemoteDevicesCount = RemoteDevicesCount; // init in zb_manager_devices
    zb_manager_obj.RemoteDevicesArray = RemoteDevicesArray; // init in zb_manager_devices
    /*if (zb_manager_devices_init(zb_manager_obj.RemoteDevicesArray, zb_manager_obj.RemoteDevicesCount)!= ESP_OK) 
    {
        ESP_LOGW(TAG, "Ошибка инициализации списка устройств");
    }*/
    
    //if(zb_manager == NULL) return ESP_ERR_INVALID_ARG;
    // action handlers
    zb_manager_obj.action_handlers.defresp_to_cmd_handler_f = NULL;
    zb_manager_obj.action_handlers.info_attr_reporting_handler_f = NULL;
    zb_manager_obj.action_handlers.info_attr_set_value_handler_f = NULL;
    zb_manager_obj.action_handlers.info_ias_zone_status_change_handler_f = NULL;
    zb_manager_obj.action_handlers.input_req_ias_zone_enroll_handler_f = NULL;
    zb_manager_obj.action_handlers.resp_to_cmd_attr_discover_handler_f = NULL;
    zb_manager_obj.action_handlers.resp_to_cmd_attr_read_handler_f = NULL;
    zb_manager_obj.action_handlers.resp_to_cmd_attr_write_handler_f = NULL;
    zb_manager_obj.action_handlers.resp_to_cmd_config_report_handler_f = NULL;
    
    // utility MATCH_DESC
    zb_manager_obj.utility_functions.cmd_to_call_match_desc_func = zb_manager_match_desc_func;
    zb_manager_obj.utility_functions.print_last_match_desc_resp_result_func = zb_manager_print_match_desc_resp_records;
    zb_manager_obj.utility_functions_callbacks.match_desc_callback = NULL;
    
    // utility IEEE_ADDR
    zb_manager_obj.utility_functions.cmd_to_call_ieee_addr_req_func = zb_manager_ieee_addr_func;
    zb_manager_obj.utility_functions.print_last_ieee_addr_resp_result_func = zb_manager_print_ieee_addr_resp_records;
    zb_manager_obj.utility_functions_callbacks.ieee_addr_req_callback = NULL;
    
    // utility NODE_DESC
    zb_manager_obj.utility_functions.cmd_to_call_node_desc_req_func = zb_manager_node_desc_func;
    zb_manager_obj.utility_functions.print_last_node_desc_resp_result_func = zb_manager_print_node_desc_resp_records;
    zb_manager_obj.utility_functions_callbacks.node_desc_req_callback = NULL;

    // utility simple_desc
    zb_manager_obj.utility_functions.cmd_to_call_simple_desc_req_func = zb_manager_simple_desc_func;
    zb_manager_obj.utility_functions_callbacks.simple_desc_req_callback = NULL;
    // clusters and endpoints
    zb_manager_obj.esp_local_zb_ep_list = esp_zb_ep_list_create();
    zb_manager_obj.endpoints_func.generate_endpoint_func = zb_manager_generate_endpoint;
    
    return ret;
}

esp_err_t zb_manager_start()
{
    //---------------------------------- Запуск очереди сообщений для ESP_ZIGBEE_EventsTask
    xZBEventsQueueHandle = NULL;
    xZBEventsQueueHandle = xQueueCreateStatic(ZB_EVENTS_QUEUE_SIZE, ZB_EVENTS_QUEUE_ITEM_SIZE, &ZB_QueueStorage[0], &xZBEventsQueueBuffer);
    if (xZBEventsQueueHandle == NULL) return ESP_FAIL;

    //-----------------------------------Запуск цикла обработки сообщений
    static StaticTask_t xZBEventsTaskBuffer;
    static StackType_t xZBEventsStack[ZIGBEE_EVENTS_STACK_SIZE];
    xZBEventsTaskHandle = NULL;
    //xZBEventsTaskHandle = xTaskCreateStatic(ESP_ZIGBEE_EventsTask, "Zigbee_Events_Task", ZIGBEE_EVENTS_STACK_SIZE, NULL, 10, xZBEventsStack, &xZBEventsTaskBuffer);
    //if (xZBEventsTaskHandle == NULL) return ESP_FAIL;
    //assert(xZBEventsTaskHandle);

    //----------------------------------- Запуск основного цикла Zigbee
    //zb_manager_init_t *init_ctx = calloc(1, sizeof(zb_manager_init_t));
   
    //init_ctx->esp_local_zb_ep_list = init_params.esp_local_zb_ep_list;
    //init_ctx->action_handler = init_params.action_handler;

    static StaticTask_t xZBTaskBuffer; 
    static StackType_t xZBStack[ZIGBEE_STACK_SIZE];
    xZBTaskHandle = NULL;
    xZBTaskHandle = xTaskCreateStatic(ESP_ZIGBEE_Task, "Zigbee_Task", ZIGBEE_STACK_SIZE, &zb_manager_obj, 20, xZBStack, &xZBTaskBuffer);
    if (xZBTaskHandle == NULL) return ESP_FAIL;
    assert(xZBTaskHandle);
    ESP_LOGW(TAG, "RAM control after zb_manager_start in zb_manager %lu", esp_get_free_heap_size());
    return ESP_OK;
}

esp_err_t zb_manager_register_match_desc_cb(zb_manager_match_desc_func_CB_t match_desc_callback)
{
    if(match_desc_callback == NULL)
    {
        ESP_LOGW(TAG, "Ошибка %s во время zb_manager_register_match_desc_cb", esp_err_to_name(ESP_ERR_INVALID_ARG));
        return ESP_ERR_INVALID_ARG;
    }
        zb_manager_obj.utility_functions_callbacks.match_desc_callback = match_desc_callback;

    return ESP_OK;
}

esp_err_t zb_manager_register_ieee_addr_req_cb(zb_manager_ieee_addr_req_func_CB_t ieee_addr_req_callback)
{
    if(ieee_addr_req_callback == NULL)
    {
        ESP_LOGW(TAG, "Ошибка %s во время zb_manager_register_ieee_addr_req_cb", esp_err_to_name(ESP_ERR_INVALID_ARG));
        return ESP_ERR_INVALID_ARG;
    }
        zb_manager_obj.utility_functions_callbacks.ieee_addr_req_callback = ieee_addr_req_callback;
    return ESP_OK;
}

esp_err_t zb_manager_register_node_desc_cb(zb_manager_node_desc_req_func_CB_t node_desc_callback)
{
    if(node_desc_callback == NULL)
    {
        ESP_LOGW(TAG, "Ошибка %s во время zb_manager_register_node_desc_cb", esp_err_to_name(ESP_ERR_INVALID_ARG));
        return ESP_ERR_INVALID_ARG;
    }
        zb_manager_obj.utility_functions_callbacks.node_desc_req_callback = node_desc_callback;
    return ESP_OK;
}

esp_err_t zb_manager_register_simple_desc_cb(zb_manager_simple_desc_req_func_CB_t simple_desc_callback)
{
    if(simple_desc_callback == NULL)
    {
         ESP_LOGW(TAG, "Ошибка %s во время zb_manager_register_simple_desc_cb", esp_err_to_name(ESP_ERR_INVALID_ARG));
        return ESP_ERR_INVALID_ARG;
    }
    zb_manager_obj.utility_functions_callbacks.simple_desc_req_callback = simple_desc_callback;
    return ESP_OK;
}

esp_err_t zb_manager_register_print_log_to_screen_cb(print_log_to_screen_CB_t print_log_to_screen_callback)
{
    if(print_log_to_screen_callback == NULL)
    {
        ESP_LOGW(TAG, "Ошибка %s во время zb_manager_print_log_to_screen_cb", esp_err_to_name(ESP_ERR_INVALID_ARG));
        return ESP_ERR_INVALID_ARG;
    }
    zb_manager_obj.utility_functions_callbacks.print_log_to_screen_callback = print_log_to_screen_callback;
    return ESP_OK;
}