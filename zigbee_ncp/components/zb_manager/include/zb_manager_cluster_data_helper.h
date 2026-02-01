#ifndef ZB_MANAGER_CLUSTER_DATA_HELPER_H
#define ZB_MANAGER_CLUSTER_DATA_HELPER_H

#include "esp_zigbee_core.h"
#include "zb_config.h"
#include "esp_log.h"

typedef struct zbstring_s {
    uint8_t len;
    char data[];
} ESP_ZB_PACKED_STRUCT
zbstring_t;

const char* zb_manager_get_cluster_name(uint16_t clusterID);
const char* zb_manager_get_attr_name(uint16_t clusterID, uint16_t attr_id);

uint16_t zb_manager_get_attr_data_size(esp_zb_zcl_attr_type_t attr_type, void* attr_data);

const char* zb_manager_attr_type_to_string(uint8_t attr_type);

#endif