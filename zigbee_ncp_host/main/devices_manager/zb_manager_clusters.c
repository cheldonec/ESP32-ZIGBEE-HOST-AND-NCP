/// @brief [zb_manager_clusters.c] Модуль для работы с Zigbee-кластерами: имена, атрибуты, логирование
/// Реализует функции получения названий кластеров и атрибутов, а также форматированный вывод атрибутов

#include "zb_manager_clusters.h"
#include "esp_log.h"
#include "string.h"
#include <inttypes.h> 

/// @brief Тег для логирования в этом модуле
static const char *TAG = "zb_manager_clusters";

/// @brief Структура для хранения соответствия ID кластера и его имени
typedef struct {
    uint16_t cluster_id;       ///< Идентификатор кластера
    const char *cluster_name;  ///< Текстовое имя кластера
} zb_cluster_entry_t;

/// @brief Таблица соответствия ID кластеров и их названий
static const zb_cluster_entry_t cluster_table[] = {
    { ESP_ZB_ZCL_CLUSTER_ID_BASIC, "Basic" },
    { ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, "Power Config" },
    { ESP_ZB_ZCL_CLUSTER_ID_DEVICE_TEMP_CONFIG, "Device Temp Config" },
    { ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY, "Identify" },
    { ESP_ZB_ZCL_CLUSTER_ID_GROUPS, "Groups" },
    { ESP_ZB_ZCL_CLUSTER_ID_SCENES, "Scenes" },
    { ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, "On/Off" },
    { ESP_ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG, "On/Off Switch Config" },
    { ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, "Level Control" },
    { ESP_ZB_ZCL_CLUSTER_ID_ALARMS, "Alarms" },
    { ESP_ZB_ZCL_CLUSTER_ID_TIME, "Time" },
    { ESP_ZB_ZCL_CLUSTER_ID_RSSI_LOCATION, "RSSI Location" },
    { ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, "Analog Input" },
    { ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, "Analog Output" },
    { ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE, "Analog Value" },
    { ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT, "Binary Input" },
    { ESP_ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT, "Binary Output" },
    { ESP_ZB_ZCL_CLUSTER_ID_BINARY_VALUE, "Binary Value" },
    { ESP_ZB_ZCL_CLUSTER_ID_MULTI_INPUT, "Multi Input" },
    { ESP_ZB_ZCL_CLUSTER_ID_MULTI_OUTPUT, "Multi Output" },
    { ESP_ZB_ZCL_CLUSTER_ID_MULTI_VALUE, "Multi Value" },
    { ESP_ZB_ZCL_CLUSTER_ID_COMMISSIONING, "Commissioning" },
    { ESP_ZB_ZCL_CLUSTER_ID_OTA_UPGRADE, "OTA Upgrade" },
    { ESP_ZB_ZCL_CLUSTER_ID_POLL_CONTROL, "Poll Control" },
    { ESP_ZB_ZCL_CLUSTER_ID_GREEN_POWER, "Green Power" },
    { ESP_ZB_ZCL_CLUSTER_ID_KEEP_ALIVE, "Keep Alive" },
    { ESP_ZB_ZCL_CLUSTER_ID_SHADE_CONFIG, "Shade Config" },
    { ESP_ZB_ZCL_CLUSTER_ID_DOOR_LOCK, "Door Lock" },
    { ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, "Window Covering" },
    { ESP_ZB_ZCL_CLUSTER_ID_PUMP_CONFIG_CONTROL, "Pump Config Control" },
    { ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, "Thermostat" },
    { ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL, "Fan Control" },
    { ESP_ZB_ZCL_CLUSTER_ID_DEHUMIDIFICATION_CONTROL, "Dehumidification Control" },
    { ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT_UI_CONFIG, "Thermostat UI Config" },
    { ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, "Color Control" },
    { ESP_ZB_ZCL_CLUSTER_ID_BALLAST_CONFIG, "Ballast Config" },
    { ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT, "Illuminance Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, "Temperature Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, "Pressure Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT, "Flow Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, "Relative Humidity Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, "Occupancy Sensing" },
    { ESP_ZB_ZCL_CLUSTER_ID_PH_MEASUREMENT, "pH Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_EC_MEASUREMENT, "Electrical Conductivity Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_WIND_SPEED_MEASUREMENT, "Wind Speed Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, "Carbon Dioxide Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_PM2_5_MEASUREMENT, "PM2.5 Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, "IAS Zone" },
    { ESP_ZB_ZCL_CLUSTER_ID_IAS_ACE, "IAS ACE" },
    { ESP_ZB_ZCL_CLUSTER_ID_IAS_WD, "IAS WD" },
    { ESP_ZB_ZCL_CLUSTER_ID_PRICE, "Price" },
    { ESP_ZB_ZCL_CLUSTER_ID_DRLC, "DRLC" },
    { ESP_ZB_ZCL_CLUSTER_ID_METERING, "Metering" },
    { ESP_ZB_ZCL_CLUSTER_ID_METER_IDENTIFICATION, "Meter Identification" },
    { ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, "Electrical Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_DIAGNOSTICS, "Diagnostics" },
};

/// @brief Получает текстовое имя кластера по его ID
/// @param clusterID Идентификатор кластера (например, 0x0006 для On/Off)
/// @return Название кластера или "Unknown Cluster"
const char* zb_manager_get_cluster_name(uint16_t clusterID) {
    for (size_t i = 0; i < sizeof(cluster_table) / sizeof(cluster_table[0]); i++) {
        if (cluster_table[i].cluster_id == clusterID) {
            return cluster_table[i].cluster_name;
        }
    }
    return "Unknown Cluster";
}

/// @brief Тип функции для получения имени атрибута по его ID
typedef const char* (*attr_func_t)(uint16_t attr_id);

/// @brief Структура для сопоставления ID кластера и функции получения имён атрибутов
static const struct {
    uint16_t cluster_id;  ///< Идентификатор кластера
    attr_func_t func;     ///< Указатель на функцию получения имени атрибута
} cluster_attr_funcs[] = {
    { ESP_ZB_ZCL_CLUSTER_ID_BASIC, zb_manager_get_basic_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, zb_manager_get_power_config_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, zb_manager_get_on_off_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, zb_manager_get_temperature_measurement_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, zb_manager_get_humidity_measurement_cluster_attr_name },
};

/// @brief Получает текстовое имя атрибута кластера по его ID
/// @param clusterID Идентификатор кластера
/// @param attr_id Идентификатор атрибута
/// @return Название атрибута или "Unknown Cluster"
const char* zb_manager_get_attr_name(uint16_t clusterID, uint16_t attr_id) {
    for (size_t i = 0; i < sizeof(cluster_attr_funcs) / sizeof(cluster_attr_funcs[0]); i++) {
        if (cluster_attr_funcs[i].cluster_id == clusterID) {
            return cluster_attr_funcs[i].func(attr_id);
        }
    }
    return "Unknown Attr";
}

/// @brief Логирует значение Zigbee-атрибута с учётом типа и кластера
/// @param cluster_id ID кластера
/// @param attr Указатель на структуру атрибута
/// @param src_addr Адрес источника (может быть NULL)
/// @param endpoint Номер эндпоинта
void log_zb_attribute(uint16_t cluster_id, const zb_manager_cmd_report_attr_t *attr, const esp_zb_zcl_addr_t *src_addr, uint8_t endpoint)
{
    if (!attr) {
        ESP_LOGW(TAG, "log_zb_attribute: attr is NULL");
        return;
    }

    const char* cluster_name = zb_manager_get_cluster_name(cluster_id);
    const char* attr_name = zb_manager_get_attr_name(cluster_id, attr->attr_id);
    void* value = attr->attr_value;
    uint16_t short_addr = src_addr ? src_addr->u.short_addr : 0xFFFF;

    // --- 🌡️ Temperature Measurement: Measured Value ---
    if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT && attr->attr_id == ATTR_TEMP_MEASUREMENT_VALUE_ID) {
        if (attr->attr_type == ESP_ZB_ZCL_ATTR_TYPE_S16) {
            int16_t temp = *(int16_t*)value;
            if (short_addr != 0xFFFF && endpoint) {
                ESP_LOGI(TAG, "🌡️  %s: %.1f °C [%s] (from 0x%04hx, EP:%u)", 
                         attr_name, temp / 100.0, cluster_name, short_addr, endpoint);
            } else if (short_addr != 0xFFFF) {
                ESP_LOGI(TAG, "🌡️  %s: %.1f °C [%s] (from 0x%04hx)", 
                         attr_name, temp / 100.0, cluster_name, short_addr);
            } else {
                ESP_LOGI(TAG, "🌡️  %s: %.1f °C [%s]", 
                         attr_name, temp / 100.0, cluster_name);
            }
        } else {
            if (short_addr != 0xFFFF && endpoint) {
                ESP_LOGI(TAG, "🌡️  %s: invalid type %d [%s] (from 0x%04hx, EP:%u)", 
                         attr_name, attr->attr_type, cluster_name, short_addr, endpoint);
            } else if (short_addr != 0xFFFF) {
                ESP_LOGI(TAG, "🌡️  %s: invalid type %d [%s] (from 0x%04hx)", 
                         attr_name, attr->attr_type, cluster_name, short_addr);
            } else {
                ESP_LOGI(TAG, "🌡️  %s: invalid type %d [%s]", 
                         attr_name, attr->attr_type, cluster_name);
            }
        }
        return;
    }

    // --- 💧 Relative Humidity Measurement: Measured Value ---
    if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT && attr->attr_id == ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID) {
        if (attr->attr_type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
            uint16_t hum = *(uint16_t*)value;
            if (short_addr != 0xFFFF && endpoint) {
                ESP_LOGI(TAG, "💧 %s: %.1f %% [%s] (from 0x%04hx, EP:%u)", 
                         attr_name, hum / 100.0, cluster_name, short_addr, endpoint);
            } else if (short_addr != 0xFFFF) {
                ESP_LOGI(TAG, "💧 %s: %.1f %% [%s] (from 0x%04hx)", 
                         attr_name, hum / 100.0, cluster_name, short_addr);
            } else {
                ESP_LOGI(TAG, "💧 %s: %.1f %% [%s]", 
                         attr_name, hum / 100.0, cluster_name);
            }
        } else {
            if (short_addr != 0xFFFF && endpoint) {
                ESP_LOGI(TAG, "💧 %s: invalid type %d [%s] (from 0x%04hx, EP:%u)", 
                         attr_name, attr->attr_type, cluster_name, short_addr, endpoint);
            } else if (short_addr != 0xFFFF) {
                ESP_LOGI(TAG, "💧 %s: invalid type %d [%s] (from 0x%04hx)", 
                         attr_name, attr->attr_type, cluster_name, short_addr);
            } else {
                ESP_LOGI(TAG, "💧 %s: invalid type %d [%s]", 
                         attr_name, attr->attr_type, cluster_name);
            }
        }
        return;
    }

    // --- 🔋 Power Config: Battery Percentage Remaining ---
    if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG && attr->attr_id == ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID) {
        if (attr->attr_type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
            uint8_t val = *(uint8_t*)value;
            if (val == 0xFF) {
                if (short_addr != 0xFFFF && endpoint) {
                    ESP_LOGI(TAG, "🔋 %s: Unknown [%s] (from 0x%04hx, EP:%u)", 
                             attr_name, cluster_name, short_addr, endpoint);
                } else if (short_addr != 0xFFFF) {
                    ESP_LOGI(TAG, "🔋 %s: Unknown [%s] (from 0x%04hx)", 
                             attr_name, cluster_name, short_addr);
                } else {
                    ESP_LOGI(TAG, "🔋 %s: Unknown [%s]", 
                             attr_name, cluster_name);
                }
            } else {
                if (short_addr != 0xFFFF && endpoint) {
                    ESP_LOGI(TAG, "🔋 %s: %.1f %% [%s] (from 0x%04hx, EP:%u)", 
                             attr_name, val * 0.5f, cluster_name, short_addr, endpoint);
                } else if (short_addr != 0xFFFF) {
                    ESP_LOGI(TAG, "🔋 %s: %.1f %% [%s] (from 0x%04hx)", 
                             attr_name, val * 0.5f, cluster_name, short_addr);
                } else {
                    ESP_LOGI(TAG, "🔋 %s: %.1f %% [%s]", 
                             attr_name, val * 0.5f, cluster_name);
                }
            }
        } else {
            if (short_addr != 0xFFFF && endpoint) {
                ESP_LOGI(TAG, "🔋 %s: invalid type %d [%s] (from 0x%04hx, EP:%u)", 
                         attr_name, attr->attr_type, cluster_name, short_addr, endpoint);
            } else if (short_addr != 0xFFFF) {
                ESP_LOGI(TAG, "🔋 %s: invalid type %d [%s] (from 0x%04hx)", 
                         attr_name, attr->attr_type, cluster_name, short_addr);
            } else {
                ESP_LOGI(TAG, "🔋 %s: invalid type %d [%s]", 
                         attr_name, attr->attr_type, cluster_name);
            }
        }
        return;
    }

    // --- ⚡ Power Config: Battery Voltage ---
    if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG && attr->attr_id == ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID) {
        if (attr->attr_type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
            uint8_t val = *(uint8_t*)value;
            if (val == 0xFF) {
                if (short_addr != 0xFFFF && endpoint) {
                    ESP_LOGI(TAG, "⚡ %s: Unknown [%s] (from 0x%04hx, EP:%u)", 
                             attr_name, cluster_name, short_addr, endpoint);
                } else if (short_addr != 0xFFFF) {
                    ESP_LOGI(TAG, "⚡ %s: Unknown [%s] (from 0x%04hx)", 
                             attr_name, cluster_name, short_addr);
                } else {
                    ESP_LOGI(TAG, "⚡ %s: Unknown [%s]", 
                             attr_name, cluster_name);
                }
            } else {
                if (short_addr != 0xFFFF && endpoint) {
                    ESP_LOGI(TAG, "⚡ %s: %.1f V [%s] (from 0x%04hx, EP:%u)", 
                             attr_name, val * 0.1f, cluster_name, short_addr, endpoint);
                } else if (short_addr != 0xFFFF) {
                    ESP_LOGI(TAG, "⚡ %s: %.1f V [%s] (from 0x%04hx)", 
                             attr_name, val * 0.1f, cluster_name, short_addr);
                } else {
                    ESP_LOGI(TAG, "⚡ %s: %.1f V [%s]", 
                             attr_name, val * 0.1f, cluster_name);
                }
            }
        } else {
            if (short_addr != 0xFFFF && endpoint) {
                ESP_LOGI(TAG, "⚡ %s: invalid type %d [%s] (from 0x%04hx, EP:%u)", 
                         attr_name, attr->attr_type, cluster_name, short_addr, endpoint);
            } else if (short_addr != 0xFFFF) {
                ESP_LOGI(TAG, "⚡ %s: invalid type %d [%s] (from 0x%04hx)", 
                         attr_name, attr->attr_type, cluster_name, short_addr);
            } else {
                ESP_LOGI(TAG, "⚡ %s: invalid type %d [%s]", 
                         attr_name, attr->attr_type, cluster_name);
            }
        }
        return;
    }

    // --- 🟢/🔴 On/Off Cluster: OnOff ---
    if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF && attr->attr_id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
        if (attr->attr_type == ESP_ZB_ZCL_ATTR_TYPE_BOOL || attr->attr_type == ESP_ZB_ZCL_ATTR_TYPE_8BIT) {
            bool on = (*(uint8_t*)value) ? true : false;
            if (short_addr != 0xFFFF && endpoint) {
                ESP_LOGI(TAG, "%s: %s [%s] (from 0x%04hx, EP:%u)", 
                         on ? "🟢 Light" : "🔴 Light", on ? "ON" : "OFF", cluster_name, short_addr, endpoint);
            } else if (short_addr != 0xFFFF) {
                ESP_LOGI(TAG, "%s: %s [%s] (from 0x%04hx)", 
                         on ? "🟢 Light" : "🔴 Light", on ? "ON" : "OFF", cluster_name, short_addr);
            } else {
                ESP_LOGI(TAG, "%s: %s [%s]", 
                         on ? "🟢 Light" : "🔴 Light", on ? "ON" : "OFF", cluster_name);
            }
        } else {
            if (short_addr != 0xFFFF && endpoint) {
                ESP_LOGI(TAG, "🟡 OnOff: invalid type %d [%s] (from 0x%04hx, EP:%u)", 
                         attr->attr_type, cluster_name, short_addr, endpoint);
            } else if (short_addr != 0xFFFF) {
                ESP_LOGI(TAG, "🟡 OnOff: invalid type %d [%s] (from 0x%04hx)", 
                         attr->attr_type, cluster_name, short_addr);
            } else {
                ESP_LOGI(TAG, "🟡 OnOff: invalid type %d [%s]", 
                         attr->attr_type, cluster_name);
            }
        }
        return;
    }

    // --- 📡 Все остальные атрибуты — подробный вывод ---
    if (src_addr) {
        if (endpoint) {
            ESP_LOGI(TAG, "📡 %s: %s (0x%04x), %s (0x%04x), Short: 0x%04hx, EP: %u",
                     cluster_name, cluster_name, cluster_id, attr_name, attr->attr_id, short_addr, endpoint);
        } else {
            ESP_LOGI(TAG, "📡 %s: %s (0x%04x), %s (0x%04x), Short: 0x%04hx",
                     cluster_name, cluster_name, cluster_id, attr_name, attr->attr_id, short_addr);
        }
    } else {
        ESP_LOGI(TAG, "📡 %s: %s (0x%04x), %s (0x%04x)",
                 cluster_name, cluster_name, cluster_id, attr_name, attr->attr_id);
    }

    // Вывод типа и значения (для неспециализированных атрибутов)
    switch (attr->attr_type) {
        case ESP_ZB_ZCL_ATTR_TYPE_BOOL: {
            bool val = *(bool*)value;
            ESP_LOGI(TAG, "  → Type: Bool, Value: %s", val ? "true" : "false");
            break;
        }
        case ESP_ZB_ZCL_ATTR_TYPE_8BIT:
        case ESP_ZB_ZCL_ATTR_TYPE_U8:
        case ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM: {
            uint8_t val = *(uint8_t*)value;
            ESP_LOGI(TAG, "  → Type: 8-bit, Value: %u", val);
            break;
        }
        case ESP_ZB_ZCL_ATTR_TYPE_16BIT:
        case ESP_ZB_ZCL_ATTR_TYPE_U16: {
            uint16_t val = *(uint16_t*)value;
            ESP_LOGI(TAG, "  → Type: 16-bit, Value: %u", val);
            break;
        }
        case ESP_ZB_ZCL_ATTR_TYPE_S16: {
            int16_t val = *(int16_t*)value;
            ESP_LOGI(TAG, "  → Type: Int16, Value: %d", val);
            break;
        }
        case ESP_ZB_ZCL_ATTR_TYPE_32BIT:
        case ESP_ZB_ZCL_ATTR_TYPE_U32: {
            uint32_t val = *(uint32_t*)value;
            ESP_LOGI(TAG, "  → Type: 32-bit, Value: %" PRIu32, val);
            break;
        }
        case ESP_ZB_ZCL_ATTR_TYPE_S32: {
            int32_t val = *(int32_t*)value;
            ESP_LOGI(TAG, "  → Type: Int32, Value: %" PRId32, val);
            break;
        }
        case ESP_ZB_ZCL_ATTR_TYPE_SINGLE: {
            float val;
            memcpy(&val, value, sizeof(float));
            ESP_LOGI(TAG, "  → Type: Float, Value: %.3f", val);
            break;
        }
        case ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING: {
            char* str = (char*)value;
            ESP_LOGI(TAG, "  → Type: String, Value: \"%s\"", str);
            break;
        }
        case ESP_ZB_ZCL_ATTR_TYPE_OCTET_STRING: {
            if (attr->attr_len > 0 && value) {
                uint8_t len = ((uint8_t*)value)[0];
                uint8_t* data = (uint8_t*)value + 1;
                ESP_LOGI(TAG, "  → Type: OctetStr, Len: %u", len);
                if (len > 0) {
                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, len, ESP_LOG_INFO);
                }
            }
            break;
        }
        case ESP_ZB_ZCL_ATTR_TYPE_ARRAY:
        case ESP_ZB_ZCL_ATTR_TYPE_STRUCTURE:
            ESP_LOGI(TAG, "  → Type: Array/Struct, Len: %u - Not decoded", attr->attr_len);
            break;
        default:
            ESP_LOGI(TAG, "  → Type: 0x%02x, Len: %u", attr->attr_type, attr->attr_len);
            if (attr->attr_len > 0 && value) {
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, value, attr->attr_len, ESP_LOG_INFO);
            }
            break;
    }
}

/// @brief Альтернативная функция логирования атрибута с подробным выводом
/// @param cluster_id ID кластера
/// @param attr Указатель на структуру атрибута
/// @param src_addr Адрес источника
/// @param endpoint Номер эндпоинта
void log_zb_attribute_temp(uint16_t cluster_id,const zb_manager_attr_t *attr,const esp_zb_zcl_addr_t *src_addr,uint8_t endpoint)
{
    if (!attr) {
        ESP_LOGW(TAG, "log_zb_attribute: attr is NULL");
        return;
    }

    const char* cluster_name = zb_manager_get_cluster_name(cluster_id);
    const char* attr_name = zb_manager_get_attr_name(cluster_id, attr->attr_id);

    // Базовый лог — устройство и атрибут
    if (src_addr) {
        if (endpoint) {
            ESP_LOGI(TAG,
                     "Zigbee Attr: Cluster: %s (0x%04x),"
                     " Attr: %s (0x%04x),"
                     " Short: 0x%04hx, EP: %u",
                     cluster_name, cluster_id,
                     attr_name, attr->attr_id,
                     src_addr->u.short_addr, endpoint);
        } else {
            ESP_LOGI(TAG,
                     "Zigbee Attr: Cluster: %s (0x%04x),"
                     " Attr: %s (0x%04x),"
                     " Short: 0x%04hx",
                     cluster_name, cluster_id,
                     attr_name, attr->attr_id,
                     src_addr->u.short_addr);
        }
    } else {
        ESP_LOGI(TAG,
                 "Zigbee Attr: Cluster: %s (0x%04x),"
                 " Attr: %s (0x%04x)",
                 cluster_name, cluster_id,
                 attr_name, attr->attr_id);
    }

    void* value = attr->attr_value;

    // Обработка по типу
    switch (attr->attr_type) {
        case ESP_ZB_ZCL_ATTR_TYPE_BOOL: {
            bool val = *(bool*)value;
            ESP_LOGI(TAG, "  Type: Bool, Value: %s", val ? "true" : "false");
            if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF && attr->attr_id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
                ESP_LOGI(TAG, "  → State: %s", val ? "ON" : "OFF");
            }
            break;
        }

        case ESP_ZB_ZCL_ATTR_TYPE_8BIT:
        case ESP_ZB_ZCL_ATTR_TYPE_U8:
        case ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM: {
            uint8_t val = *(uint8_t*)value;
            ESP_LOGI(TAG, "  Type: 8-bit, Value: %u", val);
            if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF && attr->attr_id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
                ESP_LOGI(TAG, "  → State: %s", val ? "ON" : "OFF");
            }
            if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG) {
                if (attr->attr_id == ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID) {
                    if (val == 0xFF) {
                        ESP_LOGI(TAG, "  → Battery Voltage: Unknown");
                    } else {
                        float voltage = val * 0.1f;
                        ESP_LOGI(TAG, "  → Battery Voltage: %.1f V", voltage);
                    }
                } else if (attr->attr_id == ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID) {
                    if (val == 0xFF) {
                        ESP_LOGI(TAG, "  → Battery Level: Unknown");
                    } else {
                        float percent = val * 0.5f;
                        ESP_LOGI(TAG, "  → Battery Level: %.1f %%", percent);
                    }
                }
            }
            break;
        }

        case ESP_ZB_ZCL_ATTR_TYPE_16BIT:
        case ESP_ZB_ZCL_ATTR_TYPE_U16: {
            uint16_t val = *(uint16_t*)value;
            ESP_LOGI(TAG, "  Type: 16-bit, Value: %u", val);
            if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT && attr->attr_id == ATTR_TEMP_MEASUREMENT_VALUE_ID) {
                ESP_LOGI(TAG, "  → Temperature: %.1f °C", val / 100.0);
            } else if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT && attr->attr_id == ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID) {
                ESP_LOGI(TAG, "  → Humidity: %.1f %%", val / 100.0);
            }
            break;
        }

        case ESP_ZB_ZCL_ATTR_TYPE_S16: {
            int16_t val = *(int16_t*)value;
            ESP_LOGI(TAG, "  Type: Int16, Value: %d", val);
            if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT && attr->attr_id == ATTR_TEMP_MEASUREMENT_VALUE_ID) {
                ESP_LOGI(TAG, "  → Temperature: %.1f °C", val / 100.0);
            }
            break;
        }

        case ESP_ZB_ZCL_ATTR_TYPE_32BIT:
        case ESP_ZB_ZCL_ATTR_TYPE_U32: {
            uint32_t val = *(uint32_t*)value;
            ESP_LOGI(TAG, "  Type: 32-bit, Value: %" PRIu32, val);
            break;
        }

        case ESP_ZB_ZCL_ATTR_TYPE_S32: {
            int32_t val = *(int32_t*)value;
            ESP_LOGI(TAG, "  Type: Int32, Value: %" PRId32, val);
            break;
        }

        case ESP_ZB_ZCL_ATTR_TYPE_SINGLE: {
            float val;
            memcpy(&val, value, sizeof(float));
            ESP_LOGI(TAG, "  Type: Float, Value: %.3f", val);
            break;
        }

        case ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING: {
            char* str = (char*)value;
            ESP_LOGI(TAG, "  Type: String, Value: \"%s\"", str);
            break;
        }

        case ESP_ZB_ZCL_ATTR_TYPE_OCTET_STRING: {
            if (attr->attr_len > 0 && value) {
                uint8_t len = ((uint8_t*)value)[0];
                uint8_t* data = (uint8_t*)value + 1;
                ESP_LOGI(TAG, "  Type: OctetStr, Data Len: %u", len);
                if (len > 0) {
                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, len, ESP_LOG_INFO);
                }
            }
            break;
        }

        case ESP_ZB_ZCL_ATTR_TYPE_ARRAY:
        case ESP_ZB_ZCL_ATTR_TYPE_STRUCTURE:
            ESP_LOGI(TAG, "  Type: Array/Struct, Len: %u - Not decoded", attr->attr_len);
            break;

        default:
            ESP_LOGI(TAG, "  Type: 0x%02x, Len: %u", attr->attr_type, attr->attr_len);
            if (attr->attr_len > 0 && value) {
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, value, attr->attr_len, ESP_LOG_INFO);
            }
            break;
    }
}
