/// @brief [zb_manager_temperature_meas_cluster.c] Модуль для работы с Zigbee Temperature Measurement Cluster (0x0402)
/// Содержит функции обновления атрибутов и получения имён атрибутов температурного датчика

#include "zb_manager_temperature_meas_cluster.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"

/// @brief [zb_manager_temperature_meas_cluster.c] Обновляет значение атрибута в Temperature Measurement-кластере Zigbee
/// @param cluster Указатель на структуру кластера
/// @param attr_id Идентификатор атрибута (например, 0x0000 — Measured Value)
/// @param value Указатель на значение атрибута
/// @return ESP_OK при успехе, иначе ошибка
esp_err_t zb_manager_temp_meas_cluster_update_attribute(zb_manager_temperature_measurement_cluster_t* cluster, uint16_t attr_id, void* value)
{
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    cluster->last_update_ms = esp_log_timestamp(); //
    switch (attr_id)
    {
    case 0x0000: //ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID
        cluster->measured_value = *(int16_t*)value;
        break;
    case 0x0001: //ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID
        cluster->min_measured_value = *(int16_t*)value;
        break;
    case 0x0002: //ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID
        cluster->max_measured_value = *(int16_t*)value;
        break;
    case 0x0003: //ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID
        cluster->tolerance = *(uint16_t*)value;
        break;
    default:
        break;
    }
    return ESP_OK;
}

/// @brief [zb_manager_temperature_meas_cluster.c] Возвращает текстовое имя атрибута Temperature Measurement-кластера по его ID
/// @param attrID Идентификатор атрибута
/// @return Название атрибута или "Unknown Attribute"
const char* zb_manager_get_temperature_measurement_cluster_attr_name(uint16_t attrID)
{
    switch (attrID) {
        case 0x0000: //ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID
            return "Measured Value";
        case 0x0001: //ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID
            return "Min Measured Value";
        case 0x0002: //ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID
            return "Max Measured Value";
        case 0x0003: //ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID
            return "Tolerance";
        default:
            return "Unknown Attribute";
    }
}
