# Шаблон: Добавление нового стандартного кластера

> 🎯 Цель: быстро и правильно добавить поддержку любого стандартного Zigbee кластера (например, IAS Zone, Pressure Measurement, Occupancy Sensing и т.д.)  
> Этот шаблон можно использовать как пошаговую инструкцию.

---

## 🔧 Шаг 1: Определите ID и атрибуты кластера

Найдите в спецификации Zigbee Cluster Library (ZCL):
- **Cluster ID** — например, `0x0406` для `Occupancy Sensing`
- **Стандартные атрибуты**:
  - ID, имя, тип (`ZBM_ATTR_TYPE_*`), размер

Пример:
| Attr ID | Name               | Type       | Size |
|--------|--------------------|------------|------|
| 0x0000 | Occupancy          | bool       | 1    |
| 0x0001 | OccupancyType      | bitmap8    | 1    |
| 0x0010 | PIROccupiedToUnoccupiedDelay | u16 | 2    |

---

## 📌 Шаг 2: Убедитесь, что кластер объявлен в `zbm_clusters_type.h`

Откройте: main/zbm_dev_base/zbm_clusters_type.h
Проверьте, есть ли ваш кластер в `zbm_cluster_id_t`. Если нет — добавьте:

```c
typedef enum {
    ...
    ZBM_CLUSTER_ID_OCCUPANCY_SENSING = 0x0406U,  /*!< Occupancy sensing cluster */
    ...
} zbm_cluster_id_t;

## 📌 Шаг 3: Создайте файлы фабрики атрибутов
Создайте два файла в папке main/zbm_dev_base/:

Файл: zbm_attr_factory_occupancy_sensing.h
=================================================================================
// File: main/zbm_dev_base/zbm_attr_factory_occupancy_sensing.h
#ifndef ZBM_ATTR_FACTORY_OCCUPANCY_SENSING_H
#define ZBM_ATTR_FACTORY_OCCUPANCY_SENSING_H

#include "zbm_clusters_type.h"

/**
 * @brief Создаёт массив стандартных атрибутов для Occupancy Sensing Cluster (0x0406)
 * @param[out] count Указатель, куда запишется количество созданных атрибутов
 * @return Массив указателей на zbm_cluster_attribute_t или NULL при ошибке
 */
zbm_cluster_attribute_t** zbm_create_occupancy_sensing_attr_array(uint8_t* count);

#endif // ZBM_ATTR_FACTORY_OCCUPANCY_SENSING_H
=================================================================================

Файл: zbm_attr_factory_occupancy_sensing.c
=================================================================================
// File: main/zbm_dev_base/zbm_attr_factory_occupancy_sensing.c
#include "zbm_attr_factory_occupancy_sensing.h"
#include "zbm_attr_types.h"
#include "zbm_dev_simple_func.h"
#include <string.h>
#include <stdlib.h>

/**
 * @brief Создаёт массив атрибутов для Occupancy Sensing Cluster
 * @param[out] count Количество созданных атрибутов
 * @return Массив указателей на атрибуты или NULL
 */
zbm_cluster_attribute_t** zbm_create_occupancy_sensing_attr_array(uint8_t* count) {
    *count = 3;
    zbm_cluster_attribute_t** arr = calloc(*count, sizeof(zbm_cluster_attribute_t*));
    if (!arr) return NULL;

    int i = 0;
    arr[i++] = create_attr(0x0000, "Occupancy",              ZBM_ATTR_TYPE_BOOL,        1);
    arr[i++] = create_attr(0x0001, "OccupancyType",          ZBM_ATTR_TYPE_T8BITMAP,    1);
    arr[i++] = create_attr(0x0010, "PIROccupiedToUnoccupiedDelay", ZBM_ATTR_TYPE_U16,   2);

    // При необходимости добавьте остальные: PIRUnoccupiedToOccupiedDelay и т.д.

    return arr;
}
=================================================================================

🔗 Шаг 4: Зарегистрируйте фабрику в общей фабрике

Найдите реализацию функции: 
zbm_cluster_attribute_t** zbm_create_standard_attribute_array(uint16_t cluster_id, uint8_t* count);

Добавьте обработку нового кластера:
=================================================================================
zbm_cluster_attribute_t** zbm_create_standard_attribute_array(uint16_t cluster_id, uint8_t* count) {
    switch (cluster_id) {
        case ZBM_CLUSTER_ID_ON_OFF:
            return zbm_create_on_off_attr_array(count);
        case ZBM_CLUSTER_ID_TEMP_MEASUREMENT:
            return zbm_create_temperature_meas_attr_array(count);
        case ZBM_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT:
            return zbm_create_humidity_meas_attr_array(count);
        case ZBM_CLUSTER_ID_OCCUPANCY_SENSING:
            return zbm_create_occupancy_sensing_attr_array(count);

        default:
            *count = 0;
            return NULL;
    }
}
=================================================================================

🛠️ Шаг 5: (Опционально) Добавьте команды

Если кластер имеет стандартные команды (например, ZoneStatusChanged), можно создать фабрику команд.

Пример: zbm_cmd_factory_occupancy_sensing.h
=================================================================================
zbm_cluster_standart_cmd_t** zbm_create_occupancy_sensing_cmd_array(uint8_t* count);
=================================================================================

Реализация аналогична zbm_create_on_off_cmd_array
=================================================================================
zbm_cluster_standart_cmd_t** zbm_create_standard_command_array(uint16_t cluster_id, uint8_t* count);
=================================================================================

💡 Шаг 6: Используйте в коде
=================================================================================
bool is_occupied = true;

uint8_t result = zbm_update_cluster_attribute_safe(
    dev,                                // zbm_dev_t*
    1,                                  // endpoint_id
    ZBM_CLUSTER_ID_OCCUPANCY_SENSING,   // cluster_id
    0x0000,                             // attr_id: Occupancy
    "Occupancy",                        // friendly name
    0,                                  // access
    ZBM_ATTR_TYPE_BOOL,                 // type
    1,                                  // size
    &is_occupied                        // value
);
=================================================================================

🔍 Шаг 7: Проверка
После вызова проверьте:

Лог: ✅ UPDATE+CREATE → значит, всё создано

Поиск по GUID:
=================================================================================
char guid[64];
zbm_generate_attr_guid(guid, sizeof(guid), dev->short_addr, 1, 0x0406, 0x0000);

zbm_cluster_attribute_t* attr = zbm_find_attr_by_guid_safe(guid);
if (attr && attr->p_value) {
    bool occ = *(bool*)attr->p_value;
    ESP_LOGI("TEST", "Occupancy: %s", occ ? "yes" : "no");
}
=================================================================================

✅ Рекомендации

Имена файлов: используйте snake_case:


zbm_attr_factory_<name>.h/.c
Например: zbm_attr_factory_ias_zone.c


Типы данных: сверьтесь с zbm_attr_types.h:


bool → ZBM_ATTR_TYPE_BOOL, size=1
int16 → ZBM_ATTR_TYPE_S16, size=2
float → ZBM_ATTR_TYPE_SINGLE, size=4


Память: все атрибуты и строки (friendlyname) дублируются через strdup или calloc — не освобождайте их вручную. Используйте zbm_free_dev_t().