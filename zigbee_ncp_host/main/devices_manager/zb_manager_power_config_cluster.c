// components/zb_manager/devices/zb_manager_power_config_cluster.c
/// @brief [zb_manager_power_config_cluster.c] Модуль для работы с Zigbee Power Configuration Cluster (0x0001)
/// Содержит функции обновления атрибутов, связанных с питанием устройства (батарея, напряжение и т.д.)
#include "zb_manager_power_config_cluster.h"
#include "esp_err.h"
#include "string.h"
#include "stdio.h"
#include "esp_log.h"

/// @brief [zb_manager_power_config_cluster.c] Обновляет значение атрибута в Power Config-кластере Zigbee
/// @param cluster Указатель на структуру кластера
/// @param attr_id Идентификатор атрибута (например, 0x0020 — Battery Voltage)
/// @param value Указатель на значение атрибута
/// @return ESP_OK при успехе, иначе ошибка
esp_err_t zb_manager_power_config_cluster_update_attribute(zb_manager_power_config_cluster_t* cluster, uint16_t attr_id, void* value)
{
    uint8_t *data = NULL;
    uint8_t len = 0;
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    cluster->last_update_ms = esp_log_timestamp(); //

    switch (attr_id)
    {
    case 0x0000: // ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_VOLTAGE_ID
        cluster->mains_voltage = *(uint16_t*)value;
        cluster->last_update_ms = esp_log_timestamp();
        break;
    case 0x0001: // ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_FREQUENCY_ID
        cluster->mains_frequency = *(uint8_t*)value;
        cluster->last_update_ms = esp_log_timestamp();
        break;

    case 0x0010:
        cluster->mains_alarm_mask = *(uint8_t*)value;
        break;
    case 0x0011:
        cluster->mains_voltage_min_th = *(uint16_t*)value;
        break;
    case 0x0012:
        cluster->mains_voltage_max_th = *(uint16_t*)value;
        break;
    case 0x0013:
        cluster->mains_dwell_trip_point = *(uint16_t*)value;
        break;

    case 0x0020: // BatteryVoltage
        cluster->battery_voltage = *(uint8_t*)value;
        
        break;
    case 0x0021: // BatteryPercentageRemaining
        cluster->battery_percentage = *(uint8_t*)value;
        
        break;

    case 0x0030: // BatteryManufacturer
        memset(cluster->battery_manufacturer, 0, sizeof(cluster->battery_manufacturer));
        data = (uint8_t*)value;
        len = data[0];
        if (len > 0 && len < sizeof(cluster->battery_manufacturer)) {
            memcpy(cluster->battery_manufacturer, data + 1, len);
            cluster->battery_manufacturer[len] = '\0';
        }
        break;

    case 0x0031: // BatterySize
        cluster->battery_size = *(uint8_t*)value;
        break;
    case 0x0032: // BatteryA-HrRating
        cluster->battery_a_hr_rating = *(uint16_t*)value;
        break;
    case 0x0033: // BatteryQuantity
        cluster->battery_quantity = *(uint8_t*)value;
        break;
    case 0x0034: // BatteryRatedVoltage
        cluster->battery_rated_voltage = *(uint8_t*)value;
        break;
    case 0x0035: // BatteryAlarmMask
        cluster->battery_alarm_mask = *(uint8_t*)value;
        break;
    case 0x0036: // BatteryVoltageMinThreshold
        cluster->battery_voltage_min_th = *(uint8_t*)value;
        break;
    case 0x0037: // Threshold 1
        cluster->battery_voltage_th1 = *(uint8_t*)value;
        break;
    case 0x0038:
        cluster->battery_voltage_th2 = *(uint8_t*)value;
        break;
    case 0x0039:
        cluster->battery_voltage_th3 = *(uint8_t*)value;
        break;
    case 0x003a:
        cluster->battery_percentage_min_th = *(uint8_t*)value;
        break;
    case 0x003b:
        cluster->battery_percentage_th1 = *(uint8_t*)value;
        break;
    case 0x003c:
        cluster->battery_percentage_th2 = *(uint8_t*)value;
        break;
    case 0x003d:
        cluster->battery_percentage_th3 = *(uint8_t*)value;
        break;
    case 0x003e: // BatteryAlarmState
        cluster->battery_alarm_state = *(uint32_t*)value;
        break;

    default:
        ESP_LOGW("Power_Config_Cluster_module", "PowerConfig: unknown attr 0x%04x", attr_id);
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

/// @brief [zb_manager_power_config_cluster.c] Возвращает текстовое имя атрибута Power Config-кластера по его ID
/// @param attrID Идентификатор атрибута
/// @return Название атрибута или "Unknown Attribute"
const char* zb_manager_get_power_config_attr_name(uint16_t attrID)
{
    switch (attrID) {
        case 0x0000: return "Mains Voltage (0.1V)";
        case 0x0001: return "Mains Frequency (Hz)";
        case 0x0010: return "Mains Alarm Mask";
        case 0x0011: return "Mains Voltage Min Threshold";
        case 0x0012: return "Mains Voltage Max Threshold";
        case 0x0013: return "Mains Dwell Trip Point";
        case 0x0020: return "Battery Voltage (0.1V)";
        case 0x0021: return "Battery Percentage Remaining (0.5%)";
        case 0x0030: return "Battery Manufacturer";
        case 0x0031: return "Battery Size";
        case 0x0032: return "Battery A-Hr Rating (10mAh)";
        case 0x0033: return "Battery Quantity";
        case 0x0034: return "Battery Rated Voltage (0.1V)";
        case 0x0035: return "Battery Alarm Mask";
        case 0x0036: return "Battery Voltage Min Threshold";
        case 0x0037: return "Battery Voltage Threshold 1";
        case 0x0038: return "Battery Voltage Threshold 2";
        case 0x0039: return "Battery Voltage Threshold 3";
        case 0x003a: return "Battery Percentage Min Threshold";
        case 0x003b: return "Battery Percentage Threshold 1";
        case 0x003c: return "Battery Percentage Threshold 2";
        case 0x003d: return "Battery Percentage Threshold 3";
        case 0x003e: return "Battery Alarm State";
        default: return "Unknown Power Config Attr";
    }
}

const char* get_battery_size_string(uint8_t size)
{
    switch (size)
    {
        case 0:  return "No Battery";
        case 1:  return "Built-in";
        case 2:  return "Other";
        case 3:  return "AA";
        case 4:  return "AAA";
        case 5:  return "C";
        case 6:  return "D";
        case 7:  return "CR2";
        case 8:  return "CR123A";
        case 255:return "Unknown";
        default: return "Invalid";
    }
}

const char* get_battery_voltage_string(uint8_t voltage_units)
{
    static char buf[16];
    switch (voltage_units) {
        case 0x00:
            return "0V";
        case 0xFF:
            return "Unknown";
        default:
            float volts = voltage_units * 0.1f;
            snprintf(buf, sizeof(buf), "%.1fV", volts);
            return buf;
    }
}

const char* get_battery_percentage_string(uint8_t percentage_units)
{
    static char buf[16];
    switch (percentage_units) {
        case 0xFF:
            return "Unknown";
        case 0x00:
            return "0%";
        default:
            // Ограничиваем максимум 200 (100%)
            if (percentage_units > 200) {
                percentage_units = 200;
            }
            float perc = percentage_units * 0.5f;
            snprintf(buf, sizeof(buf), "%.1f%%", perc);
            return buf;
    }
}
