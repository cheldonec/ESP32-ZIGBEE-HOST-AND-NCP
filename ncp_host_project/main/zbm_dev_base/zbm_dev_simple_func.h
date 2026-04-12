#ifndef ZBM_DEV_SIMPLE_FUNC_H

#define ZBM_DEV_SIMPLE_FUNC_H
#include "zbm_dev_types.h"
#include "zbm_guid_db.h"
#include <stdbool.h>
#include "cJSON.h"



// Обновление атрибута и стандартного и кастомного кластера с проверкой role_mask
// result = 0 (update), result = 1 (update with create), result = 0xff (update error)
uint8_t zbm_device_apply_reported_value(zbm_dev_t* dev_obj, uint8_t endpoint_id, uint16_t cluster_id, 
    zbm_cluster_role_t role_mask, uint16_t attr_id,
    const char* attr_friendlyname, uint8_t acces, zbm_attr_data_types_t data_type, uint16_t data_size, const void* new_value);

// Обновление нестандартного репорта(команды) кластера с проверкой role_mask
// result = 0 (update), result = 1 (update with create), result = 0xff (update error)
uint8_t zbm_update_cluster_custom_report(zbm_dev_t* dev_obj,uint8_t endpoint_id,
    uint16_t cluster_id,zbm_cluster_role_t role_mask,uint8_t cmd_id,
    const char* cmd_friendlyname,zbm_cmd_data_types_t data_type,uint16_t data_size,const void* new_value);

// === Обработка ответа Active Endpoint ===
// Обновление нестандартного репорта(команды) кластера с проверкой role_mask
// result = 0 (update), result = 1 (update with create), result = 0xff (update error)
uint8_t zbm_process_active_endpoint_response(zbm_dev_t* dev, uint8_t zdo_status, uint8_t ep_count, uint8_t* ep_list);

    /**
 * @brief Генерирует имя кластера (например, "On/Off" или "Custom Cluster 0xEF00")
 * @param cluster_id ID кластера
 * @param is_custom true — если это кастомный кластер
 * @return Указатель на строку (выделена через malloc). Клиент должен вызвать free().
 */
char* generate_cluster_name(uint16_t cluster_id, bool is_custom);


// Convertations

// Вспомогательная: создаёт краткую версию устройства
/*
[
  {
    "short": "0x1234",
    "ieee": "00:12:4B:00:12:34:56:78",
    "friendly_name": "Гостиная лампа",
    "online": true,
    "last_seen": "2025-04-05T12:34:56Z",
    "linkquality": 120
  }
]
*/
cJSON* device_to_brief_json(zbm_dev_t* dev);





#endif // ZBM_DEV_SIMPLE_FUNC_H
