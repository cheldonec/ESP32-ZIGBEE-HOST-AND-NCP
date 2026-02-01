#ifndef ZB_MANAGER_BASIC_CLUSTER_H
#define ZB_MANAGER_BASIC_CLUSTER_H
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    // Information Attributes
    uint8_t source_ep;                  /*!< Source endpoint Иногда Basic cluster только на первой EP*/
    uint8_t zcl_version;                /*!< ZCL version (default: 0x08) */
    uint8_t application_version;        /*!< Application firmware version (default: 0x00) */
    uint8_t stack_version;              /*!< Zigbee stack version (default: 0x00) */
    uint8_t hw_version;                 /*!< Hardware version (default: 0x00) */
    
    // Device Identification Attributes
    char manufacturer_name[33];         /*!< Manufacturer name (default: empty string) */
    char model_identifier[33];          /*!< Model identifier (default: empty string) */
    char date_code[17];                 /*!< Manufacturing date code (default: empty string) */
    char product_code[19];              /*!< Product code (default: empty string) */
    char product_url[255];              /*!< Product URL (default: empty string) */
    char manufacturer_version_details[65]; /*!< Vendor-specific version details (default: empty string) */
    char serial_number[33];             /*!< Serial number (default: empty string) */
    char product_label[33];             /*!< Product label (default: empty string) */
    
    // Device Status Attributes
    uint8_t power_source;               /*!< Power source (default: 0x00 - Unknown)
                                         *   See esp_zb_zcl_basic_power_source_t for values.
                                         *   Bit 7: Secondary power source indicator.
                                         */
    uint8_t generic_device_class;       /*!< Generic device class (default: 0xFF - Not defined) */
    uint8_t generic_device_type;        /*!< Generic device type (default: 0xFF - Not defined) */
    char location_description[17];      /*!< Location description (default: empty string) */
    uint8_t physical_environment;       /*!< Physical environment (default: 0x00 - Unspecified) */
    bool device_enabled;                /*!< Device enabled state (default: true) */
    uint8_t alarm_mask;                 /*!< Alarm mask (default: 0x00 - No alarms) */
    uint8_t disable_local_config;       /*!< Disable local configuration (default: 0x00 - Enabled) */
    
    // Software Information
    char sw_build_id[17];               /*!< Software build ID (default: empty string) 
                                         *   Stored as a Pascal-style string (length byte + content).
                                         */
} zigbee_manager_basic_cluster_t;

#define ZIGBEE_BASIC_CLUSTER_DEFAULT_INIT() { \
    /* Information Attributes */ \
    .source_ep = 0x01, \
    .zcl_version = 0x08, \
    .application_version = 0x00, \
    .stack_version = 0x00, \
    .hw_version = 0x00, \
    \
    /* Device Identification Attributes */ \
    .manufacturer_name = {0}, \
    .model_identifier = {0}, \
    .date_code = {0}, \
    .product_code = {0}, \
    .product_url = {0}, \
    .manufacturer_version_details = {0}, \
    .serial_number = {0}, \
    .product_label = {0}, \
    \
    /* Device Status Attributes */ \
    .power_source = 0x00, \
    .generic_device_class = 0xFF, \
    .generic_device_type = 0xFF, \
    .location_description = {0}, \
    .physical_environment = 0x00, \
    .device_enabled = true, \
    .alarm_mask = 0x00, \
    .disable_local_config = 0x00, \
    \
    /* Software Information */ \
    .sw_build_id = {0}, \
}

esp_err_t zb_manager_basic_cluster_update_attribute(zigbee_manager_basic_cluster_t* cluster, uint16_t attr_id, void* value);
/* Пример
zigbee_manager_basic_cluster_t basic_info = ZIGBEE_BASIC_CLUSTER_DEFAULT_INIT();
snprintf(basic_info.manufacturer_name, sizeof(basic_info.manufacturer_name), "MyCompany");
    snprintf(basic_info.model_identifier, sizeof(basic_info.model_identifier), "Model-01");
    snprintf(basic_info.date_code, sizeof(basic_info.date_code), "20240501");
    basic_info.power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_BATTERY;
    basic_info.physical_environment = 1; // Atrium
    snprintf(basic_info.serial_number, sizeof(basic_info.serial_number), "SN123456789");
*/



#endif