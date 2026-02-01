#include <stdio.h>
#include "zb_manager.h"
#include "string.h"
#include "esp_log.h"
static const char *TAG = "ZB_MANAGER";



esp_err_t zm_init_devices_base(void)
{
        return zb_manager_init_devices_base();
}

esp_err_t zm_FastStartZigbee(esp_host_zb_endpoint_t *endpoint1, esp_host_zb_endpoint_t *endpoint2, esp_host_zb_endpoint_t *endpoint3, esp_host_zb_endpoint_t *endpoint4)
{
    
        esp_host_zb_endpoint_t *ep1 = NULL;
        esp_host_zb_endpoint_t *ep2 = NULL;
        esp_host_zb_endpoint_t *ep3 = NULL;
        esp_host_zb_endpoint_t *ep4 = NULL;
        if (endpoint1 != NULL)
        {
           ep1 = calloc(1, sizeof(esp_host_zb_endpoint_t));
            memcpy(ep1, endpoint1, sizeof(esp_host_zb_endpoint_t));
        } else ESP_LOGW(TAG, "zm_FastStartZigbee endpoint1 is NULL");
        
        if (endpoint2 != NULL)
        {
          ep2 = calloc(1, sizeof(esp_host_zb_endpoint_t));
          memcpy(ep2, endpoint2, sizeof(esp_host_zb_endpoint_t));
        } else ESP_LOGW(TAG, "zm_FastStartZigbee endpoint2 is NULL");

        if (endpoint3 != NULL)
        {
           ep3 = calloc(1, sizeof(esp_host_zb_endpoint_t));
            memcpy(ep3, endpoint3, sizeof(esp_host_zb_endpoint_t));
        }else ESP_LOGW(TAG, "zm_FastStartZigbee endpoint3 is NULL");

        if (endpoint4 != NULL)
        {
           ep4 = calloc(1, sizeof(esp_host_zb_endpoint_t));
            memcpy(ep4, endpoint4, sizeof(esp_host_zb_endpoint_t));
        }else ESP_LOGW(TAG, "zm_FastStartZigbee endpoint4 is NULL");

        return zb_manager_ncp_host_fast_start(ep1, ep2, ep3, ep4);
    
}

esp_err_t zm_FastRestartZigbeeOnFoultedure(void)
{
        return zb_manager_ncp_host_restart_on_ncp_foulture();
}

esp_err_t zm_open_network(uint8_t seconds)
{
    return zb_manager_open_network(seconds);
}
esp_err_t zm_close_network(void)
{
    return zb_manager_close_network();
}

esp_err_t zm_register_user_app_signal_handler(local_esp_zb_app_signal_handler_t user_app_signal_handler)
{
    return zb_manager_register_user_app_signal_handler(user_app_signal_handler);
}

esp_err_t zm_register_user_event_action_handler(zb_manager_event_action_handler_t user_event_action_handler, void* user_ctx)
{
    return zb_manager_register_user_event_handler(user_event_action_handler, user_ctx);
}