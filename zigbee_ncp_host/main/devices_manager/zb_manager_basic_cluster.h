#ifndef ZB_MANAGER_BASIC_CLUSTER_H
#define ZB_MANAGER_BASIC_CLUSTER_H
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "zbm_clusters.h"
#include "esp_zigbee_zcl_command.h"



/** @brief Basic cluster information attribute set identifiers
*/
typedef enum {
    ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID                  = 0x0000,                 /*!<ZCL version attribute */
    ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID          = 0x0001,                 /*!< Application version attribute */
    ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID                = 0x0002,                 /*!< Stack version attribute */
    ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID                   = 0x0003,                 /*!< Hardware version attribute */
    ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID            = 0x0004,                 /*!< Manufacturer name attribute */
    ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID             = 0x0005,                 /*!< Model identifier attribute */
    ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID                    = 0x0006,                 /*!< Date code attribute */
    ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID                 = 0x0007,                 /*!< Power source attribute */
    ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_CLASS_ID         = 0x0008,                 /*!< The GenericDeviceClass attribute defines the field of application of the  GenericDeviceType attribute. */
    ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_TYPE_ID          = 0x0009,                 /*!< The GenericDeviceType attribute allows an application to show an icon on a rich user interface (e.g. smartphone app). */
    ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_CODE_ID                 = 0x000a,                 /*!< The ProductCode attribute allows an application to specify a code for the product. */
    ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_URL_ID                  = 0x000b,                 /*!< The ProductURL attribute specifies a link to a web page containing specific product information. */
    ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_VERSION_DETAILS_ID = 0x000c,                 /*!< Vendor specific human readable (displayable) string representing the versions of one of more program images supported on the device. */
    ESP_ZB_ZCL_ATTR_BASIC_SERIAL_NUMBER_ID                = 0x000d,                 /*!< Vendor specific human readable (displayable) serial number. */
    ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_LABEL_ID                = 0x000e,                 /*!< Vendor specific human readable (displayable) product label. */
    ESP_ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID         = 0x0010,                 /*!< Location description attribute */
    ESP_ZB_ZCL_ATTR_BASIC_PHYSICAL_ENVIRONMENT_ID         = 0x0011,                 /*!< Physical environment attribute */
    ESP_ZB_ZCL_ATTR_BASIC_DEVICE_ENABLED_ID               = 0x0012,                 /*!< Device enabled attribute */
    ESP_ZB_ZCL_ATTR_BASIC_ALARM_MASK_ID                   = 0x0013,                 /*!< Alarm mask attribute */
    ESP_ZB_ZCL_ATTR_BASIC_DISABLE_LOCAL_CONFIG_ID         = 0x0014,                 /*!< Disable local config attribute */
    ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID                     = 0x4000                  /*!< Manufacturer-specific reference to the version of the software. */
} local_esp_zb_zcl_basic_attr_t;

// для использования без Zigbee Дшв
typedef enum local_esp_zb_zcl_basic_power_source_e {
    ESP_ZB_ZCL_BASIC_POWER_SOURCE_UNKNOWN                = 0x00,  /*!< Unknown power source */
    ESP_ZB_ZCL_BASIC_POWER_SOURCE_MAINS_SINGLE_PHASE     = 0x01,  /*!< Single-phase mains. */
    ESP_ZB_ZCL_BASIC_POWER_SOURCE_MAINS_THREE_PHASE      = 0x02,  /*!< 3-phase mains. */
    ESP_ZB_ZCL_BASIC_POWER_SOURCE_BATTERY                = 0x03,  /*!< Battery source. */
    ESP_ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE              = 0x04,  /*!< DC source. */
    ESP_ZB_ZCL_BASIC_POWER_SOURCE_EMERGENCY_MAINS_CONST  = 0x05,  /*!< Emergency mains constantly powered. */
    ESP_ZB_ZCL_BASIC_POWER_SOURCE_EMERGENCY_MAINS_TRANSF = 0x06   /*!< Emergency mains and transfer switch. */
} local_esp_zb_zcl_basic_power_source_t;

typedef struct {
    // Information Attributes
    uint8_t role_mask;                  //esp_zb_zcl_cluster_role_t;  01 Server
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
    char power_source_text[33];         /*!< Power source text (default: empty string) [32*/
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
    /**
     * @brief Last Update Timestamp
     *
     * Time in milliseconds when the `on_off` state was last updated.
     */
    uint32_t last_update_ms;

    uint16_t                    nostandart_attr_count;
    attribute_custom_t**        nostandart_attr_array;
} zigbee_manager_basic_cluster_t;

#define ZIGBEE_BASIC_CLUSTER_DEFAULT_INIT() { \
    /* Information Attributes */ \
    .role_mask = 0x01, \
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
    .last_update_ms = 0, \
    .nostandart_attr_count = 0, \
    .nostandart_attr_array = NULL, \
}

esp_err_t zb_manager_basic_cluster_update_attribute(zigbee_manager_basic_cluster_t* cluster,uint16_t attr_id,uint8_t attr_type,void* value,uint16_t value_len);
/* Пример
zigbee_manager_basic_cluster_t basic_info = ZIGBEE_BASIC_CLUSTER_DEFAULT_INIT();
snprintf(basic_info.manufacturer_name, sizeof(basic_info.manufacturer_name), "MyCompany");
    snprintf(basic_info.model_identifier, sizeof(basic_info.model_identifier), "Model-01");
    snprintf(basic_info.date_code, sizeof(basic_info.date_code), "20240501");
    basic_info.power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_BATTERY;
    basic_info.physical_environment = 1; // Atrium
    snprintf(basic_info.serial_number, sizeof(basic_info.serial_number), "SN123456789");
*/
const char* zb_manager_get_basic_cluster_attr_name(uint16_t attrID);

const char* get_power_source_string(uint8_t power_source);

/*! @brief Basic cluster command identifiers
*/
typedef enum {
    ESP_ZB_ZCL_CMD_BASIC_RESET_ID       = 0x00 /*!< "Reset to Factory Defaults" command. */
} esp_zb_zcl_basic_cmd_id_t;

/**
 * @brief   Send ZCL basic reset to factory default command
 *
 * @param[in]  cmd_req  pointer to the basic command @ref esp_zb_zcl_basic_fact_reset_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t zb_manager_basic_factory_reset_cmd_req(esp_zb_zcl_basic_fact_reset_cmd_t *cmd_req);

esp_err_t zb_manager_basic_cluster_add_custom_attribute(zigbee_manager_basic_cluster_t *cluster, uint16_t attr_id, uint8_t attr_type);

attribute_custom_t *zb_manager_basic_cluster_find_custom_attr_obj(zigbee_manager_basic_cluster_t *cluster, uint16_t attr_id);
#endif