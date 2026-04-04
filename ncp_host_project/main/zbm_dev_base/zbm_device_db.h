#ifndef ZBM_DEVICE_DB_H
#define ZBM_DEVICE_DB_H

#include "zbm_dev_types.h"

#define ZBM_DEVICE_HASH_SIZE 256

typedef struct dev_node {
    zbm_dev_t* dev;
    struct dev_node* next;
} dev_node_t;


void zbm_device_db_init(void);

bool zbm_device_add_to_devdb(zbm_dev_t* dev);

zbm_dev_t* zbm_find_device_in_devdb_by_short(uint16_t short_addr);

zbm_dev_t* zbm_find_device_in_devdb_by_ieee(const uint8_t* ieee_addr);


/**
 * @brief Удаляет устройство из базы по указателю
 * @param dev Указатель на устройство
 * @return true если успешно удалено
 */
bool zbm_remove_device_from_devdb_by_dev(zbm_dev_t* dev);

bool zbm_remove_device_from_devdb_by_short(uint16_t short_addr);

bool zbm_remove_device_from_devdb_by_ieee(const uint8_t* ieee_addr);

/**
 * @brief Функция обратного вызова для перебора устройств
 * @param dev Указатель на устройство
 * @param ctx Пользовательский контекст
 */
typedef void (*zbm_device_visitor_t)(zbm_dev_t* dev, void* ctx);

/**
 * @brief Обходит все устройства в базе
 * @param visitor Функция, вызываемая для каждого устройства
 * @param ctx Контекст (может быть NULL)
 */
void zbm_device_db_foreach(zbm_device_visitor_t visitor, void* ctx);


/**
 * @brief Возвращает количество устройств в базе
 * @return Число активных устройств
 */
size_t zbm_device_db_count(void);

size_t zbm_device_db_count_safe(void);

#endif