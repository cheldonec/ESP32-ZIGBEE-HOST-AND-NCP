#include "zb_manager_automation.h"
#include "zb_manager_devices.h"
#include "zb_manager_clusters.h"
#include "esp_zigbee_zcl_command.h"
#include "esp_log.h"
#include "string.h"


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

static const char* TAG = "ZB_AUTO";

// Forward declarations
static esp_err_t send_on_off_command(const zb_automation_request_t* req);
static esp_err_t send_identify_command(const zb_automation_request_t* req);
static esp_err_t send_level_control_command(const zb_automation_request_t* req);

// ===================================================================
//                   Реестр команд (реализация)
// ===================================================================

static const zb_automation_command_t commands_on_off_light[] = {
    {
        .cmd_id = ZB_AUTO_CL_ON_OFF_CMD_ON,
        .name = "ON",
        .friendly_name = "Включить",
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        .param_type = ZB_AUTO_PARAM_NONE,
        .param_size = 0,
        .is_client_cmd = false
    },
    {
        .cmd_id = ZB_AUTO_CL_ON_OFF_CMD_OFF,
        .name = "OFF",
        .friendly_name = "Выключить",
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        .param_type = ZB_AUTO_PARAM_NONE,
        .param_size = 0,
        .is_client_cmd = false
    },
    {
        .cmd_id = ZB_AUTO_CL_ON_OFF_CMD_TOGGLE,
        .name = "TOGGLE",
        .friendly_name = "Переключить",
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        .param_type = ZB_AUTO_PARAM_NONE,
        .param_size = 0,
        .is_client_cmd = false
    },
    {
        .cmd_id = ZB_AUTO_CL_ON_OFF_CMD_ON_WITH_TIMED_OFF,
        .name = "ON_WITH_TIMED_OFF",
        .friendly_name = "Вкл с таймером",
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        .param_type = ZB_AUTO_PARAM_STRUCT_TIMED_OFF,
        .param_size = sizeof(zb_auto_params_timed_off_t),
        .is_client_cmd = false
    }
};

static const zb_automation_command_t commands_identify[] = {
    {
        .cmd_id = ZB_AUTO_CL_IDENTIFY_CMD_IDENTIFY,
        .name = "IDENTIFY",
        .friendly_name = "Опознать",
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY,
        .param_type = ZB_AUTO_PARAM_STRUCT_IDENTIFY,
        .param_size = sizeof(zb_auto_params_identify_t),
        .is_client_cmd = true
    }
};

static const zb_automation_command_t commands_dimmable_light[] = {
    {
        .cmd_id = ZB_AUTO_CL_ON_OFF_CMD_ON,
        .name = "ON",
        .friendly_name = "Включить",
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        .param_type = ZB_AUTO_PARAM_NONE,
        .param_size = 0,
        .is_client_cmd = false
    },
    {
        .cmd_id = ZB_AUTO_CL_ON_OFF_CMD_OFF,
        .name = "OFF",
        .friendly_name = "Выключить",
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        .param_type = ZB_AUTO_PARAM_NONE,
        .param_size = 0,
        .is_client_cmd = false
    },
    {
        .cmd_id = ZB_AUTO_CL_ON_OFF_CMD_TOGGLE,
        .name = "TOGGLE",
        .friendly_name = "Переключить",
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        .param_type = ZB_AUTO_PARAM_NONE,
        .param_size = 0,
        .is_client_cmd = false
    },
    {
        .cmd_id = ZB_AUTO_CL_LEVEL_CMD_MOVE_TO_LEVEL,
        .name = "MOVE_TO_LEVEL",
        .friendly_name = "Установить яркость",
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
        .param_type = ZB_AUTO_PARAM_STRUCT_MOVE_TO_LEVEL,
        .param_size = sizeof(zb_auto_params_move_to_level_t),
        .is_client_cmd = false
    },
    {
        .cmd_id = ZB_AUTO_CL_LEVEL_CMD_STOP,
        .name = "STOP",
        .friendly_name = "Остановить",
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
        .param_type = ZB_AUTO_PARAM_NONE,
        .param_size = 0,
        .is_client_cmd = false
    }
};

// Реестр профилей
typedef struct {
    const char* device_type;
    const zb_automation_command_t* commands;
    size_t count;
} automation_profile_t;

static const automation_profile_t profiles[] = {
    { .device_type = "on_off_light",       .commands = commands_on_off_light,     .count = ARRAY_SIZE(commands_on_off_light) },
    { .device_type = "dimmable_light",     .commands = commands_dimmable_light,   .count = ARRAY_SIZE(commands_dimmable_light) },
    { .device_type = "on_off_plugin_unit", .commands = commands_on_off_light,     .count = ARRAY_SIZE(commands_on_off_light) },
    { .device_type = "smart_plug",         .commands = commands_on_off_light,     .count = ARRAY_SIZE(commands_on_off_light) },
    { .device_type = "identify_sensor",    .commands = commands_identify,         .count = ARRAY_SIZE(commands_identify) },
};

// ===================================================================
//                      API Реализация
// ===================================================================

const zb_automation_command_t* zb_automation_get_commands_for_device_type(
    const char* device_type,
    size_t* count)
{
    for (int i = 0; i < ARRAY_SIZE(profiles); i++) {
        if (strcmp(profiles[i].device_type, device_type) == 0) {
            *count = profiles[i].count;
            return profiles[i].commands;
        }
    }
    *count = 0;
    return NULL;
}

const zb_automation_command_t* zb_automation_get_command_by_id(zb_automation_cmd_id_t cmd_id)
{
    for (int i = 0; i < ARRAY_SIZE(profiles); i++) {
        for (int j = 0; j < profiles[i].count; j++) {
            if (profiles[i].commands[j].cmd_id == cmd_id) {
                return &profiles[i].commands[j];
            }
        }
    }
    return NULL;
}

esp_err_t zb_automation_send_command(const zb_automation_request_t* req)
{
    const zb_automation_command_t* cmd = zb_automation_get_command_by_id(req->cmd_id);
    if (!cmd) {
        ESP_LOGE(TAG, "Unknown command ID: %d", req->cmd_id);
        return ESP_ERR_NOT_FOUND;
    }

    switch (cmd->cluster_id) {
        case ESP_ZB_ZCL_CLUSTER_ID_ON_OFF:
            ESP_LOGI(TAG,"RUN ON/OFF");
            return send_on_off_command(req);
        case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
            return send_identify_command(req);
        case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
            return send_level_control_command(req);
        default:
            ESP_LOGW(TAG, "No sender for cluster 0x%04x", cmd->cluster_id);
            return ESP_ERR_NOT_SUPPORTED;
    }
}

// ===================================================================
//                  Отправка реальных команд
// ===================================================================

static esp_err_t send_on_off_command(const zb_automation_request_t* req)
{
    

    switch (req->cmd_id) {
        case ZB_AUTO_CL_ON_OFF_CMD_ON:{
            esp_zb_zcl_on_off_cmd_t cmd_req = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = req->short_addr,
                .dst_endpoint = req->endpoint_id,
                .src_endpoint = 1,
            },
            .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            };

            cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_ON_ID;
            uint8_t seq = zb_manager_on_off_cmd_req(&cmd_req);
            ESP_LOGI(TAG, "Sent ON/OFF cmd=%d to 0x%04x ep=%d, seq=%d", cmd_req.on_off_cmd_id, req->short_addr, req->endpoint_id, seq);
            break;}
        case ZB_AUTO_CL_ON_OFF_CMD_OFF:{
            esp_zb_zcl_on_off_cmd_t cmd_req = {
                .zcl_basic_cmd = {
                    .dst_addr_u.addr_short = req->short_addr,
                    .dst_endpoint = req->endpoint_id,
                    .src_endpoint = 1,
                },
                .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                };

            cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID;
            uint8_t seq = zb_manager_on_off_cmd_req(&cmd_req);
            ESP_LOGI(TAG, "Sent ON/OFF cmd=%d to 0x%04x ep=%d, seq=%d", cmd_req.on_off_cmd_id, req->short_addr, req->endpoint_id, seq);
            break;}
        case ZB_AUTO_CL_ON_OFF_CMD_TOGGLE:{
            esp_zb_zcl_on_off_cmd_t cmd_req = {
                .zcl_basic_cmd = {
                    .dst_addr_u.addr_short = req->short_addr,
                    .dst_endpoint = req->endpoint_id,
                    .src_endpoint = 1,
                },
                .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            };
            cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
            uint8_t seq = zb_manager_on_off_cmd_req(&cmd_req);
            ESP_LOGI(TAG, "Sent ON/OFF cmd=%d to 0x%04x ep=%d, seq=%d", cmd_req.on_off_cmd_id, req->short_addr, req->endpoint_id, seq);
            break;}
        case ZB_AUTO_CL_ON_OFF_CMD_ON_WITH_TIMED_OFF:{
            esp_zb_zcl_on_off_on_with_timed_off_cmd_t cmd_req = {
                .zcl_basic_cmd = {
                    .dst_addr_u.addr_short = req->short_addr,
                    .dst_endpoint = req->endpoint_id,
                    .src_endpoint = 1,
                },
                .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                .on_time = req->params.timed_off.on_time,           
                .off_wait_time = req->params.timed_off.off_wait_time,
                .on_off_control = req->params.timed_off.on_off_control,      
                };
            
            ESP_LOGI(TAG, "ON_WITH_TIMED_OFF: on_time=%d, off_wait=%d",
                     req->params.timed_off.on_time, req->params.timed_off.off_wait_time);
            uint8_t seq = zb_manager_on_off_on_with_timed_off_cmd_req(&cmd_req);
            ESP_LOGI(TAG, "ON_WITH_TIMED_OFF  to 0x%04x ep=%d, seq=%d", req->short_addr, req->endpoint_id, seq);
            break;}
        default:
            return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t send_identify_command(const zb_automation_request_t* req)
{
    if (req->cmd_id != ZB_AUTO_CL_IDENTIFY_CMD_IDENTIFY) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_zb_zcl_identify_cmd_t cmd_req = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = req->short_addr,
            .dst_endpoint = req->endpoint_id,
            .src_endpoint = 1,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .identify_time = req->params.identify.identify_time
    };

    uint8_t seq = esp_zb_zcl_identify_cmd_req(&cmd_req);
    ESP_LOGI(TAG, "Sent IDENTIFY time=%d to 0x%04x ep=%d, seq=%d",
             cmd_req.identify_time, req->short_addr, req->endpoint_id, seq);

    return ESP_OK;
}

static esp_err_t send_level_control_command(const zb_automation_request_t* req)
{
    switch (req->cmd_id) {
        case ZB_AUTO_CL_LEVEL_CMD_MOVE_TO_LEVEL: {
            esp_zb_zcl_move_to_level_cmd_t cmd_req = {
                .zcl_basic_cmd = {
                    .dst_addr_u.addr_short = req->short_addr,
                    .dst_endpoint = req->endpoint_id,
                    .src_endpoint = 1,
                },
                .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                .level = req->params.move_to_level.level,
                .transition_time = req->params.move_to_level.transition_time
            };
            uint8_t seq = esp_zb_zcl_level_move_to_level_cmd_req(&cmd_req);
            ESP_LOGI(TAG, "MOVE_TO_LEVEL: level=%d, trans=%d, seq=%d",
                     cmd_req.level, cmd_req.transition_time, seq);
            break;
        }
        case ZB_AUTO_CL_LEVEL_CMD_STOP: {
            esp_zb_zcl_level_stop_cmd_t cmd_req = {
                .zcl_basic_cmd = {
                    .dst_addr_u.addr_short = req->short_addr,
                    .dst_endpoint = req->endpoint_id,
                    .src_endpoint = 1,
                },
                .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT
            };
            uint8_t seq = esp_zb_zcl_level_stop_cmd_req(&cmd_req);
            ESP_LOGI(TAG, "STOP LEVEL, seq=%d", seq);
            break;
        }
        default:
            return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

// ===================================================================
//                      JSON: To / From
// ===================================================================

cJSON* zb_automation_command_to_json(const zb_automation_command_t* cmd)
{
    if (!cmd) return NULL;

    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id", cmd->name);
    cJSON_AddStringToObject(obj, "name", cmd->friendly_name);
    cJSON_AddNumberToObject(obj, "cmd_id", cmd->cmd_id);
    cJSON_AddNumberToObject(obj, "cluster_id", cmd->cluster_id);

    if (cmd->param_type == ZB_AUTO_PARAM_NONE) {
        cJSON_AddStringToObject(obj, "params", "none");
    } else {
        cJSON* params = cJSON_CreateObject();
        switch (cmd->param_type) {
            case ZB_AUTO_PARAM_STRUCT_TIMED_OFF:
                cJSON_AddStringToObject(params, "type", "timed_off");
                cJSON_AddNumberToObject(params, "on_time", 600);
                cJSON_AddNumberToObject(params, "off_wait_time", 0);
                break;
            case ZB_AUTO_PARAM_STRUCT_IDENTIFY:
                cJSON_AddStringToObject(params, "type", "identify");
                cJSON_AddNumberToObject(params, "identify_time", 5);
                break;
            case ZB_AUTO_PARAM_STRUCT_MOVE_TO_LEVEL:
                cJSON_AddStringToObject(params, "type", "move_to_level");
                cJSON_AddNumberToObject(params, "level", 128);
                cJSON_AddNumberToObject(params, "transition_time", 10);
                break;
            default:
                cJSON_AddStringToObject(params, "type", "unknown");
                break;
        }
        cJSON_AddItemToObject(obj, "params", params);
    }

    return obj;
}

cJSON* zb_automation_get_commands_json(const char* device_type)
{
    size_t count;
    const zb_automation_command_t* cmds = zb_automation_get_commands_for_device_type(device_type, &count);
    if (!cmds) return cJSON_CreateArray();

    cJSON* arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON* cmd_json = zb_automation_command_to_json(&cmds[i]);
        if (cmd_json) cJSON_AddItemToArray(arr, cmd_json);
    }
    return arr;
}

esp_err_t zb_automation_request_from_json(cJSON* json, zb_automation_request_t* req)
{
    memset(req, 0, sizeof(*req));

    cJSON* short_addr = cJSON_GetObjectItem(json, "short_addr");
    cJSON* endpoint = cJSON_GetObjectItem(json, "endpoint");
    cJSON* cmd_id = cJSON_GetObjectItem(json, "cmd_id");
    cJSON* cmd_str = cJSON_GetObjectItem(json, "cmd");

    if (!short_addr || !endpoint || !(cmd_id || cmd_str)) {
        ESP_LOGE(TAG, "Missing required fields");
        return ESP_ERR_INVALID_ARG;
    }

    req->short_addr = short_addr->valueint;
    req->endpoint_id = endpoint->valueint;
    req->cmd_id = cmd_id ? cmd_id->valueint : 0;

    const zb_automation_command_t* cmd = zb_automation_get_command_by_id(req->cmd_id);
    if (!cmd) {
        ESP_LOGE(TAG, "Invalid command ID: %d", req->cmd_id);
        return ESP_ERR_NOT_FOUND;
    }

    req->has_params = (cmd->param_type != ZB_AUTO_PARAM_NONE);

    cJSON* params = cJSON_GetObjectItem(json, "params");
    if (params && req->has_params) {
        switch (cmd->param_type) {
            case ZB_AUTO_PARAM_STRUCT_TIMED_OFF:
                req->params.timed_off.on_time = cJSON_GetObjectItem(params, "on_time") ? cJSON_GetObjectItem(params, "on_time")->valueint : 600;
                req->params.timed_off.off_wait_time = cJSON_GetObjectItem(params, "off_wait_time") ? cJSON_GetObjectItem(params, "off_wait_time")->valueint : 0;
                break;
            case ZB_AUTO_PARAM_STRUCT_IDENTIFY:
                req->params.identify.identify_time = cJSON_GetObjectItem(params, "identify_time") ? cJSON_GetObjectItem(params, "identify_time")->valueint : 5;
                break;
            case ZB_AUTO_PARAM_STRUCT_MOVE_TO_LEVEL:
                req->params.move_to_level.level = cJSON_GetObjectItem(params, "level") ? cJSON_GetObjectItem(params, "level")->valueint : 128;
                req->params.move_to_level.transition_time = cJSON_GetObjectItem(params, "transition_time") ? cJSON_GetObjectItem(params, "transition_time")->valueint : 10;
                break;
            default:
                // Пока не поддерживаем UINT8/UINT16 или нет параметров
                break;
        }
    }

    return ESP_OK;
}

cJSON* zb_automation_request_to_json(const zb_automation_request_t* req)
{
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "short_addr", req->short_addr);
    cJSON_AddNumberToObject(obj, "endpoint", req->endpoint_id);
    cJSON_AddNumberToObject(obj, "cmd_id", req->cmd_id);

    const zb_automation_command_t* cmd = zb_automation_get_command_by_id(req->cmd_id);
    if (cmd) {
        cJSON_AddStringToObject(obj, "cmd", cmd->name);
    }

    if (req->has_params && cmd) {
        cJSON* params = cJSON_CreateObject();

        switch (cmd->param_type) {
            case ZB_AUTO_PARAM_STRUCT_TIMED_OFF:
                cJSON_AddNumberToObject(params, "on_time", req->params.timed_off.on_time);
                cJSON_AddNumberToObject(params, "off_wait_time", req->params.timed_off.off_wait_time);
                break;
            case ZB_AUTO_PARAM_STRUCT_IDENTIFY:
                cJSON_AddNumberToObject(params, "identify_time", req->params.identify.identify_time);
                break;
            case ZB_AUTO_PARAM_STRUCT_MOVE_TO_LEVEL:
                cJSON_AddNumberToObject(params, "level", req->params.move_to_level.level);
                cJSON_AddNumberToObject(params, "transition_time", req->params.move_to_level.transition_time);
                break;
            default:
                // Пока не поддерживаем UINT8/UINT16 или нет параметров
                break;
        }

        cJSON_AddItemToObject(obj, "params", params);
    }

    return obj;
}
