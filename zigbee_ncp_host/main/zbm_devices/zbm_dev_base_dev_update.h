#ifndef ZBM_DEV_BASE_DEV_UPDATE_H

#define ZBM_DEV_BASE_DEV_UPDATE_H

#include "zbm_dev_types.h"

// не использовать напрямую (небезопасное изменение устройств)
esp_err_t zbm_dev_base_dev_update_from_read_response(device_custom_t* dev, endpoint_custom_t* ep, zb_manager_cmd_read_attr_resp_message_t* read_resp);

esp_err_t zbm_dev_base_dev_update_from_report(device_custom_t* dev, endpoint_custom_t* ep, zb_manager_cmd_report_attr_resp_message_t* rep);

#endif

