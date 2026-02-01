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
