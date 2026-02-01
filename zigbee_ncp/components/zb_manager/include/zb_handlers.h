#ifndef ZB_HANDLERS_H
#define ZB_HANDLERS_H
#include "esp_zigbee_core.h"
#include "sdkconfig.h"

#define HANDLER_ZB_ROLE_COORDINATOR
#define HANDLER_ZB_ROLE_ROUTER
#define HANDLER_ZB_ROLE_END_DEVICE

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct);

esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message);

esp_err_t esp_ncp_zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message);

// включаем, чтобы нашли нас
#define BDB_FIND_BIND_TARGET_DURATION (180) // 180 seconds
esp_err_t zb_bdb_enter_finding_binding_target(uint8_t ep);

// Запуск F&B как Initiator (инициатор)
esp_err_t start_finding_binding(uint8_t local_ep);

void identify_handler(uint8_t identify_on); // включение/выключение индикации
void identify_handler_req(bool identify_on); // 
#endif