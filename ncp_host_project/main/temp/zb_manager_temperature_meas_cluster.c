/// @brief [zb_manager_temperature_meas_cluster.c] Модуль для работы с Zigbee Temperature Measurement Cluster (0x0402)
/// Содержит функции обновления атрибутов и получения имён атрибутов температурного датчика

#include "zb_manager_temperature_meas_cluster.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_zigbee_type.h"
#include "esp_zigbee_zcl_command.h"
#include "zbm_dev_base_utils.h"

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


/**
 * @brief Convert Temperature Measurement Cluster object to cJSON
 * @param cluster Pointer to the cluster
 * @return cJSON* - new JSON object, or NULL on failure
 */
cJSON* zbm_temperature_cluster_to_json(zb_manager_temperature_measurement_cluster_t *cluster)
{
    if (!cluster) {
        ESP_LOGW(TAG, "zbm_temperature_cluster_to_json: cluster is NULL");
        return NULL;
    }

    cJSON *temp = cJSON_CreateObject();
    if (!temp) {
        ESP_LOGE(TAG, "Failed to create temperature cluster JSON object");
        return NULL;
    }

    // === Standard Attributes ===
    cJSON_AddNumberToObject(temp, "cluster_id", 0x0402);

    // Measured Value
    if (cluster->measured_value == 0x8000) {
        cJSON_AddNullToObject(temp, "measured_value");
        cJSON_AddStringToObject(temp, "measured_value_str", "Unknown");
    } else {
        cJSON_AddNumberToObject(temp, "measured_value", cluster->measured_value);
        cJSON_AddNumberToObject(temp, "measured_value_c", TEMP_INT_TO_FLOAT(cluster->measured_value));
        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.2f°C", TEMP_INT_TO_FLOAT(cluster->measured_value));
        cJSON_AddStringToObject(temp, "measured_value_str", temp_str);
    }

    // Min Measured Value
    if (cluster->min_measured_value == 0x8000) {
        cJSON_AddNullToObject(temp, "min_measured_value");
        cJSON_AddStringToObject(temp, "min_measured_value_str", "Unknown");
    } else {
        cJSON_AddNumberToObject(temp, "min_measured_value", cluster->min_measured_value);
        cJSON_AddNumberToObject(temp, "min_measured_value_c", TEMP_INT_TO_FLOAT(cluster->min_measured_value));
        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.2f°C", TEMP_INT_TO_FLOAT(cluster->min_measured_value));
        cJSON_AddStringToObject(temp, "min_measured_value_str", temp_str);
    }

    // Max Measured Value
    if (cluster->max_measured_value == 0x8000) {
        cJSON_AddNullToObject(temp, "max_measured_value");
        cJSON_AddStringToObject(temp, "max_measured_value_str", "Unknown");
    } else {
        cJSON_AddNumberToObject(temp, "max_measured_value", cluster->max_measured_value);
        cJSON_AddNumberToObject(temp, "max_measured_value_c", TEMP_INT_TO_FLOAT(cluster->max_measured_value));
        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.2f°C", TEMP_INT_TO_FLOAT(cluster->max_measured_value));
        cJSON_AddStringToObject(temp, "max_measured_value_str", temp_str);
    }

    // Tolerance
    cJSON_AddNumberToObject(temp, "tolerance", cluster->tolerance);
    cJSON_AddNumberToObject(temp, "tolerance_c", TEMP_INT_TO_FLOAT(cluster->tolerance));
    char tol_str[16];
    snprintf(tol_str, sizeof(tol_str), "%.2f°C", TEMP_INT_TO_FLOAT(cluster->tolerance));
    cJSON_AddStringToObject(temp, "tolerance_str", tol_str);

    // Last update
    cJSON_AddNumberToObject(temp, "last_update_ms", cluster->last_update_ms);

    // Read error flag
    cJSON_AddBoolToObject(temp, "read_error", cluster->read_error);

    // Optional: human-readable state
    if (cluster->read_error) {
        cJSON_AddStringToObject(temp, "state", "Error");
    } else if (cluster->measured_value == 0x8000) {
        cJSON_AddStringToObject(temp, "state", "Unknown");
    } else {
        cJSON_AddStringToObject(temp, "state", "OK");
    }

    // === Custom Attributes (nostandart_attributes) ===
    if (cluster->nostandart_attr_count > 0 && cluster->nostandart_attr_array) {
        cJSON *attrs = cJSON_CreateArray();
        if (attrs) {
            for (int i = 0; i < cluster->nostandart_attr_count; i++) {
                attribute_custom_t *attr = cluster->nostandart_attr_array[i];
                if (!attr) continue;

                cJSON *attr_obj = cJSON_CreateObject();
                if (!attr_obj) continue;

                cJSON_AddNumberToObject(attr_obj, "id", attr->id);
                cJSON_AddStringToObject(attr_obj, "attr_id_text", attr->attr_id_text);
                cJSON_AddNumberToObject(attr_obj, "type", attr->type);
                cJSON_AddNumberToObject(attr_obj, "acces", attr->acces);
                cJSON_AddNumberToObject(attr_obj, "size", attr->size);
                cJSON_AddNumberToObject(attr_obj, "is_void_pointer", attr->is_void_pointer);
                cJSON_AddNumberToObject(attr_obj, "manuf_code", attr->manuf_code);
                cJSON_AddNumberToObject(attr_obj, "parent_cluster_id", attr->parent_cluster_id);

                // Добавляем значение атрибута
                zbm_json_add_attribute_value(attr_obj, attr);

                cJSON_AddItemToArray(attrs, attr_obj);
            }
            cJSON_AddItemToObject(temp, "nostandart_attributes", attrs);
        }
    }

    return temp;
}

/**
 * @brief Load Temperature Measurement Cluster from cJSON object
 * @param cluster Pointer to allocated or zeroed zb_manager_temperature_measurement_cluster_t
 * @param json_obj cJSON object representing the cluster
 * @return ESP_OK on success
 */
esp_err_t zbm_temperature_cluster_load_from_json(zb_manager_temperature_measurement_cluster_t *cluster, cJSON *json_obj)
{
    if (!cluster || !json_obj) {
        return ESP_ERR_INVALID_ARG;
    }

    // Инициализируем структуру
    *cluster = (zb_manager_temperature_measurement_cluster_t)ZIGBEE_TEMP_MEASUREMENT_CLUSTER_DEFAULT_INIT();

    // === Standard Attributes ===
    cJSON *measured_value_item = cJSON_GetObjectItem(json_obj, "measured_value");
    if (measured_value_item) {
        if (cJSON_IsNumber(measured_value_item)) {
            cluster->measured_value = (int16_t)measured_value_item->valueint;
        } else if (cJSON_IsNull(measured_value_item)) {
            cluster->measured_value = 0x8000; // Unknown
        }
    }

    cJSON *min_measured_value_item = cJSON_GetObjectItem(json_obj, "min_measured_value");
    if (min_measured_value_item) {
        if (cJSON_IsNumber(min_measured_value_item)) {
            cluster->min_measured_value = (int16_t)min_measured_value_item->valueint;
        } else if (cJSON_IsNull(min_measured_value_item)) {
            cluster->min_measured_value = 0x8000;
        }
    }

    cJSON *max_measured_value_item = cJSON_GetObjectItem(json_obj, "max_measured_value");
    if (max_measured_value_item) {
        if (cJSON_IsNumber(max_measured_value_item)) {
            cluster->max_measured_value = (int16_t)max_measured_value_item->valueint;
        } else if (cJSON_IsNull(max_measured_value_item)) {
            cluster->max_measured_value = 0x8000;
        }
    }

    LOAD_NUMBER(json_obj, "tolerance", cluster->tolerance);

    // === Last Update & Error State ===
    cJSON *last_update_item = cJSON_GetObjectItem(json_obj, "last_update_ms");
    if (last_update_item && cJSON_IsNumber(last_update_item)) {
        cluster->last_update_ms = last_update_item->valueint;
    } else {
        cluster->last_update_ms = esp_log_timestamp();
    }

    cJSON *read_error_item = cJSON_GetObjectItem(json_obj, "read_error");
    if (read_error_item) {
        cluster->read_error = cJSON_IsTrue(read_error_item);
    }

    // === Custom Attributes (nostandart_attributes) ===
    cJSON *attrs_json = cJSON_GetObjectItem(json_obj, "nostandart_attributes");
    if (attrs_json && cJSON_IsArray(attrs_json)) {
        int count = cJSON_GetArraySize(attrs_json);
        cluster->nostandart_attr_count = count;
        cluster->nostandart_attr_array = calloc(count, sizeof(attribute_custom_t*));
        if (!cluster->nostandart_attr_array) {
            return ESP_ERR_NO_MEM;
        }

        bool alloc_failed = false;
        for (int i = 0; i < count; i++) {
            cJSON *attr_json = cJSON_GetArrayItem(attrs_json, i);
            if (!attr_json) continue;

            attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
            if (!attr) {
                alloc_failed = true;
                continue;
            }

            attr->id = cJSON_GetObjectItem(attr_json, "id")->valueint;
            LOAD_STRING(attr_json, "attr_id_text", attr->attr_id_text);
            attr->type = cJSON_GetObjectItem(attr_json, "type")->valueint;
            attr->acces = cJSON_GetObjectItem(attr_json, "acces")->valueint;
            attr->size = cJSON_GetObjectItem(attr_json, "size")->valueint;
            attr->is_void_pointer = cJSON_GetObjectItem(attr_json, "is_void_pointer")->valueint;
            attr->manuf_code = cJSON_GetObjectItem(attr_json, "manuf_code")->valueint;
            attr->parent_cluster_id = cJSON_GetObjectItem(attr_json, "parent_cluster_id")->valueint;

            attr->p_value = NULL;
            zbm_json_load_attribute_value(attr, attr_json);

            cluster->nostandart_attr_array[i] = attr;
        }
        if (alloc_failed) {
            ESP_LOGW(TAG, "Partial failure loading custom attributes for Temperature cluster");
        }
    }

    return ESP_OK;
}