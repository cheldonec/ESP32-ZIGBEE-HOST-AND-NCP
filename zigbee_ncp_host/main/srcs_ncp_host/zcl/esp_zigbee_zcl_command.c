/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "ncp_host_zb_api.h"

#include "esp_zigbee_zcl_command.h"
#include "esp_log.h"
#include "zb_manager_clusters.h"
#include "zb_manager_ncp_host.h"

static const char *TAG = "ESP_ZB_ZCL_COMMAND_C";

uint8_t esp_zb_zcl_custom_cluster_cmd_req(esp_zb_zcl_custom_cluster_cmd_t *cmd_req)
{
    typedef struct {
        esp_zb_zcl_basic_cmd_t zcl_basic_cmd;                   /*!< Basic command info */
        uint8_t  address_mode;                                  /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
        uint16_t profile_id;                                    /*!< Profile id */
        uint16_t cluster_id;                                    /*!< Cluster id */
        uint16_t custom_cmd_id;                                 /*!< Custom command id */
        uint8_t  direction;                                     /*!< Direction of command */
        uint8_t  type;                                          /*!< The type of attribute, which can refer to esp_zb_zcl_attr_type_t */
        uint16_t size;                                          /*!< The value size of attribute  */
    } ESP_ZNSP_ZB_PACKED_STRUCT esp_host_zb_zcl_data_t;

    uint16_t data_len = sizeof(esp_host_zb_zcl_data_t);
    uint8_t *data = NULL;
    esp_host_zb_zcl_data_t zcl_data = {
        .zcl_basic_cmd = cmd_req->zcl_basic_cmd,
        .address_mode = cmd_req->address_mode,
        .profile_id = cmd_req->profile_id,
        .cluster_id = cmd_req->cluster_id,
        .custom_cmd_id = cmd_req->custom_cmd_id,
        .direction = cmd_req->direction,
        .type = cmd_req->data.type,
        .size = cmd_req->data.size,
    };

    switch (cmd_req->data.type) {
        case ESP_ZB_ZCL_ATTR_TYPE_NULL:
            break;
        case ESP_ZB_ZCL_ATTR_TYPE_U32:
            zcl_data.size = 4;
        case ESP_ZB_ZCL_ATTR_TYPE_U40:
            zcl_data.size = 5;
        case ESP_ZB_ZCL_ATTR_TYPE_BOOL:
        case ESP_ZB_ZCL_ATTR_TYPE_8BITMAP:
        case ESP_ZB_ZCL_ATTR_TYPE_U8:
            zcl_data.size = 1;
            break;
        case ESP_ZB_ZCL_ATTR_TYPE_16BIT_ARRAY:
            uint16_t el_count = *(uint16_t*)cmd_req->data.value;
            //zcl_data.size = el_count * sizeof(uint16_t);
            ESP_LOGW(TAG, "el_count: %d data_size %d", el_count,zcl_data.size );
            break;
        default:
            break;
    }

    data = calloc(1, data_len + zcl_data.size);
    memcpy(data, &zcl_data, data_len);
    if (cmd_req->data.value && zcl_data.size) {
        memcpy(data + data_len, cmd_req->data.value, zcl_data.size);
        data_len += zcl_data.size;
    }

    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    if (zigbee_ncp_module_state == WORKING)
        {
            esp_host_zb_output(ESP_NCP_ZCL_WRITE_CMD, data, data_len, &output, &outlen);
        }
    if (data) {
        free(data);
        data = NULL;
    }

    //return ESP_OK;
    return output;
}

uint8_t zb_manager_zcl_read_attr_cmd_req(esp_zb_zcl_read_attr_cmd_t *cmd_req)
{
    ESP_LOGI(TAG, "Try to Read ATTR");
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);
    typedef struct {
        esp_zb_zcl_basic_cmd_t  zcl_basic_cmd;      /*!< Basic command info */
        uint8_t                 address_mode;       /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
        uint16_t                cluster_id;         /*!< Cluster ID to read */
        uint8_t                 attr_number;        /*!< Number of attribute in the attr_field */
        uint8_t                 flags;
        uint16_t                manuf_code;
    } __attribute__ ((packed)) esp_host_zb_read_attr_t;

    esp_host_zb_read_attr_t req ={
        .zcl_basic_cmd = {
            .dst_addr_u   = cmd_req->zcl_basic_cmd.dst_addr_u,
            //.dst_addr_u   = cmd_req->zcl_basic_cmd.dst_addr_u,
            .dst_endpoint = cmd_req->zcl_basic_cmd.dst_endpoint,
            .src_endpoint = cmd_req->zcl_basic_cmd.src_endpoint,
        },
        .address_mode = cmd_req->address_mode,
        .cluster_id   = cmd_req->clusterID,
        .attr_number  = cmd_req->attr_number,
        .flags = (cmd_req->manuf_specific & 0x03) |
                 ((cmd_req->direction & 0x01) << 2) |
                 ((cmd_req->dis_defalut_resp & 0x01) << 3),
        .manuf_code = cmd_req->manuf_code,
    };

    /*if (req.address_mode == ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT) {
        memcpy(&req.zcl_basic_cmd.dst_addr_u.addr_long, &cmd_req->zcl_basic_cmd.dst_addr_u.addr_long, 8);
    } else req.zcl_basic_cmd.dst_addr_u.addr_short = cmd_req->zcl_basic_cmd.dst_addr_u.addr_short;*/

    uint16_t attr_list_size = cmd_req->attr_number * sizeof(uint16_t);
    uint16_t inlen = sizeof(esp_host_zb_read_attr_t) + attr_list_size ;
    uint8_t  *input = calloc(1, inlen);
    if (!input) {
        ESP_LOGE(TAG, "Failed to allocate input buffer");
        return 0xFF;
    }
    
    memcpy(input, &req, sizeof(esp_host_zb_read_attr_t));
    memcpy(input + sizeof(esp_host_zb_read_attr_t), cmd_req->attr_field,cmd_req->attr_number * sizeof(uint16_t));
    esp_err_t err = ESP_FAIL;
    if (zigbee_ncp_module_state == WORKING)
        {
            err = esp_host_zb_output(ESP_NCP_ZCL_ATTR_READ_CMD, input, inlen, &output, &outlen);
        }
    free(input);
    input = NULL;

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send read_attr_cmd_req");
        return 0xFF;
    }
    
    ESP_LOGI(TAG, "Read Attr Req sent (TSN: %d) to 0x%04x, EP: %d, Cluster: 0x%04x, Attr Count: %d",
             output,
             req.zcl_basic_cmd.dst_addr_u.addr_short,
             req.zcl_basic_cmd.dst_endpoint,
             req.cluster_id,
             req.attr_number);
    //return ESP_OK;
    return output;
}

void zb_manager_free_read_attr_resp_attr_array(zb_manager_cmd_read_attr_resp_message_t* resp)
{
    if (!resp) return;

    if (resp->attr_arr) {
        for (int i = 0; i < resp->attr_count; i++) {
            if (resp->attr_arr[i].attr_value) {
                free(resp->attr_arr[i].attr_value);
                resp->attr_arr[i].attr_value = NULL;
            }
        }
        free(resp->attr_arr);
        resp->attr_arr = NULL;
        resp->attr_count = 0;
    }
}

void zb_manager_free_report_attr_resp(zb_manager_cmd_report_attr_resp_message_t* resp)
{
    if (!resp) return;
    if (resp->attr.attr_value != NULL)
    {
        free(resp->attr.attr_value);
        resp->attr.attr_value = NULL;
    }
}

uint16_t zb_manager_get_attr_data_size(esp_zb_zcl_attr_type_t attr_type, void* attr_data)
{
    if (!attr_data) {
        return 0;
    }

    // === Строки: OCTET_STRING, CHAR_STRING и их long-версии ===
    if (attr_type == ESP_ZB_ZCL_ATTR_TYPE_OCTET_STRING ||
        attr_type == ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING ||
        attr_type == ESP_ZB_ZCL_ATTR_TYPE_LONG_OCTET_STRING ||
        attr_type == ESP_ZB_ZCL_ATTR_TYPE_LONG_CHAR_STRING)
    {
        zbstring_t *zbstr = (zbstring_t *)attr_data;
        return (zbstr->len + 1); // +1 — байт длины
    }

    // === ARRAY, SET, BAG ===
    if (attr_type == ESP_ZB_ZCL_ATTR_TYPE_ARRAY ||
        attr_type == ESP_ZB_ZCL_ATTR_TYPE_SET ||
        attr_type == ESP_ZB_ZCL_ATTR_TYPE_BAG)
    {
        uint8_t *data = (uint8_t*)attr_data;
        uint8_t element_type = data[0]; // тип элементов
        uint8_t count = data[1];        // количество
        uint16_t total_size = 2;        // счётчик + тип

        // Упрощённо: предполагаем, что все элементы одного типа и фиксированного размера
        uint8_t item_size = 0;

        // Определим размер одного элемента, если он фиксированный
        switch (element_type) {
            case ESP_ZB_ZCL_ATTR_TYPE_BOOL:
            case ESP_ZB_ZCL_ATTR_TYPE_8BIT:
            case ESP_ZB_ZCL_ATTR_TYPE_U8:
            case ESP_ZB_ZCL_ATTR_TYPE_S8:
            case ESP_ZB_ZCL_ATTR_TYPE_8BITMAP:
            case ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM:
                item_size = 1; break;
            case ESP_ZB_ZCL_ATTR_TYPE_16BIT:
            case ESP_ZB_ZCL_ATTR_TYPE_U16:
            case ESP_ZB_ZCL_ATTR_TYPE_S16:
            case ESP_ZB_ZCL_ATTR_TYPE_16BITMAP:
            case ESP_ZB_ZCL_ATTR_TYPE_16BIT_ENUM:
            case ESP_ZB_ZCL_ATTR_TYPE_SEMI:
            case ESP_ZB_ZCL_ATTR_TYPE_CLUSTER_ID:
            case ESP_ZB_ZCL_ATTR_TYPE_ATTRIBUTE_ID:
                item_size = 2; break;
            case ESP_ZB_ZCL_ATTR_TYPE_24BIT:
            case ESP_ZB_ZCL_ATTR_TYPE_U24:
            case ESP_ZB_ZCL_ATTR_TYPE_S24:
                item_size = 3; break;
            case ESP_ZB_ZCL_ATTR_TYPE_32BIT:
            case ESP_ZB_ZCL_ATTR_TYPE_U32:
            case ESP_ZB_ZCL_ATTR_TYPE_S32:
            case ESP_ZB_ZCL_ATTR_TYPE_32BITMAP:
            case ESP_ZB_ZCL_ATTR_TYPE_SINGLE:
            case ESP_ZB_ZCL_ATTR_TYPE_TIME_OF_DAY:
            case ESP_ZB_ZCL_ATTR_TYPE_DATE:
            case ESP_ZB_ZCL_ATTR_TYPE_UTC_TIME:
            case ESP_ZB_ZCL_ATTR_TYPE_BACNET_OID:
                item_size = 4; break;
            case ESP_ZB_ZCL_ATTR_TYPE_40BIT:
            case ESP_ZB_ZCL_ATTR_TYPE_U40:
            case ESP_ZB_ZCL_ATTR_TYPE_S40:
                item_size = 5; break;
            case ESP_ZB_ZCL_ATTR_TYPE_48BIT:
            case ESP_ZB_ZCL_ATTR_TYPE_U48:
            case ESP_ZB_ZCL_ATTR_TYPE_S48:
                item_size = 6; break;
            case ESP_ZB_ZCL_ATTR_TYPE_56BIT:
            case ESP_ZB_ZCL_ATTR_TYPE_U56:
            case ESP_ZB_ZCL_ATTR_TYPE_S56:
                item_size = 7; break;
            case ESP_ZB_ZCL_ATTR_TYPE_64BIT:
            case ESP_ZB_ZCL_ATTR_TYPE_U64:
            case ESP_ZB_ZCL_ATTR_TYPE_S64:
            case ESP_ZB_ZCL_ATTR_TYPE_64BITMAP:
            case ESP_ZB_ZCL_ATTR_TYPE_DOUBLE:
            case ESP_ZB_ZCL_ATTR_TYPE_IEEE_ADDR:
                item_size = 8; break;
            case ESP_ZB_ZCL_ATTR_TYPE_128_BIT_KEY:
                item_size = 16; break;
            default:
                // Для строк и других сложных типов — упрощённо считаем 1 байт
                item_size = 1;
                break;
        }

        total_size += item_size * count;
        return total_size;
    }

    // === STRUCTURE ===
    if (attr_type == ESP_ZB_ZCL_ATTR_TYPE_STRUCTURE)
    {
        uint8_t *data = (uint8_t*)attr_data;
        uint8_t field_count = data[0]; // количество полей
        uint16_t total_size = 1;      // счётчик
        uint8_t *ptr = data + 1;

        for (int i = 0; i < field_count; i++) {
            if (ptr + 2 > (uint8_t*)attr_data + 128) break; // защита от переполнения

            uint8_t tag = ptr[0];
            uint8_t type = ptr[1];
            ptr += 2;

            uint8_t field_size = 0;
            switch (type) {
                case ESP_ZB_ZCL_ATTR_TYPE_BOOL:
                case ESP_ZB_ZCL_ATTR_TYPE_8BIT:
                case ESP_ZB_ZCL_ATTR_TYPE_U8:
                case ESP_ZB_ZCL_ATTR_TYPE_S8:
                case ESP_ZB_ZCL_ATTR_TYPE_8BITMAP:
                case ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM:
                    field_size = 1; break;
                case ESP_ZB_ZCL_ATTR_TYPE_16BIT:
                case ESP_ZB_ZCL_ATTR_TYPE_U16:
                case ESP_ZB_ZCL_ATTR_TYPE_S16:
                case ESP_ZB_ZCL_ATTR_TYPE_16BITMAP:
                case ESP_ZB_ZCL_ATTR_TYPE_16BIT_ENUM:
                case ESP_ZB_ZCL_ATTR_TYPE_SEMI:
                case ESP_ZB_ZCL_ATTR_TYPE_CLUSTER_ID:
                case ESP_ZB_ZCL_ATTR_TYPE_ATTRIBUTE_ID:
                    field_size = 2; break;
                case ESP_ZB_ZCL_ATTR_TYPE_24BIT:
                case ESP_ZB_ZCL_ATTR_TYPE_U24:
                case ESP_ZB_ZCL_ATTR_TYPE_S24:
                    field_size = 3; break;
                case ESP_ZB_ZCL_ATTR_TYPE_32BIT:
                case ESP_ZB_ZCL_ATTR_TYPE_U32:
                case ESP_ZB_ZCL_ATTR_TYPE_S32:
                case ESP_ZB_ZCL_ATTR_TYPE_32BITMAP:
                case ESP_ZB_ZCL_ATTR_TYPE_SINGLE:
                case ESP_ZB_ZCL_ATTR_TYPE_TIME_OF_DAY:
                case ESP_ZB_ZCL_ATTR_TYPE_DATE:
                case ESP_ZB_ZCL_ATTR_TYPE_UTC_TIME:
                case ESP_ZB_ZCL_ATTR_TYPE_BACNET_OID:
                    field_size = 4; break;
                case ESP_ZB_ZCL_ATTR_TYPE_64BIT:
                case ESP_ZB_ZCL_ATTR_TYPE_U64:
                case ESP_ZB_ZCL_ATTR_TYPE_S64:
                case ESP_ZB_ZCL_ATTR_TYPE_DOUBLE:
                case ESP_ZB_ZCL_ATTR_TYPE_IEEE_ADDR:
                    field_size = 8; break;
                case ESP_ZB_ZCL_ATTR_TYPE_128_BIT_KEY:
                    field_size = 16; break;
                default:
                    field_size = 1; break;
            }
            total_size += 2 + field_size; // tag + type + value
            ptr += field_size;
        }

        return total_size;
    }

    // === 16BIT_ARRAY, 32BIT_ARRAY — специфичны, но редки. Упрощённо: считаем, что это массив 16-битных значений ===
    if (attr_type == ESP_ZB_ZCL_ATTR_TYPE_16BIT_ARRAY)
    {
        uint8_t *data = (uint8_t*)attr_data;
        uint8_t count = data[0]; // количество элементов
        return 1 + count * 2;    // count + значения (по 2 байта)
    }

    if (attr_type == ESP_ZB_ZCL_ATTR_TYPE_32BIT_ARRAY)
    {
        uint8_t *data = (uint8_t*)attr_data;
        uint8_t count = data[0];
        return 1 + count * 4;
    }

    // === Остальные типы — по таблице ===
    switch (attr_type) {
        case ESP_ZB_ZCL_ATTR_TYPE_NULL:             return 0;
        case ESP_ZB_ZCL_ATTR_TYPE_8BIT:             return 1;
        case ESP_ZB_ZCL_ATTR_TYPE_16BIT:            return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_24BIT:            return 3;
        case ESP_ZB_ZCL_ATTR_TYPE_32BIT:            return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_40BIT:            return 5;
        case ESP_ZB_ZCL_ATTR_TYPE_48BIT:            return 6;
        case ESP_ZB_ZCL_ATTR_TYPE_56BIT:            return 7;
        case ESP_ZB_ZCL_ATTR_TYPE_64BIT:            return 8;
        case ESP_ZB_ZCL_ATTR_TYPE_BOOL:             return 1;
        case ESP_ZB_ZCL_ATTR_TYPE_8BITMAP:          return 1;
        case ESP_ZB_ZCL_ATTR_TYPE_16BITMAP:         return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_24BITMAP:         return 3;
        case ESP_ZB_ZCL_ATTR_TYPE_32BITMAP:         return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_40BITMAP:         return 5;
        case ESP_ZB_ZCL_ATTR_TYPE_48BITMAP:         return 6;
        case ESP_ZB_ZCL_ATTR_TYPE_56BITMAP:         return 7;
        case ESP_ZB_ZCL_ATTR_TYPE_64BITMAP:         return 8;
        case ESP_ZB_ZCL_ATTR_TYPE_U8:               return 1;
        case ESP_ZB_ZCL_ATTR_TYPE_U16:              return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_U24:              return 3;
        case ESP_ZB_ZCL_ATTR_TYPE_U32:              return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_U40:              return 5;
        case ESP_ZB_ZCL_ATTR_TYPE_U48:              return 6;
        case ESP_ZB_ZCL_ATTR_TYPE_U56:              return 7;
        case ESP_ZB_ZCL_ATTR_TYPE_U64:              return 8;
        case ESP_ZB_ZCL_ATTR_TYPE_S8:               return 1;
        case ESP_ZB_ZCL_ATTR_TYPE_S16:              return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_S24:              return 3;
        case ESP_ZB_ZCL_ATTR_TYPE_S32:              return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_S40:              return 5;
        case ESP_ZB_ZCL_ATTR_TYPE_S48:              return 6;
        case ESP_ZB_ZCL_ATTR_TYPE_S56:              return 7;
        case ESP_ZB_ZCL_ATTR_TYPE_S64:              return 8;
        case ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM:        return 1;
        case ESP_ZB_ZCL_ATTR_TYPE_16BIT_ENUM:       return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_SEMI:             return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_SINGLE:           return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_DOUBLE:           return 8;
        // Строки — обработаны выше
        case ESP_ZB_ZCL_ATTR_TYPE_TIME_OF_DAY:      return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_DATE:             return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_UTC_TIME:         return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_CLUSTER_ID:       return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_ATTRIBUTE_ID:     return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_BACNET_OID:       return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_IEEE_ADDR:        return 8;
        case ESP_ZB_ZCL_ATTR_TYPE_128_BIT_KEY:      return 16;
        case ESP_ZB_ZCL_ATTR_TYPE_INVALID:          return 0;
        default:                                    return 0xFFFF;
    }
}

// ВНУТРИ esp_zigbee_zcl_command.c

uint16_t zb_manager_get_zcl_attr_size(esp_zb_zcl_attr_type_t attr_type)
{
    switch (attr_type) {
        case ESP_ZB_ZCL_ATTR_TYPE_BOOL:
        case ESP_ZB_ZCL_ATTR_TYPE_8BIT:
        case ESP_ZB_ZCL_ATTR_TYPE_U8:
        case ESP_ZB_ZCL_ATTR_TYPE_S8:
        case ESP_ZB_ZCL_ATTR_TYPE_8BITMAP:
        case ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM:
            return 1;
        case ESP_ZB_ZCL_ATTR_TYPE_16BIT:
        case ESP_ZB_ZCL_ATTR_TYPE_U16:
        case ESP_ZB_ZCL_ATTR_TYPE_S16:
        case ESP_ZB_ZCL_ATTR_TYPE_16BITMAP:
        case ESP_ZB_ZCL_ATTR_TYPE_16BIT_ENUM:
        case ESP_ZB_ZCL_ATTR_TYPE_SEMI:
        case ESP_ZB_ZCL_ATTR_TYPE_CLUSTER_ID:
        case ESP_ZB_ZCL_ATTR_TYPE_ATTRIBUTE_ID:
            return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_24BIT:
        case ESP_ZB_ZCL_ATTR_TYPE_U24:
        case ESP_ZB_ZCL_ATTR_TYPE_S24:
            return 3;
        case ESP_ZB_ZCL_ATTR_TYPE_32BIT:
        case ESP_ZB_ZCL_ATTR_TYPE_U32:
        case ESP_ZB_ZCL_ATTR_TYPE_S32:
        case ESP_ZB_ZCL_ATTR_TYPE_32BITMAP:
        case ESP_ZB_ZCL_ATTR_TYPE_SINGLE:
        case ESP_ZB_ZCL_ATTR_TYPE_TIME_OF_DAY:
        case ESP_ZB_ZCL_ATTR_TYPE_DATE:
        case ESP_ZB_ZCL_ATTR_TYPE_UTC_TIME:
        case ESP_ZB_ZCL_ATTR_TYPE_BACNET_OID:
            return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_40BIT:
        case ESP_ZB_ZCL_ATTR_TYPE_U40:
        case ESP_ZB_ZCL_ATTR_TYPE_S40:
            return 5;
        case ESP_ZB_ZCL_ATTR_TYPE_48BIT:
        case ESP_ZB_ZCL_ATTR_TYPE_U48:
        case ESP_ZB_ZCL_ATTR_TYPE_S48:
            return 6;
        case ESP_ZB_ZCL_ATTR_TYPE_56BIT:
        case ESP_ZB_ZCL_ATTR_TYPE_U56:
        case ESP_ZB_ZCL_ATTR_TYPE_S56:
            return 7;
        case ESP_ZB_ZCL_ATTR_TYPE_64BIT:
        case ESP_ZB_ZCL_ATTR_TYPE_U64:
        case ESP_ZB_ZCL_ATTR_TYPE_S64:
        case ESP_ZB_ZCL_ATTR_TYPE_64BITMAP:
        case ESP_ZB_ZCL_ATTR_TYPE_DOUBLE:
        case ESP_ZB_ZCL_ATTR_TYPE_IEEE_ADDR:
            return 8;
        case ESP_ZB_ZCL_ATTR_TYPE_128_BIT_KEY:
            return 16;
        default:
            return 1; // fallback
    }
}


//                                  === REPORTING CONFIG ===
esp_err_t zb_manager_reporting_config_req(esp_zb_zcl_config_report_cmd_t *cmd_req)
{
    ESP_LOGI(TAG, "Sending Config Report command");
    typedef struct {
        uint8_t  addr_mode;         // ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT или 64
        uint8_t  dst_endpoint;
        uint8_t  src_endpoint;
        uint16_t clusterID;
        uint8_t  flags;             // manuf, dir, dis_def_resp
        uint16_t manuf_code;
        uint16_t record_number;

        // Union: безопасно, packed
        union {
            uint16_t short_addr;
            uint8_t  ieee_addr[8];
        } addr;
    } __attribute__((packed)) esp_host_zb_config_report_req_t;

    // Проверка размера — критически важно!
    _Static_assert(sizeof(esp_host_zb_config_report_req_t) == 18,
                "esp_host_zb_config_report_req_t must be 18 bytes");

    if (!cmd_req || !cmd_req->record_field || cmd_req->record_number == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // === Создаём упакованную структуру ===
    esp_host_zb_config_report_req_t req = {0}; // обнуляем, чтобы ieee_addr не был мусором

    req.addr_mode = cmd_req->address_mode;
    req.dst_endpoint = cmd_req->zcl_basic_cmd.dst_endpoint;
    req.src_endpoint = cmd_req->zcl_basic_cmd.src_endpoint;
    req.clusterID = cmd_req->clusterID;
    req.flags = (cmd_req->manuf_specific & 0x03) |
                ((cmd_req->direction & 0x01) << 2) |
                ((cmd_req->dis_default_resp & 0x01) << 3);
    req.manuf_code = cmd_req->manuf_code;
    req.record_number = cmd_req->record_number;

    // Заполняем адрес в зависимости от mode
    if (cmd_req->address_mode == ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT ||
        cmd_req->address_mode == ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT) {
        req.addr.short_addr = cmd_req->zcl_basic_cmd.dst_addr_u.addr_short;
    } else if (cmd_req->address_mode == ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT) {
        memcpy(req.addr.ieee_addr, cmd_req->zcl_basic_cmd.dst_addr_u.addr_long, 8);
    } else {
        ESP_LOGE(TAG, "Unsupported address mode: %d", cmd_req->address_mode);
        return ESP_ERR_INVALID_ARG;
    }

    // === Подсчёт размера записей ===
    size_t records_size = 0;
    for (int i = 0; i < cmd_req->record_number; i++) {
        const esp_zb_zcl_config_report_record_t *rec = &cmd_req->record_field[i];
        records_size += 1 + 2 + 1 + 2 + 2; // direction, attrID, type, min, max
        if (rec->reportable_change && rec->min_interval > 0) {
            records_size += zb_manager_get_zcl_attr_size(rec->attrType);
        }
    }

    uint16_t inlen = sizeof(req) + records_size;
    uint8_t *input = calloc(1, inlen);
    if (!input) {
        ESP_LOGE(TAG, "Failed to allocate input buffer");
        return ESP_ERR_NO_MEM;
    }

    uint8_t *ptr = input;
    memcpy(ptr, &req, sizeof(req));
    ptr += sizeof(req);

    // === Копируем записи ===
    for (int i = 0; i < cmd_req->record_number; i++) {
        const esp_zb_zcl_config_report_record_t *rec = &cmd_req->record_field[i];

        *ptr++ = rec->direction;
        memcpy(ptr, &rec->attributeID, 2); ptr += 2;
        *ptr++ = rec->attrType;
        memcpy(ptr, &rec->min_interval, 2); ptr += 2;
        memcpy(ptr, &rec->max_interval, 2); ptr += 2;

        if (rec->reportable_change && rec->min_interval > 0) {
            uint8_t rcv_len = zb_manager_get_zcl_attr_size(rec->attrType);
            memcpy(ptr, rec->reportable_change, rcv_len);
            ptr += rcv_len;
        }

        ESP_LOGW(TAG, "Sent record %d: attr=0x%04x, type=0x%02x, min=%d, max=%d",
                 i, rec->attributeID, rec->attrType, rec->min_interval, rec->max_interval);
    }

    uint8_t output = 0;
    uint16_t outlen = sizeof(output);
    
    esp_err_t err = ESP_FAIL;
    if (zigbee_ncp_module_state == WORKING)
        {
            err = esp_host_zb_output(ZB_MANAGER_REPORT_CONFIG_CMD, input, inlen, &output, &outlen);
        }
    free(input);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Config Report sent: TSN=%d, dev=0x%04x, ep=%d, cluster=0x%04x, records=%d",
                 output,
                 cmd_req->zcl_basic_cmd.dst_addr_u.addr_short,
                 cmd_req->zcl_basic_cmd.dst_endpoint,
                 cmd_req->clusterID,
                 cmd_req->record_number);
    } else {
        ESP_LOGE(TAG, "Failed to send Config Report");
    }

    return err;
}


esp_err_t zb_manager_configure_reporting_temperature(uint16_t short_addr, uint8_t endpoint)
{
    uint8_t delta_buf[2];
    int16_t delta = 10; // 0.1°C
    memcpy(delta_buf, &delta, 2);

    esp_zb_zcl_config_report_record_t record = {
        .direction = ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
        .attributeID = 0x0010, // MinMeasuredValue (workaround для Tuya)
        .attrType = ESP_ZB_ZCL_ATTR_TYPE_S16,
        .min_interval = 60,
        .max_interval = 300,
        .max_interval = 300,
        .reportable_change = delta_buf,
    };

    esp_zb_zcl_config_report_cmd_t cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = short_addr,
            .dst_endpoint = endpoint,
            .src_endpoint = endpoint,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .dis_default_resp = 1,
        .manuf_code = 0,
        .record_number = 1,
        .record_field = &record,
    };

    esp_err_t err = zb_manager_reporting_config_req(&cmd);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "🌡️ Reporting configured for Temperature (0x%04x)", short_addr);
    } else {
        ESP_LOGW(TAG, "🌡️ Failed to configure temperature reporting for 0x%04x", short_addr);
    }

    return err;
}

esp_err_t zb_manager_configure_reporting_humidity(uint16_t short_addr, uint8_t endpoint)
{
    uint8_t delta_buf[2];
    uint16_t delta = 50; // 0.5%
    memcpy(delta_buf, &delta, 2);

    esp_zb_zcl_config_report_record_t record = {
        .direction = ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
        .attributeID = ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, // 0x0000
        .attrType = ESP_ZB_ZCL_ATTR_TYPE_U16,
        .min_interval = 180,
        .max_interval = 600,
        .reportable_change = delta_buf,
    };

    esp_zb_zcl_config_report_cmd_t cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = short_addr,
            .dst_endpoint = endpoint,
            .src_endpoint = endpoint,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .dis_default_resp = 1,
        .manuf_code = 0,
        .record_number = 1,
        .record_field = &record,
    };

    esp_err_t err = zb_manager_reporting_config_req(&cmd);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "💧 Reporting configured for Humidity (0x%04x)", short_addr);
    } else {
        ESP_LOGE(TAG, "💧 Failed to configure humidity reporting for 0x%04x", short_addr);
    }

    return err;
}

esp_err_t zb_manager_configure_reporting_power(uint16_t short_addr, uint8_t endpoint)
{
    uint8_t delta_buf[1] = {2}; // 1% (0.5% per unit)

    esp_zb_zcl_config_report_record_t record = {
        .direction = ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
        .attributeID = ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, // 0x0021
        .attrType = ESP_ZB_ZCL_ATTR_TYPE_U8,
        .min_interval = 60,
        .max_interval = 120,
        .reportable_change = delta_buf,
    };

    esp_zb_zcl_config_report_cmd_t cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = short_addr,
            .dst_endpoint = endpoint,
            .src_endpoint = endpoint,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .dis_default_resp = 1,
        .manuf_code = 0,
        .record_number = 1,
        .record_field = &record,
    };

    esp_err_t err = zb_manager_reporting_config_req(&cmd);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "🔋 Reporting configured for Power (0x%04x)", short_addr);
    } else {
        ESP_LOGW(TAG, "🔋 Power reporting failed — device may use Tuya-specific attribute");
    }

    return err;
}

esp_err_t zb_manager_configure_reporting_onoff(uint16_t short_addr, uint8_t endpoint)
{
    esp_zb_zcl_config_report_record_t record = {
        .direction = ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
        .attributeID = ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, // 0x0000
        .attrType = ESP_ZB_ZCL_ATTR_TYPE_BOOL,
        .min_interval = 1,
        .max_interval = 300,
        .reportable_change = NULL, // не нужен для bool
    };

    esp_zb_zcl_config_report_cmd_t cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = short_addr,
            .dst_endpoint = endpoint,
            .src_endpoint = endpoint,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .dis_default_resp = 1,
        .manuf_code = 0,
        .record_number = 1,
        .record_field = &record,
    };

    esp_err_t err = zb_manager_reporting_config_req(&cmd);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "💡 Reporting configured for OnOff (0x%04x)", short_addr);
    } else {
        ESP_LOGW(TAG, "💡 Failed to configure OnOff reporting for 0x%04x", short_addr);
    }

    return err;
}
void zb_manager_free_report_config_resp(zb_manager_cmd_report_config_resp_message_t *msg)
{
    if (msg) {
        if (msg->attr_list) {
            free(msg->attr_list);
            msg->attr_list = NULL;
        }
        //free(msg); сам msg очистится в обработчике event
    }
}

void zb_manager_free_custom_cluster_report_message(zb_manager_custom_cluster_report_message_t *msg)
{
    if (msg) {
        if (msg->data) {
            free(msg->data);
            msg->data = NULL;
        }
        //free(msg); сам msg очистится в обработчике event
    }
}

