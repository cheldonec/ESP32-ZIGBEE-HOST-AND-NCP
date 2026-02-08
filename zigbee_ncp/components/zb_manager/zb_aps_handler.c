//#include "main_core.h"
#include "esp_log.h"
#include "zboss_api.h"
#include "string.h"
#include "zb_aps_handler.h"
#include "zb_manager_lqi_rssi_cache.h"

static const char *TAG = "ZB_APS_MODULE";

bool esp_zb_aps_data_indication_handler(esp_zb_apsde_data_ind_t ind)
{
    ESP_LOGI(TAG, "aps_data_indication_handler");
    
     ESP_LOGI(TAG,"data_indication status %02x profileId: %04x clusterId: %04x srcAddr: %04x dstAddr: %04x srcEndpoint: %02x dstEndpoint: %02x data: %d  lqi: %d",ind.status, ind.profile_id, ind.cluster_id ,ind.src_short_addr,ind.dst_short_addr, ind.src_endpoint,ind.dst_endpoint,(int)ind.asdu_length, ind.lqi);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, ind.asdu, ind.asdu_length, ESP_LOG_INFO);

    update_device_lqi(ind.src_short_addr, ind.lqi);
    /*for(int i = 0; i < CONTROL_DEVICES_LIST_MAX_COUNT; i++)
        {
            if(control_devices_list[i]->device_obj)
            {
                if(control_devices_list[i]->device_obj->short_addr == ind.src_short_addr)
                {
                   control_devices_list[i]->lqi = ind.lqi;
                   break; 
                }
            }
        }*/
    //return true;
    return false;     // false чтобы стек продолжил обработку сообщения
}

void esp_zb_aps_data_confirm_handler(esp_zb_apsde_data_confirm_t confirm)
{

    ESP_LOGI(TAG, "aps_data_confirm_handler");
    /********My code */
     if (confirm.status == 0x00)
        {
            esp_zb_addr_u addr;
            memcpy(&addr, &confirm.dst_addr, sizeof(esp_zb_addr_u));
        ESP_LOGI("APSDE CONFIRM",
                 "Sent successfully from endpoint %d, source address 0x%04hx to endpoint %d, destination address 0x%04hx", confirm.src_endpoint, esp_zb_get_short_address(), confirm.dst_endpoint, addr.addr_short);
        ESP_LOG_BUFFER_CHAR_LEVEL("APSDE CONFIRM", confirm.asdu, confirm.asdu_length, ESP_LOG_INFO);
    }
    else
    {
        ESP_LOGE("APSDE CONFIRM", "Failed to send APSDE-DATA request, error code: %d", confirm.status);
    }
}