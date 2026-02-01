/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/*  WiFi softAP & station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
/*#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#if IP_NAPT
#include "lwip/lwip_napt.h"
#endif
#include "lwip/err.h"
#include "lwip/sys.h"*/
#include "zb_manager.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "quirks_storage.h"
#include "zb_manager_main_config.h"
#include "ha_mqtt_publisher.h"
#include "zb_manager_rules.h"
static const char *TAG = "main";
void app_main(void)
{
    
   ESP_ERROR_CHECK(esp_event_loop_create_default());

   esp_err_t ret = init_spiffs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS init failed");
        return;
    }
   ESP_ERROR_CHECK(ret);

   load_zb_manager_app_main_config(); //
   // Теперь можно применить язык
    if (strcmp(g_app_config.ha_config.language, "en") == 0) {
        ha_set_language(HA_LANG_EN);
    } else if (strcmp(g_app_config.ha_config.language, "de") == 0) {
        ha_set_language(HA_LANG_DE);
    } else {
        ha_set_language(HA_LANG_RU); // по умолчанию
    }

   // Create EP 1
    uint16_t inputClusterEP1[] = {0x0000, 0x0001, 0x0003, 0x0004, 0x0005, 0x0006, 0x0402, 0x0405, 0xEF00};
    uint16_t outputClusterEP1[] = {0x0000, 0x0001, 0x0003, 0x0004, 0x0005, 0x0006, 0x0402, 0x0405, 0xEF00};
    esp_host_zb_endpoint_t host_endpoint1 = {
        .endpoint = 1,
        .profileId = ESP_ZB_AF_HA_PROFILE_ID,
        .deviceId = ESP_ZB_HA_REMOTE_CONTROL_DEVICE_ID,
        .appFlags = 0,
        .inputClusterCount = sizeof(inputClusterEP1) / sizeof(inputClusterEP1[0]),
        .inputClusterList = inputClusterEP1,
        .outputClusterCount = sizeof(outputClusterEP1) / sizeof(outputClusterEP1[0]),
        .outputClusterList = outputClusterEP1,
    };
    
    // Create EP 2
    uint16_t inputClusterEP2[] = {0x0000, 0x0001, 0x0003, 0x0004, 0x0005, 0x0006, 0x0402, 0x0405};
    uint16_t outputClusterEP2[] = {0x0000, 0x0001, 0x0003, 0x0004, 0x0005, 0x0006, 0x0402, 0x0405};
    esp_host_zb_endpoint_t host_endpoint2 = {
        .endpoint = 2,
        .profileId = ESP_ZB_AF_HA_PROFILE_ID,
        .deviceId = ESP_ZB_HA_REMOTE_CONTROL_DEVICE_ID,
        .appFlags = 0,
        .inputClusterCount = sizeof(inputClusterEP2) / sizeof(inputClusterEP2[0]),
        .inputClusterList = inputClusterEP2,
        .outputClusterCount = sizeof(outputClusterEP2) / sizeof(outputClusterEP2[0]),
        .outputClusterList = outputClusterEP2,
    };

   ret = zm_init_devices_base();
   ret = json_load_and_print(ZB_MANAGER_JSON_DEVICES_FILE);

   ESP_ERROR_CHECK(ret);

   // инициализация автоматизации
   zb_rule_engine_init();

   // ⚠️ Только для отладки!
    //ESP_LOGW("MAIN", "⚠️ Удаляем все правила для восстановления состояния");
    //zb_rule_engine_remove_all_rules();

   ret = quirks_load_tuya_database();

   ret = zm_FastStartZigbee(&host_endpoint1, NULL, NULL, NULL);

   vTaskDelay(pdMS_TO_TICKS(1000));

   wifi_manager_init();

   // контроль NCP
   /*while (1) {
        uint32_t notify = ulTaskNotifyTake(pdTRUE, 100);
        if (notify) {
            
            g_zigbee_restarting = true;
            ESP_LOGW("MAIN", "⚠️ Перезапуск NCP");
            zm_FastRestartZigbeeOnFoultedure();
            g_zigbee_restarting = false;
        }

    }*/
   
}