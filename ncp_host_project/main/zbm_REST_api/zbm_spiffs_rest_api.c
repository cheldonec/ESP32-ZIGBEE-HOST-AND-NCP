// spiffs_api.c
#include "zbm_spiffs_rest_api.h"
#include "zbm_spiffs_helper.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "SPIFFS_API";

#define MAX_FILE_READ_SIZE (8 * 1024)
#define MAX_FILE_WRITE_SIZE (32 * 1024)

static void url_decode(char* dst, const char* src, size_t size) {
    while (*src && size > 1) {
        char a = *src++;
        if (a == '%' && src[0] && src[1]) {
            char hex[3] = { src[0], src[1], '\0' };
            char ch = (char)strtol(hex, NULL, 16);
            *dst++ = ch;
            src += 2;
            size--;
        } else if (a == '+') {
            *dst++ = ' ';
            size--;
        } else {
            *dst++ = a;
            size--;
        }
    }
    *dst = '\0';
}

// === Общий обработчик: GET /api/spiffs/<partition>/ls?path=/ ===
esp_err_t spiffs_api_ls_handler(httpd_req_t* req) {
    // Получаем root из user_ctx
    const char* root = (const char*)req->user_ctx;
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    char query[64];
    size_t len = httpd_req_get_url_query_len(req) + 1;
    if (len > sizeof(query)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Query too long");
        return ESP_OK;
    }

    httpd_req_get_url_query_str(req, query, len);
    char* path_param = strstr(query, "path=");
    const char* path = "/";

    if (path_param) {
        path = path_param + 5;
        if (strstr(path, "..")) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
            return ESP_OK;
        }
    }

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s%s", root, path);
    size_t flen = strlen(full_path);
    if (flen > 0 && full_path[flen - 1] == '/') {
        full_path[flen - 1] = '\0';
    }

    cJSON* list = spiffs_list_directory(full_path);
    if (!list) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot read directory");
        return ESP_OK;
    }

    char* json_str = cJSON_PrintUnformatted(list);
    cJSON_Delete(list);

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

// === Обработчик: GET /api/spiffs/<partition>/get/<filename> ===
esp_err_t spiffs_api_get_file_handler(httpd_req_t* req) {
    const char* root = (const char*)req->user_ctx;
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    const char* filename_start = strstr(req->uri, "/get/");
    if (!filename_start) {
        httpd_resp_send_404(req);
        return ESP_OK;
    }
    filename_start += 5;

    char filename[64];
    url_decode(filename, filename_start, sizeof(filename));

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", root, filename);

    if (!spiffs_file_exists(path)) {
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    FILE* f = fopen(path, "r");
    if (!f) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size > MAX_FILE_READ_SIZE) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File too large");
        return ESP_OK;
    }

    char* data = malloc(size + 1);
    if (!data) {
        fclose(f);
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    fread(data, 1, size, f);
    data[size] = '\0';
    fclose(f);

    // Устанавливаем тип и заголовки
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // ✅ Формируем Content-Disposition без лишних аргументов
    char content_disposition[128];
    snprintf(content_disposition, sizeof(content_disposition),
             "attachment; filename=\"%s\"", filename);
    httpd_resp_set_hdr(req, "Content-Disposition", content_disposition);

    httpd_resp_send(req, data, size);
    free(data);
    return ESP_OK;
}

// === Обработчик: POST /api/spiffs/<partition>/save/<filename> ===
esp_err_t spiffs_api_save_file_handler(httpd_req_t* req) {
    // Получаем root из user_ctx
    const char* root = (const char*)req->user_ctx;
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    // Извлекаем имя файла из пути после "save/"
    const char* filename_start = strstr(req->uri, "/save/");
    if (!filename_start) {
        httpd_resp_send_404(req);
        return ESP_OK;
    }
    filename_start += 6; // пропускаем "/save/"

    char filename[64];
    url_decode(filename, filename_start, sizeof(filename));

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", root, filename);

    if (req->content_len > MAX_FILE_WRITE_SIZE) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too large");
        return ESP_OK;
    }

    char* data = malloc(req->content_len + 1);
    if (!data) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    int received = httpd_req_recv(req, data, req->content_len);
    if (received <= 0) {
        free(data);
        httpd_resp_send_500(req);
        return ESP_OK;
    }
    data[received] = '\0';

    FILE* f = fopen(path, "w");
    if (!f) {
        free(data);
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    fwrite(data, 1, received, f);
    fclose(f);
    free(data);

    ESP_LOGI(TAG, "Saved: %s", path);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "{}");
    return ESP_OK;
}

// === Обработчик: POST /api/spiffs/<partition>/delete/<filename> ===
esp_err_t spiffs_api_delete_file_handler(httpd_req_t* req) {
    // Получаем root из user_ctx
    const char* root = (const char*)req->user_ctx;
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    // Извлекаем имя файла из пути после "delete/"
    const char* filename_start = strstr(req->uri, "/delete/");
    if (!filename_start) {
        httpd_resp_send_404(req);
        return ESP_OK;
    }
    filename_start += 8; // пропускаем "/delete/"

    char filename[64];
    url_decode(filename, filename_start, sizeof(filename));

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", root, filename);

    if (remove(path) == 0) {
        httpd_resp_sendstr(req, "{}");
        ESP_LOGI(TAG, "Deleted: %s", path);
    } else {
        ESP_LOGE(TAG, "Failed to delete: %s", path);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Delete failed");
    }
    return ESP_OK;
}

// === Обработчик: GET /api/backup ===
esp_err_t spiffs_api_backup_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/backup");

    cJSON* backup = cJSON_CreateObject();
    if (!backup) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    // === 1. Конфигурация ===
cJSON* config_obj = cJSON_CreateObject();
cJSON_AddItemToObject(backup, "config", config_obj);

const char* config_files[] = {
    "zbm_config.json",
    "zbm_devices_index.json"
};

for (int i = 0; i < 2; i++) {
    const char* filename = config_files[i];
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", SPIFFS_ZBM_CONF_MOUNT_POINT, filename);

    if (!spiffs_file_exists(path)) {
        continue;
    }

    FILE* f = fopen(path, "r");
    if (!f) continue;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size > MAX_FILE_READ_SIZE) {
        fclose(f);
        continue;
    }

    char* data = malloc(size + 1);
    if (!data) {
        fclose(f);
        continue;
    }

    fread(data, 1, size, f);
    data[size] = '\0';
    fclose(f);

    cJSON_AddStringToObject(config_obj, filename, data);
    free(data);
}

    // === 2. Квирки ===
    cJSON* quirks_obj = cJSON_CreateObject();
    cJSON_AddItemToObject(backup, "quirks", quirks_obj);

    // Сначала индекс
    const char* quirks_index_path = ZBM_QUIRKS_INDEX_JSON;
    if (spiffs_file_exists(quirks_index_path)) {
        FILE* f = fopen(quirks_index_path, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (size <= MAX_FILE_READ_SIZE) {
                char* data = malloc(size + 1);
                if (data) {
                    fread(data, 1, size, f);
                    data[size] = '\0';
                    cJSON_AddStringToObject(quirks_obj, "zbm_quirks_index.json", data);
                    free(data);
                }
            }
            fclose(f);
        }
    }

    // Затем все .json в папке quirks
    cJSON* quirks_list = spiffs_list_directory(SPIFFS_ZBM_QUIRKS_MOUNT_POINT);
    if (quirks_list) {
        int count = cJSON_GetArraySize(quirks_list);
        for (int i = 0; i < count; i++) {
            cJSON* file = cJSON_GetArrayItem(quirks_list, i);
            const char* name = cJSON_GetStringValue(cJSON_GetObjectItem(file, "name"));
            if (!name || cJSON_GetObjectItem(file, "is_dir")->valueint != 0) {
                continue;
            }
            if (strstr(name, ".json") == NULL || strcmp(name, "zbm_quirks_index.json") == 0) {
                continue;
            }

            char path[256];
            snprintf(path, sizeof(path), "%s/%s", SPIFFS_ZBM_QUIRKS_MOUNT_POINT, name);

            FILE* f = fopen(path, "r");
            if (!f) continue;

            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (size > MAX_FILE_READ_SIZE) {
                fclose(f);
                continue;
            }

            char* data = malloc(size + 1);
            if (data) {
                fread(data, 1, size, f);
                data[size] = '\0';
                cJSON_AddStringToObject(quirks_obj, name, data);
                free(data);
            }
            fclose(f);
        }
        cJSON_Delete(quirks_list);
    }

    // === 3. Сертификаты (опционально) ===
    cJSON* certs_obj = cJSON_CreateObject();
    cJSON_AddItemToObject(backup, "certs", certs_obj);

    cJSON* certs_list = spiffs_list_directory(SPIFFS_ZBM_CERTS_MOUNT_POINT);
    if (certs_list) {
        int count = cJSON_GetArraySize(certs_list);
        for (int i = 0; i < count; i++) {
            cJSON* file = cJSON_GetArrayItem(certs_list, i);
            const char* name = cJSON_GetStringValue(cJSON_GetObjectItem(file, "name"));
            if (!name || cJSON_GetObjectItem(file, "is_dir")->valueint != 0) {
                continue;
            }

            char path[256];
            snprintf(path, sizeof(path), "%s/%s", SPIFFS_ZBM_CERTS_MOUNT_POINT, name);

            FILE* f = fopen(path, "r");
            if (!f) continue;

            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (size > MAX_FILE_READ_SIZE * 4) { // сертификаты могут быть больше
                fclose(f);
                continue;
            }

            char* data = malloc(size + 1);
            if (data) {
                fread(data, 1, size, f);
                data[size] = '\0';
                cJSON_AddStringToObject(certs_obj, name, data);
                free(data);
            }
            fclose(f);
        }
        cJSON_Delete(certs_list);
    }

    // === Отправка результата ===
    char* json_str = cJSON_PrintUnformatted(backup);
    cJSON_Delete(backup);

    if (!json_str) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"zbm_backup.json\"");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    free(json_str);

    ESP_LOGI(TAG, "Backup sent successfully");
    return ESP_OK;
}

// === Обработчик: POST /api/restore ===
esp_err_t spiffs_api_restore_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/restore");

    if (req->content_len > MAX_FILE_WRITE_SIZE * 4) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Backup too large");
        return ESP_OK;
    }

    char* json_str = malloc(req->content_len + 1);
    if (!json_str) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    int received = httpd_req_recv(req, json_str, req->content_len);
    if (received <= 0) {
        free(json_str);
        httpd_resp_send_500(req);
        return ESP_OK;
    }
    json_str[received] = '\0';

    cJSON* backup = cJSON_Parse(json_str);
    free(json_str);

    if (!backup) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    // === 1. Восстановление config файлов ===
    cJSON* config_obj = cJSON_GetObjectItem(backup, "config");
    if (config_obj) {
        cJSON* item = NULL;
        cJSON_ArrayForEach(item, config_obj) {
            const char* filename = item->string;
            const char* content = cJSON_GetStringValue(item);
            if (!filename || !content) continue;

            // Запрет на опасные имена
            if (strcmp(filename, "zbm_config.json") == 0 ||
                strcmp(filename, "zbm_devices_index.json") == 0 ||
                strncmp(filename, "dev_", 4) == 0) {
                // Можно добавить валидацию позже
            }

            char path[256];
            snprintf(path, sizeof(path), "%s/%s", SPIFFS_ZBM_CONF_MOUNT_POINT, filename);

            FILE* f = fopen(path, "w");
            if (f) {
                fwrite(content, 1, strlen(content), f);
                fclose(f);
                ESP_LOGI(TAG, "Restored: %s", path);
            } else {
                ESP_LOGE(TAG, "Failed to write: %s", path);
            }
        }
    }

    // === 2. Восстановление quirks ===
    cJSON* quirks_obj = cJSON_GetObjectItem(backup, "quirks");
    if (quirks_obj) {
        cJSON* item = NULL;
        cJSON_ArrayForEach(item, quirks_obj) {
            const char* filename = item->string;
            const char* content = cJSON_GetStringValue(item);
            if (!filename || !content) continue;

            char path[256];
            snprintf(path, sizeof(path), "%s/%s", SPIFFS_ZBM_QUIRKS_MOUNT_POINT, filename);

            FILE* f = fopen(path, "w");
            if (f) {
                fwrite(content, 1, strlen(content), f);
                fclose(f);
                ESP_LOGI(TAG, "Restored: %s", path);
            } else {
                ESP_LOGE(TAG, "Failed to write: %s", path);
            }
        }
    }

    // === 3. Восстановление сертификатов ===
    cJSON* certs_obj = cJSON_GetObjectItem(backup, "certs");
    if (certs_obj) {
        cJSON* item = NULL;
        cJSON_ArrayForEach(item, certs_obj) {
            const char* filename = item->string;
            const char* content = cJSON_GetStringValue(item);
            if (!filename || !content) continue;

            char path[256];
            snprintf(path, sizeof(path), "%s/%s", SPIFFS_ZBM_CERTS_MOUNT_POINT, filename);

            FILE* f = fopen(path, "w");
            if (f) {
                fwrite(content, 1, strlen(content), f);
                fclose(f);
                ESP_LOGI(TAG, "Restored: %s", path);
            } else {
                ESP_LOGE(TAG, "Failed to write: %s", path);
            }
        }
    }

    cJSON_Delete(backup);

    // Ответ
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "{\"success\": true}");

    ESP_LOGI(TAG, "Restore completed successfully");

    // Опционально: перезагрузка
    // xTaskCreate([](void*){ esp_restart(); }, "restart", 2048, NULL, 5, NULL);

    return ESP_OK;
}