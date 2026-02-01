#include "zb_manager_identify_cluster.h"

#include "esp_err.h"
#include "string.h"

esp_err_t zigbee_manager_identify_cluster_update_attribute(zb_manager_identify_cluster_t* cluster, uint16_t attr_id, void* value)
{
    switch (attr_id)
    {
    case 0x0000:
        cluster->identify_time = *((uint16_t*)value);
        break;
    
    default:
        break;
    }
    return ESP_OK;
}