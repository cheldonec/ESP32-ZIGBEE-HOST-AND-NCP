#include "ha_integration.h"
#include "esp_log.h"
#include "ha_config.h"
#include "ha_mqtt_publisher.h"
#include "lwip/inet.h"
#include "ha_rest_api.h"

static const char* TAG = "HA_INTEGRATION";
static char current_ip[16] = "192.168.4.1";

esp_err_t ha_integration_init(void) {
#if HA_MQTT_PUBLISHER_ENABLED
    ha_mqtt_init();
#endif

#if HA_REST_API_ENABLED
    ha_rest_api_init();
#endif

    return ESP_OK;
}



/*JSON*/
/*
{
  "name": "ESP32 Zigbee Gateway",
  "manufacturer": "CheldonecCo",
  "model": "Zigbee NCP Host",
  "sw_version": "1.0",
  "configuration_url": "http://esp32-zigbee.local",
  "zeroconf": [
    {
      "type": "_http._tcp.local.",
      "name": "esp32-zigbee"
    },
    {
      "type": "_hap._tcp.local.",
      "name": "esp32-zigbee"
    }
  ],
  "ssdp": [
    {
      "st": "upnp:rootdevice",
      "manufacturer": "CheldonecCo",
      "modelName": "Zigbee NCP Host"
    },
    {
      "st": "urn:dial-multiscreen-org:service:dial:1",
      "manufacturer": "CheldonecCo",
      "modelName": "Zigbee NCP Host"
    }
  ]
}
*/
esp_err_t description_HA_Json_handler(httpd_req_t *req)
{
    // Получаем имя хоста из конфига
    const char* host_name = CONFIG_MDNS_HOST_NAME;  // например, "esp32-zigbee"

    // Формируем URL: http://esp32-zigbee.local
    char config_url[64];
    snprintf(config_url, sizeof(config_url), "http://%s.local", host_name);

    // Формируем JSON
    char json[512];
    int len = snprintf(json, sizeof(json),
        "{"
            "\"name\":\"ESP32 Zigbee Gateway\","
            "\"manufacturer\":\"CheldonecCo\","
            "\"model\":\"Zigbee NCP Host\","
            "\"sw_version\":\"1.0\","
            "\"configuration_url\":\"%s\","
            "\"zeroconf\":["
                "{"
                    "\"type\":\"_http._tcp.local.\","
                    "\"name\":\"%s\""
                "},"
                "{"
                    "\"type\":\"_hap._tcp.local.\","
                    "\"name\":\"%s\""
                "}"
            "],"
            "\"ssdp\":["
                "{"
                    "\"st\":\"upnp:rootdevice\","
                    "\"manufacturer\":\"CheldonecCo\","
                    "\"modelName\":\"Zigbee NCP Host\""
                "},"
                "{"
                    "\"st\":\"urn:dial-multiscreen-org:service:dial:1\","
                    "\"manufacturer\":\"CheldonecCo\","
                    "\"modelName\":\"Zigbee NCP Host\""
                "}"
            "]"
        "}",
        config_url, host_name, host_name);

    if (len < 0 || len >= sizeof(json)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON too long");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_send(req, json, len);
    return ESP_OK;
}


void ha_integration_update_ip_from_event(uint32_t newIP)
{
     const char* new_ip = inet_ntoa(newIP);
    if (strcmp(current_ip, new_ip) != 0) {
        strlcpy(current_ip, new_ip, sizeof(current_ip));
        ESP_LOGI(TAG, "HA Integration: IP updated to %s", current_ip);
    }
}

