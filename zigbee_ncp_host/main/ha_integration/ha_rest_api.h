#ifndef HA_REST_API_H

#define HA_REST_API_H

#include "esp_http_server.h"
#include "esp_err.h"
void ha_rest_api_init(void);

// не используется пока
esp_err_t ha_devices_handler(httpd_req_t *req);

#endif