#ifndef ZB_MANAGER_HUMIDITY_MEAS_CLUSTER_H
#define ZB_MANAGER_HUMIDITY_MEAS_CLUSTER_H

#include <stdint.h>
#include "esp_err.h"
#include <stdbool.h>
#include "zbm_clusters.h"
#include "cJSON.h"


typedef enum {
    ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID     = 0x0000,  /*!<  MeasuredValue */
    ATTR_REL_HUMIDITY_MEASUREMENT_MIN_VALUE_ID = 0x0001,  /*!<  MinMeasuredValue Attribute */
    ATTR_REL_HUMIDITY_MEASUREMENT_MAX_VALUE_ID = 0x0002,  /*!<  MaxMeasuredValue Attribute */
    ATTR_REL_HUMIDITY_TOLERANCE_ID             = 0x0003,  /*!<  The Tolerance attribute SHALL indicate the magnitude of the possible error that is associated with MeasuredValue, using the same units and resolution.*/
} zb_manager_zcl_rel_humidity_measurement_attr_t;

/**
 * @brief Structure representing the Zigbee Relative Humidity Measurement Cluster
 *
 * This structure holds the state for the Relative Humidity cluster,
 * which is used to report and define the range of humidity readings from
 * a sensor. All humidity values are represented in 0.01%.
 *
 * Example: 45.6% is represented as 4560.
 */
typedef struct {
    /**
     * @brief Measured Value
     *
     * The most recent humidity measurement from the sensor.
     *
     * - Data Type: `uint16_t`
     * - Unit: 0.01% (e.g., 45.6% = 4560)
     * - Range: 0% (0) to 100% (10000)
     * - Unknown Value: `0xFFFF` (ESP_ZB_ZCL_VALUE_U16_NONE)
     * - Default Value: Unknown (0xFFFF)
     */
    uint16_t measured_value;

    /**
     * @brief Minimum Measured Value
     *
     * The minimum humidity value that the sensor can measure.
     *
     * - Data Type: `uint16_t`
     * - Unit: 0.01%
     * - Range: 0 to 99.99%
     * - Unknown Value: `0xFFFF`
     * - Default Value: Unknown (0xFFFF)
     */
    uint16_t min_measured_value;

    /**
     * @brief Maximum Measured Value
     *
     * The maximum humidity value that the sensor can measure.
     *
     * - Data Type: `uint16_t`
     * - Unit: 0.01%
     * - Range: 0.01% to 100.00%
     * - Unknown Value: `0xFFFF`
     * - Default Value: Unknown (0xFFFF)
     */
    uint16_t max_measured_value;

    /**
     * @brief Tolerance
     *
     * The maximum variation in the measured value. The true humidity is
     * considered to be within `measured_value ± tolerance`.
     *
     * - Data Type: `uint16_t`
     * - Unit: 0.01% (e.g., ±2.0% = 200)
     * - Range: 0 (exact) to 5.12% (512)
     * - Default Value: 0
     */
    uint16_t tolerance;

    /**
     * @brief Last Update Timestamp
     *
     * Time in milliseconds when the `measured_value` was last updated.
     */
    uint32_t last_update_ms;

    /**
     * @brief Read Error Flag
     *
     * Indicates if the last sensor read failed.
     */
    bool read_error;

    uint16_t                    nostandart_attr_count;
    attribute_custom_t**        nostandart_attr_array;

} zb_manager_humidity_measurement_cluster_t;

/**
 * @brief Default initialization macro for the Humidity Measurement Cluster
 */
#define ZIGBEE_HUMIDITY_MEASUREMENT_CLUSTER_DEFAULT_INIT() { \
    .measured_value = 0xFFFF, \
    .min_measured_value = 0xFFFF, \
    .max_measured_value = 0xFFFF, \
    .tolerance = 0, \
    .last_update_ms = 0, \
    .read_error = false, \
    .nostandart_attr_count = 0, \
    .nostandart_attr_array = NULL, \
}

/**
 * @brief Utility macros for humidity conversion
 */
#define HUMIDITY_INT_TO_FLOAT(hum_int) ((float)(hum_int) / 100.0f)
#define HUMIDITY_FLOAT_TO_INT(hum_float) ((uint16_t)((hum_float) * 100.0f))

/**
 * @brief Callback for reading humidity from physical sensor
 *
 * @param user_data Pointer to sensor context (I2C handle, etc.)
 * @return uint16_t Humidity in 0.01%, or 0xFFFF on error.
 */
typedef uint16_t (*zigbee_humidity_read_cb_t)(void *user_data);

/**
 * @brief Update attribute in the Humidity Measurement Cluster
 *
 * @param cluster Pointer to the cluster structure
 * @param attr_id Attribute ID (e.g., 0x0000 for MeasuredValue)
 * @param value Pointer to the new attribute value
 * @return esp_err_t ESP_OK on success
 */
esp_err_t zb_manager_humidity_meas_cluster_update_attribute(zb_manager_humidity_measurement_cluster_t* cluster,uint16_t attr_id,uint8_t attr_type,void* value,uint16_t value_len);

const char* zb_manager_get_humidity_measurement_cluster_attr_name(uint16_t attrID);

esp_err_t zb_manager_humidity_meas_cluster_add_custom_attribute(zb_manager_humidity_measurement_cluster_t *cluster, uint16_t attr_id, uint8_t attr_type);

attribute_custom_t *zb_manager_humidity_meas_cluster_find_custom_attr_obj(zb_manager_humidity_measurement_cluster_t *cluster, uint16_t attr_id);

/**
 * @brief Convert Humidity Measurement Cluster object to cJSON
 * @param cluster Pointer to the cluster
 * @return cJSON* - new JSON object, or NULL
 */
cJSON* zbm_humidity_cluster_to_json(zb_manager_humidity_measurement_cluster_t *cluster);

/**
 * @brief Load Humidity Measurement Cluster from JSON
 * @param cluster Pointer to cluster struct (must be allocated or zeroed)
 * @param json_obj cJSON object
 * @return ESP_OK on success
 */
esp_err_t zbm_humidity_cluster_load_from_json(zb_manager_humidity_measurement_cluster_t *cluster, cJSON *json_obj);

#endif // ZB_MANAGER_HUMIDITY_MEAS_CLUSTER_H