// main/zbm_dev_base/zbm_core_sync.h
#ifndef ZBM_CORE_SYNC_H
#define ZBM_CORE_SYNC_H

#include "zbm_dev_types.h"
#include "zbm_attr_types.h"
#include <stdint.h>
#include <stdbool.h>
#include "zbm_guid_db.h"
#include "zbm_device_db.h"
#include "cJSON.h"
#include "zbm_dev_simple_func.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Инициализация синхронизации (вызывается один раз при старте)
 */
void zbm_core_sync_init(void);

/**
 * @brief Блокировка доступа ко всей системе устройств и атрибутов
 */
void zbm_core_sync_lock(void);

/**
 * @brief Разблокировка
 */
void zbm_core_sync_unlock(void);

// ===================================================================
// === Устройства: Safe-обёртки ======================================
// ===================================================================

//!!!!!!!!!  to_devdb  только в базу устройств zbm_device_db
//!!!!!!!!!  to_devdb_and_guiddb   как правило полное обновление
//!!!!!!!!!  to_guiddb             в базу атрибутов, команд и репортов

/**
 * @brief Безопасное добавление устройства только по IEEE-адресу с добавлением в хэш таблицу
 * @param ieee_addr Указатель на 8 байт IEEE-адреса
 * @return Указатель на существующее или новое устройство, или NULL при ошибке
 * @note Если устройство с таким IEEE уже есть — возвращает его.
 *       Инициализирует short_addr = 0xFFFE (unknown).
 */
zbm_dev_t* zbm_dev_create_and_add_to_devdb_by_ieee_safe(const uint8_t* ieee_addr);

/**
 * @brief Безопасное обновление короткого адреса устройства
 * @param dev Указатель на устройство
 * @param short_addr Новый короткий адрес
 * @return true при успехе, false если ошибка или конфликт значит надо попробовать создать по zbm_dev_create_and_add_to_devdb_by_ieee_safe
 * @note Потокобезопасна. Обновляет friendly_name.
 */
bool zbm_dev_update_short_addr_safe(zbm_dev_t* dev, uint16_t new_short_addr, const uint8_t* ieee_addr);

/**
 * @brief Безопасное удаление устройства по IEEE-адресу
 * @param ieee_addr Указатель на 8 байт IEEE-адреса
 * @return true при успехе, false если устройство не найдено или ошибка
 * @note Потокобезопасна. Автоматически освобождает память.
 */
bool zbm_device_manager_remove_by_ieee_safe(const uint8_t* ieee_addr);

/**
 * @brief Безопасное добавление эндпоинта к устройству
 * @param dev Указатель на устройство
 * @param ep Указатель на эндпоинт (должен быть создан отдельно)
 * @return true при успехе, false при ошибках (например, дубликат ID)
 * @note Потокобезопасна. Копирует указатель, не копирует содержимое.
 */
bool zbm_device_manager_add_endpoint_safe(zbm_dev_t* dev, uint8_t endpoint_id,zbm_device_type_t device_id,const char* friendlyname);


/**
 * @brief Безопасный поиск по short_addr
 */
zbm_dev_t* zbm_find_device_in_devdb_by_short_safe(uint16_t short_addr);

/**
 * @brief Безопасный поиск по IEEE в базе хэш-таблицы
 */
zbm_dev_t* zbm_find_device_in_devdb_by_ieee_safe(const uint8_t* ieee_addr);

/**
 * @brief Безопасное удаление устройства из всех хэш-таблиц 
 */
bool zbm_remove_device_from_devdb_and_guiddb_by_short_safe(uint16_t short_addr);

/**
 * @brief Безопасный перебор всех устройств
 */
void zbm_device_db_foreach_safe(void (*visitor)(zbm_dev_t*, void*), void* ctx);

// обновление GUID  с контролем дубликатов
void zbm_guid_db_update_device_guids_safe(zbm_dev_t* dev);

// ===================================================================
// === Атрибуты: Safe-обёртки ========================================
// ===================================================================

/**
 * @brief Безопасное создание стандартного кластера
 */
zbm_standart_cluster_t* zbm_create_standard_cluster_safe(uint16_t cluster_id, zbm_cluster_role_t role_mask);

/**
 * @brief Безопасная регистрация атрибута в GUID DB
 */
bool zbm_guid_db_register_safe(zbm_cluster_attribute_t** pp_attr,
                               uint16_t short_addr,
                               uint8_t endpoint,
                               uint16_t cluster_id,
                               uint16_t attr_id,
                               const char* custom_guid);

/**
 * @brief Безопасный поиск атрибута по GUID
 */
zbm_cluster_attribute_t* zbm_find_attr_by_guid_safe(const char* guid);

/**
 * @brief Безопасный поиск атрибута по ключу
 */
zbm_cluster_attribute_t* zbm_find_attr_by_key_safe(uint16_t short_addr,
                                                   uint8_t endpoint,
                                                   uint16_t cluster_id,
                                                   uint16_t attr_id);

/**
 * @brief Безопасное удаление записи по GUID
 */
void zbm_guid_db_unregister_by_guid_safe(const char* guid);

/**
 * @brief Безопасное удаление по указателю на атрибут
 */
void zbm_guid_db_unregister_by_attr_ptr_safe(zbm_cluster_attribute_t* attr);

/**
 * @brief Безопасное обновление всех указателей атрибутов
 */
void zbm_guid_db_refresh_all_attr_ptrs_safe(void);



// ===================================================================
// === Обновление модели =============================================
// ===================================================================

/**
 * @brief Безопасное обновление атрибута (включая создание при необходимости)
 * 
 * !!! если результат = 1 рекомендуется вызвать сохранение bool zbm_save_device_to_spiffs_safe(zbm_dev_t* dev);
 */
// Обновление атрибута и стандартного и кастомного кластера с проверкой role_mask
// result = 0 (update), result = 1 (update with create), result = 0xff (update error)
uint8_t zbm_device_apply_reported_value_safe(zbm_dev_t* dev_obj,
                                          uint8_t endpoint_id,
                                          uint16_t cluster_id,
                                          zbm_cluster_role_t role_mask,
                                          uint16_t attr_id,
                                          const char* attr_friendlyname,
                                          uint8_t acces,
                                          zbm_attr_data_types_t data_type,
                                          uint16_t data_size,
                                          const void* new_value);

/**
 * @brief Безопасное обновление кастомного репорта (например, Tuya 0xFD)
 */
// Обновление нестандартного репорта(команды) кластера с проверкой role_mask
// result = 0 (update), result = 1 (update with create), result = 0xff (update error)
//!!! если результат = 1 рекомендуется вызвать сохранение bool zbm_save_device_to_spiffs_safe(zbm_dev_t* dev);
uint8_t zbm_update_cluster_custom_report_safe(zbm_dev_t* dev_obj,uint8_t endpoint_id,uint16_t cluster_id, zbm_cluster_role_t role_mask,
            uint8_t cmd_id, const char* cmd_friendlyname,
            zbm_cmd_data_types_t data_type, uint16_t data_size, const void* new_value);


// ===========================  ZDO ===========================
// ===================================================================
// === zbm_device_apply_simple_descriptor_safe =======================
// ===================================================================

void zbm_device_apply_simple_descriptor_safe(zbm_dev_t* dev,uint8_t endpoint_id,uint16_t device_id,
        uint16_t* input_clusters, uint8_t in_count,uint16_t* output_clusters, uint8_t out_count);



extern cJSON* device_to_brief_json(zbm_dev_t* dev);

// === Функция: сериализация устройства в cJSON (полная структура) ===
extern cJSON* device_to_json(zbm_dev_t* dev);

/**
 * @brief Сохраняет устройство в SPIFFS: dev_0x1234.json + обновляет индекс
 * @param dev Указатель на устройство
 * @return true при успехе
 */
extern  bool zbm_save_device_to_spiffs_safe(zbm_dev_t* dev);

/**
 * @brief Считает количество устройств в базе
 * @return Количество активных устройств
 */
extern size_t zbm_device_db_count(void);
extern size_t zbm_device_db_count_safe(void);
/**
 * @brief Сохраняет все устройства из базы в SPIFFS (с обновлением индекса)
 * @return true если все устройства успешно сохранены
 */
extern bool zbm_save_all_devices_to_spiffs_safe(void);

/**
 * @brief Загружает JSON устройства по короткому адресу
 * @param short_addr Короткий адрес
 * @return cJSON* или NULL
 */
extern cJSON* zbm_load_device_json_by_short_safe(uint16_t short_addr);

/**
 * @brief Загружает JSON устройства по IEEE-адресу (полезно при пересопряжении)
 * @param ieee_addr Указатель на 8 байт IEEE
 * @return cJSON* или NULL
 */
extern cJSON* zbm_load_device_json_by_ieee_safe(const uint8_t* ieee_addr);

/**
 * @brief Загружает все устройства из индекса (пока только логирование)
 */
extern void zbm_load_all_devices_from_spiffs(void);

/**
 * @brief Загружает все устройства из индекса (пока только логирование)
 */
//void zbm_load_all_devices_from_spiffs(void);

extern void zbm_load_all_devices_from_spiffs_and_restore(void);

#ifdef __cplusplus
}
#endif

#endif // ZBM_CORE_SYNC_H