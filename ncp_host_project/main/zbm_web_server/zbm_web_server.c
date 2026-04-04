#include "zbm_web_server.h"
#include "zbm_rest_api.h"
#include "esp_log.h"
#include "zbm_spiffs_helper.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "zbm_spiffs_rest_api.h"
#include "zbm_coordinator.h"
#include "ssdp_server.h"
#include "socket.h"

static const char *TAG = "ZBM_WEB_SERVER";

// === Глобальные переменные ===
httpd_handle_t server_handle = NULL;
static int ws_client_fd = -1;
static char json_buffer_for_response[8192]; // буфер для cJSON
static SemaphoreHandle_t json_buffer_mutex = NULL; // мьютекс для потокобезопасности

// MIME-типы
static const char* get_content_type(const char* path) {
    if (strstr(path, ".html")) return "text/html";
    else if (strstr(path, ".css")) return "text/css";
    else if (strstr(path, ".js")) return "application/javascript";
    else if (strstr(path, ".png")) return "image/png";
    else if (strstr(path, ".ico")) return "image/x-icon";
    else if (strstr(path, ".json")) return "application/json";
    else if (strstr(path, ".jpg") || strstr(path, ".jpeg")) return "image/jpeg";
    else if (strstr(path, ".gif")) return "image/gif";
    else if (strstr(path, ".svg")) return "image/svg+xml";
    else return "application/octet-stream";
}



// Функция отправки через web socket отдельным потоком httpd_queue_work(server_handle, ws_send_async_task, async_data)
void ws_send_async_task(void *arg);

// Обработчики
esp_err_t get_index_html_req_handler(httpd_req_t *req);
esp_err_t get_from_ws_handler(httpd_req_t *req);

// Асинхронная отправка WS
void ws_send_async_task(void *arg)
{
    ws_async_data_t *data = (ws_async_data_t *)arg;
    if (!data || !data->hd || !data->payload) {
        free(data);
        return;
    }

    httpd_ws_frame_t frame = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = data->payload,
        .len = data->len
    };

    size_t fd_count = CONFIG_LWIP_MAX_LISTENING_TCP;
    int client_fds[CONFIG_LWIP_MAX_LISTENING_TCP];

    esp_err_t ret = httpd_get_client_list(data->hd, &fd_count, client_fds);
    if (ret != ESP_OK) {
        free(data->payload);
        free(data);
        return;
    }

    //bool sent = false;
    for (size_t i = 0; i < fd_count; i++) {
        int client_info = httpd_ws_get_fd_info(data->hd, client_fds[i]);
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            if (httpd_ws_send_frame_async(data->hd, client_fds[i], &frame) == ESP_OK) {
                //sent = true;
            }
        }
    }

    free(data->payload);
    free(data);
}



//============================================================ WEB SERVER HANDLERS =================================================

esp_err_t get_index_html_req_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "REQ get_req_handler (uri get): %s", req->uri); 

    struct stat st;
    if (stat(ZBM_WEB_SERVER_HOME_PAGE, &st) != 0 || S_ISDIR(st.st_mode)) {
        ESP_LOGE(TAG, "index.html not found at %s", ZBM_WEB_SERVER_HOME_PAGE);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "index.html not found");
        return ESP_FAIL;
    }

    FILE *f = fopen(ZBM_WEB_SERVER_HOME_PAGE, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open index.html");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Cannot open index.html");
        return ESP_FAIL;
    }

    char chunk[1024];
    size_t read_len;

    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");

    while ((read_len = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, read_len) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    ESP_LOGI(TAG, "Sent index.html successfully");
    return ESP_OK;
}

// === Обработчик для Windows IPv6 проверки ===
esp_err_t ipv6check_handler(httpd_req_t *req)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    const char* peer = "unknown";
    if (getpeername(httpd_req_to_sockfd(req), (struct sockaddr*)&addr, &addr_len) == 0) {
        peer = inet_ntoa(addr.sin_addr);
    }
    ESP_LOGI(TAG, "/ipv6check requested from %s", peer);

    const char *response = "OK";
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

//обработчики для разных ОС, когда они подключаются по wifi к нашей AP
// === Обработчик для Android: /generate_204 ===
esp_err_t generate_204_handler(httpd_req_t *req)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    const char* peer = "unknown";
    if (getpeername(httpd_req_to_sockfd(req), (struct sockaddr*)&addr, &addr_len) == 0) {
        peer = inet_ntoa(addr.sin_addr);
    }
    ESP_LOGI(TAG, "/generate_204 requested from %s", peer);

    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t static_file_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Handling static file: %s", req->uri);

    // Пропускаем только нужные пути
    /*if (strncmp(req->uri, "/static/", 8) != 0 &&
    strcmp(req->uri, "/favicon.ico") != 0 &&
    strcmp(req->uri, "/manifest.json") != 0) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
    return ESP_OK;
   }*/

    // Проверяем длину URI
    size_t uri_len = strlen(req->uri);
    if (uri_len >= 200) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "URI too long");
        return ESP_OK;
    }

    char filepath[256];
    int len = snprintf(filepath, sizeof(filepath), "%s%s", SPIFFS_ZBM_UI_MOUNT_POINT, req->uri);
    if (len < 0 || len >= sizeof(filepath)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Path too long");
        return ESP_OK;
    }

    struct stat st;
    if (stat(filepath, &st) != 0 || S_ISDIR(st.st_mode)) {
        ESP_LOGE(TAG, "❌ File not found: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_OK;
    }

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        ESP_LOGE(TAG, "❌ Cannot open file: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_OK;
    }

    // Определяем MIME-тип
    httpd_resp_set_type(req, get_content_type(req->uri));
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");

    // Читаем и отправляем по кускам
    char *chunk = malloc(1024);
    if (!chunk) {
        fclose(file);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    size_t read_len;
    while ((read_len = fread(chunk, 1, 1024, file)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, read_len) != ESP_OK) {
            break;
        }
    }

    free(chunk);
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t get_from_ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake requested");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = { .type = HTTPD_WS_TYPE_TEXT };
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS recv failed (len): %s", esp_err_to_name(ret));
        return ret;
    }

    if (ws_pkt.len == 0) {
        ESP_LOGW(TAG, "Empty WS frame");
        return ESP_OK;
    }

    uint8_t *buf = malloc(ws_pkt.len + 1);
    if (!buf) {
        ESP_LOGE(TAG, "WS malloc failed");
        return ESP_ERR_NO_MEM;
    }
    memset(buf, 0, ws_pkt.len + 1);

    cJSON *req_json = NULL;

    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS recv failed (payload): %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI(TAG, "WS RX: %.*s", ws_pkt.len, buf);
    req_json = cJSON_Parse((char*)buf);
    if (!req_json) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        goto cleanup;
    }

    cJSON *cmd = cJSON_GetObjectItem(req_json, "cmd");
    if (!cmd || !cJSON_IsString(cmd)) {
        ESP_LOGE(TAG, "Missing or invalid 'cmd' field");
        goto cleanup;
    }

    // === Обработка команд ===
    if (strcmp(cmd->valuestring, "get_network_status") == 0) {
        cJSON *response = cJSON_CreateObject();
        if (!response) {
            ESP_LOGE(TAG, "Failed to create JSON response");
            goto cleanup;
        }

        cJSON_AddStringToObject(response, "event", "network_status");
        // Здесь можно добавить данные: IP, RSSI, etc.

        // Печать JSON в общий буфер с защитой мьютексом
        if (xSemaphoreTake(json_buffer_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            int len = cJSON_PrintPreallocated(response, json_buffer_for_response, sizeof(json_buffer_for_response), false);
            if (len > 0) {
                cJSON_Minify(json_buffer_for_response);
                httpd_ws_frame_t frame = {
                    .type = HTTPD_WS_TYPE_TEXT,
                    .payload = (uint8_t*)json_buffer_for_response,
                    .len = strlen(json_buffer_for_response)
                };
                httpd_ws_send_frame(req, &frame);
            } else {
                ESP_LOGE(TAG, "Failed to print JSON into buffer (too large?)");
            }
            xSemaphoreGive(json_buffer_mutex);
        } else {
            ESP_LOGE(TAG, "Timeout waiting for JSON buffer mutex");
        }

        cJSON_Delete(response);
    }
    else {
        ESP_LOGW(TAG, "Unknown command: %s", cmd->valuestring);
    }

cleanup:
    cJSON_Delete(req_json);
    free(buf);
    return ESP_OK;
}

//============================================================ URI MAPPING =======================================================

httpd_uri_t uri_get_index_html_req = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_index_html_req_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_generate_204_windows = {
    .uri = "/204",
    .method = HTTP_GET,
    .handler = generate_204_handler,
    .user_ctx = NULL
};

// Для Windows: /ipv6check
httpd_uri_t uri_ipv6check = {
    .uri = "/ipv6check",
    .method = HTTP_GET,
    .handler = ipv6check_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_static = {
    .uri = "/*",  // ловит всё, что не поймано раньше
    .method = HTTP_GET,
    .handler = static_file_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_static_css = {
    .uri = "/static/css/*",
    .method = HTTP_GET,
    .handler = static_file_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_static_js = {
    .uri = "/static/js/*",
    .method = HTTP_GET,
    .handler = static_file_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_static_media = {
    .uri = "/static/media/*",
    .method = HTTP_GET,
    .handler = static_file_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_favicon = {
    .uri = "/favicon.ico",
    .method = HTTP_GET,
    .handler = static_file_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_manifest = {
    .uri = "/manifest.json",
    .method = HTTP_GET,
    .handler = static_file_handler,
    .user_ctx = NULL
};


httpd_uri_t uri_get_req_from_ws = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = get_from_ws_handler,
    .user_ctx = NULL,
    .is_websocket = true
};

httpd_uri_t uri_zbm_rest_api_get_devices = {
    .uri       = "/api/devices",
    .method    = HTTP_GET,
    .handler   = zbm_rest_api_get_devices_handler,
    .user_ctx  = NULL
};

//http://192.168.4.1/api/device/by_short?addr=0x1234
httpd_uri_t uri_zbm_rest_api_get_device_by_short = {
    .uri       = "/api/device/by_short",
    .method    = HTTP_GET,
    .handler   = zbm_rest_api_get_device_by_short_handler,
    .user_ctx  = NULL
};

httpd_uri_t uri_zbm_rest_api_get_device_by_ieee = {
    .uri       = "/api/device/by_ieee",
    .method    = HTTP_GET,
    .handler   = zbm_rest_api_get_device_by_ieee_handler,
    .user_ctx  = NULL
};

// === Coordinator ===
httpd_uri_t api_coordinator_get = {
    .uri       = "/api/get/coordinator",
    .method    = HTTP_GET,
    .handler   = zbm_rest_api_get_coordinator_handler,
    .user_ctx  = NULL
};

httpd_uri_t api_coordinator_post = {
    .uri       = "/api/post/coordinator",
    .method    = HTTP_POST,
    .handler   = zbm_rest_api_post_coordinator_handler,
    .user_ctx  = NULL
};
// === SPIFFS API ===
// === Для /api/spiffs/config ===
// === Для config ===
static httpd_uri_t uri_spiffs_config_ls = {
    .uri       = "/api/spiffs/config/ls",
    .method    = HTTP_GET,
    .handler   = spiffs_api_ls_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_CONF_MOUNT_POINT
};

static httpd_uri_t uri_spiffs_config_get_file = {
    .uri       = "/api/spiffs/config/get/*",
    .method    = HTTP_GET,
    .handler   = spiffs_api_get_file_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_CONF_MOUNT_POINT
};

static httpd_uri_t uri_spiffs_config_save_file = {
    .uri       = "/api/spiffs/config/save/*",
    .method    = HTTP_POST,
    .handler   = spiffs_api_save_file_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_CONF_MOUNT_POINT
};

static httpd_uri_t uri_spiffs_config_delete_file = {
    .uri       = "/api/spiffs/config/delete/*",
    .method    = HTTP_POST,
    .handler   = spiffs_api_delete_file_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_CONF_MOUNT_POINT
};

// === Для quirks ===
static httpd_uri_t uri_spiffs_quirks_ls = {
    .uri       = "/api/spiffs/quirks/ls",
    .method    = HTTP_GET,
    .handler   = spiffs_api_ls_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_QUIRKS_MOUNT_POINT
};

static httpd_uri_t uri_spiffs_quirks_get_file = {
    .uri       = "/api/spiffs/quirks/get/*",
    .method    = HTTP_GET,
    .handler   = spiffs_api_get_file_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_QUIRKS_MOUNT_POINT
};

static httpd_uri_t uri_spiffs_quirks_save_file = {
    .uri       = "/api/spiffs/quirks/save/*",
    .method    = HTTP_POST,
    .handler   = spiffs_api_save_file_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_QUIRKS_MOUNT_POINT
};

static httpd_uri_t uri_spiffs_quirks_delete_file = {
    .uri       = "/api/spiffs/quirks/delete/*",
    .method    = HTTP_POST,
    .handler   = spiffs_api_delete_file_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_QUIRKS_MOUNT_POINT
};

// === Для certs ===
static httpd_uri_t uri_spiffs_certs_ls = {
    .uri       = "/api/spiffs/certs/ls",
    .method    = HTTP_GET,
    .handler   = spiffs_api_ls_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_CERTS_MOUNT_POINT
};

static httpd_uri_t uri_spiffs_certs_get_file = {
    .uri       = "/api/spiffs/certs/get/*",
    .method    = HTTP_GET,
    .handler   = spiffs_api_get_file_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_CERTS_MOUNT_POINT
};

static httpd_uri_t uri_spiffs_certs_save_file = {
    .uri       = "/api/spiffs/certs/save/*",
    .method    = HTTP_POST,
    .handler   = spiffs_api_save_file_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_CERTS_MOUNT_POINT
};

static httpd_uri_t uri_spiffs_certs_delete_file = {
    .uri       = "/api/spiffs/certs/delete/*",
    .method    = HTTP_POST,
    .handler   = spiffs_api_delete_file_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_CERTS_MOUNT_POINT
};
//SPIFFS_ZBM_UI_MOUNT_POINT
// === Для WEB UI ===
static httpd_uri_t uri_spiffs_webui_ls = {
    .uri       = "/api/spiffs/webui/ls",
    .method    = HTTP_GET,
    .handler   = spiffs_api_ls_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_UI_MOUNT_POINT
};

static httpd_uri_t uri_spiffs_webui_get_file = {
    .uri       = "/api/spiffs/webui/get/*",
    .method    = HTTP_GET,
    .handler   = spiffs_api_get_file_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_UI_MOUNT_POINT
};

static httpd_uri_t uri_spiffs_webui_save_file = {
    .uri       = "/api/spiffs/webui/save/*",
    .method    = HTTP_POST,
    .handler   = spiffs_api_save_file_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_UI_MOUNT_POINT
};

static httpd_uri_t uri_spiffs_webui_delete_file = {
    .uri       = "/api/spiffs/webui/delete/*",
    .method    = HTTP_POST,
    .handler   = spiffs_api_delete_file_handler,
    .user_ctx  = (void*)SPIFFS_ZBM_UI_MOUNT_POINT
};
httpd_uri_t uri_spiffs_backup = {
    .uri       = "/api/backup",
    .method    = HTTP_GET,
    .handler   = spiffs_api_backup_handler,
    .user_ctx  = NULL
};

httpd_uri_t uri_spiffs_restore = {
    .uri       = "/api/restore",
    .method    = HTTP_POST,
    .handler   = spiffs_api_restore_handler,
    .user_ctx  = NULL
};

// SSDP Descriptor
// SSDP description
httpd_uri_t get_ssdp_description_xml = {
    .uri      = "/description.xml",
    .method   = HTTP_GET,
    .handler  = description_xml_handler, // функция обработки находится в ssdp_server.c
    .user_ctx = NULL
};
//============================================================ SERVER CONTROL ====================================================

void start_webserver(void)
{
    if (server_handle) {
        ESP_LOGW(TAG, "Web server already running");
        return;
    }

    // Создаём мьютекс для JSON-буфера
    if (!json_buffer_mutex) {
        json_buffer_mutex = xSemaphoreCreateMutex();
        if (!json_buffer_mutex) {
            ESP_LOGE(TAG, "Failed to create JSON buffer mutex");
            return;
        }
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 16384;
    config.core_id = 1;
    config.send_wait_timeout = 5;
    config.recv_wait_timeout = 5;
    config.task_priority = 5;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 40;
    config.max_open_sockets = 8;

    if (httpd_start(&server_handle, &config) == ESP_OK) {
        // === Сначала: специфичные API и статика ===

        // SSDP
        httpd_register_uri_handler(server_handle, &get_ssdp_description_xml);

        // Главная страница
        httpd_register_uri_handler(server_handle, &uri_get_index_html_req);

        //204, ip6
        httpd_register_uri_handler(server_handle, &uri_generate_204_windows);
        httpd_register_uri_handler(server_handle, &uri_ipv6check);

        // Favicon и манифест
        httpd_register_uri_handler(server_handle, &uri_favicon);
        httpd_register_uri_handler(server_handle, &uri_manifest);

        // WebSocket
        httpd_register_uri_handler(server_handle, &uri_get_req_from_ws);

        // === REST API: все /api/... ===
        httpd_register_uri_handler(server_handle, &uri_zbm_rest_api_get_devices);
        httpd_register_uri_handler(server_handle, &uri_zbm_rest_api_get_device_by_short);
        httpd_register_uri_handler(server_handle, &uri_zbm_rest_api_get_device_by_ieee);
        httpd_register_uri_handler(server_handle, &api_coordinator_get);
        httpd_register_uri_handler(server_handle, &api_coordinator_post);

        // === SPIFFS API ===
        // config
        httpd_register_uri_handler(server_handle, &uri_spiffs_config_ls);
        httpd_register_uri_handler(server_handle, &uri_spiffs_config_get_file);
        httpd_register_uri_handler(server_handle, &uri_spiffs_config_save_file);
        httpd_register_uri_handler(server_handle, &uri_spiffs_config_delete_file);

        // quirks
        httpd_register_uri_handler(server_handle, &uri_spiffs_quirks_ls);
        httpd_register_uri_handler(server_handle, &uri_spiffs_quirks_get_file);
        httpd_register_uri_handler(server_handle, &uri_spiffs_quirks_save_file);
        httpd_register_uri_handler(server_handle, &uri_spiffs_quirks_delete_file);

        // certs
        httpd_register_uri_handler(server_handle, &uri_spiffs_certs_ls);
        httpd_register_uri_handler(server_handle, &uri_spiffs_certs_get_file);
        httpd_register_uri_handler(server_handle, &uri_spiffs_certs_save_file);
        httpd_register_uri_handler(server_handle, &uri_spiffs_certs_delete_file);

        // webui
        httpd_register_uri_handler(server_handle, &uri_spiffs_webui_ls);
        httpd_register_uri_handler(server_handle, &uri_spiffs_webui_get_file);
        httpd_register_uri_handler(server_handle, &uri_spiffs_webui_save_file);
        httpd_register_uri_handler(server_handle, &uri_spiffs_webui_delete_file);

        // backup/restore
        httpd_register_uri_handler(server_handle, &uri_spiffs_backup);
        httpd_register_uri_handler(server_handle, &uri_spiffs_restore);

        // === Статические файлы: от более точных к общим ===
        httpd_register_uri_handler(server_handle, &uri_static_css);
        httpd_register_uri_handler(server_handle, &uri_static_js);
        httpd_register_uri_handler(server_handle, &uri_static_media);

        // === В самом конце — общий обработчик "/*" ===
        httpd_register_uri_handler(server_handle, &uri_static); // должен быть последним!
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
        vSemaphoreDelete(json_buffer_mutex);
        json_buffer_mutex = NULL;
    }
}

void stop_webserver(void)
{
    if (server_handle) {
        httpd_stop(server_handle);
        server_handle = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }

    // Удаляем мьютекс
    if (json_buffer_mutex) {
        vSemaphoreDelete(json_buffer_mutex);
        json_buffer_mutex = NULL;
    }
}