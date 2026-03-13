/// @brief [zb_manager_identify_cluster.c] Модуль для работы с Zigbee Identify Cluster (0x0003)
/// Содержит функции обновления атрибутов кластера идентификации (например, время мигания)

#include "zb_manager_identify_cluster.h"

#include "esp_err.h"
#include "string.h"

/// @brief [zb_manager_identify_cluster.c] Обновляет значение атрибута в Identify-кластере Zigbee
/// @param cluster Указатель на структуру кластера
/// @param attr_id Идентификатор атрибута (например, 0x0000 — Identify Time)
/// @param value Указатель на значение атрибута
/// @return ESP_OK при успехе, иначе ошибка
esp_err_t zigbee_manager_identify_cluster_update_attribute(zb_manager_identify_cluster_t* cluster, uint16_t attr_id, void* value)
{
    
    switch (attr_id)
    {
    case 0x0000:
        cluster->identify_time = *((uint16_t*)value);
        break;
    
    default:
        break;
    }
    return ESP_OK;
}

// File: zb_manager_identify_cluster.c

#include "zb_manager_identify_cluster.h"
#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "ZB_IDENTIFY_CL";

/**
 * @brief Convert Identify Cluster object to cJSON
 * @param cluster Pointer to the Identify cluster structure
 * @return cJSON* - new JSON object, or NULL on failure
 */
cJSON* zbm_identify_cluster_to_json(zb_manager_identify_cluster_t *cluster)
{
    if (!cluster) {
        ESP_LOGW(TAG, "zbm_identify_cluster_to_json: cluster is NULL");
        return NULL;
    }

    cJSON *identify = cJSON_CreateObject();
    if (!identify) {
        ESP_LOGE(TAG, "Failed to create identify cluster JSON object");
        return NULL;
    }

    // === Standard Attributes ===
    cJSON_AddNumberToObject(identify, "cluster_id", 0x0003);
    cJSON_AddNumberToObject(identify, "identify_time", cluster->identify_time);

    // Optional: human-readable state
    if (cluster->identify_time > 0) {
        cJSON_AddStringToObject(identify, "state", "Identifying");
    } else {
        cJSON_AddStringToObject(identify, "state", "Idle");
    }

    // Effect info (from Trigger Effect command)
    cJSON_AddNumberToObject(identify, "effect_identifier", cluster->effect_identifier);
    cJSON_AddNumberToObject(identify, "effect_variant", cluster->effect_variant);
    cJSON_AddBoolToObject(identify, "identify_in_progress", cluster->identify_in_progress);

    // Optional: effect name
    const char *effect_name = "Unknown";
    switch (cluster->effect_identifier) {
        case ZIGBEE_IDENTIFY_EFFECT_BLINK:
            effect_name = "Blink";
            break;
        case ZIGBEE_IDENTIFY_EFFECT_BREATHE:
            effect_name = "Breathe";
            break;
        case ZIGBEE_IDENTIFY_EFFECT_OKAY:
            effect_name = "Okay";
            break;
        case ZIGBEE_IDENTIFY_EFFECT_CHANNEL_CHANGE:
            effect_name = "Channel Change";
            break;
        case ZIGBEE_IDENTIFY_EFFECT_FINISH_EFFECT:
            effect_name = "Finish Effect";
            break;
        case ZIGBEE_IDENTIFY_EFFECT_STOP:
            effect_name = "Stop";
            break;
        default:
            break;
    }
    cJSON_AddStringToObject(identify, "effect_name", effect_name);

    return identify;
}

/**
 * @brief Load Identify Cluster from cJSON object
 * @param cluster Pointer to allocated or zeroed zb_manager_identify_cluster_t
 * @param json_obj cJSON object representing the cluster
 * @return ESP_OK on success
 */
esp_err_t zbm_identify_cluster_load_from_json(zb_manager_identify_cluster_t *cluster, cJSON *json_obj)
{
    if (!cluster || !json_obj) {
        return ESP_ERR_INVALID_ARG;
    }

    // Инициализируем структуру
    *cluster = (zb_manager_identify_cluster_t)ZIGBEE_IDENTIFY_CLUSTER_DEFAULT_INIT();

    // === Standard Attributes ===
    cJSON *identify_time_item = cJSON_GetObjectItem(json_obj, "identify_time");
    if (identify_time_item && cJSON_IsNumber(identify_time_item)) {
        uint16_t time_val = (uint16_t)identify_time_item->valueint;
        cluster->identify_time = (time_val <= 0xFFFE) ? time_val : 0;
    }

    // === Effect Data (from Trigger Effect command) ===
    cJSON *effect_id_item = cJSON_GetObjectItem(json_obj, "effect_identifier");
    if (effect_id_item && cJSON_IsNumber(effect_id_item)) {
        cluster->effect_identifier = (uint8_t)effect_id_item->valueint;
    }

    cJSON *effect_variant_item = cJSON_GetObjectItem(json_obj, "effect_variant");
    if (effect_variant_item && cJSON_IsNumber(effect_variant_item)) {
        cluster->effect_variant = (uint8_t)effect_variant_item->valueint;
    }

    cJSON *in_progress_item = cJSON_GetObjectItem(json_obj, "identify_in_progress");
    if (in_progress_item) {
        cluster->identify_in_progress = cJSON_IsTrue(in_progress_item);
    }

    // Опционально: имя кластера
    cJSON *name_item = cJSON_GetObjectItem(json_obj, "cluster_id_name");
    if (name_item && cJSON_IsString(name_item) && name_item->valuestring) {
        strlcpy(cluster->cluster_Id_name, name_item->valuestring, sizeof(cluster->cluster_Id_name));
    } else {
        strlcpy(cluster->cluster_Id_name, "Identify", sizeof(cluster->cluster_Id_name));
    }

    return ESP_OK;
}