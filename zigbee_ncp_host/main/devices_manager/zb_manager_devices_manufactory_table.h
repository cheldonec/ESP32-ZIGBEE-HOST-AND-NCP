// zb_manager_devices_manufactory_table.h

#ifndef ZB_MANAGER_DEVICES_MANUFACTORY_TABLE_H
#define ZB_MANAGER_DEVICES_MANUFACTORY_TABLE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct local_esp_zb_af_node_desc_s {
    uint16_t          node_desc_flags;            /*!< node description */
    uint8_t           mac_capability_flags;       /*!< mac capability */
    uint16_t          manufacturer_code;          /*!< Manufacturer code */
    uint8_t           max_buf_size;               /*!< Maximum buffer size */
    uint16_t          max_incoming_transfer_size; /*!< Maximum incoming transfer size */
    uint16_t          server_mask;                /*!< Server mask */
    uint16_t          max_outgoing_transfer_size; /*!< Maximum outgoing transfer size */
    uint8_t           desc_capability_field;      /*!< Descriptor capability field */
} __attribute__ ((packed)) local_esp_zb_af_node_desc_t;

/**
 * @brief Запись в таблице соответствия OUI → manufacturer_code
 */
typedef struct {
    uint8_t oui[3];           // Первые 3 байта IEEE-адреса (байты 5,6,7)
    uint16_t manuf_code;      // manufacturer_code
    const char *name;         // Имя производителя
} ieee_manuf_entry_t;

/**
 * @brief Глобальная таблица OUI → manuf_code
 */
extern const ieee_manuf_entry_t ieee_manuf_table[];

/**
 * @brief Определить manufacturer_code по IEEE-адресу (OUI)
 * @param ieee_addr IEEE-адрес (8 байт)
 * @return manuf_code или 0, если не найден
 */
uint16_t zb_manager_guess_manufacturer_code(const uint8_t ieee_addr[8]);

/**
 * @brief Получить имя производителя по IEEE-адресу
 * @param ieee_addr IEEE-адрес
 * @return имя или "unknown"
 */
const char* zb_manager_get_manufacturer_name(const uint8_t ieee_addr[8]);

/**
 * @brief Определить manufacturer_code с приоритетом:
 * 1. node_desc → 2. basic.manufacturer_name → 3. OUI
 * @param ieee_addr IEEE-адрес устройства
 * @param node_desc Указатель на node_desc (может быть NULL)
 * @param basic_manufacturer_name Строка из Basic Cluster (может быть NULL)
 * @return manufacturer_code или 0
 */
uint16_t zb_manager_resolve_manufacturer_code(
    const uint8_t ieee_addr[8],
    const void *node_desc,                    // Указатель на esp_zb_af_node_desc_t
    const char *basic_manufacturer_name);

const char* zb_manager_get_manufacturer_name_by_code(uint16_t manuf_code);
#ifdef __cplusplus
}
#endif

#endif // ZB_MANAGER_DEVICES_MANUFACTORY_TABLE_H
