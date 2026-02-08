/// @brief [zb_manager_temperature_meas_cluster.c] Модуль для работы с Zigbee Temperature Measurement Cluster (0x0402)
/// Содержит функции обновления атрибутов и получения имён атрибутов температурного датчика

#include "zb_manager_temperature_meas_cluster.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_zigbee_type.h"
#include "esp_zigbee_zcl_command.h"

static const char* TAG = "zb_manager_temperature_meas_cluster";
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

esp_err_t zb_manager_configure_reporting_temperature_ext(uint16_t short_addr, uint8_t endpoint,
                                                   uint16_t min_interval, uint16_t max_interval, uint16_t change)
{
    uint8_t delta_buf[2];
    int16_t delta = 10; // 0.1°C
    memcpy(delta_buf, &delta, 2);

    esp_zb_zcl_config_report_record_t record = {
        .direction = ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
        .attributeID = ATTR_TEMP_MEASUREMENT_VALUE_ID, // MinMeasuredValue (workaround для Tuya)
        .attrType = ESP_ZB_ZCL_ATTR_TYPE_S16,
        .min_interval = min_interval,
        .max_interval = max_interval,
        .reportable_change = delta_buf,
        //.timeout = 0,  Configurations to report receiver. This is presented when the direction is ESP_ZB_ZCL_REPORT_DIRECTION_RECV,
                        //when the sender is configuring the receiver to receive to attributes report.
    };

    esp_zb_zcl_config_report_cmd_t cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = short_addr,
            .dst_endpoint = endpoint,
            .src_endpoint = 1,
            
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .dis_default_resp = 1,
        .manuf_code = 0,
        .record_number = 1,
        .record_field = &record,
    };

    esp_err_t err = zb_manager_reporting_config_req(&cmd);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "🌡️ Reporting configured for Temperature (0x%04x)", short_addr);
    } else {
        ESP_LOGW(TAG, "🌡️ Failed to configure temperature reporting for 0x%04x", short_addr);
    }

    return err;
}

