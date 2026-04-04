#include "zbm_dev_to_json.h"

#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char* TAG = "ZBM_DEV_TO_JSON";
// === Функция: сериализация устройства в cJSON (полная структура) ===
cJSON* device_to_json(zbm_dev_t* dev) {
    if (!dev) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "name", dev->friendly_name);

    // short_addr как строка "0x1234"
    char short_addr_str[7];
    snprintf(short_addr_str, sizeof(short_addr_str), "0x%04X", dev->short_addr);
    cJSON_AddStringToObject(root, "short_addr", short_addr_str);

    cJSON_AddNumberToObject(root, "lqi", dev->lqi);
    cJSON_AddBoolToObject(root, "is_online", dev->is_online);
    cJSON_AddNumberToObject(root, "last_seen_ms", dev->last_seen_ms);
    cJSON_AddNumberToObject(root, "device_timeout_ms", dev->device_timeout_ms);
    cJSON_AddNumberToObject(root, "manufacturer_code", dev->manufacturer_code);
    cJSON_AddBoolToObject(root, "has_pending_read", dev->has_pending_read);
    cJSON_AddBoolToObject(root, "has_pending_response", dev->has_pending_response);
    cJSON_AddNumberToObject(root, "last_pending_read_ms", dev->last_pending_read_ms);
    cJSON_AddNumberToObject(root, "index_in_array", dev->index_in_array);

    // IEEE-адрес
    char ieee_str[24];
    snprintf(ieee_str, sizeof(ieee_str), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             dev->ieee_addr[0], dev->ieee_addr[1], dev->ieee_addr[2], dev->ieee_addr[3],
             dev->ieee_addr[4], dev->ieee_addr[5], dev->ieee_addr[6], dev->ieee_addr[7]);
    cJSON_AddStringToObject(root, "ieee_addr", ieee_str);

    // Эндпоинты
    cJSON* endpoints = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "endpoints", endpoints);

    for (uint8_t ep_idx = 0; ep_idx < dev->endpoints_count; ep_idx++) {
        zbm_dev_endpoint_t* ep = dev->endpoints_array[ep_idx];
        if (!ep) continue;

        cJSON* jep = cJSON_CreateObject();
        cJSON_AddNumberToObject(jep, "id", ep->id);
        cJSON_AddNumberToObject(jep, "device_id", ep->device_id);
        cJSON_AddStringToObject(jep, "device_type", get_device_type_name(ep->device_id));
        cJSON_AddStringToObject(jep, "name", ep->friendlyname ? ep->friendlyname : "");
        cJSON_AddBoolToObject(jep, "is_use_on_device", ep->is_use_on_device);

        // Стандартные кластеры
        cJSON* clusters = cJSON_CreateArray();
        cJSON_AddItemToObject(jep, "standard_clusters", clusters);
        for (int cl_idx = 0; cl_idx < ep->standart_cluster_count; cl_idx++) {
            zbm_standart_cluster_t* cluster = ep->standart_cluster_array[cl_idx];
            if (!cluster) continue;

            cJSON* jcl = cJSON_CreateObject();
            cJSON_AddNumberToObject(jcl, "id", cluster->id);
            cJSON_AddStringToObject(jcl, "role", (cluster->role_mask == ZBM_CLUSTER_ROLE_SERVER) ? "server" : "client");
            cJSON_AddStringToObject(jcl, "name", cluster->friendlyname ? cluster->friendlyname : "StandardCluster");

            // Атрибуты
            cJSON* attrs = cJSON_CreateArray();
            cJSON_AddItemToObject(jcl, "attributes", attrs);

            for (int a_idx = 0; a_idx < cluster->attr_count; a_idx++) {
                zbm_cluster_attribute_t* attr = cluster->attr_array[a_idx];
                if (!attr) continue;

                cJSON* jattr = cJSON_CreateObject();
                cJSON_AddNumberToObject(jattr, "id", attr->id);
                cJSON_AddStringToObject(jattr, "name", attr->friendlyname ? attr->friendlyname : "UnknownAttr");
                cJSON_AddStringToObject(jattr, "guid", attr->guid);
                cJSON_AddNumberToObject(jattr, "type", attr->data_type);
                cJSON_AddNumberToObject(jattr, "size", attr->data_size);

                // === Обработка значения атрибута в зависимости от типа ===
                switch (attr->data_type) {
                    // ===== Булево =====
                    case ZBM_ATTR_TYPE_BOOL: {
                        bool val = *(bool*)attr->p_value;
                        cJSON_AddBoolToObject(jattr, "value", val);
                        break;
                    }

                    // ===== 8-битные целые (беззнаковые/знаковые) =====
                    case ZBM_ATTR_TYPE_U8: {
                        uint8_t val = *(uint8_t*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", val);
                        break;
                    }
                    case ZBM_ATTR_TYPE_S8: {
                        int8_t val = *(int8_t*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", val);
                        break;
                    }

                    // ===== 16-битные =====
                    case ZBM_ATTR_TYPE_U16: {
                        uint16_t val = *(uint16_t*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", val);
                        break;
                    }
                    case ZBM_ATTR_TYPE_S16: {
                        int16_t val = *(int16_t*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", val);
                        break;
                    }
                    case ZBM_ATTR_TYPE_T16BIT_ENUM:
                    case ZBM_ATTR_TYPE_T8BIT_ENUM: {
                        uint16_t val = (attr->data_type == ZBM_ATTR_TYPE_T8BIT_ENUM) ?
                                    *(uint8_t*)attr->p_value : *(uint16_t*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", val);
                        break;
                    }

                    // ===== 32-битные =====
                    case ZBM_ATTR_TYPE_U24:  // Трактуем как U32
                    case ZBM_ATTR_TYPE_U32: {
                        uint32_t val = 0;
                        memcpy(&val, attr->p_value, attr->data_size);
                        cJSON_AddNumberToObject(jattr, "value", (double)val); // Чтобы избежать потерь
                        break;
                    }
                    case ZBM_ATTR_TYPE_S24:  // Трактуем как S32
                    case ZBM_ATTR_TYPE_S32: {
                        int32_t val = 0;
                        memcpy(&val, attr->p_value, attr->data_size);
                        cJSON_AddNumberToObject(jattr, "value", (double)val);
                        break;
                    }

                    // ===== 64-битные =====
                    case ZBM_ATTR_TYPE_U40:
                    case ZBM_ATTR_TYPE_U48:
                    case ZBM_ATTR_TYPE_U56:
                    case ZBM_ATTR_TYPE_U64: {
                        uint64_t val = 0;
                        memcpy(&val, attr->p_value, attr->data_size);
                        char num_str[21];
                        snprintf(num_str, sizeof(num_str), "%" PRIu64, val);
                        cJSON_AddStringToObject(jattr, "value", num_str); // JSON не поддерживает U64 как число
                        break;
                    }
                    case ZBM_ATTR_TYPE_S40:
                    case ZBM_ATTR_TYPE_S48:
                    case ZBM_ATTR_TYPE_S56:
                    case ZBM_ATTR_TYPE_S64: {
                        int64_t val = 0;
                        memcpy(&val, attr->p_value, attr->data_size);
                        char num_str[22];
                        snprintf(num_str, sizeof(num_str), "%" PRId64, val);
                        cJSON_AddStringToObject(jattr, "value", num_str);
                        break;
                    }

                    // ===== Плавающая точка =====
                    case ZBM_ATTR_TYPE_SEMI: // 2-byte float — редко используется
                    case ZBM_ATTR_TYPE_SINGLE: { // float (4 байта)
                        float val = *(float*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", (double)val);
                        break;
                    }
                    case ZBM_ATTR_TYPE_DOUBLE: { // double (8 байт)
                        double val = *(double*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", val);
                        break;
                    }

                    // ===== Строки =====
                    case ZBM_ATTR_TYPE_CHAR_STRING:
                    case ZBM_ATTR_TYPE_LONG_CHAR_STRING: {
                        uint8_t* raw = (uint8_t*)attr->p_value;
                        if (attr->data_size == 0 || raw == NULL) {
                            cJSON_AddStringToObject(jattr, "value", "");
                            break;
                        }

                        uint8_t len = raw[0];  // Первый байт — длина
                        if (len == 0) {
                            cJSON_AddStringToObject(jattr, "value", "");
                            break;
                        }

                        // Убедимся, что данных достаточно
                        if (len > attr->data_size - 1) {
                            len = attr->data_size - 1;  // Обрезаем, если неконсистентно
                        }

                        char* safe_str = strndup((char*)raw + 1, len);  // Пропускаем первый байт
                        if (safe_str) {
                            cJSON_AddStringToObject(jattr, "value", safe_str);
                            free(safe_str);
                        } else {
                            cJSON_AddStringToObject(jattr, "value", "<string>");
                        }
                        break;
                    }

                    case ZBM_ATTR_TYPE_OCTET_STRING:
                    case ZBM_ATTR_TYPE_LONG_OCTET_STRING: {
                        // Выводим как hex-строку
                        char* hex = malloc(attr->data_size * 2 + 1);
                        if (hex) {
                            for (int i = 0; i < attr->data_size; i++) {
                                sprintf(&hex[i * 2], "%02X", ((uint8_t*)attr->p_value)[i]);
                            }
                            cJSON_AddStringToObject(jattr, "value", hex);
                            free(hex);
                        } else {
                            cJSON_AddStringToObject(jattr, "value", "<octet>");
                        }
                        break;
                    }

                    // ===== IEEE Address (U64) =====
                    case ZBM_ATTR_TYPE_IEEE_ADDR: {
                        uint8_t* addr = (uint8_t*)attr->p_value;
                        char ieee_str[24];
                        snprintf(ieee_str, sizeof(ieee_str), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
                        cJSON_AddStringToObject(jattr, "value", ieee_str);
                        break;
                    }

                    // ===== Cluster ID / Attribute ID =====
                    case ZBM_ATTR_TYPE_CLUSTER_ID: {
                        uint16_t id = *(uint16_t*)attr->p_value;
                        char id_str[7];
                        snprintf(id_str, sizeof(id_str), "0x%04X", id);
                        cJSON_AddStringToObject(jattr, "value", id_str);
                        break;
                    }
                    case ZBM_ATTR_TYPE_ATTRIBUTE_ID: {
                        uint16_t id = *(uint16_t*)attr->p_value;
                        char id_str[7];
                        snprintf(id_str, sizeof(id_str), "0x%04X", id);
                        cJSON_AddStringToObject(jattr, "value", id_str);
                        break;
                    }

                    // ===== Время и дата =====
                    case ZBM_ATTR_TYPE_UTC_TIME: {
                        uint32_t utc = *(uint32_t*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", (double)utc);
                        break;
                    }
                    case ZBM_ATTR_TYPE_TIME_OF_DAY: {
                        uint32_t tod = *(uint32_t*)attr->p_value;
                        uint8_t hour = (tod >> 24) & 0xFF;
                        uint8_t min  = (tod >> 16) & 0xFF;
                        uint8_t sec  = (tod >>  8) & 0xFF;
                        uint8_t hun  = (tod      ) & 0xFF;
                        char time_str[16];
                        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d.%02d", hour, min, sec, hun);
                        cJSON_AddStringToObject(jattr, "value", time_str);
                        break;
                    }
                    case ZBM_ATTR_TYPE_DATE: {
                        uint32_t date = *(uint32_t*)attr->p_value;
                        uint8_t year  = (date >> 24) & 0xFF;
                        uint8_t month = (date >> 16) & 0xFF;
                        uint8_t day   = (date >>  8) & 0xFF;
                        uint8_t dow   = (date      ) & 0xFF;
                        char date_str[16];
                        static const char* dow_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
                        snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d (%s)", 2000 + year, month, day, dow_names[dow % 7]);
                        cJSON_AddStringToObject(jattr, "value", date_str);
                        break;
                    }

                    // ===== Массивы и структуры (нельзя вывести напрямую) =====
                    case ZBM_ATTR_TYPE_ARRAY:
                    case ZBM_ATTR_TYPE_T16BIT_ARRAY:
                    case ZBM_ATTR_TYPE_T32BIT_ARRAY:
                    case ZBM_ATTR_TYPE_STRUCTURE:
                    case ZBM_ATTR_TYPE_SET:
                    case ZBM_ATTR_TYPE_BAG:
                        cJSON_AddStringToObject(jattr, "value", "<complex>");
                        break;

                    // ===== Неизвестный или необработанный тип =====
                    default:
                        cJSON_AddStringToObject(jattr, "value", "<raw>");
                        break;
                }

                cJSON_AddItemToArray(attrs, jattr);
            }

            // Команды
            cJSON* cmds = cJSON_CreateArray();
            cJSON_AddItemToObject(jcl, "commands", cmds);

            for (int c_idx = 0; c_idx < cluster->standart_cmd_count; c_idx++) {
                zbm_cluster_standart_cmd_t* cmd = cluster->standart_cmd_array[c_idx];
                if (!cmd) continue;

                cJSON* jcmd = cJSON_CreateObject();
                cJSON_AddNumberToObject(jcmd, "id", cmd->id);
                cJSON_AddStringToObject(jcmd, "name", cmd->friendlyname ? cmd->friendlyname : "Unknown");
                cJSON_AddStringToObject(jcmd, "guid", cmd->guid);
                // === Добавляем параметры ===
                if (cmd->param_count > 0) {
                    cJSON* params = cJSON_CreateArray();
                    cJSON_AddItemToObject(jcmd, "params", params);
                    for (int p = 0; p < cmd->param_count; p++) {
                        zbm_cluster_cmd_param_t* param = cmd->params[p];
                        if (!param) continue;

                        cJSON* jparam = cJSON_CreateObject();
                        cJSON_AddStringToObject(jparam, "name", param->friendlyname ? param->friendlyname : "UnknownParam");
                        cJSON_AddNumberToObject(jparam, "type", param->data_type);
                        cJSON_AddNumberToObject(jparam, "size", param->data_size);

                        // Опционально: значение (если нужно)
                        if (param->p_value && param->data_size == 1) {
                            cJSON_AddNumberToObject(jparam, "value", *(uint8_t*)param->p_value);
                        } else if (param->data_size > 1) {
                            cJSON_AddStringToObject(jparam, "value", "<buffer>");
                        }

                        cJSON_AddItemToArray(params, jparam);
                    }
                }
                cJSON_AddItemToArray(cmds, jcmd);
            }

            // Кастомные репорты (Tuya 0xFD и др.)
            cJSON* custom_reports = cJSON_CreateArray();
            cJSON_AddItemToObject(jcl, "custom_reports", custom_reports);

            for (int r_idx = 0; r_idx < cluster->custom_report_cmd_count; r_idx++) {
                zbm_cluster_custom_report_cmd_t* rep = cluster->custom_report_cmd_array[r_idx];
                if (!rep) continue;

                cJSON* jrep = cJSON_CreateObject();
                cJSON_AddNumberToObject(jrep, "id", rep->id);
                cJSON_AddStringToObject(jrep, "name", rep->friendlyname ? rep->friendlyname : "Unknown");
                cJSON_AddStringToObject(jrep, "guid", rep->guid);
                cJSON_AddNumberToObject(jrep, "type", rep->data_type);
                cJSON_AddNumberToObject(jrep, "size", rep->data_size);

                if (rep->data_size == 1) {
                    cJSON_AddNumberToObject(jrep, "value", *(uint8_t*)rep->p_value);
                } else {
                    cJSON_AddStringToObject(jrep, "value", "<raw>");
                }

                cJSON_AddItemToArray(custom_reports, jrep);
            }

            cJSON_AddItemToArray(clusters, jcl);
        }

        // Кастомные кластеры
        cJSON* cust_clusters = cJSON_CreateArray();
        cJSON_AddItemToObject(jep, "custom_clusters", cust_clusters);
        for (int cl_idx = 0; cl_idx < ep->custom_cluster_count; cl_idx++) {
            zbm_custom_cluster_t* cluster = ep->custom_cluster_array[cl_idx];
            if (!cluster) continue;

            cJSON* jcl = cJSON_CreateObject();
            cJSON_AddNumberToObject(jcl, "id", cluster->id);
            cJSON_AddStringToObject(jcl, "role", (cluster->role_mask == ZBM_CLUSTER_ROLE_SERVER) ? "server" : "client");
            cJSON_AddStringToObject(jcl, "name", cluster->friendlyname ? cluster->friendlyname : "CustomCluster");

            // Атрибуты
            cJSON* attrs = cJSON_CreateArray();
            cJSON_AddItemToObject(jcl, "attributes", attrs);

            for (int a_idx = 0; a_idx < cluster->attr_count; a_idx++) {
                zbm_cluster_attribute_t* attr = cluster->attr_array[a_idx];
                if (!attr) continue;

                cJSON* jattr = cJSON_CreateObject();
                cJSON_AddNumberToObject(jattr, "id", attr->id);
                cJSON_AddStringToObject(jattr, "name", attr->friendlyname ? attr->friendlyname : "UnknownAttr");
                cJSON_AddStringToObject(jattr, "guid", attr->guid);
                cJSON_AddNumberToObject(jattr, "type", attr->data_type);
                cJSON_AddNumberToObject(jattr, "size", attr->data_size);

                // === Копируем обработку значения из оригинала ===
                switch (attr->data_type) {
                    case ZBM_ATTR_TYPE_BOOL: {
                        bool val = *(bool*)attr->p_value;
                        cJSON_AddBoolToObject(jattr, "value", val);
                        break;
                    }
                    case ZBM_ATTR_TYPE_U8: {
                        uint8_t val = *(uint8_t*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", val);
                        break;
                    }
                    case ZBM_ATTR_TYPE_S8: {
                        int8_t val = *(int8_t*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", val);
                        break;
                    }
                    case ZBM_ATTR_TYPE_U16:
                    case ZBM_ATTR_TYPE_T16BIT_ENUM: {
                        uint16_t val = *(uint16_t*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", val);
                        break;
                    }
                    case ZBM_ATTR_TYPE_S16:
                    case ZBM_ATTR_TYPE_T8BIT_ENUM: {
                        int16_t val = *(int16_t*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", val);
                        break;
                    }
                    case ZBM_ATTR_TYPE_U24:
                    case ZBM_ATTR_TYPE_U32: {
                        uint32_t val = 0;
                        memcpy(&val, attr->p_value, attr->data_size);
                        cJSON_AddNumberToObject(jattr, "value", (double)val);
                        break;
                    }
                    case ZBM_ATTR_TYPE_S24:
                    case ZBM_ATTR_TYPE_S32: {
                        int32_t val = 0;
                        memcpy(&val, attr->p_value, attr->data_size);
                        cJSON_AddNumberToObject(jattr, "value", (double)val);
                        break;
                    }
                    case ZBM_ATTR_TYPE_U40:
                    case ZBM_ATTR_TYPE_U48:
                    case ZBM_ATTR_TYPE_U56:
                    case ZBM_ATTR_TYPE_U64: {
                        uint64_t val = 0;
                        memcpy(&val, attr->p_value, attr->data_size);
                        char num_str[21];
                        snprintf(num_str, sizeof(num_str), "%" PRIu64, val);
                        cJSON_AddStringToObject(jattr, "value", num_str);
                        break;
                    }
                    case ZBM_ATTR_TYPE_S40:
                    case ZBM_ATTR_TYPE_S48:
                    case ZBM_ATTR_TYPE_S56:
                    case ZBM_ATTR_TYPE_S64: {
                        int64_t val = 0;
                        memcpy(&val, attr->p_value, attr->data_size);
                        char num_str[22];
                        snprintf(num_str, sizeof(num_str), "%" PRId64, val);
                        cJSON_AddStringToObject(jattr, "value", num_str);
                        break;
                    }
                    case ZBM_ATTR_TYPE_SEMI:
                    case ZBM_ATTR_TYPE_SINGLE: {
                        float val = *(float*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", (double)val);
                        break;
                    }
                    case ZBM_ATTR_TYPE_DOUBLE: {
                        double val = *(double*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", val);
                        break;
                    }
                    case ZBM_ATTR_TYPE_CHAR_STRING:
                    case ZBM_ATTR_TYPE_LONG_CHAR_STRING: {
                        uint8_t* raw = (uint8_t*)attr->p_value;
                        if (attr->data_size == 0 || raw == NULL) {
                            cJSON_AddStringToObject(jattr, "value", "");
                            break;
                        }

                        uint8_t len = raw[0];  // Первый байт — длина
                        if (len == 0) {
                            cJSON_AddStringToObject(jattr, "value", "");
                            break;
                        }

                        // Убедимся, что данных достаточно
                        if (len > attr->data_size - 1) {
                            len = attr->data_size - 1;  // Обрезаем, если неконсистентно
                        }

                        char* safe_str = strndup((char*)raw + 1, len);  // Пропускаем первый байт
                        if (safe_str) {
                            cJSON_AddStringToObject(jattr, "value", safe_str);
                            free(safe_str);
                        } else {
                            cJSON_AddStringToObject(jattr, "value", "<string>");
                        }
                        break;
                    }
                    case ZBM_ATTR_TYPE_OCTET_STRING:
                    case ZBM_ATTR_TYPE_LONG_OCTET_STRING: {
                        char* hex = malloc(attr->data_size * 2 + 1);
                        if (hex) {
                            for (int i = 0; i < attr->data_size; i++) {
                                sprintf(&hex[i * 2], "%02X", ((uint8_t*)attr->p_value)[i]);
                            }
                            cJSON_AddStringToObject(jattr, "value", hex);
                            free(hex);
                        } else {
                            cJSON_AddStringToObject(jattr, "value", "<octet>");
                        }
                        break;
                    }
                    case ZBM_ATTR_TYPE_IEEE_ADDR: {
                        uint8_t* addr = (uint8_t*)attr->p_value;
                        char ieee_str[24];
                        snprintf(ieee_str, sizeof(ieee_str), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
                        cJSON_AddStringToObject(jattr, "value", ieee_str);
                        break;
                    }
                    case ZBM_ATTR_TYPE_CLUSTER_ID:
                    case ZBM_ATTR_TYPE_ATTRIBUTE_ID: {
                        uint16_t id = *(uint16_t*)attr->p_value;
                        char id_str[7];
                        snprintf(id_str, sizeof(id_str), "0x%04X", id);
                        cJSON_AddStringToObject(jattr, "value", id_str);
                        break;
                    }
                    case ZBM_ATTR_TYPE_UTC_TIME: {
                        uint32_t utc = *(uint32_t*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", (double)utc);
                        break;
                    }
                    case ZBM_ATTR_TYPE_TIME_OF_DAY: {
                        uint32_t tod = *(uint32_t*)attr->p_value;
                        uint8_t hour = (tod >> 24) & 0xFF;
                        uint8_t min  = (tod >> 16) & 0xFF;
                        uint8_t sec  = (tod >>  8) & 0xFF;
                        uint8_t hun  = (tod      ) & 0xFF;
                        char time_str[16];
                        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d.%02d", hour, min, sec, hun);
                        cJSON_AddStringToObject(jattr, "value", time_str);
                        break;
                    }
                    case ZBM_ATTR_TYPE_DATE: {
                        uint32_t date = *(uint32_t*)attr->p_value;
                        uint8_t year  = (date >> 24) & 0xFF;
                        uint8_t month = (date >> 16) & 0xFF;
                        uint8_t day   = (date >>  8) & 0xFF;
                        uint8_t dow   = (date      ) & 0xFF;
                        char date_str[16];
                        static const char* dow_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
                        snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d (%s)", 2000 + year, month, day, dow_names[dow % 7]);
                        cJSON_AddStringToObject(jattr, "value", date_str);
                        break;
                    }
                    case ZBM_ATTR_TYPE_ARRAY:
                    case ZBM_ATTR_TYPE_T16BIT_ARRAY:
                    case ZBM_ATTR_TYPE_T32BIT_ARRAY:
                    case ZBM_ATTR_TYPE_STRUCTURE:
                    case ZBM_ATTR_TYPE_SET:
                    case ZBM_ATTR_TYPE_BAG:
                        cJSON_AddStringToObject(jattr, "value", "<complex>");
                        break;
                    default:
                        cJSON_AddStringToObject(jattr, "value", "<raw>");
                        break;
                }

                cJSON_AddItemToArray(attrs, jattr);
            }

            // Команды (стандартные)
            cJSON* cmds = cJSON_CreateArray();
            cJSON_AddItemToObject(jcl, "commands", cmds);
            for (int c_idx = 0; c_idx < cluster->standart_cmd_count; c_idx++) {
                zbm_cluster_standart_cmd_t* cmd = cluster->standart_cmd_array[c_idx];
                if (!cmd) continue;
                cJSON* jcmd = cJSON_CreateObject();
                cJSON_AddNumberToObject(jcmd, "id", cmd->id);
                cJSON_AddStringToObject(jcmd, "name", cmd->friendlyname ? cmd->friendlyname : "Unknown");
                cJSON_AddStringToObject(jcmd, "guid", cmd->guid);
                cJSON_AddItemToArray(cmds, jcmd);
            }

            // Кастомные репорты
            cJSON* custom_reports = cJSON_CreateArray();
            cJSON_AddItemToObject(jcl, "custom_reports", custom_reports);
            for (int r_idx = 0; r_idx < cluster->custom_report_cmd_count; r_idx++) {
                zbm_cluster_custom_report_cmd_t* rep = cluster->custom_report_cmd_array[r_idx];
                if (!rep) continue;
                cJSON* jrep = cJSON_CreateObject();
                cJSON_AddNumberToObject(jrep, "id", rep->id);
                cJSON_AddStringToObject(jrep, "name", rep->friendlyname ? rep->friendlyname : "Unknown");
                cJSON_AddStringToObject(jrep, "guid", rep->guid);
                cJSON_AddNumberToObject(jrep, "type", rep->data_type);
                cJSON_AddNumberToObject(jrep, "size", rep->data_size);

                if (rep->data_size == 1) {
                    cJSON_AddNumberToObject(jrep, "value", *(uint8_t*)rep->p_value);
                } else {
                    cJSON_AddStringToObject(jrep, "value", "<raw>");
                }
                cJSON_AddItemToArray(custom_reports, jrep);
            }

            cJSON_AddItemToArray(cust_clusters, jcl);  // ← добавляем в "ZCL_Clusters" или "NoZCL_Clusters"? 
            // Решение: будем использовать один массив "clusters", но ниже разделим
        }
        cJSON_AddItemToArray(endpoints, jep);
    }

    return root;  // ← возвращаем объект JSON, не строку!
}

// Вспомогательная: создаёт краткую версию устройства
cJSON* device_to_brief_json(zbm_dev_t* dev) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj) return NULL;

    char ieee_str[24];
    snprintf(ieee_str, sizeof(ieee_str), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             dev->ieee_addr[0], dev->ieee_addr[1], dev->ieee_addr[2], dev->ieee_addr[3],
             dev->ieee_addr[4], dev->ieee_addr[5], dev->ieee_addr[6], dev->ieee_addr[7]);

    cJSON_AddStringToObject(obj, "ieee", ieee_str);
    cJSON_AddNumberToObject(obj, "short", dev->short_addr);

    if (dev->friendly_name && strlen(dev->friendly_name) > 0) {
        cJSON_AddStringToObject(obj, "friendly_name", dev->friendly_name);
    } else {
        cJSON_AddStringToObject(obj, "friendly_name", ieee_str);
    }

    // Онлайн/оффлайн
    bool online = /* твоя логика */ (dev->last_seen_ms > (esp_log_timestamp() - 300 * 1000)); // за последние 5 минут
    cJSON_AddBoolToObject(obj, "online", online);

    cJSON_AddNumberToObject(obj, "last_seen", dev->last_seen_ms);
    cJSON_AddNumberToObject(obj, "linkquality", dev->lqi);

    return obj;
}


