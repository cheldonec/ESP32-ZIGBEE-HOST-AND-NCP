#include "ha_mqtt_publisher.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "ha_config.h"
#include "zb_manager_devices.h"
#include "zb_manager.h"
#include "zb_manager_main_config.h"

static const char* TAG = "ha_mqtt";

static esp_mqtt_client_handle_t client = NULL;
static bool connected_state = false;

// === Локализация ===



#ifndef HA_LANGUAGE_DEFAULT
#define HA_LANGUAGE_DEFAULT HA_LANG_RU
#endif

static ha_language_t current_language = HA_LANGUAGE_DEFAULT;

ha_language_t ha_get_current_language(void) {
    return current_language;
}

void ha_set_language(ha_language_t lang) {
    if (lang < HA_LANG_COUNT) {
        current_language = lang;
    }
}

typedef struct {
    const char* temperature;
    const char* humidity;
    const char* switch_;
} ha_translation_t;

const ha_translation_t ha_translations[HA_LANG_COUNT] = {
    [HA_LANG_RU] = {
        .temperature = "Температура",
        .humidity = "Влажность",
        .switch_ = "Выключатель"
    },
    [HA_LANG_EN] = {
        .temperature = "Temperature",
        .humidity = "Humidity",
        .switch_ = "Switch"
    },
    [HA_LANG_DE] = {
        .temperature = "Temperatur",
        .humidity = "Luftfeuchtigkeit",
        .switch_ = "Schalter"
    }
};

#define TR_TEMP(lang) (ha_translations[lang].temperature)
#define TR_HUM(lang)  (ha_translations[lang].humidity)
#define TR_SWITCH(lang) (ha_translations[lang].switch_)

// Публикация состояния устройства
void ha_mqtt_publish_on_off_state(device_custom_t* dev, endpoint_custom_t* ep) {
    if (!client || !mqtt_is_connected() || !ep->server_OnOffClusterObj) return;

    char topic[64];
    HA_MQTT_ON_OFF_STATE_TOPIC(topic, dev->short_addr, ep->ep_id);

    const char* payload = ep->server_OnOffClusterObj->on_off ? "ON" : "OFF";

    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    if (msg_id > 0) {
        ESP_LOGI(TAG, "State published: %s → %s (msg_id=%d)", topic, payload, msg_id);
    } else {
        ESP_LOGE(TAG, "Failed to publish state: %s", topic);
    }
}

// Публикация discovery для OnOff
void ha_mqtt_publish_on_off_discovery(device_custom_t *dev, endpoint_custom_t *ep) {
    if (!client || !mqtt_is_connected() || !ep->is_use_on_off_cluster) return;

    char unique_id[64];
    HA_MQTT_UNIQUE_ID(unique_id, dev->short_addr, ep->ep_id);

    char object_id[64];
    snprintf(object_id, sizeof(object_id), "esp32_zb_%04x_ep%d_switch", dev->short_addr, ep->ep_id);

    char state_topic[64], command_topic[64], discovery_topic[128];
    HA_MQTT_ON_OFF_STATE_TOPIC(state_topic, dev->short_addr, ep->ep_id);
    HA_MQTT_COMMAND_TOPIC(command_topic, dev->short_addr, ep->ep_id);
    HA_MQTT_DISCOVERY_TOPIC(discovery_topic, "switch", unique_id);

    cJSON *config = cJSON_CreateObject();

    ha_language_t lang = ha_get_current_language();
    const char* base_name = ep->friendly_name[0] ? ep->friendly_name : TR_SWITCH(lang);
    char full_name[128] = {0};

    // Для OnOff не добавляем суффикс, если только один кластер
    int cluster_count = 0;
    if (ep->is_use_on_off_cluster) cluster_count++;
    if (ep->is_use_temperature_measurement_cluster) cluster_count++;
    if (ep->is_use_humidity_measurement_cluster) cluster_count++;

    if (cluster_count > 1 && ep->friendly_name[0]) {
        snprintf(full_name, sizeof(full_name), "%s (%s)", ep->friendly_name, TR_SWITCH(lang));
    } else {
        snprintf(full_name, sizeof(full_name), "%s", base_name);
    }

    cJSON_AddStringToObject(config, "name", full_name);
    cJSON_AddStringToObject(config, "object_id", object_id);
    cJSON_AddStringToObject(config, "state_topic", state_topic);
    cJSON_AddStringToObject(config, "command_topic", command_topic);
    cJSON_AddStringToObject(config, "payload_on", "ON");
    cJSON_AddStringToObject(config, "payload_off", "OFF");
    cJSON_AddBoolToObject(config, "retain", true);
    cJSON_AddStringToObject(config, "unique_id", unique_id);
    cJSON_AddStringToObject(config, "icon", "mdi:light-switch");  // ✅ Иконка

    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "name", "ESP32 Zigbee Gateway");
    cJSON_AddStringToObject(device, "model", "ESP32-ZB-GW");
    cJSON_AddStringToObject(device, "manufacturer", "CheldonecCo");
    cJSON *ids = cJSON_CreateArray();
    cJSON_AddItemToArray(ids, cJSON_CreateString("esp32_zb_gateway"));
    cJSON_AddItemToObject(device, "identifiers", ids);
    cJSON_AddItemToObject(config, "device", device);

    char *config_str = cJSON_PrintUnformatted(config);
    if (config_str) {
        int msg_id = esp_mqtt_client_publish(client, discovery_topic, config_str, 0, 1, true);
        if (msg_id > 0) {
            ESP_LOGI(TAG, "Discovery published: %s (msg_id=%d)", discovery_topic, msg_id);
        } else {
            ESP_LOGE(TAG, "Failed to publish discovery: %s", discovery_topic);
        }
        free(config_str);
    } else {
        ESP_LOGE(TAG, "Failed to print discovery JSON");
    }

    cJSON_Delete(config);
}

// === Температура ===
void ha_mqtt_publish_temperature(device_custom_t *dev, endpoint_custom_t *ep) {
    if (!client || !mqtt_is_connected() || !ep->server_TemperatureMeasurementClusterObj) return;

    char topic[64];
    HA_MQTT_TEMP_STATE_TOPIC(topic, dev->short_addr, ep->ep_id);

    int16_t raw = ep->server_TemperatureMeasurementClusterObj->measured_value;
    float temp = raw / 100.0f;

    char payload[16];
    snprintf(payload, sizeof(payload), "%.1f", temp);

    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    if (msg_id > 0) {
        ESP_LOGI(TAG, "✅ Temperature published: %s → %.1f°C (msg_id=%d)", topic, temp, msg_id);
    } else {
        ESP_LOGE(TAG, "❌ Failed to publish temperature: %s", topic);
    }
}

void ha_mqtt_publish_discovery_temperature(device_custom_t *dev, endpoint_custom_t *ep) {
    if (!client || !mqtt_is_connected() || !ep->is_use_temperature_measurement_cluster) return;

    char unique_id[64];
    snprintf(unique_id, sizeof(unique_id), "esp32_zb_%04x_ep%d_temp", dev->short_addr, ep->ep_id);

    char object_id[64];
    snprintf(object_id, sizeof(object_id), "esp32_zb_%04x_ep%d_temp", dev->short_addr, ep->ep_id);

    char state_topic[64], discovery_topic[128];
    HA_MQTT_TEMP_STATE_TOPIC(state_topic, dev->short_addr, ep->ep_id);
    HA_MQTT_DISCOVERY_TOPIC(discovery_topic, "sensor", unique_id);

    cJSON *config = cJSON_CreateObject();

    ha_language_t lang = ha_get_current_language();
    const char* base_name = ep->friendly_name[0] ? ep->friendly_name : TR_TEMP(lang);
    char full_name[128] = {0};

    int cluster_count = 0;
    if (ep->is_use_temperature_measurement_cluster) cluster_count++;
    if (ep->is_use_humidity_measurement_cluster) cluster_count++;
    if (ep->is_use_on_off_cluster) cluster_count++;

    if (cluster_count > 1 && ep->friendly_name[0]) {
        snprintf(full_name, sizeof(full_name), "%s (%s)", ep->friendly_name, TR_TEMP(lang));
    } else {
        snprintf(full_name, sizeof(full_name), "%s", base_name);
    }

    cJSON_AddStringToObject(config, "name", full_name);
    cJSON_AddStringToObject(config, "object_id", object_id);
    cJSON_AddStringToObject(config, "state_topic", state_topic);
    cJSON_AddStringToObject(config, "unit_of_measurement", "°C");
    cJSON_AddStringToObject(config, "device_class", "temperature");
    cJSON_AddBoolToObject(config, "retain", true);
    cJSON_AddStringToObject(config, "unique_id", unique_id);
    cJSON_AddStringToObject(config, "icon", "mdi:thermometer");  // ✅ Иконка

    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "name", "ESP32 Zigbee Gateway");
    cJSON_AddStringToObject(device, "model", "ESP32-ZB-GW");
    cJSON_AddStringToObject(device, "manufacturer", "CheldonecCo");
    cJSON *ids = cJSON_CreateArray();
    cJSON_AddItemToArray(ids, cJSON_CreateString("esp32_zb_gateway"));
    cJSON_AddItemToObject(device, "identifiers", ids);
    cJSON_AddItemToObject(config, "device", device);

    char *config_str = cJSON_PrintUnformatted(config);
    if (config_str) {
        int msg_id = esp_mqtt_client_publish(client, discovery_topic, config_str, 0, 1, true);
        if (msg_id > 0) {
            ESP_LOGI(TAG, "✅ Discovery (temperature) published: %s", discovery_topic);
        } else {
            ESP_LOGE(TAG, "❌ Failed to publish discovery (temperature): %s", discovery_topic);
        }
        free(config_str);
    } else {
        ESP_LOGE(TAG, "❌ Failed to print discovery JSON (temperature)");
    }

    cJSON_Delete(config);
}

// === Влажность ===
void ha_mqtt_publish_humidity(device_custom_t *dev, endpoint_custom_t *ep) {
    if (!client || !mqtt_is_connected() || !ep->server_HumidityMeasurementClusterObj) return;

    char topic[64];
    HA_MQTT_HUMIDITY_STATE_TOPIC(topic, dev->short_addr, ep->ep_id);

    uint16_t raw = ep->server_HumidityMeasurementClusterObj->measured_value;
    float hum = raw / 100.0f;

    char payload[16];
    snprintf(payload, sizeof(payload), "%.1f", hum);

    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    if (msg_id > 0) {
        ESP_LOGI(TAG, "✅ Humidity published: %s → %.1f%% (msg_id=%d)", topic, hum, msg_id);
    } else {
        ESP_LOGE(TAG, "❌ Failed to publish humidity: %s", topic);
    }
}

void ha_mqtt_publish_discovery_humidity(device_custom_t *dev, endpoint_custom_t *ep) {
    if (!client || !mqtt_is_connected() || !ep->is_use_humidity_measurement_cluster) return;

    char unique_id[64];
    snprintf(unique_id, sizeof(unique_id), "esp32_zb_%04x_ep%d_hum", dev->short_addr, ep->ep_id);

    char object_id[64];
    snprintf(object_id, sizeof(object_id), "esp32_zb_%04x_ep%d_hum", dev->short_addr, ep->ep_id);

    char state_topic[64], discovery_topic[128];
    HA_MQTT_HUMIDITY_STATE_TOPIC(state_topic, dev->short_addr, ep->ep_id);
    HA_MQTT_DISCOVERY_TOPIC(discovery_topic, "sensor", unique_id);

    cJSON *config = cJSON_CreateObject();

    ha_language_t lang = ha_get_current_language();
    const char* base_name = ep->friendly_name[0] ? ep->friendly_name : TR_HUM(lang);
    char full_name[128] = {0};

    int cluster_count = 0;
    if (ep->is_use_temperature_measurement_cluster) cluster_count++;
    if (ep->is_use_humidity_measurement_cluster) cluster_count++;
    if (ep->is_use_on_off_cluster) cluster_count++;

    if (cluster_count > 1 && ep->friendly_name[0]) {
        snprintf(full_name, sizeof(full_name), "%s (%s)", ep->friendly_name, TR_HUM(lang));
    } else {
        snprintf(full_name, sizeof(full_name), "%s", base_name);
    }

    cJSON_AddStringToObject(config, "name", full_name);
    cJSON_AddStringToObject(config, "object_id", object_id);
    cJSON_AddStringToObject(config, "state_topic", state_topic);
    cJSON_AddStringToObject(config, "unit_of_measurement", "%");
    cJSON_AddStringToObject(config, "device_class", "humidity");
    cJSON_AddBoolToObject(config, "retain", true);
    cJSON_AddStringToObject(config, "unique_id", unique_id);
    cJSON_AddStringToObject(config, "icon", "mdi:water-percent");  // ✅ Иконка

    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "name", "ESP32 Zigbee Gateway");
    cJSON_AddStringToObject(device, "model", "ESP32-ZB-GW");
    cJSON_AddStringToObject(device, "manufacturer", "CheldonecCo");
    cJSON *ids = cJSON_CreateArray();
    cJSON_AddItemToArray(ids, cJSON_CreateString("esp32_zb_gateway"));
    cJSON_AddItemToObject(device, "identifiers", ids);
    cJSON_AddItemToObject(config, "device", device);

    char *config_str = cJSON_PrintUnformatted(config);
    if (config_str) {
        int msg_id = esp_mqtt_client_publish(client, discovery_topic, config_str, 0, 1, true);
        if (msg_id > 0) {
            ESP_LOGI(TAG, "✅ Discovery (humidity) published: %s", discovery_topic);
        } else {
            ESP_LOGE(TAG, "❌ Failed to publish discovery (humidity): %s", discovery_topic);
        }
        free(config_str);
    } else {
        ESP_LOGE(TAG, "❌ Failed to print discovery JSON (humidity)");
    }

    cJSON_Delete(config);
}

void ha_device_updated(device_custom_t* dev, endpoint_custom_t* ep) {
#if HA_MQTT_PUBLISHER_ENABLED
    if (ep->is_use_on_off_cluster) {
        ha_mqtt_publish_on_off_state(dev, ep);
    }
    if (ep->is_use_temperature_measurement_cluster) {
        ha_mqtt_publish_temperature(dev, ep);
    }
    if (ep->is_use_humidity_measurement_cluster) {
        ha_mqtt_publish_humidity(dev, ep);
    }
#endif
}

// Публикация discovery для всех устройств
void ha_mqtt_publish_all_devices(void) {
    if (!client || !mqtt_is_connected()) return;

    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
        for (int i = 0; i < RemoteDevicesCount; i++) {
            device_custom_t *dev = RemoteDevicesArray[i];
            if (!dev || !dev->is_online) continue;

            for (int j = 0; j < dev->endpoints_count; j++) {
                endpoint_custom_t *ep = dev->endpoints_array[j];
                if (!ep) continue;

                if (ep->is_use_on_off_cluster) {
                    ha_mqtt_publish_on_off_discovery(dev, ep);
                    ha_mqtt_publish_on_off_state(dev, ep);
                }
                if (ep->is_use_temperature_measurement_cluster) {
                    ha_mqtt_publish_discovery_temperature(dev, ep);
                    ha_mqtt_publish_temperature(dev, ep);
                }
                if (ep->is_use_humidity_measurement_cluster) {
                    ha_mqtt_publish_discovery_humidity(dev, ep);
                    ha_mqtt_publish_humidity(dev, ep);
                }
            }
        }
        DEVICE_ARRAY_UNLOCK();
    }

    char subscribe_topic[64];
    snprintf(subscribe_topic, sizeof(subscribe_topic), "%s/+/+/set", HA_MQTT_BASE_TOPIC);
    int msg_id = esp_mqtt_client_subscribe(client, subscribe_topic, 0);
    ESP_LOGI(TAG, "Subscribed to commands: %s (msg_id=%d)", subscribe_topic, msg_id);
}

// Публикация статуса шлюза (online/offline)
void ha_mqtt_publish_status(const char* status) {
    if (!client || !mqtt_is_connected()) return;
    int msg_id = esp_mqtt_client_publish(client, HA_MQTT_BASE_TOPIC "/status", status, 0, 1, true);
    ESP_LOGI(TAG, "Status published: %s/%s (msg_id=%d)", HA_MQTT_BASE_TOPIC, status, msg_id);
}

// Перепубликация discovery для всех эндпоинтов устройства
void ha_mqtt_republish_discovery_for_device(device_custom_t *dev) {
    if (!client || !mqtt_is_connected()) return;

    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex in ha_mqtt_republish_discovery_for_device");
        return;
    }

    for (int i = 0; i < dev->endpoints_count; i++) {
        endpoint_custom_t *ep = dev->endpoints_array[i];
        if (!ep) continue;

        if (ep->is_use_on_off_cluster) {
            ha_mqtt_publish_on_off_discovery(dev, ep);
        }
        if (ep->is_use_temperature_measurement_cluster) {
            ha_mqtt_publish_discovery_temperature(dev, ep);
        }
        if (ep->is_use_humidity_measurement_cluster) {
            ha_mqtt_publish_discovery_humidity(dev, ep);
        }
    }

    DEVICE_ARRAY_UNLOCK();
}

// Перепубликация discovery только для одного эндпоинта
void ha_mqtt_republish_discovery_for_endpoint(device_custom_t *dev, endpoint_custom_t *ep) {
    if (!client || !mqtt_is_connected()) return;

    if (ep->is_use_on_off_cluster) {
        ha_mqtt_publish_on_off_discovery(dev, ep);
        ha_mqtt_publish_on_off_state(dev, ep);
    }
    if (ep->is_use_temperature_measurement_cluster) {
        ha_mqtt_publish_discovery_temperature(dev, ep);
        ha_mqtt_publish_temperature(dev, ep);
    }
    if (ep->is_use_humidity_measurement_cluster) {
        ha_mqtt_publish_discovery_humidity(dev, ep);
        ha_mqtt_publish_humidity(dev, ep);
    }
}

// Вызывается при подключении MQTT
void ha_mqtt_on_connect() {
    ESP_LOGI(TAG, "MQTT connected → republishing discovery for all devices");

    ha_mqtt_publish_status("online");
    ha_mqtt_publish_all_devices();
}

// Обработчик MQTT событий
static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "✅ MQTT Connected — restoring state and discovery");
            connected_state = true;
            ha_mqtt_on_connect();
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected");
            connected_state = false;
            ha_mqtt_publish_status("offline");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT Subscribed (msg_id=%d)", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT Unsubscribed (msg_id=%d)", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT Published (msg_id=%d)", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA: topic=%.*s, data=%.*s",
                    event->topic_len, event->topic,
                    event->data_len, event->data);

            // === Тестовая лампочка ===
            if (event->topic_len == strlen("esp32_zb/light/test/set") &&
                strncmp((char*)event->topic, "esp32_zb/light/test/set", event->topic_len) == 0) {
                bool on = (strncmp((char*)event->data, "ON", event->data_len) == 0);
                ESP_LOGI(TAG, "✅ Test light command: %s", on ? "ON" : "OFF");
                esp_mqtt_client_publish(client, "esp32_zb/light/test/state", on ? "ON" : "OFF", 0, 1, true);
                break;
            }

            // === Управление реальным устройством ===
            if (event->topic_len > 5 &&
                strncmp((char*)event->topic, HA_MQTT_BASE_TOPIC "/", strlen(HA_MQTT_BASE_TOPIC) + 1) == 0 &&
                strstr((char*)event->topic, "/set")) {

                uint16_t short_addr = 0;
                uint8_t ep_id = 0;
                int parsed = sscanf((char*)event->topic, HA_MQTT_BASE_TOPIC "/%hx/%hhu/set", &short_addr, &ep_id);

                if (parsed != 2) {
                    ESP_LOGW(TAG, "❌ Failed to parse topic: %.*s", event->topic_len, event->topic);
                    break;
                }

                device_custom_t *dev = zb_manager_find_device_by_short_safe(short_addr);
                if (!dev) {
                    ESP_LOGW(TAG, "❌ Device 0x%04x not found", short_addr);
                    break;
                }

                if (!dev->is_online) {
                    ESP_LOGW(TAG, "❌ Device 0x%04x is OFFLINE", short_addr);
                    break;
                }

                endpoint_custom_t *ep = NULL;
                for (int i = 0; i < dev->endpoints_count; i++) {
                    if (dev->endpoints_array[i] && dev->endpoints_array[i]->ep_id == ep_id) {
                        ep = dev->endpoints_array[i];
                        break;
                    }
                }
                if (!ep || !ep->is_use_on_off_cluster || !ep->server_OnOffClusterObj) {
                    ESP_LOGW(TAG, "❌ Endpoint %d does not support OnOff", ep_id);
                    break;
                }

                uint8_t cmd_id = 0xff;
                if (strncmp((char*)event->data, "ON", event->data_len) == 0) {
                    cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_ON_ID;
                } else if (strncmp((char*)event->data, "OFF", event->data_len) == 0) {
                    cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID;
                } else {
                    ESP_LOGW(TAG, "❌ Unknown command: %.*s", event->data_len, event->data);
                    break;
                }

                esp_zb_zcl_on_off_cmd_t on_off_cmd = {
                    .zcl_basic_cmd = {
                        .dst_addr_u.addr_short = dev->short_addr,
                        .dst_endpoint = ep->ep_id,
                        .src_endpoint = 1,
                    },
                    .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                    .on_off_cmd_id = cmd_id,
                };

                esp_err_t zb_err = zb_manager_on_off_cmd_req(&on_off_cmd);
                if (zb_err == ESP_OK) {
                    ESP_LOGI(TAG, "✅ Sent ZCL OnOff: 0x%04x (ep:%d) → %s", dev->short_addr, ep_id, cmd_id == ESP_ZB_ZCL_CMD_ON_OFF_ON_ID ? "ON" : "OFF");
                    vTaskDelay(pdMS_TO_TICKS(100));

                    if (dev->manufacturer_code == 4447) {
                        ESP_LOGI(TAG, "🔍 Device is Lumi/Aqara (manuf: %d) → forcing ReadAttr to sync state", dev->manufacturer_code);
                        uint8_t tsn = zb_manager_read_on_off_attribute(dev->short_addr, ep->ep_id);
                        if (tsn == 0xff) {
                            ESP_LOGW(TAG, "❌ Failed to send Read Attribute for OnOff to 0x%04x (ep: %d)", dev->short_addr, ep->ep_id);
                        } else {
                            ESP_LOGI(TAG, "✅ Read Attribute (OnOff) sent with TSN=%d", tsn);
                        }
                    } else {
                        ESP_LOGD(TAG, "💡 Device manuf: %d → skipping ReadAttr, relying on reporting", dev->manufacturer_code);
                    }
                } else {
                    ESP_LOGE(TAG, "❌ Failed to send ZCL command: %d", zb_err);
                }

                vTaskDelay(pdMS_TO_TICKS(500));
                ha_mqtt_publish_on_off_state(dev, ep);

                break;
            }
            ESP_LOGD(TAG, "🔁 Unknown topic: %.*s", event->topic_len, event->topic);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT Event Error");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "TCP Error: errno=%d", event->error_handle->esp_transport_sock_errno);
            } else {
                ESP_LOGE(TAG, "MQTT Connection refused: 0x%x", event->error_handle->connect_return_code);
            }
            break;

        default:
            ESP_LOGD(TAG, "MQTT Event: %d", event_id);
            break;
    }
}

// Инициализация MQTT клиента
void ha_mqtt_init(void) {
    // Проверяем, включена ли интеграция и MQTT
    if (!g_app_config.ha_config.ha_enabled || !g_app_config.ha_config.mqtt.mqtt_enabled) {
        ESP_LOGI(TAG, "HA or MQTT is disabled in config → skip init");
        return;
    }

    // Формируем URI: mqtt://[user:pass@]host:port
    char uri[255] = {0};

    if (g_app_config.ha_config.mqtt.username[0] && g_app_config.ha_config.mqtt.password[0]) {
        snprintf(uri, sizeof(uri), "mqtt://%s:%s@%s:%u",
                 g_app_config.ha_config.mqtt.username,
                 g_app_config.ha_config.mqtt.password,
                 g_app_config.ha_config.mqtt.broker,
                 g_app_config.ha_config.mqtt.port);
    } else {
        snprintf(uri, sizeof(uri), "mqtt://%s:%u",
                 g_app_config.ha_config.mqtt.broker,
                 g_app_config.ha_config.mqtt.port);
    }

    ESP_LOGI(TAG, "MQTT Connecting to: %s", uri);

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .credentials.client_id = "esp32_zb_gateway",
        .network.timeout_ms = 5000,
        .session.keepalive = 60,
        .network.disable_auto_reconnect = false,  // авто-переподключение включено
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_err_t err = esp_mqtt_client_start(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "MQTT client started successfully");
    } else {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
    }
}


// === Остановка MQTT клиента ===
void ha_mqtt_stop(void) {
    if (!client) {
        ESP_LOGW(TAG, "MQTT client is not initialized, nothing to stop");
        return;
    }

    // 1. Публикуем статус "offline", если ещё подключены
    if (connected_state) {
        ha_mqtt_publish_status("offline");
        ESP_LOGI(TAG, "Published offline status");
    }

    // 2. Останавливаем клиента
    esp_err_t err = esp_mqtt_client_stop(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "MQTT client stopped successfully");
    } else {
        ESP_LOGE(TAG, "Failed to stop MQTT client: %s", esp_err_to_name(err));
    }

    // 3. Освобождаем ресурс
    esp_mqtt_client_destroy(client);
    client = NULL;
    connected_state = false;

    ESP_LOGI(TAG, "HA MQTT client stopped and destroyed");
}

// Проверка подключения
bool mqtt_is_connected(void) {
    return connected_state && client;
}
