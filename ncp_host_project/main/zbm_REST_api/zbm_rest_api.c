#include "zbm_rest_api.h"
#include "zbm_device_db.h"
#include "zbm_dev_types.h"
#include "zbm_core_sync.h"
#include "cJSON.h"
#include "zbm_web_server.h"
#include "esp_log.h"
#include "zbm_coordinator.h"

static const char* TAG = "ZBM_REST_API";

static zbm_coordinator_t* g_zbm_coordinator = &zbm_coordinator;

// === Вспомогательная функция: форматирование IEEE-адреса в строку ===
/*static void format_ieee_addr(char* buf, size_t size, const uint8_t* ieee_addr) {
    snprintf(buf, size, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             ieee_addr[0], ieee_addr[1], ieee_addr[2], ieee_addr[3],
             ieee_addr[4], ieee_addr[5], ieee_addr[6], ieee_addr[7]);
}*/





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