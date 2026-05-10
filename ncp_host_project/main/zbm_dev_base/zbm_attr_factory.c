// File: main/zbm_dev_base/zbm_attr_factory.c
#include "zbm_clusters_type.h"
#include "zbm_attr_factory_on_off.h"
#include "zbm_attr_factory_basic.h"
#include "zbm_attr_factory_temperature_meas.h"
#include "zbm_attr_factory_humidity_meas.h"
#include "zbm_attr_types.h"
#include "zbm_guid_db.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ps_ram_utils.h"

//функция создания атрибута
zbm_cluster_attribute_t* create_attr(uint16_t id, const char* name, zbm_attr_data_types_t type, uint16_t size) {
    //zbm_cluster_attribute_t* attr = (zbm_cluster_attribute_t*)calloc(1, sizeof(zbm_cluster_attribute_t));
    zbm_cluster_attribute_t* attr = heap_caps_malloc(sizeof(zbm_cluster_attribute_t), MALLOC_CAP_SPIRAM);
    if (!attr) return NULL;

    attr->id = id;
    attr->data_type = type;
    attr->data_size = size;
    attr->acces = 0;
    attr->last_update_ms = 0;

    // Если имя задано — дублируем, иначе используем значение по умолчанию
    const char* friendly_name_str = name ? name : "unk_attr_name";
    attr->friendlyname = psram_strdup(friendly_name_str);
    if (!attr->friendlyname) {
        heap_caps_free(attr);
        attr = NULL;
        return NULL;
    }

    attr->p_value = calloc(1, size);
    if (!attr->p_value) {
        heap_caps_free(attr->friendlyname);
        attr->friendlyname = NULL;
        heap_caps_free(attr);
        attr = NULL;
        return NULL;
    }

    return attr;
}

// функция удаления атрибута
bool zbm_free_cluster_attribute(zbm_cluster_attribute_t* attr)
{
    if (!attr) return false;

    if (attr->friendlyname) {
        heap_caps_free(attr->friendlyname);
        attr->friendlyname = NULL;
    }

    if (attr->p_value) {
        heap_caps_free(attr->p_value);
        attr->p_value = NULL;
    }

    heap_caps_free(attr);
    return true;
}
/**
 * @brief Создаёт массив стандартных атрибутов для указанного кластера
 * @param[in] cluster_id ID кластера
 * @param[out] role_mask Маска ролей, для которых нужно создать атрибуты
 * @param[out] count Указатель, куда запишется количество атрибутов
 * @return Массив zbm_cluster_attribute_t* или NULL
 */
zbm_cluster_attribute_t** zbm_create_standard_attribute_array(uint16_t cluster_id, zbm_cluster_role_t role_mask, uint8_t* count) {
    if (!count) return NULL;
    *count = 0;

    switch (cluster_id) {
        case ZBM_CLUSTER_ID_ON_OFF:
            return zbm_create_on_off_attr_array(role_mask, count);

        case ZBM_CLUSTER_ID_BASIC:
            return zbm_create_basic_attr_array(role_mask, count);

        case ZBM_CLUSTER_ID_TEMP_MEASUREMENT:
            return zbm_create_temperature_meas_attr_array(role_mask, count);

        case ZBM_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT:
            return zbm_create_humidity_meas_attr_array(role_mask, count);

        default:
            *count = 0;
            return NULL;
    }
}

void zbm_generate_attr_guid(char* out_guid, uint8_t len,uint16_t short_addr, uint8_t endpoint,uint16_t cluster_id, uint16_t attr_id)
{
    snprintf(out_guid, len, "0x%04X:%d:%04X:%04X",short_addr, endpoint, cluster_id, attr_id);
}

bool zbm_is_valid_data_size(zbm_attr_data_types_t type, uint16_t size) {
    switch (type) {
        case ZBM_ATTR_TYPE_TNULL:
            return size == 0;
        case ZBM_ATTR_TYPE_T8BIT:
        case ZBM_ATTR_TYPE_BOOL:
        case ZBM_ATTR_TYPE_U8:
        case ZBM_ATTR_TYPE_S8:
        case ZBM_ATTR_TYPE_T8BIT_ENUM:
            return size == 1;

        case ZBM_ATTR_TYPE_T16BIT:
        case ZBM_ATTR_TYPE_U16:
        case ZBM_ATTR_TYPE_S16:
        case ZBM_ATTR_TYPE_T16BIT_ENUM:
        case ZBM_ATTR_TYPE_TIME_OF_DAY:
        case ZBM_ATTR_TYPE_DATE:
        case ZBM_ATTR_TYPE_UTC_TIME:
        case ZBM_ATTR_TYPE_CLUSTER_ID:
        case ZBM_ATTR_TYPE_ATTRIBUTE_ID:
            return size == 2;

        case ZBM_ATTR_TYPE_T24BIT:
        case ZBM_ATTR_TYPE_U24:
        case ZBM_ATTR_TYPE_S24:
            return size == 3;

        case ZBM_ATTR_TYPE_T32BIT:
        case ZBM_ATTR_TYPE_U32:
        case ZBM_ATTR_TYPE_S32:
        case ZBM_ATTR_TYPE_SEMI:
        case ZBM_ATTR_TYPE_SINGLE:
        case ZBM_ATTR_TYPE_BACNET_OID:
            return size == 4;

        case ZBM_ATTR_TYPE_T40BIT:
        case ZBM_ATTR_TYPE_U40:
        case ZBM_ATTR_TYPE_S40:
            return size == 5;

        case ZBM_ATTR_TYPE_T48BIT:
        case ZBM_ATTR_TYPE_U48:
        case ZBM_ATTR_TYPE_S48:
            return size == 6;

        case ZBM_ATTR_TYPE_T56BIT:
        case ZBM_ATTR_TYPE_U56:
        case ZBM_ATTR_TYPE_S56:
            return size == 7;

        case ZBM_ATTR_TYPE_T64BIT:
        case ZBM_ATTR_TYPE_U64:
        case ZBM_ATTR_TYPE_S64:
        case ZBM_ATTR_TYPE_DOUBLE:
        case ZBM_ATTR_TYPE_IEEE_ADDR:
        case ZBM_ATTR_TYPE_S128_BIT_KEY:
            return size == 8;

        case ZBM_ATTR_TYPE_OCTET_STRING:
        case ZBM_ATTR_TYPE_CHAR_STRING:
        case ZBM_ATTR_TYPE_LONG_OCTET_STRING:
        case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
        case ZBM_ATTR_TYPE_ARRAY:
        case ZBM_ATTR_TYPE_T16BIT_ARRAY:
        case ZBM_ATTR_TYPE_T32BIT_ARRAY:
        case ZBM_ATTR_TYPE_STRUCTURE:
        case ZBM_ATTR_TYPE_SET:
        case ZBM_ATTR_TYPE_BAG:
            // Variable size — any size is acceptable
            return size > 0;

        default:
            return false; // Unknown or invalid type
    }
}

/**
 * @brief Возвращает рекомендуемый размер (в байтах) для указанного типа атрибута.
 *        Используется при создании виртуальных переменных и атрибутов по умолчанию.
 * 
 * @param type Тип данных из zbm_attr_data_types_t
 * @return uint16_t Рекомендуемый размер в байтах
 */
uint16_t zbm_get_attr_size(zbm_attr_data_types_t type) {
    switch (type) {
        // 1 байт
        case ZBM_ATTR_TYPE_TNULL:
        case ZBM_ATTR_TYPE_T8BIT:
        case ZBM_ATTR_TYPE_BOOL:
        case ZBM_ATTR_TYPE_U8:
        case ZBM_ATTR_TYPE_S8:
        case ZBM_ATTR_TYPE_T8BIT_ENUM:
            return 1;

        // 2 байта
        case ZBM_ATTR_TYPE_T16BIT:
        case ZBM_ATTR_TYPE_U16:
        case ZBM_ATTR_TYPE_S16:
        case ZBM_ATTR_TYPE_T16BIT_ENUM:
        case ZBM_ATTR_TYPE_TIME_OF_DAY:
        case ZBM_ATTR_TYPE_DATE:
        case ZBM_ATTR_TYPE_UTC_TIME:
        case ZBM_ATTR_TYPE_CLUSTER_ID:
        case ZBM_ATTR_TYPE_ATTRIBUTE_ID:
            return 2;

        // 3 байта
        case ZBM_ATTR_TYPE_T24BIT:
        case ZBM_ATTR_TYPE_U24:
        case ZBM_ATTR_TYPE_S24:
            return 3;

        // 4 байта
        case ZBM_ATTR_TYPE_T32BIT:
        case ZBM_ATTR_TYPE_U32:
        case ZBM_ATTR_TYPE_S32:
        case ZBM_ATTR_TYPE_SEMI:
        case ZBM_ATTR_TYPE_SINGLE:
        case ZBM_ATTR_TYPE_BACNET_OID:
            return 4;

        // 5 байт
        case ZBM_ATTR_TYPE_T40BIT:
        case ZBM_ATTR_TYPE_U40:
        case ZBM_ATTR_TYPE_S40:
            return 5;

        // 6 байт
        case ZBM_ATTR_TYPE_T48BIT:
        case ZBM_ATTR_TYPE_U48:
        case ZBM_ATTR_TYPE_S48:
            return 6;

        // 7 байт
        case ZBM_ATTR_TYPE_T56BIT:
        case ZBM_ATTR_TYPE_U56:
        case ZBM_ATTR_TYPE_S56:
            return 7;

        // 8 байт
        case ZBM_ATTR_TYPE_T64BIT:
        case ZBM_ATTR_TYPE_U64:
        case ZBM_ATTR_TYPE_S64:
        case ZBM_ATTR_TYPE_DOUBLE:
        case ZBM_ATTR_TYPE_IEEE_ADDR:
        case ZBM_ATTR_TYPE_S128_BIT_KEY:
            return 8;

        // Строки: размер по умолчанию
        case ZBM_ATTR_TYPE_CHAR_STRING:
            return 32;  // стандартная строка до 32 символов
        case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
            return 64;  // длинная строка до 64 символов

        // Переменные типы (массивы, структуры) — минимум 1, но лучше задавать явно
        case ZBM_ATTR_TYPE_OCTET_STRING:
        case ZBM_ATTR_TYPE_LONG_OCTET_STRING:
        case ZBM_ATTR_TYPE_ARRAY:
        case ZBM_ATTR_TYPE_T16BIT_ARRAY:
        case ZBM_ATTR_TYPE_T32BIT_ARRAY:
        case ZBM_ATTR_TYPE_STRUCTURE:
        case ZBM_ATTR_TYPE_SET:
        case ZBM_ATTR_TYPE_BAG:
            return 1; // Зависит от контекста — здесь возвращаем минимальный

        default:
            return 1; // fallback
    }
}

const char* zbm_attr_type_to_str(zbm_attr_data_types_t type) {
    switch (type) {
        case ZBM_ATTR_TYPE_U8:           return "U8";
        case ZBM_ATTR_TYPE_S8:           return "S8";
        case ZBM_ATTR_TYPE_BOOL:         return "BOOL";
        case ZBM_ATTR_TYPE_U16:          return "U16";
        case ZBM_ATTR_TYPE_S16:          return "S16";
        case ZBM_ATTR_TYPE_CHAR_STRING:  return "CHAR_STR";
        case ZBM_ATTR_TYPE_LONG_CHAR_STRING: return "LONG_STR";
        default:                         return "UNKNOWN";
    }
}