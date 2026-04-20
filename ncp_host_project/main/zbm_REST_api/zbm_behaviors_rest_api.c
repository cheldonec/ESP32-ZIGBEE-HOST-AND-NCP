#include "cJSON.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "string.h"
#include "zbm_spiffs_helper.h"
#include "zbm_rest_api.h"
#include "zbm_behavior.h"
#include "zbm_web_server.h" // для ws уведомлений
#include "esp_random.h"

static const char* TAG = "ZBM_BEHAVIOR_API";

// Вспомогательная: установка состояния enabled
static esp_err_t set_behavior_enabled_state(httpd_req_t* req, bool enabled);

// === GET /api/behaviors — список всех поведений ===
esp_err_t zbm_rest_api_get_behaviors_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/behaviors");

    cJSON* index = read_json_from_file(BEHAVIORS_INDEX_FILE);
    if (!index) {
        ESP_LOGW(TAG, "No behaviors index found");
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

// === GET /api/behavior/{id} — получить полное поведение ===
esp_err_t zbm_rest_api_get_behavior_by_id_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/behavior/{id}");

    char uri[128];
    strlcpy(uri, req->uri, sizeof(uri));
    char* id = uri + strlen("/api/behavior/");

    if (strlen(id) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing behavior ID");
        return ESP_OK;
    }

    // Найти путь из индекса
    cJSON* index = read_json_from_file(BEHAVIORS_INDEX_FILE);
    if (!index) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Behaviors index not found");
        return ESP_OK;
    }

    const char* path = NULL;
    for (int i = 0; i < cJSON_GetArraySize(index); i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        cJSON* item_id = cJSON_GetObjectItem(item, "id");
        cJSON* item_path = cJSON_GetObjectItem(item, "path");
        if (item_id && item_path && strcmp(item_id->valuestring, id) == 0) {
            path = item_path->valuestring;
            break;
        }
    }
    cJSON_Delete(index);

    if (!path) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Behavior not found in index");
        return ESP_OK;
    }

    cJSON* json = read_json_from_file(path);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Behavior file not found");
        return ESP_OK;
    }

    char* str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (!str) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, str, HTTPD_RESP_USE_STRLEN);
    free(str);

    return ESP_OK;
}

// === POST /api/behavior — создать/обновить поведение ===
esp_err_t zbm_rest_api_post_behavior_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/behavior (POST)");

    if (req->content_len > 2048) {
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

    cJSON* json = cJSON_Parse(body);
    free(body);

    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    cJSON* id_obj = cJSON_GetObjectItem(json, "id");
    const char* id = id_obj ? id_obj->valuestring : NULL;

    char temp_id[37];
    if (!id || strlen(id) == 0) {
        uint32_t rand = esp_random();
        snprintf(temp_id, sizeof(temp_id), "%08" PRIx32 "-bhv", rand);
        cJSON_AddStringToObject(json, "id", temp_id);
        id = temp_id;
        ESP_LOGI(TAG, "Generated new behavior ID: %s", id);
    }

    if (!cJSON_GetObjectItem(json, "name")) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Field 'name' is required");
        return ESP_OK;
    }

    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(json, "enabled"));

    // Генерация пути
    char path[ZBM_BEHAVIOR_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s.json", SPIFFS_ZBM_CONF_MOUNT_POINT, id);

    // Сохраняем полный JSON
    if (!write_json_to_file(path, json)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save behavior file");
        return ESP_OK;
    }

    // Обновляем индекс
    cJSON* index = read_json_from_file(BEHAVIORS_INDEX_FILE);
    if (!index) index = cJSON_CreateArray();

    // Удаляем старую запись
    for (int i = 0; i < cJSON_GetArraySize(index); i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        cJSON* item_id = cJSON_GetObjectItem(item, "id");
        if (item_id && strcmp(item_id->valuestring, id) == 0) {
            cJSON_DeleteItemFromArray(index, i);
            break;
        }
    }

    // Добавляем новую
    cJSON* entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "id", id);
    cJSON_AddStringToObject(entry, "path", path);
    cJSON_AddBoolToObject(entry, "enabled", enabled);
    cJSON_AddStringToObject(entry, "name", cJSON_GetObjectItem(json, "name")->valuestring);
    cJSON_AddItemToArray(index, entry);

    write_json_to_file(BEHAVIORS_INDEX_FILE, index);
    cJSON_Delete(index);
    cJSON_Delete(json);

    // Уведомление
    cJSON* notify = cJSON_CreateObject();
    cJSON_AddStringToObject(notify, "id", id);
    cJSON_AddStringToObject(notify, "action", "behavior_saved");
    zbm_ws_send_sys_notify("behavior_saved", "Behavior saved", notify);

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"success\":true,\"id\":\"%s\"}", id);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

// === DELETE /api/behavior/{id} ===
esp_err_t zbm_rest_api_delete_behavior_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/behavior/{id} (DELETE)");

    char uri[128];
    strlcpy(uri, req->uri, sizeof(uri));
    char* id = uri + strlen("/api/behavior/");

    if (strlen(id) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing behavior ID");
        return ESP_OK;
    }

    // Найти в индексе
    cJSON* index = read_json_from_file(BEHAVIORS_INDEX_FILE);
    if (!index) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Index not found");
        return ESP_OK;
    }

    const char* path = NULL;
    int idx = -1;
    for (int i = 0; i < cJSON_GetArraySize(index); i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        cJSON* item_id = cJSON_GetObjectItem(item, "id");
        cJSON* item_path = cJSON_GetObjectItem(item, "path");
        if (item_id && strcmp(item_id->valuestring, id) == 0) {
            path = item_path->valuestring;
            idx = i;
            break;
        }
    }

    if (!path) {
        cJSON_Delete(index);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Behavior not found");
        return ESP_OK;
    }

    // Удалить файл
    unlink(path);

    // Удалить из индекса
    cJSON_DeleteItemFromArray(index, idx);
    write_json_to_file(BEHAVIORS_INDEX_FILE, index);
    cJSON_Delete(index);

    // Уведомление
    cJSON* notify = cJSON_CreateObject();
    cJSON_AddStringToObject(notify, "id", id);
    cJSON_AddStringToObject(notify, "action", "behavior_deleted");
    zbm_ws_send_sys_notify("behavior_deleted", "Behavior deleted", notify);

    httpd_resp_sendstr(req, "{\"success\":true}");
    return ESP_OK;
}

// === POST /api/behavior/{id}/enable ===
esp_err_t zbm_rest_api_post_behavior_enable_handler(httpd_req_t* req) {
    return set_behavior_enabled_state(req, true);
}

// === POST /api/behavior/{id}/disable ===
esp_err_t zbm_rest_api_post_behavior_disable_handler(httpd_req_t* req) {
    return set_behavior_enabled_state(req, false);
}

static esp_err_t set_behavior_enabled_state(httpd_req_t* req, bool enabled) {
    ESP_LOGI(TAG, "REQ /api/behavior/{id}/%s", enabled ? "enable" : "disable");

    char uri[128];
    strlcpy(uri, req->uri, sizeof(uri));
    char* id = uri + strlen(enabled ? "/api/behavior/" : "/api/behavior/") + (enabled ? 0 : 7);
    char* slash = strchr(id, '/');
    if (slash) *slash = '\0';

    if (strlen(id) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing behavior ID");
        return ESP_OK;
    }

    // Найти путь
    cJSON* index = read_json_from_file(BEHAVIORS_INDEX_FILE);
    if (!index) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Index not found");
        return ESP_OK;
    }

    const char* path = NULL;
    for (int i = 0; i < cJSON_GetArraySize(index); i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        cJSON* item_id = cJSON_GetObjectItem(item, "id");
        cJSON* item_path = cJSON_GetObjectItem(item, "path");
        if (item_id && item_path && strcmp(item_id->valuestring, id) == 0) {
            path = item_path->valuestring;
            break;
        }
    }
    cJSON_Delete(index);

    if (!path) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Behavior path not found");
        return ESP_OK;
    }

    // Обновить файл
    cJSON* json = read_json_from_file(path);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Behavior file not found");
        return ESP_OK;
    }

    cJSON* en_obj = cJSON_GetObjectItem(json, "enabled");
    if (en_obj) cJSON_Delete(en_obj);
    cJSON_AddBoolToObject(json, "enabled", enabled);
    write_json_to_file(path, json);
    cJSON_Delete(json);

    // Обновить индекс
    cJSON* idx = read_json_from_file(BEHAVIORS_INDEX_FILE);
    if (idx) {
        for (int i = 0; i < cJSON_GetArraySize(idx); i++) {
            cJSON* item = cJSON_GetArrayItem(idx, i);
            cJSON* item_id = cJSON_GetObjectItem(item, "id");
            if (item_id && strcmp(item_id->valuestring, id) == 0) {
                cJSON* e = cJSON_GetObjectItem(item, "enabled");
                if (e) cJSON_Delete(e);
                cJSON_AddBoolToObject(item, "enabled", enabled);
                break;
            }
        }
        write_json_to_file(BEHAVIORS_INDEX_FILE, idx);
        cJSON_Delete(idx);
    }

    // Уведомление
    cJSON* notify = cJSON_CreateObject();
    cJSON_AddStringToObject(notify, "id", id);
    cJSON_AddBoolToObject(notify, "enabled", enabled);
    zbm_ws_send_sys_notify("behavior_toggled", enabled ? "Behavior enabled" : "Behavior disabled", notify);

    httpd_resp_sendstr(req, "{\"success\":true}");
    return ESP_OK;
}

// === POST /api/behavior/{id}/run — ручной запуск ===
esp_err_t zbm_rest_api_post_behavior_run_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/behavior/{id}/run");

    char uri[128];
    strlcpy(uri, req->uri, sizeof(uri));
    char* id = uri + strlen("/api/behavior/") + 1; // skip "*"
    char* slash = strchr(id, '/');
    if (slash) *slash = '\0';

    if (zbm_behavior_execute(id)) {
        httpd_resp_sendstr(req, "{\"success\":true}");
    } else {
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"not found or disabled\"}");
    }
    return ESP_OK;
}