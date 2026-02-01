#ifndef ZB_MANAGER_H
#define ZB_MANAGER_H



/********************************************* NCP HOST MODE ***********************************/

        #include "zb_manager_ncp_host.h"

        /**** API FOR USER ****/
        extern bool isZigbeeNetworkOpened; // global variable in zb_manager_ncp_host.c
        extern zigbee_ncp_module_state_e zigbee_ncp_module_state; // global variable in zb_manager_ncp_host.c

        esp_err_t zm_init_devices_base(void);
        esp_err_t zm_FastStartZigbee(esp_host_zb_endpoint_t *endpoint1, esp_host_zb_endpoint_t *endpoint2, esp_host_zb_endpoint_t *endpoint3, esp_host_zb_endpoint_t *endpoint4);

        esp_err_t zm_FastRestartZigbeeOnFoultedure(void);
        esp_err_t zm_open_network(uint8_t seconds);
        esp_err_t zm_close_network(void);

        // for user functions
        // declare in esp_zigbee_core.h
        //extern esp_zb_app_signal_handler_t zb_manager_user_app_signal_handler; // информативное описание, менять через zm_register_user_app_signal_handler();
        esp_err_t zm_register_user_app_signal_handler(local_esp_zb_app_signal_handler_t user_app_signal_handler);

        //extern zb_manager_event_handler_t zb_manager_user_event_handler; // информативное описание, менять через zm_register_user_event_handler();
        esp_err_t zm_register_user_event_action_handler(zb_manager_event_action_handler_t user_event_action_handler, void* user_ctx);

        // можно реализовать эту функцию и зарегистрировать через zm_register_user_event_handler(&zb_manager_user_event_handler)
        extern void zm_user_event_action_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);
        
        // можно реализовать эту функцию и зарегистрировать через zm_register_user_app_signal_handler(&esp_zb_app_signal_handler)
        
        extern void esp_zb_app_signal_handler(local_esp_zb_app_signal_t *signal_s);
        





/*
uint16_t inputCluster[] = {ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF};
    uint16_t outputCluster[] = {ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY};
    esp_host_zb_endpoint_t host_endpoint = {
        .endpoint = endpoint_id,
        .profileId = ESP_ZB_AF_HA_PROFILE_ID,
        .deviceId = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
        .appFlags = 0,
        .inputClusterCount = sizeof(inputCluster) / sizeof(inputCluster[0]),
        .inputClusterList = inputCluster,
        .outputClusterCount = sizeof(outputCluster) / sizeof(outputCluster[0]),
        .outputClusterList = outputCluster,
    };
*/

#endif // ZB_MANAGER_H
