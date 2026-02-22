#include "web_server.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "cJSON.h"
#include "zb_manager_devices.h"
#include "zb_manager.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdlib.h>
#include "wifi_manager.h"
#include "ha_integration.h"
#include "ha_rest_api.h"
#include "ha_mqtt_publisher.h"
#include "zb_manager_main_config.h"
#include "zb_manager_automation.h"
#include "zb_manager_rules.h"
#include "zb_manager_rule_editor_api.h"
#include "zbm_dev_base_utils.h"

static const char *TAG = "WEB_SERVER";

static char json_print_buffer[4096]; // буфер для cJSON

static const uint16_t coordinator_output_clusters[] = {
    ESP_ZB_ZCL_CLUSTER_ID_OTA_UPGRADE,   // 0x0019
    ESP_ZB_ZCL_CLUSTER_ID_TIME,          // 0x000A
    ESP_ZB_ZCL_CLUSTER_ID_POLL_CONTROL,  // 0x0021
    ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, // 0x0402
    ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, // 0x0405
};

#define COORD_OUTPUT_COUNT (sizeof(coordinator_output_clusters) / sizeof(coordinator_output_clusters[0]))

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

// Обработчик статических файлов
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
    int len = snprintf(filepath, sizeof(filepath), "%s%s", SPIFFS_UI_MOUNT_POINT, req->uri);
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




// === Настройки ===
//#define INDEX_HTML_PATH "/spiffs/index.html"
static char index_html_buffer[8192]; // буфер для index.html

// === Глобальные переменные ===
httpd_handle_t server_handle = NULL;
static int ws_client_fd = -1;

// === Инициализация HTML-страницы из SPIFFS ===
static void load_index_html(void)
{
    /*esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs_zb_base",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS: %s", esp_err_to_name(ret));
        return;
    }*/

    // Проверим, что файловая система доступна
    DIR *dir = opendir(SPIFFS_UI_MOUNT_POINT);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open /spiffs directory");
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI(TAG, "File in SPIFFS: %s", entry->d_name);
    }
    closedir(dir);

    struct stat st;
    if (stat(ZB_MANAGER_WEB_SERVER_HOME_PAGE, &st) != 0) {
        ESP_LOGE(TAG, "index.html not found at %s", ZB_MANAGER_WEB_SERVER_HOME_PAGE);
        return;
    }

    FILE *f = fopen(ZB_MANAGER_WEB_SERVER_HOME_PAGE, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open index.html");
        return;
    }

    size_t read = fread(index_html_buffer, 1, sizeof(index_html_buffer) - 1, f);
    index_html_buffer[read] = '\0';
    fclose(f);

    ESP_LOGI(TAG, "Loaded index.html (%d bytes)", read);
}

// === Асинхронная отправка через WebSocket ===
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
    uint16_t short_addr;
    bool state;
};

static void ws_async_send(void *arg)
{
    struct async_resp_arg *resp_arg = (struct async_resp_arg *)arg;
    httpd_handle_t hd = resp_arg->hd;

    // Формат: {"event":"state_update","short":1234,"state":true}
    char buff[128];
    snprintf(buff, sizeof(buff),
             "{\"event\":\"state_update\",\"short\":%d,\"online\":%s}",
             resp_arg->short_addr, resp_arg->state ? "true" : "false");

    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t*)buff,
        .len = strlen(buff)
    };

    size_t fd_count = CONFIG_LWIP_MAX_LISTENING_TCP;
    int client_fds[CONFIG_LWIP_MAX_LISTENING_TCP];
    esp_err_t ret = httpd_get_client_list(hd, &fd_count, client_fds);
    if (ret != ESP_OK) {
        free(resp_arg);
        return;
    }

    for (size_t i = 0; i < fd_count; i++) {
        int client_info = httpd_ws_get_fd_info(hd, client_fds[i]);
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(hd, client_fds[i], &ws_pkt);
        }
    }

    free(resp_arg);
}

// === Отправка состояния (вызывается извне) ===
// === Отправка пока сделана из zb_manager_update_device_activity (zb_manager_devices.c)
void ws_notify_state_changed(uint16_t short_addr, bool state)
{
    if(s_is_in_ap_only_mode == true) return; // значит режим настройки SSID репортить ничего не надо на websocket
    if (server_handle == NULL) return;

    struct async_resp_arg *arg = malloc(sizeof(struct async_resp_arg));
    if (!arg) return;

    arg->hd = server_handle;
    arg->short_addr = short_addr;
    arg->state = state;

    httpd_queue_work(server_handle, ws_async_send, arg);
}


// Структура для асинхронной отправки
typedef struct {
    httpd_handle_t hd;
    char *payload;
    size_t len;
} ws_async_data_t;

// Функция отправки (та же, что и раньше)
void ws_send_async_task(void *arg)
{
    ws_async_data_t *data = (ws_async_data_t *)arg;
    httpd_ws_frame_t frame = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)data->payload,
        .len = data->len
    };

    size_t fd_count = CONFIG_LWIP_MAX_LISTENING_TCP;
    int client_fds[CONFIG_LWIP_MAX_LISTENING_TCP];

    esp_err_t ret = httpd_get_client_list(data->hd, &fd_count, client_fds);
    if (ret != ESP_OK) {
        //free(data->payload);
        free(data);
        return;
    }

    for (size_t i = 0; i < fd_count; i++) {
        int client_info = httpd_ws_get_fd_info(data->hd, client_fds[i]);
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(data->hd, client_fds[i], &frame);
        }
    }

    //free(data->payload);
    free(data);
}


esp_err_t wifi_config_form_handler(httpd_req_t *req);

// === Обработчик GET / ===
esp_err_t get_req_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "REQ get_req_handler (uri get): %s", req->uri); 

    // если AP режим, то отправляем форму настройки
    if (s_is_in_ap_only_mode == true) {
        return wifi_config_form_handler(req);
    }

    // Читаем index.html из SPIFFS при каждом запросе
    struct stat st;
    if (stat(ZB_MANAGER_WEB_SERVER_HOME_PAGE, &st) != 0 || S_ISDIR(st.st_mode)) {
        ESP_LOGE(TAG, "index.html not found at %s", ZB_MANAGER_WEB_SERVER_HOME_PAGE);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "index.html not found");
        return ESP_FAIL;
    }

    FILE *f = fopen(ZB_MANAGER_WEB_SERVER_HOME_PAGE, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open index.html");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Cannot open index.html");
        return ESP_FAIL;
    }

    // Отправляем по частям
    char chunk[1024];
    size_t read_len;

    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
ESP_LOGI(TAG, "wifi_config_form_handler send chunk index.html:"); 
    while ((read_len = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, read_len) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0); // завершаем
    ESP_LOGI(TAG, "wifi_config_form_handler send chunk index.html compliete:"); 
    return ESP_OK;
}

esp_err_t get_req_handler_old(httpd_req_t *req)
{
    ESP_LOGI(TAG, "REQ get_req_handler (uri get): %s", req->uri); 
    if (strlen(index_html_buffer) == 0) {
        ESP_LOGE(TAG, "❌ index_html_buffer is empty! UI not loaded.");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "UI not loaded");
        return ESP_FAIL;
    }
    // если AP режим, то отправляем форму настройки
    if (s_is_in_ap_only_mode == true) {
        return wifi_config_form_handler(req);
    }
    // Можно вставить состояние (опционально)
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html_buffer, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

cJSON* create_device_json(device_custom_t *dev)
{
    if (!dev) return NULL;

    // Получаем ПОЛНЫЙ JSON устройства
    cJSON *full_dev = zbm_dev_base_device_to_json(dev);
    //cJSON *full_dev = zb_manager_get_device_json_safe(dev);
    if (!full_dev) return NULL;

    // Добавляем event для фронтенда
    cJSON_AddStringToObject(full_dev, "event", "device_update");

    return full_dev;
}
// Функция для создания JSON-объекта устройства
// === Обновлённая и улучшенная версия create_device_json ===
cJSON* create_device_json_old(device_custom_t *dev)
{
    if (!dev) return NULL;

    cJSON *d = cJSON_CreateObject();
    if (!d) return NULL;

    // Основные поля
    cJSON_AddNumberToObject(d, "short", dev->short_addr);
    cJSON_AddStringToObject(d, "name", dev->friendly_name[0] ? dev->friendly_name : "unknown");
    cJSON_AddBoolToObject(d, "online", dev->is_online);

    // Производитель и модель
    if (dev->server_BasicClusterObj) {
        if (dev->server_BasicClusterObj->manufacturer_name[0]) {
            cJSON_AddStringToObject(d, "manufacturer_name", dev->server_BasicClusterObj->manufacturer_name);
        }
        if (dev->server_BasicClusterObj->model_identifier[0]) {
            cJSON_AddStringToObject(d, "model_id", dev->server_BasicClusterObj->model_identifier);
        }
    }

    // Батарея (если есть)
    if (dev->is_online && dev->server_PowerConfigurationClusterObj) {
        uint8_t voltage_raw = dev->server_PowerConfigurationClusterObj->battery_voltage;
        if (voltage_raw != 0xFF) {
            float voltage = voltage_raw * 0.1f;
            uint8_t percentage_raw = dev->server_PowerConfigurationClusterObj->battery_percentage;
            int percentage = (percentage_raw != 0xFF) ? (percentage_raw / 2) : -1;

            cJSON *battery = cJSON_CreateObject();
            cJSON_AddNumberToObject(battery, "voltage", voltage);
            if (percentage >= 0 && percentage <= 100) {
                cJSON_AddNumberToObject(battery, "percent", percentage);
                char percent_disp[16];
                snprintf(percent_disp, sizeof(percent_disp), "%d%%", percentage);
                cJSON_AddStringToObject(battery, "percent_display", percent_disp);
            }

            char volt_disp[32];
            snprintf(volt_disp, sizeof(volt_disp), "%.1f В", voltage);
            cJSON_AddStringToObject(battery, "display", volt_disp);
            cJSON_AddItemToObject(d, "battery", battery);
        }
    }

    // Массив кластеров
    cJSON *clusters = cJSON_CreateArray();
    if (!clusters) {
        cJSON_Delete(d);
        return NULL;
    }

    for (int ep_idx = 0; ep_idx < dev->endpoints_count; ep_idx++) {
        endpoint_custom_t *ep = dev->endpoints_array[ep_idx];
        if (!ep) continue;

        const char* ep_friendly_name = ep->friendly_name[0] ? ep->friendly_name : NULL;
        char default_ep_name[32];
        if (!ep_friendly_name) {
            snprintf(default_ep_name, sizeof(default_ep_name), "[0x%04x] [0x%02x]", dev->short_addr, ep->ep_id);
            ep_friendly_name = default_ep_name;
        }

        // 🔌 On/Off Cluster
        if (ep->is_use_on_off_cluster && ep->server_OnOffClusterObj) {
            cJSON *cl = cJSON_CreateObject();
            cJSON_AddStringToObject(cl, "type", "on_off");
            cJSON_AddNumberToObject(cl, "endpoint_id", ep->ep_id);
            cJSON_AddStringToObject(cl, "endpoint_name", ep_friendly_name);

            if (dev->is_online) {
                bool on = ep->server_OnOffClusterObj->on_off;
                cJSON_AddBoolToObject(cl, "value", on);
                cJSON_AddStringToObject(cl, "display", on ? "ON" : "OFF");
                cJSON_AddStringToObject(cl, "unit", "");
            } else {
                cJSON_AddStringToObject(cl, "display", "Offline");
                cJSON_AddStringToObject(cl, "unit", "");
            }
            cJSON_AddItemToArray(clusters, cl);
        }

        // 🌡️ Temperature Cluster
        if (ep->is_use_temperature_measurement_cluster && ep->server_TemperatureMeasurementClusterObj) {
            cJSON *cl = cJSON_CreateObject();
            cJSON_AddStringToObject(cl, "type", "temperature");
            cJSON_AddNumberToObject(cl, "endpoint_id", ep->ep_id);
            cJSON_AddStringToObject(cl, "endpoint_name", ep_friendly_name);

            if (dev->is_online) {
                int16_t raw = ep->server_TemperatureMeasurementClusterObj->measured_value;
                float temp = raw / 100.0f;
                char disp[32];
                snprintf(disp, sizeof(disp), "%.1f °C", temp);
                cJSON_AddNumberToObject(cl, "value", temp);
                cJSON_AddStringToObject(cl, "display", disp);
                cJSON_AddStringToObject(cl, "unit", "°C");
            } else {
                cJSON_AddStringToObject(cl, "display", "Offline");
                cJSON_AddStringToObject(cl, "unit", "°C");
            }
            cJSON_AddItemToArray(clusters, cl);
        }

        // 💧 Humidity Cluster
        if (ep->is_use_humidity_measurement_cluster && ep->server_HumidityMeasurementClusterObj) {
            cJSON *cl = cJSON_CreateObject();
            cJSON_AddStringToObject(cl, "type", "humidity");
            cJSON_AddNumberToObject(cl, "endpoint_id", ep->ep_id);
            cJSON_AddStringToObject(cl, "endpoint_name", ep_friendly_name);

            if (dev->is_online) {
                uint16_t raw = ep->server_HumidityMeasurementClusterObj->measured_value;
                float hum = raw / 100.0f;
                char disp[32];
                snprintf(disp, sizeof(disp), "%.1f %%", hum);
                cJSON_AddNumberToObject(cl, "value", hum);
                cJSON_AddStringToObject(cl, "display", disp);
                cJSON_AddStringToObject(cl, "unit", "%");
            } else {
                cJSON_AddStringToObject(cl, "display", "Offline");
                cJSON_AddStringToObject(cl, "unit", "%");
            }
            cJSON_AddItemToArray(clusters, cl);
        }
    }

    // Если нет кластеров — добавляем заглушку
    if (cJSON_GetArraySize(clusters) == 0) {
        cJSON *cl = cJSON_CreateObject();
        cJSON_AddStringToObject(cl, "type", "unknown");
        cJSON_AddStringToObject(cl, "display", "No data");
        cJSON_AddStringToObject(cl, "unit", "");
        cJSON_AddNumberToObject(cl, "endpoint_id", 0);
        cJSON_AddStringToObject(cl, "endpoint_name", "Unknown");
        cJSON_AddItemToArray(clusters, cl);
    }

    cJSON_AddItemToObject(d, "clusters", clusters);
    return d;
}







void ws_notify_network_status(void)
{
    if(s_is_in_ap_only_mode == true) return; // режим настройки SSID постить не надо на websocet
    ESP_LOGI(TAG, "🌐 Notify network status");
    if (server_handle == NULL) return;
    ESP_LOGI(TAG, "🌐 Notify network status: server_handle != NULL sending");
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "event", "network_status");
    cJSON_AddStringToObject(data, "wifi_ssid", s_last_ssid);
    cJSON_AddBoolToObject(data, "zigbee_open", isZigbeeNetworkOpened);

    //char *rendered = cJSON_PrintUnformatted(data);
    
    int len = cJSON_PrintPreallocated(data, json_print_buffer, sizeof(json_print_buffer), false);
    cJSON_Minify(json_print_buffer); // опционально — убирает пробелы
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to print JSON into buffer");
        cJSON_Delete(data);
        return;
    }
    char *rendered = json_print_buffer;
    cJSON_Delete(data);

    if (!rendered) return;

    ws_async_data_t *async_data = malloc(sizeof(ws_async_data_t));
    if (!async_data) {
        //free(rendered);
        return;
    }

    async_data->hd = server_handle;
    async_data->payload = rendered;
    async_data->len = strlen(rendered);

    if (httpd_queue_work(server_handle, ws_send_async_task, async_data) != ESP_OK) {
        //free(rendered);
        free(async_data);
    }
}

// web_server.c или ws_server.c
void ws_notify_rules_update(void)
{
    if(s_is_in_ap_only_mode == true) return;
    ESP_LOGI(TAG, "🔄 Notify rules updated");
    if (server_handle == NULL) return;

    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "event", "rules_updated");

    int len = cJSON_PrintPreallocated(msg, json_print_buffer, sizeof(json_print_buffer), false);
    cJSON_Minify(json_print_buffer); // убираем пробелы
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to print JSON into buffer");
        cJSON_Delete(msg);
        return;
    }
    char *rendered = json_print_buffer;
    cJSON_Delete(msg);

    ws_async_data_t *async_data = malloc(sizeof(ws_async_data_t));
    if (!async_data) {
        return;
    }

    async_data->hd = server_handle;
    async_data->payload = rendered;
    async_data->len = strlen(rendered);

    if (httpd_queue_work(server_handle, ws_send_async_task, async_data) != ESP_OK) {
        free(async_data);
    }
}

void ws_notify_endpoint_name_update(device_custom_t *dev, endpoint_custom_t *ep)
{
    if(s_is_in_ap_only_mode == true) return;

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "event", "endpoint_name_updated");
    cJSON_AddNumberToObject(data, "short", dev->short_addr);
    cJSON_AddNumberToObject(data, "endpoint_id", ep->ep_id);
    cJSON_AddStringToObject(data, "name", ep->friendly_name[0] ? ep->friendly_name : "EP");

    int len = cJSON_PrintPreallocated(data, json_print_buffer, sizeof(json_print_buffer), false);
    cJSON_Minify(json_print_buffer);
    if (len < 0) {
        cJSON_Delete(data);
        return;
    }
    char *rendered = json_print_buffer;
    cJSON_Delete(data);

    ws_async_data_t *async_data = malloc(sizeof(ws_async_data_t));
    if (!async_data) return;

    async_data->hd = server_handle;
    async_data->payload = rendered;
    async_data->len = strlen(rendered);

    if (httpd_queue_work(server_handle, ws_send_async_task, async_data) != ESP_OK) {
        free(async_data);
    }
}


// === Обработчик WebSocket /ws ===
esp_err_t ws_handler(httpd_req_t *req)
{
    
   if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake requested");
        // Не сохраняй fd — httpd сам управляет
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = { .type = HTTPD_WS_TYPE_TEXT };
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0); // получить длину
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

    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS recv failed (payload): %s", esp_err_to_name(ret));
        free(buf);
        return ret;
    }

    ESP_LOGI(TAG, "WS RX: %.*s", ws_pkt.len, buf);
    cJSON *req_json = cJSON_Parse((char*)buf);
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Загрузка устройств изменена
    if (req_json) {
        cJSON *cmd = cJSON_GetObjectItem(req_json, "cmd");
        /*if (cmd && strcmp(cmd->valuestring, "get_devices") == 0) {
            ESP_LOGI(TAG, "CMD: get_devices received");

            cJSON *devices = cJSON_CreateArray();
            cJSON *response = cJSON_CreateObject();

            if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
                for (int i = 0; i < RemoteDevicesCount; i++) {
                    device_custom_t *dev = RemoteDevicesArray[i];
                    if (!dev) continue;

                    cJSON *d = create_device_json(dev);
                    if (d) {
                        cJSON_AddItemToArray(devices, d);
                    }
                }
                DEVICE_ARRAY_UNLOCK();
            } else {
                ESP_LOGE(TAG, "Failed to take mutex in get_devices");
            }

            cJSON_AddItemToObject(response, "devices", devices);
            //char *rendered = cJSON_PrintUnformatted(response);
            
            int len = cJSON_PrintPreallocated(response, json_print_buffer, sizeof(json_print_buffer), false);
            cJSON_Minify(json_print_buffer); // опционально — убирает пробелы
            if (len < 0) {
                ESP_LOGE(TAG, "Failed to print JSON into buffer");
                cJSON_Delete(response);
                return ESP_FAIL;
            }
            char *rendered = json_print_buffer;
            cJSON_Delete(response);

            httpd_ws_frame_t resp = {
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t*)rendered,
                .len = strlen(rendered)
            };
            httpd_ws_send_frame(req, &resp);
            //free(rendered);
        } */ // end get_devices

 ///////////
        if (strcmp(cmd->valuestring, "send_automation_command") == 0) {
            cJSON *short_addr = cJSON_GetObjectItem(req_json, "short_addr");
            cJSON *endpoint = cJSON_GetObjectItem(req_json, "endpoint");
            cJSON *cmd_id = cJSON_GetObjectItem(req_json, "cmd_id");
            cJSON *params = cJSON_GetObjectItem(req_json, "params");

            if (!short_addr || !endpoint || !cmd_id) {
                ESP_LOGW(TAG, "Missing fields in send_automation_command");
                return ESP_FAIL;
            }

            zb_automation_request_t req = {0};
            req.short_addr = short_addr->valueint;
            req.endpoint_id = endpoint->valueint;
            req.cmd_id = cmd_id->valueint;

            // Парсим параметры
            esp_err_t parse_result = zb_automation_request_from_json(req_json, &req);
            if (parse_result != ESP_OK) {
                ESP_LOGE(TAG, "Failed to parse automation request");
                return ESP_FAIL;
            }

            // Отправляем
            esp_err_t send_result = zb_automation_send_command(&req);
            if (send_result == ESP_OK) {
                ESP_LOGI(TAG, "✅ Automation command sent: %d", req.cmd_id);
            } else {
                ESP_LOGE(TAG, "❌ Failed to send automation command: %s", esp_err_to_name(send_result));
            }
        }
        else if (cmd && strcmp(cmd->valuestring, "toggle") == 0) {
                cJSON *short_addr = cJSON_GetObjectItem(req_json, "short");
                cJSON *ep_id_obj = cJSON_GetObjectItem(req_json, "endpoint");
                if (short_addr && ep_id_obj) {
                    uint16_t addr = short_addr->valueint;
                    uint8_t ep_id = ep_id_obj->valueint;

                    ESP_LOGI(TAG, "Toggle request for device: 0x%04x, endpoint: %d", addr, ep_id);

                    device_custom_t *dev = zbm_dev_base_find_device_by_short_safe(addr);
                    if (!dev) {
                        ESP_LOGW(TAG, "Device 0x%04x not found", addr);
                        return ESP_FAIL;
                    }

                    // Поиск нужного endpoint'а
                    endpoint_custom_t *target_ep = NULL;
                    for (int i = 0; i < dev->endpoints_count; i++) {
                        if (dev->endpoints_array[i] && dev->endpoints_array[i]->ep_id == ep_id) {
                            target_ep = dev->endpoints_array[i];
                            break;
                        }
                    }

                    if (!target_ep) {
                        ESP_LOGW(TAG, "Endpoint %d not found on device 0x%04x", ep_id, addr);
                        return ESP_FAIL;
                    }

                    if (!target_ep->is_use_on_off_cluster) {
                        ESP_LOGW(TAG, "Endpoint %d has no OnOff cluster", ep_id);
                        return ESP_FAIL;
                    }

                    // Формируем и отправляем команду Toggle
                    esp_zb_zcl_on_off_cmd_t on_off_cmd = {
                        .zcl_basic_cmd = {
                            .dst_addr_u.addr_short = dev->short_addr,
                            .dst_endpoint = target_ep->ep_id,
                            .src_endpoint = 1,
                        },
                        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                        //.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT,
                        .on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID,
                    };
                   // memcpy(&on_off_cmd.zcl_basic_cmd.dst_addr_u.addr_long, dev->ieee_addr, sizeof(esp_zb_ieee_addr_t));

                    //esp_zb_zcl_on_off_cmd_req(&on_off_cmd);
                    zb_manager_on_off_cmd_req(&on_off_cmd);
                    ESP_LOGI(TAG, "✅ Sent Toggle to 0x%04x (ep: %d)", dev->short_addr, target_ep->ep_id);
                    //vTaskDelay(100 / portTICK_PERIOD_MS);
                    // Ждём, чтобы устройство успело ответить
                    vTaskDelay(pdMS_TO_TICKS(300));

                    // 🔥 Принудительно обновляем состояние в MQTT но позже автообновление из worker
                    ha_mqtt_publish_on_off_state(dev, target_ep);

                    // 🔹 Только для Lumi/Aqara: отправляем Read Attribute (известная проблема — не присылают report после toggle)
                    /*if (dev->manufacturer_code == 4447) {
                        ESP_LOGI(TAG, "🔍 Device is Lumi/Aqara (manuf: %d) → forcing ReadAttr to sync state", dev->manufacturer_code);
                        uint8_t tsn = zb_manager_read_on_off_attribute(dev->short_addr, target_ep->ep_id);
                        if (tsn == 0xff) {
                            ESP_LOGW(TAG, "❌ Failed to send Read Attribute for OnOff to 0x%04x (ep: %d)", dev->short_addr, target_ep->ep_id);
                        } else {
                            ESP_LOGI(TAG, "✅ Read Attribute (OnOff) sent with TSN=%d", tsn);
                        }
                    } else {
                        ESP_LOGD(TAG, "💡 Device manuf: %d → skipping ReadAttr, relying on reporting", dev->manufacturer_code);
                    }*/
                } else {
                    ESP_LOGW(TAG, "❌ Missing 'short' or 'endpoint' in toggle command");
                }
            } // end toggle

        else if (strcmp(cmd->valuestring, "update_friendly_name") == 0) {
            uint16_t short_addr = cJSON_GetObjectItem(req_json, "short")->valueint;
            const char* name = cJSON_GetObjectItem(req_json, "name")->valuestring;
            ESP_LOGD(TAG, "Updating device name for device 0x%04x", short_addr);
            if (zbm_dev_base_dev_update_friendly_name(short_addr, name) == ESP_OK)
            {
                if (zbm_dev_base_queue_save_req_cmd() == ESP_OK)
                {
                    // ✅ Обновляем discovery в HA
                    //ha_mqtt_republish_discovery_for_device(dev);
                    // ✅ Отправляем ОБЩЕЕ обновление устройства
                    ws_notify_device_update(short_addr);
                }
            }
        } // end update_friendly_name
        
        else if (strcmp(cmd->valuestring, "update_endpoint_name") == 0) {
            uint16_t short_addr = cJSON_GetObjectItem(req_json, "short")->valueint;
            uint8_t endpoint_id = cJSON_GetObjectItem(req_json, "endpoint")->valueint;
            const char* name = cJSON_GetObjectItem(req_json, "name")->valuestring;
            ESP_LOGD(TAG, "Updating endpoint name for device 0x%04x, endpoint %d", short_addr, endpoint_id);
            if (zbm_dev_base_endpoint_update_friendly_name(short_addr, endpoint_id, name) == ESP_OK)
            {
                if (zbm_dev_base_queue_save_req_cmd() == ESP_OK)
                {
                    // ✅ Обновляем discovery в HA
                    //ha_mqtt_republish_discovery_for_device(dev);
                    // ✅ Отправляем ОБЩЕЕ обновление устройства
                    ws_notify_device_update(short_addr);
                }
            }
        }
        else if (strcmp(cmd->valuestring, "get_network_status") == 0) {
            cJSON *response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "event", "network_status");

            // Имя Wi-Fi — глобальная переменная (s_last_ssid)
            cJSON_AddStringToObject(response, "wifi_ssid", s_last_ssid);

            // Состояние сети — глобальная (например, s_zigbee_network_open)
            cJSON_AddBoolToObject(response, "zigbee_open", isZigbeeNetworkOpened);

            //char *rendered = cJSON_PrintUnformatted(response);
            //cJSON_Minify(response); // опционально — убирает пробелы
            int len = cJSON_PrintPreallocated(response, json_print_buffer, sizeof(json_print_buffer), false);
            cJSON_Minify(json_print_buffer); // опционально — убирает пробелы
            if (len < 0) {
                ESP_LOGE(TAG, "Failed to print JSON into buffer");
                cJSON_Delete(response);
                return ESP_FAIL;
            }
            char *rendered = json_print_buffer;
            cJSON_Delete(response);

            httpd_ws_frame_t frame = {
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t*)rendered,
                .len = strlen(rendered)
            };
            httpd_ws_send_frame(req, &frame);
            //free(rendered);
        } // end get_network_status
        else if (strcmp(cmd->valuestring, "toggle_network") == 0) {
            int duration = 60;
            cJSON *duration_obj = cJSON_GetObjectItem(req_json, "duration");
            if (duration_obj) {
                duration = duration_obj->valueint;
            }

            if (isZigbeeNetworkOpened) {
                zb_manager_close_network();
                //s_zigbee_network_open = false;
            } else {
                zb_manager_open_network(duration);
                //s_zigbee_network_open = true;
            }

            // ✅ Отправим обновление всем клиентам
            // (реализуем через ws_notify_network_status())
            // здесь отправлять рано ответ будет отправляться из  zb_manager_action_handler_worker.c
            //ws_notify_network_status();
        }
    }
    cJSON_Delete(req_json);
    free(buf);
    return ESP_OK;
}

// Основная функция: обновить всё устройство
// === Основная функция: обновить всё устройство ===
#define DEBOUNCE_TIMEOUT_MS 200  // не чаще раза в 500 мс на устройство

static uint32_t last_update_time[256] = {0};  // хэш от short_addr

// 🔹 Версия для использования ВНУТРИ мьютекса (не берёт мьютекс!)
void ws_notify_device_update_unlocked(device_custom_t *dev)
{
    //if(s_is_in_ap_only_mode == true) return; // режим настройки SSID постить не надо на websocket
    uint32_t now = esp_log_timestamp();
    uint8_t idx = dev->short_addr % 256;

    // 🔹 Проверим: изменилось ли состояние On/Off?
    bool current_on_off = false;
    bool has_on_off = false;

    for (int i = 0; i < dev->endpoints_count; i++) {
        endpoint_custom_t *ep = dev->endpoints_array[i];
        if (ep && ep->is_use_on_off_cluster && ep->server_OnOffClusterObj) {
            current_on_off = ep->server_OnOffClusterObj->on_off;
            has_on_off = true;
            break;
        }
    }

    // ✅ Если значение изменилось — отправляем, даже если дебаунс
    static bool last_on_off_state[256] = {0};
    static bool state_known[256] = {0};
    
    // всегда отправляем теперь
    goto send_update;
    /*if (has_on_off && state_known[idx]) {
        if (last_on_off_state[idx] != current_on_off) {
            goto send_update; // → пропускаем дебаунс
        }
    }

    // 🔹 Обычный дебаунс
    if (now - last_update_time[idx] < DEBOUNCE_TIMEOUT_MS) {
        ESP_LOGD(TAG, "ws_notify_device_update: skipped 0x%04x — debounced", dev->short_addr);
        goto update_cache;
    }*/

send_update:
    {
        ESP_LOGI(TAG, "ws_notify_device_update_unlocked - send_update");
        //cJSON *device_json = create_device_json(dev);
        cJSON *device_json = NULL;
        device_json = zbm_dev_base_device_to_json(dev);
        if (!device_json) return;

        cJSON_AddStringToObject(device_json, "event", "device_update");
        //char *rendered = cJSON_PrintUnformatted(device_json);
        //cJSON_Minify(device_json); // опционально — убирает пробелы
            int len = cJSON_PrintPreallocated(device_json, json_print_buffer, sizeof(json_print_buffer), false);
            cJSON_Minify(json_print_buffer); // опционально — убирает пробелы
            if (len < 0) {
                ESP_LOGE(TAG, "Failed to print JSON into buffer");
                cJSON_Delete(device_json);
                return;
            }
        char *rendered = json_print_buffer;
        cJSON_Delete(device_json);
        if (!rendered) return;

        last_update_time[idx] = now;

        ws_async_data_t *data = malloc(sizeof(ws_async_data_t));
        if (!data) {
            //free(rendered);
            goto update_cache;
        }

        data->hd = server_handle;
        data->payload = rendered;
        data->len = strlen(rendered);

        if (httpd_queue_work(server_handle, ws_send_async_task, data) != ESP_OK) {
            // ❌ Очередь переполнена — освобождаем вручную
            //free(rendered);
            free(data);
        }
        // ✅ УСПЕШНО: память будет освобождена в ws_send_async_task
    }

update_cache:
    if (has_on_off) {
        last_on_off_state[idx] = current_on_off;
        state_known[idx] = true;
    }
    
}



// 🔹 Версия для использования ВНУТРИ мьютекса (не берёт мьютекс!)
void ws_notify_device_update_unlocked_old(device_custom_t *dev)
{
    uint32_t now = esp_log_timestamp();
    uint8_t idx = dev->short_addr % 256;

    // 🔹 Проверим: изменилось ли состояние On/Off?
    bool current_on_off = false;
    bool has_on_off = false;

    for (int i = 0; i < dev->endpoints_count; i++) {
        endpoint_custom_t *ep = dev->endpoints_array[i];
        if (ep && ep->is_use_on_off_cluster && ep->server_OnOffClusterObj) {
            current_on_off = ep->server_OnOffClusterObj->on_off;
            has_on_off = true;
            break;
        }
    }

    // ✅ Если значение изменилось — отправляем, даже если дебаунс
    static bool last_on_off_state[256] = {0};
    static bool state_known[256] = {0};

    if (has_on_off && state_known[idx]) {
        if (last_on_off_state[idx] != current_on_off) {
            goto send_update; // → пропускаем дебаунс
        }
    }

    // 🔹 Обычный дебаунс
    if (now - last_update_time[idx] < DEBOUNCE_TIMEOUT_MS) {
        ESP_LOGD(TAG, "ws_notify_device_update: skipped 0x%04x — debounced", dev->short_addr);
        goto update_cache;
    }

send_update:
    {
        cJSON *device_json = create_device_json(dev);
        if (!device_json) return;

        cJSON_AddStringToObject(device_json, "event", "device_update");
        //char *rendered = cJSON_PrintUnformatted(device_json);
        //cJSON_Minify(device_json); // опционально — убирает пробелы
        int len = cJSON_PrintPreallocated(device_json, json_print_buffer, sizeof(json_print_buffer), false);
        cJSON_Minify(json_print_buffer); // опционально — убирает пробелы
        if (len < 0) {
            ESP_LOGE(TAG, "Failed to print JSON into buffer");
            cJSON_Delete(device_json);
            return;
        }
        char *rendered = json_print_buffer;
        cJSON_Delete(device_json);
        if (!rendered) return;

        last_update_time[idx] = now;

        ws_async_data_t *data = malloc(sizeof(ws_async_data_t));
        if (!data) {
            //free(rendered);
            goto update_cache;
        }

        data->hd = server_handle;
        data->payload = rendered;
        data->len = strlen(rendered);

        if (httpd_queue_work(server_handle, ws_send_async_task, data) != ESP_OK) {
            //free(rendered);
            free(data);
        } else {
            ESP_LOGI("WS_NOTIFY", "📤 Sent device_update: %s (0x%04x) → on_off=%s", 
                     dev->friendly_name, dev->short_addr, current_on_off ? "ON" : "OFF");
        }
    }

update_cache:
    if (has_on_off) {
        last_on_off_state[idx] = current_on_off;
        state_known[idx] = true;
    }
}

/**
 * @brief Основная функция для отправки обновления устройства (вызывается извне)
 * Берёт мьютекс, находит устройство, вызывает unlocked-версию
 */
void ws_notify_device_update(uint16_t short_addr)
{
    //return; // ВРЕМЕННО!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if(s_is_in_ap_only_mode == true) return; // режим настройки SSID постиь не надо на websocet
    device_custom_t *dev = zbm_dev_base_find_device_by_short_safe(short_addr);
    if (dev) {
        ws_notify_device_update_unlocked(dev);
    }
}


// Форма для ввода Wi-Fi
esp_err_t wifi_config_form_handler(httpd_req_t *req)
{
    // Формируем HTML с подставленным SSID
    char response[2048]; // достаточно для формы + SSID

    const char* ssid_display = strlen(s_last_ssid) > 0 ? s_last_ssid : "<i>не настроено</i>";
    const char* password_hint = strlen(s_last_ssid) > 0 ? "Оставьте пустым, чтобы не менять" : "";

    int len = snprintf(response, sizeof(response),
        "<!DOCTYPE html>"
        "<html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>Настройка Wi-Fi</title>"
        "<style>"
        "body{font-family:sans-serif;margin:40px;background:#f5f5f5;color:#333;}"
        "form{background:#fff;padding:24px;border-radius:10px;max-width:400px;margin:0 auto;box-shadow:0 4px 12px rgba(0,0,0,0.1);}"
        "h2{text-align:center;color:#1976d2;margin-bottom:24px;font-size:1.4em;}"
        "label{display:block;margin-top:12px;font-weight:500;}"
        "input[type='text'],input[type='password']{width:100%%;padding:10px;margin-top:6px;border:1px solid #ddd;border-radius:6px;font-size:14px;box-sizing:border-box;}"
        "input:focus{outline:none;border-color:#1976d2;box-shadow:0 0 4px rgba(25,118,210,0.5);}"
        ".btn-group{display:flex;gap:10px;margin-top:24px;}"
        "input[type='submit'],button[type='button']{flex:1;padding:12px 24px;border:none;border-radius:6px;cursor:pointer;font-size:16px;font-weight:500;}"
        "input[type='submit']{background:#1976d2;color:white;}"
        "input[type='submit']:hover{background:#1565c0;}"
        "button[type='button']{background:#757575;color:white;}"
        "button[type='button']:hover{background:#616161;}"
        ".status{font-size:14px;color:#666;margin-bottom:20px;text-align:center;}"
        ".icon{font-size:1.8em;vertical-align:middle;margin-right:8px;}"
        "</style>"
        "</head><body>"
        "<form action='/save_wifi' method='post'>"
        "<h2><span class='icon'>🔧</span>Настройка Wi-Fi</h2>"
        "<div class='status'>"
        "Текущая сеть:<br><b>%s</b>"
        "</div>",
        ssid_display);

    // Добавим поля ввода
    strcat(response,
        "<label for='ssid'>Новый SSID</label>"
        "<input type='text' name='ssid' id='ssid'>"
        "<label for='password'>Новый пароль</label>"
        "<input type='password' name='password' id='password'>"
        "<div style='font-size:12px;color:#777;text-align:center;margin:8px 0;'>"
        "Оставьте пустым, чтобы не менять"
        "</div>"
        "<div class='btn-group'>"
        "<input type='submit' value='Сохранить'>"
        "<button type='submit' formaction='/cancel_save_wifi' style='background:#757575;'>Отмена</button>"
        "</div>"
        "</form>"
        "</body></html>");

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


// Обработчик POST /save_wifi
esp_err_t save_wifi_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_500(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Парсим: ssid=MyWiFi&password=12345678
    char ssid[32] = {0}, password[64] = {0};
    char *s = strstr(buf, "ssid=");
    char *p = strstr(buf, "password=");

    if (s) {
        s += 5;
        char *end = strchr(s, '&');
        int len = end ? end - s : strlen(s);
        len = len > 31 ? 31 : len;
        strncpy(ssid, s, len);
    }

    if (p) {
        p += 9;
        int len = strlen(p);
        len = len > 63 ? 63 : len;
        strncpy(password, p, len);
    }

    ESP_LOGI(TAG, "WiFi config received: SSID='%s', Password='%s'", ssid, password);

    // Сохраняем в NVS
    if (save_wifi_config_to_nvs(ssid, password) == ESP_OK) {
        // Обновляем глобальные
        strncpy(s_last_ssid, ssid, 31);
        strncpy(s_last_password, password, 63);

        // Ответ
        const char *resp = "<!DOCTYPE html>"
                            "<html><head>"
                            "<meta charset='utf-8'>"
                            "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                            "<title>Настройка Wi-Fi</title>"                  
                            "</head><body>"
                            "<h3>✅ Сохранено! Перезагружаемся...</h3>"
                            "<script>setTimeout(()=>window.close(),2000);</script>"
                            "</body></html>";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        vTaskDelay(pdMS_TO_TICKS(1500));
        //esp_restart();  // ✅ Перезагрузка

        // Через 3 сек — перезапустим STA
        //wifi_manager_send_command(WIFI_MANAGER_STOP_AP, NULL, NULL);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Save failed");
    }

    return ESP_OK;
}

// отмена изменений wifi
esp_err_t cancel_save_wifi_handler(httpd_req_t *req)
{
    const char *resp = "<!DOCTYPE html>"
                       "<html><head>"
                       "<meta charset='utf-8'>"
                       "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                       "<title>Отмена</title>"
                       "</head><body>"
                       "<h3>❌ Изменения отменены</h3>"
                       "<script>setTimeout(()=>window.close(),1000);</script>"
                       "</body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(1500));
    //wifi_manager_send_command(WIFI_MANAGER_STOP_AP, NULL, NULL);

    wifi_manager_reinit_sta();
    return ESP_OK;
}

esp_err_t memory_api_handler(httpd_req_t *req)
{
    // Получаем данные из кучи (DRAM)
    size_t total_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    size_t min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

    // Вычисляем процент фрагментации (косвенно)
    uint32_t fragmentation_pct = 0;
    if (total_free > 0 && largest_free_block < total_free) {
        fragmentation_pct = 100 - ((largest_free_block * 100) / total_free);
    }

    // Формируем JSON
    httpd_resp_set_type(req, "application/json");
    char response[256];
    int len = snprintf(response, sizeof(response),
        "{"
            "\"free\":%u,"
            "\"min\":%u,"
            "\"largest\":%u,"
            "\"frag_pct\":%u,"
            "\"timestamp\":%lld"
        "}",
        (unsigned)total_free,
        (unsigned)min_free,
        (unsigned)largest_free_block,
        (unsigned)fragmentation_pct,
        esp_timer_get_time() / 1000  // в миллисекундах
    );

    if (len < 0 || len >= sizeof(response)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Response too long");
        return ESP_FAIL;
    }

    httpd_resp_send(req, response, len);
    return ESP_OK;
}


esp_err_t memory_api_handler_old(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    char response[128];
    int free = esp_get_free_heap_size();
    int min = esp_get_minimum_free_heap_size();

    int len = snprintf(response, sizeof(response),
        "{\"free\":%d,\"min\":%d,\"timestamp\":%lld}",
        free, min, esp_timer_get_time() / 1000);

    httpd_resp_send(req, response, len);
    return ESP_OK;

    /*size_t total = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    size_t min = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

    httpd_resp_set_type(req, "application/json");
    char response[256];
    int len = snprintf(response, sizeof(response),"{\"free\":%d,\"min\":%d\"largest\":%d,\"frag_pct\":%lu,\"timestamp\":%lld}",
        (int)total, (int)min, largest,(uint32_t)(100 - (largest * 100.0 / total)), esp_timer_get_time() / 1000);
    httpd_resp_send(req, response, len);
    return ESP_OK;*/
}

//=== Получение устройств для Биндинга и Анбиндинга ===
//LocalIeeeAdr
esp_err_t handle_get_devices_for_binding_api(httpd_req_t *req)
{
    cJSON *devices = cJSON_CreateArray();
    if (!devices) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON alloc failed");
        return ESP_FAIL;
    }

    // 1. Сначала реальные устройства
    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
        for (int i = 0; i < RemoteDevicesCount; i++) {
            device_custom_t *dev = RemoteDevicesArray[i];
            if (!dev) continue;

            cJSON *d = cJSON_CreateObject();
            cJSON_AddNumberToObject(d, "short", dev->short_addr);

            char ieee_str[24];
            ieee_to_str(ieee_str, dev->ieee_addr);
            cJSON_AddStringToObject(d, "ieee", ieee_str);
            cJSON_AddStringToObject(d, "name", dev->friendly_name[0] ? dev->friendly_name : "unknown");

            cJSON *eps = cJSON_CreateArray();
            for (int j = 0; j < dev->endpoints_count; j++) {
                endpoint_custom_t *ep = dev->endpoints_array[j];
                if (!ep) continue;

                cJSON *e = cJSON_CreateObject();
                cJSON_AddNumberToObject(e, "id", ep->ep_id);

                // ✅ Input Clusters = SERVER = может ПРИСЫЛАТЬ отчёты → может быть TARGET
                cJSON *input_clusters = cJSON_CreateArray();
                if (ep->is_use_temperature_measurement_cluster)
                {
                    cJSON_AddItemToArray(input_clusters, cJSON_CreateNumber(0x0402));
                }
                if (ep->is_use_humidity_measurement_cluster)
                {
                    cJSON_AddItemToArray(input_clusters, cJSON_CreateNumber(0x0405));
                }
                if (ep->is_use_on_off_cluster)
                {
                    cJSON_AddItemToArray(input_clusters, cJSON_CreateNumber(0x0006));
                }
                
                cJSON_AddItemToObject(e, "input_clusters", input_clusters);

                // ✅ Output Clusters = CLIENT = хочет ПОЛУЧАТЬ отчёты → может быть SOURCE
                cJSON *output_clusters = cJSON_CreateArray();
                for (int k = 0; k < ep->output_clusters_count; k++) {
                    //cluster_custom_t *c = &ep->output_clusters_array[k];
                    cJSON_AddItemToArray(output_clusters, cJSON_CreateNumber(ep->output_clusters_array[k]));
                }
                cJSON_AddItemToObject(e, "output_clusters", output_clusters);
                cJSON_AddItemToArray(eps, e);
            }
            cJSON_AddItemToObject(d, "endpoints", eps);
            cJSON_AddItemToArray(devices, d);
        }
        DEVICE_ARRAY_UNLOCK();
    }

    // 2. Добавляем координатор
    {
        cJSON *d = cJSON_CreateObject();
        char ieee_coord_str[24];
        ieee_to_str(ieee_coord_str, LocalIeeeAdr);
        cJSON_AddNumberToObject(d, "short", 0x0000);
        cJSON_AddStringToObject(d, "ieee", ieee_coord_str);
        cJSON_AddStringToObject(d, "name", "Coordinator");

        cJSON *eps = cJSON_CreateArray();
        cJSON *ep = cJSON_CreateObject();
        cJSON_AddNumberToObject(ep, "id", 1);

        cJSON *out_clusters = cJSON_CreateArray();
        for (int i = 0; i < COORD_OUTPUT_COUNT; i++) {
            cJSON_AddItemToArray(out_clusters, cJSON_CreateNumber(coordinator_output_clusters[i]));
        }
        cJSON_AddItemToObject(ep, "output_clusters", out_clusters);
        cJSON_AddItemToArray(eps, ep);

        cJSON_AddItemToObject(d, "endpoints", eps);
        cJSON_AddItemToArray(devices, d);
    }

    //char *rendered = cJSON_PrintUnformatted(devices);
    //cJSON_Minify(devices); // опционально — убирает пробелы
        int len = cJSON_PrintPreallocated(devices, json_print_buffer, sizeof(json_print_buffer), false);
        cJSON_Minify(json_print_buffer); // опционально — убирает пробелы
        if (len < 0) {
            ESP_LOGE(TAG, "Failed to print JSON into buffer");
            cJSON_Delete(devices);
            return ESP_FAIL;
        }
        char *rendered = json_print_buffer;
    cJSON_Delete(devices);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, rendered);
    //free(rendered);
    return ESP_OK;
}

// === Обработчик POST /api/bind ===
esp_err_t handle_bind_api(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Read failed");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Извлекаем параметры
    cJSON *src_addr_item = cJSON_GetObjectItem(json, "src_addr");
    cJSON *src_ep_item   = cJSON_GetObjectItem(json, "src_ep");
    cJSON *cluster_item  = cJSON_GetObjectItem(json, "cluster_id");
    cJSON *tgt_addr_item = cJSON_GetObjectItem(json, "tgt_addr");
    cJSON *tgt_ep_item   = cJSON_GetObjectItem(json, "tgt_ep");

    if (!src_addr_item || !src_ep_item || !cluster_item || !tgt_addr_item || !tgt_ep_item) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing required fields");
        return ESP_FAIL;
    }

    uint16_t src_addr = src_addr_item->valueint;
    uint8_t  src_ep   = src_ep_item->valueint;
    uint16_t cluster  = cluster_item->valueint;
    uint16_t tgt_addr = tgt_addr_item->valueint;
    uint8_t  tgt_ep   = tgt_ep_item->valueint;

    cJSON_Delete(json);

    ESP_LOGI(TAG, "BindingUtil: Bind request: %04x/%d -> %04x/%d, cluster=0x%04x",
             src_addr, src_ep, tgt_addr, tgt_ep, cluster);

    // Формируем запрос на привязку
    /*esp_zb_zdo_bind_req_param_t bind_req = {
        .src_addr = src_addr,
        .src_endp = src_ep,
        .cluster_id = cluster,
        .dst_addr_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .dst_addr = {.addr_short = tgt_addr},
        .dst_endp = tgt_ep,
    };*/

    delayed_bind_dev_dev_req_t bind_req = {
            .src_short_addr = src_addr,
            .src_endpoint = src_ep,
            .dst_short_addr = tgt_addr,
            .dst_endpoint = tgt_ep,
            .cluster_id = cluster,
    };

    bool status = zb_manager_post_to_action_worker(ZB_ACTION_DELAYED_BIND_DEV_DEV_REQ, &bind_req, sizeof(delayed_bind_dev_dev_req_t));
    if (status == true) {
        httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    } else {
        char err_resp[64];
        snprintf(err_resp, sizeof(err_resp), "{\"status\":\"fail\",\"error\":\"%d\"}", status);
        httpd_resp_sendstr(req, err_resp);
    }

    return ESP_OK;
}

// === Обработчик POST /api/report_config ===
esp_err_t handle_report_config_api(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Read failed");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Извлекаем параметры
    cJSON *short_addr_item = cJSON_GetObjectItem(json, "short_addr");
    cJSON *endpoint_item   = cJSON_GetObjectItem(json, "endpoint");
    cJSON *cluster_item    = cJSON_GetObjectItem(json, "cluster_id");
    cJSON *min_item        = cJSON_GetObjectItem(json, "min_interval");
    cJSON *max_item        = cJSON_GetObjectItem(json, "max_interval");
    cJSON *change_item     = cJSON_GetObjectItem(json, "reportable_change");

    if (!short_addr_item || !endpoint_item || !cluster_item) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing required fields");
        return ESP_FAIL;
    }

    uint16_t short_addr = short_addr_item->valueint;
    uint8_t  endpoint   = endpoint_item->valueint;
    uint16_t cluster    = cluster_item->valueint;
    uint16_t min_interval = min_item ? min_item->valueint : 1;      // по умолчанию 1 сек
    uint16_t max_interval = max_item ? max_item->valueint : 300;    // 5 минут
    uint16_t reportable_change = change_item ? change_item->valueint : 0;

    cJSON_Delete(json);

    ESP_LOGI(TAG, "REPORT_CONFIG: short=0x%04x, ep=%d, cluster=0x%04x, min=%d, max=%d, change=%d",
             short_addr, endpoint, cluster, min_interval, max_interval, reportable_change);

    // Формируем запрос
    delayed_configure_report_req_t *cfg_req = calloc(1, sizeof(delayed_configure_report_req_t));
    if (!cfg_req) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    cfg_req->short_addr = short_addr;
    cfg_req->src_endpoint = endpoint;
    cfg_req->cluster_id = cluster;
    cfg_req->min_interval = min_interval;
    cfg_req->max_interval = max_interval;
    cfg_req->reportable_change = reportable_change;

    // Отправляем в action worker
    if (zb_manager_post_to_action_worker(ZB_ACTION_DELAYED_CONFIG_REPORT_REQ, cfg_req, sizeof(*cfg_req))) {
        httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    } else {
        free(cfg_req);
        httpd_resp_sendstr(req, "{\"status\":\"fail\",\"error\":\"queue_full\"}");
    }

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

// === Альтернатива для Windows: /ncsi.txt ===
esp_err_t ncsi_txt_handler(httpd_req_t *req)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    const char* peer = "unknown";
    if (getpeername(httpd_req_to_sockfd(req), (struct sockaddr*)&addr, &addr_len) == 0) {
        peer = inet_ntoa(addr.sin_addr);
    }
    ESP_LOGI(TAG, "/ncsi.txt requested from %s", peer);

    const char *response = "Microsoft NCSI";
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// === Обработчик для Windows: /connecttest.txt ===
esp_err_t connecttest_handler(httpd_req_t *req)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    const char* peer = "unknown";
    if (getpeername(httpd_req_to_sockfd(req), (struct sockaddr*)&addr, &addr_len) == 0) {
        peer = inet_ntoa(addr.sin_addr);
    }
    ESP_LOGI(TAG, "/connecttest.txt requested from %s", peer);

    const char *response = "Microsoft Connect Test";
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// === Для iOS: /hotspot-detect.html ===
esp_err_t hotspot_detect_handler(httpd_req_t *req)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    const char* peer = "unknown";
    if (getpeername(httpd_req_to_sockfd(req), (struct sockaddr*)&addr, &addr_len) == 0) {
        peer = inet_ntoa(addr.sin_addr);
    }
    ESP_LOGI(TAG, "/hotspot-detect.html requested from %s", peer);

    const char *response = "<html><head><title>Success</title></head><body>Success</body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
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

// === mtuprobe (POST) ===
esp_err_t mtuprobe_handler(httpd_req_t *req)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    const char* peer = "unknown";
    if (getpeername(httpd_req_to_sockfd(req), (struct sockaddr*)&addr, &addr_len) == 0) {
        peer = inet_ntoa(addr.sin_addr);
    }
    ESP_LOGI(TAG, "/mtuprobe POST requested from %s", peer);

    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t upload_cert_handler(httpd_req_t *req) {
    if (!is_certs_partition_ready()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Certs partition not ready");
        return ESP_FAIL;
    }

    FILE *f = fopen(MQTT_ROOT_CERT_PATH, "w");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open cert file");
        return ESP_FAIL;
    }

    char buf[1024];
    int received;

    while ((received = httpd_req_recv(req, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, received, f);
    }

    fclose(f);
    ESP_LOGI(TAG, "MQTT root certificate saved to %s", MQTT_ROOT_CERT_PATH);

    httpd_resp_sendstr(req, "Certificate uploaded successfully");
    return ESP_OK;
}

esp_err_t get_config_handler(httpd_req_t *req) {
    load_zb_manager_app_main_config(); // актуализируем

    char buffer[1024];
    int len = snprintf(buffer, sizeof(buffer),
        "{"
            "\"ha\":{"
                "\"enabled\":%s,"
                "\"mqtt\":{"
                    "\"enabled\":%s,"
                    "\"broker\":\"%s\","
                    "\"port\":%u,"
                    "\"username\":\"%s\","
                    "\"password\":\"%s\","
                    "\"discovery\":%s,"
                    "\"availability\":%s"
                "},"
                "\"language\":\"%s\""
            "},"
            "\"web\":{"
                "\"theme\":\"%s\""
            "}"
        "}",
        g_app_config.ha_config.ha_enabled ? "true" : "false",
        g_app_config.ha_config.mqtt.mqtt_enabled ? "true" : "false",
        g_app_config.ha_config.mqtt.broker,
        g_app_config.ha_config.mqtt.port,
        g_app_config.ha_config.mqtt.username,
        g_app_config.ha_config.mqtt.password,
        g_app_config.ha_config.mqtt.discovery ? "true" : "false",
        g_app_config.ha_config.mqtt.availability ? "true" : "false",
        g_app_config.ha_config.language,
        g_app_config.web_ui_config.theme
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, len);
    return ESP_OK;
}

esp_err_t post_config_handler(httpd_req_t *req) {
    char buf[1024];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON* json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON* ha = cJSON_GetObjectItem(json, "ha");
    if (ha) {
        cJSON* item;
        if ((item = cJSON_GetObjectItem(ha, "enabled"))) g_app_config.ha_config.ha_enabled = item->valueint;
        cJSON* mqtt = cJSON_GetObjectItem(ha, "mqtt");
        if (mqtt) {
            if ((item = cJSON_GetObjectItem(mqtt, "enabled"))) g_app_config.ha_config.mqtt.mqtt_enabled = item->valueint;
            if ((item = cJSON_GetObjectItem(mqtt, "broker"))) strncpy(g_app_config.ha_config.mqtt.broker, item->valuestring, 63);
            if ((item = cJSON_GetObjectItem(mqtt, "port"))) g_app_config.ha_config.mqtt.port = item->valueint;
            if ((item = cJSON_GetObjectItem(mqtt, "username"))) strncpy(g_app_config.ha_config.mqtt.username, item->valuestring, 31);
            if ((item = cJSON_GetObjectItem(mqtt, "password"))) strncpy(g_app_config.ha_config.mqtt.password, item->valuestring, 63);
            if ((item = cJSON_GetObjectItem(mqtt, "discovery"))) g_app_config.ha_config.mqtt.discovery = item->valueint;
            if ((item = cJSON_GetObjectItem(mqtt, "availability"))) g_app_config.ha_config.mqtt.availability = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(ha, "language"))) strncpy(g_app_config.ha_config.language, item->valuestring, 7);
    }

    cJSON* web = cJSON_GetObjectItem(json, "web");
    if (web) {
        cJSON* item;
        if ((item = cJSON_GetObjectItem(web, "theme"))) strncpy(g_app_config.web_ui_config.theme, item->valuestring, 7);
    }

    cJSON_Delete(json);

    save_zb_manager_app_main_config();

    // Применяем изменения
    if (g_app_config.ha_config.ha_enabled && g_app_config.ha_config.mqtt.mqtt_enabled) {
        ha_mqtt_stop();
        ha_mqtt_init();
    } else {
        ha_mqtt_stop();
    }

    // Язык
    if (strcmp(g_app_config.ha_config.language, "ru") == 0) {
        ha_set_language(HA_LANG_RU);
    } else if (strcmp(g_app_config.ha_config.language, "en") == 0) {
        ha_set_language(HA_LANG_EN);
    } else if (strcmp(g_app_config.ha_config.language, "de") == 0) {
        ha_set_language(HA_LANG_DE);
    }

    // Тема
    // ws_notify_theme_changed(); — можно реализовать позже

    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}


//END обработчики для разных ОС

// web_server.c

esp_err_t handle_get_devices_list(httpd_req_t *req)
{
    /*cJSON *list = cJSON_CreateArray();
    if (!list) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON alloc failed");
        return ESP_FAIL;
    }

    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
        for (int i = 0; i < RemoteDevicesCount; i++) {
            device_custom_t *dev = RemoteDevicesArray[i];
            if (!dev) continue;

            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "short", dev->short_addr);
            cJSON_AddStringToObject(item, "name", dev->friendly_name[0] ? dev->friendly_name : "unknown");
            cJSON_AddBoolToObject(item, "online", dev->is_online);

            cJSON_AddItemToArray(list, item);
        }
        DEVICE_ARRAY_UNLOCK();
    }*/

    cJSON *list = NULL;
    list = zbm_base_dev_short_list_for_webserver();
    if (!list) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON alloc failed");
        return ESP_FAIL;
    }
    int len = cJSON_PrintPreallocated(list, json_print_buffer, sizeof(json_print_buffer), false);
    cJSON_Minify(json_print_buffer);
    if (len < 0) {
        cJSON_Delete(list);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Print failed");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_print_buffer);
    ESP_LOGI(TAG,"handle_get_devices_list httpd_resp_sendstr");
    cJSON_Delete(list);
    return ESP_OK;
}

// web_server.c

esp_err_t handle_get_device_detail(httpd_req_t *req)
{
    // Извлекаем short_addr из URI: /api/device/12345
    ESP_LOGI(TAG,"handle_get_device_detail:processing");
    const char *uri = req->uri;
    const char *start = strrchr(uri, '/') + 1;
    uint16_t short_addr = (uint16_t)atoi(start);

    device_custom_t *dev = zbm_dev_base_find_device_by_short_safe(short_addr);
    
    if (!dev) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Device not found");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG,"handle_get_device_detail dev ok");
    cJSON *device_json = create_device_json(dev);
    if (!device_json) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create JSON");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,"handle_get_device_detail JSONok");
    int len = cJSON_PrintPreallocated(device_json, json_print_buffer, sizeof(json_print_buffer), false);
    cJSON_Minify(json_print_buffer);
    cJSON_Delete(device_json);

    if (len < 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Print failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_print_buffer);
    return ESP_OK;
}
// === Регистрация обработчиков ===

// режим AP
/*httpd_uri_t uri_form = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = wifi_config_form_handler,
    .user_ctx = NULL
};*/

// Основная конфигурация системы
httpd_uri_t uri_get_config = {
    .uri = "/api/config",
    .method = HTTP_GET,
    .handler = get_config_handler,
};

httpd_uri_t uri_post_config = {
    .uri = "/api/config",
    .method = HTTP_POST,
    .handler = post_config_handler,
};

//контроль свободной памяти
httpd_uri_t uri_memory = {
    .uri       = "/api/memory",
    .method    = HTTP_GET,
    .handler   = memory_api_handler,
    .user_ctx  = NULL
};

httpd_uri_t uri_upload_cert = {
     .uri = "/api/upload_cert",
    .method = HTTP_POST,
    .handler = upload_cert_handler,
};

httpd_uri_t uri_save_wifi = {
    .uri = "/save_wifi",
    .method = HTTP_POST,
    .handler = save_wifi_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_cancel_save_wifi = {
    .uri = "/cancel_save_wifi",
    .method = HTTP_POST,
    .handler = cancel_save_wifi_handler,
    .user_ctx = NULL
};

// Режим STA
httpd_uri_t uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_req_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_ws = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = ws_handler,
    .user_ctx = NULL,
    .is_websocket = true
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

//обработчики для разных ОС, когда они подключаются по wifi к нашей AP
// Каптивный портал: обработчики для разных ОС
httpd_uri_t uri_generate_204_android = {
    .uri = "/generate204",
    .method = HTTP_GET,
    .handler = generate_204_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_generate_204_androidV2 = {
    .uri = "/generate_204",
    .method = HTTP_GET,
    .handler = generate_204_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_generate_204_windows = {
    .uri = "/204",
    .method = HTTP_GET,
    .handler = generate_204_handler,
    .user_ctx = NULL
};


httpd_uri_t uri_connecttest = {
    .uri = "/connecttest.txt",
    .method = HTTP_GET,
    .handler = connecttest_handler,
    .user_ctx = NULL
};


httpd_uri_t uri_ncsi_txt = {
    .uri = "/ncsi.txt",
    .method = HTTP_GET,
    .handler = ncsi_txt_handler,
    .user_ctx = NULL
};


httpd_uri_t uri_hotspot_detect = {
    .uri = "/hotspot-detect.html",
    .method = HTTP_GET,
    .handler = hotspot_detect_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_mtuprobe = {
    .uri = "/mtuprobe",
    .method = HTTP_POST,  // важно: POST
    .handler = mtuprobe_handler,
    .user_ctx = NULL
};

// Для Windows: /ipv6check
httpd_uri_t uri_ipv6check = {
    .uri = "/ipv6check",
    .method = HTTP_GET,
    .handler = ipv6check_handler,
    .user_ctx = NULL
};

// SSDP description
httpd_uri_t get_ssdp_description_xml = {
    .uri      = "/description.xml",
    .method   = HTTP_GET,
    .handler  = description_xml_handler, // функция обработки находится в ssdp_server.c
    .user_ctx = NULL
};

//HomeAssistant description
// не используется по факту
// SSDP description
httpd_uri_t get_homeassistant_json = {
    .uri      = "/homeassistant.json",
    .method   = HTTP_GET,
    .handler  = description_HA_Json_handler, // функция обработки находится в ssdp_server.c
    .user_ctx = NULL
};

//END обработчики для разных ОС, когда они подключаются по wifi к нашей AP


// загрузка устройств
httpd_uri_t uri_get_devices_list = {
    .uri       = "/api/devices/list",
    .method    = HTTP_GET,
    .handler   = handle_get_devices_list,
    .user_ctx  = NULL
};

httpd_uri_t uri_get_device_detail = {
    .uri       = "/api/device/*",  // wildcard
    .method    = HTTP_GET,
    .handler   = handle_get_device_detail,
    .user_ctx  = NULL
};
//bind
/*httpd_uri_t uri_get_binding_targets = {
    .uri       = "/api/binding_targets",
    .method    = HTTP_GET,
    .handler   = handle_get_devices_for_binding_api,
    .user_ctx  = NULL
};*/

// === HA REST API ===
httpd_uri_t uri_ha_devices = {
    .uri       = "/api/ha/devices",
    .method    = HTTP_GET,
    .handler   = ha_devices_handler,
    .user_ctx  = NULL
};

// === URI для привязки ===
httpd_uri_t uri_bind = {
    .uri       = "/api/bind",
    .method    = HTTP_POST,
    .handler   = handle_bind_api,
    .user_ctx  = NULL
};

// === URI для настройки reporting ===
httpd_uri_t uri_report_config = {
    .uri       = "/api/report_config",
    .method    = HTTP_POST,
    .handler   = handle_report_config_api,
    .user_ctx  = NULL
};

// === URI для настройки сценариев (правил) ===
// API для правил
httpd_uri_t uri_api_get_rules = {
    .uri       = "/api/rules/load",
    .method    = HTTP_GET,
    .handler   = api_get_rules_handler,
};

//создать
httpd_uri_t uri_api_post_rule = {
    .uri       = "/api/rules",
    .method    = HTTP_POST,
    .handler   = api_post_rule_handler,
};

// обновить
httpd_uri_t uri_api_put_rule = {
    .uri       = "/api/rules",
    .method    = HTTP_PUT,
    .handler   = api_put_rule_handler,
};

httpd_uri_t uri_api_delete_rule = {
    .uri       = "/api/rules/delete/*",  // /api/rules/my_light_rule
    .method    = HTTP_DELETE,
    .handler   = api_delete_rule_handler,
};

httpd_uri_t api_delete_all_rules = {
    .uri =      "/api/rules/clear",
    .method    = HTTP_DELETE,
    .handler   = api_delete_all_rules_handler,
    .user_ctx  = NULL
};
// === URI для запуска правила ===
httpd_uri_t uri_api_run_rule = {
    .uri       = "/api/rules/run/*",   // wildcard: /api/rules/run/my_rule_id
    .method    = HTTP_POST,            // только POST!
    .handler   = api_run_rule_handler,
    .user_ctx  = NULL
};

// === Запуск сервера ===
void start_webserver(void)
{
    if (server_handle) {
        ESP_LOGW(TAG, "Web server already running");
        return;
    }

    //load_index_html();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 12288; //16384
    config.core_id = 0;
    config.send_wait_timeout = 5;
    config.recv_wait_timeout = 5;
    config.task_priority = 5;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;  // ✅ ВАЖНО! чтобы пути через * работали
    config.max_uri_handlers = 40;
    config.max_open_sockets = 5;
    if (httpd_start(&server_handle, &config) == ESP_OK) {
        // Регистрируем КАПТИВНЫЕ порталы ВСЕГДА
        httpd_register_uri_handler(server_handle, &uri_save_wifi);
        httpd_register_uri_handler(server_handle, &uri_cancel_save_wifi);
        httpd_register_uri_handler(server_handle, &uri_generate_204_android);
        httpd_register_uri_handler(server_handle, &uri_generate_204_androidV2);
        httpd_register_uri_handler(server_handle, &uri_generate_204_windows);
        httpd_register_uri_handler(server_handle, &uri_connecttest);
        httpd_register_uri_handler(server_handle, &uri_ncsi_txt);
        httpd_register_uri_handler(server_handle, &uri_hotspot_detect);
        httpd_register_uri_handler(server_handle, &uri_ipv6check);
        httpd_register_uri_handler(server_handle, &uri_mtuprobe);
        httpd_register_uri_handler(server_handle, &uri_get);

        // Только в режиме STA — основной UI
        //if (!s_is_in_ap_only_mode) {
            httpd_register_uri_handler(server_handle, &uri_memory);
            httpd_register_uri_handler(server_handle, &uri_get_devices_list);
            httpd_register_uri_handler(server_handle, &uri_get_device_detail);
            httpd_register_uri_handler(server_handle, &uri_ws);
            httpd_register_uri_handler(server_handle, &uri_favicon);
            httpd_register_uri_handler(server_handle, &uri_manifest);
            httpd_register_uri_handler(server_handle, &uri_static_css);
            httpd_register_uri_handler(server_handle, &uri_static_js);
            httpd_register_uri_handler(server_handle, &uri_static_media);
            //httpd_register_uri_handler(server_handle, &uri_get_binding_targets);
            httpd_register_uri_handler(server_handle, &uri_bind);
            httpd_register_uri_handler(server_handle, &uri_report_config);
            httpd_register_uri_handler(server_handle, &get_ssdp_description_xml);
            httpd_register_uri_handler(server_handle, &get_homeassistant_json);
            httpd_register_uri_handler(server_handle, &uri_upload_cert);
            httpd_register_uri_handler(server_handle, &uri_ha_devices);
            httpd_register_uri_handler(server_handle, &uri_get_config);
            httpd_register_uri_handler(server_handle, &uri_post_config);
            httpd_register_uri_handler(server_handle, &uri_api_get_rules);
            httpd_register_uri_handler(server_handle, &uri_api_post_rule);
            httpd_register_uri_handler(server_handle, &uri_api_put_rule);
            httpd_register_uri_handler(server_handle, &uri_api_delete_rule);
            httpd_register_uri_handler(server_handle, &uri_api_run_rule);
            httpd_register_uri_handler(server_handle, &api_delete_all_rules);
            httpd_register_uri_handler(server_handle, &uri_static);
        //}
            

            ESP_LOGI(TAG, "🌐 Web server started on port %d", config.server_port);
        //}
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}

void stop_webserver(void)
{
    if (server_handle) {
        httpd_stop(server_handle);
        server_handle = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
}
