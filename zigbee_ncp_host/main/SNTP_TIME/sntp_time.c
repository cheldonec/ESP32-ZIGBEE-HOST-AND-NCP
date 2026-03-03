// main/SNTP_TIME/sntp_time.c
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "zb_manager_rules.h"

static const char* TAG = "SNTP_TIME";
extern void check_time_triggers(void); 
// Колбэк: вызывается при успешной синхронизации времени
void time_sync_notification_cb(struct timeval* tv)
{
    struct tm timeinfo;
    char strftime_buf[64];

    time_t now = tv->tv_sec;
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    ESP_LOGI(TAG, "✅ Время синхронизировано: %s", strftime_buf);

    // 🔥 Немедленно проверить временные триггеры!
    check_time_triggers();
}

// Инициализация SNTP клиента
void sntp_initialize(void)
{
    // Установка часового пояса (Москва = UTC+3)
    setenv("TZ", "MSK-3", 1);  // Можно использовать: "CET-1CEST,M3.5.0,M10.5.0/3", "UTC+3"
    tzset();

    ESP_LOGI(TAG, "🌍 Часовой пояс установлен: MSK (UTC+3)");

    // Конфигурация SNTP
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = time_sync_notification_cb;           // опционально: пользовательский колбэк
    config.start = false;                                 // запустим вручную позже

    // === ПРОВЕРКА: если ESP-IDF >= 5.1, можно использовать события ===
    // Но если нет — просто пропускаем
#if CONFIG_ESP_NETIF_ENABLE_SNTP_EVENT
    // Только если в menuconfig включено: Component config → TCP/IP → Enable SNTP event
    esp_err_t err_reg = esp_event_handler_register(NETIF_SNTP_EVENT, NETIF_SNTP_TIME_SYNC, NULL, NULL);
    if (err_reg == ESP_OK) {
        ESP_LOGI(TAG, "📡 Обработчик SNTP-событий зарегистрирован");
    } else {
        ESP_LOGW(TAG, "⚠️ Не удалось зарегистрировать SNTP-событие (возможно, отключено в menuconfig)");
    }
#else
    ESP_LOGI(TAG, "📡 События SNTP отключены (CONFIG_ESP_NETIF_ENABLE_SNTP_EVENT)");
#endif

    // Инициализируем SNTP через esp-netif
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Ошибка инициализации SNTP: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "📡 SNTP клиент инициализирован, ожидание IP...");

    // Запускаем SNTP (после получения IP)
    esp_netif_sntp_start();
}