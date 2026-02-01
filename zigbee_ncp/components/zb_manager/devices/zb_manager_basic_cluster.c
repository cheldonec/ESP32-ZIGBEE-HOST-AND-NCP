#include "zb_manager_basic_cluster.h"
#include "esp_err.h"
#include "string.h"

esp_err_t zb_manager_basic_cluster_update_attribute(zigbee_manager_basic_cluster_t* cluster, uint16_t attr_id, void* value)
{
    switch (attr_id)
    {
    case 0x0000:
        cluster->zcl_version = *(uint8_t*)value;
        break;
    case 0x0001:
        cluster->application_version = *(uint8_t*)value;
        break;
    case 0x0002:
        cluster->stack_version = *(uint8_t*)value;
        break;
    case 0x0003:
        cluster->hw_version = *(uint8_t*)value;
        break;
    case 0x0004:
        memset(cluster->manufacturer_name, 0, sizeof(cluster->manufacturer_name));
        memcpy(cluster->manufacturer_name, (char*)value, strlen((char*)value));
        break;
    case 0x0005:
        memset(cluster->model_identifier, 0, sizeof(cluster->model_identifier));
        memcpy(cluster->model_identifier, (char*)value, strlen((char*)value));
        break;
    case 0x0006:
        memset(cluster->date_code, 0, sizeof(cluster->date_code));
        memcpy(cluster->date_code, (char*)value, strlen((char*)value));
        break;
    case 0x0007:
        cluster->power_source = *(uint8_t*)value;
        break;
    case 0x0008:
        cluster->generic_device_class = *(uint8_t*)value;
        break;
    case 0x0009:
        cluster->generic_device_type = *(uint8_t*)value;
        break;
    case 0x000a:
        memset(cluster->product_code, 0, sizeof(cluster->product_code)); 
        memcpy(cluster->product_code, (char*)value, strlen((char*)value));
        break;
    case 0x000b:
        memset(cluster->product_url, 0, sizeof(cluster->product_url)); // 256 - максимальная длина URL (не включая завершающий ноль
        memcpy(cluster->product_url, (char*)value, strlen((char*)value));
        break;
    case 0x000c:
        memset(cluster->manufacturer_version_details, 0, sizeof(cluster->manufacturer_version_details)); // 256 - максимальная длина URL (не включая завершающий ноль
        memcpy(cluster->manufacturer_version_details, (char*)value, strlen((char*)value));
        break;
    case 0x000d:
        memset(cluster->serial_number, 0, sizeof(cluster->serial_number)); // 256 - максимальная длина URL (не включая завершающий ноль
        memcpy(cluster->serial_number, (char*)value, strlen((char*)value));
        break;
    case 0x000e:
        memset(cluster->product_label, 0, sizeof(cluster->product_label)); // 256 - максимальная длина URL (не включая завершающий ноль
        memcpy(cluster->product_label, (char*)value, strlen((char*)value));
        break;
    case 0x0010:
        memset(cluster->location_description, 0, sizeof(cluster->location_description)); // 256 - максимальная длина URL (не включая завершающий ноль
        memcpy(cluster->location_description, (char*)value, strlen((char*)value));
        break;
    case 0x0011:
        cluster->physical_environment = *(uint8_t*)value;
        break;
    case 0x0012:
        cluster->device_enabled = *(bool*)value;
        break;
    case 0x0013:
        cluster->alarm_mask = *(uint8_t*)value;
        break;
    case 0x0014:
        cluster->disable_local_config = *(bool*)value;
        break;
    case 0x4000:
        memset(cluster->sw_build_id, 0, sizeof(cluster->sw_build_id)); //
        memcpy(cluster->sw_build_id, (char*)value, strlen((char*)value));
        break;
    default:
        break;
    }
    return ESP_OK;
}

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

const char* get_power_source_string(uint8_t power_source) {
    // Check if the secondary power source bit is set (bit 7)
    bool is_secondary = (power_source & 0x80) != 0;
    // Mask off the secondary bit to get the base power source
    uint8_t base_source = power_source & 0x7F;

    // Select the base power source string
    const char* base_str;
    switch (base_source) {
        case ESP_ZB_ZCL_BASIC_POWER_SOURCE_UNKNOWN:
            base_str = "Unknown";
            break;
        case ESP_ZB_ZCL_BASIC_POWER_SOURCE_MAINS_SINGLE_PHASE:
            base_str = "Mains (Single Phase)";
            break;
        case ESP_ZB_ZCL_BASIC_POWER_SOURCE_MAINS_THREE_PHASE:
            base_str = "Mains (Three Phase)";
            break;
        case ESP_ZB_ZCL_BASIC_POWER_SOURCE_BATTERY:
            base_str = "Battery";
            break;
        case ESP_ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE:
            base_str = "DC Source";
            break;
        case ESP_ZB_ZCL_BASIC_POWER_SOURCE_EMERGENCY_MAINS_CONST:
            base_str = "Emergency Mains (Constant)";
            break;
        case ESP_ZB_ZCL_BASIC_POWER_SOURCE_EMERGENCY_MAINS_TRANSF:
            base_str = "Emergency Mains (Transfer)";
            break;
        default:
            base_str = "Invalid";
            break;
    }

    // Depending on whether it's a secondary source, modify the string.
    // For simplicity, this uses a static buffer. In a multi-threaded environment,
    // a different approach (like returning a pointer to a static array) would be safer.

    // Since the longest string is ~35 chars, and we might add "Secondary: ", 64 is safe.
    static char result[64];
    if (is_secondary) {
        snprintf(result, sizeof(result), "Secondary: %s", base_str);
    } else {
        // If not secondary, return the base string directly for efficiency.
        return base_str;
    }

    return result;
}