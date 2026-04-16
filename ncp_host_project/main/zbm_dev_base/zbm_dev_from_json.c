#include "zbm_dev_from_json.h"
#include <string.h>
#include "esp_log.h"
#include "zbm_core_sync.h"

static const char* TAG = "ZBM_DEV_FROM_JSON";

// Вспомогательная: парсинг hex-строки IEEE
bool parse_ieee_addr(const char* str, uint8_t ieee[8]) {
    if (strlen(str) != 23) return false;
    for (int i = 0; i < 8; i++) {
        if (sscanf(str + i*3, "%02hhX", &ieee[i]) != 1) return false;
        if (i < 7 && str[i*3 + 2] != ':') return false;
    }
    return true;
}

uint16_t parse_hex16(const char* str) {
    if (!str) return 0;
    if (strncmp(str, "0x", 2) == 0 || strncmp(str, "0X", 2) == 0) {
        return (uint16_t)strtol(str + 2, NULL, 16);
    }
    return (uint16_t)atoi(str);
}

// Вспомогательная: безопасное копирование строки
char* safe_strdup(const char* str) {
    return str ? strdup(str) : NULL;
}

// Основная функция восстановления устройства из JSON
zbm_dev_t* restore_device_from_json(cJSON* json) {
    if (!json) return NULL;

    zbm_dev_t* dev = NULL;
    uint8_t ieee_addr[8] = {0};

    cJSON* j_ieee = cJSON_GetObjectItem(json, "ieee_addr");
    if (!j_ieee || !cJSON_IsString(j_ieee) || !parse_ieee_addr(j_ieee->valuestring, ieee_addr)) {
        ESP_LOGE(TAG, "Invalid or missing ieee_addr");
        return NULL;
    }

    //dev = zbm_dev_create_and_add_to_devdb_by_ieee_safe(ieee_addr); // создаёт с short = 0xFFFE

    dev = zbm_create_device_obj_by_ieee(ieee_addr);
    if (!dev) return NULL;

    uint16_t new_short_addr = 0xFFFE;
    dev->last_guid_update_short_addr = 0x0000;
    cJSON* j_short = cJSON_GetObjectItem(json, "short_addr");
    if (j_short && cJSON_IsString(j_short)) {
        new_short_addr = parse_hex16(j_short->valuestring);
    }

    bool short_updated = false;
    //short_updated = zbm_dev_update_short_addr_safe(dev, new_short_addr, ieee_addr);
    dev->short_addr = new_short_addr;
    short_updated = true;
    //zbm_guid_db_update_device_guids_safe(dev);

    // Опциональные поля
    cJSON* j_fn = cJSON_GetObjectItem(json, "name");
    if (j_fn && cJSON_IsString(j_fn)) {
        dev->friendly_name = safe_strdup(j_fn->valuestring);
    }

    

    cJSON* j_lqi = cJSON_GetObjectItem(json, "lqi");
    if (j_lqi && cJSON_IsNumber(j_lqi)) {
        dev->lqi = (uint8_t)j_lqi->valueint;
    }

    cJSON* j_online = cJSON_GetObjectItem(json, "is_online");
    if (j_online && cJSON_IsBool(j_online)) {
        dev->is_online = cJSON_IsTrue(j_online);
    }

    cJSON* j_last_seen = cJSON_GetObjectItem(json, "last_seen_ms");
    if (j_last_seen && cJSON_IsNumber(j_last_seen)) {
        dev->last_seen_ms = (uint32_t)j_last_seen->valuedouble;
    }

    cJSON* j_timeout = cJSON_GetObjectItem(json, "device_timeout_ms");
    if (j_timeout && cJSON_IsNumber(j_timeout)) {
        dev->device_timeout_ms = (uint64_t)j_timeout->valuedouble;
    }

    cJSON* j_manuf = cJSON_GetObjectItem(json, "manufacturer_code");
    if (j_manuf && cJSON_IsNumber(j_manuf)) {
        dev->manufacturer_code = (uint16_t)j_manuf->valueint;
    }

    cJSON* j_has_read = cJSON_GetObjectItem(json, "has_pending_read");
    if (j_has_read && cJSON_IsBool(j_has_read)) {
        dev->has_pending_read = cJSON_IsTrue(j_has_read);
    }

    cJSON* j_has_resp = cJSON_GetObjectItem(json, "has_pending_response");
    if (j_has_resp && cJSON_IsBool(j_has_resp)) {
        dev->has_pending_response = cJSON_IsTrue(j_has_resp);
    }

    cJSON* j_last_read = cJSON_GetObjectItem(json, "last_pending_read_ms");
    if (j_last_read && cJSON_IsNumber(j_last_read)) {
        dev->last_pending_read_ms = (uint32_t)j_last_read->valuedouble;
    }

    cJSON* j_index = cJSON_GetObjectItem(json, "index_in_array");
    if (j_index && cJSON_IsNumber(j_index)) {
        dev->index_in_array = (uint8_t)j_index->valueint;
    }

    // Эндпоинты
    cJSON* j_endpoints = cJSON_GetObjectItem(json, "endpoints");
    if (!j_endpoints || !cJSON_IsArray(j_endpoints)) {
        goto error;
    }

    dev->endpoints_count = (uint8_t)cJSON_GetArraySize(j_endpoints);
    if (dev->endpoints_count == 0) {
        dev->endpoints_array = NULL;
    } else {
        dev->endpoints_array = calloc(dev->endpoints_count, sizeof(zbm_dev_endpoint_t*));
        if (!dev->endpoints_array) {
            ESP_LOGE(TAG, "Failed: calloc failed for endpoints_array");
            goto error;
        }
    }

    for (int ep_i = 0; ep_i < dev->endpoints_count; ep_i++) {
        cJSON* j_ep = cJSON_GetArrayItem(j_endpoints, ep_i);
        if (!j_ep || !cJSON_IsObject(j_ep)) continue;

        zbm_dev_endpoint_t* ep = zbm_create_empty_endpoint();
        if (!ep) {
            ESP_LOGE(TAG, "Failed: zbm_create_empty_endpoint returned NULL at index %d", ep_i);
            goto error;
        }

        cJSON* j_id = cJSON_GetObjectItem(j_ep, "id");
        if (j_id && cJSON_IsNumber(j_id)) {
            ep->id = (uint8_t)j_id->valueint;
        }

        cJSON* j_is_use = cJSON_GetObjectItem(j_ep, "is_use_on_device");
        if (j_is_use && cJSON_IsBool(j_is_use)) {
            ep->is_use_on_device = cJSON_IsTrue(j_is_use);
        }

        cJSON* j_fn_ep = cJSON_GetObjectItem(j_ep, "name");
        if (j_fn_ep && cJSON_IsString(j_fn_ep)) {
            ep->friendlyname = safe_strdup(j_fn_ep->valuestring);
        }

        cJSON* j_dev_type = cJSON_GetObjectItem(j_ep, "device_id");
        if (j_dev_type && cJSON_IsNumber(j_dev_type)) {
            ep->device_id = (zbm_device_type_t)j_dev_type->valueint;
        } else {
            cJSON* j_name = cJSON_GetObjectItem(j_ep, "device_type"); // совместимость
            if (j_name && cJSON_IsString(j_name)) {
                ep->device_id = get_device_type_by_name(j_name->valuestring);
            }
        }

        // === Стандартные кластеры ===
        cJSON* j_std_clusters = cJSON_GetObjectItem(j_ep, "standard_clusters");
        if (j_std_clusters && cJSON_IsArray(j_std_clusters)) {
            ep->standart_cluster_count = (uint8_t)cJSON_GetArraySize(j_std_clusters);
            if (ep->standart_cluster_count == 0) {
                ep->standart_cluster_array = NULL;
            } else {
                ep->standart_cluster_array = calloc(ep->standart_cluster_count, sizeof(zbm_standart_cluster_t*));
                if (!ep->standart_cluster_array) {
                    ESP_LOGE(TAG, "Failed: calloc failed for standart_cluster_array at endpoint %d", ep_i);
                    goto error;
                }
            }

            for (int cl_i = 0; cl_i < ep->standart_cluster_count; cl_i++) {
                cJSON* j_cl = cJSON_GetArrayItem(j_std_clusters, cl_i);
                if (!j_cl || !cJSON_IsObject(j_cl)) continue;

                uint16_t cluster_id = 0;
                cJSON* j_cid = cJSON_GetObjectItem(j_cl, "id");
                if (!j_cid || !cJSON_IsNumber(j_cid)) continue;
                cluster_id = (uint16_t)j_cid->valueint;

                cJSON* j_role = cJSON_GetObjectItem(j_cl, "role");
                zbm_cluster_role_t role_mask = ZBM_CLUSTER_ROLE_SERVER;
                if (j_role && cJSON_IsString(j_role)) {
                    if (strcmp(j_role->valuestring, "client") == 0) {
                        role_mask = ZBM_CLUSTER_ROLE_CLIENT;
                    }
                }

                zbm_standart_cluster_t* cluster = (zbm_standart_cluster_t*)create_cluster(cluster_id, role_mask, false);
                if (!cluster) continue;

                cJSON* j_friendly = cJSON_GetObjectItem(j_cl, "name");
                if (j_friendly && cJSON_IsString(j_friendly)) {
                    free(cluster->friendlyname);
                    cluster->friendlyname = safe_strdup(j_friendly->valuestring);
                }

                // === Атрибуты ===
                cJSON* j_attrs = cJSON_GetObjectItem(j_cl, "attributes");
                if (j_attrs && cJSON_IsArray(j_attrs)) {
                    cluster->attr_count = (uint8_t)cJSON_GetArraySize(j_attrs);
                    if (cluster->attr_count == 0) {
                        cluster->attr_array = NULL;
                    } else {
                        cluster->attr_array = calloc(cluster->attr_count, sizeof(zbm_cluster_attribute_t*));
                        if (!cluster->attr_array) {
                            ESP_LOGE(TAG, "Failed: calloc failed for attr_array (cluster 0x%04x, ep %d)", cluster_id, ep->id);
                            goto error;
                        }
                    }

                    for (int a_i = 0; a_i < cluster->attr_count; a_i++) {
                        cJSON* j_attr = cJSON_GetArrayItem(j_attrs, a_i);
                        if (!j_attr || !cJSON_IsObject(j_attr)) continue;

                        uint16_t attr_id = 0;
                        cJSON* j_aid = cJSON_GetObjectItem(j_attr, "id");
                        if (!j_aid || !cJSON_IsNumber(j_aid)) continue;
                        attr_id = (uint16_t)j_aid->valueint;

                        cJSON* j_type = cJSON_GetObjectItem(j_attr, "type");
                        if (!j_type || !cJSON_IsNumber(j_type)) continue;
                        zbm_attr_data_types_t type = (zbm_attr_data_types_t)j_type->valueint;

                        cJSON* j_size = cJSON_GetObjectItem(j_attr, "size");
                        if (!j_size || !cJSON_IsNumber(j_size)) continue;
                        uint16_t size = (uint16_t)j_size->valueint;

                        cJSON* j_name = cJSON_GetObjectItem(j_attr, "name");
                        const char* name = j_name && cJSON_IsString(j_name) ? j_name->valuestring : NULL;

                        zbm_cluster_attribute_t* attr = create_attr(attr_id, name, type, size);
                        if (!attr) continue;

                        // GUID
                        cJSON* j_guid = cJSON_GetObjectItem(j_attr, "guid");
                        if (j_guid && cJSON_IsString(j_guid)) {
                            strncpy(attr->guid, j_guid->valuestring, sizeof(attr->guid) - 1);
                        }

                        // Значение
                        // === Установка значения атрибута в зависимости от типа ===
                        cJSON* j_val = cJSON_GetObjectItem(j_attr, "value");
                        if (!j_val) {
                            memset(attr->p_value, 0, attr->data_size); // Обнуляем, если значение не задано
                        } else {
                            void* p_val = attr->p_value;
                            uint16_t size = attr->data_size;

                            switch (attr->data_type) {
                                case ZBM_ATTR_TYPE_BOOL:
                                    if (cJSON_IsBool(j_val)) {
                                        *(bool*)p_val = cJSON_IsTrue(j_val);
                                    } else if (cJSON_IsNumber(j_val)) {
                                        *(bool*)p_val = (j_val->valueint != 0);
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;

                                case ZBM_ATTR_TYPE_U8:
                                    if (cJSON_IsNumber(j_val)) {
                                        uint8_t v = (uint8_t)j_val->valueint;
                                        memcpy(p_val, &v, 1);
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;

                                case ZBM_ATTR_TYPE_S8:
                                    if (cJSON_IsNumber(j_val)) {
                                        int8_t v = (int8_t)j_val->valueint;
                                        memcpy(p_val, &v, 1);
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;

                                case ZBM_ATTR_TYPE_U16:
                                case ZBM_ATTR_TYPE_T16BIT_ENUM:
                                    if (cJSON_IsNumber(j_val)) {
                                        uint16_t v = (uint16_t)j_val->valueint;
                                        memcpy(p_val, &v, 2);
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;

                                case ZBM_ATTR_TYPE_S16:
                                case ZBM_ATTR_TYPE_T8BIT_ENUM:
                                    if (cJSON_IsNumber(j_val)) {
                                        int16_t v = (int16_t)j_val->valueint;
                                        memcpy(p_val, &v, 2);
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;

                                case ZBM_ATTR_TYPE_U24:
                                case ZBM_ATTR_TYPE_U32:
                                case ZBM_ATTR_TYPE_S24:
                                case ZBM_ATTR_TYPE_S32: {
                                    uint32_t v = (uint32_t)j_val->valuedouble;
                                    memcpy(p_val, &v, (size > 4) ? 4 : size);
                                    break;
                                }

                                case ZBM_ATTR_TYPE_U40:
                                case ZBM_ATTR_TYPE_U48:
                                case ZBM_ATTR_TYPE_U56:
                                case ZBM_ATTR_TYPE_U64:
                                case ZBM_ATTR_TYPE_S40:
                                case ZBM_ATTR_TYPE_S48:
                                case ZBM_ATTR_TYPE_S56:
                                case ZBM_ATTR_TYPE_S64: {
                                    uint64_t v = 0;
                                    if (cJSON_IsString(j_val)) {
                                        v = strtoull(j_val->valuestring, NULL, 10);
                                    } else if (cJSON_IsNumber(j_val)) {
                                        v = (uint64_t)j_val->valuedouble;
                                    }
                                    memcpy(p_val, &v, (size > 8) ? 8 : size);
                                    break;
                                }

                                case ZBM_ATTR_TYPE_SEMI:
                                case ZBM_ATTR_TYPE_SINGLE: {
                                    float v = (float)j_val->valuedouble;
                                    memcpy(p_val, &v, 4);
                                    break;
                                }

                                case ZBM_ATTR_TYPE_DOUBLE: {
                                    double v = j_val->valuedouble;
                                    memcpy(p_val, &v, 8);
                                    break;
                                }

                                case ZBM_ATTR_TYPE_CHAR_STRING:
                                case ZBM_ATTR_TYPE_LONG_CHAR_STRING: {
                                    if (cJSON_IsString(j_val)) {
                                        size_t src_len = strlen(j_val->valuestring);
                                        uint8_t max_len = (size > 0) ? size - 1 : 0;
                                        uint8_t actual_len = (src_len < max_len) ? (uint8_t)src_len : max_len;

                                        // Устанавливаем длину
                                        if (size > 0) {
                                            ((uint8_t*)p_val)[0] = actual_len;
                                        }

                                        // Копируем данные
                                        if (actual_len > 0 && size > 1) {
                                            memcpy((uint8_t*)p_val + 1, j_val->valuestring, actual_len);
                                        }

                                        // Обнуляем остаток
                                        if (size > 1 + actual_len) {
                                            memset((uint8_t*)p_val + 1 + actual_len, 0, size - 1 - actual_len);
                                        }
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;
                                }

                                case ZBM_ATTR_TYPE_OCTET_STRING:
                                case ZBM_ATTR_TYPE_LONG_OCTET_STRING: {
                                    if (cJSON_IsString(j_val)) {
                                        const char* hex = j_val->valuestring;
                                        int len = strlen(hex);
                                        if (len == size * 2) {
                                            for (int i = 0; i < size; i++) {
                                                sscanf(hex + i*2, "%02hhX", &((uint8_t*)p_val)[i]);
                                            }
                                        } else {
                                            memset(p_val, 0, size);
                                        }
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;
                                }

                                case ZBM_ATTR_TYPE_IEEE_ADDR: {
                                    if (cJSON_IsString(j_val)) {
                                        uint8_t tmp[8];
                                        if (parse_ieee_addr(j_val->valuestring, tmp)) {
                                            memcpy(p_val, tmp, 8);
                                        } else {
                                            memset(p_val, 0, size);
                                        }
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;
                                }

                                case ZBM_ATTR_TYPE_CLUSTER_ID:
                                case ZBM_ATTR_TYPE_ATTRIBUTE_ID: {
                                    uint16_t v = parse_hex16(cJSON_IsString(j_val) ? j_val->valuestring : NULL);
                                    memcpy(p_val, &v, 2);
                                    break;
                                }

                                case ZBM_ATTR_TYPE_UTC_TIME: {
                                    uint32_t v = (uint32_t)j_val->valuedouble;
                                    memcpy(p_val, &v, 4);
                                    break;
                                }

                                case ZBM_ATTR_TYPE_TIME_OF_DAY: {
                                    if (cJSON_IsString(j_val)) {
                                        int h, m, s, hun;
                                        if (sscanf(j_val->valuestring, "%d:%d:%d.%d", &h, &m, &s, &hun) == 4) {
                                            uint32_t tod = (h << 24) | (m << 16) | (s << 8) | hun;
                                            memcpy(p_val, &tod, 4);
                                        } else {
                                            memset(p_val, 0, size);
                                        }
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;
                                }

                                case ZBM_ATTR_TYPE_DATE: {
                                    if (cJSON_IsString(j_val)) {
                                        int year, month, day;
                                        if (sscanf(j_val->valuestring, "%d-%d-%d", &year, &month, &day) == 3) {
                                            uint8_t y = (uint8_t)(year - 2000);
                                            uint32_t date = (y << 24) | (month << 16) | (day << 8);
                                            memcpy(p_val, &date, 4);
                                        } else {
                                            memset(p_val, 0, size);
                                        }
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;
                                }

                                // Для сложных типов — обнуляем
                                case ZBM_ATTR_TYPE_ARRAY:
                                case ZBM_ATTR_TYPE_T16BIT_ARRAY:
                                case ZBM_ATTR_TYPE_T32BIT_ARRAY:
                                case ZBM_ATTR_TYPE_STRUCTURE:
                                case ZBM_ATTR_TYPE_SET:
                                case ZBM_ATTR_TYPE_BAG:
                                default:
                                    memset(p_val, 0, size);
                                    break;
                            }
                        }

                        cluster->attr_array[a_i] = attr;
                    }
                }

                // === Команды ===
                cJSON* j_cmds = cJSON_GetObjectItem(j_cl, "commands");
                if (j_cmds && cJSON_IsArray(j_cmds)) {
                    cluster->standart_cmd_count = (uint8_t)cJSON_GetArraySize(j_cmds);
                    uint8_t count_out;
                    cluster->standart_cmd_array = zbm_create_standard_command_array(cluster_id, role_mask, &count_out);
                    // Просто копируем GUID
                    for (int c_i = 0; c_i < cluster->standart_cmd_count; c_i++) {
                        cJSON* j_cmd = cJSON_GetArrayItem(j_cmds, c_i);
                        if (!j_cmd) continue;
                        cJSON* j_guid = cJSON_GetObjectItem(j_cmd, "guid");
                        if (j_guid && cJSON_IsString(j_guid) && cluster->standart_cmd_array[c_i]) {
                            strncpy(cluster->standart_cmd_array[c_i]->guid, j_guid->valuestring, sizeof(cluster->standart_cmd_array[c_i]->guid) - 1);
                        }
                    }
                }

                // === Кастомные репорты ===
                cJSON* j_reps = cJSON_GetObjectItem(j_cl, "custom_reports");
                if (j_reps && cJSON_IsArray(j_reps)) {
                    cluster->custom_report_cmd_count = (uint8_t)cJSON_GetArraySize(j_reps);

                    if (cluster->custom_report_cmd_count == 0) {
                        cluster->custom_report_cmd_array = NULL; // Явно указываем, что нет команд
                    } else {
                        cluster->custom_report_cmd_array = calloc(cluster->custom_report_cmd_count, sizeof(zbm_cluster_custom_report_cmd_t*));
                        if (!cluster->custom_report_cmd_array) {
                            ESP_LOGE(TAG, "Failed: calloc failed for custom_report_cmd_array (cluster 0x%04x, ep %d)", cluster_id, ep->id);
                            goto error;
                        }
                    }

                    for (int r_i = 0; r_i < cluster->custom_report_cmd_count; r_i++) {
                        cJSON* j_rep = cJSON_GetArrayItem(j_reps, r_i);
                        if (!j_rep || !cJSON_IsObject(j_rep)) continue;

                        zbm_cluster_custom_report_cmd_t* rep = calloc(1, sizeof(zbm_cluster_custom_report_cmd_t));
                        if (!rep) continue;

                        cJSON* j_rid = cJSON_GetObjectItem(j_rep, "id");
                        if (j_rid && cJSON_IsNumber(j_rid)) rep->id = (uint8_t)j_rid->valueint;

                        cJSON* j_name = cJSON_GetObjectItem(j_rep, "name");
                        if (j_name && cJSON_IsString(j_name)) rep->friendlyname = safe_strdup(j_name->valuestring);

                        cJSON* j_guid = cJSON_GetObjectItem(j_rep, "guid");
                        if (j_guid && cJSON_IsString(j_guid)) {
                            strncpy(rep->guid, j_guid->valuestring, sizeof(rep->guid) - 1);
                        }

                        cJSON* j_type = cJSON_GetObjectItem(j_rep, "type");
                        if (j_type && cJSON_IsNumber(j_type)) rep->data_type = (zbm_cmd_data_types_t)j_type->valueint;

                        cJSON* j_size = cJSON_GetObjectItem(j_rep, "size");
                        if (j_size && cJSON_IsNumber(j_size)) rep->data_size = (uint16_t)j_size->valueint;

                        if (rep->data_size > 0) {
                            rep->p_value = calloc(1, rep->data_size);
                            if (!rep->p_value) {
                                free(rep->friendlyname);
                                free(rep);
                                continue;
                            }
                        }

                        cJSON* j_val = cJSON_GetObjectItem(j_rep, "value");
                        if (j_val && rep->p_value && rep->data_size == 1 && cJSON_IsNumber(j_val)) {
                            uint8_t v = (uint8_t)j_val->valueint;
                            memcpy(rep->p_value, &v, 1);
                        }

                        cluster->custom_report_cmd_array[r_i] = rep;
                    }
                }

                ep->standart_cluster_array[cl_i] = cluster;
            }
        }

        // === Кастомные кластеры ===
        cJSON* j_cust_clusters = cJSON_GetObjectItem(j_ep, "custom_clusters");
        if (j_cust_clusters && cJSON_IsArray(j_cust_clusters)) {
            ep->custom_cluster_count = (uint8_t)cJSON_GetArraySize(j_cust_clusters);
            if (ep->custom_cluster_count == 0) {
                ep->custom_cluster_array = NULL;
            } else {
                ep->custom_cluster_array = calloc(ep->custom_cluster_count, sizeof(zbm_custom_cluster_t*));
                if (!ep->custom_cluster_array) {
                    ESP_LOGE(TAG, "Failed: calloc failed for custom_cluster_array at endpoint %d", ep_i);
                    goto error;
                }
            }

            //ESP_LOGI(TAG, "custom_count = %d", ep->custom_cluster_count);
            for (int cl_i = 0; cl_i < ep->custom_cluster_count; cl_i++) {
                cJSON* j_cl = cJSON_GetArrayItem(j_cust_clusters, cl_i);
                if (!j_cl || !cJSON_IsObject(j_cl)) continue;

                uint16_t cluster_id = 0;
                cJSON* j_cid = cJSON_GetObjectItem(j_cl, "id");
                if (!j_cid || !cJSON_IsNumber(j_cid)) continue;
                cluster_id = (uint16_t)j_cid->valueint;

                cJSON* j_role = cJSON_GetObjectItem(j_cl, "role");
                zbm_cluster_role_t role_mask = ZBM_CLUSTER_ROLE_SERVER;
                if (j_role && cJSON_IsString(j_role)) {
                    if (strcmp(j_role->valuestring, "client") == 0) {
                        role_mask = ZBM_CLUSTER_ROLE_CLIENT;
                    }
                }

                /*zbm_custom_cluster_t* cluster = NULL;
                cluster = (zbm_custom_cluster_t*)create_cluster(cluster_id, role_mask, true);
                if (!cluster) continue;*/
                zbm_custom_cluster_t* cluster = calloc(1, sizeof(zbm_custom_cluster_t));
                if (!cluster) continue;

                cluster->id = cluster_id;
                cluster->role_mask = role_mask;
                cluster->attr_count = 0;
                cluster->attr_array = NULL;
                cluster->standart_cmd_count = 0;
                cluster->standart_cmd_array = NULL;
                cluster->custom_report_cmd_count = 0;
                cluster->custom_report_cmd_array = NULL;
                cluster->friendlyname = NULL;

                cJSON* j_friendly = cJSON_GetObjectItem(j_cl, "name");
                if (j_friendly && cJSON_IsString(j_friendly)) {
                    free(cluster->friendlyname);
                    cluster->friendlyname = safe_strdup(j_friendly->valuestring);
                }

                // === Атрибуты ===
                cJSON* j_attrs = cJSON_GetObjectItem(j_cl, "attributes");
                if (j_attrs && cJSON_IsArray(j_attrs)) {
                    cluster->attr_count = (uint8_t)cJSON_GetArraySize(j_attrs);
                    if (cluster->attr_count == 0) {
                        cluster->attr_array = NULL;
                    } else {
                        cluster->attr_array = calloc(cluster->attr_count, sizeof(zbm_cluster_attribute_t*));
                        if (!cluster->attr_array) {
                            ESP_LOGE(TAG, "Failed: calloc failed for attr_array in custom cluster (cluster 0x%04x, ep %d)", cluster_id, ep->id);
                            goto error;
                        }
                    }

                    for (int a_i = 0; a_i < cluster->attr_count; a_i++) {
                        cJSON* j_attr = cJSON_GetArrayItem(j_attrs, a_i);
                        if (!j_attr || !cJSON_IsObject(j_attr)) continue;

                        uint16_t attr_id = 0;
                        cJSON* j_aid = cJSON_GetObjectItem(j_attr, "id");
                        if (!j_aid || !cJSON_IsNumber(j_aid)) continue;
                        attr_id = (uint16_t)j_aid->valueint;

                        cJSON* j_type = cJSON_GetObjectItem(j_attr, "type");
                        if (!j_type || !cJSON_IsNumber(j_type)) continue;
                        zbm_attr_data_types_t type = (zbm_attr_data_types_t)j_type->valueint;

                        cJSON* j_size = cJSON_GetObjectItem(j_attr, "size");
                        if (!j_size || !cJSON_IsNumber(j_size)) continue;
                        uint16_t size = (uint16_t)j_size->valueint;

                        cJSON* j_name = cJSON_GetObjectItem(j_attr, "name");
                        const char* name = j_name && cJSON_IsString(j_name) ? j_name->valuestring : NULL;

                        zbm_cluster_attribute_t* attr = create_attr(attr_id, name, type, size);
                        if (!attr) continue;

                        // GUID
                        cJSON* j_guid = cJSON_GetObjectItem(j_attr, "guid");
                        if (j_guid && cJSON_IsString(j_guid)) {
                            strncpy(attr->guid, j_guid->valuestring, sizeof(attr->guid) - 1);
                        }

                        // === Установка значения атрибута в зависимости от типа ===
                        cJSON* j_val = cJSON_GetObjectItem(j_attr, "value");
                        if (!j_val) {
                            memset(attr->p_value, 0, attr->data_size); // Обнуляем, если значение не задано
                        } else {
                            void* p_val = attr->p_value;
                            uint16_t size = attr->data_size;

                            switch (attr->data_type) {
                                case ZBM_ATTR_TYPE_BOOL:
                                    if (cJSON_IsBool(j_val)) {
                                        *(bool*)p_val = cJSON_IsTrue(j_val);
                                    } else if (cJSON_IsNumber(j_val)) {
                                        *(bool*)p_val = (j_val->valueint != 0);
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;

                                case ZBM_ATTR_TYPE_U8:
                                    if (cJSON_IsNumber(j_val)) {
                                        uint8_t v = (uint8_t)j_val->valueint;
                                        memcpy(p_val, &v, 1);
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;

                                case ZBM_ATTR_TYPE_S8:
                                    if (cJSON_IsNumber(j_val)) {
                                        int8_t v = (int8_t)j_val->valueint;
                                        memcpy(p_val, &v, 1);
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;

                                case ZBM_ATTR_TYPE_U16:
                                case ZBM_ATTR_TYPE_T16BIT_ENUM:
                                    if (cJSON_IsNumber(j_val)) {
                                        uint16_t v = (uint16_t)j_val->valueint;
                                        memcpy(p_val, &v, 2);
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;

                                case ZBM_ATTR_TYPE_S16:
                                case ZBM_ATTR_TYPE_T8BIT_ENUM:
                                    if (cJSON_IsNumber(j_val)) {
                                        int16_t v = (int16_t)j_val->valueint;
                                        memcpy(p_val, &v, 2);
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;

                                case ZBM_ATTR_TYPE_U24:
                                case ZBM_ATTR_TYPE_U32:
                                case ZBM_ATTR_TYPE_S24:
                                case ZBM_ATTR_TYPE_S32: {
                                    uint32_t v = (uint32_t)j_val->valuedouble;
                                    memcpy(p_val, &v, (size > 4) ? 4 : size);
                                    break;
                                }

                                case ZBM_ATTR_TYPE_U40:
                                case ZBM_ATTR_TYPE_U48:
                                case ZBM_ATTR_TYPE_U56:
                                case ZBM_ATTR_TYPE_U64:
                                case ZBM_ATTR_TYPE_S40:
                                case ZBM_ATTR_TYPE_S48:
                                case ZBM_ATTR_TYPE_S56:
                                case ZBM_ATTR_TYPE_S64: {
                                    uint64_t v = 0;
                                    if (cJSON_IsString(j_val)) {
                                        v = strtoull(j_val->valuestring, NULL, 10);
                                    } else if (cJSON_IsNumber(j_val)) {
                                        v = (uint64_t)j_val->valuedouble;
                                    }
                                    memcpy(p_val, &v, (size > 8) ? 8 : size);
                                    break;
                                }

                                case ZBM_ATTR_TYPE_SEMI:
                                case ZBM_ATTR_TYPE_SINGLE: {
                                    float v = (float)j_val->valuedouble;
                                    memcpy(p_val, &v, 4);
                                    break;
                                }

                                case ZBM_ATTR_TYPE_DOUBLE: {
                                    double v = j_val->valuedouble;
                                    memcpy(p_val, &v, 8);
                                    break;
                                }

                                case ZBM_ATTR_TYPE_CHAR_STRING:
                                case ZBM_ATTR_TYPE_LONG_CHAR_STRING: {
                                    if (cJSON_IsString(j_val)) {
                                        size_t src_len = strlen(j_val->valuestring);
                                        uint8_t max_len = (size > 0) ? size - 1 : 0;
                                        uint8_t actual_len = (src_len < max_len) ? (uint8_t)src_len : max_len;

                                        // Устанавливаем длину
                                        if (size > 0) {
                                            ((uint8_t*)p_val)[0] = actual_len;
                                        }

                                        // Копируем данные
                                        if (actual_len > 0 && size > 1) {
                                            memcpy((uint8_t*)p_val + 1, j_val->valuestring, actual_len);
                                        }

                                        // Обнуляем остаток
                                        if (size > 1 + actual_len) {
                                            memset((uint8_t*)p_val + 1 + actual_len, 0, size - 1 - actual_len);
                                        }
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;
                                }

                                case ZBM_ATTR_TYPE_OCTET_STRING:
                                case ZBM_ATTR_TYPE_LONG_OCTET_STRING: {
                                    if (cJSON_IsString(j_val)) {
                                        const char* hex = j_val->valuestring;
                                        int len = strlen(hex);
                                        if (len == size * 2) {
                                            for (int i = 0; i < size; i++) {
                                                sscanf(hex + i*2, "%02hhX", &((uint8_t*)p_val)[i]);
                                            }
                                        } else {
                                            memset(p_val, 0, size);
                                        }
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;
                                }

                                case ZBM_ATTR_TYPE_IEEE_ADDR: {
                                    if (cJSON_IsString(j_val)) {
                                        uint8_t tmp[8];
                                        if (parse_ieee_addr(j_val->valuestring, tmp)) {
                                            memcpy(p_val, tmp, 8);
                                        } else {
                                            memset(p_val, 0, size);
                                        }
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;
                                }

                                case ZBM_ATTR_TYPE_CLUSTER_ID:
                                case ZBM_ATTR_TYPE_ATTRIBUTE_ID: {
                                    uint16_t v = parse_hex16(cJSON_IsString(j_val) ? j_val->valuestring : NULL);
                                    memcpy(p_val, &v, 2);
                                    break;
                                }

                                case ZBM_ATTR_TYPE_UTC_TIME: {
                                    uint32_t v = (uint32_t)j_val->valuedouble;
                                    memcpy(p_val, &v, 4);
                                    break;
                                }

                                case ZBM_ATTR_TYPE_TIME_OF_DAY: {
                                    if (cJSON_IsString(j_val)) {
                                        int h, m, s, hun;
                                        if (sscanf(j_val->valuestring, "%d:%d:%d.%d", &h, &m, &s, &hun) == 4) {
                                            uint32_t tod = (h << 24) | (m << 16) | (s << 8) | hun;
                                            memcpy(p_val, &tod, 4);
                                        } else {
                                            memset(p_val, 0, size);
                                        }
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;
                                }

                                case ZBM_ATTR_TYPE_DATE: {
                                    if (cJSON_IsString(j_val)) {
                                        int year, month, day;
                                        if (sscanf(j_val->valuestring, "%d-%d-%d", &year, &month, &day) == 3) {
                                            uint8_t y = (uint8_t)(year - 2000);
                                            uint32_t date = (y << 24) | (month << 16) | (day << 8);
                                            memcpy(p_val, &date, 4);
                                        } else {
                                            memset(p_val, 0, size);
                                        }
                                    } else {
                                        memset(p_val, 0, size);
                                    }
                                    break;
                                }

                                // Для сложных типов — обнуляем
                                case ZBM_ATTR_TYPE_ARRAY:
                                case ZBM_ATTR_TYPE_T16BIT_ARRAY:
                                case ZBM_ATTR_TYPE_T32BIT_ARRAY:
                                case ZBM_ATTR_TYPE_STRUCTURE:
                                case ZBM_ATTR_TYPE_SET:
                                case ZBM_ATTR_TYPE_BAG:
                                default:
                                    memset(p_val, 0, size);
                                    break;
                            }
                        }

                        cluster->attr_array[a_i] = attr;
                    }
                }

                // === Команды ===
                cJSON* j_cmds = cJSON_GetObjectItem(j_cl, "commands");
                if (j_cmds && cJSON_IsArray(j_cmds)) {
                    cluster->standart_cmd_count = (uint8_t)cJSON_GetArraySize(j_cmds);
                    uint8_t count_out;
                    cluster->standart_cmd_array = zbm_create_standard_command_array(cluster_id, role_mask, &count_out);
                    // Копируем GUID
                    for (int c_i = 0; c_i < cluster->standart_cmd_count; c_i++) {
                        cJSON* j_cmd = cJSON_GetArrayItem(j_cmds, c_i);
                        if (!j_cmd) continue;
                        cJSON* j_guid = cJSON_GetObjectItem(j_cmd, "guid");
                        if (j_guid && cJSON_IsString(j_guid) && cluster->standart_cmd_array[c_i]) {
                            strncpy(cluster->standart_cmd_array[c_i]->guid, j_guid->valuestring, sizeof(cluster->standart_cmd_array[c_i]->guid) - 1);
                        }
                    }
                }

                // === Кастомные репорты ===
                cJSON* j_reps = cJSON_GetObjectItem(j_cl, "custom_reports");
                if (j_reps && cJSON_IsArray(j_reps)) {
                    cluster->custom_report_cmd_count = (uint8_t)cJSON_GetArraySize(j_reps);
                    if (cluster->custom_report_cmd_count == 0) {
                        cluster->custom_report_cmd_array = NULL;
                    } else {
                        cluster->custom_report_cmd_array = calloc(cluster->custom_report_cmd_count, sizeof(zbm_cluster_custom_report_cmd_t*));
                        if (!cluster->custom_report_cmd_array) {
                            ESP_LOGE(TAG, "Failed: calloc failed for custom_report_cmd_array in custom cluster (cluster 0x%04x, ep %d)", cluster_id, ep->id);
                            goto error;
                        }
                    }

                    for (int r_i = 0; r_i < cluster->custom_report_cmd_count; r_i++) {
                        cJSON* j_rep = cJSON_GetArrayItem(j_reps, r_i);
                        if (!j_rep || !cJSON_IsObject(j_rep)) continue;

                        zbm_cluster_custom_report_cmd_t* rep = calloc(1, sizeof(zbm_cluster_custom_report_cmd_t));
                        if (!rep) continue;

                        cJSON* j_rid = cJSON_GetObjectItem(j_rep, "id");
                        if (j_rid && cJSON_IsNumber(j_rid)) rep->id = (uint8_t)j_rid->valueint;

                        cJSON* j_name = cJSON_GetObjectItem(j_rep, "name");
                        if (j_name && cJSON_IsString(j_name)) rep->friendlyname = safe_strdup(j_name->valuestring);

                        cJSON* j_guid = cJSON_GetObjectItem(j_rep, "guid");
                        if (j_guid && cJSON_IsString(j_guid)) {
                            strncpy(rep->guid, j_guid->valuestring, sizeof(rep->guid) - 1);
                        }

                        cJSON* j_type = cJSON_GetObjectItem(j_rep, "type");
                        if (j_type && cJSON_IsNumber(j_type)) rep->data_type = (zbm_cmd_data_types_t)j_type->valueint;

                        cJSON* j_size = cJSON_GetObjectItem(j_rep, "size");
                        if (j_size && cJSON_IsNumber(j_size)) rep->data_size = (uint16_t)j_size->valueint;

                        if (rep->data_size > 0) {
                            rep->p_value = calloc(1, rep->data_size);
                            if (!rep->p_value) {
                                free(rep->friendlyname);
                                free(rep);
                                continue;
                            }
                        }

                        cJSON* j_val = cJSON_GetObjectItem(j_rep, "value");
                        if (j_val && rep->p_value && rep->data_size == 1 && cJSON_IsNumber(j_val)) {
                            uint8_t v = (uint8_t)j_val->valueint;
                            memcpy(rep->p_value, &v, 1);
                        } else if (j_val && cJSON_IsString(j_val) && rep->p_value && rep->data_size > 1) {
                            // Пока только hex-строки для octet и подобных
                            const char* hex = j_val->valuestring;
                            int len = strlen(hex);
                            if (len == rep->data_size * 2) {
                                for (int i = 0; i < rep->data_size; i++) {
                                    sscanf(hex + i*2, "%02hhX", &((uint8_t*)rep->p_value)[i]);
                                }
                            }
                        }
                    

                        cluster->custom_report_cmd_array[r_i] = rep;
                    }
                }
                ep->custom_cluster_array[cl_i] = cluster;
            }
        }

        dev->endpoints_array[ep_i] = ep;
    }

    if (zbm_device_add_to_devdb_safe(dev))
    {
        dev->last_guid_update_short_addr = dev->short_addr;
    }
    //zbm_guid_db_update_device_guids_safe(dev);
    return dev;

error:
    ESP_LOGW("ZBM", "Failed to create device from JSON error:");
    if (dev) zbm_free_dev_t(dev);
    return NULL;
    
}
