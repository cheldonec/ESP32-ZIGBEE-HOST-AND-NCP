#include "zbm_dev_polling.h"
#include "string.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_check.h"

static const char* TAG = "ZBM_DEV_POLLING_MODULE";
static esp_timer_handle_t online_status_timer; // таймер для обновления online статуса

esp_err_t zbm_dev_pooling_init(uint16_t pooling_period_in_seconds);
uint32_t zbm_dev_get_timeout_for_device_id(uint16_t device_id);
void zbm_dev_configure_device_timeout(device_custom_t* dev);
//================================================================================================================================
//============================================ ZBM_DEV_GET_TIMEOUT_FOR_DEVICE_ID =============================================
//================================================================================================================================

uint32_t zbm_dev_get_timeout_for_device_id(uint16_t device_id)
{
    switch (device_id) {
        // Датчики — редко отвечают
        case ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID:
        case ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID:
        case ESP_ZB_HA_IAS_ZONE_ID:
        case ESP_ZB_HA_LIGHT_SENSOR_DEVICE_ID:
            return ZB_DEVICE_SENSOR_TIMEOUT_MS; 
        //case ESP_ZB_HA_OCCUPANCY_SENSOR_DEVICE_ID:
            return ZB_DEVICE_SENSOR_TIMEOUT_MS;          // 5 мин

        // Выключатели, кнопки — могут быть "спящими", но лучше проверять чаще
        case ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID:
        case ESP_ZB_HA_DIMMER_SWITCH_DEVICE_ID:
        case ESP_ZB_HA_COLOR_DIMMER_SWITCH_DEVICE_ID:
            return ZB_DEVICE_SWITCH_TIMEOUT_MS;          // 2 мин

        // Устройства с питанием — должны быть всегда онлайн
        case ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID:
        case ESP_ZB_HA_DIMMABLE_LIGHT_DEVICE_ID:
        case ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID:
        case ESP_ZB_HA_MAINS_POWER_OUTLET_DEVICE_ID:
        case ESP_ZB_HA_SMART_PLUG_DEVICE_ID:
        case ESP_ZB_HA_RANGE_EXTENDER_DEVICE_ID:
            return ZB_DEVICE_DEFAULT_TIMEOUT_MS;         // 60 сек
            //return ZB_DEVICE_TUYA_COIL_TIMEOUT_MS;         // 3 минуты

        // Роутеры, шлюзы
        case ESP_ZB_HA_HOME_GATEWAY_DEVICE_ID:
            return ZB_DEVICE_ROUTER_TIMEOUT_MS;          // 60 сек
        //case ESP_ZB_HA_RANGE_EXTENDER_DEVICE_ID:
            //return ZB_DEVICE_ROUTER_TIMEOUT_MS;          // 60 сек

        // По умолчанию
        default:
            return ZB_DEVICE_DEFAULT_TIMEOUT_MS;
    }
}
//********************************************************************************************************************************

//================================================================================================================================
//============================================ ZBM_DEV_CONFIGURE_DEVICE_TIMEOUT ==================================================
//================================================================================================================================
void zbm_dev_configure_device_timeout(device_custom_t* dev)
{
    if (!dev) return;

    uint32_t timeout = ZB_DEVICE_DEFAULT_TIMEOUT_MS;

    // Если Aqara b2lc04 → 60 сек
    if (dev->manufacturer_code == 0x115F) {
        const char *model = dev->server_BasicClusterObj ? dev->server_BasicClusterObj->model_identifier : NULL;
        if (model && strcmp(model, "lumi.switch.b2lc04") == 0) {
            dev->device_timeout_ms = 60000;
            return;
        }
    }

    // По умолчанию: выбираем максимальный таймаут
    for (int i = 0; i < dev->endpoints_count; i++) {
        endpoint_custom_t* ep = dev->endpoints_array[i];
        if (!ep) continue;
        if (ep->is_use_on_device == true)
        {
            uint32_t ep_timeout = zbm_dev_get_timeout_for_device_id(ep->deviceId);
            if (ep_timeout > timeout) {
                timeout = ep_timeout;
            }
        }
    }
    dev->device_timeout_ms = timeout;
}
//********************************************************************************************************************************

//================================================================================================================================
//========================================================= ZBM_DEV_POOLING_INIT =================================================
//================================================================================================================================
static void check_all_devices_status(void *arg)
{
    ESP_LOGI(TAG,"check_all_devices_status: processing АЛГОРИТМА НЕТ ПОКА");
}
esp_err_t zbm_dev_pooling_init(uint16_t pooling_period_in_seconds)
{
    esp_timer_create_args_t status_timer_args = {
    .callback = check_all_devices_status,
    .name = "online_status_checker"
    };
    ESP_ERROR_CHECK(esp_timer_create(&status_timer_args, &online_status_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(online_status_timer, pooling_period_in_seconds * 1000000)); // 
    return ESP_OK;
}