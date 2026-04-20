#ifndef ZBM_REST_API_H

#define ZBM_REST_API_H

#include "esp_err.h"
#include "esp_http_server.h"

// вызывать при ребуте
void generate_session_token();

// типа пинга, проверка работоспособности сервера если токен сессии поменялся, значит был ребут и клиенту надо перезагрузить всё
esp_err_t zbm_rest_api_get_status_handler(httpd_req_t* req);

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


esp_err_t zbm_rest_api_post_open_close_zigbee_network_handler(httpd_req_t* req);

// === Обработчик: GET /api/get/zigbee_network/status — статус сети Zigbee ===
esp_err_t zbm_rest_api_get_zigbee_network_status_handler(httpd_req_t* req);

// === Обработчик: POST /api/zdo/active_endpoint — запрос Active Endpoint ===
esp_err_t zbm_rest_api_post_active_endpoint_handler(httpd_req_t* req);

/**
 * @brief Обработчик: POST /api/zdo/simple_desc — запрос Simple Descriptor от устройства
 * @param req HTTP-запрос
 * @return esp_err_t
 */
esp_err_t zbm_rest_api_post_simple_descriptor_handler(httpd_req_t* req);

/**
 * @brief Обработчик: POST /api/device/update_friendly_name — изменить friendly_name устройства
 * @param req HTTP-запрос
 * @return esp_err_t
 */
esp_err_t zbm_rest_api_post_update_dev_friendly_name_handler(httpd_req_t* req);

// Правила
esp_err_t zbm_rest_api_get_vars_handler(httpd_req_t* req);

esp_err_t zbm_rest_api_post_var_handler(httpd_req_t* req);
/**
 * @brief Получить список правил (кратко)
 */
esp_err_t zbm_rest_api_get_rules_handler(httpd_req_t* req);

/**
 * @brief Получить полное правило по ID
 */
esp_err_t zbm_rest_api_get_rule_by_id_handler(httpd_req_t* req);

/**
 * @brief Создать или обновить правило
 */
esp_err_t zbm_rest_api_post_rule_handler(httpd_req_t* req);

/**
 * @brief Удалить правило
 */
esp_err_t zbm_rest_api_delete_rule_handler(httpd_req_t* req);

/**
 * @brief Включить правило
 */
esp_err_t zbm_rest_api_post_rule_enable_handler(httpd_req_t* req);

/**
 * @brief Выключить правило
 */
esp_err_t zbm_rest_api_post_rule_disable_handler(httpd_req_t* req);

/**
 * @brief Ручной запуск правила
 */
// POST /api/rule/{id}/run
esp_err_t zbm_rest_api_post_rule_run_handler(httpd_req_t* req);

// === Behaviors API ===

/**
 * @brief GET /api/behaviors — получить список поведений
 */
esp_err_t zbm_rest_api_get_behaviors_handler(httpd_req_t* req);

/**
 * @brief GET /api/behavior/{id} — получить полное поведение
 */
esp_err_t zbm_rest_api_get_behavior_by_id_handler(httpd_req_t* req);

/**
 * @brief POST /api/behavior — создать или обновить поведение
 */
esp_err_t zbm_rest_api_post_behavior_handler(httpd_req_t* req);

/**
 * @brief DELETE /api/behavior/{id} — удалить поведение
 */
esp_err_t zbm_rest_api_delete_behavior_handler(httpd_req_t* req);

/**
 * @brief POST /api/behavior/{id}/enable — включить поведение
 */
esp_err_t zbm_rest_api_post_behavior_enable_handler(httpd_req_t* req);

/**
 * @brief POST /api/behavior/{id}/disable — выключить поведение
 */
esp_err_t zbm_rest_api_post_behavior_disable_handler(httpd_req_t* req);

/**
 * @brief POST /api/behavior/{id}/run — ручной запуск поведения
 */
esp_err_t zbm_rest_api_post_behavior_run_handler(httpd_req_t* req);
#endif