// main/zbm_automation/zbm_automation.h
#ifndef ZBM_AUTOMATION_H
#define ZBM_AUTOMATION_H

#include <stdint.h>
#include <stdbool.h>
#include "zbm_rules.h"          // Уже включает zbm_clusters_type.h и zbm_attr_types.h
#include "zbm_clusters_type.h"  // Для ясности (можно не включать, если уже в rules.h)
#include "zbm_attr_types.h"     // Типы атрибутов

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Запрос на выполнение команды автоматизации
 */
typedef struct {
    uint16_t short_addr;        // Адрес устройства
    uint8_t endpoint_id;        // Endpoint
    uint8_t cluster_id;         // Кластер (например, ON_OFF, LEVEL_CONTROL)
    uint8_t cmd_id;             // Команда: 0x00=off, 0x01=on, 0x02=toggle
    uint8_t value;              // Дополнительное значение (например, яркость)
} zb_automation_request_t;

/**
 * @brief Инициализация подсистемы автоматизации
 */
void zb_automation_init(void);

/**
 * @brief Отправить команду устройству (вызывается из правил)
 */
bool zb_automation_send_command(const zb_automation_request_t* req);

/**
 * @brief Вызвать при изменении атрибута устройства
 *
 * Эта функция должна вызываться из ZCL-обработчика при обновлении состояния.
 */
void zb_automation_report_received(
    uint16_t short_addr,
    uint8_t endpoint_id,
    uint16_t cluster_id,                // ← было esp_zb_zcl_cluster_id_t
    uint16_t attr_id,
    void* data,
    uint8_t data_len,
    zbm_attr_data_types_t attr_type     // ← было esp_zb_zcl_attr_type_t
);

/**
 * @brief Вызвать при потере связи с устройством
 */
void zb_automation_device_unavailable(uint16_t short_addr, uint16_t cluster_id);

/**
 * @brief Проверка временных триггеров (вызывать раз в секунду)
 */
void zb_automation_check_time_triggers(void);

#ifdef __cplusplus
}
#endif

#endif // ZBM_AUTOMATION_H