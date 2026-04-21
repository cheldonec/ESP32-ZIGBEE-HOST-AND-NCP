#ifndef ZBM_ATTR_TYPES_H

#define ZBM_ATTR_TYPES_H
#include <stdint.h>
#include <stdbool.h>
#include "zbm_low_level_types.h"

typedef enum {
    ZBM_ATTR_TYPE_TNULL               = 0x00U,        /*!< Null data type */
    ZBM_ATTR_TYPE_T8BIT               = 0x08U,        /*!< 8-bit value data type */
    ZBM_ATTR_TYPE_T16BIT              = 0x09U,        /*!< 16-bit value data type */
    ZBM_ATTR_TYPE_T24BIT              = 0x0aU,        /*!< 24-bit value data type */
    ZBM_ATTR_TYPE_T32BIT              = 0x0bU,        /*!< 32-bit value data type */
    ZBM_ATTR_TYPE_T40BIT              = 0x0cU,        /*!< 40-bit value data type */
    ZBM_ATTR_TYPE_T48BIT              = 0x0dU,        /*!< 48-bit value data type */
    ZBM_ATTR_TYPE_T56BIT              = 0x0eU,        /*!< 56-bit value data type */
    ZBM_ATTR_TYPE_T64BIT              = 0x0fU,        /*!< 64-bit value data type */
    ZBM_ATTR_TYPE_BOOL                = 0x10U,        /*!< Boolean data type */
    ZBM_ATTR_TYPE_T8BITMAP            = 0x18U,        /*!< 8-bit bitmap data type */
    ZBM_ATTR_TYPE_T16BITMAP           = 0x19U,        /*!< 16-bit bitmap data type */
    ZBM_ATTR_TYPE_T24BITMAP           = 0x1aU,        /*!< 24-bit bitmap data type */
    ZBM_ATTR_TYPE_T32BITMAP           = 0x1bU,        /*!< 32-bit bitmap data type */
    ZBM_ATTR_TYPE_T40BITMAP           = 0x1cU,        /*!< 40-bit bitmap data type */
    ZBM_ATTR_TYPE_T48BITMAP           = 0x1dU,        /*!< 48-bit bitmap data type */
    ZBM_ATTR_TYPE_T56BITMAP           = 0x1eU,        /*!< 56-bit bitmap data type */
    ZBM_ATTR_TYPE_T64BITMAP           = 0x1fU,        /*!< 64-bit bitmap data type */
    ZBM_ATTR_TYPE_U8                  = 0x20U,        /*!< Unsigned 8-bit value data type */
    ZBM_ATTR_TYPE_U16                 = 0x21U,        /*!< Unsigned 16-bit value data type */
    ZBM_ATTR_TYPE_U24                 = 0x22U,        /*!< Unsigned 24-bit value data type */
    ZBM_ATTR_TYPE_U32                 = 0x23U,        /*!< Unsigned 32-bit value data type */
    ZBM_ATTR_TYPE_U40                 = 0x24U,        /*!< Unsigned 40-bit value data type */
    ZBM_ATTR_TYPE_U48                 = 0x25U,        /*!< Unsigned 48-bit value data type */
    ZBM_ATTR_TYPE_U56                 = 0x26U,        /*!< Unsigned 56-bit value data type */
    ZBM_ATTR_TYPE_U64                 = 0x27U,        /*!< Unsigned 64-bit value data type */
    ZBM_ATTR_TYPE_S8                  = 0x28U,        /*!< Signed 8-bit value data type */
    ZBM_ATTR_TYPE_S16                 = 0x29U,        /*!< Signed 16-bit value data type */
    ZBM_ATTR_TYPE_S24                 = 0x2aU,        /*!< Signed 24-bit value data type */
    ZBM_ATTR_TYPE_S32                 = 0x2bU,        /*!< Signed 32-bit value data type */
    ZBM_ATTR_TYPE_S40                 = 0x2cU,        /*!< Signed 40-bit value data type */
    ZBM_ATTR_TYPE_S48                 = 0x2dU,        /*!< Signed 48-bit value data type */
    ZBM_ATTR_TYPE_S56                 = 0x2eU,        /*!< Signed 56-bit value data type */
    ZBM_ATTR_TYPE_S64                 = 0x2fU,        /*!< Signed 64-bit value data type */
    ZBM_ATTR_TYPE_T8BIT_ENUM          = 0x30U,        /*!< 8-bit enumeration (U8 discrete) data type */
    ZBM_ATTR_TYPE_T16BIT_ENUM         = 0x31U,        /*!< 16-bit enumeration (U16 discrete) data type */
    ZBM_ATTR_TYPE_SEMI                = 0x38U,        /*!< 2 byte floating point */
    ZBM_ATTR_TYPE_SINGLE              = 0x39U,        /*!< 4 byte floating point */
    ZBM_ATTR_TYPE_DOUBLE              = 0x3aU,        /*!< 8 byte floating point */
    ZBM_ATTR_TYPE_OCTET_STRING        = 0x41U,        /*!< Octet string data type */
    ZBM_ATTR_TYPE_CHAR_STRING         = 0x42U,        /*!< Character string (array) data type */
    ZBM_ATTR_TYPE_LONG_OCTET_STRING   = 0x43U,        /*!< Long octet string */
    ZBM_ATTR_TYPE_LONG_CHAR_STRING    = 0x44U,        /*!< Long character string */
    ZBM_ATTR_TYPE_ARRAY               = 0x48U,        /*!< Array data with 8bit type, size = 2 + sum of content len */
    ZBM_ATTR_TYPE_T16BIT_ARRAY        = 0x49U,        /*!< Array data with 16bit type, size = 2 + sum of content len */
    ZBM_ATTR_TYPE_T32BIT_ARRAY        = 0x4aU,        /*!< Array data with 32bit type, size = 2 + sum of content len */
    ZBM_ATTR_TYPE_STRUCTURE           = 0x4cU,        /*!< Structure data type 2 + sum of content len */
    ZBM_ATTR_TYPE_SET                 = 0x50U,        /*!< Collection:set, size = sum of len of content */
    ZBM_ATTR_TYPE_BAG                 = 0x51U,        /*!< Collection:bag, size = sum of len of content */
    ZBM_ATTR_TYPE_TIME_OF_DAY         = 0xe0U,        /*!< Time of day, 4 bytes */
    ZBM_ATTR_TYPE_DATE                = 0xe1U,        /*!< Date, 4 bytes */
    ZBM_ATTR_TYPE_UTC_TIME            = 0xe2U,        /*!< UTC Time, 4 bytes */
    ZBM_ATTR_TYPE_CLUSTER_ID          = 0xe8U,        /*!< Cluster ID, 2 bytes */
    ZBM_ATTR_TYPE_ATTRIBUTE_ID        = 0xe9U,        /*!< Attribute ID, 2 bytes */
    ZBM_ATTR_TYPE_BACNET_OID          = 0xeaU,        /*!< BACnet OID, 4 bytes */
    ZBM_ATTR_TYPE_IEEE_ADDR           = 0xf0U,        /*!< IEEE address (U64) type */
    ZBM_ATTR_TYPE_S128_BIT_KEY        = 0xf1U,        /*!< 128-bit security key */
    ZBM_ATTR_TYPE_INVALID             = 0xffU,        /*!< Invalid data type */
} zbm_attr_data_types_t;

typedef struct zbm_cluster_attribute_s {
    uint16_t                                id;
    char*                                   friendlyname; 
    uint8_t                                 acces;
    uint64_t                                last_update_ms;
    zbm_attr_data_types_t                   data_type;
    uint16_t                                data_size;
    void*                                   p_value;
    char                                    guid[64];

    // === Поведение (реакция) ===
    char                                    behavior_id[37];   // UUID модуля поведения
    bool                                    behavior_enabled;  // включено ли поведение
} zbm_cluster_attribute_t;

void zbm_generate_attr_guid(char* out_guid, uint8_t len,uint16_t short_addr, uint8_t endpoint,uint16_t cluster_id, uint16_t attr_id);

/**
 * @brief Создаёт новый атрибут (выделяет память, дублирует имя, инициализирует поля)
 * @note Реализована в zbm_attr_factory.c
 */
zbm_cluster_attribute_t* create_attr(uint16_t id, const char* name, zbm_attr_data_types_t type, uint16_t size);


/**
 * @brief простая очистка атрибута
 * @note Реализована в zbm_attr_factory.c
 */
bool zbm_free_cluster_attribute(zbm_cluster_attribute_t* attr);

/**
 * @brief Создаёт массив стандартных атрибутов для указанного кластера
 * @param[in] cluster_id ID кластера
 * @param[out] role_mask Маска ролей, для которых нужно создать атрибуты
 * @param[out] count Указатель, куда запишется количество атрибутов
 * @return Массив zbm_cluster_attribute_t* или NULL
 */
zbm_cluster_attribute_t** zbm_create_standard_attribute_array(uint16_t cluster_id, zbm_cluster_role_t role_mask, uint8_t* count);

/**
 * @brief Проверяет, допустим ли размер данных для указанного типа
 * @param type Тип атрибута
 * @param size Размер данных в байтах
 * @return true, если совместимо
 */
bool zbm_is_valid_data_size(zbm_attr_data_types_t type, uint16_t size);

/**
 * @brief Возвращает рекомендуемый размер (в байтах) для указанного типа атрибута.
 *        Используется при создании виртуальных переменных и атрибутов по умолчанию.
 * 
 * @param type Тип данных из zbm_attr_data_types_t
 * @return uint16_t Рекомендуемый размер в байтах
 */
uint16_t zbm_get_attr_size(zbm_attr_data_types_t type);

const char* zbm_attr_type_to_str(zbm_attr_data_types_t type);

#endif