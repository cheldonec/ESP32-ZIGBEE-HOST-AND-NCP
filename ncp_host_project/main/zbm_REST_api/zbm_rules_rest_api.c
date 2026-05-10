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








// === Обработчик: GET /api/rules — получить список правил (индекс) ===
esp_err_t zbm_rest_api_get_rules_handler(httpd_req_t* req) {
    cJSON* arr = cJSON_CreateArray();
    for (int i = 0; i < zb_rules_count; i++) {
        cJSON* brief = cJSON_CreateObject();
        cJSON_AddStringToObject(brief, "id", zb_rules[i]->id);
        cJSON_AddStringToObject(brief, "name", zb_rules[i]->name);
        cJSON_AddBoolToObject(brief, "enabled", zb_rules[i]->enabled);
        cJSON_AddNumberToObject(brief, "priority", zb_rules[i]->priority);
        cJSON_AddStringToObject(brief, "updated_at", "now");
        cJSON_AddItemToArray(arr, brief);
    }

    char* str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, str, HTTPD_RESP_USE_STRLEN);
    free(str);
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

    // Поиск в памяти
    zb_rule_t* rule = NULL;
    for (int i = 0; i < zb_rules_count; i++) {
        if (strcmp(zb_rules[i]->id, id) == 0) {
            rule = zb_rules[i];
            break;
        }
    }

    if (!rule) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Rule not found in memory");
        return ESP_OK;
    }

    // Сериализуем в JSON
    cJSON* json = rule_to_json(rule);
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    char* json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

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

    cJSON* rule_json = cJSON_Parse(body);
    free(body);

    if (!rule_json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    // Генерация ID, если нет
    cJSON* rule_id_obj = cJSON_GetObjectItem(rule_json, "id");
    const char* rule_id = rule_id_obj ? rule_id_obj->valuestring : NULL;

    char id[9];
    if (!rule_id || strlen(rule_id) == 0) {
        generate_rule_id(id, sizeof(id));
        cJSON_AddStringToObject(rule_json, "id", id);
        rule_id = id;
        ESP_LOGI(TAG, "Generated new rule ID: %s", rule_id);
    }

    // Валидация имени
    if (!cJSON_GetObjectItem(rule_json, "name")) {
        cJSON_Delete(rule_json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Field 'name' is required");
        return ESP_OK;
    }

    // === КЛЮЧЕВОЙ МОМЕНТ: обновляем in-memory движок ===
    if (!zb_automation_v2_update_rule_from_json(rule_json)) {
        cJSON_Delete(rule_json);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to update rule in engine");
        return ESP_OK;
    }

    // === Теперь сохраняем на диск ===
    if (!zb_automation_v2_save_rule_to_storage(rule_id)) {
        ESP_LOGE(TAG, "Failed to save rule %s to SPIFFS, but it's active in memory", rule_id);
    }

    cJSON_Delete(rule_json);

    // Уведомление WebSocket
    cJSON* notify_data = cJSON_CreateObject();
    cJSON_AddStringToObject(notify_data, "id", rule_id);
    cJSON_AddStringToObject(notify_data, "action", "rule_updated");
    zbm_ws_send_sys_notify("rule_updated", "Rule saved", notify_data);
    cJSON_Delete(notify_data);

    // ✅ ПРАВИЛЬНЫЙ СПОСОБ: используем cJSON для формирования ответа
    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "id", rule_id);
    char* response_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);

    if (!response_str) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response_str, strlen(response_str)); // ✅ Точная длина
    free(response_str); // ✅ Не забываем освободить

    return ESP_OK;
}
esp_err_t zbm_rest_api_post_rule_handler_old(httpd_req_t* req) {
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

    cJSON* rule_json = cJSON_Parse(body);
    free(body);

    if (!rule_json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    // Генерация ID, если нет
    cJSON* rule_id_obj = cJSON_GetObjectItem(rule_json, "id");
    const char* rule_id = rule_id_obj ? rule_id_obj->valuestring : NULL;

    char id[9];
    if (!rule_id || strlen(rule_id) == 0) {
        generate_rule_id(id, sizeof(id));
        cJSON_AddStringToObject(rule_json, "id", id);
        rule_id = id;
        ESP_LOGI(TAG, "Generated new rule ID: %s", rule_id);
    }

    // Валидация имени
    if (!cJSON_GetObjectItem(rule_json, "name")) {
        cJSON_Delete(rule_json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Field 'name' is required");
        return ESP_OK;
    }

    // === КЛЮЧЕВОЙ МОМЕНТ: обновляем in-memory движок ===
    if (!zb_automation_v2_update_rule_from_json(rule_json)) {
        cJSON_Delete(rule_json);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to update rule in engine");
        return ESP_OK;
    }

    // === Теперь сохраняем на диск ===
    if (!zb_automation_v2_save_rule_to_storage(rule_id)) {
        // Но даже если не сохранилось — правило уже работает!
        ESP_LOGE(TAG, "Failed to save rule %s to SPIFFS, but it's active in memory", rule_id);
    }

    cJSON_Delete(rule_json);

    // Уведомление WebSocket
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
    char* id_start = uri + strlen("/api/rule/");

    // Найти конец ID: до '/', '?' или конца строки
    char* id_end = strpbrk(id_start, "/?");
    if (id_end) {
        *id_end = '\0'; // Обрезаем строку
    }

    if (strlen(id_start) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing rule ID");
        return ESP_OK;
    }

    // Копируем в буфер для безопасности
    char rule_id[37]; // UUID-like, например: 8 символов + \0
    if (strlen(id_start) >= sizeof(rule_id)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Rule ID too long");
        return ESP_OK;
    }
    strcpy(rule_id, id_start);

    // Удаляем из памяти
    bool removed = zb_automation_v2_remove_rule(rule_id);
    if (!removed) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Rule not found in engine");
        return ESP_OK;
    }

    // Удаляем файл
    char file_name[64];
    snprintf(file_name, sizeof(file_name), "rule_%s.json", rule_id);
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", SPIFFS_ZBM_CONF_MOUNT_POINT, file_name);
    remove(path);

    // Обновляем индекс
    char index_path[64];
    snprintf(index_path, sizeof(index_path), "%s", ZBM_RULES_INDEX_FILE);
    cJSON* index = read_json_from_file(index_path);
    if (index) {
        for (int i = 0; i < cJSON_GetArraySize(index); i++) {
            cJSON* item = cJSON_GetArrayItem(index, i);
            cJSON* item_id = cJSON_GetObjectItem(item, "id");
            if (item_id && strcmp(item_id->valuestring, rule_id) == 0) {
                cJSON_DeleteItemFromArray(index, i);
                break;
            }
        }
        write_json_to_file(index_path, index);
        cJSON_Delete(index);
    }

    // Уведомление
    cJSON* notify_data = cJSON_CreateObject();
    cJSON_AddStringToObject(notify_data, "id", rule_id);
    zbm_ws_send_sys_notify("rule_deleted", "Rule deleted", notify_data);
    cJSON_Delete(notify_data);

    /*const char* response = "{\"success\": true}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);*/
    cJSON* del_response = cJSON_CreateObject();
    cJSON_AddBoolToObject(del_response, "success", true);
    char* del_str = cJSON_PrintUnformatted(del_response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, del_str, strlen(del_str));
    free(del_str);
    cJSON_Delete(del_response);

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