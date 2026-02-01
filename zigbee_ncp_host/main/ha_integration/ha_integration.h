#ifndef HA_INTEGRATION_H
#define HA_INTEGRATION_H

#include "esp_err.h"
#include "zb_manager_devices.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация интеграции
esp_err_t ha_integration_init(void);



esp_err_t description_HA_Json_handler(httpd_req_t *req);

void ha_integration_update_ip_from_event(uint32_t newIP);

#ifdef __cplusplus
}
#endif

#endif