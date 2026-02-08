/// @brief [zb_manager_basic_cluster.c] Модуль для работы с Zigbee Basic Cluster (0x0000)
/// Содержит функции обновления атрибутов и преобразования значений в читаемый вид

#include "zb_manager_basic_cluster.h"
#include "esp_err.h"
#include "string.h"
#include "esp_log.h"
#include "ncp_host_zb_api.h"
#include "zb_manager_ncp_host.h"

/// @brief Тег для логирования в этом модуле
static const char *TAG = "ZB_BASIC_CL";

/// @brief Обновляет значение атрибута в Basic-кластере Zigbee
/// @param cluster Указатель на структуру кластера
/// @param attr_id Идентификатор атрибута (например, 0x0000 — ZCL Version)
/// @param value Указатель на значение атрибута
/// @return ESP_OK при успехе, иначе ошибка
esp_err_t zb_manager_basic_cluster_update_attribute(zigbee_manager_basic_cluster_t* cluster, uint16_t attr_id, void* value)
{
    uint8_t *data = NULL;
    uint8_t len = 0;
    if (value == NULL) {
        ESP_LOGW(TAG, "attr_id=0x%04x: value is NULL", attr_id);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Updating attr 0x%04x with value at %p", attr_id, value);

    cluster->last_update_ms = esp_log_timestamp(); //
    switch (attr_id)
    {
    case ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID: // ZCL_VERSION
        cluster->zcl_version = *(uint8_t*)value;
        ESP_LOGI(TAG, "✅ ZCL Version = %u", cluster->zcl_version);
        break;

    case ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID: // APP_VERSION
        cluster->application_version = *(uint8_t*)value;
        ESP_LOGI(TAG, "✅ Application Version = %u", cluster->application_version);
        break;

    case ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID: // STACK_VERSION
        cluster->stack_version = *(uint8_t*)value;
        ESP_LOGI(TAG, "✅ Stack Version = %u", cluster->stack_version);
        break;

    case ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID: // HW_VERSION
        cluster->hw_version = *(uint8_t*)value;
        ESP_LOGI(TAG, "✅ Hardware Version = %u", cluster->hw_version);
        break;

    case ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID: // MANUFACTURER_NAME
        memset(cluster->manufacturer_name, 0, sizeof(cluster->manufacturer_name));
        data = (uint8_t*)value;
        len = data[0];
        if (len > 0 && len < sizeof(cluster->manufacturer_name)) {
            memcpy(cluster->manufacturer_name, data + 1, len);
            cluster->manufacturer_name[len] = '\0';
            ESP_LOGI(TAG, "✅ Manufacturer Name = '%s' (len=%u)", cluster->manufacturer_name, len);
        } else {
            cluster->manufacturer_name[0] = '\0';
            ESP_LOGW(TAG, "⚠️ Invalid manufacturer name length: %u", len);
        }
        break;

    case ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID: // MODEL_IDENTIFIER
        memset(cluster->model_identifier, 0, sizeof(cluster->model_identifier));
        data = (uint8_t*)value;
        len = data[0];
        if (len > 0 && len < sizeof(cluster->model_identifier)) {
            memcpy(cluster->model_identifier, data + 1, len);
            cluster->model_identifier[len] = '\0';
            ESP_LOGI(TAG, "✅ Model Identifier = '%s' (len=%u)", cluster->model_identifier, len);
        } else {
            cluster->model_identifier[0] = '\0';
            ESP_LOGW(TAG, "⚠️ Invalid model ID length: %u", len);
        }
        break;

    case ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID: // DATE_CODE
        memset(cluster->date_code, 0, sizeof(cluster->date_code));
        data = (uint8_t*)value;
        len = data[0];
        if (len > 0 && len < sizeof(cluster->date_code)) {
            memcpy(cluster->date_code, data + 1, len);
            cluster->date_code[len] = '\0';
            ESP_LOGI(TAG, "✅ Date Code = '%s' (len=%u)", cluster->date_code, len);
        } else {
            cluster->date_code[0] = '\0';
            ESP_LOGW(TAG, "⚠️ Invalid date code length: %u", len);
        }
        break;

    case ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID: // POWER_SOURCE
        cluster->power_source = *(uint8_t*)value;
        const char *ps_text = get_power_source_string(cluster->power_source);
        strncpy(cluster->power_source_text, ps_text, sizeof(cluster->power_source_text) - 1);
        cluster->power_source_text[sizeof(cluster->power_source_text) - 1] = '\0';
        ESP_LOGI(TAG, "✅ Power Source = 0x%02x → '%s'", cluster->power_source, ps_text);
        break;
    case ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_CLASS_ID: //ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_CLASS_ID
        cluster->generic_device_class = *(uint8_t*)value;
        cluster->last_update_ms = esp_log_timestamp();
        break;
    case ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_TYPE_ID: //ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_TYPE_ID
        cluster->generic_device_type = *(uint8_t*)value;
        cluster->last_update_ms = esp_log_timestamp();
        break;
    case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_CODE_ID: //ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_CODE_ID
        memset(cluster->product_code, 0, sizeof(cluster->product_code)); 
        data = (uint8_t*)value;
        len = data[0];
        if (len > 0 && len < sizeof(cluster->product_code)) {
            memcpy(cluster->product_code, data + 1, len);
            cluster->product_code[len] = '\0';
        } else {
            cluster->product_code[0] = '\0';
        }
        cluster->last_update_ms = esp_log_timestamp();
        break;
    case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_URL_ID: //ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_URL_ID
        memset(cluster->product_url, 0, sizeof(cluster->product_url)); // 256 - максимальная длина URL (не включая завершающий ноль
        data = (uint8_t*)value;
        len = data[0];
        if (len > 0 && len < sizeof(cluster->product_url)) {
            memcpy(cluster->product_url, data + 1, len);
            cluster->product_url[len] = '\0';
        } else {
            cluster->product_url[0] = '\0';
        }
        cluster->last_update_ms = esp_log_timestamp();
        break;
    case ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_VERSION_DETAILS_ID: //ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_VERSION_DETAILS_ID
        memset(cluster->manufacturer_version_details, 0, sizeof(cluster->manufacturer_version_details)); // 256 - максимальная длина URL (не включая завершающий ноль
        data = (uint8_t*)value;
        len = data[0];
        if (len > 0 && len < sizeof(cluster->manufacturer_version_details)) {
            memcpy(cluster->manufacturer_version_details, data + 1, len);
            cluster->manufacturer_version_details[len] = '\0';
        } else {
            cluster->manufacturer_version_details[0] = '\0';
        }
        cluster->last_update_ms = esp_log_timestamp();
        break;
    case ESP_ZB_ZCL_ATTR_BASIC_SERIAL_NUMBER_ID: //ESP_ZB_ZCL_ATTR_BASIC_SERIAL_NUMBER_ID
        memset(cluster->serial_number, 0, sizeof(cluster->serial_number)); // 256 - максимальная длина URL (не включая завершающий ноль
        data = (uint8_t*)value;
        len = data[0];
        if (len > 0 && len < sizeof(cluster->serial_number)) {
            memcpy(cluster->serial_number, data + 1, len);
            cluster->serial_number[len] = '\0';
        } else {
            cluster->serial_number[0] = '\0';
        }
        cluster->last_update_ms = esp_log_timestamp();
        break;
    case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_LABEL_ID: //ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_LABEL_ID
        memset(cluster->product_label, 0, sizeof(cluster->product_label)); // 256 - максимальная длина URL (не включая завершающий ноль
        data = (uint8_t*)value;
        len = data[0];
        if (len > 0 && len < sizeof(cluster->product_label)) {
            memcpy(cluster->product_label, data + 1, len);
            cluster->product_label[len] = '\0';
        } else {
            cluster->product_label[0] = '\0';
        }
        cluster->last_update_ms = esp_log_timestamp();
        break;
    case ESP_ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID: //ESP_ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID
        memset(cluster->location_description, 0, sizeof(cluster->location_description)); // 256 - максимальная длина URL (не включая завершающий ноль
        data = (uint8_t*)value;
        len = data[0];
        if (len > 0 && len < sizeof(cluster->location_description)) {
            memcpy(cluster->location_description, data + 1, len);
            cluster->location_description[len] = '\0';
        } else {
            cluster->location_description[0] = '\0';
        }
        cluster->last_update_ms = esp_log_timestamp();
        break;
    case ESP_ZB_ZCL_ATTR_BASIC_PHYSICAL_ENVIRONMENT_ID: //ESP_ZB_ZCL_ATTR_BASIC_PHYSICAL_ENVIRONMENT_ID
        cluster->physical_environment = *(uint8_t*)value;
        cluster->last_update_ms = esp_log_timestamp();
        break;
    case ESP_ZB_ZCL_ATTR_BASIC_DEVICE_ENABLED_ID: //ESP_ZB_ZCL_ATTR_BASIC_DEVICE_ENABLED_ID
        cluster->device_enabled = *(bool*)value;
        cluster->last_update_ms = esp_log_timestamp();
        break;
    case ESP_ZB_ZCL_ATTR_BASIC_ALARM_MASK_ID: //ESP_ZB_ZCL_ATTR_BASIC_ALARM_MASK_ID
        cluster->alarm_mask = *(uint8_t*)value;
        cluster->last_update_ms = esp_log_timestamp();
        break;
    case ESP_ZB_ZCL_ATTR_BASIC_DISABLE_LOCAL_CONFIG_ID: //ESP_ZB_ZCL_ATTR_BASIC_DISABLE_LOCAL_CONFIG_ID
        cluster->disable_local_config = *(bool*)value;
        cluster->last_update_ms = esp_log_timestamp();
        break;
    case ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID: //ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID
        memset(cluster->sw_build_id, 0, sizeof(cluster->sw_build_id)); //
        data = (uint8_t*)value;
        len = data[0];
        if (len > 0 && len < sizeof(cluster->sw_build_id)) {
            memcpy(cluster->sw_build_id, data + 1, len);
            cluster->sw_build_id[len] = '\0';
        } else {
            cluster->sw_build_id[0] = '\0';
        }
        cluster->last_update_ms = esp_log_timestamp();
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED;
        //break;
    }
    return ESP_OK;
}

/// @brief [zb_manager_basic_cluster.c] Преобразует значение атрибута Power Source в читаемую строку
/// @param power_source Значение атрибута (может включать бит вторичного источника)
/// @return Указатель на строку с описанием (статический буфер, если secondary)
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

/// @brief [zb_manager_basic_cluster.c] Возвращает текстовое имя атрибута Basic-кластера по его ID
/// @param attrID Идентификатор атрибута
/// @return Название атрибута или "Unknown Attribute"
const char* zb_manager_get_basic_cluster_attr_name(uint16_t attrID)
{
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID: //ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID:
            return "ZCL Version";
        case ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID: //ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID:
            return "Application Version";
        case ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID: //ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID:
            return "Stack Version";
        case ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID: //ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID:
            return "Hardware Version";
        case ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID: //ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID:
            return "Manufacturer Name";
        case ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID: //ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID:
            return "Model Identifier";
        case ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID: //ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID:
            return "Date Code";
        case ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID: //ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID:
            return "Power Source";
        case ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_CLASS_ID: //ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_CLASS_ID:
            return "Generic Device Class";
        case ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_TYPE_ID: //ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_TYPE_ID:
            return "Generic Device Type";
        case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_CODE_ID: //ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_CODE_ID:
            return "Product Code";
        case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_URL_ID: //ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_URL_ID:
            return "Product URL";
        case ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_VERSION_DETAILS_ID: //ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_VERSION_DETAILS_ID:
            return "Manufacturer Version Details";
        case ESP_ZB_ZCL_ATTR_BASIC_SERIAL_NUMBER_ID: //ESP_ZB_ZCL_ATTR_BASIC_SERIAL_NUMBER_ID:
            return "Serial Number";
        case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_LABEL_ID: //ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_LABEL_ID:
            return "Product Label";
        case ESP_ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID: //ESP_ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID:
            return "Location Description";
        case ESP_ZB_ZCL_ATTR_BASIC_PHYSICAL_ENVIRONMENT_ID: //ESP_ZB_ZCL_ATTR_BASIC_PHYSICAL_ENVIRONMENT_ID:
            return "Physical Environment";
        case ESP_ZB_ZCL_ATTR_BASIC_DEVICE_ENABLED_ID: //ESP_ZB_ZCL_ATTR_BASIC_DEVICE_ENABLED_ID:
            return "Device Enabled";
        case ESP_ZB_ZCL_ATTR_BASIC_ALARM_MASK_ID: //ESP_ZB_ZCL_ATTR_BASIC_ALARM_MASK_ID:
            return "Alarm Mask";
        case ESP_ZB_ZCL_ATTR_BASIC_DISABLE_LOCAL_CONFIG_ID: //ESP_ZB_ZCL_ATTR_BASIC_DISABLE_LOCAL_CONFIG_ID:
            return "Disable Local Config";
        case ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID: //ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID:
            return "SW Build";
        default:
            return "Unknown Attribute";
    }
}

uint8_t zb_manager_basic_factory_reset_cmd_req(esp_zb_zcl_basic_fact_reset_cmd_t *cmd_req)
{
    typedef struct {
        esp_zb_zcl_basic_cmd_t zcl_basic_cmd;                   /*!< Basic command info */
        uint8_t  address_mode;                                  /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */  
    } ESP_ZNSP_ZB_PACKED_STRUCT zb_manager_basic_reset_cmd_req;

    zb_manager_basic_reset_cmd_req data;
    data.address_mode = cmd_req->address_mode;
    if (cmd_req->address_mode == ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT) {
        memcpy(&data.zcl_basic_cmd.dst_addr_u.addr_long, cmd_req->zcl_basic_cmd.dst_addr_u.addr_long, 8);
    } else {
        data.zcl_basic_cmd.dst_addr_u.addr_short = cmd_req->zcl_basic_cmd.dst_addr_u.addr_short;
    }
    data.zcl_basic_cmd.dst_endpoint = cmd_req->zcl_basic_cmd.dst_endpoint;
    data.zcl_basic_cmd.src_endpoint = 1;

    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);
    if (zigbee_ncp_module_state == WORKING)
        {
            esp_host_zb_output(ZB_MANAGER_BASIC_RESET_CMD_REQ, &data, sizeof(data), &output, &outlen);
        }

    return output;
}
