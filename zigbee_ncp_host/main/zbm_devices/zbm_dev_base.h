#ifndef ZBM_DEV_BASE_H

#define ZBM_DEV_BASE_H

#include "zbm_dev_types.h"
#include "cJSON.h"

#define REMOTE_DEVICES_COUNT (16)
#define SAVE_TASK_STACK_SIZE (8192)
#define SAVE_TASK_PRIORITY (10)

#define SAVE_TASK_CMD_QUEUE_SIZE  3
#define SAVE_TASK_CMD_SAVE        1

#define ZBM_BASE_MUTEX_TIMEOUT_LONG_MS        1000
#define ZBM_BASE_MUTEX_TIMEOUT_SHORT_MS       100



esp_err_t zbm_dev_base_init(uint8_t core_id);

// используется только в pairing safe mode не нужен
device_custom_t* zbm_dev_base_create_new_device_obj(esp_zb_ieee_addr_t ieee_addr); // скорее всего применение из ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED

esp_err_t zbm_dev_base_dev_obj_append_safe(device_custom_t* dev_object);
// используется только в pairing safe mode не нужен
endpoint_custom_t* zbm_dev_base_create_new_endpoint_obj(uint8_t ep_id);

// поиск по короткому адресу
device_custom_t* zbm_dev_base_find_device_by_short_safe(uint16_t short_addr);

// поиск по длинному адресу
device_custom_t* zbm_dev_base_find_device_by_long_safe(esp_zb_ieee_addr_t *ieee);

esp_err_t zbm_dev_base_dev_delete_safe(device_custom_t* dev_object);

cJSON *zbm_base_dev_short_list_for_webserver(void);
cJSON *zbm_dev_base_device_to_json(device_custom_t* dev);
esp_err_t zbm_dev_base_dev_delete_safe(device_custom_t* dev_object);

cJSON *zbm_dev_base_device_to_json(device_custom_t* dev);

cJSON *zbm_dev_base_to_json_safe(void);

esp_err_t zbm_dev_base_queue_save_req_cmd(void);

// UPDATE DATA
esp_err_t zbm_dev_base_dev_update_from_read_response_safe(zb_manager_cmd_read_attr_resp_message_t* read_resp);

esp_err_t zbm_dev_base_dev_update_from_report_notify_safe(zb_manager_cmd_report_attr_resp_message_t *rep);

esp_err_t zbm_dev_base_dev_update_friendly_name(uint16_t short_addr, const char* name);

esp_err_t zbm_dev_base_endpoint_update_friendly_name(uint16_t short_addr, uint8_t ep_id, const char* name);
#endif


