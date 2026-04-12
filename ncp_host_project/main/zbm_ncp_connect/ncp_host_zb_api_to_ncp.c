#include "ncp_host_zb_api_to_ncp.h"
#include "ncp_host_zb_api.h"
//#include "zbm_ncp_connect.h"
#include "zbm_zigbee_structures.h"
#include "string.h"
#include "esp_log.h"
#include "esp_err.h"


static const char* TAG = "NCP_HOST_ZB_API_TO_NCP";

esp_err_t zbm_to_ncp_cmd_init_zigbee_stack(void)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ZB_MANAGER_INIT_CMD, NULL, 0, &output, &outlen);

    return output;
}

esp_err_t zbm_to_ncp_cmd_register_endpoint(esp_host_zb_endpoint_t *endpoint)
{
    typedef struct {
        uint8_t     endpoint;                               /*!< The application endpoint to be added */
        uint16_t    profileId;                              /*!< The endpoint's application profile */
        uint16_t    deviceId;                               /*!< The endpoint's device ID within the application profile */
        uint8_t     appFlags;                               /*!< The device version and flags indicating description availability */
        uint8_t     inputClusterCount;                      /*!< The number of cluster IDs in inputClusterList */
        uint8_t     outputClusterCount;                     /*!< The number of cluster IDs in outputClusterList */
    } __attribute__ ((packed)) esp_endpoint_t;
 
    uint16_t data_len = 0;
    uint16_t data_head_len = sizeof(esp_endpoint_t);
    uint16_t inputClusterLength = endpoint->inputClusterCount * sizeof(uint16_t);
    uint16_t outputClusterLength = endpoint->outputClusterCount * sizeof(uint16_t);
    uint8_t *input = calloc(1, data_head_len + inputClusterLength + outputClusterLength);

    if (input) {
        esp_endpoint_t esp_endpoint = {
            .endpoint = endpoint->endpoint,
            .profileId = endpoint->profileId,
            .deviceId = endpoint->deviceId,
            .appFlags = endpoint->appFlags,
            .inputClusterCount = endpoint->inputClusterCount,
            .outputClusterCount = endpoint->outputClusterCount,
        };
        data_len = data_head_len + inputClusterLength + outputClusterLength;
        memcpy(input, &esp_endpoint, data_head_len);
        if (inputClusterLength) {
            memcpy(input + data_head_len, endpoint->inputClusterList, inputClusterLength);

        }

        if (outputClusterLength) {
            memcpy(input + data_head_len + inputClusterLength, endpoint->outputClusterList, outputClusterLength);
        }
    }

    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ESP_NCP_ZCL_ENDPOINT_ADD, input, data_len, &output, &outlen);

    if (input) {
        free(input);
        input = NULL;
    }

    return ESP_OK;
}

esp_err_t zbm_to_ncp_cmd_start_zigbee_stack(void)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ZB_MANAGER_START_CMD, NULL, 0, &output, &outlen);

    return output;
}

esp_err_t zbm_to_ncp_req_get_coord_long_addr(esp_zb_ieee_addr_t ieee_addr)
{
    uint16_t outlen = sizeof(esp_zb_ieee_addr_t);
    return esp_host_zb_output(ESP_NCP_NETWORK_LONG_ADDRESS_GET, NULL, 0, ieee_addr, &outlen);
}

esp_err_t zbm_to_ncp_req_get_extended_pan_id(esp_zb_ieee_addr_t ext_pan_id)
{
    uint16_t outlen = sizeof(esp_zb_ieee_addr_t);
    return esp_host_zb_output(ESP_NCP_NETWORK_EXTENDED_PAN_ID_GET, NULL, 0, ext_pan_id, &outlen);
}

uint16_t zbm_to_ncp_req_get_pan_id(void)
{
    uint16_t output = 0;
    uint16_t outlen = sizeof(uint16_t);

    esp_host_zb_output(ESP_NCP_NETWORK_PAN_ID_GET, NULL, 0, &output, &outlen);

    return output;
}

uint8_t zbm_to_ncp_req_get_current_channel(void)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ESP_NCP_NETWORK_CHANNEL_GET, NULL, 0, &output, &outlen);

    return output;
}

uint16_t zbm_to_ncp_req_get_network_short_addr(void)
{
    uint16_t output = 0;
    uint16_t outlen = sizeof(uint16_t);
    
    esp_host_zb_output(ESP_NCP_NETWORK_SHORT_ADDRESS_GET, NULL, 0, &output, &outlen);

    return output;
}

esp_err_t zbm_to_ncp_cmd_open_zigbee_network(uint8_t seconds)
{
    uint8_t output = 0;                        // результат выполнения команды
    uint16_t outlen = sizeof(uint8_t);        // ожидаемый размер ответа
    uint16_t req_param_len = sizeof(uint8_t);        // размер запроса
    esp_host_zb_output(ZB_MANAGER_OPEN_NETWORK_CMD, &seconds, req_param_len, &output, &outlen);
    return output;
}

esp_err_t zbm_to_ncp_cmd_close_zigbee_network(void)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ZB_MANAGER_CLOSE_NETWORK_CMD, NULL, 0, &output, &outlen);

    return output;
}

esp_err_t zbm_to_ncp_cmd_get_local_long_addr(esp_zb_ieee_addr_t ieee_addr)
{
    
    uint16_t outlen = sizeof(esp_zb_ieee_addr_t);
    return esp_host_zb_output(ESP_NCP_NETWORK_LONG_ADDRESS_GET, NULL, 0, ieee_addr, &outlen);
}

uint8_t zbm_to_ncp_req_read_attributes(esp_zb_zcl_read_attr_cmd_t *cmd_req)
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

uint8_t zbm_to_ncp_req_send_zcl_cmd_to_cluster(zbm_send_zcl_cmd_to_cluster_cmd_t *cmd_req)
{
    ESP_LOGI(TAG, "Try to Send Cluster CMD");
    if (!cmd_req || !cmd_req->cmd_object) {
        ESP_LOGE(TAG, "Invalid command request or cmd_object");
        return 0xFF;
    }

    uint8_t tsn = 0;
    uint16_t outlen = sizeof(uint8_t);

    // === Упакованная структура для передачи на NCP ===
    typedef struct {
        esp_zb_zcl_basic_cmd_t  zcl_basic_cmd;
        uint8_t                 address_mode;
        uint8_t                 flags;                // <--- сюда положим объединённый Frame Control
        uint16_t                manuf_code;
        uint16_t                cluster_id;
        uint8_t                 cmd_id;
        uint8_t                 cmd_params_count;
        uint16_t                cmd_params_size;
    } __attribute__((packed)) zbm_zb_send_cluster_cmd_t;

    
    // === Считываем Frame Control как байт ===
    uint8_t flags = (cmd_req->frame_control.frame_type & 0x03) | ((cmd_req->frame_control.manuf_specific & 0x01) << 2) |
           ((cmd_req->frame_control.direction & 0x01) << 3) | ((cmd_req->frame_control.dis_defalut_resp & 0x01) << 4);
    
    uint16_t params_payload_size = 0;
    if (cmd_req->cmd_object->param_count > 0)
    {
        for (int i = 0; i < cmd_req->cmd_object->param_count; i++) {
        params_payload_size += sizeof(uint8_t) + sizeof(uint16_t) + cmd_req->cmd_object->params[i]->data_size;
        }
    }

    // === Заполняем заголовок ===
    zbm_zb_send_cluster_cmd_t req = {
        .zcl_basic_cmd = cmd_req->zcl_basic_cmd,
        .address_mode = cmd_req->address_mode,
        .cluster_id = cmd_req->clusterID,
        .flags = flags,  // <-- уже собран как uint8_t
        .manuf_code = cmd_req->manuf_code,
        .cmd_id = cmd_req->cmd_object->id,
        .cmd_params_count = cmd_req->cmd_object->param_count,
        .cmd_params_size = params_payload_size,
    };

    // === Формируем буфер [заголовок][TLV-параметры] ===
    uint16_t header_size = sizeof(zbm_zb_send_cluster_cmd_t);
    uint16_t inlen = header_size + params_payload_size;
    uint8_t *input = calloc(1, inlen);
    if (!input) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return 0xFF;
    }

    memcpy(input, &req, header_size);
    uint8_t *ptr = input + header_size;

    for (int i = 0; i < cmd_req->cmd_object->param_count; i++) {
        zbm_cluster_cmd_param_t *param = cmd_req->cmd_object->params[i];
        if (!param || !param->p_value) {
            free(input);
            ESP_LOGE(TAG, "Invalid parameter #%d", i);
            return 0xFF;
        }

        *ptr++ = (uint8_t)param->data_type;
        *(uint16_t*)ptr = param->data_size;
        ptr += 2;
        memcpy(ptr, param->p_value, param->data_size);
        ptr += param->data_size;
    }

    // === 🔹 HEX-ЛОГ НА ХОСТЕ: весь сериализованный буфер ===
    ESP_LOGI(TAG, "Serialized buffer to NCP (total %u bytes):", inlen);
    ESP_LOG_BUFFER_HEX_LEVEL("NCP_TX", input, inlen, ESP_LOG_INFO);

    
    // === Отправка на NCP ===
    esp_err_t err = esp_host_zb_output(
        ZB_MANAGER_SEND_ZCL_CMD_TO_CLUSTER_REQ,
        input,
        inlen,
        &tsn,
        &outlen
    );

    free(input);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send ZCL command to NCP");
        return 0xFF;
    }

    ESP_LOGI(TAG, "ZCL cmd 0x%02x sent (TSN=%d) to 0x%04x EP:%d",
             req.cmd_id,
             tsn,
             req.zcl_basic_cmd.dst_addr_u.addr_short,
             req.zcl_basic_cmd.dst_endpoint);

    return tsn;
}


uint8_t zbm_to_ncp_req_send_zcl_cmd_from_ws_json(cJSON *req_json)
{
    const char* TAG = "ZBM_NCP_JSON";

    cJSON *guid_obj = cJSON_GetObjectItem(req_json, "guid");
    cJSON *params_arr = cJSON_GetObjectItem(req_json, "params");

    if (!guid_obj || !cJSON_IsString(guid_obj)) {
        ESP_LOGE(TAG, "Missing or invalid 'guid'");
        return 0xFF;
    }

    // === 1. Найти команду по GUID ===
    zbm_cluster_standart_cmd_t *cmd_model = zbm_find_cmd_by_guid_safe(guid_obj->valuestring);
    if (!cmd_model) {
        ESP_LOGE(TAG, "Command not found by GUID: %s", guid_obj->valuestring);
        return 0xFF;
    }

    // === 2. Распарсить short_addr, ep, cluster_id из GUID ===
    uint16_t short_addr = 0;
    uint8_t endpoint_id = 0;
    uint16_t cluster_id = 0;
    uint8_t cmd_id = 0;

    int parsed = sscanf(guid_obj->valuestring, "0x%hx:%hhu:cmd:%hx:%hhx",
                        &short_addr, &endpoint_id, &cluster_id, &cmd_id);

    if (parsed != 4) {
        ESP_LOGE(TAG, "Failed to parse GUID: %s", guid_obj->valuestring);
        return 0xFF;
    }

    // === 3. Подготавливаем структуру для отправки ===
    zbm_send_zcl_cmd_to_cluster_cmd_t cmd_req = {0};

    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
    cmd_req.zcl_basic_cmd.dst_endpoint = endpoint_id;
    cmd_req.zcl_basic_cmd.src_endpoint = 1;  // можно сделать настраиваемым позже
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd_req.clusterID = cluster_id;
    cmd_req.manuf_code = 0x0000;  // пока нет поддержки
    cmd_req.frame_control.frame_type = 1;         // ZB_ZCL_FRAME_TYPE_CLUSTER_SPECIFIC
    cmd_req.frame_control.manuf_specific = 0;
    cmd_req.frame_control.direction = 0;
    cmd_req.frame_control.dis_defalut_resp = 1;
    cmd_req.cmd_object = cmd_model;

    // === 4. Обновляем параметры из JSON ===
    bool params_ok = true;
    if (cmd_model->param_count > 0) {
        if (!params_arr || !cJSON_IsArray(params_arr)) {
            ESP_LOGE(TAG, "Expected 'params' array for command with parameters");
            return 0xFF;
        }

        cJSON *param_item;
        int i = 0;
        cJSON_ArrayForEach(param_item, params_arr) {
            if (i >= cmd_model->param_count) break;

            zbm_cluster_cmd_param_t *param = cmd_model->params[i];
            if (!param) {
                ESP_LOGE(TAG, "Model param #%d is NULL", i);
                params_ok = false;
                break;
            }

            cJSON *value_obj = cJSON_GetObjectItem(param_item, "value");
            if (!value_obj) {
                ESP_LOGE(TAG, "Missing 'value' field in param #%d", i);
                params_ok = false;
                break;
            }

            void *new_value = NULL;
            uint16_t new_size = 0;

            switch (param->data_type) {
                case ZBM_CMD_DATA_TYPE_U8:
                case ZBM_CMD_DATA_TYPE_T8BIT:
                case ZBM_CMD_DATA_TYPE_T8BIT_ENUM:
                case ZBM_CMD_DATA_TYPE_BOOL:
                case ZBM_CMD_DATA_TYPE_T8BITMAP: {
                    double num_val = value_obj->valuedouble;
                    if (num_val < 0 || num_val > 255) {
                        ESP_LOGE(TAG, "Value out of range for uint8_t: %f", num_val);
                        params_ok = false;
                        break;
                    }
                    uint8_t val = (uint8_t)(num_val + 0.5);
                    new_size = 1;
                    new_value = malloc(new_size);
                    if (new_value) *(uint8_t*)new_value = val;
                    else params_ok = false;
                    break;
                }

                case ZBM_CMD_DATA_TYPE_U16:
                case ZBM_CMD_DATA_TYPE_T16BIT:
                case ZBM_CMD_DATA_TYPE_T16BIT_ENUM:
                case ZBM_CMD_DATA_TYPE_T16BITMAP:
                case ZBM_CMD_DATA_TYPE_CLUSTER_ID:
                case ZBM_CMD_DATA_TYPE_ATTRIBUTE_ID: {
                    double num_val = value_obj->valuedouble;
                    if (num_val < 0 || num_val > 65535) {
                        ESP_LOGE(TAG, "Value out of range for uint16_t: %f", num_val);
                        params_ok = false;
                        break;
                    }
                    uint16_t val = (uint16_t)(num_val + 0.5);
                    new_size = 2;
                    new_value = malloc(new_size);
                    if (new_value) *(uint16_t*)new_value = val;
                    else params_ok = false;
                    break;
                }

                case ZBM_CMD_DATA_TYPE_U32:
                case ZBM_CMD_DATA_TYPE_T32BIT:
                case ZBM_CMD_DATA_TYPE_T32BITMAP:
                case ZBM_CMD_DATA_TYPE_UTC_TIME:
                case ZBM_CMD_DATA_TYPE_TIME_OF_DAY:
                case ZBM_CMD_DATA_TYPE_DATE: {
                    uint32_t val = (uint32_t)value_obj->valuedouble;
                    new_size = 4;
                    new_value = malloc(new_size);
                    if (new_value) *(uint32_t*)new_value = val;
                    break;
                }

                case ZBM_CMD_DATA_TYPE_CHAR_STRING:
                case ZBM_CMD_DATA_TYPE_OCTET_STRING: {
                    if (!cJSON_IsString(value_obj)) {
                        ESP_LOGE(TAG, "Expected string for param #%d", i);
                        params_ok = false;
                        break;
                    }
                    const char *str = value_obj->valuestring;
                    new_size = strlen(str) + 1;
                    new_value = malloc(new_size);
                    if (new_value) strcpy((char*)new_value, str);
                    break;
                }

                default:
                    ESP_LOGW(TAG, "Unsupported param type 0x%02x", param->data_type);
                    params_ok = false;
                    break;
            }

            if (new_value && params_ok) {
                if (param->p_value) free(param->p_value);
                param->p_value = new_value;
                param->data_size = new_size;
            } else {
                params_ok = false;
            }

            i++;
        }
    }

    if (!params_ok) {
        ESP_LOGE(TAG, "Failed to process command parameters");
        return 0xFF;
    }

    // === 5. Отправляем через NCP ===
    uint8_t tsn = zbm_to_ncp_req_send_zcl_cmd_to_cluster(&cmd_req);

    ESP_LOGI(TAG, "ZCL cmd from JSON sent (TSN=%d) via GUID: %s", tsn, guid_obj->valuestring);
    return tsn;
}