/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_zigbee_zcl_command.h"

#include "esp_log.h"

/* ZCL on off cluster list command */

uint8_t esp_zb_zcl_on_off_cmd_req(esp_zb_zcl_on_off_cmd_t *cmd_req)
{
    bool on_off = false;
    

    ESP_LOGW("ZCL", "esp_zb_zcl_on_off_cmd_req");
    esp_zb_zcl_custom_cluster_cmd_t zcl_data = {
        .zcl_basic_cmd = cmd_req->zcl_basic_cmd,
        .address_mode = cmd_req->address_mode,
        .profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        .custom_cmd_id = cmd_req->on_off_cmd_id,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .data.type = ESP_ZB_ZCL_ATTR_TYPE_BOOL,
        .data.value = &on_off,
    };
    esp_zb_zcl_custom_cluster_cmd_req(&zcl_data);

    return ESP_OK;
}

uint8_t esp_zb_zcl_on_off_on_with_timed_off_cmd_req(esp_zb_zcl_on_off_on_with_timed_off_cmd_t *cmd_req)
{
   /* ESP_LOGW("ZCL", "esp_zb_zcl_on_off_on_with_timed_off_cmd_req");
    typedef struct {
        uint8_t on_off_control;
        uint16_t on_time;                                       
        uint16_t off_wait_time;
    } __attribute__ ((packed)) on_with_timed_off_params_t;

    on_with_timed_off_params_t params;
    params.on_off_control = cmd_req->on_off_control;
    params.on_time = cmd_req->on_time;
    params.off_wait_time = cmd_req->off_wait_time;

    esp_zb_zcl_custom_cluster_cmd_t zcl_data = {
        .zcl_basic_cmd = cmd_req->zcl_basic_cmd,
        .address_mode = cmd_req->address_mode,
        .profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        .custom_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_ON_WITH_TIMED_OFF_ID,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .data.type = ESP_ZB_ZCL_ATTR_TYPE_SET,
        .data.value = &params,
        .data.size = sizeof(params),
        
    };
    esp_zb_zcl_custom_cluster_cmd_req(&zcl_data);*/
    return ESP_OK;
}
