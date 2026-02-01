#ifndef ZB_MANAGER_LQI_RSSI_CACHE_H
#define ZB_MANAGER_LQI_RSSI_CACHE_H

#include "esp_zigbee_core.h"
#include "zb_manager.h"
#include "zb_manager_config.h"

static struct {
    uint16_t short_addr;
    uint8_t lqi;
    int8_t rssi;
} lqi_cache[MAX_CHILDREN];


void update_device_lqi(uint16_t short_addr, uint8_t lqi);

void update_device_rssi(uint16_t short_addr, uint8_t rssi);

void get_device_lqi_rssi(uint16_t short_addr, uint8_t *lqi, int8_t *rssi);
#endif