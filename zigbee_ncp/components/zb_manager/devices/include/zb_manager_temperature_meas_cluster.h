#ifndef ZB_MANAGER_TEMPERATURE_MEAS_CLUSTER_H

#define ZB_MANAGER_TEMPERATURE_MEAS_CLUSTER_H

#include <stdint.h>
#include "esp_err.h"
/**
 * @brief Structure representing the Zigbee Temperature Measurement Cluster
 * 
 * This structure holds the state for the Temperature Measurement cluster,
 * which is used to report and define the range of temperature readings from
 * a sensor. All temperature values are represented in degrees Celsius * 100.
 * 
 * Example: 25.5°C is represented as 2550, -10.0°C is represented as -1000.
 */
typedef struct {
    /**
     * @brief Measured Value
     * 
     * The most recent temperature measurement from the sensor.
     * 
     * - Data Type: `int16_t`
     * - Unit: Degrees Celsius * 100 (e.g., 25.5°C = 2550)
     * - Range: -273.15°C (-27315) to +327.67°C (+32767)
     * - Unknown Value: `0x8000` (Defined as ESP_ZB_ZCL_VALUE_S16_NONE)
     * - Default Value: Unknown (0x8000)
     */
    int16_t measured_value;

    /**
     * @brief Minimum Measured Value
     * 
     * The minimum temperature value that the sensor can measure.
     * 
     * - Data Type: `int16_t`
     * - Unit: Degrees Celsius * 100
     * - Range: -273.15°C (-27315) to +327.66°C (+32766)
     * - Unknown Value: `0x8000`
     * - Default Value: Unknown (0x8000)
     */
    int16_t min_measured_value;

    /**
     * @brief Maximum Measured Value
     * 
     * The maximum temperature value that the sensor can measure.
     * 
     * - Data Type: `int16_t`
     * - Unit: Degrees Celsius * 100
     * - Range: -273.14°C (-27314) to +327.67°C (+32767)
     * - Unknown Value: `0x8000`
     * - Default Value: Unknown (0x8000)
     */
    int16_t max_measured_value;

    /**
     * @brief Tolerance
     * 
     * The maximum variation in the measured value. The true temperature is
     * considered to be within `measured_value ± tolerance`.
     * 
     * - Data Type: `uint16_t`
     * - Unit: Degrees Celsius * 100 (e.g., ±0.5°C = 50)
     * - Range: 0 (exact) to 20.48°C (2048)
     * - Default Value: 0
     */
    uint16_t tolerance;

    /**
     * @brief Last Update Timestamp
     * 
     * A non-standard but highly useful field to store the time (in milliseconds)
     * when the `measured_value` was last updated. This helps in determining
     * if the data is stale.
     */
    uint32_t last_update_ms;

    /**
     * @brief Read Error Flag
     * 
     * A flag to indicate if the last attempt to read the physical sensor failed.
     * This can be used to set `measured_value` to `UNKNOWN` and inform the network.
     */
    bool read_error;

} zb_manager_temperature_measurement_cluster_t;

/**
 * @brief Default initialization macro for the Temperature Measurement Cluster structure
 * 
 * This macro initializes the structure with default "unknown" values as specified
 * in the Zigbee specification.
 */
#define ZIGBEE_TEMP_MEASUREMENT_CLUSTER_DEFAULT_INIT() { \
    .measured_value = 0x8000, \
    .min_measured_value = 0x8000, \
    .max_measured_value = 0x8000, \
    .tolerance = 0, \
    .last_update_ms = 0, \
    .read_error = false, \
}

/**
 * @brief Utility macros for temperature conversion
 * 
 * These macros simplify the conversion between integer representation and
 * floating-point degrees Celsius.
 */
#define TEMP_INT_TO_FLOAT(temp_int) ((float)(temp_int) / 100.0f)
#define TEMP_FLOAT_TO_INT(temp_float) ((int16_t)((temp_float) * 100.0f))

/**
 * @brief Function pointer for a sensor read callback
 * 
 * This callback is invoked when the cluster needs a fresh temperature reading,
 * such as when the `Read Attributes` command is received or during periodic updates.
 * 
 * @param user_data A pointer to user-defined context data (e.g., I2C bus handle, sensor address).
 * @return int16_t The new temperature reading in degrees Celsius * 100, or 0x8000 on error.
 */
typedef int16_t (*zigbee_temp_read_cb_t)(void *user_data);

/**
 * @brief Example of initializing and using the Temperature Measurement Cluster
 */
/*
zigbee_temperature_measurement_cluster_t temp_sensor = ZIGBEE_TEMP_MEASUREMENT_CLUSTER_DEFAULT_INIT();

// Example: Function to update the sensor value
esp_err_t update_temperature(zigbee_temp_read_cb_t read_cb, void *sensor_ctx) {
    if (!read_cb) {
        return ESP_ERR_INVALID_ARG;
    }

    int16_t new_reading = read_cb(sensor_ctx);
    uint32_t now = esp_log_timestamp();

    if (new_reading == 0x8000) {
        // Sensor read failed
        temp_sensor.read_error = true;
        temp_sensor.measured_value = 0x8000; // Set to "unknown"
    } else {
        // Update the value only if valid and different (to avoid unnecessary network traffic)
        if (temp_sensor.measured_value != new_reading) {
            temp_sensor.measured_value = new_reading;
            // Optionally, report this change to the network
            // esp_zb_zcl_report_attr_cmd_req(&report_cmd);
        }
        temp_sensor.read_error = false;
    }
    temp_sensor.last_update_ms = now;

    return ESP_OK;
}
*/
esp_err_t zb_manager_temp_meas_cluster_update_attribute(zb_manager_temperature_measurement_cluster_t* cluster, uint16_t attr_id, void* value);

#endif