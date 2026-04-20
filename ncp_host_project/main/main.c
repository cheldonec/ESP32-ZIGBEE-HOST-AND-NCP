#include <stdio.h>
#include "zbm_core_sync.h"

#include "zbm_test_dynamic.h"
#include "esp_log.h"
#include "esp_err.h"
#include "wifi_manager.h"
#include "zbm_web_server.h"
#include "zbm_spiffs_helper.h"
#include "zbm_core_sync.h" 
#include "zbm_ncp_connect.h"
#include "zbm_automation_v2.h"

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default()); 

    // === ИНИЦИАЛИЗАЦИЯ SPIFFS ===
    esp_err_t ret = init_spiffs();
    if (ret != ESP_OK) {
        ESP_LOGE("main", "SPIFFS init failed: %s", esp_err_to_name(ret));
        // Можно продолжить, но без файлов
    }
    
    // Инициализация системы
    zbm_core_sync_init();
    zbm_guid_db_init();  // если ещё не вызвано
    zbm_device_db_init();

    // Подгружаем сохранённые устройства и координатор
    zbm_load_all_devices_from_spiffs_and_restore();

    zb_automation_v2_init();  // инициализируем движок правил


    ret = zbm_ncp_connect_start();

    // Запуск теста: каждые 2 секунды, бесконечно
    /*zbm_test_run(1000, true);

    ESP_LOGI("MAIN", "Test is running...");*/

    //zbm_test_dynamic_run(); // запустить тест

    

    ESP_LOGI("main", "Starting Wi-Fi Manager...");

    // Инициализация Wi-Fi менеджера
    esp_err_t ret_wifi = wifi_manager_init();
    if (ret_wifi != ESP_OK) {
        ESP_LOGE("main", "Wi-Fi Manager init failed: %s", esp_err_to_name(ret_wifi));
        return;
    }

    

    // === ЗАПУСК ВЕБ-СЕРВЕРА ===
    start_webserver();  // ← Это запустит HTTP + WebSocket + API
    

    //ESP_LOGI("main", "Wi-Fi Manager started in AP mode");
}
