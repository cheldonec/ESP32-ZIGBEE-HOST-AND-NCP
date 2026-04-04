#include "zbm_clusters_type.h"
#include <stddef.h>  //
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char* zbm_get_cluster_friendlyname(uint16_t cluster_id)
{
    switch (cluster_id) {
        case ZBM_CLUSTER_ID_BASIC:                  return "Basic";
        case ZBM_CLUSTER_ID_POWER_CONFIG:           return "Power Configuration";
        case ZBM_CLUSTER_ID_DEVICE_TEMP_CONFIG:     return "Device Temperature Configuration";
        case ZBM_CLUSTER_ID_IDENTIFY:               return "Identify";
        case ZBM_CLUSTER_ID_GROUPS:                 return "Groups";
        case ZBM_CLUSTER_ID_SCENES:                 return "Scenes";
        case ZBM_CLUSTER_ID_ON_OFF:                 return "On/Off";
        case ZBM_CLUSTER_ID_ON_OFF_SWITCH_CONFIG:   return "On/Off Switch Config";
        case ZBM_CLUSTER_ID_LEVEL_CONTROL:          return "Level Control";
        case ZBM_CLUSTER_ID_ALARMS:                 return "Alarms";
        case ZBM_CLUSTER_ID_TIME:                   return "Time";
        case ZBM_CLUSTER_ID_RSSI_LOCATION:          return "RSSI Location";
        case ZBM_CLUSTER_ID_ANALOG_INPUT:           return "Analog Input";
        case ZBM_CLUSTER_ID_ANALOG_OUTPUT:          return "Analog Output";
        case ZBM_CLUSTER_ID_ANALOG_VALUE:           return "Analog Value";
        case ZBM_CLUSTER_ID_BINARY_INPUT:           return "Binary Input";
        case ZBM_CLUSTER_ID_BINARY_OUTPUT:          return "Binary Output";
        case ZBM_CLUSTER_ID_BINARY_VALUE:           return "Binary Value";
        case ZBM_CLUSTER_ID_MULTI_INPUT:            return "Multistate Input";
        case ZBM_CLUSTER_ID_MULTI_OUTPUT:           return "Multistate Output";
        case ZBM_CLUSTER_ID_MULTI_VALUE:            return "Multistate Value";
        case ZBM_CLUSTER_ID_COMMISSIONING:          return "Commissioning";
        case ZBM_CLUSTER_ID_OTA_UPGRADE:            return "OTA Upgrade";
        case ZBM_CLUSTER_ID_POLL_CONTROL:           return "Poll Control";
        case ZBM_CLUSTER_ID_GREEN_POWER:            return "Green Power";
        case ZBM_CLUSTER_ID_KEEP_ALIVE:             return "Keep Alive";
        case ZBM_CLUSTER_ID_SHADE_CONFIG:           return "Shade Config";
        case ZBM_CLUSTER_ID_DOOR_LOCK:              return "Door Lock";
        case ZBM_CLUSTER_ID_WINDOW_COVERING:        return "Window Covering";
        case ZBM_CLUSTER_ID_PUMP_CONFIG_CONTROL:    return "Pump Config Control";
        case ZBM_CLUSTER_ID_THERMOSTAT:             return "Thermostat";
        case ZBM_CLUSTER_ID_FAN_CONTROL:            return "Fan Control";
        case ZBM_CLUSTER_ID_DEHUMIDIFICATION_CONTROL: return "Dehumidification Control";
        case ZBM_CLUSTER_ID_THERMOSTAT_UI_CONFIG:   return "Thermostat UI Config";
        case ZBM_CLUSTER_ID_COLOR_CONTROL:          return "Color Control";
        case ZBM_CLUSTER_ID_BALLAST_CONFIG:         return "Ballast Config";
        case ZBM_CLUSTER_ID_ILLUMINANCE_MEASUREMENT: return "Illuminance Measurement";
        case ZBM_CLUSTER_ID_TEMP_MEASUREMENT:       return "Temperature Measurement";
        case ZBM_CLUSTER_ID_PRESSURE_MEASUREMENT:   return "Pressure Measurement";
        case ZBM_CLUSTER_ID_FLOW_MEASUREMENT:       return "Flow Measurement";
        case ZBM_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT: return "Relative Humidity Measurement";
        case ZBM_CLUSTER_ID_OCCUPANCY_SENSING:      return "Occupancy Sensing";
        case ZBM_CLUSTER_ID_PH_MEASUREMENT:         return "pH Measurement";
        case ZBM_CLUSTER_ID_EC_MEASUREMENT:         return "Electrical Conductivity";
        case ZBM_CLUSTER_ID_WIND_SPEED_MEASUREMENT: return "Wind Speed Measurement";
        case ZBM_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT: return "CO2 Measurement";
        case ZBM_CLUSTER_ID_PM2_5_MEASUREMENT:      return "PM2.5 Measurement";
        case ZBM_CLUSTER_ID_IAS_ZONE:               return "IAS Zone";
        case ZBM_CLUSTER_ID_IAS_ACE:                return "IAS ACE";
        case ZBM_CLUSTER_ID_IAS_WD:                 return "IAS WD";
        case ZBM_CLUSTER_ID_PRICE:                  return "Price";
        case ZBM_CLUSTER_ID_DRLC:                   return "DRLC";
        case ZBM_CLUSTER_ID_METERING:               return "Metering";
        case ZBM_CLUSTER_ID_METER_IDENTIFICATION:   return "Meter Identification";
        case ZBM_CLUSTER_ID_ELECTRICAL_MEASUREMENT: return "Electrical Measurement";
        case ZBM_CLUSTER_ID_DIAGNOSTICS:            return "Diagnostics";

        // Кастомные кластеры
        case ZBM_CLUSTER_ID_CUSTOM_TUYA:            return "Tuya";
        case ZBM_CLUSTER_ID_CUSTOM_TUYA_DATA:       return "Tuya Data";
        case ZBM_CLUSTER_ID_CUSTOM_XIAOMI:          return "Xiaomi";
        case ZBM_CLUSTER_ID_CUSTOM_AQARA:           return "Aqara";
        case ZBM_CLUSTER_ID_CUSTOM_LUMI:            return "Lumi";
        //case ZBM_CLUSTER_ID_CUSTOM_GLEDOPTO:        return "Gledopto";
        case ZBM_CLUSTER_ID_CUSTOM_SONOFF:          return "Sonoff";
        case ZBM_CLUSTER_ID_CUSTOM_IKEA_ONOFF:      return "IKEA On/Off";
        case ZBM_CLUSTER_ID_CUSTOM_IKEA_LEVEL:      return "IKEA Level";
        case ZBM_CLUSTER_ID_CUSTOM_HUE_SCENES:      return "Hue Scenes";
        case ZBM_CLUSTER_ID_CUSTOM_DEVELCO:         return "Develco";
        case ZBM_CLUSTER_ID_CUSTOM_CENTRALITE:      return "Centralite";
        case ZBM_CLUSTER_ID_CUSTOM_SMARTTHINGS_ACCELERATION: return "SmartThings Accel";
        case ZBM_CLUSTER_ID_CUSTOM_TEST:            return "Test Cluster";
        case ZBM_CLUSTER_ID_CUSTOM_DEBUG:           return "Debug Cluster";

        default:
            break;
    }

    // Для неизвестных — возвращаем NULL, чтобы вызывающая сторона сама сформировала имя
    return NULL;
}

bool zbm_is_standard_cluster_id(uint16_t cluster_id)
{
    switch (cluster_id) {
        case ZBM_CLUSTER_ID_BASIC:
        case ZBM_CLUSTER_ID_POWER_CONFIG:
        case ZBM_CLUSTER_ID_DEVICE_TEMP_CONFIG:
        case ZBM_CLUSTER_ID_IDENTIFY:
        case ZBM_CLUSTER_ID_GROUPS:
        case ZBM_CLUSTER_ID_SCENES:
        case ZBM_CLUSTER_ID_ON_OFF:
        case ZBM_CLUSTER_ID_ON_OFF_SWITCH_CONFIG:
        case ZBM_CLUSTER_ID_LEVEL_CONTROL:
        case ZBM_CLUSTER_ID_ALARMS:
        case ZBM_CLUSTER_ID_TIME:
        case ZBM_CLUSTER_ID_RSSI_LOCATION:
        case ZBM_CLUSTER_ID_ANALOG_INPUT:
        case ZBM_CLUSTER_ID_ANALOG_OUTPUT:
        case ZBM_CLUSTER_ID_ANALOG_VALUE:
        case ZBM_CLUSTER_ID_BINARY_INPUT:
        case ZBM_CLUSTER_ID_BINARY_OUTPUT:
        case ZBM_CLUSTER_ID_BINARY_VALUE:
        case ZBM_CLUSTER_ID_MULTI_INPUT:
        case ZBM_CLUSTER_ID_MULTI_OUTPUT:
        case ZBM_CLUSTER_ID_MULTI_VALUE:
        case ZBM_CLUSTER_ID_COMMISSIONING:
        case ZBM_CLUSTER_ID_OTA_UPGRADE:
        case ZBM_CLUSTER_ID_POLL_CONTROL:
        case ZBM_CLUSTER_ID_GREEN_POWER:
        case ZBM_CLUSTER_ID_KEEP_ALIVE:
        case ZBM_CLUSTER_ID_SHADE_CONFIG:
        case ZBM_CLUSTER_ID_DOOR_LOCK:
        case ZBM_CLUSTER_ID_WINDOW_COVERING:
        case ZBM_CLUSTER_ID_PUMP_CONFIG_CONTROL:
        case ZBM_CLUSTER_ID_THERMOSTAT:
        case ZBM_CLUSTER_ID_FAN_CONTROL:
        case ZBM_CLUSTER_ID_DEHUMIDIFICATION_CONTROL:
        case ZBM_CLUSTER_ID_THERMOSTAT_UI_CONFIG:
        case ZBM_CLUSTER_ID_COLOR_CONTROL:
        case ZBM_CLUSTER_ID_BALLAST_CONFIG:
        case ZBM_CLUSTER_ID_ILLUMINANCE_MEASUREMENT:
        case ZBM_CLUSTER_ID_TEMP_MEASUREMENT:
        case ZBM_CLUSTER_ID_PRESSURE_MEASUREMENT:
        case ZBM_CLUSTER_ID_FLOW_MEASUREMENT:
        case ZBM_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT:
        case ZBM_CLUSTER_ID_OCCUPANCY_SENSING:
        case ZBM_CLUSTER_ID_PH_MEASUREMENT:
        case ZBM_CLUSTER_ID_EC_MEASUREMENT:
        case ZBM_CLUSTER_ID_WIND_SPEED_MEASUREMENT:
        case ZBM_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT:
        case ZBM_CLUSTER_ID_PM2_5_MEASUREMENT:
        case ZBM_CLUSTER_ID_IAS_ZONE:
        case ZBM_CLUSTER_ID_IAS_ACE:
        case ZBM_CLUSTER_ID_IAS_WD:
        case ZBM_CLUSTER_ID_PRICE:
        case ZBM_CLUSTER_ID_DRLC:
        case ZBM_CLUSTER_ID_METERING:
        case ZBM_CLUSTER_ID_METER_IDENTIFICATION:
        case ZBM_CLUSTER_ID_ELECTRICAL_MEASUREMENT:
        case ZBM_CLUSTER_ID_DIAGNOSTICS:
            return true;
        default:
            return false;
    }
}

// универсальный конструктор, подходит и для стандартных, и для кастомных.
void* create_cluster(uint16_t cluster_id, zbm_cluster_role_t role_mask, bool is_custom)
{
    const char* name = zbm_get_cluster_friendlyname(cluster_id);
    if (!name) {
        // Если имя не найдено — генерим шаблон
        char temp_name[64];
        snprintf(temp_name, sizeof(temp_name), "Cluster 0x%04X", cluster_id);
        name = temp_name;
    }

    char* friendly_name = strdup(name);
    if (!friendly_name) return NULL;

    if (is_custom) {
        zbm_custom_cluster_t* cluster = calloc(1, sizeof(zbm_custom_cluster_t));
        if (!cluster) {
            free(friendly_name);
            return NULL;
        }
        cluster->id = cluster_id;
        cluster->role_mask = role_mask;
        cluster->friendlyname = friendly_name;
        return cluster;
    } else {
        zbm_standart_cluster_t* cluster = calloc(1, sizeof(zbm_standart_cluster_t));
        if (!cluster) {
            free(friendly_name);
            return NULL;
        }
        cluster->id = cluster_id;
        cluster->role_mask = role_mask;
        cluster->friendlyname = friendly_name;
        return cluster;
    }
}

zbm_standart_cluster_t* zbm_create_standard_cluster(uint16_t cluster_id, zbm_cluster_role_t role_mask)
{
    zbm_standart_cluster_t* cluster = (zbm_standart_cluster_t*)create_cluster(cluster_id, role_mask, false);
    if (!cluster) return NULL;

    // === Атрибуты ===
    uint8_t attr_count = 0;
    zbm_cluster_attribute_t** attr_array = zbm_create_standard_attribute_array(cluster_id, role_mask, &attr_count);
    cluster->attr_array = attr_array;
    cluster->attr_count = attr_count;

    // === Команды (только для SERVER) ===
    uint8_t cmd_count = 0;
    zbm_cluster_standart_cmd_t** cmd_array = NULL;

    if (role_mask & ZBM_CLUSTER_ROLE_SERVER) {
        cmd_array = zbm_create_standard_command_array(cluster_id, role_mask, &cmd_count);
    }

    cluster->standart_cmd_array = cmd_array;
    cluster->standart_cmd_count = cmd_count;

    // ✅ Инициализируем кастомные репорты
    cluster->custom_report_cmd_array = NULL;
    cluster->custom_report_cmd_count = 0;

    return cluster;
}

bool zbm_free_standart_cluster(zbm_standart_cluster_t* cluster)
{
    if (!cluster) return false;

    if (cluster->friendlyname) {
        free(cluster->friendlyname);
        cluster->friendlyname = NULL;
    }

    if (cluster->attr_array && cluster->attr_count > 0) {
        for (int i = 0; i < cluster->attr_count; i++) {
            zbm_cluster_attribute_t* attr = cluster->attr_array[i];
            if (attr) {
                zbm_free_cluster_attribute(attr);
                cluster->attr_array[i] = NULL;
            }
        }
        free(cluster->attr_array);
        cluster->attr_array = NULL;
    }
    cluster->attr_count = 0;

    if (cluster->standart_cmd_array && cluster->standart_cmd_count > 0) {
        for (int i = 0; i < cluster->standart_cmd_count; i++) {
            zbm_cluster_standart_cmd_t* cmd = cluster->standart_cmd_array[i];
            if (cmd) {
                zbm_free_cluster_standart_cmd(cmd);
                cluster->standart_cmd_array[i] = NULL;
            }
        }
        free(cluster->standart_cmd_array);
        cluster->standart_cmd_array = NULL;
    }
    cluster->standart_cmd_count = 0;

    if (cluster->custom_report_cmd_array && cluster->custom_report_cmd_count > 0) {
        for (int i = 0; i < cluster->custom_report_cmd_count; i++) {
            zbm_cluster_custom_report_cmd_t* report_cmd = cluster->custom_report_cmd_array[i];
            if (report_cmd) {
                zbm_free_cluster_custom_report_cmd(report_cmd);
                cluster->custom_report_cmd_array[i] = NULL;
            }
        }
        free(cluster->custom_report_cmd_array);
        cluster->custom_report_cmd_array = NULL;
    }
    cluster->custom_report_cmd_count = 0;

    free(cluster);
    return true;
}

bool zbm_free_custom_cluster(zbm_custom_cluster_t* cluster)
{
    if (!cluster) return false;

    if (cluster->friendlyname) {
        free(cluster->friendlyname);
        cluster->friendlyname = NULL;
    }

    if (cluster->attr_array && cluster->attr_count > 0) {
        for (int i = 0; i < cluster->attr_count; i++) {
            zbm_cluster_attribute_t* attr = cluster->attr_array[i];
            if (attr) {
                zbm_free_cluster_attribute(attr);
                cluster->attr_array[i] = NULL;
            }
        }
        free(cluster->attr_array);
        cluster->attr_array = NULL;
    }
    cluster->attr_count = 0;

    if (cluster->standart_cmd_array && cluster->standart_cmd_count > 0) {
        for (int i = 0; i < cluster->standart_cmd_count; i++) {
            zbm_cluster_standart_cmd_t* cmd = cluster->standart_cmd_array[i];
            if (cmd) {
                zbm_free_cluster_standart_cmd(cmd);
                cluster->standart_cmd_array[i] = NULL;
            }
        }
        free(cluster->standart_cmd_array);
        cluster->standart_cmd_array = NULL;
    }
    cluster->standart_cmd_count = 0;

    if (cluster->custom_report_cmd_array && cluster->custom_report_cmd_count > 0) {
        for (int i = 0; i < cluster->custom_report_cmd_count; i++) {
            zbm_cluster_custom_report_cmd_t* report_cmd = cluster->custom_report_cmd_array[i];
            if (report_cmd) {
                zbm_free_cluster_custom_report_cmd(report_cmd);
                cluster->custom_report_cmd_array[i] = NULL;
            }
        }
        free(cluster->custom_report_cmd_array);
        cluster->custom_report_cmd_array = NULL;
    }
    cluster->custom_report_cmd_count = 0;

    free(cluster);
    return true;
}