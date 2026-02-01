#ifndef ZB_MANAGER_TUYA_HELPER_H    
#define ZB_MANAGER_TUYA_HELPER_H
#include "esp_zigbee_core.h"

typedef struct {
    uint16_t short_addr;
    uint8_t  endpoint;
    uint8_t  dp_id;
    uint8_t  dp_type;
    uint32_t dp_value;  // подходит для int32, enum, bool
    int8_t   rssi;
    uint8_t  lqi;
} zb_manager_tuya_dp_report_t;

esp_err_t parse_and_send_tuya_dp(uint16_t short_addr, uint8_t endpoint, const uint8_t *data, uint16_t data_len, int8_t rssi, uint8_t lqi);
#endif