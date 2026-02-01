// zb_manager_devices_manufactory_table.c

#include "zb_manager_devices_manufactory_table.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>



static const char *TAG = "manuf_resolver";

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

uint16_t zb_manager_guess_manufacturer_code(const uint8_t ieee_addr[8])
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

const char* zb_manager_get_manufacturer_name(const uint8_t ieee_addr[8])
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

uint16_t zb_manager_resolve_manufacturer_code(
    const uint8_t ieee_addr[8],
    const void *node_desc,
    const char *basic_manufacturer_name)
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
        uint16_t oui_code = zb_manager_guess_manufacturer_code(ieee_addr);

        if (oui_code == 0x1002 && code != 0x1002) {
            ESP_LOGW(TAG, "OUI says Tuya (0x1002), but node_desc.manuf_code=0x%04x → ignoring!", code);
            code = 0;  // сброс
        } else {
            ESP_LOGI(TAG, "Manuf code from node_desc: 0x%04x", code);
            return code;
        }
    }

    // 3. Fallback: по OUI
    code = zb_manager_guess_manufacturer_code(ieee_addr);
    if (code != 0) {
        ESP_LOGI(TAG, "Manuf code from OUI fallback: 0x%04x", code);
        return code;
    }

    ESP_LOGW(TAG, "Failed to resolve manufacturer_code for IEEE %02x:%02x:%02x:xx:xx:xx:xx:xx",
             ieee_addr[5], ieee_addr[6], ieee_addr[7]);

    return 0;
}

const char* zb_manager_get_manufacturer_name_by_code(uint16_t manuf_code) {
    switch (manuf_code) {
        case 0x1002: return "Tuya";
        case 0x115F: return "Lumi/Aqara";
        case 0x117C: return "IKEA";
        case 0x100B: return "Philips";
        case 0x1241: return "Heiman";
        default: return "unknown";
    }
}