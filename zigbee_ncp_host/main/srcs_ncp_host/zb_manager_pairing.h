#ifndef ZB_MANAGER_PAIRING_H
#define ZB_MANAGER_PAIRING_H

#include "zb_manager_devices.h"
#include "ncp_host_zb_api.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the pairing worker task
 * @param core Core to pin the task (0 or 1)
 * @return ESP_OK on success
 */
esp_err_t zb_manager_start_pairing_worker(uint8_t core);

/**
 * @brief Post event to pairing worker
 * @param id Event ID (e.g., ATTR_READ_RESP)
 * @param data Event data (copied)
 * @param size Size of data
 * @return true if posted
 */
bool zb_manager_post_to_pairing_worker(int32_t id, void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif // ZB_MANAGER_PAIRING_H
