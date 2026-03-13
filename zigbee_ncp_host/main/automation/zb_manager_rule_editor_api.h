#ifndef ZB_MANAGER_RULE_EDITOR_API_H

#define ZB_MANAGER_RULE_EDITOR_API_H


#include "esp_err.h"
#include "esp_http_server.h"
#include "zb_manager_rules.h"

//получить активные правила
esp_err_t api_get_rules_handler(httpd_req_t *req);

//добавить правило
esp_err_t api_post_rule_handler(httpd_req_t *req);

//изменить правило
esp_err_t api_put_rule_handler(httpd_req_t *req);

//удалить правило
esp_err_t api_delete_rule_handler(httpd_req_t *req);

// удалить все правила
esp_err_t api_delete_all_rules_handler(httpd_req_t *req);
// === POST /api/rules/run/:id ===
// тестовый запуск правила в ручном режиме из браузера, условия не учитываются, просто выполнение для проверки
esp_err_t api_run_rule_handler(httpd_req_t *req);

esp_err_t api_rules_vars_handler(httpd_req_t *req);
  
#endif