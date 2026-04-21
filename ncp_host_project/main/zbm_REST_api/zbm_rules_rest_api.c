#include "cJSON.h"
#include "inttypes.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "string.h"
#include "zbm_spiffs_helper.h"
#include "zbm_web_server.h" // для ws уведомлений
#include "zbm_rest_api.h"
#include "esp_random.h"
#include "zbm_spiffs_helper.h"
#include "zbm_attr_types.h"
#include "zbm_automation_v2.h"

static const char* TAG = "ZBM_RULES";

static esp_err_t set_rule_enabled_state(httpd_req_t* req, bool enabled);
// === Вспомогательная: чтение JSON из файла ===


// === Генерация ID правила ===
static char* generate_rule_id(char* buf, size_t size) {
    uint32_t rand = esp_random();
    snprintf(buf, size, "%08" PRIx32, rand);
    return buf;
}

// Возвращает полный путь к правилу по ID или NULL, если не найдено
static bool get_rule_path_by_id(const char* rule_id, char* out_path, size_t path_size) {
    char index_path[64];
    snprintf(index_path, sizeof(index_path), "%s", ZBM_RULES_INDEX_FILE);

    cJSON* index = read_json_from_file(index_path);
    if (!index) return false;

    bool found = false;
    for (int i = 0; i < cJSON_GetArraySize(index); i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        cJSON* item_id = cJSON_GetObjectItem(item, "id");
        cJSON* item_path = cJSON_GetObjectItem(item, "path");

        if (item_id && item_path && 
            strcmp(item_id->valuestring, rule_id) == 0 &&
            item_path->valuestring != NULL &&
            strlen(item_path->valuestring) > 0) {

            // Защита от path traversal: путь должен начинаться с SPIFFS_ZBM_CONF_MOUNT_POINT
            if (strncmp(item_path->valuestring, SPIFFS_ZBM_CONF_MOUNT_POINT, strlen(SPIFFS_ZBM_CONF_MOUNT_POINT)) == 0) {
                strlcpy(out_path, item_path->valuestring, path_size);
                found = true;
                break;
            } else {
                ESP_LOGW(TAG, "Rule path outside allowed directory: %s", item_path->valuestring);
            }
        }
    }

    cJSON_Delete(index);
    return found && strlen(out_path) > 0;
}



esp_err_t zbm_rest_api_get_vars_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/get/vars - Fetching all variables");

    cJSON* arr = cJSON_CreateArray();
    for (int i = 0; i < ZB_AUTO_VAR_COUNT; i++) {
        cJSON* v = cJSON_CreateObject();

        // Защита: имя может быть NULL
        const char* name = zbm_vars[i].name;
        cJSON_AddStringToObject(v, "name", name);

        cJSON_AddNumberToObject(v, "idx", i);
        cJSON_AddNumberToObject(v, "type", zbm_vars[i].data_type);

        char value_str[64] = {0};

        switch (zbm_vars[i].data_type) {
            case ZBM_ATTR_TYPE_U8: {
                uint8_t val = *(uint8_t*)zbm_vars[i].p_value;
                cJSON_AddNumberToObject(v, "value", val);
                snprintf(value_str, sizeof(value_str), "U8=%u", val);
                break;
            }
            case ZBM_ATTR_TYPE_S8: {
                int8_t val = *(int8_t*)zbm_vars[i].p_value;
                cJSON_AddNumberToObject(v, "value", val);
                snprintf(value_str, sizeof(value_str), "S8=%d", val);
                break;
            }
            case ZBM_ATTR_TYPE_BOOL: {
                bool val = *(bool*)zbm_vars[i].p_value;
                cJSON_AddBoolToObject(v, "value", val);
                snprintf(value_str, sizeof(value_str), "BOOL=%s", val ? "true" : "false");
                break;
            }
            case ZBM_ATTR_TYPE_U16: {
                uint16_t val = *(uint16_t*)zbm_vars[i].p_value;
                cJSON_AddNumberToObject(v, "value", val);
                snprintf(value_str, sizeof(value_str), "U16=%u", val);
                break;
            }
            case ZBM_ATTR_TYPE_S16: {
                int16_t val = *(int16_t*)zbm_vars[i].p_value;
                cJSON_AddNumberToObject(v, "value", val);
                snprintf(value_str, sizeof(value_str), "S16=%d", val);
                break;
            }
            case ZBM_ATTR_TYPE_CHAR_STRING:
            case ZBM_ATTR_TYPE_LONG_CHAR_STRING: {
                char* str = (char*)zbm_vars[i].p_value;
                if (!str) {
                    cJSON_AddStringToObject(v, "value", "");
                    snprintf(value_str, sizeof(value_str), "STR=(null)");
                } else {
                    cJSON_AddStringToObject(v, "value", str);
                    if (zbm_vars[i].data_type == ZBM_ATTR_TYPE_CHAR_STRING) {
                        snprintf(value_str, sizeof(value_str), "STR=\"%s\"", str);
                    } else {
                        snprintf(value_str, sizeof(value_str), "LSTR(len=%d)", strlen(str));
                    }
                }
                break;
            }
            default: {
                uint8_t val = *(uint8_t*)zbm_vars[i].p_value;
                cJSON_AddNumberToObject(v, "value", val);
                snprintf(value_str, sizeof(value_str), "UNK=%u", val);
                break;
            }
        }

        cJSON_AddItemToArray(arr, v);

        // 🔐 Безопасный вывод имени и типа
        const char* safe_name = zbm_vars[i].name;
        const char* type_str = zbm_attr_type_to_str(zbm_vars[i].data_type);
        if (!type_str) type_str = "UNKNOWN";

        ESP_LOGI(TAG, "VAR[%d] name='%s' type=%d (%s) -> %s",
                 i, safe_name, zbm_vars[i].data_type, type_str, value_str);
    }

    char* str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    if (!str) {
        ESP_LOGE(TAG, "Failed to serialize variables to JSON");
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, str, HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "RESP /api/get/vars -> %u bytes sent", (unsigned int)strlen(str));

    free(str);
    return ESP_OK;
}

esp_err_t zbm_rest_api_post_var_handler(httpd_req_t* req) {
    uint32_t temp_value = 0;  // будет использоваться как буфер
    void* free_on_exit = NULL; // если выделяем память для строки
    char uri[64];
    strlcpy(uri, req->uri, sizeof(uri));
    int idx = atoi(uri + strlen("/api/post/var/"));

    if (idx < 0 || idx >= ZB_AUTO_VAR_COUNT) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid variable index");
        return ESP_OK;
    }

    char body[256];
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }
    body[len] = '\0';

    cJSON* json = cJSON_Parse(body);
    if (!json) {
        const char* resp = "{\"success\":false,\"error\":\"invalid_json\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }

    bool updated = false;
    const char* name = NULL;
    zbm_attr_data_types_t type = zbm_vars[idx].data_type;
    void* value_ptr = NULL;
    uint16_t value_size = 0;

    // === Парсим имя ===
    cJSON* name_obj = cJSON_GetObjectItem(json, "name");
    if (name_obj && cJSON_IsString(name_obj)) {
        name = name_obj->valuestring;
        if (strlen(name) == 0 || strlen(name) >= sizeof(zbm_vars[idx].name)) {
            cJSON_Delete(json);
            const char* resp = "{\"success\":false,\"error\":\"invalid_name_length\"}";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_send(req, resp, strlen(resp));
            return ESP_OK;
        }
    }

    // === Парсим тип ===
    cJSON* type_obj = cJSON_GetObjectItem(json, "type");
    if (type_obj && cJSON_IsNumber(type_obj)) {
        zbm_attr_data_types_t new_type = (zbm_attr_data_types_t)type_obj->valueint;
        if (new_type != ZBM_ATTR_TYPE_U8 &&
            new_type != ZBM_ATTR_TYPE_S8 &&
            new_type != ZBM_ATTR_TYPE_BOOL &&
            new_type != ZBM_ATTR_TYPE_U16 &&
            new_type != ZBM_ATTR_TYPE_S16 &&
            new_type != ZBM_ATTR_TYPE_CHAR_STRING &&
            new_type != ZBM_ATTR_TYPE_LONG_CHAR_STRING) {
            cJSON_Delete(json);
            const char* resp = "{\"success\":false,\"error\":\"unsupported_type\"}";
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_send(req, resp, strlen(resp));
            return ESP_OK;
        }
        type = new_type;
    }

    // === Парсим значение ===
    cJSON* value_obj = cJSON_GetObjectItem(json, "value");
    if (value_obj) {
        // Определяем размер
        switch (type) {
            case ZBM_ATTR_TYPE_U8:
            case ZBM_ATTR_TYPE_S8:
            case ZBM_ATTR_TYPE_BOOL:
                value_size = 1;
                break;
            case ZBM_ATTR_TYPE_U16:
            case ZBM_ATTR_TYPE_S16:
                value_size = 2;
                break;
            case ZBM_ATTR_TYPE_CHAR_STRING:
            case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
                if (cJSON_IsString(value_obj)) {
                    value_size = strlen(value_obj->valuestring) + 1;
                } else {
                    cJSON_Delete(json);
                    const char* resp = "{\"success\":false,\"error\":\"string_value_expected\"}";
                    httpd_resp_set_type(req, "application/json");
                    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                    httpd_resp_send(req, resp, strlen(resp));
                    return ESP_OK;
                }
                break;
            default:
                value_size = 1;
                break;
        }

        // Подготавливаем указатель на значение
        if (type == ZBM_ATTR_TYPE_CHAR_STRING || type == ZBM_ATTR_TYPE_LONG_CHAR_STRING) {
            if (cJSON_IsString(value_obj)) {
                value_ptr = (void*)value_obj->valuestring;
            } else if (cJSON_IsNumber(value_obj)) {
                // Число → строка
                char* str_val = malloc(16);
                if (!str_val) {
                    cJSON_Delete(json);
                    const char* resp = "{\"success\":false,\"error\":\"no_mem\"}";
                    httpd_resp_send(req, resp, strlen(resp));
                    return ESP_OK;
                }
                snprintf(str_val, 16, "%d", value_obj->valueint);
                value_ptr = str_val;
                free_on_exit = str_val; // см. ниже
            }
        } else {
            // Числовые типы: U8, S8, U16, S16
            if (cJSON_IsNumber(value_obj)) {
                if (value_size == 1) {
                    uint8_t val = (uint8_t)value_obj->valueint;
                    memcpy(&temp_value, &val, 1);
                    value_ptr = &temp_value;
                } else if (value_size == 2) {
                    uint16_t val = (uint16_t)value_obj->valueint;
                    memcpy(&temp_value, &val, 2);
                    value_ptr = &temp_value;
                }
            } else if (cJSON_IsString(value_obj)) {
                // Строка → число
                int num = atoi(value_obj->valuestring);
                if (value_size == 1) {
                    uint8_t val = (uint8_t)num;
                    memcpy(&temp_value, &val, 1);
                    value_ptr = &temp_value;
                } else if (value_size == 2) {
                    uint16_t val = (uint16_t)num;
                    memcpy(&temp_value, &val, 2);
                    value_ptr = &temp_value;
                }
            }
        }
    }

    // === Применяем изменения через безопасную функцию ===
    if (zbm_var_set_config(idx, name, type, value_ptr, value_size)) {
        updated = true;
    }

    cJSON_Delete(json);

    // === Формируем успешный ответ ===
    if (updated) {
        // Уведомление WebSocket
        cJSON* notify = cJSON_CreateObject();
        cJSON_AddNumberToObject(notify, "idx", idx);
        zbm_ws_send_sys_notify("var_updated", "Variable updated", notify);
        cJSON_Delete(notify);
    }

    const char* resp = "{\"success\":true}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));

    if (free_on_exit) {
        free(free_on_exit);
        free_on_exit = NULL;
    }
    return ESP_OK;
}

// === Обработчик: GET /api/rules — получить список правил (индекс) ===
esp_err_t zbm_rest_api_get_rules_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/rules");

    char index_path[64];
    snprintf(index_path, sizeof(index_path), "%s", ZBM_RULES_INDEX_FILE);

    cJSON* index = read_json_from_file(index_path);
    if (!index) {
        ESP_LOGW(TAG, "No rules index found, creating empty one");
        index = cJSON_CreateArray();
    }

    char* json_str = cJSON_PrintUnformatted(index);
    cJSON_Delete(index);

    if (!json_str) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    free(json_str);

    return ESP_OK;
}

// === Обработчик: GET /api/rule/:id — получить полное правило ===
esp_err_t zbm_rest_api_get_rule_by_id_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/rule/{id}");

    char uri[128];
    strlcpy(uri, req->uri, sizeof(uri));
    char* id = uri + strlen("/api/rule/");

    if (strlen(id) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing rule ID");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "REQ /api/rule/{id}: %s", id);

    // Читаем индекс, чтобы получить путь
    char index_path[64];
    snprintf(index_path, sizeof(index_path), "%s", ZBM_RULES_INDEX_FILE);

    cJSON* index = read_json_from_file(index_path);
    if (!index) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Rules index not found");
        return ESP_OK;
    }

    char rule_path[128] = {0};
    bool found = false;
    for (int i = 0; i < cJSON_GetArraySize(index); i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        cJSON* item_id = cJSON_GetObjectItem(item, "id");
        cJSON* item_path = cJSON_GetObjectItem(item, "path");

        ESP_LOGI(TAG, "Index item: id='%s', path='%s'", 
            item_id ? item_id->valuestring : "null",
            item_path ? item_path->valuestring : "null");

        if (item_id && item_path && strcmp(item_id->valuestring, id) == 0) {
            strlcpy(rule_path, item_path->valuestring, sizeof(rule_path));
            found = true;
            break;
        }
    }

    cJSON_Delete(index);

    if (!found || strlen(rule_path) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Rule path not found in index");
        return ESP_OK;
    }

    // Читаем правило по пути
    cJSON* rule = read_json_from_file(rule_path);
    if (!rule) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Rule file not found");
        return ESP_OK;
    }

    char* json_str = cJSON_PrintUnformatted(rule);
    cJSON_Delete(rule);

    if (!json_str) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    free(json_str);

    return ESP_OK;
}

// === Обработчик: POST /api/rule — создать или обновить правило ===
esp_err_t zbm_rest_api_post_rule_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/rule (POST)");

    if (req->content_len > 4096) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request too large");
        return ESP_OK;
    }

    char* body = calloc(1, req->content_len + 1);
    if (!body) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    int received = httpd_req_recv(req, body, req->content_len);
    if (received <= 0) {
        free(body);
        httpd_resp_send_500(req);
        return ESP_OK;
    }
    body[received] = '\0';

    cJSON* rule = cJSON_Parse(body);
    free(body);

    if (!rule) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    cJSON* rule_id_obj = cJSON_GetObjectItem(rule, "id");
    const char* rule_id = rule_id_obj ? rule_id_obj->valuestring : NULL;

    char id[9];
    if (!rule_id || strlen(rule_id) == 0) {
        generate_rule_id(id, sizeof(id));
        cJSON_AddStringToObject(rule, "id", id);
        rule_id = id;
        ESP_LOGI(TAG, "Generated new rule ID: %s", rule_id);
    } else {
        strlcpy(id, rule_id, sizeof(id));
    }

    // Валидация
    if (!cJSON_GetObjectItem(rule, "name")) {
        cJSON_Delete(rule);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Field 'name' is required");
        return ESP_OK;
    }

    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(rule, "enabled"));

    cJSON* exec_mode_obj = cJSON_GetObjectItem(rule, "exec_mode");
    if (!exec_mode_obj || !cJSON_IsNumber(exec_mode_obj)) {
        cJSON_AddNumberToObject(rule, "exec_mode", ZB_RULE_EXEC_FIRST);
    }

    // === Генерация пути ===
    char file_name[32];
    snprintf(file_name, sizeof(file_name), "rule_%s.json", rule_id);

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", SPIFFS_ZBM_CONF_MOUNT_POINT, file_name);

    // === Записываем полное правило ===
    if (!write_json_to_file(path, rule)) {
        cJSON_Delete(rule);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save rule file");
        return ESP_OK;
    }

    // === Обновляем индекс (добавляем путь) ===
    char index_path[64];
    snprintf(index_path, sizeof(index_path), "%s", ZBM_RULES_INDEX_FILE);

    cJSON* index = read_json_from_file(index_path);
    if (!index) index = cJSON_CreateArray();

    // Удаляем старую запись
    for (int i = 0; i < cJSON_GetArraySize(index); i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        cJSON* item_id = cJSON_GetObjectItem(item, "id");
        if (item_id && strcmp(item_id->valuestring, rule_id) == 0) {
            cJSON_DeleteItemFromArray(index, i);
            break;
        }
    }

    // Добавляем новую с полным путём
    cJSON* brief = cJSON_CreateObject();
    cJSON_AddStringToObject(brief, "id", rule_id);
    cJSON_AddStringToObject(brief, "name", cJSON_GetObjectItem(rule, "name")->valuestring);
    cJSON_AddBoolToObject(brief, "enabled", enabled);
    cJSON_AddStringToObject(brief, "path", path);  // ✅ Сохраняем путь
    cJSON_AddStringToObject(brief, "updated_at", "now");
    cJSON_AddItemToArray(index, brief);

    if (!write_json_to_file(index_path, index)) {
        cJSON_Delete(index);
        cJSON_Delete(rule);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to update rules index");
        return ESP_OK;
    }

    cJSON_Delete(index);
    cJSON_Delete(rule);

    // Уведомление
    cJSON* notify_data = cJSON_CreateObject();
    cJSON_AddStringToObject(notify_data, "id", rule_id);
    cJSON_AddStringToObject(notify_data, "action", "rule_updated");
    zbm_ws_send_sys_notify("rule_updated", "Rule saved", notify_data);
    cJSON_Delete(notify_data);
    char resp_str[128];
    snprintf(resp_str, sizeof(resp_str), "{\"success\": true, \"id\": \"%s\"}", rule_id);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

// === Обработчик: DELETE /api/rule/:id — удалить правило ===
esp_err_t zbm_rest_api_delete_rule_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/rule/{id} (DELETE)");

    char uri[128];
    strlcpy(uri, req->uri, sizeof(uri));
    char* id = uri + strlen("/api/rule/");

    if (strlen(id) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing rule ID");
        return ESP_OK;
    }

    char rule_path[128] = {0};
    if (!get_rule_path_by_id(id, rule_path, sizeof(rule_path))) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Rule not found or invalid path");
        return ESP_OK;
    }

    // Удаляем файл
    if (remove(rule_path) != 0) {
        ESP_LOGE(TAG, "Failed to remove rule file: %s", rule_path);
    }

    // Обновляем индекс
    char index_path[64];
    snprintf(index_path, sizeof(index_path), "%s", ZBM_RULES_INDEX_FILE);

    cJSON* index = read_json_from_file(index_path);
    if (index) {
        for (int i = 0; i < cJSON_GetArraySize(index); i++) {
            cJSON* item = cJSON_GetArrayItem(index, i);
            cJSON* item_id = cJSON_GetObjectItem(item, "id");
            if (item_id && strcmp(item_id->valuestring, id) == 0) {
                cJSON_DeleteItemFromArray(index, i);
                break;
            }
        }
        write_json_to_file(index_path, index);
        cJSON_Delete(index);
    }

    // Уведомление
    cJSON* notify_data = cJSON_CreateObject();
    cJSON_AddStringToObject(notify_data, "id", id);
    cJSON_AddStringToObject(notify_data, "action", "rule_deleted");
    zbm_ws_send_sys_notify("rule_deleted", "Rule deleted", notify_data);
    cJSON_Delete(notify_data);
    const char* response = "{\"success\": true}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

// === Обработчик: POST /api/rule/:id/enable — включить правило ===
esp_err_t zbm_rest_api_post_rule_enable_handler(httpd_req_t* req) {
    return set_rule_enabled_state(req, true);
}

// === Обработчик: POST /api/rule/:id/disable — выключить правило ===
esp_err_t zbm_rest_api_post_rule_disable_handler(httpd_req_t* req) {
    return set_rule_enabled_state(req, false);
}

// === Внутренняя функция: установка состояния enabled ===
static esp_err_t set_rule_enabled_state(httpd_req_t* req, bool enabled) {
    ESP_LOGI(TAG, "REQ /api/rule/{id}/%s", enabled ? "enable" : "disable");

    char uri[128];
    strlcpy(uri, req->uri, sizeof(uri));
    char* id_start = uri + strlen("/api/rule/");
    char* slash = strchr(id_start, '/');
    if (slash) *slash = '\0';

    char* id = id_start;
    if (strlen(id) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing rule ID");
        return ESP_OK;
    }

    char rule_path[128] = {0};
    if (!get_rule_path_by_id(id, rule_path, sizeof(rule_path))) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Rule not found or invalid path");
        return ESP_OK;
    }

    // Читаем правило
    cJSON* rule = read_json_from_file(rule_path);
    if (!rule) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Rule file not found");
        return ESP_OK;
    }

    cJSON* enabled_obj = cJSON_GetObjectItem(rule, "enabled");
    if (enabled_obj) {
        cJSON_Delete(enabled_obj);
    }
    cJSON_AddBoolToObject(rule, "enabled", enabled);

    if (!write_json_to_file(rule_path, rule)) {
        cJSON_Delete(rule);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save rule");
        return ESP_OK;
    }
    cJSON_Delete(rule);

    // Обновляем индекс
    char index_path[64];
    snprintf(index_path, sizeof(index_path), "%s", ZBM_RULES_INDEX_FILE);

    cJSON* index = read_json_from_file(index_path);
    if (index) {
        for (int i = 0; i < cJSON_GetArraySize(index); i++) {
            cJSON* item = cJSON_GetArrayItem(index, i);
            cJSON* item_id = cJSON_GetObjectItem(item, "id");
            if (item_id && strcmp(item_id->valuestring, id) == 0) {
                cJSON* en = cJSON_GetObjectItem(item, "enabled");
                if (en) cJSON_Delete(en);
                cJSON_AddBoolToObject(item, "enabled", enabled);
                break;
            }
        }
        write_json_to_file(index_path, index);
        cJSON_Delete(index);
    }

    // Уведомление
    cJSON* notify_data = cJSON_CreateObject();
    cJSON_AddStringToObject(notify_data, "id", id);
    cJSON_AddBoolToObject(notify_data, "enabled", enabled);
    zbm_ws_send_sys_notify("rule_toggled", enabled ? "Rule enabled" : "Rule disabled", notify_data);
    cJSON_Delete(notify_data);
    const char* response = "{\"success\": true}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

// POST /api/rule/{id}/run
// POST /api/rule/{id}/run
esp_err_t zbm_rest_api_post_rule_run_handler(httpd_req_t* req) {
    char uri[128];
    strlcpy(uri, req->uri, sizeof(uri));
    char* id = uri + strlen("/api/rule/");
    if (strncmp(id, "", 1) == 0) id++; // пропускаем '/'
    char* slash = strchr(id, '/');
    if (slash) *slash = '\0';

    if (strlen(id) == 0) {
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"missing id\"}");
        return ESP_OK;
    }

    char rule_path[128] = {0};
    if (!get_rule_path_by_id(id, rule_path, sizeof(rule_path))) {
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"rule not found\"}");
        return ESP_OK;
    }

    if (zb_automation_v2_run_rule_now(id)) {
        httpd_resp_sendstr(req, "{\"success\":true}");
    } else {
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"not found\"}");
    }
    return ESP_OK;
}