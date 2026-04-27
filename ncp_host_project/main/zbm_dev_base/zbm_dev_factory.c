#include "zbm_dev_types.h"
#include "zbm_core_sync.h"
#include "string.h"
#include <stdlib.h>
#include <stdio.h>
#include <esp_log.h>
#include "ps_ram_utils.h"


zbm_dev_endpoint_t* zbm_create_empty_endpoint(void)
{
    zbm_dev_endpoint_t* ep = (zbm_dev_endpoint_t*)calloc(1, sizeof(zbm_dev_endpoint_t));
    if (!ep) return NULL;

    // Инициализация полей
    ep->is_use_on_device = 0;
    ep->id = 0;
    ep->friendlyname = NULL;
    ep->device_id = ZBM_DEVICE_TYPE_CUSTOM_ATTRIBUTES; // значение по умолчанию
    ep->standart_cluster_count = 0;
    ep->standart_cluster_array = NULL;
    ep->custom_cluster_count = 0;
    ep->custom_cluster_array = NULL;

    return ep;
}

zbm_dev_endpoint_t* zbm_find_endpoint_by_id(zbm_dev_t* dev, uint8_t endpoint_id) {
    if (!dev) return NULL;
    for (uint8_t i = 0; i < dev->endpoints_count; i++) {
        if (dev->endpoints_array[i] && dev->endpoints_array[i]->id == endpoint_id) {
            return dev->endpoints_array[i];
        }
    }
    return NULL;
}

zbm_standart_cluster_t* zbm_find_standard_cluster_by_id(zbm_dev_endpoint_t* ep, uint16_t cluster_id) {
    if (!ep) return NULL;
    for (int i = 0; i < ep->standart_cluster_count; i++) {
        if (ep->standart_cluster_array[i] && ep->standart_cluster_array[i]->id == cluster_id) {
            return ep->standart_cluster_array[i];
        }
    }
    return NULL;
}

zbm_dev_t* zbm_create_device_obj_by_ieee(const uint8_t* ieee_addr)
{
    if (!ieee_addr) return NULL;

    zbm_dev_t* dev = (zbm_dev_t*)calloc(1, sizeof(zbm_dev_t));
    if (!dev) return NULL;

    // Инициализация полей
    dev->index_in_array = 0;
    dev->friendly_name = NULL;
    dev->short_addr = 0xFFFE;                 // Unknown short address
    dev->parent_short_addr = 0xFFFE;          // Invalid
    memcpy(dev->ieee_addr, ieee_addr, 8);
    dev->capability = 0;
    dev->lqi = 0;
    dev->last_seen_ms = 0;
    dev->device_timeout_ms = 15000;
    dev->is_online = false;
    dev->manufacturer_code = 0;
    dev->has_pending_read = false;
    dev->has_pending_response = false;
    dev->last_pending_read_ms = 0;
    dev->endpoints_count = 0;
    dev->endpoints_array = NULL;
    dev->last_guid_update_short_addr = 0xffff;
    dev->device_registered_status = 0;

    // Генерим имя: "Device D0:CF:5E:XX:XX:XX:XX:XX"
    char temp_name[32];
    snprintf(temp_name, sizeof(temp_name), "Dev %02X:%02X..%02X",
             ieee_addr[0], ieee_addr[1], ieee_addr[7]);

    dev->friendly_name = psram_strdup(temp_name);
    if (!dev->friendly_name) {
        free(dev);
        return NULL;
    }

    return dev;
}


bool zbm_free_dev_endpoint(zbm_dev_endpoint_t* endpoint)
{
    if (!endpoint) return false;

    if (endpoint->friendlyname) {
        heap_caps_free(endpoint->friendlyname);
        endpoint->friendlyname = NULL;
    }

    if (endpoint->standart_cluster_array && endpoint->standart_cluster_count > 0) {
        for (int i = 0; i < endpoint->standart_cluster_count; i++) {
            zbm_standart_cluster_t* cluster = endpoint->standart_cluster_array[i];
            if (cluster) {
                zbm_free_standart_cluster(cluster);
                endpoint->standart_cluster_array[i] = NULL;
            }
        }
        free(endpoint->standart_cluster_array);
        endpoint->standart_cluster_array = NULL;
    }
    endpoint->standart_cluster_count = 0;

    if (endpoint->custom_cluster_array && endpoint->custom_cluster_count > 0) {
        for (int i = 0; i < endpoint->custom_cluster_count; i++) {
            zbm_custom_cluster_t* cluster = endpoint->custom_cluster_array[i];
            if (cluster) {
                zbm_free_custom_cluster(cluster);
                endpoint->custom_cluster_array[i] = NULL;
            }
        }
        free(endpoint->custom_cluster_array);
        endpoint->custom_cluster_array = NULL;
    }
    endpoint->custom_cluster_count = 0;

    return true;
}

bool zbm_free_dev_t(zbm_dev_t* dev)
{
    if (!dev) return false;

    // Шаг 1: Отписываем все атрибуты из GUID DB
    if (dev->endpoints_array && dev->endpoints_count > 0) {
        for (uint8_t i = 0; i < dev->endpoints_count; i++) {
            zbm_dev_endpoint_t* ep = dev->endpoints_array[i];
            if (!ep) continue;

            // Стандартные кластеры
            if (ep->standart_cluster_array && ep->standart_cluster_count > 0) {
                for (int j = 0; j < ep->standart_cluster_count; j++) {
                    zbm_standart_cluster_t* cluster = ep->standart_cluster_array[j];
                    if (!cluster || !cluster->attr_array) continue;

                    for (int k = 0; k < cluster->attr_count; k++) {
                        zbm_cluster_attribute_t* attr = cluster->attr_array[k];
                        if (attr) {
                            zbm_guid_db_unregister_by_attr_ptr(attr);
                        }
                    }
                }
            }

            // Кастомные кластеры
            if (ep->custom_cluster_array && ep->custom_cluster_count > 0) {
                for (int j = 0; j < ep->custom_cluster_count; j++) {
                    zbm_custom_cluster_t* cluster = ep->custom_cluster_array[j];
                    if (!cluster || !cluster->attr_array) continue;

                    for (int k = 0; k < cluster->attr_count; k++) {
                        zbm_cluster_attribute_t* attr = cluster->attr_array[k];
                        if (attr) {
                            zbm_guid_db_unregister_by_attr_ptr(attr);
                        }
                    }
                }
            }
        }
    }

    // Шаг 2: Освобождение памяти
    if (dev->friendly_name) {
        free(dev->friendly_name);
        dev->friendly_name = NULL;
    }

    if (dev->endpoints_array && dev->endpoints_count > 0) {
        for (uint8_t i = 0; i < dev->endpoints_count; i++) {
            if (dev->endpoints_array[i]) {
                zbm_free_dev_endpoint(dev->endpoints_array[i]);
                dev->endpoints_array[i] = NULL;
            }
        }
        free(dev->endpoints_array);
        dev->endpoints_array = NULL;
    }
    dev->endpoints_count = 0;

    // Шаг 3: Освобождение самого устройства
    free(dev);
    return true;
}

const char* get_device_type_name(zbm_device_type_t type) {
    switch (type) {
        case ZBM_DEVICE_TYPE_ON_OFF_SWITCH:                 return "ON_OFF_SWITCH";
        case ZBM_DEVICE_TYPE_LEVEL_CONTROL_SWITCH:          return "LEVEL_CONTROL_SWITCH";
        case ZBM_DEVICE_TYPE_ON_OFF_OUTPUT:                 return "ON_OFF_OUTPUT";
        case ZBM_DEVICE_TYPE_LEVEL_CONTROLLABLE_OUTPUT:     return "LEVEL_CONTROLLABLE_OUTPUT";
        case ZBM_DEVICE_TYPE_SCENE_SELECTOR:                return "SCENE_SELECTOR";
        case ZBM_DEVICE_TYPE_CONFIGURATION_TOOL:            return "CONFIGURATION_TOOL";
        case ZBM_DEVICE_TYPE_REMOTE_CONTROL:                return "REMOTE_CONTROL";
        case ZBM_DEVICE_TYPE_COMBINED_INTERFACE:            return "COMBINED_INTERFACE";
        case ZBM_DEVICE_TYPE_RANGE_EXTENDER:                return "RANGE_EXTENDER";
        case ZBM_DEVICE_TYPE_MAINS_POWER_OUTLET:            return "MAINS_POWER_OUTLET";
        case ZBM_DEVICE_TYPE_DOOR_LOCK_CLIENT:              return "DOOR_LOCK_CLIENT";
        case ZBM_DEVICE_TYPE_DOOR_LOCK_CONTROLLER:          return "DOOR_LOCK_CONTROLLER";
        case ZBM_DEVICE_TYPE_SIMPLE_SENSOR:                 return "SIMPLE_SENSOR";
        case ZBM_DEVICE_TYPE_CONSUMPTION_AWARENESS:         return "CONSUMPTION_AWARENESS";
        case ZBM_DEVICE_TYPE_HOME_GATEWAY:                  return "HOME_GATEWAY";
        case ZBM_DEVICE_TYPE_SMART_PLUG:                    return "SMART_PLUG";
        case ZBM_DEVICE_TYPE_WHITE_GOODS:                   return "WHITE_GOODS";
        case ZBM_DEVICE_TYPE_METER_INTERFACE:               return "METER_INTERFACE";
        case ZBM_DEVICE_TYPE_ON_OFF_LIGHT:                  return "ON_OFF_LIGHT";
        case ZBM_DEVICE_TYPE_DIMMABLE_LIGHT:                return "DIMMABLE_LIGHT";
        case ZBM_DEVICE_TYPE_COLOR_DIMMABLE_LIGHT:          return "COLOR_DIMMABLE_LIGHT";
        case ZBM_DEVICE_TYPE_DIMMER_SWITCH:                 return "DIMMER_SWITCH";
        case ZBM_DEVICE_TYPE_COLOR_DIMMER_SWITCH:           return "COLOR_DIMMER_SWITCH";
        case ZBM_DEVICE_TYPE_LIGHT_SENSOR:                  return "LIGHT_SENSOR";
        case ZBM_DEVICE_TYPE_SHADE:                         return "SHADE";
        case ZBM_DEVICE_TYPE_SHADE_CONTROLLER:              return "SHADE_CONTROLLER";
        case ZBM_DEVICE_TYPE_WINDOW_COVERING:               return "WINDOW_COVERING";
        case ZBM_DEVICE_TYPE_WINDOW_COVERING_CONTROLLER:    return "WINDOW_COVERING_CONTROLLER";
        case ZBM_DEVICE_TYPE_HEATING_COOLING_UNIT:          return "HEATING_COOLING_UNIT";
        case ZBM_DEVICE_TYPE_THERMOSTAT:                    return "THERMOSTAT";
        case ZBM_DEVICE_TYPE_TEMPERATURE_SENSOR:            return "TEMPERATURE_SENSOR";
        case ZBM_DEVICE_TYPE_IAS_CONTROL_INDICATING_EQUIP:  return "IAS_CONTROL_INDICATING_EQUIP";
        case ZBM_DEVICE_TYPE_IAS_ANCILLARY_CONTROL_EQUIP:   return "IAS_ANCILLARY_CONTROL_EQUIP";
        case ZBM_DEVICE_TYPE_IAS_ZONE:                      return "IAS_ZONE";
        case ZBM_DEVICE_TYPE_IAS_WARNING_DEVICE:            return "IAS_WARNING_DEVICE";
        case ZBM_DEVICE_TYPE_TEST:                          return "TEST";
        case ZBM_DEVICE_TYPE_CUSTOM_TUNNEL:                 return "CUSTOM_TUNNEL";
        case ZBM_DEVICE_TYPE_CUSTOM_ATTRIBUTES:             return "CUSTOM_ATTRIBUTES";
        default: return "UNKNOWN_DEVICE_TYPE";
    }
}

// ... предыдущий код ...

zbm_device_type_t get_device_type_by_name(const char* name) {
    if (!name) return ZBM_DEVICE_TYPE_CUSTOM_ATTRIBUTES;

    if (strcmp(name, "ON_OFF_SWITCH") == 0) return ZBM_DEVICE_TYPE_ON_OFF_SWITCH;
    else if (strcmp(name, "LEVEL_CONTROL_SWITCH") == 0) return ZBM_DEVICE_TYPE_LEVEL_CONTROL_SWITCH;
    else if (strcmp(name, "ON_OFF_OUTPUT") == 0) return ZBM_DEVICE_TYPE_ON_OFF_OUTPUT;
    else if (strcmp(name, "LEVEL_CONTROLLABLE_OUTPUT") == 0) return ZBM_DEVICE_TYPE_LEVEL_CONTROLLABLE_OUTPUT;
    else if (strcmp(name, "SCENE_SELECTOR") == 0) return ZBM_DEVICE_TYPE_SCENE_SELECTOR;
    else if (strcmp(name, "CONFIGURATION_TOOL") == 0) return ZBM_DEVICE_TYPE_CONFIGURATION_TOOL;
    else if (strcmp(name, "REMOTE_CONTROL") == 0) return ZBM_DEVICE_TYPE_REMOTE_CONTROL;
    else if (strcmp(name, "COMBINED_INTERFACE") == 0) return ZBM_DEVICE_TYPE_COMBINED_INTERFACE;
    else if (strcmp(name, "RANGE_EXTENDER") == 0) return ZBM_DEVICE_TYPE_RANGE_EXTENDER;
    else if (strcmp(name, "MAINS_POWER_OUTLET") == 0) return ZBM_DEVICE_TYPE_MAINS_POWER_OUTLET;
    else if (strcmp(name, "DOOR_LOCK_CLIENT") == 0) return ZBM_DEVICE_TYPE_DOOR_LOCK_CLIENT;
    else if (strcmp(name, "DOOR_LOCK_CONTROLLER") == 0) return ZBM_DEVICE_TYPE_DOOR_LOCK_CONTROLLER;
    else if (strcmp(name, "SIMPLE_SENSOR") == 0) return ZBM_DEVICE_TYPE_SIMPLE_SENSOR;
    else if (strcmp(name, "CONSUMPTION_AWARENESS") == 0) return ZBM_DEVICE_TYPE_CONSUMPTION_AWARENESS;
    else if (strcmp(name, "HOME_GATEWAY") == 0) return ZBM_DEVICE_TYPE_HOME_GATEWAY;
    else if (strcmp(name, "SMART_PLUG") == 0) return ZBM_DEVICE_TYPE_SMART_PLUG;
    else if (strcmp(name, "WHITE_GOODS") == 0) return ZBM_DEVICE_TYPE_WHITE_GOODS;
    else if (strcmp(name, "METER_INTERFACE") == 0) return ZBM_DEVICE_TYPE_METER_INTERFACE;
    else if (strcmp(name, "ON_OFF_LIGHT") == 0) return ZBM_DEVICE_TYPE_ON_OFF_LIGHT;
    else if (strcmp(name, "DIMMABLE_LIGHT") == 0) return ZBM_DEVICE_TYPE_DIMMABLE_LIGHT;
    else if (strcmp(name, "COLOR_DIMMABLE_LIGHT") == 0) return ZBM_DEVICE_TYPE_COLOR_DIMMABLE_LIGHT;
    else if (strcmp(name, "DIMMER_SWITCH") == 0) return ZBM_DEVICE_TYPE_DIMMER_SWITCH;
    else if (strcmp(name, "COLOR_DIMMER_SWITCH") == 0) return ZBM_DEVICE_TYPE_COLOR_DIMMER_SWITCH;
    else if (strcmp(name, "LIGHT_SENSOR") == 0) return ZBM_DEVICE_TYPE_LIGHT_SENSOR;
    else if (strcmp(name, "SHADE") == 0) return ZBM_DEVICE_TYPE_SHADE;
    else if (strcmp(name, "SHADE_CONTROLLER") == 0) return ZBM_DEVICE_TYPE_SHADE_CONTROLLER;
    else if (strcmp(name, "WINDOW_COVERING") == 0) return ZBM_DEVICE_TYPE_WINDOW_COVERING;
    else if (strcmp(name, "WINDOW_COVERING_CONTROLLER") == 0) return ZBM_DEVICE_TYPE_WINDOW_COVERING_CONTROLLER;
    else if (strcmp(name, "HEATING_COOLING_UNIT") == 0) return ZBM_DEVICE_TYPE_HEATING_COOLING_UNIT;
    else if (strcmp(name, "THERMOSTAT") == 0) return ZBM_DEVICE_TYPE_THERMOSTAT;
    else if (strcmp(name, "TEMPERATURE_SENSOR") == 0) return ZBM_DEVICE_TYPE_TEMPERATURE_SENSOR;
    else if (strcmp(name, "IAS_CONTROL_INDICATING_EQUIP") == 0) return ZBM_DEVICE_TYPE_IAS_CONTROL_INDICATING_EQUIP;
    else if (strcmp(name, "IAS_ANCILLARY_CONTROL_EQUIP") == 0) return ZBM_DEVICE_TYPE_IAS_ANCILLARY_CONTROL_EQUIP;
    else if (strcmp(name, "IAS_ZONE") == 0) return ZBM_DEVICE_TYPE_IAS_ZONE;
    else if (strcmp(name, "IAS_WARNING_DEVICE") == 0) return ZBM_DEVICE_TYPE_IAS_WARNING_DEVICE;
    else if (strcmp(name, "TEST") == 0) return ZBM_DEVICE_TYPE_TEST;
    else if (strcmp(name, "CUSTOM_TUNNEL") == 0) return ZBM_DEVICE_TYPE_CUSTOM_TUNNEL;
    else if (strcmp(name, "CUSTOM_ATTRIBUTES") == 0) return ZBM_DEVICE_TYPE_CUSTOM_ATTRIBUTES;
    else return ZBM_DEVICE_TYPE_CUSTOM_ATTRIBUTES; // значение по умолчанию для неизвестных
}