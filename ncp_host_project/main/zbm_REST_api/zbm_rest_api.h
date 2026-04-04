#ifndef ZBM_REST_API_H

#define ZBM_REST_API_H

#include "esp_err.h"
#include "esp_http_server.h"

// === Обработчик: GET /api/devices — список всех устройств ===
//http://192.168.4.1/api/devices
esp_err_t zbm_rest_api_get_devices_handler(httpd_req_t* req);

// === Обработчик: GET /api/device/by_short?addr=0x1234 — по короткому адресу ===
//http://192.168.4.1/api/device/by_short?addr=0x1234
esp_err_t zbm_rest_api_get_device_by_short_handler(httpd_req_t* req);

// === Обработчик: GET /api/device/by_ieee?ieee=00:11:22... — по IEEE ===
//http://192.168.4.1/api/device/by_ieee?ieee=DE:AD:BE:EF:00:01:02:03
esp_err_t zbm_rest_api_get_device_by_ieee_handler(httpd_req_t* req);

/**
 * @brief Обработчик: GET /api/coordinator — получить информацию о координаторе
 * @param req HTTP-запрос
 * @return esp_err_t
 */
esp_err_t zbm_rest_api_get_coordinator_handler(httpd_req_t* req);

/**
 * @brief Обработчик: POST /api/coordinator — обновить параметры координатора (например, имя)
 * @param req HTTP-запрос
 * @return esp_err_t
 */
esp_err_t zbm_rest_api_post_coordinator_handler(httpd_req_t* req);

#endif