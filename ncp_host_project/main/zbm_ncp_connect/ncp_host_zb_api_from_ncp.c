#include "ncp_host_zb_api_from_ncp.h"
#include "ncp_host_zb_api_to_ncp.h"
#include "ncp_host_zb_api.h"
#include "zbm_zigbee_app_signal_handler.h"
//#include "zbm_ncp_connect.h"
#include "zbm_zigbee_structures.h"
#include "esp_log.h"
#include "zbm_core_sync.h"
#include "zbm_web_server.h"
#include "string.h"

static const char* TAG = "NCP_HOST_ZB_API_FROM_NCP";
typedef struct {
    esp_zb_ieee_addr_t  extendedPanId;                      /*!< The network's extended PAN identifier */
    uint16_t            panId;                              /*!< The network's PAN identifier */
    uint8_t             radioChannel;                       /*!< A radio channel */
} esp_host_zb_network_t;

static esp_host_zb_network_t        s_host_zb_network;

static esp_err_t esp_host_zb_form_network_fn(const uint8_t *input, uint16_t inlen)
{
    typedef struct {
        esp_zb_ieee_addr_t  extendedPanId;                  /*!< The network's extended PAN identifier */
        uint16_t            panId;                          /*!< The network's PAN identifier */
        uint8_t             radioChannel;                   /*!< A radio channel */
    } ESP_ZNSP_ZB_PACKED_STRUCT esp_zb_form_network_t;

    esp_zb_form_network_t *form_network = (esp_zb_form_network_t *)input;
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_BDB_SIGNAL_FORMATION,
        .msg = NULL,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = ESP_OK,
    };

    memcpy(s_host_zb_network.extendedPanId, form_network->extendedPanId, sizeof(esp_zb_ieee_addr_t));
    s_host_zb_network.panId = form_network->panId;
    s_host_zb_network.radioChannel = form_network->radioChannel;

    zbm_zigbee_app_signal_handler(&app_signal);
    ESP_LOGI(TAG, " HEAP: %u bytes free, largest: %u", 
             esp_get_free_heap_size(), 
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return ESP_OK;
}

static esp_err_t esp_host_zb_permit_joining_fn(const uint8_t *input, uint16_t inlen)
{
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS,
        .msg = (const char *)input,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = ESP_OK,
    };

    zbm_zigbee_app_signal_handler(&app_signal);

    return ESP_OK;
}

static esp_err_t zb_manager_dev_assoc_event_fn(const uint8_t *input, uint16_t inlen)
{
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED,
        .msg = (const char *)input,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = ESP_OK,
    };

    zbm_zigbee_app_signal_handler(&app_signal);
    ESP_LOGI(TAG, " HEAP: %u bytes free, largest: %u", 
             esp_get_free_heap_size(), 
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return ESP_OK;
}

static esp_err_t zb_manager_dev_update_event_fn(const uint8_t *input, uint16_t inlen)
{
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE,
        .msg = (const char *)input,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = ESP_OK,
    };

    zbm_zigbee_app_signal_handler(&app_signal);
    ESP_LOGI(TAG, " HEAP: %u bytes free, largest: %u", 
             esp_get_free_heap_size(), 
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return ESP_OK;
}

static esp_err_t zb_manager_dev_annce_event_fn(const uint8_t *input, uint16_t inlen)
{
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE,
        .msg = (const char *)input,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = ESP_OK,
    };

    zbm_zigbee_app_signal_handler(&app_signal);
    ESP_LOGI(TAG, " HEAP: %u bytes free, largest: %u", 
             esp_get_free_heap_size(), 
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return ESP_OK;
}

static esp_err_t zb_manager_dev_auth_event_fn(const uint8_t *input, uint16_t inlen)
{
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED,
        .msg = (const char *)input,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = ESP_OK,
    };

    zbm_zigbee_app_signal_handler(&app_signal);
    ESP_LOGI(TAG, " HEAP: %u bytes free, largest: %u", 
             esp_get_free_heap_size(), 
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return ESP_OK;
}

static esp_err_t zb_manager_report_attr_event_fn(const uint8_t *input, uint16_t inlen)
{
    typedef struct {
        esp_zb_zcl_status_t status;       /*!< The status of the report attribute response, which can refer to esp_zb_zcl_status_t */
        esp_zb_zcl_addr_t src_address;    /*!< The struct of address contains short and ieee address, which can refer to esp_zb_zcl_addr_s */
        uint8_t src_endpoint;             /*!< The endpoint id which comes from report device */
        uint8_t dst_endpoint;             /*!< The destination endpoint id */
        uint16_t cluster;                 /*!< The cluster id that reported */
    } ESP_ZNSP_ZB_PACKED_STRUCT esp_ncp_zb_report_attr_t;

    typedef struct {
        uint16_t id;                                    /*!< The identify of attribute */
        uint8_t  type;                                  /*!< The type of attribute, which can refer to esp_zb_zcl_attr_type_t */
        uint16_t  size;                                  /*!< The value size of attribute  */
    } ESP_ZNSP_ZB_PACKED_STRUCT esp_ncp_zb_attr_data_t;

    if (inlen < sizeof(esp_ncp_zb_report_attr_t) + sizeof(esp_ncp_zb_attr_data_t)) {
        ESP_LOGE(TAG, "Input too short: %u", inlen);
        return ESP_ERR_INVALID_SIZE;
    }


    // 1. Копируем RAW-данные
    uint8_t *raw_copy = malloc(inlen);
    if (!raw_copy) {
        ESP_LOGE(TAG, "Failed to allocate raw_copy");
        return ESP_ERR_NO_MEM;
    }
    memcpy(raw_copy, input, inlen);
    /************************************ */
    esp_ncp_zb_report_attr_t *report_info = (esp_ncp_zb_report_attr_t *)raw_copy;
            esp_ncp_zb_attr_data_t *attr_data = (esp_ncp_zb_attr_data_t *)(raw_copy + sizeof(*report_info));

    zbm_dev_t *dev_obj = NULL;
    // Поиск устройства по short_addr
    dev_obj = zbm_find_device_in_devdb_by_short_safe(report_info->src_address.u.short_addr);
    if (!dev_obj) {
        ESP_LOGW(TAG, "Device with short address 0x%04x not found in devdb, skipping value update", report_info->src_address.u.short_addr);
        // Всё равно парсим, чтобы освободить память
    }

    esp_zb_zcl_report_attr_message_t report;
    report.status = report_info->status;
    memcpy(&report.src_address, &report_info->src_address, sizeof(esp_zb_zcl_addr_t));
    report.src_endpoint = report_info->src_endpoint;
    report.dst_endpoint = report_info->dst_endpoint;
    report.cluster = report_info->cluster;
    report.attribute.id = attr_data->id;
    report.attribute.data.type= attr_data->type;
    report.attribute.data.size = attr_data->size;
    report.attribute.data.value = calloc(1, attr_data->size);
    if (!report.attribute.data.value) {
        ESP_LOGE(TAG, "Failed to allocate memory for attribute value");
        free(raw_copy);
        raw_copy = NULL;
        return ESP_ERR_NO_MEM;
    }
    memcpy(report.attribute.data.value, raw_copy + sizeof(*report_info) + sizeof(*attr_data), attr_data->size);
    

    if (report.status == ESP_ZB_ZCL_STATUS_SUCCESS && dev_obj)
    {
        // Определяем role_mask: если команда пришла от сервера, то это SERVER_READ
            // Здесь info — это ответ от устройства, значит оно играет роль сервера
        zbm_cluster_role_t role_mask = ZBM_CLUSTER_ROLE_SERVER;
        // Получаем friendlyname (можно улучшить через базу, пока NULL)
        const char* attr_friendlyname = NULL; // или генерировать/брать из маппинга

            // Конвертируем тип данных ZCL → zbm_attr_data_types_t (упрощённо)
        zbm_attr_data_types_t zbm_data_type = (zbm_attr_data_types_t)report.attribute.data.type;

        uint8_t result = zbm_device_apply_reported_value_safe(
                dev_obj,
                report.src_endpoint,
                report.cluster,
                role_mask,
                report.attribute.id,
                attr_friendlyname,
                ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,  // только чтение
                zbm_data_type,
                report.attribute.data.size,
                report.attribute.data.value
            );
    
            if (result == 1) {
                ESP_LOGI(TAG, "✅ Created cluster 0x%04x and attribute  0x%04x on dev 0x%04x", report.cluster, report.attribute.id, dev_obj->short_addr);
                bool saved = zbm_save_device_to_spiffs_safe(dev_obj);
                if (saved) {
                    ESP_LOGI(TAG, "💾 Device saved to SPIFFS: dev_0x%04X.json", dev_obj->short_addr);
                } else {
                    ESP_LOGE(TAG, "❌ Failed to save device to SPIFFS");
                }
            } else {
                ESP_LOGI(TAG, "🔄 Updated cluster 0x%04x and attribute  0x%04x on dev 0x%04x", report.cluster, report.attribute.id, dev_obj->short_addr);
            }
        }
    free(raw_copy);
    raw_copy = NULL;
    if (report.attribute.data.value)
    {
        free(report.attribute.data.value);
        report.attribute.data.value = NULL;
    }
    ESP_LOGI(TAG, " HEAP: %u bytes free, largest: %u", 
             esp_get_free_heap_size(), 
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return ESP_OK;
}

static esp_err_t zb_manager_read_attr_resp_fn(const uint8_t *input, uint16_t inlen)
{
    if (!input || inlen < sizeof(esp_zb_zcl_cmd_info_t) + 1) {
        ESP_LOGE(TAG, "Invalid input or insufficient length: %u", inlen);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t* copied_input = calloc(1, inlen);
    if (!copied_input) {
        ESP_LOGE(TAG, "Failed to allocate copy buffer");
        return ESP_ERR_NO_MEM;
    }
    memcpy(copied_input, input, inlen);

    const uint8_t *ptr = copied_input;
    const size_t INFO_LEN = sizeof(esp_zb_zcl_cmd_info_t);

    // Извлекаем info
    esp_zb_zcl_cmd_info_t info;
    memcpy(&info, ptr, INFO_LEN);
    ptr += INFO_LEN;

    uint8_t attr_count = *ptr++;
    ESP_LOGI(TAG, "Read attribute response parsed: src_addr=0x%04x, endpoint=%u, cluster=0x%04x, attr_count=%u",
             info.src_address.u.short_addr, info.src_endpoint, info.cluster, attr_count);

    zbm_dev_t* dev_obj = zbm_find_device_in_devdb_by_short_safe(info.src_address.u.short_addr);
    if (!dev_obj) {
        ESP_LOGW(TAG, "Device with short address 0x%04x not found in devdb", info.src_address.u.short_addr);
    }

    esp_zb_zcl_read_attr_resp_variable_t *variables = NULL;
    esp_zb_zcl_read_attr_resp_variable_t *tail = NULL;

    for (int i = 0; i < attr_count; i++) {
        // === ИСПРАВЛЕНО: copied_input + inlen ===
        if (ptr + 3 > copied_input + inlen) {
            ESP_LOGE(TAG, "Buffer overflow while parsing attribute %d: insufficient space for status and attr_id", i);
            goto cleanup;
        }

        esp_zb_zcl_read_attr_resp_variable_t *var =
            (esp_zb_zcl_read_attr_resp_variable_t *)calloc(1, sizeof(*var));
        if (!var) {
            ESP_LOGE(TAG, "Failed to allocate variable for attribute %d", i);
            goto cleanup;
        }

        var->status = *ptr++;

        memcpy(&var->attribute.id, ptr, sizeof(uint16_t));
        ptr += sizeof(uint16_t);

        ESP_LOGI(TAG, "Parsing attr: id=0x%04x, status=0x%02x", var->attribute.id, var->status);

        if (var->status == ESP_ZB_ZCL_STATUS_SUCCESS) {
            // Проверяем наличие type и size
            if (ptr + 3 > copied_input + inlen) {  // type (1) + size (2)
                ESP_LOGE(TAG, "Buffer overflow: not enough data for type and size of attr 0x%04x", var->attribute.id);
                free(var);
                goto cleanup;
            }

            var->attribute.data.type = *ptr++;
            var->attribute.data.size = *(uint16_t*)ptr;
            ptr += sizeof(uint16_t);

            if (var->attribute.data.size > 0) {
                if (ptr + var->attribute.data.size > copied_input + inlen) {
                    ESP_LOGE(TAG, "Data overflow for attribute 0x%04x: not enough space for value", var->attribute.id);
                    free(var);
                    goto cleanup;
                }

                var->attribute.data.value = malloc(var->attribute.data.size);
                if (!var->attribute.data.value) {
                    ESP_LOGE(TAG, "Failed to allocate value for attribute 0x%04x", var->attribute.id);
                    free(var);
                    goto cleanup;
                }
                memcpy(var->attribute.data.value, ptr, var->attribute.data.size);
                ptr += var->attribute.data.size;
            } else {
                var->attribute.data.value = NULL;
            }
        } else {
            var->attribute.data.type = 0;
            var->attribute.data.size = 0;
            var->attribute.data.value = NULL;
        }

        var->next = NULL;
        if (!variables) {
            variables = var;
        } else {
            tail->next = var;
        }
        tail = var;

        if (var->status == ESP_ZB_ZCL_STATUS_SUCCESS && dev_obj) {
            zbm_cluster_role_t role_mask = ZBM_CLUSTER_ROLE_SERVER;
            const char* attr_friendlyname = NULL;
            zbm_attr_data_types_t zbm_data_type = (zbm_attr_data_types_t)var->attribute.data.type;

            uint8_t result = zbm_device_apply_reported_value_safe(
                dev_obj,
                info.src_endpoint,
                info.cluster,
                role_mask,
                var->attribute.id,
                attr_friendlyname,
                ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,
                zbm_data_type,
                var->attribute.data.size,
                var->attribute.data.value
            );

            if (result == 1) {
                ESP_LOGI(TAG, "✅ Created cluster 0x%04x and attribute 0x%04x on dev 0x%04x", info.cluster, var->attribute.id, info.src_address.u.short_addr);
                bool saved = zbm_save_device_to_spiffs_safe(dev_obj);
                if (saved) {
                    ESP_LOGI(TAG, "💾 Device saved to SPIFFS: dev_0x%04X.json", dev_obj->short_addr);
                } else {
                    ESP_LOGE(TAG, "❌ Failed to save device to SPIFFS");
                }
            } else {
                ESP_LOGI(TAG, "🔄 Updated cluster 0x%04x and attribute 0x%04x on dev 0x%04x", info.cluster, var->attribute.id, info.src_address.u.short_addr);
            }
        } else if (var->status != ESP_ZB_ZCL_STATUS_SUCCESS) {
            ESP_LOGW(TAG, "Attribute 0x%04x read failed with status 0x%02x", var->attribute.id, var->status);
        }
    }

    ESP_LOGI(TAG, "Successfully processed read attribute response from 0x%04x", info.src_address.u.short_addr);
    free(copied_input);
    return ESP_OK;

cleanup:
    while (variables) {
        esp_zb_zcl_read_attr_resp_variable_t *tmp = variables;
        variables = variables->next;
        if (tmp->attribute.data.value) {
            free(tmp->attribute.data.value);
        }
        free(tmp);
    }
    free(copied_input);
    return ESP_ERR_INVALID_SIZE;
}

static esp_err_t zb_manager_read_attr_resp_fn_old(const uint8_t *input, uint16_t inlen)
{
    if (!input || inlen < sizeof(esp_zb_zcl_cmd_info_t) + 1) {
        ESP_LOGE(TAG, "Invalid input or insufficient length: %u", inlen);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t* copied_input = calloc(1, inlen);
    if (!copied_input) {
        ESP_LOGE(TAG, "Failed to allocate copy buffer");
        return ESP_ERR_NO_MEM;
    }
    memcpy(copied_input, input, inlen);

    const uint8_t *ptr = copied_input;
    const size_t INFO_LEN = sizeof(esp_zb_zcl_cmd_info_t);

    // Извлекаем info
    esp_zb_zcl_cmd_info_t info;
    memcpy(&info, ptr, INFO_LEN);
    ptr += INFO_LEN;

    uint8_t attr_count = *ptr++;
    ESP_LOGI(TAG, "Read attribute response parsed: src_addr=0x%04x, endpoint=%u, cluster=0x%04x, attr_count=%u",
             info.src_address.u.short_addr, info.src_endpoint, info.cluster, attr_count);

    // Поиск устройства
    zbm_dev_t* dev_obj = zbm_find_device_in_devdb_by_short_safe(info.src_address.u.short_addr);
    if (!dev_obj) {
        ESP_LOGW(TAG, "Device with short address 0x%04x not found in devdb", info.src_address.u.short_addr);
    }

    esp_zb_zcl_read_attr_resp_variable_t *variables = NULL;
    esp_zb_zcl_read_attr_resp_variable_t *tail = NULL;

    for (int i = 0; i < attr_count; i++) {
        // === Минимальная длина: status (1) + attr_id (2) ===
        if (ptr + 3 > input + inlen) {
            ESP_LOGE(TAG, "Buffer overflow while parsing attribute %d: insufficient space for status and attr_id", i);
            goto cleanup;
        }

        esp_zb_zcl_read_attr_resp_variable_t *var = 
            (esp_zb_zcl_read_attr_resp_variable_t *)calloc(1, sizeof(*var));
        if (!var) {
            ESP_LOGE(TAG, "Failed to allocate variable for attribute %d", i);
            goto cleanup;
        }

        var->status = *ptr++;  // status (1 байт)

        memcpy(&var->attribute.id, ptr, sizeof(uint16_t));  // attr_id (2 байта)
        ptr += sizeof(uint16_t);

        ESP_LOGI(TAG, "Parsing attr: id=0x%04x, status=0x%02x", var->attribute.id, var->status);

        if (var->status == ESP_ZB_ZCL_STATUS_SUCCESS) {
            // === Только при успехе читаем type, size, value ===
            if (ptr + 3 > input + inlen) {  // type (1) + size (2)
                ESP_LOGE(TAG, "Buffer overflow: not enough data for type and size of attr 0x%04x", var->attribute.id);
                free(var);
                goto cleanup;
            }

            var->attribute.data.type = *ptr++;  // attr_type (1 байт)

            var->attribute.data.size = *(uint16_t*)ptr;  // data_size (2 байта)
            ptr += sizeof(uint16_t);

            if (var->attribute.data.size > 0) {
                if (ptr + var->attribute.data.size > input + inlen) {
                    ESP_LOGE(TAG, "Data overflow for attribute 0x%04x: not enough space for value", var->attribute.id);
                    free(var);
                    goto cleanup;
                }

                var->attribute.data.value = malloc(var->attribute.data.size);
                if (!var->attribute.data.value) {
                    ESP_LOGE(TAG, "Failed to allocate value for attribute 0x%04x", var->attribute.id);
                    free(var);
                    goto cleanup;
                }
                memcpy(var->attribute.data.value, ptr, var->attribute.data.size);
                ptr += var->attribute.data.size;
            } else {
                var->attribute.data.value = NULL;
            }
        } else {
            // При ошибке — ничего больше не читаем!
            var->attribute.data.type = 0;
            var->attribute.data.size = 0;
            var->attribute.data.value = NULL;
        }

        // Добавляем в список
        var->next = NULL;
        if (!variables) {
            variables = var;
        } else {
            tail->next = var;
        }
        tail = var;

        // Применяем значение, если успех и есть устройство
        if (var->status == ESP_ZB_ZCL_STATUS_SUCCESS && dev_obj) {
            zbm_cluster_role_t role_mask = ZBM_CLUSTER_ROLE_SERVER;
            const char* attr_friendlyname = NULL;
            zbm_attr_data_types_t zbm_data_type = (zbm_attr_data_types_t)var->attribute.data.type;

            uint8_t result = zbm_device_apply_reported_value_safe(
                dev_obj,
                info.src_endpoint,
                info.cluster,
                role_mask,
                var->attribute.id,
                attr_friendlyname,
                ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,
                zbm_data_type,
                var->attribute.data.size,
                var->attribute.data.value
            );

            if (result == 1) {
                ESP_LOGI(TAG, "✅ Created cluster 0x%04x and attribute 0x%04x on dev 0x%04x", info.cluster, var->attribute.id, info.src_address.u.short_addr);
                bool saved = zbm_save_device_to_spiffs_safe(dev_obj);
                if (saved) {
                    ESP_LOGI(TAG, "💾 Device saved to SPIFFS: dev_0x%04X.json", dev_obj->short_addr);
                } else {
                    ESP_LOGE(TAG, "❌ Failed to save device to SPIFFS");
                }
            } else {
                ESP_LOGI(TAG, "🔄 Updated cluster 0x%04x and attribute 0x%04x on dev 0x%04x", info.cluster, var->attribute.id, info.src_address.u.short_addr);
            }
        } else if (var->status != ESP_ZB_ZCL_STATUS_SUCCESS) {
            ESP_LOGW(TAG, "Attribute 0x%04x read failed with status 0x%02x", var->attribute.id, var->status);
        }
    }

    ESP_LOGI(TAG, "Successfully processed read attribute response from 0x%04x", info.src_address.u.short_addr);
    
    free(copied_input);
    return ESP_OK;

cleanup:
    // Очистка всех выделенных переменных
    while (variables) {
        esp_zb_zcl_read_attr_resp_variable_t *tmp = variables;
        variables = variables->next;
        if (tmp->attribute.data.value) {
            free(tmp->attribute.data.value);
        }
        free(tmp);
    }
    free(copied_input);
    return ESP_ERR_INVALID_SIZE;
}



/**
 * @brief Обработчик ZB_MANAGER_NOSTANDART_CLUSTER_CMD_REPORT от NCP
 */
static esp_err_t zb_manager_nostandart_cluster_cmd_resp_fn(const uint8_t *input, uint16_t inlen)
{
    typedef struct {
        esp_zb_zcl_status_t status;
        esp_zb_zcl_addr_t src_address;
        uint8_t src_endpoint;
        uint8_t dst_endpoint;
        uint16_t cluster;
        uint8_t command_id;
        uint8_t data_len;
        uint8_t data[64]; // variable, but capped
    } ESP_ZNSP_ZB_PACKED_STRUCT zb_ncp_nostandart_cmd_t;

    const size_t hdr_len = offsetof(zb_ncp_nostandart_cmd_t, data);
    if (inlen < hdr_len) {
        ESP_LOGE(TAG, "NOSTANDART_CMD: invalid length %u < %u", inlen, hdr_len);
        return ESP_ERR_INVALID_SIZE;
    }

    const zb_ncp_nostandart_cmd_t *cmd = (const zb_ncp_nostandart_cmd_t *)input;

    // Проверка длины пейлоада
    uint8_t actual_len = (cmd->data_len > 64) ? 64 : cmd->data_len;
    if (hdr_len + actual_len > inlen) {
        ESP_LOGE(TAG, "NOSTANDART_CMD: data overflows buffer");
        return ESP_ERR_INVALID_SIZE;
    }

    // Логируем
    ESP_LOGI(TAG, "🔧 NOSTANDART CMD: short=0x%04x, ep=%d→%d, cluster=0x%04x, cmd=0x%02x, len=%u",
             cmd->src_address.u.short_addr,
             cmd->src_endpoint,
             cmd->dst_endpoint,
             cmd->cluster,
             cmd->command_id,
             actual_len);

    if (actual_len > 0) {
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, cmd->data, actual_len, ESP_LOG_INFO);
    }

    // Поиск устройства по short_addr
    zbm_dev_t* dev_obj = zbm_find_device_in_devdb_by_short_safe(cmd->src_address.u.short_addr);
    if (!dev_obj) {
        ESP_LOGW(TAG, "Device with short address 0x%04x not found in devdb, skipping value update", cmd->src_address.u.short_addr);
        return ESP_ERR_NOT_FOUND;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%s_Report_0x%02X", "Custom", cmd->command_id);
    uint8_t result = zbm_update_cluster_custom_report_safe(
        dev_obj,
        cmd->src_endpoint,
        cmd->cluster,
        ZBM_CLUSTER_ROLE_SERVER,
        cmd->command_id,
        buf,
        ZBM_CMD_DATA_TYPE_U8,
        actual_len,
        cmd->data
    );

    if (result == 1) {
        ESP_LOGI(TAG, "✅ Custom report %s created with value 0x%02X", buf,  cmd->data);
        bool saved = zbm_save_device_to_spiffs_safe(dev_obj);
        if (saved) {
            ESP_LOGI(TAG, "💾 Device saved to SPIFFS: dev_0x%04X.json", dev_obj->short_addr);
        } else {
            ESP_LOGE(TAG, "❌ Failed to save device to SPIFFS");
        }
    } else {
        ESP_LOGI(TAG, "🔄 Custom report %s updated", buf);
    }

    ESP_LOGI(TAG, " HEAP: %u bytes free, largest: %u", 
             esp_get_free_heap_size(), 
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return ESP_OK;
}

static esp_err_t zb_manager_active_ep_resp_fn(const uint8_t *input, uint16_t inlen)
{
    ESP_LOGI(TAG, "zb_manager_active_ep_resp_fn: inlen=%d", inlen);

    typedef struct {
        local_esp_zb_zdp_status_t zdo_status;
        uint8_t                 ep_count;
        esp_zb_user_cb_t        find_usr;
        // далее следует массив ep_count байт
    } ESP_ZNSP_ZB_PACKED_STRUCT esp_zb_zdo_active_ep_t;

    if (inlen < sizeof(esp_zb_zdo_active_ep_t)) {
        ESP_LOGE(TAG, "Invalid inlen for active_ep_resp");
        return ESP_OK;
    }

    esp_zb_zdo_active_ep_t *zdo_resp = (esp_zb_zdo_active_ep_t *)input;
    uint8_t* ep_list = ((uint8_t*)input) + sizeof(esp_zb_zdo_active_ep_t);

    // Вызов callback (если задан)
    if (zdo_resp->find_usr.user_cb) {
        local_esp_zb_zdo_active_ep_callback_t cb = (local_esp_zb_zdo_active_ep_callback_t)zdo_resp->find_usr.user_cb;
        cb(zdo_resp->zdo_status, zdo_resp->ep_count, ep_list, (void*)zdo_resp->find_usr.user_ctx);
    }

    // === Обрабатываем ответ ===
    zbm_dev_t* dev = NULL;
    uint16_t* short_addr_value = (uint16_t*)zdo_resp->find_usr.user_ctx;
    ESP_LOGW(TAG, "short_addr_value: %d", *short_addr_value);
    dev = zbm_find_device_in_devdb_by_short_safe(*short_addr_value);
    //(zbm_dev_t*)zdo_resp->find_usr.user_ctx;
    if (!dev) {
        ESP_LOGW(TAG, "No device context in user_ctx for active_ep_resp");
        return ESP_OK;
    }

    uint8_t result = zbm_process_active_endpoint_response_safe(
        dev,
        zdo_resp->zdo_status,
        zdo_resp->ep_count,
        ep_list
    );

    if (result == 0xFF) {
        ESP_LOGW(TAG, "Active EP response processing failed or no valid endpoints");
        return ESP_FAIL;
    }
   ESP_LOGW(TAG, "result: %d", result);
    // === Если были изменения — сохраняем и уведомляем ===
    // Даже если result == 0 (обновление), можно обновить last_seen_ms
    // Но чаще всего нас интересует создание новых эндпоинтов → result == 1
   zbm_save_device_to_spiffs_safe(dev);
    

    if (result == 1) {
        // Что-то новое добавилось → точно надо сохранить
        zbm_save_device_to_spiffs_safe(dev);
        cJSON *data = cJSON_CreateObject();
        ESP_LOGI(TAG, "✅ Active EP response processing success and create ep -> next send simpledesc");
        
        // Отправляем только short_addr
        if (dev->short_addr != 0xFFFE) {
            char short_str[16];
            snprintf(short_str, sizeof(short_str), "0x%04X", dev->short_addr);
            cJSON_AddStringToObject(data, "short_addr", short_str);
        }

        zbm_ws_send_sys_notify("device_updated", "Device endpoints updated", data);
        cJSON_Delete(data);
    } else if (result == 0) {
        // Можно обновить время, но не обязательно сохранять
        // Например, все эндпоинты уже были, но устройство просто ответило
        // Решение: не сохраняем, но можно отправить notify, если важно
        // zbm_ws_send_device_update_notify(dev->ieee_addr);
        // Пока — только если создано
    }

    // отправляем simple_desc
        for (int i = 0; i < zdo_resp->ep_count; i++) {
            uint8_t endpoint_id = ep_list[i];
            esp_err_t ret = zbm_to_ncp_req_simple_desc_req(
                dev->short_addr,
                endpoint_id,
                NULL,                           // user_cb
                &dev->short_addr     // user_ctx
            );

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "✅ Simple Descriptor Request sent to 0x%04X, EP=%d", dev->short_addr, endpoint_id);
            } else {
                ESP_LOGE(TAG, "❌ Failed to send Simple Descriptor Request");
            }
        }
        // конец simple_desc продолжаем уведомления

    return ESP_OK;
}

static esp_err_t zb_manager_simple_desc_resp_fn(const uint8_t *input, uint16_t inlen)
{
    ESP_LOGW(TAG, "zb_manager_simple_desc_resp_fn: received response, inlen=%d", inlen);
    typedef struct {
        local_esp_zb_zdp_status_t zdo_status;
        esp_zb_user_cb_t    find_usr;
        local_esp_zb_af_simple_desc_1_1_t simple_desc; 
    } ESP_ZNSP_ZB_PACKED_STRUCT zb_manager_simple_desc_resp_pack_t;

    zb_manager_simple_desc_resp_pack_t *pkg = (zb_manager_simple_desc_resp_pack_t *)input;

    // 1. Вызываем callback (user_cb) с user_ctx
    if (pkg->find_usr.user_cb) {
        local_esp_zb_zdo_simple_desc_callback_t zdo_simple_desc_callback = (local_esp_zb_zdo_simple_desc_callback_t)pkg->find_usr.user_cb;
        ESP_LOGD(TAG, "Invoking user callback for Simple Desc");
        zdo_simple_desc_callback(pkg->zdo_status, (local_esp_zb_af_simple_desc_1_1_t*)(&pkg->simple_desc), (void *)pkg->find_usr.user_ctx);
    }

    // === Пропускаем ошибки ===
    if (pkg->zdo_status != ESP_ZB_ZDP_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "ZDO Simple Desc failed: status=0x%02x", pkg->zdo_status);
        return ESP_OK;
    }
    ESP_LOGI(TAG, "ZDO Simple Descriptor Response SUCCESS for device");

    // === Извлекаем short_addr из user_ctx ===
    uint16_t* short_addr_ptr = (uint16_t*)pkg->find_usr.user_ctx;
    if (!short_addr_ptr || *short_addr_ptr == 0xFFFF || *short_addr_ptr == 0xFFFE) {
        ESP_LOGW(TAG, "Invalid or null short_addr in user_ctx: 0x%04X", short_addr_ptr ? *short_addr_ptr : 0);
        return ESP_OK;
    }
    uint16_t short_addr = *short_addr_ptr;
    ESP_LOGI(TAG, "Processing Simple Descriptor for short_addr=0x%04X", short_addr);

    // === Находим устройство ===
    zbm_dev_t* dev = zbm_find_device_in_devdb_by_short_safe(short_addr);
    if (!dev) {
        ESP_LOGW(TAG, "Device not found by short_addr=0x%04X", short_addr);
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Found device: nwk_addr=0x%04X , endpoints_count=%d",
             dev->short_addr, dev->endpoints_count);
    
    // === Извлекаем данные из simple_desc ===
    local_esp_zb_af_simple_desc_1_1_t* desc = &pkg->simple_desc;
    uint8_t  endpoint_id     = desc->endpoint;
    uint16_t device_id       = desc->app_device_id;
    uint8_t  profile_id      = desc->app_profile_id;
    uint8_t  in_count        = desc->app_input_cluster_count;
    uint8_t  out_count       = desc->app_output_cluster_count;
    uint16_t* input_clusters = desc->app_cluster_list;
    uint16_t* output_clusters = in_count > 0 ? &desc->app_cluster_list[in_count] : NULL;
    
    zbm_dev_endpoint_t* ep = NULL;
    ep = zbm_find_endpoint_by_id_safe(dev, desc->endpoint);
    if (ep)
    {
        ep->device_id = device_id;
    }
    
    if (out_count == 0) {
        output_clusters = NULL;
    }

    ESP_LOGI(TAG, "Simple Descriptor: ep=%d, profile_id=0x%04X, device_id=0x%04X, in_clusters=%d, out_clusters=%d",
             endpoint_id, profile_id, device_id, in_count, out_count);

    // === Логируем входящие кластеры ===
    for (int i = 0; i < in_count; i++) {
        ESP_LOGI(TAG, "  Input cluster[%d] = 0x%04X", i, input_clusters[i]);
    }

    /*if (out_count > 0) {
    // === Логируем исходящие кластеры ===
        for (int i = 0; i < out_count; i++) {
            ESP_LOGI(TAG, "  Output cluster[%d] = 0x%04X", i, output_clusters[i]);
        }
    }*/

    // === Проверим: есть ли вообще кластеры? ===
    if (in_count == 0 && out_count == 0) {
        ESP_LOGD(TAG, "No clusters in Simple Descriptor for ep=%d", endpoint_id);
        return ESP_OK;
    }

    // === Применяем дескриптор ===
    zbm_device_apply_simple_descriptor_safe(
        dev,
        endpoint_id,
        device_id,
        input_clusters, in_count,
        output_clusters, out_count
    );

    zbm_save_device_to_spiffs_safe(dev);
    ESP_LOGI(TAG, "Applied and saved Simple Descriptor for dev=0x%04X, ep=%d", short_addr, endpoint_id);

    // === Отправляем discovery_attr для входящих кластеров ===
    if (in_count > 0 && input_clusters) {
        for (int i = 0; i < in_count; i++) {
            uint16_t cluster_id = input_clusters[i];
            ESP_LOGI(TAG, "Sending DISCOVERY ATTR for cluster=0x%04X, ep=%d -> dst_short=0x%04X",
                     cluster_id, endpoint_id, dev->short_addr);

            esp_zb_zcl_disc_attr_cmd_t cmd_req = {0};
            cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
            cmd_req.cluster_id = cluster_id;
            cmd_req.direction = 0; // To Server
            cmd_req.dis_defalut_resp = 1;
            cmd_req.max_attr_number = 30;
            cmd_req.start_attr_id = 0;

            // Заполняем адрес назначения
            cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = dev->short_addr;
            cmd_req.zcl_basic_cmd.dst_endpoint = endpoint_id;
            cmd_req.zcl_basic_cmd.src_endpoint = 1; 

            // Логируем полную структуру запроса
            ESP_LOGD(TAG, "DISC_ATTR_CMD: dst=0x%04X, ep=%d, src_ep=1, cluster=0x%04X, start_id=0, max=30",
                     dev->short_addr, endpoint_id, cluster_id);

            uint8_t tsn = zb_manager_disc_attr_cmd_req(&cmd_req);
            if (tsn != 0xff) {
                ESP_LOGI(TAG, "✅ DISCOVERY ATTR SEND: TSN=%d, cluster=0x%04X, ep=%d", tsn, cluster_id, endpoint_id);
            } else {
                ESP_LOGE(TAG, "❌ Failed to send DISCOVERY ATTR: cluster=0x%04X, ep=%d", cluster_id, endpoint_id);
            }
        }
    } else {
        ESP_LOGD(TAG, "No input clusters → skipping DISCOVERY ATTR");
    }

    // === Уведомление через WebSocket ===
    cJSON *data = cJSON_CreateObject();
    char short_str[16];
    snprintf(short_str, sizeof(short_str), "0x%04X", dev->short_addr);
    cJSON_AddStringToObject(data, "short_addr", short_str);
    cJSON_AddNumberToObject(data, "endpoint", endpoint_id);
    cJSON_AddNumberToObject(data, "input_clusters", in_count);
    cJSON_AddNumberToObject(data, "output_clusters", out_count);

    zbm_ws_send_sys_notify("device_updated", "Device structure updated from Simple Descriptor", data);
    cJSON_Delete(data);

    ESP_LOGI(TAG, "✅ Completed processing Simple Descriptor for dev=0x%04X, ep=%d, in=%d, out=%d",
             short_addr, endpoint_id, in_count, out_count);

    return ESP_OK;
}

static esp_err_t zb_manager_disc_attr_resp_fn(const uint8_t *input, uint16_t inlen)
{
    ESP_LOGI(TAG, "Received ZB_MANAGER_DISCOVERY_ATTR_RESP, len=%u", inlen);

    if (inlen < sizeof(esp_zb_zcl_cmd_info_t) + 1) {
        ESP_LOGE(TAG, "Invalid length for discover attr response");
        return ESP_ERR_INVALID_SIZE;
    }

    // Копируем RAW-данные, чтобы передать в worker
    uint8_t *raw_copy = malloc(inlen);
    if (!raw_copy) {
        ESP_LOGE(TAG, "Failed to allocate raw_copy");
        return ESP_ERR_NO_MEM;
    }
    memcpy(raw_copy, input, inlen);

    // Проверим: устройство уже известно? Если да — шлём в action_worker, иначе в pairing?
    esp_zb_zcl_cmd_info_t *info = (esp_zb_zcl_cmd_info_t *)raw_copy;
    zbm_dev_t *dev_info = NULL;
    dev_info = zbm_find_device_in_devdb_by_short_safe(info->src_address.u.short_addr);
    if (!dev_info)
    {
        ESP_LOGW(TAG, "Device not found in devdb, skipping value update");
        free(raw_copy);
        raw_copy = NULL;
        return ESP_FAIL;
    }
    if(info->status!=ESP_OK)
    {
        ESP_LOGE(TAG, "Error in discovery attr response: %d", info->status);
        free(raw_copy);
        raw_copy = NULL;
        return ESP_FAIL;
    }
    uint16_t src_addr = info->src_address.u.short_addr;
    uint8_t ep = info->dst_endpoint;
    uint16_t cluster_id = info->cluster;

    uint8_t attr_count = raw_copy[sizeof(esp_zb_zcl_cmd_info_t)];
    const uint8_t *ptr = raw_copy + sizeof(esp_zb_zcl_cmd_info_t) + 1;

    ESP_LOGI(TAG, "Discovery result: short=0x%04x, ep=%d, cluster=0x%04x, count=%d", src_addr, ep, cluster_id, attr_count);

    uint16_t attributes[attr_count];
    for (int i = 0; i < attr_count; i++) {
        uint16_t attr_id = (ptr[1] << 8) | ptr[0];
        zbm_attr_data_types_t attr_type = ptr[2];
        ESP_LOGI(TAG, "  Attr[0x%04x] Type=0x%02x", attr_id, attr_type);
        ptr += 3;
        attributes[i] = attr_id;
    }
    
    esp_zb_zcl_read_attr_cmd_t cmd_req;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd_req.zcl_basic_cmd.src_endpoint = 1;
    cmd_req.zcl_basic_cmd.dst_endpoint = 1;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = dev_info->short_addr;
    cmd_req.clusterID = cluster_id;
    cmd_req.dis_defalut_resp = 1;
    cmd_req.manuf_specific = 0;
    cmd_req.manuf_code = 0x0000;
    //uint16_t attributes[]={0x0004, 0x0000, 0x0001, 0x0005, 0x0007, 0xfffe};
    cmd_req.attr_number = sizeof(attributes) / sizeof(attributes[0]);
    cmd_req.attr_field = calloc(1,sizeof(attributes[0]) * cmd_req.attr_number);
    memcpy(cmd_req.attr_field, attributes, sizeof(uint16_t) * cmd_req.attr_number);
    //send cmd
    uint8_t tsn = 0xff;
    tsn = zbm_to_ncp_req_read_attributes(&cmd_req);
    if (tsn != 0xff)
    {
        ESP_LOGI(TAG, "ZB_MANAGER_DISCOVERY_ATTR_RESP Send READ ATTR with TSN = %d", tsn );
    }else
    {
        ESP_LOGW(TAG, "ZB_MANAGER_DISCOVERY_ATTR_RESP Error Sending TREAD ATTR");
    }
    if (cmd_req.attr_field != NULL)
    {
        free(cmd_req.attr_field);
        cmd_req.attr_field = NULL;
    }

    return ESP_OK;
}

static esp_err_t zb_manager_signal_zb_stack_failure_event_fn(const uint8_t *input, uint16_t inlen)
{
    ESP_LOGI(TAG, "ZB Stack Failure: RESTARTING Connection");
    zbm_host_reinit_on_ncp_foulture();
    return ESP_OK;
}

const esp_host_zb_func_t host_zb_api_from_ncp_func_table[] = {
    {ESP_NCP_NETWORK_FORMNETWORK, esp_host_zb_form_network_fn},
    /*{ESP_NCP_NETWORK_JOINNETWORK, esp_host_zb_joining_network_fn},*/
    {ESP_NCP_NETWORK_PERMIT_JOINING, esp_host_zb_permit_joining_fn},
    /*{ESP_NCP_NETWORK_LEAVENETWORK, esp_host_zb_leave_network_fn},
    {ESP_NCP_ZDO_BIND_SET, esp_host_zb_set_bind_fn},
    {ESP_NCP_ZDO_UNBIND_SET, esp_host_zb_set_unbind_fn},
    {ESP_NCP_ZDO_FIND_MATCH, esp_host_zb_find_match_fn},*/
    {ESP_NCP_ZCL_ATTR_REPORT_EVENT, zb_manager_report_attr_event_fn},
    {ESP_NCP_ZCL_ATTR_READ_RESP, zb_manager_read_attr_resp_fn},
    {ZB_MANAGER_DEV_ANNCE_EVENT, zb_manager_dev_annce_event_fn},
    {ZB_MANAGER_DEV_ASSOCIATED_EVENT, zb_manager_dev_assoc_event_fn},
    {ZB_MANAGER_DEV_UPDATE_EVENT, zb_manager_dev_update_event_fn},
    {ZB_MANAGER_DEV_AUTH_EVENT, zb_manager_dev_auth_event_fn},
    {ZB_MANAGER_ACTIVE_EP_RESP, zb_manager_active_ep_resp_fn},
    {ZB_MANAGER_SIMPLE_DESC_RESP, zb_manager_simple_desc_resp_fn},
    /*{ZB_MANAGER_NODE_DESC_RSP, zb_manager_node_desc_resp_fn},
    {ZB_MANAGER_REPORT_CONFIG_RESP, zb_manager_report_config_resp_fn},
    {ZB_MANAGER_CUSTOM_CLUSTER_REPORT , zb_manager_custom_cluster_rep_event_fn },*/ 
    {ZB_MANAGER_DISCOVERY_ATTR_RESP, zb_manager_disc_attr_resp_fn},
    {ZB_MANAGER_NOSTANDART_CLUSTER_CMD_REPORT, zb_manager_nostandart_cluster_cmd_resp_fn},
    {ZB_MANAGER_SIGNAL_ZB_STACK_FAILURE_EVENT, zb_manager_signal_zb_stack_failure_event_fn}
};

const uint8_t host_zb_api_from_ncp_func_table_size = sizeof(host_zb_api_from_ncp_func_table) / sizeof(esp_host_zb_func_t);

