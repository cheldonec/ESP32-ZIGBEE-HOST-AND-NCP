#include "zb_manager_temperature_meas_cluster.h"
#include <stdint.h>
#include <stdbool.h>

esp_err_t zb_manager_temp_meas_cluster_update_attribute(zb_manager_temperature_measurement_cluster_t* cluster, uint16_t attr_id, void* value)
{
    switch (attr_id)
    {
    case 0x0000:
        cluster->measured_value = *(int16_t*)value;
        break;
    case 0x0001:
        cluster->min_measured_value = *(int16_t*)value;
        break;
    case 0x0002:
        cluster->max_measured_value = *(int16_t*)value;
        break;
    case 0x0003:
        cluster->tolerance = *(uint16_t*)value;
        break;
    default:
        break;
    }
    return ESP_OK;
}