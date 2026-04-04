#ifndef ZBM_CLUSTERS_TYPE_H
#define ZBM_CLUSTERS_TYPE_H

#include "zbm_cmd_types.h"
#include "zbm_attr_types.h"
#include "zbm_low_level_types.h"


/**
 * @brief Идентификаторы стандартных Zigbee-кластеров
 */
typedef enum {
    ZBM_CLUSTER_ID_BASIC                  = 0x0000U,  /*!< Basic cluster identifier */
    ZBM_CLUSTER_ID_POWER_CONFIG           = 0x0001U,  /*!< Power configuration cluster */
    ZBM_CLUSTER_ID_DEVICE_TEMP_CONFIG     = 0x0002U,  /*!< Device temperature configuration cluster */
    ZBM_CLUSTER_ID_IDENTIFY               = 0x0003U,  /*!< Identify cluster */
    ZBM_CLUSTER_ID_GROUPS                 = 0x0004U,  /*!< Groups cluster */
    ZBM_CLUSTER_ID_SCENES                 = 0x0005U,  /*!< Scenes cluster */
    ZBM_CLUSTER_ID_ON_OFF                 = 0x0006U,  /*!< On/Off cluster */
    ZBM_CLUSTER_ID_ON_OFF_SWITCH_CONFIG   = 0x0007U,  /*!< On/Off switch configuration cluster */
    ZBM_CLUSTER_ID_LEVEL_CONTROL          = 0x0008U,  /*!< Level control cluster */
    ZBM_CLUSTER_ID_ALARMS                 = 0x0009U,  /*!< Alarms cluster */
    ZBM_CLUSTER_ID_TIME                   = 0x000aU,  /*!< Time cluster */
    ZBM_CLUSTER_ID_RSSI_LOCATION          = 0x000bU,  /*!< RSSI location cluster */
    ZBM_CLUSTER_ID_ANALOG_INPUT           = 0x000cU,  /*!< Analog input (basic) cluster */
    ZBM_CLUSTER_ID_ANALOG_OUTPUT          = 0x000dU,  /*!< Analog output (basic) cluster */
    ZBM_CLUSTER_ID_ANALOG_VALUE           = 0x000eU,  /*!< Analog value (basic) cluster */
    ZBM_CLUSTER_ID_BINARY_INPUT           = 0x000fU,  /*!< Binary input (basic) cluster */
    ZBM_CLUSTER_ID_BINARY_OUTPUT          = 0x0010U,  /*!< Binary output (basic) cluster */
    ZBM_CLUSTER_ID_BINARY_VALUE           = 0x0011U,  /*!< Binary value (basic) cluster */
    ZBM_CLUSTER_ID_MULTI_INPUT            = 0x0012U,  /*!< Multistate input (basic) cluster */
    ZBM_CLUSTER_ID_MULTI_OUTPUT           = 0x0013U,  /*!< Multistate output (basic) cluster */
    ZBM_CLUSTER_ID_MULTI_VALUE            = 0x0014U,  /*!< Multistate value (basic) cluster */
    ZBM_CLUSTER_ID_COMMISSIONING          = 0x0015U,  /*!< Commissioning cluster */
    ZBM_CLUSTER_ID_OTA_UPGRADE            = 0x0019U,  /*!< Over The Air (OTA) Upgrade cluster */
    ZBM_CLUSTER_ID_POLL_CONTROL           = 0x0020U,  /*!< Poll control cluster */
    ZBM_CLUSTER_ID_GREEN_POWER            = 0x0021U,  /*!< Green Power cluster */
    ZBM_CLUSTER_ID_KEEP_ALIVE             = 0x0025U,  /*!< Keep Alive cluster */
    ZBM_CLUSTER_ID_SHADE_CONFIG           = 0x0100U,  /*!< Shade configuration cluster */
    ZBM_CLUSTER_ID_DOOR_LOCK              = 0x0101U,  /*!< Door lock cluster */
    ZBM_CLUSTER_ID_WINDOW_COVERING        = 0x0102U,  /*!< Window covering cluster */
    ZBM_CLUSTER_ID_PUMP_CONFIG_CONTROL    = 0x0200U,  /*!< Pump configuration and control cluster */
    ZBM_CLUSTER_ID_THERMOSTAT             = 0x0201U,  /*!< Thermostat cluster */
    ZBM_CLUSTER_ID_FAN_CONTROL            = 0x0202U,  /*!< Fan control cluster */
    ZBM_CLUSTER_ID_DEHUMIDIFICATION_CONTROL = 0x0203U,/*!< Dehumidification control cluster */
    ZBM_CLUSTER_ID_THERMOSTAT_UI_CONFIG   = 0x0204U,  /*!< Thermostat user interface configuration cluster */
    ZBM_CLUSTER_ID_COLOR_CONTROL          = 0x0300U,  /*!< Color control cluster */
    ZBM_CLUSTER_ID_BALLAST_CONFIG         = 0x0301U,  /*!< Ballast configuration cluster */
    ZBM_CLUSTER_ID_ILLUMINANCE_MEASUREMENT = 0x0400U, /*!< Illuminance measurement cluster */
    ZBM_CLUSTER_ID_TEMP_MEASUREMENT       = 0x0402U,  /*!< Temperature measurement cluster */
    ZBM_CLUSTER_ID_PRESSURE_MEASUREMENT   = 0x0403U,  /*!< Pressure measurement cluster */
    ZBM_CLUSTER_ID_FLOW_MEASUREMENT       = 0x0404U,  /*!< Flow measurement cluster */
    ZBM_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT = 0x0405U,/*!< Relative humidity measurement cluster */
    ZBM_CLUSTER_ID_OCCUPANCY_SENSING      = 0x0406U,  /*!< Occupancy sensing cluster */
    ZBM_CLUSTER_ID_PH_MEASUREMENT         = 0x0409U,  /*!< pH measurement cluster */
    ZBM_CLUSTER_ID_EC_MEASUREMENT         = 0x040aU,  /*!< Electrical conductivity measurement cluster */
    ZBM_CLUSTER_ID_WIND_SPEED_MEASUREMENT = 0x040bU,  /*!< Wind speed measurement cluster */
    ZBM_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT = 0x040dU,/*!< Carbon dioxide measurement cluster */
    ZBM_CLUSTER_ID_PM2_5_MEASUREMENT      = 0x042aU,  /*!< PM2.5 measurement cluster */
    ZBM_CLUSTER_ID_IAS_ZONE               = 0x0500U,  /*!< IAS zone cluster */
    ZBM_CLUSTER_ID_IAS_ACE                = 0x0501U,  /*!< IAS ACE cluster */
    ZBM_CLUSTER_ID_IAS_WD                 = 0x0502U,  /*!< IAS WD (Warning Device) cluster */
    ZBM_CLUSTER_ID_PRICE                  = 0x0700U,  /*!< Price cluster */
    ZBM_CLUSTER_ID_DRLC                   = 0x0701U,  /*!< Demand Response and Load Control cluster */
    ZBM_CLUSTER_ID_METERING               = 0x0702U,  /*!< Metering cluster */
    ZBM_CLUSTER_ID_METER_IDENTIFICATION   = 0x0b01U,  /*!< Meter Identification cluster */
    ZBM_CLUSTER_ID_ELECTRICAL_MEASUREMENT = 0x0b04U,  /*!< Electrical measurement cluster */
    ZBM_CLUSTER_ID_DIAGNOSTICS            = 0x0b05U,  /*!< Diagnostics cluster */
} zbm_cluster_id_t;

typedef enum {
    // === TUYA / MOES / LONSONHO / Jinvoo ===
    ZBM_CLUSTER_ID_CUSTOM_TUYA               = 0xEF00U,  /*!< Tuya-specific cluster (часто используется для управления) */
    ZBM_CLUSTER_ID_CUSTOM_TUYA_DATA          = 0xE000U,  /*!< Tuya Data Point (DP) cluster (альтернативный диапазон) */
    ZBM_CLUSTER_ID_CUSTOM_TUYA_MANUFACTURER  = 0x0000U,  /*!< Manufacturer-specific cluster (внутри manuf code) */

    // === XIAOMI / Aqara ===
    ZBM_CLUSTER_ID_CUSTOM_XIAOMI             = 0xFCC0U,  /*!< Xiaomi proprietary cluster (атрибуты датчиков, battery, link quality) */
    ZBM_CLUSTER_ID_CUSTOM_AQARA              = 0xFCC1U,  /*!< Aqara-specific cluster (motion timeout, sensitivity) */
    ZBM_CLUSTER_ID_CUSTOM_LUMI               = 0xFCC2U,  /*!< Lumi (Aqara) custom cluster */
    ZBM_CLUSTER_ID_CUSTOM_XIAOMI_MIJA        = 0xFFC0U,  /*!< MiJia (Xiaomi) sensor extensions */

    // === GLEDOPTO (RGB+CCT контроллеры) ===
    //ZBM_CLUSTER_ID_CUSTOM_GLEDOPTO           = 0xFC01U,  /*!< GLEDOPTO: RGB+CCT control, firmware update */
    //ZBM_CLUSTER_ID_CUSTOM_GLEDOPTO_SCENE     = 0xFC7D,   /*!< GLEDOPTO Scene commands */

    // === SONOFF (eWeLink) ===
    ZBM_CLUSTER_ID_CUSTOM_SONOFF             = 0xFFFFU,  /*!< Sonoff (eWeLink) custom commands (deprecated) */
    ZBM_CLUSTER_ID_CUSTOM_SONOFF_OO          = 0xFFFEU,  /*!< Sonoff OO (out-of-band) cluster */
    ZBM_CLUSTER_ID_CUSTOM_SONOFF_ZBRELAY     = 0xFFFDU,  /*!< Sonoff ZBRelay control cluster */
    ZBM_CLUSTER_ID_CUSTOM_SONOFF_ZIGBEE3     = 0xFFFCU,  /*!< Sonoff Zigbee 3.0 specific */

    // === IKEA TRÅDFRI ===
    ZBM_CLUSTER_ID_CUSTOM_IKEA_ONOFF         = 0xFC7C,   /*!< IKEA: On/Off with timed mode */
    ZBM_CLUSTER_ID_CUSTOM_IKEA_LEVEL         = 0xFC7B,   /*!< IKEA: Level control enhancements */
    ZBM_CLUSTER_ID_CUSTOM_IKEA_GROUP         = 0xFC40,   /*!< IKEA: Group binding extension */
    ZBM_CLUSTER_ID_CUSTOM_IKEA_COMMAND       = 0xFC00,   /*!< IKEA: Proprietary command cluster */

    // === PHILIPS HUE ===
    ZBM_CLUSTER_ID_CUSTOM_HUE_SCENES         = 0xFC01,   /*!< Philips Hue: Enhanced scenes */
    ZBM_CLUSTER_ID_CUSTOM_HUE_GROUPS         = 0xFC02,   /*!< Philips Hue: Extended groups */
    ZBM_CLUSTER_ID_CUSTOM_HUE_SENSORS        = 0xFC03,   /*!< Philips Hue: Sensor data */
    ZBM_CLUSTER_ID_CUSTOM_HUE_LIGHTLINK      = 0xFC21,   /*!< Philips Hue: Light Link info */

    // === DEVELCO (датчики) ===
    ZBM_CLUSTER_ID_CUSTOM_DEVELCO            = 0xFC03,   /*!< Develco: Sensor configuration and diagnostics */
    ZBM_CLUSTER_ID_CUSTOM_DEVELCO_IAS        = 0xFC04,   /*!< Develco: Extended IAS functionality */

    // === Centralite / Cooper ===
    ZBM_CLUSTER_ID_CUSTOM_CENTRALITE         = 0xFC05,   /*!< Centralite: Battery and reporting config */
    ZBM_CLUSTER_ID_CUSTOM_CENTRALITE_OOB     = 0xFC06,   /*!< Centralite: Out-of-band setup */

    // === Samsung SmartThings ===
    ZBM_CLUSTER_ID_CUSTOM_SMARTTHINGS_ACCELERATION = 0xFC02, /*!< SmartThings: Acceleration sensor */
    ZBM_CLUSTER_ID_CUSTOM_SMARTTHINGS_TEMPERATURE  = 0xFC40, /*!< SmartThings: Local temp override */
    ZBM_CLUSTER_ID_CUSTOM_SMARTTHINGS_ILLUMINANCE  = 0xFC41, /*!< SmartThings: Illuminance calibration */

    // === Universal Custom / Vendor-Specific ===
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_1           = 0xFFE0U,  /*!< Vendor-specific cluster 1 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_2           = 0xFFE1U,  /*!< Vendor-specific cluster 2 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_3           = 0xFFE2U,  /*!< Vendor-specific cluster 3 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_4           = 0xFFE3U,  /*!< Vendor-specific cluster 4 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_5           = 0xFFE4U,  /*!< Vendor-specific cluster 5 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_6           = 0xFFE5U,  /*!< Vendor-specific cluster 6 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_7           = 0xFFE6U,  /*!< Vendor-specific cluster 7 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_8           = 0xFFE7U,  /*!< Vendor-specific cluster 8 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_9           = 0xFFE8U,  /*!< Vendor-specific cluster 9 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_10          = 0xFFE9U,  /*!< Vendor-specific cluster 10 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_11          = 0xFFEAU,  /*!< Vendor-specific cluster 11 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_12          = 0xFFEBU,  /*!< Vendor-specific cluster 12 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_13          = 0xFFECU,  /*!< Vendor-specific cluster 13 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_14          = 0xFFEDU,  /*!< Vendor-specific cluster 14 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_15          = 0xFFEEU,  /*!< Vendor-specific cluster 15 */
    ZBM_CLUSTER_ID_CUSTOM_VENDOR_16          = 0xFFEFU,  /*!< Vendor-specific cluster 16 */

    // === Special / Debug / Test ===
    ZBM_CLUSTER_ID_CUSTOM_TEST               = 0xFF01U,  /*!< Для тестирования и отладки */
    ZBM_CLUSTER_ID_CUSTOM_DEBUG              = 0xFF02U,  /*!< Отладочные команды и логи */

} zbm_cluster_id_custom_t;

typedef struct zbm_standart_cluster_s {
    zbm_cluster_id_t                        id;
    char*                                   friendlyname;
    zbm_cluster_role_t                      role_mask; // esp_zb_zcl_cluster_role_t; 0x01U SERVER (input) 0x02U CLIENT (output)
    uint8_t                                 attr_count; // стандартные атрибуты кластера по ZCL
    zbm_cluster_attribute_t**               attr_array;
    uint8_t                                 standart_cmd_count; // типа 0x00 in cluster 0x0006 (команда по ZCL доступная для конкретного кластера)
    zbm_cluster_standart_cmd_t**            standart_cmd_array;
    uint8_t                                 custom_report_cmd_count; // типа TUYA 0xfd from cluster 0x0006 (в кластер не шлётся, только читается репорт)
    zbm_cluster_custom_report_cmd_t**       custom_report_cmd_array;
}zbm_standart_cluster_t;

typedef struct zbm_custom_cluster_s {
    zbm_cluster_id_custom_t                 id;
    char*                                   friendlyname;
    zbm_cluster_role_t                      role_mask; // esp_zb_zcl_cluster_role_t; 0x01U SERVER (input) 0x02U CLIENT (output)
    uint8_t                                 attr_count; // стандартные атрибуты кластера по ZCL
    zbm_cluster_attribute_t**               attr_array;
    uint8_t                                 standart_cmd_count; // типа 0x00 in cluster 0x0006 (команда по ZCL доступная для конкретного кластера)
    zbm_cluster_standart_cmd_t**            standart_cmd_array;
    uint8_t                                 custom_report_cmd_count; // типа TUYA 0xfd from cluster 0x0006 (в кластер не шлётся, только читается репорт)
    zbm_cluster_custom_report_cmd_t**       custom_report_cmd_array;
}zbm_custom_cluster_t;

bool zbm_is_standard_cluster_id(uint16_t cluster_id);



const char* zbm_get_cluster_friendlyname(uint16_t cluster_id);

/**
 * @brief Создаёт пустой кластер (стандартный или кастомный)
 * @param cluster_id ID кластера
 * @param role_mask Роль: ZBM_CLUSTER_ROLE_SERVER | ZBM_CLUSTER_ROLE_CLIENT
 * @param is_custom true — если это кастомный кластер
 * @return void* — указатель на zbm_standart_cluster_t или zbm_custom_cluster_t
 */
void* create_cluster(uint16_t cluster_id, zbm_cluster_role_t role_mask, bool is_custom);

/**
 * @brief Создаёт стандартный кластер с атрибутами и командами
 * @param cluster_id ID кластера (например, ZBM_CLUSTER_ID_ON_OFF)
 * @param role_mask Роль кластера
 * @return Готовый zbm_standart_cluster_t* или NULL
 */
zbm_standart_cluster_t* zbm_create_standard_cluster(uint16_t cluster_id, zbm_cluster_role_t role_mask);

//простая очистка стандартного кластера
bool zbm_free_standart_cluster(zbm_standart_cluster_t* cluster);

//простая очистка нестандартного кластера
bool zbm_free_custom_cluster(zbm_custom_cluster_t* cluster);

#endif // ZBM_CLUSTERS_TYPE_H