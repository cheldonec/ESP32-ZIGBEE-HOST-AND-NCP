#ifndef HA_CONFIG_H

#define HA_CONFIG_H

// Включение/отключение режимов
#define HA_INTEGRATION_ENABLED          1
#define HA_MQTT_PUBLISHER_ENABLED       1
#define HA_REST_API_ENABLED             1

// Настройки MQTT
//#define HA_MQTT_BROKER                  "mqtt://cheldonec:Ruslan%1935@homeassistant.local:1883"
//#define HA_MQTT_PORT                    1883
//#define HA_MQTT_CLIENT_ID               "esp32_zigbee_gw"
//#define HA_MQTT_USER                    "cheldonec"
//#define HA_MQTT_PASS                    "Ruslan%1935"


// Базовый префикс для всех топиков
#define HA_MQTT_BASE_TOPIC              "esp32_zb"

// Длины буферов
#define HA_MQTT_TOPIC_BUF_LEN           64
#define HA_MQTT_DISCOVERY_BUF_LEN       128

// Макросы для формирования топиков
#define HA_MQTT_ON_OFF_STATE_TOPIC(buf, short_addr, ep_id) \
    snprintf(buf, HA_MQTT_TOPIC_BUF_LEN, HA_MQTT_BASE_TOPIC "/%04x/%d/on_off/state", short_addr, ep_id)

#define HA_MQTT_TEMP_STATE_TOPIC(buf, short_addr, ep_id) \
    snprintf(buf, HA_MQTT_TOPIC_BUF_LEN, HA_MQTT_BASE_TOPIC "/%04x/%d/temp/state", short_addr, ep_id)

#define HA_MQTT_HUMIDITY_STATE_TOPIC(buf, short_addr, ep_id) \
    snprintf(buf, HA_MQTT_TOPIC_BUF_LEN, HA_MQTT_BASE_TOPIC "/%04x/%d/humidity/state", short_addr, ep_id)

#define HA_MQTT_COMMAND_TOPIC(buf, short_addr, ep_id) \
    snprintf(buf, HA_MQTT_TOPIC_BUF_LEN, HA_MQTT_BASE_TOPIC "/%04x/%d/set", short_addr, ep_id)

#define HA_MQTT_UNIQUE_ID(buf, short_addr, ep_id) \
    snprintf(buf, HA_MQTT_TOPIC_BUF_LEN, "esp32_zb_%04x_ep%d", short_addr, ep_id)

#define HA_MQTT_DISCOVERY_TOPIC(buf, type, unique_id) \
    snprintf(buf, HA_MQTT_DISCOVERY_BUF_LEN, "homeassistant/%s/%s/config", type, unique_id)

// Период обновления REST
#define HA_REST_POLL_INTERVAL_MS        5000

#endif