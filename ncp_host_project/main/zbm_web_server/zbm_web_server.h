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

typedef struct {
    char guid[64];              // строка GUID
    uint8_t type;               // тип атрибута/репорта (Zigbee ZCL type)
    uint8_t value[256];         // само значение (до 256 байт)
    uint16_t value_len;         // длина
} zbm_ws_update_msg_t;


bool zbm_ws_send_update(const char* guid, uint8_t type, const void* value, size_t value_len);

#endif