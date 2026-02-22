// zb_manager_devices_manufactory_table.c

#include "zbm_dev_base_utils.h"
#include "esp_zigbee_zcl_common.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>


static const char *TAG = "ZBM_DEV_BASE_UTILS_MODULE";

void            ieee_to_str(char* out, const esp_zb_ieee_addr_t addr);
bool            str_to_ieee(const char* str, esp_zb_ieee_addr_t addr);
uint16_t        zbm_dev_base_extract_manuf_code_from_ieee(const uint8_t ieee_addr[8]);
const char*     zbm_dev_base_extract_manuf_name_from_ieee(const uint8_t ieee_addr[8]);
uint16_t        zbm_base_get_manuf_code_by_priority(const uint8_t ieee_addr[8], const void *node_desc, const char *basic_manufacturer_name);
const char*     zbm_extract_manufacturer_name_by_code(uint16_t manuf_code);

// Реальная таблица
const ieee_manuf_entry_t ieee_manuf_table[] = {
    // =============== TUYA ===============
    { .oui = {0x84, 0x71, 0x27}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0xBC, 0x33, 0xAC}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0x00, 0x15, 0x8D}, .manuf_code = 0x1037, .name = "TUYA/Nue" },
    { .oui = {0x00, 0x0D, 0x6F}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0x7C, 0x2F, 0x80}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0x54, 0xEF, 0x44}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0x60, 0x01, 0x84}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0x60, 0xA4, 0x23}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0x7C, 0x49, 0xEB}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0x84, 0x2E, 0x14}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0x84, 0x7B, 0x0F}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0x94, 0x50, 0x33}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0x94, 0x79, 0x77}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0x98, 0x29, 0xA6}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0x98, 0x3C, 0x8F}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0x98, 0x5A, 0x86}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0xA4, 0xC1, 0x38}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0xB4, 0xE3, 0xF9}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0xC0, 0x86, 0xA8}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0xC4, 0x39, 0x3A}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0xCC, 0x86, 0xEC}, .manuf_code = 0x1002, .name = "Tuya" },
    { .oui = {0xD0, 0x03, 0x4B}, .manuf_code = 0x1002, .name = "Tuya" },

    // =============== LUMI / AQARA ===============
    { .oui = {0x00, 0x15, 0x8D}, .manuf_code = 0x115F, .name = "Lumi/Aqara" },
    { .oui = {0x54, 0xEF, 0x44}, .manuf_code = 0x115F, .name = "Aqara" },
    { .oui = {0x04, 0xCF, 0x8C}, .manuf_code = 0x115F, .name = "Aqara" },

    // =============== PHILIPS ===============
    { .oui = {0x00, 0x17, 0x88}, .manuf_code = 0x100B, .name = "Philips" },
    { .oui = {0x00, 0x17, 0x89}, .manuf_code = 0x100B, .name = "Philips" },

    // =============== IKEA ===============
    { .oui = {0xC0, 0x38, 0x96}, .manuf_code = 0x117C, .name = "IKEA" },
    { .oui = {0xBC, 0xCD, 0x12}, .manuf_code = 0x117C, .name = "IKEA" },
    { .oui = {0xBC, 0xE5, 0x05}, .manuf_code = 0x117C, .name = "IKEA" },
    { .oui = {0x68, 0x0A, 0xE2}, .manuf_code = 0x117C, .name = "IKEA" },

    // =============== SAMSUNG ===============
    { .oui = {0x00, 0x60, 0x57}, .manuf_code = 0x104E, .name = "Samsung" },
    { .oui = {0x00, 0x60, 0x6E}, .manuf_code = 0x104E, .name = "Samsung" },

    // =============== CENTRALITE / JASCO ===============
    { .oui = {0x00, 0x0D, 0x6F}, .manuf_code = 0x104E, .name = "Centralite" },
    { .oui = {0x00, 0x67, 0x88}, .manuf_code = 0x104E, .name = "Jasco (GE)" },

    // =============== OSRAM / LEDVANCE ===============
    { .oui = {0x84, 0x18, 0x26}, .manuf_code = 0x110C, .name = "OSRAM" },
    { .oui = {0x84, 0x2E, 0x14}, .manuf_code = 0x110C, .name = "LEDVANCE" },

    // =============== INNR ===============
    { .oui = {0x08, 0x6B, 0xD7}, .manuf_code = 0x111E, .name = "Innr" },
    { .oui = {0x08, 0x6B, 0xD8}, .manuf_code = 0x111E, .name = "Innr" },

    // =============== SCHNEIDER ===============
    { .oui = {0x00, 0x1E, 0x5E}, .manuf_code = 0x114B, .name = "Schneider" },

    // =============== BOSCH ===============
    { .oui = {0x08, 0x00, 0x17}, .manuf_code = 0x10F2, .name = "Bosch" },

    // =============== HEIMAN ===============
    { .oui = {0x00, 0x12, 0x4B}, .manuf_code = 0x1241, .name = "Heiman" },

    // =============== LEGRAND ===============
    { .oui = {0x00, 0x04, 0x1B}, .manuf_code = 0x1021, .name = "Legrand" },

    // =============== DEVELCO ===============
    { .oui = {0x00, 0x15, 0x42}, .manuf_code = 0x1179, .name = "Develco" },

    // =============== TERMINATOR ===============
    { .oui = {0}, .manuf_code = 0, .name = NULL }
};

//================================================================================================================================
//============================================= ZBM_DEV_DEVICE_MANUF_CODE_FROM_IEEE ==============================================
//================================================================================================================================
uint16_t zbm_dev_base_extract_manuf_code_from_ieee(const uint8_t ieee_addr[8])
{
    if (!ieee_addr) return 0;
    uint8_t oui[3] = { ieee_addr[5], ieee_addr[6], ieee_addr[7] };

    for (int i = 0; ieee_manuf_table[i].manuf_code != 0; i++) {
        if (memcmp(oui, ieee_manuf_table[i].oui, 3) == 0) {
            ESP_LOGD(TAG, "OUI %02x:%02x:%02x → manuf_code=0x%04x (%s)",
                     oui[0], oui[1], oui[2],
                     ieee_manuf_table[i].manuf_code,
                     ieee_manuf_table[i].name);
            return ieee_manuf_table[i].manuf_code;
        }
    }
    ESP_LOGW(TAG, "Unknown OUI %02x:%02x:%02x", oui[0], oui[1], oui[2]);
    return 0;
}

//================================================================================================================================
//========================================= ZBM_DEV_DEVICE_MANUF_CODE_TXT_FROM_IEEE ==============================================
//================================================================================================================================
const char* zbm_dev_base_extract_manuf_name_from_ieee(const uint8_t ieee_addr[8])
{
    if (!ieee_addr) return "unknown";
    uint8_t oui[3] = { ieee_addr[5], ieee_addr[6], ieee_addr[7] };

    for (int i = 0; ieee_manuf_table[i].manuf_code != 0; i++) {
        if (memcmp(oui, ieee_manuf_table[i].oui, 3) == 0) {
            return ieee_manuf_table[i].name;
        }
    }
    return "unknown";
}

//================================================================================================================================
//========================================= ZBM_DEV_DEVICE_MANUF_CODE_BY_PRIORITY ================================================
//================================================================================================================================
uint16_t zbm_base_get_manuf_code_by_priority(const uint8_t ieee_addr[8], const void *node_desc, const char *basic_manufacturer_name)
{
   uint16_t code = 0;

    // 1. Сначала пробуем по строке из Basic Cluster — она надёжнее для Tuya/Aqara
    if (basic_manufacturer_name) {
        if (strstr(basic_manufacturer_name, "lumi") || strstr(basic_manufacturer_name, "LUMI")) {
            code = 0x115F;
        } else if (strstr(basic_manufacturer_name, "_TZ3000_") || 
                   strstr(basic_manufacturer_name, "_TZE200_") ||
                   strstr(basic_manufacturer_name, "tuya") ||
                   strstr(basic_manufacturer_name, "TUYA")) {
            code = 0x1002;
        } else if (strstr(basic_manufacturer_name, "IKEA") || strstr(basic_manufacturer_name, "TRADFRI")) {
            code = 0x117C;
        } else if (strstr(basic_manufacturer_name, "Philips")) {
            code = 0x100B;
        } else if (strstr(basic_manufacturer_name, "heiman")) {
            code = 0x1241;
        }

        if (code != 0) {
            ESP_LOGI(TAG, "Manuf code from basic.manufacturer: 0x%04x ('%s')", code, basic_manufacturer_name);
            return code;  // 🔥 Возвращаем сразу — выше приоритет!
        }
    }

    // 2. Теперь node_desc (только если он валидный И не противоречит ожидаемому)
    const local_esp_zb_af_node_desc_t *desc = (const local_esp_zb_af_node_desc_t *)node_desc;
    if (desc && desc->manufacturer_code != 0xFFFF && desc->manufacturer_code != 0x0000) {
        code = desc->manufacturer_code;

        // 🔒 Блокировка: если OUI указывает на Tuya, но node_desc — не Tuya → игнорируем node_desc
        // ✅ Передаём ПОЛНЫЙ ieee_addr, а не только oui[3]
        uint16_t oui_code = zbm_dev_base_extract_manuf_code_from_ieee(ieee_addr);

        if (oui_code == 0x1002 && code != 0x1002) {
            ESP_LOGW(TAG, "OUI says Tuya (0x1002), but node_desc.manuf_code=0x%04x → ignoring!", code);
            code = 0;  // сброс
        } else {
            ESP_LOGI(TAG, "Manuf code from node_desc: 0x%04x", code);
            return code;
        }
    }

    // 3. Fallback: по OUI
    code = zbm_dev_base_extract_manuf_code_from_ieee(ieee_addr);
    if (code != 0) {
        ESP_LOGI(TAG, "Manuf code from OUI fallback: 0x%04x", code);
        return code;
    }

    ESP_LOGW(TAG, "Failed to resolve manufacturer_code for IEEE %02x:%02x:%02x:xx:xx:xx:xx:xx",
             ieee_addr[5], ieee_addr[6], ieee_addr[7]);

    return 0;
}

//================================================================================================================================
//========================================= ZBM_DEV_DEVICE_MANUF_CODE_TXT_FROM_CODE ==============================================
//================================================================================================================================
const char* zbm_extract_manufacturer_name_by_code(uint16_t manuf_code) {
    switch (manuf_code) {
        case 0x1002: return "Tuya";
        case 0x115F: return "Lumi/Aqara";
        case 0x117C: return "IKEA";
        case 0x100B: return "Philips";
        case 0x1241: return "Heiman";
        default: return "unknown";
    }
}

//================================================================================================================================
//================================================ ZBM_DEV_DEVICE_TYPE_NAME_FROM_ID ==============================================
//================================================================================================================================
const char* zbm_dev_base_extract_device_type_name_from_device_id(uint16_t device_id)
{
    switch (device_id) {
        case ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID:                 return "on_off_switch";
        case ESP_ZB_HA_LEVEL_CONTROL_SWITCH_DEVICE_ID:          return "level_control_switch";
        case ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID:                 return "on_off_output";
        case ESP_ZB_HA_LEVEL_CONTROLLABLE_OUTPUT_DEVICE_ID:     return "level_controllable_output";
        case ESP_ZB_HA_SCENE_SELECTOR_DEVICE_ID:                return "scene_selector";
        case ESP_ZB_HA_CONFIGURATION_TOOL_DEVICE_ID:            return "configuration_tool";
        case ESP_ZB_HA_REMOTE_CONTROL_DEVICE_ID:                return "remote_control";
        case ESP_ZB_HA_COMBINED_INTERFACE_DEVICE_ID:            return "combined_interface";
        case ESP_ZB_HA_RANGE_EXTENDER_DEVICE_ID:                return "range_extender";
        case ESP_ZB_HA_MAINS_POWER_OUTLET_DEVICE_ID:            return "mains_power_outlet";
        case ESP_ZB_HA_DOOR_LOCK_DEVICE_ID:                     return "door_lock";
        case ESP_ZB_HA_DOOR_LOCK_CONTROLLER_DEVICE_ID:          return "door_lock_controller";
        case ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID:                 return "simple_sensor";
        case ESP_ZB_HA_CONSUMPTION_AWARENESS_DEVICE_ID:         return "consumption_awareness";
        case ESP_ZB_HA_HOME_GATEWAY_DEVICE_ID:                  return "home_gateway";
        case ESP_ZB_HA_SMART_PLUG_DEVICE_ID:                    return "smart_plug";
        case ESP_ZB_HA_WHITE_GOODS_DEVICE_ID:                   return "white_goods";
        case ESP_ZB_HA_METER_INTERFACE_DEVICE_ID:               return "meter_interface";
        case ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID:                  return "on_off_light";
        case ESP_ZB_HA_DIMMABLE_LIGHT_DEVICE_ID:                return "dimmable_light";
        case ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID:          return "color_dimmable_light";
        case ESP_ZB_HA_DIMMER_SWITCH_DEVICE_ID:                 return "dimmer_switch";
        case ESP_ZB_HA_COLOR_DIMMER_SWITCH_DEVICE_ID:           return "color_dimmer_switch";
        case ESP_ZB_HA_LIGHT_SENSOR_DEVICE_ID:                  return "light_sensor";
        case ESP_ZB_HA_SHADE_DEVICE_ID:                         return "shade";
        case ESP_ZB_HA_SHADE_CONTROLLER_DEVICE_ID:              return "shade_controller";
        case ESP_ZB_HA_WINDOW_COVERING_DEVICE_ID:               return "window_covering";
        case ESP_ZB_HA_WINDOW_COVERING_CONTROLLER_DEVICE_ID:    return "window_covering_controller";
        case ESP_ZB_HA_HEATING_COOLING_UNIT_DEVICE_ID:          return "heating_cooling_unit";
        case ESP_ZB_HA_THERMOSTAT_DEVICE_ID:                    return "thermostat";
        case ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID:            return "temperature_sensor";
        case ESP_ZB_HA_IAS_CONTROL_INDICATING_EQUIPMENT_ID:     return "ias_control_indicator";
        case ESP_ZB_HA_IAS_ANCILLARY_CONTROL_EQUIPMENT_ID:      return "ias_ancillary_control";
        case ESP_ZB_HA_IAS_ZONE_ID:                             return "ias_zone";
        case ESP_ZB_HA_IAS_WARNING_DEVICE_ID:                   return "ias_warning";
        case ESP_ZB_HA_TEST_DEVICE_ID:                          return "test_device";
        case ESP_ZB_HA_CUSTOM_TUNNEL_DEVICE_ID:                return "custom_tunnel";
        case ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID:                  return "custom_attr";
        default: return "unknown_device_type";
    }
}

//================================================================================================================================
//========================================================== ZBM_DEV_BASE_UTILS ==================================================
//================================================================================================================================
/****** Save Load Functions **************/
void ieee_to_str(char* out, const esp_zb_ieee_addr_t addr) {
    sprintf(out, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
            addr[7], addr[6], addr[5], addr[4],
            addr[3], addr[2], addr[1], addr[0]);
}

bool str_to_ieee(const char* str, esp_zb_ieee_addr_t addr) {
    uint8_t tmp[8];
    if (sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &tmp[7], &tmp[6], &tmp[5], &tmp[4],
               &tmp[3], &tmp[2], &tmp[1], &tmp[0]) != 8) {
        return false;
    }
    memcpy(addr, tmp, 8);
    return true;
}

/**
 * @brief Функция сравнения двух IEEE-адресов для использования с lfind/bsearch
 * 
 * @param a Указатель на первый IEEE-адрес (const void*)
 * @param b Указатель на второй IEEE-адрес (const void*)
 * @return int 0 если адреса равны, не ноль если различаются
 */
int ieee_addr_compare(esp_zb_ieee_addr_t *a, esp_zb_ieee_addr_t *b)
{
    if (!a || !b) return -1;
    return memcmp((const uint8_t *)a, (const uint8_t *)b, sizeof(esp_zb_ieee_addr_t));
}
//********************************************************************************************************************************