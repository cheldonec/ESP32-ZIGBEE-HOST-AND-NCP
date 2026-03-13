#include "zbm_clusters.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdlib.h>
#include "esp_zigbee_type.h"
#include "esp_zigbee_zcl_command.h"
#include "zbm_dev_base_utils.h"

static const char *TAG = "ZBM_CLUSTERS";

void zbm_update_custom_attribute_value(attribute_custom_t *attr, const uint8_t *new_value, uint16_t value_len);

//================================================================================================================================
//=============================================== ZBM_DEV_BASE_DEV_UPDATE_NOSTANDART_ATTR ========================================
//================================================================================================================================
void zbm_update_custom_attribute_value(attribute_custom_t *attr, const uint8_t *new_value, uint16_t value_len)
{
    if (!attr || !new_value || value_len == 0) return;

    uint16_t alloc_len = value_len;

    // Для строковых типов: значение содержит [len][data...]
    if (attr->is_void_pointer) {
        if (value_len < 1) {
            ESP_LOGW(TAG, "Invalid value_len for string attr 0x%04x: %u", attr->id, value_len);
            return;
        }
        uint8_t str_len = new_value[0];
        if (str_len == 0xFF) {
            ESP_LOGW(TAG, "Unspecified length in string attr 0x%04x", attr->id);
            return;
        }
        alloc_len = str_len + 1; // включаем байт длины
    }

    // Перевыделяем память при необходимости
    if (attr->p_value == NULL || attr->size != alloc_len) {
        if (attr->p_value) {
            free(attr->p_value);
        }
        attr->p_value = malloc(alloc_len);
        if (!attr->p_value) {
            ESP_LOGE(TAG, "Failed to allocate p_value for attr 0x%04x", attr->id);
            return;
        }
        attr->size = alloc_len;
    }

    memcpy(attr->p_value, new_value, alloc_len);
    attr->last_update_ms = esp_log_timestamp(); // если поле есть
}

esp_err_t zbm_cluster_remove_custom_attribute(cluster_custom_t *cluster, uint16_t attr_id)
{
    if (!cluster || !cluster->attr_array) return ESP_ERR_INVALID_ARG;

    int idx = -1;
    for (int i = 0; i < cluster->attr_count; i++) {
        if (cluster->attr_array[i] && cluster->attr_array[i]->id == attr_id) {
            idx = i;
            break;
        }
    }

    if (idx == -1) return ESP_ERR_NOT_FOUND;

    attribute_custom_t *attr = cluster->attr_array[idx];
    if (attr->p_value) free(attr->p_value);
    free(attr);

    // Сдвигаем массив
    for (int i = idx; i < cluster->attr_count - 1; i++) {
        cluster->attr_array[i] = cluster->attr_array[i + 1];
    }
    cluster->attr_count--;

    // Реаллокируем
    if (cluster->attr_count == 0) {
        free(cluster->attr_array);
        cluster->attr_array = NULL;
    } else {
        void *new_array = realloc(cluster->attr_array, cluster->attr_count * sizeof(attribute_custom_t*));
        if (new_array) {
            cluster->attr_array = (attribute_custom_t**)new_array;
        }
    }

    ESP_LOGI(TAG, "🗑 Removed custom attr 0x%04x from cluster 0x%04x", attr_id, cluster->id);
    return ESP_OK;
}

void zbm_cluster_free_all_attributes(cluster_custom_t *cluster)
{
    if (!cluster || !cluster->attr_array) return;

    for (int i = 0; i < cluster->attr_count; i++) {
        attribute_custom_t *attr = cluster->attr_array[i];
        if (attr) {
            if (attr->p_value) free(attr->p_value);
            free(attr);
        }
    }
    free(cluster->attr_array);
    cluster->attr_array = NULL;
    cluster->attr_count = 0;

    ESP_LOGI(TAG, "🧹 Freed all attributes in cluster 0x%04x", cluster->id);
}

esp_err_t zbm_cluster_add_custom_attribute(
    cluster_custom_t *cluster,
    uint16_t attr_id,
    uint8_t attr_type)
{
    if (!cluster) {
        ESP_LOGE(TAG, "Cluster is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Проверяем, существует ли уже атрибут с таким ID
    for (int i = 0; i < cluster->attr_count; i++) {
        if (cluster->attr_array[i] && cluster->attr_array[i]->id == attr_id) {
            ESP_LOGD(TAG, "Attr 0x%04x already exists in cluster 0x%04x", attr_id, cluster->id);
            return ESP_OK;
        }
    }

    // Выделяем память под новый атрибут
    attribute_custom_t *new_attr = calloc(1, sizeof(attribute_custom_t));
    if (!new_attr) {
        ESP_LOGE(TAG, "Failed to allocate memory for attribute 0x%04x", attr_id);
        return ESP_ERR_NO_MEM;
    }

    // Заполняем поля
    new_attr->id = attr_id;
    new_attr->type = attr_type;
    new_attr->acces = 0;                    // По умолчанию — нет доступа (можно обновить позже)
    new_attr->manuf_code = 0;               // По умолчанию — нет manufacturer code
    new_attr->parent_cluster_id = cluster->id;
    new_attr->size = zb_manager_get_zcl_attr_size(attr_type); 
    new_attr->is_void_pointer = (attr_type >= 0x41 && attr_type <= 0x51); // Для строк, массивов и т.п.
    new_attr->p_value = NULL;

    // Генерируем имя атрибута: "Custom_0xXXXX"
    snprintf(new_attr->attr_id_text, sizeof(new_attr->attr_id_text), "Custom_0x%04X", attr_id);

    // Увеличиваем массив атрибутов
    void *new_array = realloc(cluster->attr_array, (cluster->attr_count + 1) * sizeof(attribute_custom_t*));
    if (!new_array) {
        free(new_attr);
        ESP_LOGE(TAG, "Failed to realloc attr_array");
        return ESP_ERR_NO_MEM;
    }

    cluster->attr_array = (attribute_custom_t**)new_array;
    cluster->attr_array[cluster->attr_count] = new_attr;
    cluster->attr_count++;

    ESP_LOGI(TAG, "✅ Added custom attribute 0x%04x to cluster 0x%04x (type: 0x%02x)",
             attr_id, cluster->id, attr_type);

    return ESP_OK;
}

attribute_custom_t* zbm_cluster_find_custom_attribute(cluster_custom_t *cluster, uint16_t attr_id)
{
    if (!cluster || !cluster->attr_array) return NULL;

    for (int i = 0; i < cluster->attr_count; i++) {
        attribute_custom_t *attr = cluster->attr_array[i];
        if (attr && attr->id == attr_id) {
            return attr;
        }
    }
    return NULL;
}

//================================================================================================================================
//=========================================== ZBM_DEV_BASE_DEVICE_TO_JSON_ADD_ATTR_VALUE =========================================
//================================================================================================================================
void zbm_json_add_attribute_value(cJSON *attr_obj, attribute_custom_t *attr)
{
    if (!attr_obj || !attr) {
        return;
    }

    // Если p_value == NULL или size == 0 → null
    if (attr->p_value == NULL || attr->size == 0) {
        cJSON_AddNullToObject(attr_obj, "value");
        return;
    }

    uint16_t expected_size = zb_manager_get_zcl_attr_size(attr->type);

    // === Определяем, является ли тип строковым ===
    if (attr->type == 0x48 || attr->type == 0x49 || attr->type == 0x4A || attr->type == 0x4B) {
        cJSON_AddStringToObject(attr_obj, "value_type", "string_skipped");
        return;
    }

    // === Обработка числовых типов ===
    switch (attr->type) {
        case 0x08:  // uint8
        case 0x10:
        case 0x20:  // bitmap8
        case 0x30:  // enum8
        case 0x00:  // no data
        {
            uint8_t val = 0;
            memcpy(&val, attr->p_value, 1);
            cJSON_AddNumberToObject(attr_obj, "value", val);
            break;
        }
        case 0x09:  // int16
        case 0x11:  // uint16
        case 0x21:  // bitmap16
        case 0x31:  // enum16
        case 0x22:  // cluster_id
        case 0x23:  // attr_id
        {
            uint16_t val = 0;
            memcpy(&val, attr->p_value, 2);
            cJSON_AddNumberToObject(attr_obj, "value", val);
            break;
        }
        case 0x0A:  // int24
        case 0x12:  // uint24
        {
            uint32_t val = 0;
            memcpy(&val, attr->p_value, 3);
            cJSON_AddNumberToObject(attr_obj, "value", val);
            break;
        }
        case 0x0B:  // int32
        case 0x13:  // uint32
        case 0x24:  // bitmap32
        case 0x41:  // single
        case 0x44:  // UTC time
        {
            uint32_t val = 0;
            memcpy(&val, attr->p_value, 4);
            cJSON_AddNumberToObject(attr_obj, "value", val);
            break;
        }
        case 0x0C:  // uint40
        case 0x14:
        {
            uint64_t val = 0;
            memcpy(&val, attr->p_value, 5);
            char hex_str[13];
            snprintf(hex_str, sizeof(hex_str), "0x%010llx", (unsigned long long)val);
            cJSON_AddStringToObject(attr_obj, "value_hex", hex_str);
            break;
        }
        case 0x0D:  // uint48
        case 0x15:
        {
            uint64_t val = 0;
            memcpy(&val, attr->p_value, 6);
            char hex_str[15];
            snprintf(hex_str, sizeof(hex_str), "0x%012llx", (unsigned long long)val);
            cJSON_AddStringToObject(attr_obj, "value_hex", hex_str);
            break;
        }
        case 0x0E:  // uint56
        case 0x16:
        {
            uint64_t val = 0;
            memcpy(&val, attr->p_value, 7);
            char hex_str[17];
            snprintf(hex_str, sizeof(hex_str), "0x%014llx", (unsigned long long)val);
            cJSON_AddStringToObject(attr_obj, "value_hex", hex_str);
            break;
        }
        case 0x0F:  // int64
        case 0x17:  // uint64
        case 0x25:  // bitmap64
        case 0x55:  // IEEE addr
        {
            uint64_t val = 0;
            memcpy(&val, attr->p_value, 8);
            cJSON_AddNumberToObject(attr_obj, "value", (double)val); // double безопасен до 2^53
            break;
        }
        case 0xF0:  // 128-bit key
        {
            char hex_str[33] = {0};
            for (int i = 0; i < 16; i++) {
                sprintf(&hex_str[i*2], "%02X", ((uint8_t*)attr->p_value)[i]);
            }
            cJSON_AddStringToObject(attr_obj, "value_hex", hex_str);
            break;
        }
        default:
        {
            // Для неизвестных типов — выводим hex до 8 байт
            if (attr->size <= 8) {
                uint64_t val = 0;
                memcpy(&val, attr->p_value, attr->size);
                char hex_str[19];
                snprintf(hex_str, sizeof(hex_str), "0x%016llx", (unsigned long long)val);
                cJSON_AddStringToObject(attr_obj, "value_hex", hex_str);
            } else {
                cJSON_AddStringToObject(attr_obj, "value", "binary_large");
            }
            break;
        }
    }

    // Дополнительно: можно добавить тип как строку
    // cJSON_AddNumberToObject(attr_obj, "expected_size", expected_size);
}

//================================================================================================================================
//======================================== ZBM_DEV_BASE_LOAD_DEV_FROM_JSON_LOAD_ATTR =============================================
//================================================================================================================================
void zbm_json_load_attribute_value(attribute_custom_t *attr, cJSON *attr_obj)
{
    if (!attr || !attr_obj || attr->size == 0) {
        return;
    }

    // Выделяем память под значение
    attr->p_value = calloc(1, attr->size);
    if (!attr->p_value) {
        ESP_LOGE(TAG, "Failed to allocate p_value for attr 0x%04x, size=%u", attr->id, attr->size);
        return;
    }

    // Сначала пробуем прочитать как число (value)
    cJSON *val_num = cJSON_GetObjectItem(attr_obj, "value");
    if (val_num && cJSON_IsNumber(val_num)) {
        uint64_t num_val = (uint64_t)val_num->valuedouble; // double безопасен до 2^53

        // Копируем младшие байты в p_value (little-endian, как в Zigbee)
        for (int i = 0; i < attr->size; i++) {
            ((uint8_t*)attr->p_value)[i] = (num_val >> (i * 8)) & 0xFF;
        }
        return;
    }

    // Потом пробуем как hex-строку (value_hex)
    cJSON *val_hex = cJSON_GetObjectItem(attr_obj, "value_hex");
    if (val_hex && cJSON_IsString(val_hex) && val_hex->valuestring) {
        uint64_t num_val = hexstr_to_uint64(val_hex->valuestring);

        for (int i = 0; i < attr->size && i < 8; i++) {
            ((uint8_t*)attr->p_value)[i] = (num_val >> (i * 8)) & 0xFF;
        }
        // Если больше 8 байт — не поддерживается этим способом
        if (attr->size > 8) {
            ESP_LOGW(TAG, "Cannot fully restore value_hex for size > 8: attr 0x%04x", attr->id);
        }
        return;
    }

    // Если нет ни value, ни value_hex — оставляем нули (calloc уже обнулил)
    ESP_LOGD(TAG, "No value found for attr 0x%04x, initialized to zero", attr->id);
}


/**
 * @brief Convert a custom/unknown cluster to cJSON
 * @param cluster Pointer to the generic cluster structure
 * @return cJSON* - new JSON object, or NULL on failure
 */
cJSON* zbm_unknown_cluster_to_json(cluster_custom_t *cluster)
{
    if (!cluster) {
        ESP_LOGW(TAG, "zbm_unknown_cluster_to_json: cluster is NULL");
        return NULL;
    }

    cJSON *json_cluster = cJSON_CreateObject();
    if (!json_cluster) {
        ESP_LOGE(TAG, "Failed to create JSON for unknown cluster 0x%04x", cluster->id);
        return NULL;
    }

    cJSON_AddNumberToObject(json_cluster, "cluster_id", cluster->id);
    cJSON_AddStringToObject(json_cluster, "cluster_id_text", cluster->cluster_id_text);
    cJSON_AddNumberToObject(json_cluster, "role_mask", cluster->role_mask);
    cJSON_AddNumberToObject(json_cluster, "manuf_code", cluster->manuf_code);
    cJSON_AddBoolToObject(json_cluster, "is_use_on_device", cluster->is_use_on_device);

    // === Custom Attributes (может быть пустым) ===
    if (cluster->attr_count > 0 && cluster->attr_array) {
        cJSON *attrs = cJSON_CreateArray();
        if (attrs) {
            for (int i = 0; i < cluster->attr_count; i++) {
                attribute_custom_t *attr = cluster->attr_array[i];
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

                zbm_json_add_attribute_value(attr_obj, attr);

                cJSON_AddItemToArray(attrs, attr_obj);
            }
            cJSON_AddItemToObject(json_cluster, "attributes", attrs);
        }
    } else {
        // Даже если атрибутов нет — добавляем пустой массив или пропускаем
         cJSON_AddArrayToObject(json_cluster, "attributes"); // если хочется явно []
    }

    return json_cluster;
}

