#ifndef ZB_MANAGER_TUYA_DP_H
#define ZB_MANAGER_TUYA_DP_H
#include <stdint.h>
#include <stdbool.h>
#include "zb_manager_devices.h"
/**
 * @brief Структура для Tuya DP события
 */
typedef struct {
    uint16_t short_addr;
    uint8_t  endpoint;
    uint8_t  dp_id;
    uint8_t  dp_type;
    uint32_t dp_value;
    int8_t   rssi;
    uint8_t  lqi;
} zb_manager_tuya_dp_report_t;

#define TUYA_DP_TO_TEMP_ZCL(tuya_temp)    ((int16_t)((tuya_temp) * 10))
#define TUYA_DP_TO_HUMI_ZCL(tuya_humi)    ((uint16_t)((tuya_humi) * 10))
#define TUYA_DP_TO_BATT_ZCL(tuya_batt)    ((uint8_t)((tuya_batt) * 2))

//void handle_tuya_temperature(device_custom_t *dev_info, uint32_t temp_x10);

//void handle_tuya_humidity(device_custom_t *dev_info, uint32_t hum_x10);

//void handle_tuya_battery(device_custom_t *dev_info, uint32_t battery_percent);

//void zb_manager_update_from_tuya_dp(device_custom_t *dev, uint8_t dp_id, uint32_t dp_value);

#endif // ZB_MANAGER_TUYA_DP_H