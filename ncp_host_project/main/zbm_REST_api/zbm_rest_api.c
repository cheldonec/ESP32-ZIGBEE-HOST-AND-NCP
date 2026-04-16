#include "zbm_rest_api.h"
#include "zbm_device_db.h"
#include "zbm_dev_types.h"
#include "zbm_core_sync.h"
#include "cJSON.h"
#include "zbm_web_server.h"
#include "esp_log.h"
#include "zbm_coordinator.h"
#include "zbm_ncp_connect.h"
#include "ncp_host_zb_api_to_ncp.h"
#include "esp_random.h"

static const char* TAG = "ZBM_REST_API";

static zbm_coordinator_t* g_zbm_coordinator = &zbm_coordinator;

static char g_session_token[9]; // уникальный идентификатор сессии, создаётся при перезагрузке, нужен для того, чтобы web клиенты могли отловить перезапуск и обновиться

// === Вспомогательная функция: форматирование IEEE-адреса в строку ===
/*static void format_ieee_addr(char* buf, size_t size, const uint8_t* ieee_addr) {
    snprintf(buf, size, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             ieee_addr[0], ieee_addr[1], ieee_addr[2], ieee_addr[3],
             ieee_addr[4], ieee_addr[5], ieee_addr[6], ieee_addr[7]);
}*/

// Генерация токена при старте
void generate_session_token() {
    uint32_t rand = esp_random();
    snprintf(g_session_token, sizeof(g_session_token), "%08x", (unsigned int)(rand & 0xFFFFFFFF));
    ESP_LOGW(TAG, "Session token generated: %s", g_session_token);
}

esp_err_t zbm_rest_api_get_status_handler(httpd_req_t* req) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "status", "online");
    cJSON_AddStringToObject(json, "session_token", g_session_token);
    cJSON_AddStringToObject(json, "version", "1.0.0");

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

// === Статическая функция-коллбэк для перебора устройств ===
static void collect_device_to_json(zbm_dev_t* dev, void* ctx) {
    cJSON* array = (cJSON*)ctx;
    cJSON* dev_json = device_to_brief_json(dev);
    if (dev_json) {
        cJSON_AddItemToArray(array, dev_json);
    }
}

// === Обработчик: GET /api/devices — список всех устройств ===
// === Обработчик: GET /api/devices — список всех устройств ===
esp_err_t zbm_rest_api_get_devices_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/devices");

    cJSON* devices = cJSON_CreateArray();
    if (!devices) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    zbm_device_db_foreach_safe(collect_device_to_json, devices);

    char *json_str = cJSON_PrintUnformatted(devices);
    cJSON_Delete(devices);
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

// === Обработчик: GET /api/device/by_short?addr=0x1234 — по короткому адресу ===
esp_err_t zbm_rest_api_get_device_by_short_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/device/by_short");

    char addr_str[16];
    size_t addr_len = httpd_req_get_url_query_len(req) + 1;
    if (addr_len > sizeof(addr_str)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Query too long");
        return ESP_OK;
    }

    httpd_req_get_url_query_str(req, addr_str, addr_len);
    char* param = strstr(addr_str, "addr=");
    uint16_t short_addr = 0;

    if (!param || sscanf(param, "addr=0x%04hx", &short_addr) != 1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid 'addr' parameter");
        return ESP_OK;
    }

    ESP_LOGI(TAG,"Find device short 0x%04hx ", short_addr);
    zbm_dev_t* dev = zbm_find_device_in_devdb_by_short_safe(short_addr);
    if (!dev) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Device not found");
        return ESP_OK;
    }

    cJSON* json = device_to_json(dev);
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

// === Обработчик: GET /api/device/by_ieee?ieee=00:11:22... — по IEEE ===
esp_err_t zbm_rest_api_get_device_by_ieee_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/device/by_ieee");

    char query_str[128];
    size_t len = httpd_req_get_url_query_len(req) + 1;
    if (len > sizeof(query_str)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Query too long");
        return ESP_OK;
    }

    httpd_req_get_url_query_str(req, query_str, len);
    char* param = strstr(query_str, "ieee=");
    if (!param) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'ieee' parameter");
        return ESP_OK;
    }

    uint8_t ieee_addr[8];
    int count = sscanf(param + 5,
                       "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX",
                       &ieee_addr[0], &ieee_addr[1], &ieee_addr[2], &ieee_addr[3],
                       &ieee_addr[4], &ieee_addr[5], &ieee_addr[6], &ieee_addr[7]);

    if (count != 8) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid IEEE format");
        return ESP_OK;
    }

    zbm_dev_t* dev = zbm_find_device_in_devdb_by_ieee_safe(ieee_addr);
    if (!dev) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Device not found");
        return ESP_OK;
    }

    cJSON* json = device_to_json(dev);
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

// === Обработчик: GET /api/coordinator — получить данные координатора ===
esp_err_t zbm_rest_api_get_coordinator_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/coordinator (GET)");

    if (!g_zbm_coordinator) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Coordinator not initialized");
        return ESP_OK;
    }

    cJSON* json = zbm_coordinator_to_json(g_zbm_coordinator);
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

// === Обработчик: POST /api/coordinator — обновить ВЕСЬ объект координатора ===
esp_err_t zbm_rest_api_post_coordinator_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/coordinator (POST)");

    if (!g_zbm_coordinator) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Coordinator not initialized");
        return ESP_OK;
    }

    if (req->content_len > 2048) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request too large");
        return ESP_OK;
    }

    char* body = malloc(req->content_len + 1);
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

    // === Создаём временный объект из JSON ===
    zbm_coordinator_t* new_coord = zbm_coordinator_from_json(json);
    if (!new_coord) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid coordinator data");
        return ESP_OK;
    }

    // === Проверка short_addr ===
    if (new_coord->zb_short_address != 0x0000) {
        ESP_LOGW(TAG, "Reject: coordinator short_addr must be 0x0000, got 0x%04X", new_coord->zb_short_address);
        zbm_free_coordinator(new_coord);
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Short address must be 0x0000");
        return ESP_OK;
    }

    // === Применяем изменения ===

    // friendly_name
    if (g_zbm_coordinator->friendly_name) {
        free(g_zbm_coordinator->friendly_name);
        g_zbm_coordinator->friendly_name = NULL;
    }
    if (new_coord->friendly_name) {
        g_zbm_coordinator->friendly_name = strdup(new_coord->friendly_name);
    }

    // PAN ID
    g_zbm_coordinator->zb_pan_id = new_coord->zb_pan_id;

    // Radio Channel
    g_zbm_coordinator->zb_radio_channel = new_coord->zb_radio_channel;

    // IEEE Address
    //memcpy(g_zbm_coordinator->zb_ieee_addr, new_coord->zb_ieee_addr, 8);

    // Extended PAN ID
    //memcpy(g_zbm_coordinator->zb_extended_pan_id, new_coord->zb_extended_pan_id, 8);

    // Endpoint — нужно освободить старые списки кластеров
    /*if (g_zbm_coordinator->zb_endpoint.inputClusterList) {
        free(g_zbm_coordinator->zb_endpoint.inputClusterList);
    }
    if (g_zbm_coordinator->zb_endpoint.outputClusterList) {
        free(g_zbm_coordinator->zb_endpoint.outputClusterList);
    }

    // Копируем endpoint
    g_zbm_coordinator->zb_endpoint.endpoint = new_coord->zb_endpoint.endpoint;
    g_zbm_coordinator->zb_endpoint.profileId = new_coord->zb_endpoint.profileId;
    g_zbm_coordinator->zb_endpoint.deviceId = new_coord->zb_endpoint.deviceId;
    g_zbm_coordinator->zb_endpoint.appFlags = new_coord->zb_endpoint.appFlags;

    // Input clusters
    g_zbm_coordinator->zb_endpoint.inputClusterCount = new_coord->zb_endpoint.inputClusterCount;
    if (new_coord->zb_endpoint.inputClusterList && new_coord->zb_endpoint.inputClusterCount > 0) {
        size_t size = new_coord->zb_endpoint.inputClusterCount * sizeof(uint16_t);
        g_zbm_coordinator->zb_endpoint.inputClusterList = malloc(size);
        if (g_zbm_coordinator->zb_endpoint.inputClusterList) {
            memcpy(g_zbm_coordinator->zb_endpoint.inputClusterList,
                   new_coord->zb_endpoint.inputClusterList, size);
        } else {
            g_zbm_coordinator->zb_endpoint.inputClusterCount = 0;
        }
    } else {
        g_zbm_coordinator->zb_endpoint.inputClusterList = NULL;
    }

    // Output clusters
    g_zbm_coordinator->zb_endpoint.outputClusterCount = new_coord->zb_endpoint.outputClusterCount;
    if (new_coord->zb_endpoint.outputClusterList && new_coord->zb_endpoint.outputClusterCount > 0) {
        size_t size = new_coord->zb_endpoint.outputClusterCount * sizeof(uint16_t);
        g_zbm_coordinator->zb_endpoint.outputClusterList = malloc(size);
        if (g_zbm_coordinator->zb_endpoint.outputClusterList) {
            memcpy(g_zbm_coordinator->zb_endpoint.outputClusterList,
                   new_coord->zb_endpoint.outputClusterList, size);
        } else {
            g_zbm_coordinator->zb_endpoint.outputClusterCount = 0;
        }
    } else {
        g_zbm_coordinator->zb_endpoint.outputClusterList = NULL;
    }*/

     // Удаляем старые SSID/пароли
    if (g_zbm_coordinator->wifi_ap_ssid) {
        free(g_zbm_coordinator->wifi_ap_ssid);
    }
    if (g_zbm_coordinator->wifi_ap_password) {
        free(g_zbm_coordinator->wifi_ap_password);
    }
    if (g_zbm_coordinator->wifi_sta_ssid) {
        free(g_zbm_coordinator->wifi_sta_ssid);
    }
    if (g_zbm_coordinator->wifi_sta_password) {
        free(g_zbm_coordinator->wifi_sta_password);
    }

    // Копируем новые
    g_zbm_coordinator->wifi_mode = new_coord->wifi_mode;

    g_zbm_coordinator->wifi_ap_ssid = new_coord->wifi_ap_ssid ? strdup(new_coord->wifi_ap_ssid) : NULL;
    g_zbm_coordinator->wifi_ap_password = new_coord->wifi_ap_password ? strdup(new_coord->wifi_ap_password) : NULL;
    g_zbm_coordinator->wifi_ap_channel = new_coord->wifi_ap_channel;
    g_zbm_coordinator->wifi_ap_max_connections = new_coord->wifi_ap_max_connections;

    g_zbm_coordinator->wifi_sta_ssid = new_coord->wifi_sta_ssid ? strdup(new_coord->wifi_sta_ssid) : NULL;
    g_zbm_coordinator->wifi_sta_password = new_coord->wifi_sta_password ? strdup(new_coord->wifi_sta_password) : NULL;
    g_zbm_coordinator->wifi_sta_channel = new_coord->wifi_sta_channel;

    g_zbm_coordinator->is_sta_valid = 0; // После настройки — ждём подключения

    // === Hostname ===
    if (g_zbm_coordinator->hostname) {
        free(g_zbm_coordinator->hostname);
        g_zbm_coordinator->hostname = NULL;
    }
    if (new_coord->hostname) {
        ESP_LOGI(TAG, "Updating hostname to: %s", new_coord->hostname);
        g_zbm_coordinator->hostname = strdup(new_coord->hostname);
    }

    // Освобождаем временный объект
    zbm_free_coordinator(new_coord);
    cJSON_Delete(json);

    // === Сохраняем в SPIFFS ===
    if (!zbm_save_coordinator_to_spiffs(g_zbm_coordinator)) {
        ESP_LOGE(TAG, "Failed to save coordinator after update");
        const char* response = "{\"success\": false}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

        return ESP_FAIL;
        // Не фатально — продолжаем отвечать
    }

    ESP_LOGI(TAG, "Coordinator updated successfully");

    vTaskDelay(500 / portTICK_PERIOD_MS);
    // === Возвращаем обновлённый объект (GET-логика) ===
    //return zbm_rest_api_get_coordinator_handler(req);
    // === Отправляем JSON-ответ об успехе ===
    const char* response = "{\"success\": true}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t zbm_rest_api_post_open_close_zigbee_network_handler(httpd_req_t* req)
{
    char *buf = NULL;
    int total_len = req->content_len;
    int cur_len = 0;
    esp_err_t ret = ESP_OK;
    cJSON *root = NULL;
    const char *resp_msg = NULL;

    if (total_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    buf = calloc(1, total_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        ESP_LOGI(TAG, "No memory");
        return ESP_ERR_NO_MEM;
    }

    while (cur_len < total_len) {
        int r = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (r <= 0) {
            if (r == HTTPD_SOCK_ERR_TIMEOUT || r == HTTPD_SOCK_ERR_FAIL) continue;
            ret = ESP_FAIL;
            goto _end;
        }
        cur_len += r;
    }

    root = cJSON_Parse(buf);
    if (!root) {
        resp_msg = "Invalid JSON";
        ret = ESP_ERR_INVALID_ARG;
        goto _end;
    }

    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    if (!cmd || !cJSON_IsString(cmd)) {
        resp_msg = "Missing or invalid 'cmd'";
        ret = ESP_ERR_INVALID_ARG;
        goto _end;
    }

    if (strcmp(cmd->valuestring, "toggle_permit_join") == 0) {
        cJSON *duration_obj = cJSON_GetObjectItem(root, "duration");
        uint8_t duration = duration_obj ? (uint8_t)duration_obj->valuedouble : 60;

        ESP_LOGI(TAG, "Toggle permit join: isZigbeeNetworkOpened=%s, duration=%d",
                isZigbeeNetworkOpened ? "true" : "false", duration);

        if (isZigbeeNetworkOpened == true) {
            ret = zbm_to_ncp_cmd_close_zigbee_network();
            resp_msg = "Permit join close command sent";
            ESP_LOGI(TAG, "✅ Zigbee network closed for joining");
        } else {
            ret = zbm_to_ncp_cmd_open_zigbee_network(duration);
            resp_msg = "Permit join open command sent";
            
            ESP_LOGI(TAG, "✅ Zigbee network opened for joining (duration=%d)", duration);
        }
               
    } else {
        resp_msg = "Unknown command";
        ret = ESP_ERR_NOT_SUPPORTED;
        ESP_LOGW(TAG, "Unknown command: %s", cmd->valuestring);
    }

_end:
{
    // Создаём JSON-ответ
    cJSON *response = cJSON_CreateObject();
    if (ret == ESP_OK) {
        cJSON_AddStringToObject(response, "status", "success");
    } else {
        cJSON_AddStringToObject(response, "status", "error");
    }
    // Если resp_msg не задано — подстрахуемся
    if (!resp_msg) {
        resp_msg = ret == ESP_OK ? "Success" : "Unknown error";
    }
    cJSON_AddStringToObject(response, "message", resp_msg);

    // Генерируем строку
    char *json_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);  // ⚠️ response больше не нужен

    // Устанавливаем заголовки
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (ret == ESP_OK) {
        httpd_resp_set_status(req, "200 OK");
    } else {
        httpd_resp_set_status(req, "500 Internal Server Error");
    }
    ESP_LOGI(TAG,"zbm_rest_api_post_open_close_zigbee_network_handler ret = %d", ret);
    // Отправляем ответ
    if (json_str) {
        httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
        free(json_str);  // Освобождаем строку JSON
    } else {
        // Фолбэк на случай нехватки памяти
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"JSON generation failed\"}");
    }
}

    cJSON_Delete(root);
    free(buf);
    return ESP_OK;
}

// === Обработчик: GET /api/get/zigbee_network/status — статус сети Zigbee ===
esp_err_t zbm_rest_api_get_zigbee_network_status_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/get/zigbee_network/status");

    cJSON* json = cJSON_CreateObject();
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    cJSON_AddBoolToObject(json, "is_open", isZigbeeNetworkOpened);
    cJSON_AddStringToObject(json, "status", isZigbeeNetworkOpened ? "open" : "closed");


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

// === Обработчик: POST /api/zdo/active_endpoint — запрос Active Endpoint ===
esp_err_t zbm_rest_api_post_active_endpoint_handler(httpd_req_t* req) {
    char *buf = NULL;
    int total_len = req->content_len;
    esp_err_t ret = ESP_OK;
    cJSON *root = NULL;

    if (total_len <= 0 || total_len > 1024) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty or large body");
        return ESP_OK;
    }

    buf = calloc(1, total_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_ERR_NO_MEM;
    }

    int received = httpd_req_recv(req, buf, total_len);
    if (received <= 0) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv failed");
        return ESP_OK;
    }
    buf[received] = '\0';

    root = cJSON_Parse(buf);
    if (!root) {
        ret = ESP_ERR_INVALID_ARG;
        goto _end;
    }

    cJSON *short_addr_obj = cJSON_GetObjectItem(root, "short_addr");
    if (!short_addr_obj || !cJSON_IsNumber(short_addr_obj)) {
        ret = ESP_ERR_INVALID_ARG;
        goto _end;
    }
    uint16_t short_addr = (uint16_t)short_addr_obj->valuedouble;

    zbm_dev_t* dev = NULL;
    dev = zbm_find_device_in_devdb_by_short_safe(short_addr);
    if (!dev) {
        ret = ESP_ERR_INVALID_ARG;
        goto _end;
    }

    ESP_LOGI(TAG, "ZDO: Send Active Endpoint Request to 0x%04X", dev->short_addr);

    ret = zbm_to_ncp_req_active_endpoint_req(short_addr, NULL, &dev->short_addr);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Active Endpoint Request sent");
    } else {
        ESP_LOGE(TAG, "❌ Failed to send Active Endpoint Request");
    }

_end:
    cJSON_Delete(root);
    free(buf);

    // Ответ
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", ret == ESP_OK ? "success" : "error");
    cJSON_AddStringToObject(resp, "message", ret == ESP_OK ? "Command sent" : "Send failed");

    char *json_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    if (ret == ESP_OK) {
        httpd_resp_set_status(req, "200 OK");
    } else {
        httpd_resp_set_status(req, "500 Internal Server Error");
    }
    if (json_str) {
        httpd_resp_send(req, json_str, strlen(json_str));
        free(json_str);
    } else {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"JSON alloc failed\"}");
    }

    return ESP_OK;
}

// === Обработчик: POST /api/zdo/simple_desc — запрос Simple Descriptor ===
esp_err_t zbm_rest_api_post_simple_descriptor_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/zdo/simple_desc");

    char *buf = NULL;
    int total_len = req->content_len;
    esp_err_t ret = ESP_OK;
    cJSON *root = NULL;

    if (total_len <= 0 || total_len > 512) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty or large body");
        return ESP_OK;
    }

    buf = calloc(1, total_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_ERR_NO_MEM;
    }

    int received = httpd_req_recv(req, buf, total_len);
    if (received <= 0) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv failed");
        return ESP_OK;
    }
    buf[received] = '\0';

    root = cJSON_Parse(buf);
    if (!root) {
        ret = ESP_ERR_INVALID_ARG;
        goto _end;
    }

    // Извлекаем short_addr
    cJSON *short_obj = cJSON_GetObjectItem(root, "short_addr");
    if (!short_obj || !cJSON_IsNumber(short_obj)) {
        ret = ESP_ERR_INVALID_ARG;
        goto _end;
    }
    uint16_t short_addr = (uint16_t)short_obj->valuedouble;

    // Извлекаем endpoint_id
    cJSON *ep_obj = cJSON_GetObjectItem(root, "endpoint_id");
    if (!ep_obj || !cJSON_IsNumber(ep_obj)) {
        ret = ESP_ERR_INVALID_ARG;
        goto _end;
    }
    uint8_t endpoint_id = (uint8_t)ep_obj->valuedouble;

    // Проверим, существует ли устройство
    zbm_dev_t* dev = zbm_find_device_in_devdb_by_short_safe(short_addr);
    if (!dev) {
        ESP_LOGW(TAG, "Device not found for 0x%04X", short_addr);
        ret = ESP_ERR_NOT_FOUND;
        goto _end;
    }

    // Отправляем ZDO Simple Descriptor Request
    ESP_LOGI(TAG, "ZDO: Send Simple Descriptor Request to 0x%04X, EP=%d", short_addr, endpoint_id);

    ret = zbm_to_ncp_req_simple_desc_req(
        short_addr,
        endpoint_id,
        NULL,                           // user_cb
        &dev->short_addr     // user_ctx
    );

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Simple Descriptor Request sent to 0x%04X, EP=%d", short_addr, endpoint_id);
    } else {
        ESP_LOGE(TAG, "❌ Failed to send Simple Descriptor Request");
    }

_end:
    cJSON_Delete(root);
    free(buf);

    // Формируем JSON-ответ
    cJSON *resp = cJSON_CreateObject();
    if (ret == ESP_OK) {
        cJSON_AddStringToObject(resp, "status", "success");
        cJSON_AddStringToObject(resp, "message", "Simple Descriptor request sent");
    } else {
        const char* msg = "Send failed";
        if (ret == ESP_ERR_NOT_FOUND) {
            msg = "Device not found";
        } else if (ret == ESP_ERR_INVALID_ARG) {
            msg = "Invalid parameters";
        }
        cJSON_AddStringToObject(resp, "status", "error");
        cJSON_AddStringToObject(resp, "message", msg);
    }

    char *json_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (ret == ESP_OK) {
        httpd_resp_set_status(req, "200 OK");
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
    }

    if (json_str) {
        httpd_resp_send(req, json_str, strlen(json_str));
        free(json_str);
    } else {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"JSON alloc failed\"}");
    }

    return ESP_OK;
}

// === Обработчик: POST /api/device/update_friendly_name ===
esp_err_t zbm_rest_api_post_update_dev_friendly_name_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "REQ /api/device/update_friendly_name");

    char *buf = NULL;
    int total_len = req->content_len;
    esp_err_t ret = ESP_OK;
    cJSON *root = NULL;

    if (total_len <= 0 || total_len > 512) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty or large body");
        return ESP_OK;
    }

    buf = calloc(1, total_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_ERR_NO_MEM;
    }

    int received = httpd_req_recv(req, buf, total_len);
    if (received <= 0) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv failed");
        return ESP_OK;
    }
    buf[received] = '\0';

    root = cJSON_Parse(buf);
    if (!root) {
        ret = ESP_ERR_INVALID_ARG;
        goto _end;
    }

    // Извлекаем IEEE и новое имя
    cJSON *ieee_obj = cJSON_GetObjectItem(root, "ieee_addr");
    cJSON *name_obj = cJSON_GetObjectItem(root, "friendly_name");

    if (!ieee_obj || !cJSON_IsString(ieee_obj)) {
        ret = ESP_ERR_INVALID_ARG;
        goto _end;
    }
    if (!name_obj || !cJSON_IsString(name_obj)) {
        ret = ESP_ERR_INVALID_ARG;
        goto _end;
    }

    const char* ieee_str = ieee_obj->valuestring;
    const char* new_name = name_obj->valuestring;

    // Парсим IEEE
    uint8_t ieee_addr[8];
    int count = sscanf(ieee_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                       &ieee_addr[0], &ieee_addr[1], &ieee_addr[2], &ieee_addr[3],
                       &ieee_addr[4], &ieee_addr[5], &ieee_addr[6], &ieee_addr[7]);
    if (count != 8) {
        ret = ESP_ERR_INVALID_ARG;
        goto _end;
    }

    // Найдём устройство
    zbm_dev_t* dev = zbm_find_device_in_devdb_by_ieee_safe(ieee_addr);
    if (!dev) {
        ret = ESP_ERR_NOT_FOUND;
        goto _end;
    }

    // Обновим имя
    if (dev->friendly_name) {
        free(dev->friendly_name);
        dev->friendly_name = NULL;
    }
    if (new_name && strlen(new_name) > 0) {
        dev->friendly_name = strdup(new_name);
    } else {
        dev->friendly_name = NULL; // можно оставить пустым
    }

    // Сохраним в SPIFFS
    if (!zbm_save_device_to_spiffs_safe(dev)) {
        ESP_LOGE(TAG, "Failed to save device after friendly_name update");
        ret = ESP_FAIL;
        goto _end;
    }

    // Отправим уведомление: устройство переименовано
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "ieee_addr", ieee_str);
    cJSON_AddStringToObject(data, "friendly_name", dev->friendly_name ? dev->friendly_name : "");
    zbm_ws_send_sys_notify("device_renamed", "Device renamed", data);

    ESP_LOGI(TAG, "✅ Device %s renamed to '%s'", ieee_str, dev->friendly_name ? dev->friendly_name : "(no name)");

_end:
    cJSON_Delete(root);
    free(buf);

    // Ответ клиенту
    cJSON *resp = cJSON_CreateObject();
    if (ret == ESP_OK) {
        cJSON_AddStringToObject(resp, "status", "success");
        cJSON_AddStringToObject(resp, "message", "Name updated");
    } else {
        const char* msg = "Update failed";
        if (ret == ESP_ERR_NOT_FOUND) msg = "Device not found";
        else if (ret == ESP_ERR_INVALID_ARG) msg = "Invalid parameters";

        cJSON_AddStringToObject(resp, "status", "error");
        cJSON_AddStringToObject(resp, "message", msg);
    }

    char *json_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (ret == ESP_OK) {
        httpd_resp_set_status(req, "200 OK");
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
    }

    if (json_str) {
        httpd_resp_send(req, json_str, strlen(json_str));
        free(json_str);
    } else {
        httpd_resp_sendstr(req, "{\"status\":\"error\",\"message\":\"JSON alloc failed\"}");
    }

    return ESP_OK;
}