// components/zb_manager/devices/zb_manager_power_config_cluster.h
#ifndef ZB_MANAGER_POWER_CONFIG_CLUSTER_H
#define ZB_MANAGER_POWER_CONFIG_CLUSTER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"


#ifdef __cplusplus
extern "C" {
#endif

/** @brief Power configuration cluster information attribute set identifiers
*/
typedef enum {
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_VOLTAGE_ID                 = 0x0000,         /*!< MainsVoltage attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_FREQUENCY_ID               = 0x0001,         /*!< MainsFrequency attribute */

    ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_ALARM_MASK_ID              = 0x0010,         /*！< MainsAlarmMask attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_VOLTAGE_MIN_THRESHOLD      = 0x0011,         /*！< MainsVoltageMinThreshold attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_VOLTAGE_MAX_THRESHOLD      = 0x0012,         /*！< MainsVoltageMaxThreshold attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_DWELL_TRIP_POINT           = 0x0013,         /*！< MainsVoltageDwellTripPoint attribute */

    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID               = 0x0020,         /*!< BatteryVoltage attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID  = 0x0021,         /*!< BatteryPercentageRemaining attribute */

    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_MANUFACTURER_ID          = 0x0030,         /*!< Name of the battery manufacturer as a character string. */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_SIZE_ID                  = 0x0031,         /*!< BatterySize attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_A_HR_RATING_ID           = 0x0032,         /*!< The Ampere-hour rating of the battery, measured in units of 10mAHr. */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_QUANTITY_ID              = 0x0033,         /*!< BatteryQuantity attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_RATED_VOLTAGE_ID         = 0x0034,         /*!< BatteryRatedVoltage attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_ALARM_MASK_ID            = 0x0035,         /*!< BatteryAlarmMask attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_MIN_THRESHOLD_ID = 0x0036,         /*!< BatteryVoltageMinThreshold attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD1_ID        = 0x0037,     /*!< BatteryVoltageThreshold1 attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD2_ID        = 0x0038,     /*!< BatteryVoltageThreshold2 attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD3_ID        = 0x0039,     /*!< BatteryVoltageThreshold3 attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_MIN_THRESHOLD_ID  = 0x003a,     /*!< BatteryPercentageMinThreshold attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD1_ID     = 0x003b,     /*!< BatteryPercentageThreshold1 attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD2_ID     = 0x003c,     /*!< BatteryPercentageThreshold2 attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD3_ID     = 0x003d,     /*!< BatteryPercentageThreshold3 attribute */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_ALARM_STATE_ID               = 0x003e,     /*!< BatteryAlarmState attribute */

    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_ID                    = 0x0040,   /*!< Battery Information 2 attribute set */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_REMAINING_ID       = 0x0041,

    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_MANUFACTURER_ID               = 0x0050,   /*!< Battery Settings 2 attribute set */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_SIZE_ID                       = 0x0051,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_A_HR_RATING_ID                = 0x0052,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_QUANTITY_ID                   = 0x0053,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_RATED_VOLTAGE_ID              = 0x0054,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_ALARM_MASK_ID                 = 0x0055,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_MIN_THRESHOLD_ID      = 0x0056,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_THRESHOLD1_ID         = 0x0057,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_THRESHOLD2_ID         = 0x0058,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_THRESHOLD3_ID         = 0x0059,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_MIN_THRESHOLD_ID   = 0x005a,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_THRESHOLD1_ID      = 0x005b,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_THRESHOLD2_ID      = 0x005c,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_THRESHOLD3_ID      = 0x005d,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_ALARM_STATE_ID                = 0x005e,

    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_ID                    = 0x0060,   /*!< Battery Information 3 attribute set */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_REMAINING_ID       = 0x0061,

    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_MANUFACTURER_ID               = 0x0070,   /*!< Battery Settings 3 attribute set */
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_SIZE_ID                       = 0x0071,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_A_HR_RATING_ID                = 0x0072,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_QUANTITY_ID                   = 0x0073,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_RATED_VOLTAGE_ID              = 0x0074,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_ALARM_MASK_ID                 = 0x0075,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_MIN_THRESHOLD_ID      = 0x0076,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_THRESHOLD1_ID         = 0x0077,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_THRESHOLD2_ID         = 0x0078,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_THRESHOLD3_ID         = 0x0079,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_MIN_THRESHOLD_ID   = 0x007a,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_THRESHOLD1_ID      = 0x007b,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_THRESHOLD2_ID      = 0x007c,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_THRESHOLD3_ID      = 0x007d,
    ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_ALARM_STATE_ID                = 0x007e,
} local_esp_zb_zcl_power_config_attr_t;

typedef struct {
    // Mains Power
    uint16_t mains_voltage;           // 0x0000, 0.1V units, e.g. 2300 = 230.0V
    uint8_t  mains_frequency;         // 0x0001, Hz

    uint8_t  mains_alarm_mask;        // 0x0010
    uint16_t mains_voltage_min_th;   // 0x0011
    uint16_t mains_voltage_max_th;   // 0x0012
    uint16_t mains_dwell_trip_point; // 0x0013

    // Battery 1
    uint8_t  battery_voltage;         // 0x0020, 0.1V units, e.g. 30 = 3.0V
    uint8_t  battery_percentage;      // 0x0021, 0.5%, so 100 = 50%, 200 = 100%

    // Battery Info 1
    char battery_manufacturer[33];    // 0x0030
    uint8_t battery_size;             // 0x0031 (enum)
    uint16_t battery_a_hr_rating;     // 0x0032, 10 mAh units
    uint8_t  battery_quantity;        // 0x0033
    uint8_t  battery_rated_voltage;   // 0x0034, 0.1V
    uint8_t  battery_alarm_mask;      // 0x0035
    uint8_t  battery_voltage_min_th;  // 0x0036
    uint8_t  battery_voltage_th1;     // 0x0037
    uint8_t  battery_voltage_th2;     // 0x0038
    uint8_t  battery_voltage_th3;     // 0x0039
    uint8_t  battery_percentage_min_th; // 0x003a
    uint8_t  battery_percentage_th1;    // 0x003b
    uint8_t  battery_percentage_th2;    // 0x003c
    uint8_t  battery_percentage_th3;    // 0x003d
    uint32_t battery_alarm_state;       // 0x003e, bit field

    // Battery 2 & 3 (опционально, можно пока не трогать)
    // Добавим в будущем при необходимости
    /**
     * @brief Last Update Timestamp
     *
     * Time in milliseconds when the `on_off` state was last updated.
     */
    uint32_t last_update_ms;
} zb_manager_power_config_cluster_t;

// Default init
#define ZIGBEE_POWER_CONFIG_CLUSTER_DEFAULT_INIT() { \
    .mains_voltage = 0, \
    .mains_frequency = 0, \
    .mains_alarm_mask = 0, \
    .mains_voltage_min_th = 0, \
    .mains_voltage_max_th = 0xFFFF, \
    .mains_dwell_trip_point = 0, \
    \
    .battery_voltage = 0, \
    .battery_percentage = 0, \
    \
    .battery_manufacturer = {0}, \
    .battery_size = 0xFF, \
    .battery_a_hr_rating = 0, \
    .battery_quantity = 1, \
    .battery_rated_voltage = 0, \
    .battery_alarm_mask = 0, \
    .battery_voltage_min_th = 0, \
    .battery_voltage_th1 = 0, \
    .battery_voltage_th2 = 0, \
    .battery_voltage_th3 = 0, \
    .battery_percentage_min_th = 0, \
    .battery_percentage_th1 = 0, \
    .battery_percentage_th2 = 0, \
    .battery_percentage_th3 = 0, \
    .battery_alarm_state = 0, \
    .last_update_ms = 0, \
}

// Функции
esp_err_t zb_manager_power_config_cluster_update_attribute(zb_manager_power_config_cluster_t* cluster, uint16_t attr_id, void* value);
const char* zb_manager_get_power_config_attr_name(uint16_t attr_id);
const char* get_battery_size_string(uint8_t size);
const char* get_battery_voltage_string(uint8_t voltage_units);  // 30 → "3.0V"
const char* get_battery_percentage_string(uint8_t percentage_units);  // 200 → "100%"

#ifdef __cplusplus
}
#endif

#endif // ZB_MANAGER_POWER_CONFIG_CLUSTER_H
