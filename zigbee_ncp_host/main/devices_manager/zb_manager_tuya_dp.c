#include "zb_manager_tuya_dp.h"
#include "zb_manager_clusters.h"

/*
void handle_tuya_temperature(device_custom_t *dev_info, uint32_t temp_x10)
{
    // Tuya: 250 = 25.0°C → ZCL: 2500 (0.01°C)
    // Если отрицательная: 65536 - 50 = -50 → 65486
    int32_t temp_c_x10 = (int32_t)temp_x10;
    if (temp_c_x10 > 32767) {
        temp_c_x10 = temp_c_x10 - 65536;
    }

    int16_t zcl_value = (int16_t)(temp_c_x10 * 10); // 0.1°C → 0.01°C

    if (dev_info->server_TemperatureMeasurementClusterObj) {
        zb_manager_temp_meas_cluster_update_attribute(
            dev_info->server_TemperatureMeasurementClusterObj,
            ATTR_TEMP_MEASUREMENT_VALUE_ID,
            &zcl_value
        );
        ESP_LOGI(TAG, "🌡️ Temp updated: %.1f °C", temp_c_x10 / 10.0f);
    } else {
        ESP_LOGI(TAG, "🌡️ Virtual Temp cluster created");
        dev_info->server_TemperatureMeasurementClusterObj = calloc(1, sizeof(zb_manager_temp_meas_cluster_t));
        if (dev_info->server_TemperatureMeasurementClusterObj) {
            zb_manager_temp_meas_cluster_t cl = ZIGBEE_TEMP_MEAS_CLUSTER_DEFAULT_INIT();
            memcpy(dev_info->server_TemperatureMeasurementClusterObj, &cl, sizeof(cl));
            zb_manager_temp_meas_cluster_update_attribute(
                dev_info->server_TemperatureMeasurementClusterObj,
                ATTR_TEMP_MEASUREMENT_VALUE_ID,
                &zcl_value
            );
        }
    }
}

void handle_tuya_humidity(device_custom_t *dev_info, uint32_t hum_x10)
{
    if (hum_x10 > 1000) hum_x10 = 1000; // max 100.0%
    uint16_t zcl_value = (uint16_t)(hum_x10 * 10); // 0.1% → 0.01%

    if (dev_info->server_RelativeHumidityClusterObj) {
        zb_manager_relative_humidity_cluster_update_attribute(
            dev_info->server_RelativeHumidityClusterObj,
            ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
            &zcl_value
        );
        ESP_LOGI(TAG, "💧 Humidity updated: %.1f %%", hum_x10 / 10.0f);
    } else {
        ESP_LOGI(TAG, "💧 Virtual Humidity cluster created");
        dev_info->server_RelativeHumidityClusterObj = calloc(1, sizeof(zb_manager_rel_humidity_cluster_t));
        if (dev_info->server_RelativeHumidityClusterObj) {
            zb_manager_rel_humidity_cluster_t cl = ZIGBEE_REL_HUMIDITY_CLUSTER_DEFAULT_INIT();
            memcpy(dev_info->server_RelativeHumidityClusterObj, &cl, sizeof(cl));
            zb_manager_relative_humidity_cluster_update_attribute(
                dev_info->server_RelativeHumidityClusterObj,
                ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
                &zcl_value
            );
        }
    }
}

void handle_tuya_battery(device_custom_t *dev_info, uint32_t battery_percent)

{
    if (battery_percent > 100) battery_percent = 100;
    uint8_t battery_units = (uint8_t)(battery_percent * 2); // 0.5%

    if (dev_info->server_PowerConfigurationClusterObj) {
        zb_manager_power_config_cluster_update_attribute(
            dev_info->server_PowerConfigurationClusterObj,
            ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
            &battery_units
        );
        ESP_LOGI(TAG, "🔋 Battery updated: %" PRIu32 "%%", battery_percent);
    } else {
        ESP_LOGI(TAG, "🔋 Virtual Power Config cluster created");
        dev_info->server_PowerConfigurationClusterObj = calloc(1, sizeof(zb_manager_power_config_cluster_t));
        if (dev_info->server_PowerConfigurationClusterObj) {
            zb_manager_power_config_cluster_t cl = ZIGBEE_POWER_CONFIG_CLUSTER_DEFAULT_INIT();
            memcpy(dev_info->server_PowerConfigurationClusterObj, &cl, sizeof(cl));
            zb_manager_power_config_cluster_update_attribute(
                dev_info->server_PowerConfigurationClusterObj,
                ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
                &battery_units
            );
        }
    }
}
    */