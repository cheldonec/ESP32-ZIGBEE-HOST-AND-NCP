#ifndef NCP_HOST_ZB_API_TO_NCP_H

#define NCP_HOST_ZB_API_TO_NCP_H

#include "esp_err.h"
#include "ncp_host_zb_api.h"
#include "zbm_ncp_connect.h"
#include "zbm_core_sync.h"
#include "cJSON.h"
#include "ncp_host_zb_api_from_ncp.h"


esp_err_t zbm_to_ncp_cmd_init_zigbee_stack(void);

esp_err_t zbm_to_ncp_cmd_register_endpoint(esp_host_zb_endpoint_t *endpoint);

esp_err_t zbm_to_ncp_cmd_start_zigbee_stack(void);

esp_err_t zbm_to_ncp_req_get_coord_long_addr(esp_zb_ieee_addr_t ieee_addr);

esp_err_t zbm_to_ncp_req_get_extended_pan_id(esp_zb_ieee_addr_t ext_pan_id);

uint16_t zbm_to_ncp_req_get_pan_id(void);

uint8_t zbm_to_ncp_req_get_current_channel(void);

uint16_t zbm_to_ncp_req_get_network_short_addr(void);

esp_err_t zbm_to_ncp_cmd_open_zigbee_network(uint8_t seconds);

esp_err_t zbm_to_ncp_cmd_close_zigbee_network(void);

esp_err_t zbm_to_ncp_cmd_get_local_long_addr(esp_zb_ieee_addr_t ieee_addr);

// ===============================  ZDO ==========================================
// active_ep_req

esp_err_t zbm_to_ncp_req_active_endpoint_req(uint16_t  addr_of_interest, local_esp_zb_zdo_active_ep_callback_t user_cb, void *user_ctx);

esp_err_t zbm_to_ncp_req_simple_desc_req(uint16_t addr_of_interest, uint8_t endpoint, local_esp_zb_zdo_simple_desc_callback_t user_cb, void *user_ctx);


// ===============================  ZCL ==========================================
uint8_t zb_manager_disc_attr_cmd_req(esp_zb_zcl_disc_attr_cmd_t *cmd_req);

//return the transaction sequence number
uint8_t zbm_to_ncp_req_read_attributes(esp_zb_zcl_read_attr_cmd_t *cmd_req);

//uint8_t zbm_to_ncp_req_write_attributes(esp_zb_zcl_write_attr_cmd_t *cmd_req);

uint8_t zbm_to_ncp_req_send_zcl_cmd_to_cluster(zbm_send_zcl_cmd_to_cluster_cmd_t *cmd_req);

uint8_t zbm_to_ncp_req_send_zcl_cmd_from_ws_json(cJSON *req_json);

#endif
