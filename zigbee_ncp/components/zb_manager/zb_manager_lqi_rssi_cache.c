#include "zb_manager_lqi_rssi_cache.h"

void update_device_lqi(uint16_t short_addr, uint8_t lqi)
{
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (lqi_cache[i].short_addr == short_addr || lqi_cache[i].short_addr == 0) {
            lqi_cache[i].short_addr = short_addr;
            lqi_cache[i].lqi = lqi;
            //lqi_cache[i].rssi = rssi;
            return;
        }
    }
}

void update_device_rssi(uint16_t short_addr, uint8_t rssi)
{
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (lqi_cache[i].short_addr == short_addr || lqi_cache[i].short_addr == 0) {
            lqi_cache[i].short_addr = short_addr;
            //lqi_cache[i].lqi = lqi;
            lqi_cache[i].rssi = rssi;
            return;
        }
    }
}

void get_device_lqi_rssi(uint16_t short_addr, uint8_t *lqi, int8_t *rssi)
{
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (lqi_cache[i].short_addr == short_addr) {
            *lqi = lqi_cache[i].lqi;
            *rssi = lqi_cache[i].rssi;
            return;
        }
    }
    *lqi = 0;
    *rssi = 0;
}