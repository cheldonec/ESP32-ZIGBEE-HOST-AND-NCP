// main/zbm_automation/zbm_automation.c
#include "zbm_automation.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static const char* TAG = "ZBM_AUTO";

// Таймер для периодической проверки времени
static TimerHandle_t time_check_timer = NULL;

// ===================================================================
//                         Отправка команд
// ===================================================================

bool zb_automation_send_command(const zb_automation_request_t* req) {
    if (!req) return false;

    // Здесь должна быть логика отправки Zigbee команды
    // Например, через esp_zb_zcl_on_off_cmd_req или аналоги

    ESP_LOGI(TAG, "📤 CMD: addr=0x%04x ep=%d clst=0x%04x cmd=0x%02x val=%d",
             req->short_addr, req->endpoint_id, req->cluster_id, req->cmd_id, req->value);

    // Пример заглушки:
    switch (req->cmd_id) {
        case 0x00:
            ESP_LOGI(TAG, "   → Off");
            break;
        case 0x01:
            ESP_LOGI(TAG, "   → On");
            break;
        case 0x02:
            ESP_LOGI(TAG, "   → Toggle");
            break;
        default:
            ESP_LOGW(TAG, "   → Unknown cmd");
            break;
    }

    // В реальной реализации:
    // - Найти endpoint
    // - Определить кластер (ON/OFF, Level и т.д.)
    // - Отправить команду через esp_zb_zcl_*
    // Пока — заглушка

    return true;
}

// ===================================================================
//                         Приём событий
// ===================================================================

void zb_automation_report_received(
    uint16_t short_addr,
    uint8_t endpoint_id,
    uint16_t cluster_id,
    uint16_t attr_id,
    void* data,
    uint8_t data_len,
    zbm_attr_data_types_t attr_type
) {
    ESP_LOGD(TAG, "⚡ State update: %04x/%d/%04x = %p (%d bytes)", 
             short_addr, endpoint_id, cluster_id, data, data_len);

    // Передаём событие в движок правил
    zb_rule_trigger_state_update(short_addr, cluster_id, attr_id, data, data_len, attr_type);
}

void zb_automation_device_unavailable(uint16_t short_addr, uint16_t cluster_id) {
    ESP_LOGW(TAG, "💀 Device unavailable: %04x, cluster=%04x", short_addr, cluster_id);
    zb_rule_trigger_device_unavailable(short_addr, cluster_id);
}

// ===================================================================
//                         Временные триггеры
// ===================================================================

void zb_automation_check_time_triggers(void) {
    check_time_triggers();        // Из zbm_rules.c
    process_delayed_actions();    // Обработка отложенных действий
}

static void time_check_callback(TimerHandle_t xTimer) {
    zb_automation_check_time_triggers();
}

// ===================================================================
//                         Инициализация
// ===================================================================

void zb_automation_init(void) {
    // Инициализация движка правил
    zb_rule_engine_init();

    // Создаём таймер: проверка каждую секунду
    time_check_timer = xTimerCreate(
        "TimeCheckTimer",
        pdMS_TO_TICKS(1000),
        pdTRUE,
        NULL,
        time_check_callback
    );

    if (time_check_timer) {
        xTimerStart(time_check_timer, 0);
        ESP_LOGI(TAG, "⏱ Time trigger checker started (1 Hz)");
    } else {
        ESP_LOGE(TAG, "❌ Failed to create time check timer");
    }

    ESP_LOGI(TAG, "✅ Automation system initialized");
}