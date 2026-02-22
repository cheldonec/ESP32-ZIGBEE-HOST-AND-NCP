#include "zbm_dev_append_sheduler.h"

uint8_t zbm_DeviceAppendShedulerCount = 0;
device_appending_sheduler_t** zbm_DeviceAppendShedulerArray = NULL;

esp_err_t zbm_dev_pairing_shedulers_init()
{
    zbm_DeviceAppendShedulerCount = DEVICE_APPEND_SHEDULER_COUNT;
    zbm_DeviceAppendShedulerArray = calloc(zbm_DeviceAppendShedulerCount, sizeof(device_appending_sheduler_t*));
    if(zbm_DeviceAppendShedulerArray == NULL) return ESP_FAIL;
    for (int i = 0; i < zbm_DeviceAppendShedulerCount; i++) 
    {
        zbm_DeviceAppendShedulerArray[i] = NULL;
    }
    return ESP_OK;
}

esp_err_t zbm_dev_delete_appending_sheduler(device_appending_sheduler_t* sheduler)
{ 
    if (!sheduler) return ESP_FAIL;
    for (int i = 0; i < zbm_DeviceAppendShedulerCount; i++) 
    {
        if (zbm_DeviceAppendShedulerArray[i] == sheduler)
        {
            free(sheduler->simple_desc_req_list);
            sheduler->simple_desc_req_list = NULL;
            free(sheduler->simple_desc_req_simple_desc_req_list_status);
            sheduler->simple_desc_req_simple_desc_req_list_status = NULL;
            //free(sheduler);
            //sheduler = NULL;
            zbm_DeviceAppendShedulerArray[i] = NULL;
            break;
        }
    }
    
    return ESP_OK;
}