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
esp_err_t zb_manager_temp_meas_cluster_update_attribute(zb_manager_temperature_measurement_cluster_t* cluster,uint16_t attr_id, uint8_t attr_type,void* value,uint16_t value_len)
{
    if (value == NULL) {
        ESP_LOGW(TAG, "attr_id=0x%04x: value is NULL", attr_id);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Updating Temp attr 0x%04x", attr_id);
    cluster->last_update_ms = esp_log_timestamp();

    switch (attr_id)
    {
        case ATTR_TEMP_MEASUREMENT_VALUE_ID:
            cluster->measured_value = *(int16_t*)value;
            cluster->read_error = false;
            break;

        case ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID:
            cluster->min_measured_value = *(int16_t*)value;
            break;

        case ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID:
            cluster->max_measured_value = *(int16_t*)value;
            break;

        case ATTR_TEMP_MEASUREMENT_TOLERANCE_ID:
            cluster->tolerance = *(uint16_t*)value;
            break;

        default: {
            attribute_custom_t *custom_attr = zb_manager_temp_meas_cluster_find_custom_attr_obj(cluster, attr_id);
            if (custom_attr) {
                if (custom_attr->p_value == NULL || custom_attr->size != value_len) {
                    if (custom_attr->p_value) free(custom_attr->p_value);
                    custom_attr->p_value = malloc(value_len);
                    if (!custom_attr->p_value) return ESP_ERR_NO_MEM;
                    custom_attr->size = value_len;
                }
                memcpy(custom_attr->p_value, value, value_len);
                //custom_attr->last_update_ms = esp_log_timestamp();

                ESP_LOGI(TAG, ".updated Temp attr 0x%04x (type=0x%02x, len=%u)", attr_id, attr_type, value_len);
                return ESP_OK;
            } else {
                ESP_LOGI(TAG, "Auto-create Temp attr 0x%04x (type=0x%02x)", attr_id, attr_type);
                esp_err_t err = zb_manager_temp_meas_cluster_add_custom_attribute(cluster, attr_id, attr_type);
                if (err != ESP_OK) return err;

                custom_attr = zb_manager_temp_meas_cluster_find_custom_attr_obj(cluster, attr_id);
                if (custom_attr) {
                    custom_attr->p_value = malloc(value_len);
                    if (!custom_attr->p_value) return ESP_ERR_NO_MEM;
                    memcpy(custom_attr->p_value, value, value_len);
                    custom_attr->size = value_len;
                    //custom_attr->last_update_ms = esp_log_timestamp();
                    return ESP_OK;
                }
                return ESP_ERR_NOT_FOUND;
            }
        }
    }

    if (attr_id == ATTR_TEMP_MEASUREMENT_VALUE_ID) {
        cluster->last_update_ms = esp_log_timestamp();
        cluster->read_error = false;
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

esp_err_t zb_manager_temp_meas_cluster_add_custom_attribute(
    zb_manager_temperature_measurement_cluster_t *cluster,
    uint16_t attr_id,
    uint8_t attr_type)
{
    if (!cluster) {
        ESP_LOGE(TAG, "Cluster is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Список стандартных атрибутов Temperature Measurement-кластера
    bool is_standard = false;
    switch (attr_id) {
        case ATTR_TEMP_MEASUREMENT_VALUE_ID:         // 0x0000 — MeasuredValue
        case ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID:     // 0x0001 — MinMeasuredValue
        case ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID:     // 0x0002 — MaxMeasuredValue
        case ATTR_TEMP_MEASUREMENT_TOLERANCE_ID:     // 0x0003 — Tolerance
            is_standard = true;
            break;
        default:
            is_standard = false;
            break;
    }

    if (is_standard) {
        ESP_LOGD(TAG, "Attr 0x%04x is standard — skipping", attr_id);
        return ESP_ERR_NOT_SUPPORTED; // можно заменить на ESP_OK, если нужно молча игнорировать
    }

    // Проверка: уже есть такой атрибут?
    for (int i = 0; i < cluster->nostandart_attr_count; i++) {
        if (cluster->nostandart_attr_array[i] && cluster->nostandart_attr_array[i]->id == attr_id) {
            ESP_LOGD(TAG, "Attr 0x%04x already exists", attr_id);
            return ESP_OK;
        }
    }

    // Создаём новый атрибут
    attribute_custom_t *new_attr = calloc(1, sizeof(attribute_custom_t));
    if (!new_attr) {
        ESP_LOGE(TAG, "Failed to allocate memory for attribute 0x%04x", attr_id);
        return ESP_ERR_NO_MEM;
    }

    new_attr->id = attr_id;
    new_attr->type = attr_type;
    new_attr->parent_cluster_id = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT; // 0x0402
    new_attr->size = zb_manager_get_zcl_attr_size(attr_type);
    new_attr->is_void_pointer = (attr_type >= 0x41 && attr_type <= 0x51); // строки, массивы и т.п.
    new_attr->acces = 0; // может быть установлен позже
    new_attr->manuf_code = 0; // по умолчанию — нет manufacturer code
    new_attr->p_value = NULL; // значение не выделяем здесь

    // Формируем текстовое имя атрибута
    snprintf(new_attr->attr_id_text, sizeof(new_attr->attr_id_text), "Custom_0x%04X", attr_id);

    // Реаллокируем массив атрибутов
    void *new_array = realloc(cluster->nostandart_attr_array,
                              (cluster->nostandart_attr_count + 1) * sizeof(attribute_custom_t*));
    if (!new_array) {
        free(new_attr);
        ESP_LOGE(TAG, "Failed to realloc nostandart_attr_array");
        return ESP_ERR_NO_MEM;
    }

    cluster->nostandart_attr_array = (attribute_custom_t**)new_array;
    cluster->nostandart_attr_array[cluster->nostandart_attr_count] = new_attr;
    cluster->nostandart_attr_count++;

    ESP_LOGI(TAG, "Added custom attribute: 0x%04x (type: 0x%02x)", attr_id, attr_type);
    return ESP_OK;
}

attribute_custom_t *zb_manager_temp_meas_cluster_find_custom_attr_obj(zb_manager_temperature_measurement_cluster_t *cluster, uint16_t attr_id)
{
    if (!cluster) {
        return NULL;
    }

    for (int i = 0; i < cluster->nostandart_attr_count; i++) {
        attribute_custom_t *attr = cluster->nostandart_attr_array[i];
        if (attr && attr->id == attr_id) {
            return attr;
        }
    }

    return NULL;
}

