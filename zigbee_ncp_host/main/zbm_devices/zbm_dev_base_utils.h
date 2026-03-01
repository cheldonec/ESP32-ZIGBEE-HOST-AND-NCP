#ifndef ZBM_DEV_BASE_UTILS_H

#define ZBM_DEV_BASE_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include "ncp_host_zb_api_zdo.h"
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
uint16_t zbm_dev_base_extract_manuf_code_from_ieee(const uint8_t ieee_addr[8]);

/**
 * @brief Получить имя производителя по IEEE-адресу
 * @param ieee_addr IEEE-адрес
 * @return имя или "unknown"
 */
const char* zbm_dev_base_extract_manuf_name_from_ieee(const uint8_t ieee_addr[8]);

/**
 * @brief Определить manufacturer_code с приоритетом:
 * 1. node_desc → 2. basic.manufacturer_name → 3. OUI
 * @param ieee_addr IEEE-адрес устройства
 * @param node_desc Указатель на node_desc (может быть NULL)
 * @param basic_manufacturer_name Строка из Basic Cluster (может быть NULL)
 * @return manufacturer_code или 0
 */
uint16_t zbm_base_get_manuf_code_by_priority(
    const uint8_t ieee_addr[8],
    const void *node_desc,                    // Указатель на esp_zb_af_node_desc_t
    const char *basic_manufacturer_name);

const char* zbm_extract_manufacturer_name_by_code(uint16_t manuf_code);

const char* zbm_dev_base_extract_device_type_name_from_device_id(uint16_t device_id);

void ieee_to_str(char* out, const esp_zb_ieee_addr_t addr);

bool str_to_ieee(const char* str, esp_zb_ieee_addr_t addr);

int ieee_addr_compare(esp_zb_ieee_addr_t *a, esp_zb_ieee_addr_t *b);

uint64_t hexstr_to_uint64(const char* hex_str);

#endif


