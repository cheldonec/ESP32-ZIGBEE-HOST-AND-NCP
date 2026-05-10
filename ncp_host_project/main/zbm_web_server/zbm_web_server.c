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
#include "ncp_host_zb_api_to_ncp.h"

static const char *TAG = "ZBM_WEB_SERVER";

// === Глобальные переменные ===
httpd_handle_t server_handle = NULL;
static int ws_client_fd = -1;

// перенесено в SPIRAM
//static char json_buffer_for_response[16384]; // буфер для cJSON
#define JSON_BUFFER_SIZE 16384
static char* json_buffer_for_response = NULL;

static SemaphoreHandle_t json_buffer_mutex = NULL; // мьютекс для потокобезопасности

// === Глобальные переменные ===
static QueueHandle_t ws_update_queue = NULL; // очередь для уведомлений об обновлении атрибутов
static QueueHandle_t ws_sys_notify_queue = NULL; // очередь для системных уведомлений
static TaskHandle_t ws_update_task_handle = NULL;
#define ZBM_WS_UPDATE_QUEUE_SIZE 32
#define ZBM_WS_SYS_NOTIFY_QUEUE_SIZE 16


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

void sanitize_utf8(char* str) {
    char* p = str;
    while (*p) {
        // ASCII
        if ((p[0] & 0x80) == 0) {
            p++;
            continue;
        }

        // Проверяем 2-байтовую: 110xxxxx 10xxxxxx
        if ((p[0] & 0xE0) == 0xC0) {
            if (p[1] != '\0' && (p[1] & 0xC0) == 0x80) {
                p += 2;
                continue;
            } else {
                *p = '?';
                p++;
                continue;
            }
        }

        // 3-байтовая: 1110xxxx 10xxxxxx 10xxxxxx
        if ((p[0] & 0xF0) == 0xE0) {
            if (p[1] != '\0' && p[2] != '\0' &&
                (p[1] & 0xC0) == 0x80 &&
                (p[2] & 0xC0) == 0x80) {
                p += 3;
                continue;
            } else {
                *p = '?';
                p++;
                continue;
            }
        }

        // 4-байтовая: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        if ((p[0] & 0xF8) == 0xF0) {
            if (p[1] != '\0' && p[2] != '\0' && p[3] != '\0' &&
                (p[1] & 0xC0) == 0x80 &&
                (p[2] & 0xC0) == 0x80 &&
                (p[3] & 0xC0) == 0x80) {
                p += 4;
                continue;
            } else {
                *p = '?';
                p++;
                continue;
            }
        }

        // Всё остальное — битый байт
        *p = '?';
        p++;
    }
}

// отправка JSON
static void send_json_event_to_ws_safe(cJSON* event) {
    ESP_LOGI(TAG, "send_json_event_to_ws_safe");
    if (xSemaphoreTake(json_buffer_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int len = cJSON_PrintPreallocated(event, json_buffer_for_response, JSON_BUFFER_SIZE, false);
        if (len > 0) {
            cJSON_Minify(json_buffer_for_response);
            // ✅ Санитизация
            sanitize_utf8(json_buffer_for_response);
            ws_async_data_t *async_data = malloc(sizeof(ws_async_data_t));
            if (async_data) {
                async_data->hd = server_handle;
                async_data->payload = (uint8_t*)strdup(json_buffer_for_response);
                async_data->len = strlen(json_buffer_for_response);
                httpd_queue_work(server_handle, ws_send_async_task, async_data);
            }
        } else {
            ESP_LOGE(TAG, "Failed to print JSON into buffer (too large?)");
        }
        xSemaphoreGive(json_buffer_mutex);
    } else {
        ESP_LOGE(TAG, "Timeout waiting for JSON buffer mutex");
    }
}
//задача для приёма атрибутов и отправки их в UI
void ws_update_task(void *pvParameters) {
    zbm_ws_update_msg_t msg_attr;
    zbm_ws_sys_notify_msg_t msg_sys;
    static const char* TAG = "WS_UPDATE_TASK";

    ESP_LOGI(TAG, "WS Update Task started");

    while (1) {
        BaseType_t attr_received = xQueueReceive(ws_update_queue, &msg_attr, pdMS_TO_TICKS(10));
        if (attr_received == pdTRUE) {
            ESP_LOGD(TAG, "Received attribute update for GUID: %s", msg_attr.guid);

            // Создаём массив байт правильно
            uint8_t raw_bytes[8] = {0};
            size_t raw_len = msg_attr.value_len > 8 ? 8 : msg_attr.value_len;
            memcpy(raw_bytes, msg_attr.value, raw_len);

            cJSON *event = cJSON_CreateObject();
            cJSON_AddStringToObject(event, "event", "attribute_updated");
            cJSON_AddStringToObject(event, "guid", msg_attr.guid);
            cJSON_AddNumberToObject(event, "type", msg_attr.data_type);

            // Правильно создаём массив байтов: каждый байт → число 0–255
            cJSON *j_value_bytes = cJSON_CreateArray();
            for (size_t i = 0; i < raw_len; i++) {
                cJSON_AddItemToArray(j_value_bytes, cJSON_CreateNumber(raw_bytes[i]));
            }
            cJSON_AddItemToObject(event, "value_bytes", j_value_bytes);

            send_json_event_to_ws_safe(event);
            cJSON_Delete(event);
        }

        BaseType_t sys_received = xQueueReceive(ws_sys_notify_queue, &msg_sys, pdMS_TO_TICKS(10));
        if (sys_received == pdTRUE) {
            ESP_LOGI(TAG, "📤 SysNotify: %s — %s", msg_sys.event_type, msg_sys.message);

            cJSON *event = cJSON_CreateObject();
            cJSON_AddStringToObject(event, "event", "system_notify");
            cJSON_AddStringToObject(event, "type", msg_sys.event_type);
            cJSON_AddStringToObject(event, "message", msg_sys.message);
            if (msg_sys.data) {
                // ✅ Дублируем, не передаём владение!
                cJSON_AddItemToObject(event, "data", cJSON_Duplicate(msg_sys.data, true));
            }

            send_json_event_to_ws_safe(event);
            cJSON_Delete(event); // ✅ Теперь безопасно
            cJSON_Delete(msg_sys.data);
            // ❌ Не обнуляем msg_sys.data — он не владеет памятью
            // memset(&msg_sys, 0, sizeof(msg_sys)); // ← УДАЛИ ЭТУ СТРОКУ!
        }
    }
}


bool zbm_ws_send_data_update_notify(const char* guid, uint8_t data_type, const void* value, size_t value_len) {
    if (!guid || !value || value_len == 0 || value_len > 256) {
        return false;
    }

    ESP_LOGI(TAG, "📤 Sending WS update: GUID=%s, type=%d, len=%d", guid, data_type, value_len);
    zbm_ws_update_msg_t msg = {0};
    strlcpy(msg.guid, guid, sizeof(msg.guid));
    msg.data_type = data_type;
    msg.value_len = value_len;
    memcpy(msg.value, value, value_len);

    BaseType_t ret = xQueueSendToBack(ws_update_queue, &msg, pdMS_TO_TICKS(10));
    if (ret != pdTRUE) {
        ESP_LOGW(TAG, "WS queue full, dropping update for %s", guid);
        return false;
    }

    return true;
}

bool zbm_ws_send_sys_notify(const char* event_type, const char* message, cJSON* data) {
    zbm_ws_sys_notify_msg_t msg = {0};
    strlcpy(msg.event_type, event_type, sizeof(msg.event_type));
    strlcpy(msg.message, message, sizeof(msg.message));

    // 🔁 Создаём копию JSON (если есть)
    if (data) {
        msg.data = cJSON_Duplicate(data, true); // полная рекурсивная копия
        if (!msg.data) {
            ESP_LOGE(TAG, "Failed to duplicate cJSON for sys notify");
            return false;
        }
    } else {
        msg.data = NULL;
    }

    BaseType_t ret = xQueueSendToBack(ws_sys_notify_queue, &msg, pdMS_TO_TICKS(10));
    if (ret != pdTRUE) {
        ESP_LOGW(TAG, "SysNotify queue full, dropping: %s", event_type);
        cJSON_Delete(msg.data); // освобождаем копию
        return false;
    }

    return true; // успех — очередь владеет копией
}

void ws_notify_automation_rule_fired(const char* rule_id, const char* trigger_guid) {
    if (!rule_id) return;

    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "rule_id", rule_id);
    if (trigger_guid) {
        cJSON_AddStringToObject(data, "trigger_guid", trigger_guid);
    }

    zbm_ws_send_sys_notify("automation", "rule_fired", data);
    cJSON_Delete(data);
}

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
    ESP_LOGI(TAG, "WebSocket handshake requested from %s", req->uri);
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
            int len = cJSON_PrintPreallocated(response, json_buffer_for_response, JSON_BUFFER_SIZE, false);
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
    else if (strcmp(cmd->valuestring, "send_zcl_command") == 0) {
        ESP_LOGI(TAG, "Received ZCL command request via JSON");
        cJSON *guid_obj = cJSON_GetObjectItem(req_json, "guid");
        uint8_t tsn = zbm_to_ncp_req_send_zcl_cmd_from_ws_json(req_json);

        // === Ответ клиенту ===
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "event", "command_sent");
        cJSON_AddStringToObject(resp, "guid", guid_obj ? guid_obj->valuestring : "unknown");
        cJSON_AddStringToObject(resp, "status", tsn != 0xFF ? "success" : "failed");
        cJSON_AddNumberToObject(resp, "tsn", tsn);

        if (xSemaphoreTake(json_buffer_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            int len = cJSON_PrintPreallocated(resp, json_buffer_for_response, JSON_BUFFER_SIZE, false);
            if (len > 0) {
                cJSON_Minify(json_buffer_for_response);
                httpd_ws_frame_t frame = {
                    .type = HTTPD_WS_TYPE_TEXT,
                    .payload = (uint8_t*)json_buffer_for_response,
                    .len = strlen(json_buffer_for_response)
                };
                httpd_ws_send_frame(req, &frame);
            }
            xSemaphoreGive(json_buffer_mutex);
        }

        cJSON_Delete(resp);
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

// network open/close
httpd_uri_t uri_zbm_rest_api_post_open_close_zigbee_network = {
    .uri       = "/api/post/zbnetwork/open_close",
    .method    = HTTP_POST,
    .handler   = zbm_rest_api_post_open_close_zigbee_network_handler,
    .user_ctx  = NULL
};

// GET /api/get/zigbee_network/status
httpd_uri_t uri_get_zigbee_network_status = {
    .uri       = "/api/get/zigbee_network/status",
    .method    = HTTP_GET,
    .handler   = zbm_rest_api_get_zigbee_network_status_handler,
    .user_ctx  = NULL
};

// GET /api/get_server_status  запрос токена сессии, если поменялся то клиент узнает, что ему надо ребутнуться
// токен меняется если esp перезагрузилась
httpd_uri_t uri_get_server_status = {
    .uri       = "/api/get_server_status",
    .method    = HTTP_GET,
    .handler   = zbm_rest_api_get_status_handler,
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

// ========================== ZDO =========================
httpd_uri_t uri_active_endpoint = {
    .uri       = "/api/zdo/active_endpoint_req",
    .method    = HTTP_POST,
    .handler   = zbm_rest_api_post_active_endpoint_handler,
    .user_ctx  = NULL
};

httpd_uri_t uri_simple_desc = {
    .uri       = "/api/zdo/simple_desc",
    .method    = HTTP_POST,
    .handler   = zbm_rest_api_post_simple_descriptor_handler,
    .user_ctx  = NULL
};

//Обработчик: POST /api/device/update_friendly_name — изменить friendly_name устройства
httpd_uri_t uri_update_dev_friendly_name = {
    .uri       = "/api/device/update_friendly_name",
    .method    = HTTP_POST,
    .handler   = zbm_rest_api_post_update_dev_friendly_name_handler,
    .user_ctx  = NULL
};

//====                 RULES                 ====

httpd_uri_t api_rules_get_vars = {
    .uri       = "/api/get/vars",
    .method    = HTTP_GET,
    .handler   = zbm_rest_api_get_vars_handler
};

httpd_uri_t api_rules_post_vars = {
    .uri       = "/api/post/var/*",
    .method    = HTTP_POST,
    .handler   = zbm_rest_api_post_var_handler
};

httpd_uri_t api_rules_get = {
    .uri       = "/api/rules",
    .method    = HTTP_GET,
    .handler   = zbm_rest_api_get_rules_handler
};


httpd_uri_t api_rule_get = {
    .uri       = "/api/rule/*",  // wildcard
    .method    = HTTP_GET,
    .handler   = zbm_rest_api_get_rule_by_id_handler
};


httpd_uri_t api_rule_post = {
    .uri       = "/api/rule",
    .method    = HTTP_POST,
    .handler   = zbm_rest_api_post_rule_handler
};


httpd_uri_t api_rule_delete = {
    .uri       = "/api/rule/*",
    .method    = HTTP_DELETE,
    .handler   = zbm_rest_api_delete_rule_handler
};


httpd_uri_t api_rule_enable = {
    .uri       = "/api/rule/*/enable",
    .method    = HTTP_POST,
    .handler   = zbm_rest_api_post_rule_enable_handler
};


httpd_uri_t api_rule_disable = {
    .uri       = "/api/rule/*/disable",
    .method    = HTTP_POST,
    .handler   = zbm_rest_api_post_rule_disable_handler
};

httpd_uri_t api_rule_run = {
    .uri       = "/api/rule/*/run",
    .method    = HTTP_POST,
    .handler   = zbm_rest_api_post_rule_run_handler
};

//====                 END RULES             ====

//====                 BEHAVIORS                 ====

static httpd_uri_t api_behaviors_get = {
    .uri       = "/api/behaviors",
    .method    = HTTP_GET,
    .handler   = zbm_rest_api_get_behaviors_handler
};

static httpd_uri_t api_behavior_get = {
    .uri       = "/api/behavior/*",
    .method    = HTTP_GET,
    .handler   = zbm_rest_api_get_behavior_by_id_handler
};

static httpd_uri_t api_behavior_post = {
    .uri       = "/api/behavior",
    .method    = HTTP_POST,
    .handler   = zbm_rest_api_post_behavior_handler
};

static httpd_uri_t api_behavior_delete = {
    .uri       = "/api/behavior/*",
    .method    = HTTP_DELETE,
    .handler   = zbm_rest_api_delete_behavior_handler
};

static httpd_uri_t api_behavior_enable = {
    .uri       = "/api/behavior/*/enable",
    .method    = HTTP_POST,
    .handler   = zbm_rest_api_post_behavior_enable_handler
};

static httpd_uri_t api_behavior_disable = {
    .uri       = "/api/behavior/*/disable",
    .method    = HTTP_POST,
    .handler   = zbm_rest_api_post_behavior_disable_handler
};

static httpd_uri_t api_behavior_run = {
    .uri       = "/api/behavior/*/run",
    .method    = HTTP_POST,
    .handler   = zbm_rest_api_post_behavior_run_handler
};

//===                  END BEHAVIORS             ===

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
    ESP_LOGI(TAG, "Starting web server...");

    // === 1. ГАРАНТИРУЕМ ЧИСТОЕ СОСТОЯНИЕ (очищаем после предыдущего запуска) ===
    if (ws_update_task_handle) {
        vTaskDelete(ws_update_task_handle);
        ws_update_task_handle = NULL;
        ESP_LOGD(TAG, "Old ws_update_task deleted");
    }

    if (ws_update_queue) {
        vQueueDelete(ws_update_queue);
        ws_update_queue = NULL;
        ESP_LOGD(TAG, "Old ws_update_queue deleted");
    }

    if (ws_sys_notify_queue) {
        vQueueDelete(ws_sys_notify_queue);
        ws_sys_notify_queue = NULL;
        ESP_LOGD(TAG, "Old ws_sys_notify_queue deleted");
    }

    if (json_buffer_mutex) {
        vSemaphoreDelete(json_buffer_mutex);
        json_buffer_mutex = NULL;
        ESP_LOGD(TAG, "Old json_buffer_mutex deleted");
    }

    if (server_handle) {
        httpd_stop(server_handle);
        server_handle = NULL;
        ESP_LOGW(TAG, "Old HTTP server stopped");
    }

    // создаём буфер для JSON
    json_buffer_for_response = heap_caps_malloc(JSON_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (!json_buffer_for_response) {
        ESP_LOGE(TAG, "Failed to allocate JSON buffer in PSRAM");
    }

    // === 2. Создаём очереди ===
    ws_update_queue = xQueueCreate(ZBM_WS_UPDATE_QUEUE_SIZE, sizeof(zbm_ws_update_msg_t));
    if (!ws_update_queue) {
        ESP_LOGE(TAG, "Failed to create WS update queue");
        return;
    }
    ESP_LOGI(TAG, "WS update queue created");

    ws_sys_notify_queue = xQueueCreate(ZBM_WS_SYS_NOTIFY_QUEUE_SIZE, sizeof(zbm_ws_sys_notify_msg_t));
    if (!ws_sys_notify_queue) {
        ESP_LOGE(TAG, "Failed to create WS sys notify queue");
        vQueueDelete(ws_update_queue);
        ws_update_queue = NULL;
        return;
    }
    ESP_LOGI(TAG, "WS sys notify queue created");

    // === 3. Создаём мьютекс ДО создания задачи ===
    json_buffer_mutex = xSemaphoreCreateMutex();
    if (!json_buffer_mutex) {
        ESP_LOGE(TAG, "Failed to create JSON buffer mutex");
        vQueueDelete(ws_update_queue);
        vQueueDelete(ws_sys_notify_queue);
        ws_update_queue = NULL;
        ws_sys_notify_queue = NULL;
        return;
    }
    ESP_LOGI(TAG, "JSON buffer mutex created");

    // === 4. Создаём задачу ОБНОВЛЕНИЯ WS (только после всех зависимостей) ===
    if (ws_update_task_handle == NULL) {
        BaseType_t ret = xTaskCreatePinnedToCore(
            ws_update_task,
            "ws_update_task",
            8192,
            NULL,
            8,
            &ws_update_task_handle,
            1
        );
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create ws_update_task");
            vSemaphoreDelete(json_buffer_mutex);
            vQueueDelete(ws_update_queue);
            vQueueDelete(ws_sys_notify_queue);
            json_buffer_mutex = NULL;
            ws_update_queue = NULL;
            ws_sys_notify_queue = NULL;
            return;
        }
        ESP_LOGI(TAG, "WS update task created");
    }

    // === 5. Настройка и запуск HTTP-сервера ===
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 17408;
    config.core_id = 1;
    config.send_wait_timeout = 5;
    config.recv_wait_timeout = 5;
    config.task_priority = 5;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 56;
    config.max_open_sockets = 8;

    if (httpd_start(&server_handle, &config) == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server started on core %d", config.core_id);

        // === Регистрация обработчиков ===

        // SSDP
        httpd_register_uri_handler(server_handle, &get_ssdp_description_xml);

        // Главная страница
        httpd_register_uri_handler(server_handle, &uri_get_index_html_req);

        // Специальные Android/Windows
        httpd_register_uri_handler(server_handle, &uri_generate_204_windows);
        httpd_register_uri_handler(server_handle, &uri_ipv6check);

        // Favicon и манифест
        httpd_register_uri_handler(server_handle, &uri_favicon);
        httpd_register_uri_handler(server_handle, &uri_manifest);

        // WebSocket
        httpd_register_uri_handler(server_handle, &uri_get_req_from_ws);

        // === REST API ===
        httpd_register_uri_handler(server_handle, &uri_zbm_rest_api_get_devices);
        httpd_register_uri_handler(server_handle, &uri_zbm_rest_api_get_device_by_short);
        httpd_register_uri_handler(server_handle, &uri_zbm_rest_api_get_device_by_ieee);
        httpd_register_uri_handler(server_handle, &api_coordinator_get);
        httpd_register_uri_handler(server_handle, &api_coordinator_post);
        httpd_register_uri_handler(server_handle, &uri_zbm_rest_api_post_open_close_zigbee_network);
        httpd_register_uri_handler(server_handle, &uri_get_zigbee_network_status);
        httpd_register_uri_handler(server_handle, &uri_get_server_status);
        httpd_register_uri_handler(server_handle, &uri_active_endpoint);
        httpd_register_uri_handler(server_handle, &uri_simple_desc);
        httpd_register_uri_handler(server_handle, &uri_update_dev_friendly_name);

        // Rules
        httpd_register_uri_handler(server_handle, &api_rules_get);
        httpd_register_uri_handler(server_handle, &api_rule_get);
        httpd_register_uri_handler(server_handle, &api_rule_post);
        httpd_register_uri_handler(server_handle, &api_rule_delete);
        httpd_register_uri_handler(server_handle, &api_rule_enable);
        httpd_register_uri_handler(server_handle, &api_rule_disable);
        httpd_register_uri_handler(server_handle, &api_rule_run);
        httpd_register_uri_handler(server_handle, &api_rules_get_vars);
        httpd_register_uri_handler(server_handle, &api_rules_post_vars);

        // Behaviors
        httpd_register_uri_handler(server_handle, &api_behaviors_get);
        httpd_register_uri_handler(server_handle, &api_behavior_get);
        httpd_register_uri_handler(server_handle, &api_behavior_post);
        httpd_register_uri_handler(server_handle, &api_behavior_delete);
        httpd_register_uri_handler(server_handle, &api_behavior_enable);
        httpd_register_uri_handler(server_handle, &api_behavior_disable);
        httpd_register_uri_handler(server_handle, &api_behavior_run);

        // SPIFFS: config
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

        // Статика
        httpd_register_uri_handler(server_handle, &uri_static_css);
        httpd_register_uri_handler(server_handle, &uri_static_js);
        httpd_register_uri_handler(server_handle, &uri_static_media);
        httpd_register_uri_handler(server_handle, &uri_static); // должен быть последним!
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        // Освобождаем всё, что создали
        vSemaphoreDelete(json_buffer_mutex);
        vQueueDelete(ws_update_queue);
        vQueueDelete(ws_sys_notify_queue);
        json_buffer_mutex = NULL;
        ws_update_queue = NULL;
        ws_sys_notify_queue = NULL;
        return;
    }

    ESP_LOGI(TAG, "Web server fully started and ready");
}

void stop_webserver(void)
{
    if (server_handle) {
        httpd_stop(server_handle);
        server_handle = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }

    if (json_buffer_for_response) {
        heap_caps_free(json_buffer_for_response);
        json_buffer_for_response = NULL;
    }

    // Удаляем мьютекс
    if (json_buffer_mutex) {
        vSemaphoreDelete(json_buffer_mutex);
        json_buffer_mutex = NULL;
    }
}