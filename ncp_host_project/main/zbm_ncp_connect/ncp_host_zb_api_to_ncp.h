#ifndef NCP_HOST_ZB_API_TO_NCP_H

#define NCP_HOST_ZB_API_TO_NCP_H

#include "esp_err.h"
#include "ncp_host_zb_api.h"
#include "zbm_ncp_connect.h"


esp_err_t zbm_to_ncp_cmd_init_zigbee_stack(void);

esp_err_t zbm_to_ncp_cmd_register_endpoint(esp_host_zb_endpoint_t *endpoint);

esp_err_t zbm_to_ncp_cmd_start_zigbee_stack(void);

esp_err_t zbm_to_ncp_req_get_coord_long_addr(esp_zb_ieee_addr_t ieee_addr);

esp_err_t zbm_to_ncp_req_get_extended_pan_id(esp_zb_ieee_addr_t ext_pan_id);

uint16_t zbm_to_ncp_req_get_pan_id(void);

uint8_t zbm_to_ncp_req_get_current_channel(void);

uint16_t zbm_to_ncp_req_get_network_short_addr(void);

// ===============================  ZCL ==========================================
//return the transaction sequence number
uint8_t zbm_to_ncp_req_read_attributes(esp_zb_zcl_read_attr_cmd_t *cmd_req);

#endif
