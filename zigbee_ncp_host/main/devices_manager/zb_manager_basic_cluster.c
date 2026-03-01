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
esp_err_t zb_manager_basic_cluster_update_attribute(zigbee_manager_basic_cluster_t* cluster,uint16_t attr_id,uint8_t attr_type,void* value,uint16_t value_len)
{
    uint8_t *data = NULL;
    uint8_t len = 0;

    if (value == NULL) {
        ESP_LOGW(TAG, "attr_id=0x%04x: value is NULL", attr_id);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Updating attr 0x%04x with value at %p", attr_id, value);
    cluster->last_update_ms = esp_log_timestamp();

    switch (attr_id)
    {
        // === Стандартные атрибуты Basic Cluster ===
        case ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID:
            cluster->zcl_version = *(uint8_t*)value;
            ESP_LOGI(TAG, "✅ ZCL Version = %u", cluster->zcl_version);
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID:
            cluster->application_version = *(uint8_t*)value;
            ESP_LOGI(TAG, "✅ Application Version = %u", cluster->application_version);
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID:
            cluster->stack_version = *(uint8_t*)value;
            ESP_LOGI(TAG, "✅ Stack Version = %u", cluster->stack_version);
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID:
            cluster->hw_version = *(uint8_t*)value;
            ESP_LOGI(TAG, "✅ Hardware Version = %u", cluster->hw_version);
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID:
            memset(cluster->manufacturer_name, 0, sizeof(cluster->manufacturer_name));
            data = (uint8_t*)value;
            len = data[0];
            if (len > 0 && len < sizeof(cluster->manufacturer_name)) {
                memcpy(cluster->manufacturer_name, data + 1, len);
                cluster->manufacturer_name[len] = '\0';
                ESP_LOGI(TAG, "✅ Manufacturer Name = '%s' (len=%u)", cluster->manufacturer_name, len);
            } else {
                ESP_LOGW(TAG, "⚠️ Invalid manufacturer name length: %u", len);
            }
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID:
            memset(cluster->model_identifier, 0, sizeof(cluster->model_identifier));
            data = (uint8_t*)value;
            len = data[0];
            if (len > 0 && len < sizeof(cluster->model_identifier)) {
                memcpy(cluster->model_identifier, data + 1, len);
                cluster->model_identifier[len] = '\0';
                ESP_LOGI(TAG, "✅ Model Identifier = '%s' (len=%u)", cluster->model_identifier, len);
            } else {
                ESP_LOGW(TAG, "⚠️ Invalid model ID length: %u", len);
            }
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID:
            memset(cluster->date_code, 0, sizeof(cluster->date_code));
            data = (uint8_t*)value;
            len = data[0];
            if (len > 0 && len < sizeof(cluster->date_code)) {
                memcpy(cluster->date_code, data + 1, len);
                cluster->date_code[len] = '\0';
                ESP_LOGI(TAG, "✅ Date Code = '%s' (len=%u)", cluster->date_code, len);
            } else {
                ESP_LOGW(TAG, "⚠️ Invalid date code length: %u", len);
            }
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID:
            cluster->power_source = *(uint8_t*)value;
            const char *ps_text = get_power_source_string(cluster->power_source);
            strncpy(cluster->power_source_text, ps_text, sizeof(cluster->power_source_text) - 1);
            cluster->power_source_text[sizeof(cluster->power_source_text) - 1] = '\0';
            ESP_LOGI(TAG, "✅ Power Source = 0x%02x → '%s'", cluster->power_source, ps_text);
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_CLASS_ID:
            cluster->generic_device_class = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_TYPE_ID:
            cluster->generic_device_type = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_CODE_ID:
            memset(cluster->product_code, 0, sizeof(cluster->product_code));
            data = (uint8_t*)value;
            len = data[0];
            if (len > 0 && len < sizeof(cluster->product_code)) {
                memcpy(cluster->product_code, data + 1, len);
                cluster->product_code[len] = '\0';
            } else {
                ESP_LOGW(TAG, "⚠️ Invalid product code length: %u", len);
            }
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_URL_ID:
            memset(cluster->product_url, 0, sizeof(cluster->product_url));
            data = (uint8_t*)value;
            len = data[0];
            if (len > 0 && len < sizeof(cluster->product_url)) {
                memcpy(cluster->product_url, data + 1, len);
                cluster->product_url[len] = '\0';
            } else {
                ESP_LOGW(TAG, "⚠️ Invalid product URL length: %u", len);
            }
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_VERSION_DETAILS_ID:
            memset(cluster->manufacturer_version_details, 0, sizeof(cluster->manufacturer_version_details));
            data = (uint8_t*)value;
            len = data[0];
            if (len > 0 && len < sizeof(cluster->manufacturer_version_details)) {
                memcpy(cluster->manufacturer_version_details, data + 1, len);
                cluster->manufacturer_version_details[len] = '\0';
            } else {
                ESP_LOGW(TAG, "⚠️ Invalid manufacturer version details length: %u", len);
            }
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_SERIAL_NUMBER_ID:
            memset(cluster->serial_number, 0, sizeof(cluster->serial_number));
            data = (uint8_t*)value;
            len = data[0];
            if (len > 0 && len < sizeof(cluster->serial_number)) {
                memcpy(cluster->serial_number, data + 1, len);
                cluster->serial_number[len] = '\0';
            } else {
                ESP_LOGW(TAG, "⚠️ Invalid serial number length: %u", len);
            }
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_LABEL_ID:
            memset(cluster->product_label, 0, sizeof(cluster->product_label));
            data = (uint8_t*)value;
            len = data[0];
            if (len > 0 && len < sizeof(cluster->product_label)) {
                memcpy(cluster->product_label, data + 1, len);
                cluster->product_label[len] = '\0';
            } else {
                ESP_LOGW(TAG, "⚠️ Invalid product label length: %u", len);
            }
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID:
            memset(cluster->location_description, 0, sizeof(cluster->location_description));
            data = (uint8_t*)value;
            len = data[0];
            if (len > 0 && len < sizeof(cluster->location_description)) {
                memcpy(cluster->location_description, data + 1, len);
                cluster->location_description[len] = '\0';
            } else {
                ESP_LOGW(TAG, "⚠️ Invalid location description length: %u", len);
            }
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_PHYSICAL_ENVIRONMENT_ID:
            cluster->physical_environment = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_DEVICE_ENABLED_ID:
            cluster->device_enabled = *(bool*)value;
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_ALARM_MASK_ID:
            cluster->alarm_mask = *(uint8_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_DISABLE_LOCAL_CONFIG_ID:
            cluster->disable_local_config = *(bool*)value;
            break;

        case ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID:
            memset(cluster->sw_build_id, 0, sizeof(cluster->sw_build_id));
            data = (uint8_t*)value;
            len = data[0];
            if (len > 0 && len < sizeof(cluster->sw_build_id)) {
                memcpy(cluster->sw_build_id, data + 1, len);
                cluster->sw_build_id[len] = '\0';
            } else {
                ESP_LOGW(TAG, "⚠️ Invalid SW build ID length: %u", len);
            }
            break;

        default: {
            attribute_custom_t *custom_attr = zb_manager_basic_cluster_find_custom_attr_obj(cluster, attr_id);
            if (custom_attr) {
                // Обновляем p_value
                if (custom_attr->p_value == NULL || custom_attr->size != value_len) {
                    if (custom_attr->p_value) free(custom_attr->p_value);
                    custom_attr->p_value = malloc(value_len);
                    if (!custom_attr->p_value) return ESP_ERR_NO_MEM;
                    custom_attr->size = value_len;
                }
                memcpy(custom_attr->p_value, value, value_len);
                //custom_attr->last_update_ms = esp_log_timestamp();

                ESP_LOGI(TAG, ".updated attr 0x%04x (type=0x%02x, len=%u)", attr_id, attr_type, value_len);
                return ESP_OK;
            } else {
                ESP_LOGI(TAG, "Auto-create attr 0x%04x (type=0x%02x, len=%u)", attr_id, attr_type, value_len);
                esp_err_t err = zb_manager_basic_cluster_add_custom_attribute(cluster, attr_id, attr_type);
                if (err != ESP_OK) return err;

                custom_attr = zb_manager_basic_cluster_find_custom_attr_obj(cluster, attr_id);
                if (custom_attr) {
                    custom_attr->p_value = malloc(value_len);
                    if (!custom_attr->p_value) return ESP_ERR_NO_MEM;
                    memcpy(custom_attr->p_value, value, value_len);
                    custom_attr->size = value_len;
                    //custom_attr->last_update_ms = esp_log_timestamp();
                    return ESP_OK;
                }
                return ESP_ERR_NOT_FOUND;
            }
        }
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

esp_err_t zb_manager_basic_cluster_add_custom_attribute(
    zigbee_manager_basic_cluster_t *cluster,
    uint16_t attr_id,
    uint8_t attr_type)
{
    if (!cluster) {
        ESP_LOGE(TAG, "Cluster is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Список стандартных атрибутов Basic-кластера
    bool is_standard = false;
    switch (attr_id) {
        case ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_CLASS_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_TYPE_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_CODE_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_URL_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_VERSION_DETAILS_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_SERIAL_NUMBER_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_LABEL_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_PHYSICAL_ENVIRONMENT_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_DEVICE_ENABLED_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_ALARM_MASK_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_DISABLE_LOCAL_CONFIG_ID:
        case ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID:
            is_standard = true;
            break;
        default:
            is_standard = false;
            break;
    }

    if (is_standard) {
        ESP_LOGD(TAG, "Attr 0x%04x is standard — skipping", attr_id);
        return ESP_ERR_NOT_SUPPORTED; // или ESP_OK — по желанию
    }

    // Проверка: уже есть такой атрибут?
    for (int i = 0; i < cluster->nostandart_attr_count; i++) {
        if (cluster->nostandart_attr_array[i] && cluster->nostandart_attr_array[i]->id == attr_id) {
            ESP_LOGD(TAG, "Attr 0x%04x already exists", attr_id);
            return ESP_OK;
        }
    }

    // Создаём новый атрибут
    attribute_custom_t *new_attr = calloc(1, sizeof(attribute_custom_t));
    if (!new_attr) {
        ESP_LOGE(TAG, "Failed to allocate memory for attribute 0x%04x", attr_id);
        return ESP_ERR_NO_MEM;
    }

    new_attr->id = attr_id;
    new_attr->type = attr_type;
    new_attr->parent_cluster_id = 0x0000; // Basic Cluster ID
    new_attr->size = zb_manager_get_zcl_attr_size(attr_type);
    new_attr->is_void_pointer = (attr_type >= 0x41 && attr_type <= 0x51); // строки, массивы и т.п.
    new_attr->acces = 0; // может быть обновлён позже
    new_attr->manuf_code = 0; // если известен — можно установить отдельно
    new_attr->p_value = NULL; // значение не выделяем здесь

    // Формируем текстовое имя атрибута
    snprintf(new_attr->attr_id_text, sizeof(new_attr->attr_id_text), "Custom_0x%04X", attr_id);

    // Реаллокируем массив атрибутов
    void *new_array = realloc(cluster->nostandart_attr_array,
                              (cluster->nostandart_attr_count + 1) * sizeof(attribute_custom_t*));
    if (!new_array) {
        free(new_attr);
        ESP_LOGE(TAG, "Failed to realloc nostandart_attr_array");
        return ESP_ERR_NO_MEM;
    }

    cluster->nostandart_attr_array = (attribute_custom_t**)new_array;
    cluster->nostandart_attr_array[cluster->nostandart_attr_count] = new_attr;
    cluster->nostandart_attr_count++;

    ESP_LOGI(TAG, "Added custom attribute: 0x%04x (type: 0x%02x)", attr_id, attr_type);
    return ESP_OK;
}

attribute_custom_t *zb_manager_basic_cluster_find_custom_attr_obj(zigbee_manager_basic_cluster_t *cluster, uint16_t attr_id)
{
    if (!cluster) {
        return NULL;
    }

    for (int i = 0; i < cluster->nostandart_attr_count; i++) {
        attribute_custom_t *attr = cluster->nostandart_attr_array[i];
        if (attr && attr->id == attr_id) {
            return attr;
        }
    }

    return NULL; // не найден
}
