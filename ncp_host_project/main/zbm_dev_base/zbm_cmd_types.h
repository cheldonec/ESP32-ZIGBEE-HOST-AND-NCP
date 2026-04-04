#ifndef ZBM_CMD_TYPES_H

#define ZBM_CMD_TYPES_H
#include <stdint.h>
#include <stdbool.h>
#include "zbm_low_level_types.h"
/**
 * @brief Типы данных для полезной нагрузки команд (payload)
 *
 * Используется при описании структуры payload в кастомных командах,
 * например, Tuya-specific команды (0xFD, 0xFE).
 */
typedef enum {
    ZBM_CMD_DATA_TYPE_NULL               = 0x00U, /*!< Null data type */
    ZBM_CMD_DATA_TYPE_T8BIT              = 0x08U, /*!< 8-bit value */
    ZBM_CMD_DATA_TYPE_T16BIT             = 0x09U, /*!< 16-bit value */
    ZBM_CMD_DATA_TYPE_T24BIT             = 0x0aU, /*!< 24-bit value */
    ZBM_CMD_DATA_TYPE_T32BIT             = 0x0bU, /*!< 32-bit value */
    ZBM_CMD_DATA_TYPE_T40BIT             = 0x0cU, /*!< 40-bit value */
    ZBM_CMD_DATA_TYPE_T48BIT             = 0x0dU, /*!< 48-bit value */
    ZBM_CMD_DATA_TYPE_T56BIT             = 0x0eU, /*!< 56-bit value */
    ZBM_CMD_DATA_TYPE_T64BIT             = 0x0fU, /*!< 64-bit value */
    ZBM_CMD_DATA_TYPE_BOOL               = 0x10U, /*!< Boolean (0/1) */
    ZBM_CMD_DATA_TYPE_T8BITMAP           = 0x18U, /*!< 8-bit bitmap */
    ZBM_CMD_DATA_TYPE_T16BITMAP          = 0x19U, /*!< 16-bit bitmap */
    ZBM_CMD_DATA_TYPE_T24BITMAP          = 0x1aU, /*!< 24-bit bitmap */
    ZBM_CMD_DATA_TYPE_T32BITMAP          = 0x1bU, /*!< 32-bit bitmap */
    ZBM_CMD_DATA_TYPE_T40BITMAP          = 0x1cU, /*!< 40-bit bitmap */
    ZBM_CMD_DATA_TYPE_T48BITMAP          = 0x1dU, /*!< 48-bit bitmap */
    ZBM_CMD_DATA_TYPE_T56BITMAP          = 0x1eU, /*!< 56-bit bitmap */
    ZBM_CMD_DATA_TYPE_T64BITMAP          = 0x1fU, /*!< 64-bit bitmap */
    ZBM_CMD_DATA_TYPE_U8                 = 0x20U, /*!< Unsigned 8-bit */
    ZBM_CMD_DATA_TYPE_U16                = 0x21U, /*!< Unsigned 16-bit */
    ZBM_CMD_DATA_TYPE_U24                = 0x22U, /*!< Unsigned 24-bit */
    ZBM_CMD_DATA_TYPE_U32                = 0x23U, /*!< Unsigned 32-bit */
    ZBM_CMD_DATA_TYPE_U40                = 0x24U, /*!< Unsigned 40-bit */
    ZBM_CMD_DATA_TYPE_U48                = 0x25U, /*!< Unsigned 48-bit */
    ZBM_CMD_DATA_TYPE_U56                = 0x26U, /*!< Unsigned 56-bit */
    ZBM_CMD_DATA_TYPE_U64                = 0x27U, /*!< Unsigned 64-bit */
    ZBM_CMD_DATA_TYPE_S8                 = 0x28U, /*!< Signed 8-bit */
    ZBM_CMD_DATA_TYPE_S16                = 0x29U, /*!< Signed 16-bit */
    ZBM_CMD_DATA_TYPE_S24                = 0x2aU, /*!< Signed 24-bit */
    ZBM_CMD_DATA_TYPE_S32                = 0x2bU, /*!< Signed 32-bit */
    ZBM_CMD_DATA_TYPE_S40                = 0x2cU, /*!< Signed 40-bit */
    ZBM_CMD_DATA_TYPE_S48                = 0x2dU, /*!< Signed 48-bit */
    ZBM_CMD_DATA_TYPE_S56                = 0x2eU, /*!< Signed 56-bit */
    ZBM_CMD_DATA_TYPE_S64                = 0x2fU, /*!< Signed 64-bit */
    ZBM_CMD_DATA_TYPE_T8BIT_ENUM         = 0x30U, /*!< 8-bit enumeration */
    ZBM_CMD_DATA_TYPE_T16BIT_ENUM        = 0x31U, /*!< 16-bit enumeration */
    ZBM_CMD_DATA_TYPE_SEMI               = 0x38U, /*!< 2-byte float (semi precision) */
    ZBM_CMD_DATA_TYPE_SINGLE             = 0x39U, /*!< 4-byte float (IEEE 754) */
    ZBM_CMD_DATA_TYPE_DOUBLE             = 0x3aU, /*!< 8-byte float (IEEE 754) */
    ZBM_CMD_DATA_TYPE_OCTET_STRING       = 0x41U, /*!< Octet string */
    ZBM_CMD_DATA_TYPE_CHAR_STRING        = 0x42U, /*!< Character string */
    ZBM_CMD_DATA_TYPE_LONG_OCTET_STRING  = 0x43U, /*!< Long octet string */
    ZBM_CMD_DATA_TYPE_LONG_CHAR_STRING   = 0x44U, /*!< Long character string */
    ZBM_CMD_DATA_TYPE_ARRAY              = 0x48U, /*!< Array (8-bit length) */
    ZBM_CMD_DATA_TYPE_T16BIT_ARRAY       = 0x49U, /*!< Array with 16-bit length */
    ZBM_CMD_DATA_TYPE_T32BIT_ARRAY       = 0x4aU, /*!< Array with 32-bit length */
    ZBM_CMD_DATA_TYPE_STRUCTURE          = 0x4cU, /*!< Structure */
    ZBM_CMD_DATA_TYPE_SET                = 0x50U, /*!< Set collection */
    ZBM_CMD_DATA_TYPE_BAG                = 0x51U, /*!< Bag collection */
    ZBM_CMD_DATA_TYPE_TIME_OF_DAY        = 0xe0U, /*!< Time of day (4 bytes) */
    ZBM_CMD_DATA_TYPE_DATE               = 0xe1U, /*!< Date (4 bytes) */
    ZBM_CMD_DATA_TYPE_UTC_TIME           = 0xe2U, /*!< UTC Time (4 bytes) */
    ZBM_CMD_DATA_TYPE_CLUSTER_ID         = 0xe8U, /*!< Cluster ID (2 bytes) */
    ZBM_CMD_DATA_TYPE_ATTRIBUTE_ID       = 0xe9U, /*!< Attribute ID (2 bytes) */
    ZBM_CMD_DATA_TYPE_BACNET_OID         = 0xeaU, /*!< BACnet OID (4 bytes) */
    ZBM_CMD_DATA_TYPE_IEEE_ADDR          = 0xf0U, /*!< IEEE address (8 bytes) */
    ZBM_CMD_DATA_TYPE_S128_BIT_KEY       = 0xf1U, /*!< 128-bit security key */
    ZBM_CMD_DATA_TYPE_INVALID            = 0xffU, /*!< Invalid type */
} zbm_cmd_data_types_t;


// параметры кластерной команды
typedef struct zbm_cluster_cmd_param_s {
    char*                                   friendlyname; 
    zbm_cmd_data_types_t                    data_type;       /*!< Attribute type see zcl_attr_type */
    uint16_t                                data_size;
    void*                                   p_value;
}zbm_cluster_cmd_param_t;

// кластерная команда
typedef struct zbm_cluster_standart_cmd_s {
    uint8_t                                 id;
    char*                                   friendlyname; 
    uint8_t                                 param_count;
    zbm_cluster_cmd_param_t**               params;
    char                                    guid[64];
}zbm_cluster_standart_cmd_t;

// нестандартный репорт в стандартный кластер например TUYA 0xfd
typedef struct zbm_cluster_custom_report_cmd_s {
    uint8_t                                 id;
    char*                                   friendlyname;  // генерится на лету только для JSON
    zbm_cmd_data_types_t                    data_type;
    uint16_t                                data_size;
    void*                                   p_value;
    char                                    guid[64];
}zbm_cluster_custom_report_cmd_t;

bool zbm_free_cluster_cmd_param(zbm_cluster_cmd_param_t* param);

//простая очистка стандартной кластерной команды
bool zbm_free_cluster_standart_cmd(zbm_cluster_standart_cmd_t* cmd);

//простая очистка нестандартной инфо_команды кластера еапример 0xfd от TUYA
bool zbm_free_cluster_custom_report_cmd(zbm_cluster_custom_report_cmd_t* report_cmd);

//=========================== Общая для стандартных команд ====================
/**
 * @brief Создаёт массив стандартных команд для указанного кластера
 * @param[in] cluster_id ID кластера
 * @param[in] role_mask Маска ролей кластера
 * @param[out] count Указатель, куда запишется количество команд
 * @return Массив zbm_cluster_standart_cmd_t* или NULL
 */
zbm_cluster_standart_cmd_t** zbm_create_standard_command_array(uint16_t cluster_id, zbm_cluster_role_t role_mask, uint8_t* count);

#endif // ZBM_CMD_TYPES_H