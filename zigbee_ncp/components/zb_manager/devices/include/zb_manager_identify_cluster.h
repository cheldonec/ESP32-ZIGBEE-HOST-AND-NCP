#ifndef ZB_MANAGER_IDENTIFY_CLUSTER_H
#define ZB_MANAGER_IDENTIFY_CLUSTER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
/**
 * @brief Structure representing the Zigbee Identify Cluster attributes and state
 * 
 * This structure holds the state for the Identify Cluster, which is used to
 * make a device physically identifiable (e.g., by flashing a light or sounding
 * a buzzer). It is designed for initialization, state management, and command
 * handling.
 */
typedef struct {
    /**
     * @brief Identify Time
     * 
     * Represents the remaining time in seconds that the device will remain in
     * its "identify" mode. When this value is non-zero, the device should perform
     * an identifying action (e.g., flashing an LED).
     * 
     * - Value: 0x0000 to 0xfffe (0 to 65534 seconds)
     * - Default Value: 0x0000 (Identify mode is off)
     * 
     * When a device receives an `Identify` command with a new time value, this
     * field is updated. The device should decrement this value every second until
     * it reaches zero.
     */
    uint16_t identify_time;

    /**
     * @brief Effect Identifier
     * 
     * Used during the processing of a `Trigger Effect` command to store the
     * type of visual effect requested.
     * 
     * @see esp_zb_zcl_identify_trigger_effect_t
     */
    uint8_t effect_identifier;

    /**
     * @brief Effect Variant
     * 
     * Used during the processing of a `Trigger Effect` command to store the
     * specific variant or speed of the requested effect.
     */
    uint8_t effect_variant;

    /**
     * @brief Identify In Progress
     * 
     * A boolean flag to track whether an identification action is currently
     * being performed. This can be used to prevent overlapping effects.
     */
    bool identify_in_progress;

} zb_manager_identify_cluster_t;

/**
 * @brief Default initialization macro for the Identify Cluster structure
 * 
 * This macro initializes the structure with default values as specified
 * in the Zigbee specification. The device starts in a non-identifying state.
 */
#define ZIGBEE_IDENTIFY_CLUSTER_DEFAULT_INIT() { \
    .identify_time = 0x0000, \
    .effect_identifier = 0x00, \
    .effect_variant = 0x00, \
    .identify_in_progress = false, \
}

/**
 * @brief Effect Identifier values (for reference within the structure)
 * 
 * These values are used with the `Trigger Effect` command. The structure includes
 * the identifiers so that the device's state machine can react appropriately.
 */
#define ZIGBEE_IDENTIFY_EFFECT_BLINK               0x00
#define ZIGBEE_IDENTIFY_EFFECT_BREATHE             0x01
#define ZIGBEE_IDENTIFY_EFFECT_OKAY                0x02
#define ZIGBEE_IDENTIFY_EFFECT_CHANNEL_CHANGE      0x0B
#define ZIGBEE_IDENTIFY_EFFECT_FINISH_EFFECT       0xFE
#define ZIGBEE_IDENTIFY_EFFECT_STOP                0xFF

/**
 * @brief Function pointer for the Identify effect callback
 * 
 * This callback is invoked when the `identify_time` decrements to zero or
 * when a new identification command is received. It allows the application
 * to control the physical "identifying" action (e.g., starting/stopping an LED flash).
 * 
 * @param effect_id The effect identifier from a `Trigger Effect` command, or 0xFF if from a standard `Identify` command.
 * @param effect_variant The effect variant.
 * @param identify_time The new identify time value.
 * @param user_data A pointer to user-defined context data.
 */
typedef void (*zigbee_identify_effect_cb_t)(uint8_t effect_id, uint8_t effect_variant, uint16_t identify_time, void *user_data);

/**
 * @brief Example of initializing an Identify Cluster instance
 */
/*
zigbee_identify_cluster_t identify_info = ZIGBEE_IDENTIFY_CLUSTER_DEFAULT_INIT();

// Example: Handling an "Identify" command from the network
void handle_identify_command(uint16_t duration_seconds, zigbee_identify_effect_cb_t effect_cb, void *user_ctx) {
    // Clamp the duration to the maximum allowed value (65534 seconds)
    identify_info.identify_time = (duration_seconds > 0xFFFE) ? 0xFFFE : duration_seconds;

    // If the time is 0, stop the identification
    if (identify_info.identify_time == 0) {
        identify_info.identify_in_progress = false;
        if (effect_cb) {
            // Signal to stop any ongoing effect
            effect_cb(ZIGBEE_IDENTIFY_EFFECT_STOP, 0, 0, user_ctx);
        }
    } else {
        // Start or update the identification
        identify_info.identify_in_progress = true;
        if (effect_cb) {
            // Signal to start a generic "blink" effect for the given duration
            effect_cb(ZIGBEE_IDENTIFY_EFFECT_BLINK, 0, identify_info.identify_time, user_ctx);
        }
    }
}
*/
esp_err_t zigbee_manager_identify_cluster_update_attribute(zb_manager_identify_cluster_t* cluster, uint16_t attr_id, void* value);


#endif