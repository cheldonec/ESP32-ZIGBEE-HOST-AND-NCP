#ifndef ZB_MANAGER_ACTION_HANDLER_WORKER_H
#define ZB_MANAGER_ACTION_HANDLER_WORKER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the action handler worker (for reports from paired devices)
 * @param core Core to pin to
 * @return ESP_OK on success
 */
esp_err_t zb_manager_start_action_worker(uint8_t core);

/**
 * @brief Post event to action worker
 * @param id Event ID
 * @param data Event data
 * @param size Data size
 * @return true if posted
 */
bool zb_manager_post_to_action_worker(int32_t id, void *data, size_t size);

void zb_manager_start_powered_device_sync(void); // started zb_manager_ncp_host.c in  zb_manager_ncp_host_fast_start

#ifdef __cplusplus
}
#endif

#endif // ZB_MANAGER_ACTION_HANDLER_WORKER_H
