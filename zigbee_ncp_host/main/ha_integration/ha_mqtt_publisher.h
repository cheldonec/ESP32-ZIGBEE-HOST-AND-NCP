#ifndef HA_MQTT_PUBLISHER_H
#define HA_MQTT_PUBLISHER_H

#include "zb_manager_devices.h"

// локализация
typedef enum {
    HA_LANG_RU,
    HA_LANG_EN,
    HA_LANG_DE,
    HA_LANG_COUNT
} ha_language_t;

ha_language_t ha_get_current_language(void);

void ha_set_language(ha_language_t lang);

void ha_mqtt_init(void);

void ha_mqtt_stop(void);
bool mqtt_is_connected(void);
void ha_mqtt_republish_discovery_for_device(device_custom_t *dev);

// ON/OFF
void ha_mqtt_publish_on_off_state(device_custom_t* dev, endpoint_custom_t* ep);
void ha_mqtt_publish_on_off_discovery(device_custom_t *dev, endpoint_custom_t *ep);

// Температура
void ha_mqtt_publish_discovery_temperature(device_custom_t *dev, endpoint_custom_t *ep);
void ha_mqtt_publish_temperature(device_custom_t *dev, endpoint_custom_t *ep);

// Влажность
void ha_mqtt_publish_discovery_humidity(device_custom_t *dev, endpoint_custom_t *ep);
void ha_mqtt_publish_humidity(device_custom_t *dev, endpoint_custom_t *ep);

void ha_mqtt_publish_all_devices(void);

void ha_mqtt_republish_discovery_for_endpoint(device_custom_t *dev, endpoint_custom_t *ep);


void ha_device_updated(device_custom_t* dev, endpoint_custom_t* ep);

#endif
