#include "zb_manager_on_off_cluster.h"
#include "esp_log.h"
//#include "web_server.h"
#include "esp_zigbee_zcl_command.h"
#include "string.h"
#include "ncp_host_zb_api.h"
#include "zb_manager_devices.h"
#include "zb_manager_ncp_host.h"
#include "zbm_dev_base_utils.h"
static const char *TAG = "ON_OFF_CL";

/// @brief [zb_manager_on_off_cluster.c] Обновляет значение атрибута в On/Off-кластере Zigbee
/// @param cluster Указатель на структуру кластера
/// @param attr_id Идентификатор атрибута (например, 0x0000 — OnOff)
/// @param value Указатель на значение атрибута
/// @return ESP_OK при успехе, иначе ошибка
esp_err_t zb_manager_on_off_cluster_update_attribute(zb_manager_on_off_cluster_t* cluster,uint16_t attr_id,uint8_t attr_type,void* value,uint16_t value_len)
{
    if (value == NULL) {
        ESP_LOGW(TAG, "attr_id=0x%04x: value is NULL", attr_id);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Updating On/Off attr 0x%04x", attr_id);
    cluster->last_update_ms = esp_log_timestamp();

    switch (attr_id)
    {
        case ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID:
            cluster->on_off = *(bool*)value;
            break;

        case ESP_ZB_ZCL_ATTR_ON_OFF_GLOBAL_SCENE_CONTROL:
            cluster->global_scene_control = *(bool*)value;
            break;

        case ESP_ZB_ZCL_ATTR_ON_OFF_ON_TIME:
            cluster->on_time = *(uint16_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_ON_OFF_OFF_WAIT_TIME:
            cluster->off_wait_time = *(uint16_t*)value;
            break;

        case ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF:
        {
            uint8_t val = *(uint8_t*)value;
            if (val <= 0x02 || val == 0xFF) {
                cluster->start_up_on_off = val;
            } else {
                ESP_LOGW(TAG, "Invalid start_up_on_off value: %d", val);
                return ESP_ERR_INVALID_ARG;
            }
            break;
        }

        default: {
            attribute_custom_t *custom_attr = zb_manager_on_off_cluster_find_custom_attr_obj(cluster, attr_id);
            if (custom_attr) {
                if (custom_attr->p_value == NULL || custom_attr->size != value_len) {
                    if (custom_attr->p_value) free(custom_attr->p_value);
                    custom_attr->p_value = malloc(value_len);
                    if (!custom_attr->p_value) return ESP_ERR_NO_MEM;
                    custom_attr->size = value_len;
                }
                memcpy(custom_attr->p_value, value, value_len);
                //custom_attr->last_update_ms = esp_log_timestamp();

                ESP_LOGI(TAG, ".updated On/Off attr 0x%04x (type=0x%02x, len=%u)", attr_id, attr_type, value_len);
                return ESP_OK;
            } else {
                ESP_LOGI(TAG, "Auto-create On/Off attr 0x%04x (type=0x%02x)", attr_id, attr_type);
                esp_err_t err = zb_manager_on_off_cluster_add_custom_attribute(cluster, attr_id, attr_type);
                if (err != ESP_OK) return err;

                custom_attr = zb_manager_on_off_cluster_find_custom_attr_obj(cluster, attr_id);
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

    cluster->last_update_ms = esp_log_timestamp();
    return ESP_OK;
}

esp_err_t zb_manager_on_off_cluster_handle_command(zb_manager_on_off_cluster_t* cluster, uint8_t cmd_id, zigbee_on_off_apply_cb_t apply_cb, void* cb_user_data)
{
    bool new_state = cluster->on_off;

    switch (cmd_id)
    {
    case ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID:
        new_state = false;
        break;

    case ESP_ZB_ZCL_CMD_ON_OFF_ON_ID:
        new_state = true;
        break;

    case ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID:
        new_state = !cluster->on_off;
        break;

    case ESP_ZB_ZCL_CMD_ON_OFF_OFF_WITH_EFFECT_ID:
        // Не реализуем эффекты — просто выключаем
        new_state = false;
        break;

    case ESP_ZB_ZCL_CMD_ON_OFF_ON_WITH_RECALL_GLOBAL_SCENE_ID:
        // Если поддерживается сцена — включить с ней
        new_state = true;
        break;

    case ESP_ZB_ZCL_CMD_ON_OFF_ON_WITH_TIMED_OFF_ID:
        // Требует парсинга тайминга — в реальном случае запускать таймер
        // Сейчас просто включаем
        new_state = true;
        ESP_LOGW(TAG, "OnWithTimedOff not fully implemented (timer needed)");
        break;

    default:
        ESP_LOGW(TAG, "Unknown On/Off command: 0x%02x", cmd_id);
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Обновляем состояние
    cluster->on_off = new_state;
    cluster->last_update_ms = esp_log_timestamp();

    // Применяем к железу
    if (apply_cb) {
        apply_cb(new_state, cb_user_data);
    }

    ESP_LOGI(TAG, "On/Off state set to: %s", new_state ? "ON" : "OFF");
    return ESP_OK;
}

/// @brief [zb_manager_on_off_cluster.c] Возвращает текстовое имя атрибута On/Off-кластера по его ID
/// @param attrID Идентификатор атрибута
/// @return Название атрибута или "Unknown Attribute"
const char* zb_manager_get_on_off_cluster_attr_name(uint16_t attrID)
{
    switch (attrID) {
        case 0x0000: return "on_off";//ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID:
        case 0x4000: return "global_scene_control";//ESP_ZB_ZCL_ATTR_ON_OFF_GLOBAL_SCENE_CONTROL:
        case 0x4001: return "on_time";//ESP_ZB_ZCL_ATTR_ON_OFF_ON_TIME:
        case 0x4002: return "off_wait_time";//ESP_ZB_ZCL_ATTR_ON_OFF_OFF_WAIT_TIME:
        case 0x4003: return "start_up_on_off";//ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF:
        default: return "unknown";
    }
}

uint8_t zb_manager_read_on_off_attribute(uint16_t short_addr, uint8_t endpoint_id)
{
    //uint16_t attributes[] = {ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID};
    uint16_t local_attr = ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;
    esp_zb_zcl_read_attr_cmd_t read_cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = short_addr,
            .dst_endpoint = endpoint_id,
            .src_endpoint = 1,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        //.attr_number = sizeof(attributes) / sizeof(attributes[0]),
        .attr_number = 1,
        .attr_field = &local_attr,
    };
    //read_cmd.attr_field = calloc(read_cmd.attr_number, sizeof(uint16_t));
    //memcpy (read_cmd.attr_field, attributes, read_cmd.attr_number * sizeof(uint16_t));

    uint8_t tsn = zb_manager_zcl_read_attr_cmd_req(&read_cmd);
    //free(read_cmd.attr_field);
    return tsn;
}

uint8_t zb_manager_read_on_off_attribute_by_ieee(uint16_t short_addr, uint8_t endpoint_id)
{
     uint16_t local_attr = ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;
    esp_zb_zcl_read_attr_cmd_t read_cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = 0xffff,
            .dst_endpoint = endpoint_id,
            .src_endpoint = 1,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT,
        .clusterID = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        //.attr_number = sizeof(attributes) / sizeof(attributes[0]),
        .attr_number = 1,
        .attr_field = &local_attr,
    };
    

    device_custom_t* dev = zb_manager_find_device_by_short(short_addr);
    if (!dev) return 0xff;

    memcpy(&read_cmd.zcl_basic_cmd.dst_addr_u.addr_long, dev->ieee_addr, 8);

    //read_cmd.attr_field = calloc(read_cmd.attr_number, sizeof(uint16_t));
    //memcpy (read_cmd.attr_field, attributes, read_cmd.attr_number * sizeof(uint16_t));

    uint8_t tsn = zb_manager_zcl_read_attr_cmd_req(&read_cmd);
    //free(read_cmd.attr_field);
    return tsn;
}

uint8_t zb_manager_on_off_cmd_req(esp_zb_zcl_on_off_cmd_t *cmd_req)
{
    typedef struct {
        esp_zb_zcl_basic_cmd_t zcl_basic_cmd;                   /*!< Basic command info */
        uint8_t  address_mode;                                  /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
        uint8_t  on_off_cmd;                                    // ON, OFF, TOGGLE
    } ESP_ZNSP_ZB_PACKED_STRUCT zb_manager_on_off_main_cmd_req_t;

    zb_manager_on_off_main_cmd_req_t data;
    data.address_mode = cmd_req->address_mode;
    if (cmd_req->address_mode == ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT) {
        memcpy(&data.zcl_basic_cmd.dst_addr_u.addr_long, cmd_req->zcl_basic_cmd.dst_addr_u.addr_long, 8);
    } else {
        data.zcl_basic_cmd.dst_addr_u.addr_short = cmd_req->zcl_basic_cmd.dst_addr_u.addr_short;
    }
    data.zcl_basic_cmd.dst_endpoint = cmd_req->zcl_basic_cmd.dst_endpoint;
    data.zcl_basic_cmd.src_endpoint = 1;
    data.on_off_cmd = cmd_req->on_off_cmd_id;
    
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);
    if (zigbee_ncp_module_state == WORKING)
        {
            esp_host_zb_output(ZB_MANAGER_ON_OFF_MAIN_CMD_REQ, &data, sizeof(data), &output, &outlen);
        }

    return output;
}

uint8_t zb_manager_on_off_on_with_timed_off_cmd_req(esp_zb_zcl_on_off_on_with_timed_off_cmd_t *cmd_req)
{
    typedef struct {
        esp_zb_zcl_basic_cmd_t zcl_basic_cmd;                   /*!< Basic command info */
        uint8_t  address_mode;                                  /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
        uint8_t on_off_control;                 /*!< The field contains information on how the device is to be operated */
        uint16_t on_time;                       /*!< The field specifies the length of time (in 1/10ths second) that the device is to remain "on", i.e.,
                                                 with its OnOff attribute equal to 0x01, before automatically turning "off".*/
        uint16_t off_wait_time;                 /*!< The field specifies the length of time (in 1/10ths second) that the device SHALL remain "off", i.e.,
                                                 with its OnOff attribute equal to 0x00, and guarded to prevent an on command turning the device back "on" */
    } ESP_ZNSP_ZB_PACKED_STRUCT zb_manager_on_off_on_with_timed_off_cmd_req;

    zb_manager_on_off_on_with_timed_off_cmd_req data;
    data.address_mode = cmd_req->address_mode;
    if (cmd_req->address_mode == ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT) {
        memcpy(&data.zcl_basic_cmd.dst_addr_u.addr_long, cmd_req->zcl_basic_cmd.dst_addr_u.addr_long, 8);
    } else {
        data.zcl_basic_cmd.dst_addr_u.addr_short = cmd_req->zcl_basic_cmd.dst_addr_u.addr_short;
    }
    data.zcl_basic_cmd.dst_endpoint = cmd_req->zcl_basic_cmd.dst_endpoint;
    data.zcl_basic_cmd.src_endpoint = 1;
    data.on_off_control = cmd_req->on_off_control;
    data.on_time = cmd_req->on_time;
    data.off_wait_time = cmd_req->off_wait_time;

    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);
    if (zigbee_ncp_module_state == WORKING)
        {
            esp_host_zb_output(ZB_MANAGER_ON_OFF_ON_WITH_TIMED_OFF_CMD_REQ, &data, sizeof(data), &output, &outlen);
        }

    return output;
}

/**
 * @brief Configure reporting for On/Off cluster attribute (OnOff)
 *
 * This function sends a "Configure Reporting" command for the OnOff attribute (0x0000)
 * of the On/Off cluster (0x0006). The reportable change is boolean (0 or 1), so we pass it as uint8_t.
 *
 * @param short_addr Short address of the target device
 * @param endpoint Endpoint ID
 * @param min_interval Minimum reporting interval in seconds
 * @param max_interval Maximum reporting interval in seconds
 * @param change Reportable change: ignored for boolean, but can be used to disable reporting if 0xFF
 * @return ESP_OK on success, error otherwise
 */
esp_err_t zb_manager_configure_reporting_onoff_ext(uint16_t short_addr, uint8_t endpoint,
                                                   uint16_t min_interval, uint16_t max_interval, uint16_t change)
{
    // Для OnOff атрибута (bool) reportable_change — это просто байт
    uint8_t delta = (change == 0) ? 1 : (uint8_t)change; // обычно не используется, но можно зафиксировать как 1

    esp_zb_zcl_config_report_record_t record = {
        .direction = ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
        .attributeID = ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, // OnOff attribute
        .attrType = ESP_ZB_ZCL_ATTR_TYPE_BOOL,
        .min_interval = min_interval,
        .max_interval = max_interval,
        .reportable_change = &delta, // указатель на значение
        // .timeout not needed when sending
    };

    esp_zb_zcl_config_report_cmd_t cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = short_addr,
            .dst_endpoint = endpoint,
            .src_endpoint = 1,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .dis_default_resp = 1,
        .manuf_code = 0x115F,
        .record_number = 1,
        .record_field = &record,
    };

    esp_err_t err = zb_manager_reporting_config_req(&cmd);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "🟢 On/Off reporting configured: 0x%04x (ep=%d), min=%d, max=%d",
                 short_addr, endpoint, min_interval, max_interval);
    } else {
        ESP_LOGW(TAG, "🔴 Failed to configure On/Off reporting for 0x%04x", short_addr);
    }

    return err;
}

esp_err_t zb_manager_on_off_cluster_add_custom_attribute(
    zb_manager_on_off_cluster_t *cluster,
    uint16_t attr_id,
    uint8_t attr_type)
{
    if (!cluster) {
        ESP_LOGE(TAG, "Cluster is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Список стандартных атрибутов On/Off-кластера
    bool is_standard = false;
    switch (attr_id) {
        case ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID:               // 0x0000
        case ESP_ZB_ZCL_ATTR_ON_OFF_GLOBAL_SCENE_CONTROL:    // 0x4000
        case ESP_ZB_ZCL_ATTR_ON_OFF_ON_TIME:                 // 0x4001
        case ESP_ZB_ZCL_ATTR_ON_OFF_OFF_WAIT_TIME:           // 0x4002
        case ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF:         // 0x4003
            is_standard = true;
            break;
        default:
            is_standard = false;
            break;
    }

    if (is_standard) {
        ESP_LOGD(TAG, "Attr 0x%04x is standard — skipping", attr_id);
        return ESP_ERR_NOT_SUPPORTED; // можно заменить на ESP_OK, если нужно игнорировать молча
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
    new_attr->parent_cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF; // 0x0006
    new_attr->size = zb_manager_get_zcl_attr_size(attr_type);
    new_attr->is_void_pointer = (attr_type >= 0x41 && attr_type <= 0x51); // строки, массивы и т.п.
    new_attr->acces = 0; // может быть установлен позже
    new_attr->manuf_code = 0; // по умолчанию — нет manufacturer code
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

attribute_custom_t *zb_manager_on_off_cluster_find_custom_attr_obj(zb_manager_on_off_cluster_t *cluster, uint16_t attr_id)
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

    return NULL; // not found
}

cluster_custom_command_t *zb_manager_on_off_cluster_find_custom_command(
    zb_manager_on_off_cluster_t *cluster, uint8_t cmd_id)
{
    if (!cluster) return NULL;

    for (int i = 0; i < cluster->custom_cmd_count; i++) {
        if (cluster->custom_commands_array[i]->cmd_id == cmd_id) {
            return cluster->custom_commands_array[i];
        }
    }
    return NULL;
}

esp_err_t zb_manager_on_off_cluster_register_custom_command(
    zb_manager_on_off_cluster_t *cluster,
    uint8_t cmd_id)
{
    if (!cluster) {
        return ESP_ERR_INVALID_ARG;
    }

    // Проверим, нет ли уже такой команды
    for (int i = 0; i < cluster->custom_cmd_count; i++) {
        if (cluster->custom_commands_array[i]->cmd_id == cmd_id) {
            // Команда уже есть — ничего не делаем, или можно обновить имя?
            // Оставляем как есть
            return ESP_OK;
        }
    }

    // Увеличиваем массив
    cluster_custom_command_t **new_array = realloc(cluster->custom_commands_array,
        (cluster->custom_cmd_count + 1) * sizeof(cluster_custom_command_t*));
    if (!new_array) {
        return ESP_ERR_NO_MEM;
    }
    cluster->custom_commands_array = new_array;

    // Создаём новую команду
    cluster_custom_command_t *cmd = calloc(1, sizeof(cluster_custom_command_t));
    if (!cmd) {
        return ESP_ERR_NO_MEM;
    }

    cmd->cmd_id = cmd_id;

    // Автоматическое имя
    if (cmd_id == 0xFD) {
        strlcpy(cmd->cmd_friendly_name, "Кнопка", sizeof(cmd->cmd_friendly_name));
    } else {
        snprintf(cmd->cmd_friendly_name, sizeof(cmd->cmd_friendly_name),
                 "Custom Cmd 0x%02X", cmd_id);
    }

    // Добавляем в массив
    cluster->custom_commands_array[cluster->custom_cmd_count] = cmd;
    cluster->custom_cmd_count++;

    return ESP_OK;
}

/**
 * @brief Convert On/Off Cluster object to cJSON
 * @param cluster Pointer to the cluster
 * @return cJSON* - new JSON object, or NULL on failure
 */
cJSON* zbm_onoff_cluster_to_json(zb_manager_on_off_cluster_t *cluster)
{
    if (!cluster) {
        ESP_LOGW(TAG, "zbm_onoff_cluster_to_json: cluster is NULL");
        return NULL;
    }

    cJSON *onoff = cJSON_CreateObject();
    if (!onoff) {
        ESP_LOGE(TAG, "Failed to create On/Off cluster JSON object");
        return NULL;
    }

    // === Standard Attributes ===
    cJSON_AddNumberToObject(onoff, "cluster_id", 0x0006);

    // Основное состояние
    cJSON_AddBoolToObject(onoff, "on_off", cluster->on_off);
    cJSON_AddStringToObject(onoff, "state", cluster->on_off ? "ON" : "OFF");

    // Global Scene Control
    cJSON_AddBoolToObject(onoff, "global_scene_control", cluster->global_scene_control);

    // Timed values
    cJSON_AddNumberToObject(onoff, "on_time", cluster->on_time);                    // in 0.1s
    cJSON_AddNumberToObject(onoff, "on_time_sec", cluster->on_time / 10.0f);        // human-readable
    cJSON_AddNumberToObject(onoff, "off_wait_time", cluster->off_wait_time);
    cJSON_AddNumberToObject(onoff, "off_wait_time_sec", cluster->off_wait_time / 10.0f);

    // Startup behavior
    const char *startup_str = "Unknown";
    switch (cluster->start_up_on_off) {
        case 0x00: startup_str = "Off"; break;
        case 0x01: startup_str = "On"; break;
        case 0x02: startup_str = "Toggle"; break;
        case 0xFF: startup_str = "Previous"; break;
        default: break;
    }
    cJSON_AddNumberToObject(onoff, "start_up_on_off", cluster->start_up_on_off);
    cJSON_AddStringToObject(onoff, "start_up_on_off_str", startup_str);

    // Last update
    cJSON_AddNumberToObject(onoff, "last_update_ms", cluster->last_update_ms);

    // === Custom Attributes (nostandart_attributes) ===
    if (cluster->nostandart_attr_count > 0 && cluster->nostandart_attr_array) {
        cJSON *attrs = cJSON_CreateArray();
        if (attrs) {
            for (int i = 0; i < cluster->nostandart_attr_count; i++) {
                attribute_custom_t *attr = cluster->nostandart_attr_array[i];
                if (!attr) continue;

                cJSON *attr_obj = cJSON_CreateObject();
                if (!attr_obj) continue;

                cJSON_AddNumberToObject(attr_obj, "id", attr->id);
                cJSON_AddStringToObject(attr_obj, "attr_id_text", attr->attr_id_text);
                cJSON_AddNumberToObject(attr_obj, "type", attr->type);
                cJSON_AddNumberToObject(attr_obj, "acces", attr->acces);
                cJSON_AddNumberToObject(attr_obj, "size", attr->size);
                cJSON_AddNumberToObject(attr_obj, "is_void_pointer", attr->is_void_pointer);
                cJSON_AddNumberToObject(attr_obj, "manuf_code", attr->manuf_code);
                cJSON_AddNumberToObject(attr_obj, "parent_cluster_id", attr->parent_cluster_id);

                // Добавляем значение атрибута
                zbm_json_add_attribute_value(attr_obj, attr);

                cJSON_AddItemToArray(attrs, attr_obj);
            }
            cJSON_AddItemToObject(onoff, "nostandart_attributes", attrs);
        }
    }

    // === Custom Commands ===
    if (cluster->custom_cmd_count > 0 && cluster->custom_commands_array) {
        cJSON *cmds = cJSON_CreateArray();
        if (cmds) {
            for (int i = 0; i < cluster->custom_cmd_count; i++) {
                cluster_custom_command_t *cmd = cluster->custom_commands_array[i];
                if (!cmd) continue;

                cJSON *cmd_obj = cJSON_CreateObject();
                if (!cmd_obj) continue;

                cJSON_AddNumberToObject(cmd_obj, "cmd_id", cmd->cmd_id);
                cJSON_AddStringToObject(cmd_obj, "cmd_friendly_name", cmd->cmd_friendly_name);

                cJSON_AddItemToArray(cmds, cmd_obj);
            }
            cJSON_AddItemToObject(onoff, "custom_commands", cmds);
        }
    }

    return onoff;
}

/**
 * @brief Load On/Off Cluster from cJSON object
 * @param cluster Pointer to allocated or zeroed zb_manager_on_off_cluster_t
 * @param json_obj cJSON object representing the cluster
 * @return ESP_OK on success
 */
esp_err_t zbm_onoff_cluster_load_from_json(zb_manager_on_off_cluster_t *cluster, cJSON *json_obj)
{
    if (!cluster || !json_obj) {
        return ESP_ERR_INVALID_ARG;
    }

    // Инициализируем структуру
    *cluster = (zb_manager_on_off_cluster_t)ZIGBEE_ON_OFF_CLUSTER_DEFAULT_INIT();

    // === Standard Attributes ===
    cJSON *on_off_item = cJSON_GetObjectItem(json_obj, "on_off");
    if (on_off_item && cJSON_IsBool(on_off_item)) {
        cluster->on_off = cJSON_IsTrue(on_off_item);
    }

    cJSON *global_scene_control_item = cJSON_GetObjectItem(json_obj, "global_scene_control");
    if (global_scene_control_item && cJSON_IsBool(global_scene_control_item)) {
        cluster->global_scene_control = cJSON_IsTrue(global_scene_control_item);
    }

    LOAD_NUMBER(json_obj, "on_time", cluster->on_time);
    LOAD_NUMBER(json_obj, "off_wait_time", cluster->off_wait_time);

    cJSON *start_up_on_off_item = cJSON_GetObjectItem(json_obj, "start_up_on_off");
    if (start_up_on_off_item && cJSON_IsNumber(start_up_on_off_item)) {
        uint8_t val = (uint8_t)start_up_on_off_item->valueint;
        if (val == 0x00 || val == 0x01 || val == 0x02 || val == 0xFF) {
            cluster->start_up_on_off = val;
        } else {
            ESP_LOGW(TAG, "Invalid start_up_on_off value: %u, using default (0xFF)", val);
            cluster->start_up_on_off = 0xFF;
        }
    }

    // Last update timestamp
    cJSON *last_update_item = cJSON_GetObjectItem(json_obj, "last_update_ms");
    if (last_update_item && cJSON_IsNumber(last_update_item)) {
        cluster->last_update_ms = last_update_item->valueint;
    } else {
        cluster->last_update_ms = esp_log_timestamp();
    }

    // === Custom Attributes (nostandart_attributes) ===
    cJSON *attrs_json = cJSON_GetObjectItem(json_obj, "nostandart_attributes");
    if (attrs_json && cJSON_IsArray(attrs_json)) {
        int count = cJSON_GetArraySize(attrs_json);
        cluster->nostandart_attr_count = count;
        cluster->nostandart_attr_array = calloc(count, sizeof(attribute_custom_t*));
        if (!cluster->nostandart_attr_array) {
            return ESP_ERR_NO_MEM;
        }

        bool alloc_failed = false;
        for (int i = 0; i < count; i++) {
            cJSON *attr_json = cJSON_GetArrayItem(attrs_json, i);
            if (!attr_json) continue;

            attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
            if (!attr) {
                alloc_failed = true;
                continue;
            }

            attr->id = cJSON_GetObjectItem(attr_json, "id")->valueint;
            LOAD_STRING(attr_json, "attr_id_text", attr->attr_id_text);
            attr->type = cJSON_GetObjectItem(attr_json, "type")->valueint;
            attr->acces = cJSON_GetObjectItem(attr_json, "acces")->valueint;
            attr->size = cJSON_GetObjectItem(attr_json, "size")->valueint;
            attr->is_void_pointer = cJSON_GetObjectItem(attr_json, "is_void_pointer")->valueint;
            attr->manuf_code = cJSON_GetObjectItem(attr_json, "manuf_code")->valueint;
            attr->parent_cluster_id = cJSON_GetObjectItem(attr_json, "parent_cluster_id")->valueint;

            attr->p_value = NULL;
            zbm_json_load_attribute_value(attr, attr_json);

            cluster->nostandart_attr_array[i] = attr;
        }
        if (alloc_failed) {
            ESP_LOGW(TAG, "Partial failure loading custom attributes for On/Off cluster");
        }
    }

    // === Custom Commands ===
    cJSON *cmds_json = cJSON_GetObjectItem(json_obj, "custom_commands");
    if (cmds_json && cJSON_IsArray(cmds_json)) {
        int cmd_count = cJSON_GetArraySize(cmds_json);
        cluster->custom_cmd_count = cmd_count;
        cluster->custom_commands_array = calloc(cmd_count, sizeof(cluster_custom_command_t*));
        if (!cluster->custom_commands_array) {
            return ESP_ERR_NO_MEM;
        }

        bool cmd_alloc_failed = false;
        for (int i = 0; i < cmd_count; i++) {
            cJSON *cmd_json = cJSON_GetArrayItem(cmds_json, i);
            if (!cmd_json) continue;

            cluster_custom_command_t *cmd = calloc(1, sizeof(cluster_custom_command_t));
            if (!cmd) {
                cmd_alloc_failed = true;
                continue;
            }

            cmd->cmd_id = cJSON_GetObjectItem(cmd_json, "cmd_id")->valueint;
            LOAD_STRING(cmd_json, "cmd_friendly_name", cmd->cmd_friendly_name);

            cluster->custom_commands_array[i] = cmd;
        }
        if (cmd_alloc_failed) {
            ESP_LOGW(TAG, "Partial failure loading custom commands for On/Off cluster");
        }
    }

    return ESP_OK;
}
