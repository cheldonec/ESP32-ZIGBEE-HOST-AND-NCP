// main/zbm_core/zbm_guid_db.h
#ifndef ZBM_GUID_DB_H
#define ZBM_GUID_DB_H

#include "zbm_attr_types.h"
#include "zbm_clusters_type.h"
#include "zbm_cmd_types.h"
#include <stdint.h>
#include <stdbool.h>
#include "zbm_dev_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Инициализация базы данных GUID
 */
void zbm_guid_db_init(void);

// ===================================================================
// === ATTRIBUTES ====================================================
// ===================================================================

/**
 * @brief Зарегистрировать атрибут в системе GUID
 */
bool zbm_guid_db_register(zbm_cluster_attribute_t** pp_attr,
                          uint16_t short_addr,
                          uint8_t endpoint,
                          uint16_t cluster_id,
                          uint16_t attr_id,
                          const char* custom_guid);

/**
 * @brief Найти атрибут по GUID
 */
zbm_cluster_attribute_t* zbm_find_attr_by_guid(const char* guid);

/**
 * @brief Найти атрибут по составному ключу
 */
zbm_cluster_attribute_t* zbm_find_attr_by_key(uint16_t short_addr,
                                              uint8_t endpoint,
                                              uint16_t cluster_id,
                                              uint16_t attr_id);

/**
 * @brief Удалить запись по GUID
 */
void zbm_guid_db_unregister_by_guid(const char* guid);

/**
 * @brief Удалить запись по указателю на атрибут
 */
void zbm_guid_db_unregister_by_attr_ptr(zbm_cluster_attribute_t* attr);

/**
 * @brief Удалить все записи в GUID DB, связанные с устройством по short_addr
 */
bool zbm_guid_db_unregister_by_short_addr(uint16_t short_addr);

/**
 * @brief Перестраивает все записи в GUID DB, обновляя указатели на атрибуты
 */
void zbm_guid_db_refresh_all_attr_ptrs(void);

void zbm_guid_db_update_cluster_attr_ptrs(uint16_t short_addr,
                                              uint8_t endpoint_id,
                                              uint16_t cluster_id,
                                              zbm_cluster_attribute_t** attr_array,
                                              uint8_t attr_count);

void zbm_guid_db_update_custom_cluster_attr_ptrs(uint16_t short_addr,
                                                     uint8_t endpoint_id,
                                                     uint16_t cluster_id,
                                                     zbm_cluster_attribute_t** attr_array,
                                                     uint8_t attr_count);

// ===================================================================
// === COMMANDS & REPORTS ============================================
// ===================================================================

/**
 * @brief Зарегистрировать стандартную команду
 */
bool zbm_guid_db_register_cmd(zbm_cluster_standart_cmd_t** pp_cmd,
                              uint16_t short_addr,
                              uint8_t endpoint,
                              uint16_t cluster_id,
                              uint8_t cmd_id,
                              const char* custom_guid);

/**
 * @brief Найти стандартную команду по GUID
 */
zbm_cluster_standart_cmd_t* zbm_find_cmd_by_guid(const char* guid);

/**
 * @brief Найти стандартную команду по ключу
 */
zbm_cluster_standart_cmd_t* zbm_find_cmd_by_key(uint16_t short_addr,
                                                uint8_t endpoint,
                                                uint16_t cluster_id,
                                                uint8_t cmd_id);

/**
 * @brief Удалить команду по GUID
 */
void zbm_guid_db_unregister_cmd_by_guid(const char* guid);

/**
 * @brief Удалить команду по указателю
 */
void zbm_guid_db_unregister_cmd_by_ptr(zbm_cluster_standart_cmd_t* cmd);

// === Custom Report Commands (Tuya 0xFD и др.) ======================

/**
 * @brief Зарегистрировать кастомный репорт (например, Tuya DP)
 */
bool zbm_guid_db_register_custom_report(zbm_cluster_custom_report_cmd_t** pp_report,
                                        uint16_t short_addr,
                                        uint8_t endpoint,
                                        uint16_t cluster_id,
                                        uint8_t cmd_id,
                                        const char* custom_guid);

/**
 * @brief Найти кастомный репорт по GUID
 */
zbm_cluster_custom_report_cmd_t* zbm_find_custom_report_by_guid(const char* guid);

/**
 * @brief Найти кастомный репорт по ключу
 */
zbm_cluster_custom_report_cmd_t* zbm_find_custom_report_by_key(uint16_t short_addr,
                                                               uint8_t endpoint,
                                                               uint16_t cluster_id,
                                                               uint8_t cmd_id);

/**
 * @brief Удалить кастомный репорт по GUID
 */
void zbm_guid_db_unregister_custom_report_by_guid(const char* guid);

/**
 * @brief Удалить кастомный репорт по указателю
 */
void zbm_guid_db_unregister_custom_report_by_ptr(zbm_cluster_custom_report_cmd_t* report);

/**
 * @brief Удалить все команды и репорты устройства
 */
//void zbm_guid_db_unregister_cmds_and_reports_by_short_addr(uint16_t short_addr);

void zbm_guid_db_update_custom_report_ptrs(uint16_t short_addr,
                                               uint8_t endpoint_id,
                                               uint16_t cluster_id,
                                               zbm_cluster_custom_report_cmd_t** report_array,
                                               uint8_t report_count);

void zbm_guid_db_update_cluster_cmd_ptrs(uint16_t short_addr,
                                             uint8_t endpoint_id,
                                             uint16_t cluster_id,
                                             zbm_cluster_standart_cmd_t** cmd_array,
                                             uint8_t cmd_count);

// Обновляет все GUID устройств в базе данных в том числе записи в zbm_guid_db
bool zbm_guid_db_update_device_guids(zbm_dev_t* dev);

void zbm_guid_db_dump_all(void);

#ifdef __cplusplus
}
#endif

#endif // ZBM_GUID_DB_H