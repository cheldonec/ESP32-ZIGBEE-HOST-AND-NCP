
#include "ncp_host_zb_api_core.h"

local_esp_zb_app_signal_handler_t zb_manager_user_app_signal_handler = NULL;
zb_manager_event_action_handler_t zb_manager_user_event_action_handler = NULL;

void *esp_zb_app_signal_get_params(uint32_t *signal_p)
{
    local_esp_zb_app_signal_msg_t *app_signal_msg = (local_esp_zb_app_signal_msg_t *)signal_p;

    return app_signal_msg ? (void *)app_signal_msg->msg : (void *)app_signal_msg;
}

esp_err_t esp_zb_bdb_start_top_level_commissioning(uint8_t mode_mask)
{
    return ESP_OK;
}

void esp_zb_scheduler_alarm(esp_zb_callback_t cb, uint8_t param, uint32_t time)
{

}

esp_err_t esp_zb_set_primary_network_channel_set(uint32_t channel_mask)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ESP_NCP_NETWORK_PRIMARY_CHANNEL_SET, &channel_mask, sizeof(uint32_t), &output, &outlen);

    return ESP_OK;
}

void esp_zb_init(esp_zb_cfg_t *nwk_cfg)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ESP_NCP_NETWORK_INIT, NULL, 0, &output, &outlen);
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP,
        .msg = NULL,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = (output == ESP_ZNSP_SUCCESS) ? ESP_OK : ESP_ERR_INVALID_ARG,
    };

    zb_manager_app_signal_handler(&app_signal);

    esp_host_zb_output(ESP_NCP_NETWORK_FORMNETWORK, nwk_cfg, sizeof(esp_zb_cfg_t), &output, &outlen);
}

esp_err_t esp_zb_start(bool autostart)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ESP_NCP_NETWORK_START, &autostart, sizeof(bool), &output, &outlen);

    return ESP_OK;
}

uint16_t esp_zb_get_short_address(void)
{
    uint16_t output = 0;
    uint16_t outlen = sizeof(uint16_t);
    
    esp_host_zb_output(ESP_NCP_NETWORK_SHORT_ADDRESS_GET, NULL, 0, &output, &outlen);

    return output;
}

void esp_zb_get_long_address(esp_zb_ieee_addr_t addr)
{
    uint16_t outlen = sizeof(esp_zb_ieee_addr_t);

    esp_host_zb_output(ESP_NCP_NETWORK_LONG_ADDRESS_GET, NULL, 0, addr, &outlen);
}

uint16_t esp_zb_address_short_by_ieee(esp_zb_ieee_addr_t address)
{
    uint16_t output = 0;
    uint16_t outlen = sizeof(uint16_t);

    esp_host_zb_output(ESP_NCP_NETWORK_IEEE_TO_SHORT, address, sizeof(esp_zb_ieee_addr_t), &output, &outlen);

    return output;
}

esp_err_t esp_zb_ieee_address_by_short(uint16_t short_addr, uint8_t *ieee_addr)
{
    uint16_t outlen = sizeof(esp_zb_ieee_addr_t);

    esp_host_zb_output(ESP_NCP_NETWORK_SHORT_TO_IEEE, &short_addr, sizeof(uint16_t), ieee_addr, &outlen);

    return ESP_OK;
}

uint16_t esp_zb_get_pan_id(void)
{
    uint16_t output = 0;
    uint16_t outlen = sizeof(uint16_t);

    esp_host_zb_output(ESP_NCP_NETWORK_PAN_ID_GET, NULL, 0, &output, &outlen);

    return output;
}

void esp_zb_get_extended_pan_id(esp_zb_ieee_addr_t ext_pan_id)
{
    uint16_t outlen = sizeof(esp_zb_ieee_addr_t);

    esp_host_zb_output(ESP_NCP_NETWORK_EXTENDED_PAN_ID_GET, NULL, 0, ext_pan_id, &outlen);
}

uint8_t esp_zb_get_current_channel(void)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ESP_NCP_NETWORK_CHANNEL_GET, NULL, 0, &output, &outlen);

    return output;
}

/***************************************************** ZB_MANAGER_FUNCTIONS *********************************/
esp_err_t zb_manager_init(void)
{

    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ZB_MANAGER_INIT_CMD, NULL, 0, &output, &outlen);

    return output;
}

esp_err_t zb_manager_start(void)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ZB_MANAGER_START_CMD, NULL, 0, &output, &outlen);

    return output;
}

esp_err_t zb_manager_open_network(uint8_t seconds)
{
    uint8_t output = 0;                        // результат выполнения команды
    uint16_t outlen = sizeof(uint8_t);        // ожидаемый размер ответа
    uint16_t req_param_len = sizeof(uint8_t);        // размер запроса
    esp_host_zb_output(ZB_MANAGER_OPEN_NETWORK_CMD, &seconds, req_param_len, &output, &outlen);
    return output;
}

esp_err_t zb_manager_close_network(void)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ZB_MANAGER_CLOSE_NETWORK_CMD, NULL, 0, &output, &outlen);

    return output;
}

esp_err_t zb_manager_register_user_app_signal_handler(local_esp_zb_app_signal_handler_t user_app_signal_handler)
{
    zb_manager_user_app_signal_handler = user_app_signal_handler;
    return ESP_OK;
}

esp_err_t zb_manager_register_event_action_handler(zb_manager_event_action_handler_t event_action_handler, void *user_ctx)
{
    return esp_event_handler_register(ZB_HANDLER_EVENTS, ESP_EVENT_ANY_ID, event_action_handler, user_ctx);
}

esp_err_t zb_manager_register_user_event_handler(zb_manager_event_action_handler_t user_event_action_handler, void* user_ctx)
{
    zb_manager_user_event_action_handler = user_event_action_handler;
    return esp_event_handler_register(ZB_HANDLER_EVENTS, ESP_EVENT_ANY_ID, user_event_action_handler, user_ctx);
    //return ESP_OK;
}