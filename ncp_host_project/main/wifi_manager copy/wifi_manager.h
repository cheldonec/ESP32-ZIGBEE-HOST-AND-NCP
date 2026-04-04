#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_server.h"

// Тип события для переключения режимов
typedef enum {
    WIFI_MODE_EVENT_AP,
    WIFI_MODE_EVENT_STA,
    WIFI_MODE_EVENT_SWITCH_TO_AP,
    WIFI_MODE_EVENT_SWITCH_TO_STA,
    WIFI_MODE_EVENT_RECONNECT_STA
} wifi_mode_event_t;

// Глобальные переменные
extern char s_last_ssid[32];
extern char s_last_password[64];

// Инициализация
esp_err_t wifi_manager_init(void);

// Переключение режимов
void wifi_manager_post_event(wifi_mode_event_t event);
void wifi_manager_toggle_wifi_mode_safe(void);

// Wi-Fi операции
//esp_err_t load_wifi_config_from_nvs(void);
//esp_err_t save_wifi_config_to_nvs(const char *ssid, const char *password);
void wifi_scan(void);

#endif // WIFI_MANAGER_H