#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include "zb_manager_devices.h"

#ifdef __cplusplus
extern "C" {
#endif

/*========= Актуальная структура ===========*/
/*
ws_handler -> (cmd->valuestring, "get_devices")
{
  "short": 13553,
  "name": "Датчик",
  "online": true,
  "clusters": [
    {
      "type": "temperature",
      "value": 23.6,
      "unit": "°C",
      "display": "23.6 °C"
    },
    {
      "type": "humidity",
      "value": 45.2,
      "unit": "%",
      "display": "45.2 %"
    },
    {
      "type": "battery_voltage",
      "value": 3.1,
      "unit": "V",
      "display": "3.1 V"
    }
  ]
}
*/
/*!!!!!!!!!!!!!!   static void ws_async_send(void *arg) для обновления онлайн статуса*/
/*!!!!!!!!!!!!1!   static void ws_send_async_task(void *arg) для обновления состояний*/


void start_webserver(void);
void stop_webserver(void);
void ws_notify_state_changed(uint16_t short_addr, bool state);  // вызывать при обновлении online статуса

cJSON* create_device_json(device_custom_t *dev);
void ws_notify_device_update(uint16_t short_addr); // вызывать при обновлении атрибутов кластера
void ws_notify_device_update_unlocked(device_custom_t *dev); // без мьютекса

// отправка результата DiscoveryAttr на страницу
void ws_notify_discovery_result(uint16_t short_addr, uint8_t endpoint, uint16_t cluster_id,
                                const uint8_t *raw_data, uint16_t raw_len);



void ws_notify_network_status(void); // вызывать при обновлении zigbee сети (открыта/закрыта)

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_H
