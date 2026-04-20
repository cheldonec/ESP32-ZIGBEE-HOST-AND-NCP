#ifndef ZBM_WEB_SERVER_H

#define ZBM_WEB_SERVER_H

#include "esp_http_server.h"
#include <stdbool.h>
#include "cJSON.h"

void start_webserver(void);
void stop_webserver(void);


// Функция отправки через web socket отдельным потоком httpd_queue_work(server_handle, ws_send_async_task, async_data)
typedef struct {
    httpd_handle_t hd;
    uint8_t *payload; 
    size_t len;
} ws_async_data_t;

void ws_send_async_task(void *arg);


// === Асинхронная отправка HTTP-ответа (аналог ws_send_async_task) ===
typedef struct {
    httpd_req_t* req;
    cJSON* response;
} http_async_resp_t;

// Сообщение для обновления атрибутов
typedef struct {
    char guid[64];              // строка GUID
    uint8_t data_type;           // тип данных
    uint8_t value[256];         // само значение (до 256 байт)
    uint16_t value_len;         // длина
} zbm_ws_update_msg_t;

// Системное уведомление
typedef struct {
    char event_type[64];     // например: "zigbee_network_up", "wifi_mode_changed"
    char message[256];       // человекочитаемое сообщение
    cJSON* data;             // дополнительные данные (может быть NULL)
} zbm_ws_sys_notify_msg_t;

bool zbm_ws_send_data_update_notify(const char* guid, uint8_t data_type, const void* value, size_t value_len);

/*
cJSON *data = cJSON_CreateObject();
cJSON_AddNumberToObject(data, "channel", zbm_coordinator.zb_radio_channel);
cJSON_AddNumberToObject(data, "pan_id", zbm_coordinator.zb_pan_id);
zbm_ws_send_sys_notify("zigbee_network_up", "Zigbee network formed successfully", data);
{
  "event": "system_notify",
  "type": "zigbee_network_up",
  "message": "Zigbee network formed successfully",
  "data": {
    "channel": 15,
    "pan_id": 6789
  }
}

Или при смене режима Wi-Fi:

cJSON *data = cJSON_CreateObject();
cJSON_AddStringToObject(data, "mode", "ap");
zbm_ws_send_sys_notify("wifi_mode_changed", "Switched to AP mode for setup", data);
*/
bool zbm_ws_send_sys_notify(const char* event_type, const char* message, cJSON* data);

/**
 * @brief Отправить уведомление через WebSocket о срабатывании правила
 */
void ws_notify_automation_rule_fired(const char* rule_id, const char* trigger_guid);



#endif