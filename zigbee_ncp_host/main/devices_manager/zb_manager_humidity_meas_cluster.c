/// @brief [zb_manager_humidity_meas_cluster.c] Модуль для работы с Zigbee Relative Humidity Measurement Cluster (0x0405)
/// Содержит функции обновления атрибутов и получения имён атрибутов датчика влажности

#include "zb_manager_humidity_meas_cluster.h"
#include "esp_zigbee_type.h"
#include "esp_zigbee_zcl_command.h"
#include "esp_log.h"
static const char *TAG = "HUMIDITY_MEAS_CL";

/// @brief [zb_manager_humidity_meas_cluster.c] Обновляет значение атрибута в Humidity Measurement-кластере Zigbee
/// @param cluster Указатель на структуру кластера
/// @param attr_id Идентификатор атрибута (например, 0x0000 — Measured Value)
/// @param value Указатель на значение атрибута
/// @return ESP_OK при успехе, иначе ошибка
esp_err_t zb_manager_humidity_meas_cluster_update_attribute(zb_manager_humidity_measurement_cluster_t* cluster,uint16_t attr_id,uint8_t attr_type,void* value,uint16_t value_len)
{
    if (value == NULL) {
        ESP_LOGW(TAG, "attr_id=0x%04x: value is NULL", attr_id);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Updating Humidity attr 0x%04x", attr_id);
    cluster->last_update_ms = esp_log_timestamp();

    switch (attr_id)
    {
        case ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID:
            cluster->measured_value = *(uint16_t*)value;
            cluster->read_error = false;
            break;

        case ATTR_REL_HUMIDITY_MEASUREMENT_MIN_VALUE_ID:
            cluster->min_measured_value = *(uint16_t*)value;
            break;

        case ATTR_REL_HUMIDITY_MEASUREMENT_MAX_VALUE_ID:
            cluster->max_measured_value = *(uint16_t*)value;
            break;

        case ATTR_REL_HUMIDITY_TOLERANCE_ID:
            cluster->tolerance = *(uint16_t*)value;
            break;

        default: {
            attribute_custom_t *custom_attr = zb_manager_humidity_meas_cluster_find_custom_attr_obj(cluster, attr_id);
            if (custom_attr) {
                if (custom_attr->p_value == NULL || custom_attr->size != value_len) {
                    if (custom_attr->p_value) free(custom_attr->p_value);
                    custom_attr->p_value = malloc(value_len);
                    if (!custom_attr->p_value) return ESP_ERR_NO_MEM;
                    custom_attr->size = value_len;
                }
                memcpy(custom_attr->p_value, value, value_len);
                //custom_attr->last_update_ms = esp_log_timestamp();

                ESP_LOGI(TAG, ".updated Humidity attr 0x%04x (type=0x%02x, len=%u)", attr_id, attr_type, value_len);
                return ESP_OK;
            } else {
                ESP_LOGI(TAG, "Auto-create Humidity attr 0x%04x (type=0x%02x)", attr_id, attr_type);
                esp_err_t err = zb_manager_humidity_meas_cluster_add_custom_attribute(cluster, attr_id, attr_type);
                if (err != ESP_OK) return err;

                custom_attr = zb_manager_humidity_meas_cluster_find_custom_attr_obj(cluster, attr_id);
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

    if (attr_id == ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID) {
        cluster->last_update_ms = esp_log_timestamp();
        cluster->read_error = false;
    }

    return ESP_OK;
}

/// @brief [zb_manager_humidity_meas_cluster.c] Возвращает текстовое имя атрибута Humidity Measurement-кластера по его ID
/// @param attrID Идентификатор атрибута
/// @return Название атрибута или "Unknown Attribute"
const char* zb_manager_get_humidity_measurement_cluster_attr_name(uint16_t attrID)
{
    switch (attrID) {
        case 0x0000: //ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID:
            return "Measured Value";
        case 0x0001: //ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MIN_VALUE_ID:
            return "Min Measured Value";
        case 0x0002: //ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MAX_VALUE_ID:
            return "Max Measured Value";
        case 0x0003: //ESP_ZB_ZCL_ATTR_REL_HUMIDITY_TOLERANCE_ID:
            return "Tolerance";
        default:
            return "Unknown Attribute";
    }
}

esp_err_t zb_manager_humidity_meas_cluster_add_custom_attribute(
    zb_manager_humidity_measurement_cluster_t *cluster,
    uint16_t attr_id,
    uint8_t attr_type)
{
    if (!cluster) {
        ESP_LOGE(TAG, "Cluster is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Список стандартных атрибутов Humidity Measurement-кластера
    bool is_standard = false;
    switch (attr_id) {
        case ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID:     // 0x0000 — MeasuredValue
        case ATTR_REL_HUMIDITY_MEASUREMENT_MIN_VALUE_ID: // 0x0001 — MinMeasuredValue
        case ATTR_REL_HUMIDITY_MEASUREMENT_MAX_VALUE_ID: // 0x0002 — MaxMeasuredValue
        case ATTR_REL_HUMIDITY_TOLERANCE_ID:             // 0x0003 — Tolerance
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
    new_attr->parent_cluster_id = ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT; // 0x0405
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

attribute_custom_t *zb_manager_humidity_meas_cluster_find_custom_attr_obj(zb_manager_humidity_measurement_cluster_t *cluster, uint16_t attr_id)
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
