#ifndef ZBM_DEV_APPEND_SCHEDULER_H

#define ZBM_DEV_APPEND_SCHEDULER_H
#include "zbm_dev_types.h"
#include "esp_timer.h"

typedef struct {
    uint8_t                                 index_in_sheduler_array;
    device_custom_t*                        appending_device;
    esp_timer_handle_t                      appending_controll_timer;
    esp_zb_zcl_read_attr_cmd_t              tuya_magic_read_attr_cmd_param;    //
    uint8_t                                 tuya_magic_req_tsn;
    uint8_t                                 tuya_magic_resp_tsn;
    uint8_t                                 tuya_magic_status;         // 0-инициализация 1 - в процессе, 2 - готово
    uint8_t                                 tuya_magic_try_count;
    
    local_esp_zb_zdo_active_ep_req_param_t        active_ep_req;
   
    uint8_t                                 active_ep_req_status;      // 0-инициализация 1 - в процессе, 2 - готово
    uint8_t                                 simple_desc_req_count;
    
    local_esp_zb_zdo_simple_desc_req_param_t*     simple_desc_req_list;
    
    
    uint8_t*                                simple_desc_req_simple_desc_req_list_status;    // 0-инициализация 1 - в процессе, 2 - готово
    //esp_zb_zdo_bind_req_param_t*            bind_req_list;
    //uint8_t                                 bind_req_count;
    //uint8_t*                                bind_req_list_status; // 0-инициализация 1 - в процессе, 2 - готово
}device_appending_sheduler_t;

extern uint8_t zbm_DeviceAppendShedulerCount;
extern device_appending_sheduler_t** zbm_DeviceAppendShedulerArray;
#define DEVICE_APPEND_SHEDULER_COUNT (2)

esp_err_t zbm_dev_pairing_shedulers_init(void);
esp_err_t zbm_dev_delete_appending_sheduler(device_appending_sheduler_t* sheduler);

#endif