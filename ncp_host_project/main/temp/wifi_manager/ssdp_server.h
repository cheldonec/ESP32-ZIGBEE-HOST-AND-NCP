/*
  ssdp.h - Merged SSDP server for ESP32
  Based on Luc Lebosse & Lyxt
*/

#ifndef ESP_SSDP_H_
#define ESP_SSDP_H_

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdint.h>
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
typedef struct {
    unsigned task_priority;
    size_t stack_size;
    BaseType_t core_id;
    uint8_t ttl;
    uint16_t port;
    uint32_t interval;          // NOTIFY interval (ms)
    uint16_t mx_max_delay;      // max MX delay (ms)
    const char* uuid_root;      // not used, we generate full UUID
    const char* uuid;           // not used
    const char* schema_url;     // e.g. "description.xml"
    const char* device_type;    // e.g. "upnp:rootdevice"
    const char* friendly_name;
    const char* serial_number;
    const char* presentation_url;
    const char* manufacturer_name;
    const char* manufacturer_url;
    const char* model_name;
    const char* model_url;
    const char* model_number;
    const char* model_description;
    const char* server_name;
    const char* services_description;
    const char* icons_description;
} ssdp_config_t;


#define SDDP_DEFAULT_CONFIG()                                               \
    {                                                                       \
        .task_priority = tskIDLE_PRIORITY + 5,                              \
        .stack_size = 4096,                                                 \
        .core_id = tskNO_AFFINITY,                                          \
        .ttl = 2,                                                           \
        .port = 80,                                                         \
        .interval = 1200000,                                                \
        .mx_max_delay = 5000,                                               \
        .uuid_root = NULL,                                                  \
        .uuid = NULL,                                                       \
        .schema_url = "description.xml",                                    \
        .device_type = "upnp:rootdevice",                                   \
        .friendly_name = "ESP32 Zigbee Gateway",                            \
        .serial_number = "00000001",                                        \
        .presentation_url = "/",                                            \
        .manufacturer_name = "Lyxt",                                        \
        .manufacturer_url = "https://github.com/cheldonec",                 \
        .model_name = "Zigbee NCP Host",                                    \
        .model_url = "https://github.com/cheldonec",                        \
        .model_number = "1.0",                                              \
        .model_description = "ESP32 Zigbee Gateway",                        \
        .server_name = "ESP32/Zigbee-Gateway v1.0",                         \
        .services_description = NULL,                                       \
        .icons_description = NULL                                           \
    }
*/
//esp_err_t ssdp_init(void);



void start_ssdp_server(void);

void ssdp_update_ip_from_event(uint32_t newIP); // просто обновляет IP
void update_ip_from_event_extern_call_and_send_notify(uint32_t newIP);
void stop_ssdp_server(void);
esp_err_t description_xml_handler(httpd_req_t *req);
//void ssdp_update_ip(const char* ip);  // вызывается из IP_EVENT

#ifdef __cplusplus
}
#endif

#endif /* ESP_SSDP_H_ */
