#include "zbm_clusters.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdlib.h>
#include "esp_zigbee_type.h"
#include "esp_zigbee_zcl_command.h"

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

