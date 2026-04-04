/*MIT License

Copyright (c) 2025 Lyxt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.*/

#include "wifi_manager.h"
#include "dns_hijack.h"
#include "ssdp_server.h"
#include "esp_log.h"
#include "mdns.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "lwip/ip_addr.h"
#include "zbm_coordinator.h"

static const char *TAG = "wifi_manager";
static const char *TAG_AP = "WiFi SoftAP";
static const char *TAG_STA = "WiFi Sta";

static wifi_mode_t current_wifi_mode = WIFI_MODE_NULL;

// ==================== КНОПКА ====================
static const int BUTTON_GPIO = 0;
static const int BUTTON_DEBOUNCE_MS = 50;
static const int BUTTON_LONG_PRESS_MS = 3000;
static void button_check_task(void *pvParameters);

static void button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Button configured on GPIO %d (pullup enabled)", BUTTON_GPIO);
    xTaskCreate(button_check_task, "button_task", 2048, NULL, 8, NULL);
}

static void button_check_task(void *pvParameters)
{
    bool button_pressed = false;
    uint32_t press_start = 0;
    static bool is_processing = false;

    while (1) {
        bool level = gpio_get_level(BUTTON_GPIO);

        if (!level && !button_pressed && !is_processing) {
            button_pressed = true;
            press_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            is_processing = true;
            ESP_LOGI(TAG, "Button pressed");
        }
        else if (level && button_pressed) {
            uint32_t press_duration = (xTaskGetTickCount() * portTICK_PERIOD_MS) - press_start;
            ESP_LOGI(TAG, "Button released. Duration: %ums", press_duration);

            if (press_duration >= BUTTON_LONG_PRESS_MS) {
                ESP_LOGW(TAG, "Long press detected! TOGGLING WIFI mode setup...");
                wifi_manager_toggle_wifi_mode_safe();
            } else {
                ESP_LOGD(TAG, "Short press ignored (duration: %ums)", press_duration);
            }

            button_pressed = false;
            is_processing = false;
        }

        vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));
    }
}

// =============== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ===============
esp_netif_t *esp_netif_ap = NULL;
esp_netif_t *esp_netif_sta = NULL;

char s_last_ssid[32] = {0};
char s_last_password[64] = {0};

static QueueHandle_t s_mode_event_queue = NULL;
static TaskHandle_t s_mode_task_handle = NULL;
static TimerHandle_t s_connection_timeout_timer = NULL;

// Новая очередь для внутренних событий
typedef enum {
    WIFI_MANAGER_EVENT_GOT_IP,
} wifi_manager_internal_event_t;

static QueueHandle_t s_internal_event_queue = NULL;

// ============= КОНФИГУРАЦИЯ =============
#define EXAMPLE_ESP_WIFI_AP_SSID      CONFIG_ESP_WIFI_AP_SSID
#define EXAMPLE_ESP_WIFI_AP_PASSWD    CONFIG_ESP_WIFI_AP_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL      6
#define EXAMPLE_MAX_STA_CONN          CONFIG_ESP_MAX_STA_CONN_AP

#define EXAMPLE_ESP_WIFI_STA_SSID     CONFIG_ESP_WIFI_REMOTE_AP_SSID
#define EXAMPLE_ESP_WIFI_STA_PASSWD   CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD

#define MDNS_HOSTNAME CONFIG_MDNS_HOST_NAME

static bool mdns_is_started = false;

// ============ mDNS ============
static void start_mdns_service(esp_netif_t *netif)
{
    if (mdns_is_started) {
        mdns_free();
        mdns_is_started = false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (mdns_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init mDNS");
        return;
    }

    // Берём hostname из координатора, если задано
    const char* hostname = zbm_coordinator.hostname ? zbm_coordinator.hostname : "esp32-zigbee";
    mdns_hostname_set(hostname);
    mdns_instance_name_set("ESP32 Zigbee Gateway");

    // HTTP сервис
    mdns_service_add("web", "_http", "_tcp", 80, NULL, 0);

    // WebSocket сервис
    mdns_service_add("ws", "_ws", "_tcp", 81, NULL, 0);

    // TXT для HTTP
    const mdns_txt_item_t http_txt[] = {{ "path", "/" }};
    mdns_service_add("ESP32 Zigbee Gateway", "_http", "_tcp", 80, http_txt, 1);

    // Home Assistant discovery
    char config_url[96];
    snprintf(config_url, sizeof(config_url), "http://%s.local/homeassistant.json", hostname);
    const mdns_txt_item_t ha_txt[] = {
        { "version", "1.0" },
        { "features", "configuration_url" },
        { "config", config_url }
    };
    mdns_service_add("ESP32 Zigbee Gateway", "_home-assistant", "_tcp", 80, ha_txt, 3);

    mdns_is_started = true;
    ESP_LOGI(TAG, "mDNS started: http://%s.local", hostname);
}

static void stop_mdns_service(void)
{
    if (mdns_is_started) {
        mdns_free();
        mdns_is_started = false;
        ESP_LOGI(TAG, "mDNS stopped");
    }
}

// ============ ИНИЦИАЛИЗАЦИЯ AP ============
esp_netif_t *wifi_init_softap(void)
{
    if (esp_netif_ap) return esp_netif_ap;
    esp_netif_ap = esp_netif_create_default_wifi_ap();

    const char* ap_ssid = zbm_coordinator.wifi_ap_ssid ? zbm_coordinator.wifi_ap_ssid : "Zigbee-GW-Setup";
    const char* ap_pass = zbm_coordinator.wifi_ap_password;
    uint8_t channel = zbm_coordinator.wifi_ap_channel;
    uint8_t max_conn = zbm_coordinator.wifi_ap_max_connections;

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = {0},
            .ssid_len = strlen(ap_ssid),
            .channel = channel,
            .max_connection = max_conn,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {.capable = false, .required = false},
        },
    };

    size_t ssid_len = strlen(ap_ssid);
    memcpy(wifi_ap_config.ap.ssid, ap_ssid, ssid_len < 32 ? ssid_len : 31);

    if (ap_pass && strlen(ap_pass) >= 8) {
        size_t pass_len = strlen(ap_pass);
        memcpy(wifi_ap_config.ap.password, ap_pass, pass_len < 64 ? pass_len : 63);
        wifi_ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
    ESP_LOGI(TAG_AP, "AP init: SSID='%s', ch=%d, auth=%s",
             ap_ssid, channel,
             wifi_ap_config.ap.authmode == WIFI_AUTH_OPEN ? "OPEN" : "WPA2");

    return esp_netif_ap;
}

// ============ ИНИЦИАЛИЗАЦИЯ STA ============
esp_netif_t *wifi_init_sta(bool connect_enable)
{
    if (!esp_netif_sta) {
        esp_netif_sta = esp_netif_create_default_wifi_sta();
    }

    if (connect_enable && zbm_coordinator.wifi_sta_ssid && strlen(zbm_coordinator.wifi_sta_ssid) > 0) {
        wifi_config_t wifi_sta_config = {
            .sta = {
                .scan_method = WIFI_ALL_CHANNEL_SCAN,
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            },
        };
        strncpy((char*)wifi_sta_config.sta.ssid, zbm_coordinator.wifi_sta_ssid, 32);
        if (zbm_coordinator.wifi_sta_password && strlen(zbm_coordinator.wifi_sta_password) > 0) {
            strncpy((char*)wifi_sta_config.sta.password, zbm_coordinator.wifi_sta_password, 64);
        } else {
            wifi_sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        }

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
        ESP_LOGI(TAG_STA, "STA init: SSID=%s", zbm_coordinator.wifi_sta_ssid);
        if (connect_enable) {
            esp_wifi_connect();
        }
    } else {
        ESP_LOGI(TAG_STA, "No saved Wi-Fi credentials in coordinator");
    }
    return esp_netif_sta;
}

// ============ ОБРАБОТЧИК СОБЫТИЙ ============
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG_STA, "STA started");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG_STA, "STA disconnected");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_STA, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        zbm_coordinator.is_sta_valid = 1;
        ssdp_update_ip_from_event(event->ip_info.ip.addr);

        if (s_connection_timeout_timer) {
            xTimerStop(s_connection_timeout_timer, 0);
        }

        // Отправляем событие в безопасную задачу
        wifi_manager_internal_event_t evt = WIFI_MANAGER_EVENT_GOT_IP;
        xQueueSend(s_internal_event_queue, &evt, 0);
    }
}

// ============ ТАЙМЕР ПОДКЛЮЧЕНИЯ ============
static void connection_timeout_callback(TimerHandle_t xTimer)
{
    ESP_LOGW(TAG, "STA connection timeout! No IP in 15s → switching to AP mode");
    zbm_coordinator.is_sta_valid = 0;
    wifi_manager_post_event(WIFI_MODE_EVENT_SWITCH_TO_AP);
}

// ============ ПЕРЕКЛЮЧЕНИЕ РЕЖИМОВ ============
void wifi_manager_post_event(wifi_mode_event_t event)
{
    if (s_mode_event_queue) {
        xQueueSend(s_mode_event_queue, &event, 0);
    }
}

static void mode_handling_task(void *pvParameters)
{
    wifi_mode_event_t event;
    while (1) {
        if (xQueueReceive(s_mode_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event) {
                case WIFI_MODE_EVENT_AP: {
                    ESP_LOGI(TAG, "→ Switching to AP mode");
                    zbm_coordinator.wifi_mode = ZBM_COORDINATOR_WIFI_MODE_AP;
                    stop_mdns_service();
                    stop_ssdp_server();
                    deinit_dns_hijack();
                    esp_wifi_stop();
                    vTaskDelay(pdMS_TO_TICKS(100));

                    if (esp_netif_ap) {
                        esp_netif_destroy(esp_netif_ap);
                        esp_netif_ap = NULL;
                    }
                    esp_netif_sta = NULL;

                    esp_wifi_set_mode(WIFI_MODE_AP);
                    wifi_init_softap();
                    esp_wifi_start();

                    vTaskDelay(pdMS_TO_TICKS(100));

                    init_dns_hijack();
                    start_ssdp_server();
                    current_wifi_mode = WIFI_MODE_AP;
                    ESP_LOGI(TAG, "AP mode activated");
                    break;
                }

                case WIFI_MODE_EVENT_STA: {
                    ESP_LOGI(TAG, "→ Switching to STA mode");
                    zbm_coordinator.wifi_mode = ZBM_COORDINATOR_WIFI_MODE_STA;
                    stop_mdns_service();
                    stop_ssdp_server();
                    deinit_dns_hijack();
                    esp_wifi_stop();
                    vTaskDelay(pdMS_TO_TICKS(100));

                    if (esp_netif_sta) {
                        esp_netif_destroy(esp_netif_sta);
                        esp_netif_sta = NULL;
                    }
                    esp_netif_ap = NULL;

                    esp_wifi_set_mode(WIFI_MODE_STA);
                    wifi_init_sta(true);
                    esp_wifi_start();

                    start_ssdp_server();
                    current_wifi_mode = WIFI_MODE_STA;

                    if (s_connection_timeout_timer) {
                        xTimerReset(s_connection_timeout_timer, 0);
                    }

                    ESP_LOGI(TAG, "STA mode activated, connecting...");
                    break;
                }

                case WIFI_MODE_EVENT_SWITCH_TO_AP:
                    ESP_LOGI(TAG, "Button: switching to AP mode");
                    wifi_manager_post_event(WIFI_MODE_EVENT_AP);
                    break;

                case WIFI_MODE_EVENT_SWITCH_TO_STA: {
                    ESP_LOGI(TAG, "Button: attempting to switch to STA mode");
                    if (zbm_coordinator.wifi_sta_ssid && strlen(zbm_coordinator.wifi_sta_ssid) > 0) {
                        wifi_manager_post_event(WIFI_MODE_EVENT_STA);
                    } else {
                        ESP_LOGW(TAG, "No saved Wi-Fi credentials → cannot switch to STA");
                    }
                    break;
                }

                case WIFI_MODE_EVENT_RECONNECT_STA:
                    ESP_LOGI(TAG, "Reconnecting STA...");
                    esp_wifi_disconnect();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    wifi_manager_post_event(WIFI_MODE_EVENT_STA);
                    break;
            }
        }
    }
}

// ============ ЗАДАЧА ДЛЯ ВНУТРЕННИХ СОБЫТИЙ ============
static void internal_event_task(void *pvParameters)
{
    wifi_manager_internal_event_t event;
    while (1) {
        if (xQueueReceive(s_internal_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event) {
                case WIFI_MANAGER_EVENT_GOT_IP:
                    ESP_LOGI(TAG, "Internal task: handling GOT_IP event");
                    start_mdns_service(esp_netif_sta);
                    //stop_ssdp_server();
                    //start_ssdp_server();
                    break;
                default:
                    break;
            }
        }
    }
}

// ============ ИНИЦИАЛИЗАЦИЯ МЕНЕДЖЕРА ============
esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing Wi-Fi Manager...");

    ESP_ERROR_CHECK(esp_netif_init());

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    bool have_saved_wifi = (zbm_coordinator.wifi_sta_ssid && strlen(zbm_coordinator.wifi_sta_ssid) > 0);
    if (have_saved_wifi) {
        current_wifi_mode = WIFI_MODE_STA;
        zbm_coordinator.wifi_mode = ZBM_COORDINATOR_WIFI_MODE_STA;
    } else {
        current_wifi_mode = WIFI_MODE_AP;
        zbm_coordinator.wifi_mode = ZBM_COORDINATOR_WIFI_MODE_AP;
    }

    s_mode_event_queue = xQueueCreate(5, sizeof(wifi_mode_event_t));
    if (!s_mode_event_queue) {
        ESP_LOGE(TAG, "Failed to create mode event queue");
        return ESP_FAIL;
    }

    s_internal_event_queue = xQueueCreate(3, sizeof(wifi_manager_internal_event_t));
    if (!s_internal_event_queue) {
        ESP_LOGE(TAG, "Failed to create internal event queue");
        return ESP_FAIL;
    }

    xTaskCreate(&mode_handling_task, "mode_task", 6144, NULL, 10, &s_mode_task_handle);
    xTaskCreate(&internal_event_task, "int_evt_task", 4096, NULL, 8, NULL);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    s_connection_timeout_timer = xTimerCreate(
        "conn_tmo",
        pdMS_TO_TICKS(15000),
        pdFALSE,
        NULL,
        connection_timeout_callback
    );

    if (have_saved_wifi) {
        ESP_LOGI(TAG, "Saved Wi-Fi found: SSID='%s'. Trying to connect in STA mode...", s_last_ssid);
        wifi_manager_post_event(WIFI_MODE_EVENT_STA);
    } else {
        ESP_LOGW(TAG, "No saved Wi-Fi credentials. Starting in AP mode for setup...");
        wifi_manager_post_event(WIFI_MODE_EVENT_AP);
    }

    button_init();

    ESP_LOGI(TAG, "Wi-Fi Manager initialized");
    return ESP_OK;
}

// ============ ПЕРЕКЛЮЧЕНИЕ В AP ============
void wifi_manager_toggle_wifi_mode_safe(void)
{
    if (current_wifi_mode == WIFI_MODE_STA) {
        wifi_manager_post_event(WIFI_MODE_EVENT_SWITCH_TO_AP);
    } else {
        wifi_manager_post_event(WIFI_MODE_EVENT_SWITCH_TO_STA);
    }
}