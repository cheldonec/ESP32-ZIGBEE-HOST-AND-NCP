// components/zb_manager/devices/zb_manager_power_config_cluster.c
/// @brief [zb_manager_power_config_cluster.c] Модуль для работы с Zigbee Power Configuration Cluster (0x0001)
/// Содержит функции обновления атрибутов, связанных с питанием устройства (батарея, напряжение и т.д.)
#include "zb_manager_power_config_cluster.h"
#include "esp_err.h"
#include "string.h"
#include "stdio.h"
#include "esp_log.h"
#include "esp_zigbee_type.h"
#include "esp_zigbee_zcl_command.h"

static const char *TAG = "ZB_PWR_CFG_CL";

/// @brief [zb_manager_power_config_cluster.c] Обновляет значение атрибута в Power Config-кластере Zigbee
/// @param cluster Указатель на структуру кластера
/// @param attr_id Идентификатор атрибута (например, 0x0020 — Battery Voltage)
/// @param value Указатель на значение атрибута
/// @return ESP_OK при успехе, иначе ошибка
esp_err_t zb_manager_power_config_cluster_update_attribute(zb_manager_power_config_cluster_t* cluster,uint16_t attr_id,uint8_t attr_type,void* value,uint16_t value_len)
{
    uint8_t *data = NULL;
    uint8_t len = 0;

    if (value == NULL) {
        ESP_LOGW(TAG, "attr_id=0x%04x: value is NULL", attr_id);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Updating PowerConfig attr 0x%04x", attr_id);
    cluster->last_update_ms = esp_log_timestamp();

    switch (attr_id)
    {
        // === Mains Power ===
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_VOLTAGE_ID:
            cluster->mains_voltage = *(uint16_t*)value;
            break;
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_FREQUENCY_ID:
            cluster->mains_frequency = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_ALARM_MASK_ID:
            cluster->mains_alarm_mask = *(uint8_t*)value;
            break;
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_VOLTAGE_MIN_THRESHOLD:
            cluster->mains_voltage_min_th = *(uint16_t*)value;
            break;
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_VOLTAGE_MAX_THRESHOLD:
            cluster->mains_voltage_max_th = *(uint16_t*)value;
            break;
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_DWELL_TRIP_POINT:
            cluster->mains_dwell_trip_point = *(uint16_t*)value;
            break;

        // === Battery 1 ===
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID:
            cluster->battery_voltage = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID:
            cluster->battery_percentage = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_MANUFACTURER_ID:
            memset(cluster->battery_manufacturer, 0, sizeof(cluster->battery_manufacturer));
            data = (uint8_t*)value;
            len = data[0];
            if (len > 0 && len < sizeof(cluster->battery_manufacturer)) {
                memcpy(cluster->battery_manufacturer, data + 1, len);
                cluster->battery_manufacturer[len] = '\0';
            } else {
                ESP_LOGW(TAG, "Invalid battery manufacturer string length: %u", len);
            }
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_SIZE_ID:
            cluster->battery_size = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_A_HR_RATING_ID:
            cluster->battery_a_hr_rating = *(uint16_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_QUANTITY_ID:
            cluster->battery_quantity = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_RATED_VOLTAGE_ID:
            cluster->battery_rated_voltage = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_ALARM_MASK_ID:
            cluster->battery_alarm_mask = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_MIN_THRESHOLD_ID:
            cluster->battery_voltage_min_th = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD1_ID:
            cluster->battery_voltage_th1 = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD2_ID:
            cluster->battery_voltage_th2 = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD3_ID:
            cluster->battery_voltage_th3 = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_MIN_THRESHOLD_ID:
            cluster->battery_percentage_min_th = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD1_ID:
            cluster->battery_percentage_th1 = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD2_ID:
            cluster->battery_percentage_th2 = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD3_ID:
            cluster->battery_percentage_th3 = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_ALARM_STATE_ID:
            cluster->battery_alarm_state = *(uint32_t*)value;
            break;

        // === Battery 2 & 3 (опционально, можно расширить позже) ===
        // Пока не реализуем — но можно добавить аналогично

        default: {
            // === Обработка кастомного атрибута ===
            attribute_custom_t *custom_attr = zb_manager_power_config_cluster_find_custom_attr_obj(cluster, attr_id);
            if (custom_attr) {
                // Для строковых типов: значение включает длину → используем её
                uint16_t expected_len = custom_attr->size;
                if (custom_attr->is_void_pointer) {
                    if (value_len < 1 || data[0] == 0xFF) {
                        ESP_LOGW(TAG, "Invalid string length: %u", data[0]);
                        return ESP_ERR_INVALID_ARG;
                    }
                    expected_len = data[0] + 1;
                }

                // Перевыделяем память при необходимости
                if (custom_attr->p_value == NULL || custom_attr->size != expected_len) {
                    if (custom_attr->p_value) free(custom_attr->p_value);
                    custom_attr->p_value = malloc(expected_len);
                    if (!custom_attr->p_value) {
                        ESP_LOGE(TAG, "Failed to allocate p_value for attr 0x%04x", attr_id);
                        return ESP_ERR_NO_MEM;
                    }
                    custom_attr->size = expected_len;
                }

                memcpy(custom_attr->p_value, value, expected_len);
                //custom_attr->last_update_ms = esp_log_timestamp();

                ESP_LOGI(TAG, ".updated attr 0x%04x in PowerConfig Cluster (type=0x%02x, len=%u)",
                         attr_id, attr_type, expected_len);
                return ESP_OK;
            } else {
                // === Автосоздание атрибута при REPORT ===
                ESP_LOGI(TAG, "Auto-create PowerConfig attr 0x%04x (type=0x%02x, len=%u)", attr_id, attr_type, value_len);

                esp_err_t err = zb_manager_power_config_cluster_add_custom_attribute(cluster, attr_id, attr_type);
                if (err != ESP_OK) return err;

                custom_attr = zb_manager_power_config_cluster_find_custom_attr_obj(cluster, attr_id);
                if (custom_attr) {
                    uint16_t alloc_len = custom_attr->size;
                    if (custom_attr->is_void_pointer) {
                        if (value_len < 1 || data[0] == 0xFF) return ESP_ERR_INVALID_ARG;
                        alloc_len = data[0] + 1;
                    }

                    custom_attr->p_value = malloc(alloc_len);
                    if (!custom_attr->p_value) return ESP_ERR_NO_MEM;
                    memcpy(custom_attr->p_value, value, alloc_len);
                    custom_attr->size = alloc_len;
                    //custom_attr->last_update_ms = esp_log_timestamp();

                    ESP_LOGI(TAG, ".created+updated attr 0x%04x", attr_id);
                    return ESP_OK;
                }
                return ESP_ERR_NOT_FOUND;
            }
        }
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

esp_err_t zb_manager_power_config_cluster_add_custom_attribute(
    zb_manager_power_config_cluster_t *cluster,
    uint16_t attr_id,
    uint8_t attr_type)
{
    if (!cluster) {
        ESP_LOGE(TAG, "Cluster is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Список стандартных атрибутов Power Configuration-кластера
    bool is_standard = false;
    switch (attr_id) {
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_VOLTAGE_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_FREQUENCY_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_ALARM_MASK_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_VOLTAGE_MIN_THRESHOLD:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_VOLTAGE_MAX_THRESHOLD:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_DWELL_TRIP_POINT:

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID:

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_MANUFACTURER_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_SIZE_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_A_HR_RATING_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_QUANTITY_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_RATED_VOLTAGE_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_ALARM_MASK_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_MIN_THRESHOLD_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD1_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD2_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD3_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_MIN_THRESHOLD_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD1_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD2_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD3_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_ALARM_STATE_ID:

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_REMAINING_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_MANUFACTURER_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_SIZE_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_A_HR_RATING_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_QUANTITY_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_RATED_VOLTAGE_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_ALARM_MASK_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_MIN_THRESHOLD_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_THRESHOLD1_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_THRESHOLD2_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_THRESHOLD3_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_MIN_THRESHOLD_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_THRESHOLD1_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_THRESHOLD2_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_THRESHOLD3_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_ALARM_STATE_ID:

        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_REMAINING_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_MANUFACTURER_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_SIZE_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_A_HR_RATING_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_QUANTITY_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_RATED_VOLTAGE_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_ALARM_MASK_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_MIN_THRESHOLD_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_THRESHOLD1_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_THRESHOLD2_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_THRESHOLD3_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_MIN_THRESHOLD_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_THRESHOLD1_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_THRESHOLD2_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_THRESHOLD3_ID:
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_ALARM_STATE_ID:
            is_standard = true;
            break;
        default:
            is_standard = false;
            break;
    }

    if (is_standard) {
        ESP_LOGD(TAG, "Attr 0x%04x is standard — skipping", attr_id);
        return ESP_ERR_NOT_SUPPORTED; // или ESP_OK, по желанию
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
    new_attr->parent_cluster_id = 0x0001; // Power Configuration Cluster ID
    new_attr->size = zb_manager_get_zcl_attr_size(attr_type);
    new_attr->is_void_pointer = (attr_type >= 0x41 && attr_type <= 0x51); // строки, массивы и т.п.
    new_attr->acces = 0; // может быть обновлён позже
    new_attr->manuf_code = 0; // можно установить отдельно при необходимости
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

attribute_custom_t *zb_manager_power_config_cluster_find_custom_attr_obj(zb_manager_power_config_cluster_t *cluster, uint16_t attr_id)
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

    return NULL; // not found
}