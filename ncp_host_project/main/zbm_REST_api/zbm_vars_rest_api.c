#include "zbm_rest_api.h"
#include "cJSON.h"
#include "inttypes.h"
#include "esp_log.h"
#include "zbm_attr_types.h"
#include "zbm_web_server.h" 
#include "zbm_automation_v2.h"

static const char* TAG = "ZBM_VARS_REST_API";
esp_err_t zbm_rest_api_get_vars_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/get/vars - Fetching all variables");
    cJSON* arr = cJSON_CreateArray();
    for (int i = 0; i < ZB_AUTO_VAR_COUNT; i++) {
        cJSON* v = cJSON_CreateObject();

        const char* name = zbm_vars[i].name;
        cJSON_AddStringToObject(v, "name", name);
        cJSON_AddNumberToObject(v, "idx", i);
        cJSON_AddNumberToObject(v, "type", zbm_vars[i].data_type);

        // Текущее значение
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
        
        // Начальное значение
        char init_value_str[64] = {0};
        switch (zbm_vars[i].data_type) {
            case ZBM_ATTR_TYPE_U8: {
                uint8_t val = *(uint8_t*)zbm_vars[i].p_init_value;
                cJSON_AddNumberToObject(v, "init_value", val);
                snprintf(init_value_str, sizeof(init_value_str), "U8=%u", val);
                break;
            }
            case ZBM_ATTR_TYPE_S8: {
                int8_t val = *(int8_t*)zbm_vars[i].p_init_value;
                cJSON_AddNumberToObject(v, "init_value", val);
                snprintf(init_value_str, sizeof(init_value_str), "S8=%d", val);
                break;
            }
            case ZBM_ATTR_TYPE_BOOL: {
                bool val = *(bool*)zbm_vars[i].p_init_value;
                cJSON_AddBoolToObject(v, "init_value", val);
                snprintf(init_value_str, sizeof(init_value_str), "BOOL=%s", val ? "true" : "false");
                break;
            }
            case ZBM_ATTR_TYPE_U16: {
                uint16_t val = *(uint16_t*)zbm_vars[i].p_init_value;
                cJSON_AddNumberToObject(v, "init_value", val);
                snprintf(init_value_str, sizeof(init_value_str), "U16=%u", val);
                break;
            }
            case ZBM_ATTR_TYPE_S16: {
                int16_t val = *(int16_t*)zbm_vars[i].p_init_value;
                cJSON_AddNumberToObject(v, "init_value", val);
                snprintf(init_value_str, sizeof(init_value_str), "S16=%d", val);
                break;
            }
            case ZBM_ATTR_TYPE_CHAR_STRING:
            case ZBM_ATTR_TYPE_LONG_CHAR_STRING: {
                char* str = (char*)zbm_vars[i].p_init_value;
                if (!str) {
                    cJSON_AddStringToObject(v, "init_value", "");
                    snprintf(init_value_str, sizeof(init_value_str), "STR=(null)");
                } else {
                    cJSON_AddStringToObject(v, "init_value", str);
                    if (zbm_vars[i].data_type == ZBM_ATTR_TYPE_CHAR_STRING) {
                        snprintf(init_value_str, sizeof(init_value_str), "STR=\"%s\"", str);
                    } else {
                        snprintf(init_value_str, sizeof(init_value_str), "LSTR(len=%d)", strlen(str));
                    }
                }
                break;
            }
            default: {
                uint8_t val = *(uint8_t*)zbm_vars[i].p_init_value;
                cJSON_AddNumberToObject(v, "init_value", val);
                snprintf(init_value_str, sizeof(init_value_str), "UNK=%u", val);
                break;
            }
        }
        cJSON_AddItemToArray(arr, v);
    }

    char* str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, str, HTTPD_RESP_USE_STRLEN);
    free(str);
    return ESP_OK;
}


esp_err_t zbm_rest_api_post_var_handler(httpd_req_t* req) {
    uint32_t temp_value = 0;  // Будет хранить числовые значения
    void* free_on_exit = NULL; // Для строк
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

    ESP_LOGI(TAG, "Received raw JSON for var/%d: %s", idx, body);

    cJSON* json = cJSON_Parse(body);
    if (!json) {
        const char* resp = "{\"success\":false,\"error\":\"invalid_json\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }

    const char* name = NULL;
    zbm_attr_data_types_t type = ZBM_ATTR_TYPE_TNULL;
    void* runtime_value = NULL;
    uint16_t runtime_size = 0;
    void* init_value = NULL;
    uint16_t init_size = 0;

    // === Имя ===
    cJSON* name_obj = cJSON_GetObjectItem(json, "name");
    if (name_obj && cJSON_IsString(name_obj)) {
        name = name_obj->valuestring;
        if (strlen(name) == 0 || strlen(name) >= sizeof(zbm_vars[idx].name)) {
            cJSON_Delete(json);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid variable name");
            return ESP_OK;
        }
    }

    // === Тип ===
    cJSON* type_obj = cJSON_GetObjectItem(json, "type");
    if (type_obj && cJSON_IsNumber(type_obj)) {
        type = (zbm_attr_data_types_t)type_obj->valueint;
    }

    // === init_value ===
    cJSON* init_value_obj = cJSON_GetObjectItem(json, "init_value");
    if (init_value_obj) {
        if (cJSON_IsNumber(init_value_obj)) {
            int num = init_value_obj->valueint;

            // Определяем, сколько байт нужно
            if (type == ZBM_ATTR_TYPE_U8 || type == ZBM_ATTR_TYPE_S8 || type == ZBM_ATTR_TYPE_BOOL) {
                *(uint8_t*)&temp_value = (uint8_t)num;
                init_value = &temp_value;
                init_size = 1;
            } else if (type == ZBM_ATTR_TYPE_U16 || type == ZBM_ATTR_TYPE_S16) {
                *(uint16_t*)&temp_value = (uint16_t)num;
                init_value = &temp_value;
                init_size = 2;
            } else {
                // По умолчанию — 1 байт
                *(uint8_t*)&temp_value = (uint8_t)num;
                init_value = &temp_value;
                init_size = 1;
            }
        } else if (cJSON_IsString(init_value_obj)) {
            size_t str_len = strlen(init_value_obj->valuestring) + 1;
            char* str_copy = malloc(str_len);
            if (!str_copy) {
                cJSON_Delete(json);
                httpd_resp_send_500(req);
                return ESP_OK;
            }
            strcpy(str_copy, init_value_obj->valuestring);
            init_value = str_copy;
            init_size = str_len;
            free_on_exit = str_copy;
        }
    }

    // === value (runtime) ===
    cJSON* value_obj = cJSON_GetObjectItem(json, "value");
    if (value_obj) {
        if (cJSON_IsNumber(value_obj)) {
            int num = value_obj->valueint;

            if (type == ZBM_ATTR_TYPE_U8 || type == ZBM_ATTR_TYPE_S8 || type == ZBM_ATTR_TYPE_BOOL) {
                *(uint8_t*)&temp_value = (uint8_t)num;
                runtime_value = &temp_value;
                runtime_size = 1;
            } else if (type == ZBM_ATTR_TYPE_U16 || type == ZBM_ATTR_TYPE_S16) {
                *(uint16_t*)&temp_value = (uint16_t)num;
                runtime_value = &temp_value;
                runtime_size = 2;
            } else {
                *(uint8_t*)&temp_value = (uint8_t)num;
                runtime_value = &temp_value;
                runtime_size = 1;
            }
        } else if (cJSON_IsString(value_obj)) {
            size_t str_len = strlen(value_obj->valuestring) + 1;
            char* str_copy = malloc(str_len);
            if (!str_copy) {
                cJSON_Delete(json);
                httpd_resp_send_500(req);
                return ESP_OK;
            }
            strcpy(str_copy, value_obj->valuestring);
            runtime_value = str_copy;
            runtime_size = str_len;
            free_on_exit = str_copy;
        }
    }

    // === ЕДИНСТВЕННЫЙ вызов обновления ===
    bool updated = zbm_var_set_config(idx, name, type, runtime_value, runtime_size, init_value, init_size);

    cJSON_Delete(json);

    if (updated) {
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

