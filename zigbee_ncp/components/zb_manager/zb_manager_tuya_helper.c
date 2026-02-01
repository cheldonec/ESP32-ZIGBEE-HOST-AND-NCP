#include "zb_manager_tuya_helper.h"
#include "zb_handlers.h"
#include "esp_log.h"
#include "esp_check.h"
#include "string.h"
#include "zb_manager_endpoints_gen.h"
#include "zb_manager.h"
#include "esp_random.h"
#include <inttypes.h> 

static const char *TAG = "zb_tuya_helper_module";
esp_err_t parse_and_send_tuya_dp(uint16_t short_addr, uint8_t endpoint, const uint8_t *data, uint16_t data_len, int8_t rssi, uint8_t lqi)
{
    esp_err_t err = ESP_FAIL;
    ESP_LOGW(TAG, "Parsing Tuya DP payload PRINT INPUT DATA:");
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, data_len, ESP_LOG_INFO);
    if (data_len < 4) return err; // min: dp_id (1) + dp_type (1) + dp_len (2) = 4

    const uint8_t *ptr = data;  // ✅ Начинаем с начала
    const uint8_t *end = data + data_len;

    uint8_t dp_id = ptr[0];
    uint8_t dp_type = ptr[1];
    ptr += 2;

    if (ptr + 2 > end) return err;
    uint16_t dp_len = (ptr[1] << 8) | ptr[0];  // little-endian
    ptr += 2;

    ESP_LOGI(TAG, "DP ID: %u, Type: 0x%02x, Len: %u", dp_id, dp_type, dp_len);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, ptr, dp_len, ESP_LOG_INFO);

    if (ptr + dp_len > end) {
    ESP_LOGW(TAG, "DP data too short: need %" PRIu32 ", have %" PRIu32, (uint32_t)dp_len, (uint32_t)(end - ptr));
    return err;
    }

    uint32_t dp_value = 0;

    switch (dp_type) {
    case 0x01: // bool, enum
        if (dp_len >= 1) dp_value = ptr[0];
        break;
    case 0x02: // string (но Tuya часто использует как value)
    case 0x04: // value
        if (dp_len == 1) dp_value = ptr[0];
        else if (dp_len == 2) dp_value = (ptr[1] << 8) | ptr[0];
        else if (dp_len == 4) {
            dp_value = (uint32_t)ptr[3] << 24 |
                       (uint32_t)ptr[2] << 16 |
                       (uint32_t)ptr[1] << 8  |
                       (uint32_t)ptr[0];
        }
        break;
    default:
        ESP_LOGW(TAG, "Unsupported DP type: 0x%02x", dp_type);
        return err;
}

    // Только DP 1, 2, 4 обрабатываем
    if (dp_id != 1 && dp_id != 2 && dp_id != 4) {
        ESP_LOGI(TAG, "Skipping unsupported DP ID: %u", dp_id);
        return ESP_OK; // не ошибка, просто игнорируем
    }

    zb_manager_tuya_dp_report_t report = {
        .short_addr = short_addr,
        .endpoint = endpoint,
        .dp_id = dp_id,
        .dp_type = dp_type,
        .dp_value = dp_value,
        .rssi = rssi,
        .lqi = lqi,
    };

    esp_ncp_header_t ncp_hdr = {
        .id = ZB_MANAGER_TUYA_DP_REPORT,
        .sn = esp_random() % 0xFF,
        .len = sizeof(report),
    };

    err = esp_ncp_noti_input(&ncp_hdr, &report, sizeof(report));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✅ DP sent: ID=%u, Value=%" PRIu32 ", Type=0x%02x", dp_id, dp_value, dp_type);
    } else {
        ESP_LOGE(TAG, "❌ Failed to send DP: %s", esp_err_to_name(err));
    }

    return err;
}
