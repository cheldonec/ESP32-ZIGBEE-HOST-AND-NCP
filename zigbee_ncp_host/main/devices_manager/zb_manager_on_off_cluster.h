#ifndef ZB_MANAGER_ON_OFF_CLUSTER_H

#define ZB_MANAGER_ON_OFF_CLUSTER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_zigbee_zcl_command.h"
/**
 * @brief Structure representing the Zigbee On/Off Cluster
 *
 * This structure holds the state for the On/Off cluster, used to control
 * binary devices like switches, lights, relays, etc.
 */
typedef struct {
    /**
     * @brief On/Off State
     *
     * - Data Type: `bool`
     * - Value: `true` = On, `false` = Off
     * - Default: `false`
     */
    bool on_off;

    /**
     * @brief Global Scene Control
     *
     * Whether the device should use a global scene when turning on.
     * - Data Type: `bool`
     * - Default: `true`
     */
    bool global_scene_control;

    /**
     * @brief On Time
     *
     * The number of 0.1-second intervals the device has been on.
     * Used with On with Timed Off command.
     *
     * - Data Type: `uint16_t`
     * - Default: `0`
     */
    uint16_t on_time;

    /**
     * @brief Off Wait Time
     *
     * The number of 0.1-second intervals the device will wait before turning off.
     * - Data Type: `uint16_t`
     * - Default: `0`
     */
    uint16_t off_wait_time;

    /**
     * @brief Startup On/Off
     *
     * Defines the behavior when the device is powered on.
     * - 0x00 = Off
     * - 0x01 = On
     * - 0x02 = Toggle
     * - 0xFF = Previous state
     *
     * - Data Type: `uint8_t`
     * - Default: `0xFF` (previous state)
     */
    uint8_t start_up_on_off;

    /**
     * @brief Last Update Timestamp
     *
     * Time in milliseconds when the `on_off` state was last updated.
     */
    uint32_t last_update_ms;

} zb_manager_on_off_cluster_t;

/**
 * @brief Default initialization macro for On/Off Cluster
 */
#define ZIGBEE_ON_OFF_CLUSTER_DEFAULT_INIT() { \
    .on_off = false, \
    .global_scene_control = true, \
    .on_time = 0, \
    .off_wait_time = 0, \
    .start_up_on_off = 0xFF, \
    .last_update_ms = 0, \
}

/**
 * @brief Command identifiers for On/Off cluster (for command handling)
 */
typedef enum {
    ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID                         = 0x00, /*!< "Turn off" command. */
    ESP_ZB_ZCL_CMD_ON_OFF_ON_ID                          = 0x01, /*!< "Turn on" command. */
    ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID                      = 0x02, /*!< "Toggle state" command. */
    ESP_ZB_ZCL_CMD_ON_OFF_OFF_WITH_EFFECT_ID             = 0x40, /*!< "Off with effect" command. */
    ESP_ZB_ZCL_CMD_ON_OFF_ON_WITH_RECALL_GLOBAL_SCENE_ID = 0x41, /*!< "On with recall global scene" command. */
    ESP_ZB_ZCL_CMD_ON_OFF_ON_WITH_TIMED_OFF_ID           = 0x42, /*!< "On with timed off" command. */
} local_esp_zb_zcl_on_off_cmd_id_t;

/**
 * @brief Callback for applying On/Off state to hardware
 *
 * This function is called when the cluster state changes.
 * Example: turn on/off relay, GPIO, etc.
 *
 * @param on_off `true` for On, `false` for Off
 * @param user_data Context (e.g., GPIO number)
 */
typedef void (*zigbee_on_off_apply_cb_t)(bool on_off, void *user_data);

/**
 * @brief Update attribute in On/Off Cluster
 *
 * @param cluster Pointer to cluster structure
 * @param attr_id Attribute ID (e.g., 0x0000 for OnOff)
 * @param value Pointer to new value
 * @return esp_err_t ESP_OK on success
 */
esp_err_t zb_manager_on_off_cluster_update_attribute(zb_manager_on_off_cluster_t* cluster, uint16_t attr_id, void* value);

/**
 * @brief Handle On/Off cluster command (e.g., ON, OFF, TOGGLE)
 *
 * @param cluster Pointer to cluster
 * @param cmd_id Command ID (e.g., ZB_MANAGER_ON_OFF_CMD_ON)
 * @param apply_cb Callback to apply state to hardware
 * @param cb_user_data Context for apply_cb
 * @return esp_err_t ESP_OK on success
 */
esp_err_t zb_manager_on_off_cluster_handle_command(
    zb_manager_on_off_cluster_t* cluster,
    uint8_t cmd_id,
    zigbee_on_off_apply_cb_t apply_cb,
    void* cb_user_data
);

const char* zb_manager_get_on_off_cluster_attr_name(uint16_t attrID);

uint8_t zb_manager_read_on_off_attribute(uint16_t short_addr, uint8_t endpoint_id);

uint8_t zb_manager_read_on_off_attribute_by_ieee(uint16_t short_addr, uint8_t endpoint_id);

/* ZCL on off cluster list command */

/**
 * @brief   Send on-off command
 *
 * @param[in]  cmd_req  pointer to the on-off command @ref esp_zb_zcl_on_off_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t zb_manager_on_off_cmd_req(esp_zb_zcl_on_off_cmd_t *cmd_req);

/**
 * @brief Send on-off On With Timed Off command
 *
 * @note The On With Timed Off command allows devices to be turned on for a specific duration with a guarded off
         duration so that SHOULD the device be subsequently switched off.
 * @param[in] cmd_req pointer to the on-off on with timed off command @ref esp_zb_zcl_on_off_on_with_timed_off_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t zb_manager_on_off_on_with_timed_off_cmd_req(esp_zb_zcl_on_off_on_with_timed_off_cmd_t *cmd_req);
#endif // ZB_MANAGER_ON_OFF_CLUSTER_H