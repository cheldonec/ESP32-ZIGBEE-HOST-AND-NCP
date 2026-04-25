// main/zbm_automation/zbm_rules_v2.c
#include "zbm_automation_v2.h"
#include "zbm_spiffs_helper.h"
#include "esp_log.h"
#include "string.h"
#include "cJSON.h"
#include "zbm_attr_types.h"

static const char* TAG = "ZB_RULES_V2";

// Внешние глобальные переменные (объявлены в .h)
extern zb_rule_t* zb_rules[ZB_AUTO_MAX_RULES];
extern uint8_t zb_rules_count;

// Статическая вспомогательная функция
static bool load_rule_from_file(const char* path, zb_rule_t* out_rule);

bool is_time_in_range(const zb_time_range_t* tr) {
    if (!tr->enabled) return true; // always

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Проверка дня недели: tm_wday: 0=вс, 1=пн, ..., 6=сб → сдвигаем на понедельник
    uint8_t weekday_bit = (timeinfo.tm_wday == 0) ? 6 : (timeinfo.tm_wday - 1); // 0=пн, 6=вс → биты: 0..6
    if (!(tr->days_of_week & (1 << weekday_bit))) {
        return false;
    }

    uint16_t current = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    uint16_t from = tr->from_hour * 60 + tr->from_min;
    uint16_t to = tr->to_hour * 60 + tr->to_min;

    if (from < to) {
        return current >= from && current <= to;
    } else {
        // Пересекает полночь: например, 22:00 → 06:00
        return current >= from || current <= to;
    }
}

// Внешняя функция для выполнения правила (объявлена ниже)
void execute_rule(const zb_rule_t* rule, const char* trigger_guid) {
    ESP_LOGI(TAG, "🔥 Rule fired: %s (triggered by: %s)", rule->name, trigger_guid);
    ws_notify_automation_rule_fired(rule->id, trigger_guid);
    for (int i = 0; i < rule->action_count; i++) {
        zb_automation_v2_execute_action(&rule->actions[i]);
    }
}

// === Парсер JSON -> zb_rule_t (V2) ===
bool rule_from_json(cJSON* json, zb_rule_t* out_rule) {
    if (!json || !out_rule) return false;

    memset(out_rule, 0, sizeof(zb_rule_t));

    // === ID ===
    cJSON* id_obj = cJSON_GetObjectItem(json, "id");
    if (!id_obj || !cJSON_IsString(id_obj) || strlen(id_obj->valuestring) == 0) {
        ESP_LOGE(TAG, "Missing or invalid rule ID");
        return false;
    }
    strlcpy(out_rule->id, id_obj->valuestring, sizeof(out_rule->id));

    // === Name ===
    cJSON* name_obj = cJSON_GetObjectItem(json, "name");
    if (name_obj && cJSON_IsString(name_obj)) {
        strlcpy(out_rule->name, name_obj->valuestring, sizeof(out_rule->name));
    } else {
        strcpy(out_rule->name, "Unnamed Rule");
    }

    // === Enabled ===
    out_rule->enabled = cJSON_IsTrue(cJSON_GetObjectItem(json, "enabled"));

    // === Priority ===
    cJSON* priority_obj = cJSON_GetObjectItem(json, "priority");
    if (priority_obj && cJSON_IsNumber(priority_obj)) {
        out_rule->priority = (int8_t)priority_obj->valueint;
    }

    // === Execution Mode ===
    cJSON* exec_mode_obj = cJSON_GetObjectItem(json, "exec_mode");
    if (exec_mode_obj && cJSON_IsNumber(exec_mode_obj)) {
        out_rule->exec_mode = (exec_mode_obj->valueint == 1) ? ZB_RULE_EXEC_ALL : ZB_RULE_EXEC_FIRST;
    } else {
        out_rule->exec_mode = ZB_RULE_EXEC_FIRST;
    }

    // === Logic Op for allowing triggers ===
    cJSON* logic_op_obj = cJSON_GetObjectItem(json, "allowing_logic_op");
    if (logic_op_obj && cJSON_IsNumber(logic_op_obj)) {
        out_rule->allowing_logic_op = (logic_op_obj->valueint == 1) ? ZB_LOGIC_AND : ZB_LOGIC_OR;
    } else {
        out_rule->allowing_logic_op = ZB_LOGIC_OR;
    }

    // === Cause Trigger ===
    cJSON* cause_obj = cJSON_GetObjectItem(json, "cause_trigger");
    if (cause_obj) {
        cJSON* guid_obj = cJSON_GetObjectItem(cause_obj, "guid");
        cJSON* cond_obj = cJSON_GetObjectItem(cause_obj, "cond");
        cJSON* expected_type_obj = cJSON_GetObjectItem(cause_obj, "expected_type");
        cJSON* value_obj = cJSON_GetObjectItem(cause_obj, "value");

        if (!guid_obj || !cJSON_IsString(guid_obj)) {
            ESP_LOGE(TAG, "Cause trigger: missing GUID");
            return false;
        }
        strlcpy(out_rule->cause_trigger.guid, guid_obj->valuestring, sizeof(out_rule->cause_trigger.guid));

        out_rule->cause_trigger.cond = ZB_COND_EQ;
        if (cond_obj && cJSON_IsNumber(cond_obj) && cond_obj->valueint >= 0 && cond_obj->valueint <= 5) {
            out_rule->cause_trigger.cond = (zb_condition_t)cond_obj->valueint;
        }

        out_rule->cause_trigger.expected_type = ZBM_ATTR_TYPE_U8; // fallback
        if (expected_type_obj && cJSON_IsNumber(expected_type_obj)) {
            out_rule->cause_trigger.expected_type = (zbm_attr_data_types_t)expected_type_obj->valueint;
        }

        // Выделяем память под значение
        uint16_t val_size = zbm_get_attr_size(out_rule->cause_trigger.expected_type);
        out_rule->cause_trigger.p_expected_value = calloc(1, val_size);
        if (!out_rule->cause_trigger.p_expected_value) {
            ESP_LOGE(TAG, "Failed to allocate memory for cause trigger value");
            return false;
        }
        out_rule->cause_trigger.expected_size = val_size;

        if (value_obj) {
            switch (out_rule->cause_trigger.expected_type) {
                case ZBM_ATTR_TYPE_U8:
                    *(uint8_t*)out_rule->cause_trigger.p_expected_value = (uint8_t)value_obj->valueint;
                    break;
                case ZBM_ATTR_TYPE_S8:
                    *(int8_t*)out_rule->cause_trigger.p_expected_value = (int8_t)value_obj->valueint;
                    break;
                case ZBM_ATTR_TYPE_U16:
                    *(uint16_t*)out_rule->cause_trigger.p_expected_value = (uint16_t)value_obj->valueint;
                    break;
                case ZBM_ATTR_TYPE_CHAR_STRING:
                case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
                    if (cJSON_IsString(value_obj)) {
                        strlcpy((char*)out_rule->cause_trigger.p_expected_value, value_obj->valuestring,
                                zbm_get_attr_size(out_rule->cause_trigger.expected_type));
                    } else {
                        snprintf((char*)out_rule->cause_trigger.p_expected_value,
                                 zbm_get_attr_size(out_rule->cause_trigger.expected_type), "%d", value_obj->valueint);
                    }
                    break;
                default:
                    *(uint8_t*)out_rule->cause_trigger.p_expected_value = (uint8_t)value_obj->valueint;
                    break;
            }
        } else {
            *(uint8_t*)out_rule->cause_trigger.p_expected_value = 1;
        }

        out_rule->cause_trigger.mode = ZB_TRIGGER_CAUSING;
    } else {
        ESP_LOGE(TAG, "Missing cause_trigger");
        return false;
    }

    // === Allowing Triggers ===
    cJSON* triggers_arr = cJSON_GetObjectItem(json, "allowing_triggers");
    out_rule->trigger_count = 0;
    if (triggers_arr && cJSON_IsArray(triggers_arr)) {
        int count = cJSON_GetArraySize(triggers_arr);
        for (int i = 0; i < count && out_rule->trigger_count < ZB_AUTO_MAX_TRIGGERS; i++) {
            cJSON* t_obj = cJSON_GetArrayItem(triggers_arr, i);
            if (!t_obj) continue;

            cJSON* t_guid = cJSON_GetObjectItem(t_obj, "guid");
            cJSON* t_cond = cJSON_GetObjectItem(t_obj, "cond");
            cJSON* t_type = cJSON_GetObjectItem(t_obj, "expected_type");
            cJSON* t_value = cJSON_GetObjectItem(t_obj, "value");

            if (!t_guid || !cJSON_IsString(t_guid)) continue;

            zb_trigger_t* t = &out_rule->triggers[out_rule->trigger_count];

            strlcpy(t->guid, t_guid->valuestring, sizeof(t->guid));
            t->cond = (t_cond && cJSON_IsNumber(t_cond)) ? (zb_condition_t)t_cond->valueint : ZB_COND_EQ;
            t->expected_type = (t_type && cJSON_IsNumber(t_type)) ? (zbm_attr_data_types_t)t_type->valueint : ZBM_ATTR_TYPE_U8;
            t->mode = ZB_TRIGGER_ALLOWING;

            uint16_t val_size = zbm_get_attr_size(t->expected_type);
            t->p_expected_value = calloc(1, val_size);
            t->expected_size = val_size;
            if (!t->p_expected_value) continue;

            if (t_value) {
                switch (t->expected_type) {
                    case ZBM_ATTR_TYPE_U8:
                        *(uint8_t*)t->p_expected_value = (uint8_t)t_value->valueint;
                        break;
                    case ZBM_ATTR_TYPE_S8:
                        *(int8_t*)t->p_expected_value = (int8_t)t_value->valueint;
                        break;
                    case ZBM_ATTR_TYPE_U16:
                        *(uint16_t*)t->p_expected_value = (uint16_t)t_value->valueint;
                        break;
                    case ZBM_ATTR_TYPE_CHAR_STRING:
                    case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
                        if (cJSON_IsString(t_value)) {
                            strlcpy((char*)t->p_expected_value, t_value->valuestring, zbm_get_attr_size(t->expected_type));
                        } else {
                            snprintf((char*)t->p_expected_value, zbm_get_attr_size(t->expected_type), "%d", t_value->valueint);
                        }
                        break;
                    default:
                        *(uint8_t*)t->p_expected_value = (uint8_t)t_value->valueint;
                        break;
                }
            } else {
                *(uint8_t*)t->p_expected_value = 1;
            }

            out_rule->trigger_count++;
        }
    }

    // === Actions ===
    cJSON* actions_arr = cJSON_GetObjectItem(json, "actions");
    out_rule->action_count = 0;
    if (actions_arr && cJSON_IsArray(actions_arr)) {
        int count = cJSON_GetArraySize(actions_arr);
        for (int i = 0; i < count && out_rule->action_count < ZB_AUTO_MAX_ACTIONS; i++) {
            cJSON* a_obj = cJSON_GetArrayItem(actions_arr, i);
            if (!a_obj) continue;

            cJSON* type_obj = cJSON_GetObjectItem(a_obj, "type");
            if (!type_obj || !cJSON_IsNumber(type_obj)) continue;

            int action_type = type_obj->valueint;
            zb_action_t* act = &out_rule->actions[out_rule->action_count];

            if (action_type == 0) {
                // Send command by GUID
                cJSON* cmd_guid = cJSON_GetObjectItem(a_obj, "cmd_guid");
                if (cmd_guid && cJSON_IsString(cmd_guid)) {
                    act->type = ZB_ACTION_SEND_CMD_BY_GUID;
                    strlcpy(act->data.send_cmd.cmd_guid, cmd_guid->valuestring, sizeof(act->data.send_cmd.cmd_guid));

                    cJSON* params = cJSON_GetObjectItem(a_obj, "params");
                    if (params) {
                        act->data.send_cmd.params = cJSON_Duplicate(params, true);
                    } else {
                        act->data.send_cmd.params = NULL;
                    }
                }
            } else if (action_type == 1) {
                // Set variable
                cJSON* var_idx_obj = cJSON_GetObjectItem(a_obj, "var_idx");
                cJSON* value_obj = cJSON_GetObjectItem(a_obj, "value");
                if (!var_idx_obj || !cJSON_IsNumber(var_idx_obj)) continue;

                uint8_t var_idx = (uint8_t)var_idx_obj->valueint;
                if (var_idx >= ZB_AUTO_VAR_COUNT) continue;

                zbm_virtual_var_t* var = &zbm_vars[var_idx];
                act->data.set_uint8.var_idx = var_idx;

                switch (var->data_type) {
                    case ZBM_ATTR_TYPE_U8:
                        act->type = ZB_ACTION_SET_VAR_UINT8;
                        act->data.set_uint8.value = (value_obj) ? (uint8_t)value_obj->valueint : 0;
                        break;
                    case ZBM_ATTR_TYPE_S8:
                        act->type = ZB_ACTION_SET_VAR_INT8;
                        act->data.set_int8.value = (value_obj) ? (int8_t)value_obj->valueint : 0;
                        break;
                    case ZBM_ATTR_TYPE_U16:
                        act->type = ZB_ACTION_SET_VAR_UINT16;
                        act->data.set_uint16.value = (value_obj) ? (uint16_t)value_obj->valueint : 0;
                        break;
                    case ZBM_ATTR_TYPE_CHAR_STRING:
                    case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
                        act->type = ZB_ACTION_SET_VAR_STRING;
                        if (value_obj) {
                            if (cJSON_IsString(value_obj)) {
                                strlcpy(act->data.set_str.str, value_obj->valuestring, sizeof(act->data.set_str.str));
                            } else {
                                snprintf(act->data.set_str.str, sizeof(act->data.set_str.str), "%d", value_obj->valueint);
                            }
                        } else {
                            strcpy(act->data.set_str.str, "");
                        }
                        break;
                    default:
                        act->type = ZB_ACTION_SET_VAR_UINT8;
                        act->data.set_uint8.value = (value_obj) ? (uint8_t)value_obj->valueint : 0;
                        break;
                }
            }

            out_rule->action_count++;
        }
    }

    // === Time Range ===
    // === Time Range ===
    cJSON* time_range_obj = cJSON_GetObjectItem(json, "time_range");
    out_rule->time_range.enabled = false; // по умолчанию — не ограничено
    out_rule->time_range.from_hour = 0;
    out_rule->time_range.from_min = 0;
    out_rule->time_range.to_hour = 23;
    out_rule->time_range.to_min = 59;
    out_rule->time_range.days_of_week = 0x7F; // все дни

    if (time_range_obj && cJSON_IsObject(time_range_obj)) {
        // Парсим enabled: если явно указано, значит ограничение активно
        cJSON* enabled_obj = cJSON_GetObjectItem(time_range_obj, "enabled");
        out_rule->time_range.enabled = cJSON_IsTrue(enabled_obj);

        // Парсим from/to
        cJSON* from_obj = cJSON_GetObjectItem(time_range_obj, "from");
        if (from_obj && cJSON_IsString(from_obj)) {
            int h, m;
            if (sscanf(from_obj->valuestring, "%d:%d", &h, &m) == 2) {
                out_rule->time_range.from_hour = (uint8_t)(h % 24);
                out_rule->time_range.from_min = (uint8_t)(m % 60);
            }
        }

        cJSON* to_obj = cJSON_GetObjectItem(time_range_obj, "to");
        if (to_obj && cJSON_IsString(to_obj)) {
            int h, m;
            if (sscanf(to_obj->valuestring, "%d:%d", &h, &m) == 2) {
                out_rule->time_range.to_hour = (uint8_t)(h % 24);
                out_rule->time_range.to_min = (uint8_t)(m % 60);
            }
        }

        cJSON* days_obj = cJSON_GetObjectItem(time_range_obj, "days");
        if (days_obj && cJSON_IsArray(days_obj)) {
            out_rule->time_range.days_of_week = 0;
            int count = cJSON_GetArraySize(days_obj);
            for (int i = 0; i < count; i++) {
                int d = cJSON_GetArrayItem(days_obj, i)->valueint;
                if (d >= 0 && d <= 6) {
                    out_rule->time_range.days_of_week |= (1 << d);
                }
            }
        }
    }

    return true;
}

// === Сериализация: zb_rule_t -> cJSON* ===
cJSON* rule_to_json(const zb_rule_t* rule) {
    if (!rule) return NULL;

    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "id", rule->id);
    cJSON_AddStringToObject(json, "name", rule->name);
    cJSON_AddBoolToObject(json, "enabled", rule->enabled);
    cJSON_AddNumberToObject(json, "priority", rule->priority);
    cJSON_AddNumberToObject(json, "exec_mode", rule->exec_mode);
    cJSON_AddNumberToObject(json, "allowing_logic_op", rule->allowing_logic_op);

    // === cause_trigger ===
    cJSON* cause_json = cJSON_CreateObject();
    cJSON_AddStringToObject(cause_json, "guid", rule->cause_trigger.guid);
    cJSON_AddNumberToObject(cause_json, "cond", rule->cause_trigger.cond);
    cJSON_AddNumberToObject(cause_json, "expected_type", rule->cause_trigger.expected_type);

    switch (rule->cause_trigger.expected_type) {
        case ZBM_ATTR_TYPE_BOOL:
        case ZBM_ATTR_TYPE_U8:
        case ZBM_ATTR_TYPE_S8:
            cJSON_AddNumberToObject(cause_json, "value", *(uint8_t*)rule->cause_trigger.p_expected_value);
            break;
        case ZBM_ATTR_TYPE_U16:
        case ZBM_ATTR_TYPE_S16:
            cJSON_AddNumberToObject(cause_json, "value", *(uint16_t*)rule->cause_trigger.p_expected_value);
            break;
        case ZBM_ATTR_TYPE_CHAR_STRING:
        case ZBM_ATTR_TYPE_LONG_CHAR_STRING: {
            uint16_t size = zbm_get_attr_size(rule->cause_trigger.expected_type);
            char* str = (char*)rule->cause_trigger.p_expected_value;
            for (int i = 0; i < size; i++) {
                if (str[i] == '\0') {
                    str = strndup(str, i);
                    cJSON_AddStringToObject(cause_json, "value", str);
                    free(str);
                    goto end_cause_str;
                }
            }
            cJSON_AddStringToObject(cause_json, "value", (char*)rule->cause_trigger.p_expected_value);
        end_cause_str:
            break;
        }
        default:
            cJSON_AddNumberToObject(cause_json, "value", *(uint8_t*)rule->cause_trigger.p_expected_value);
            break;
    }
    cJSON_AddItemToObject(json, "cause_trigger", cause_json);

    // === allowing_triggers ===
    cJSON* allowing_json = cJSON_CreateArray();
    for (int i = 0; i < rule->trigger_count; i++) {
        const zb_trigger_t* t = &rule->triggers[i];
        cJSON* t_json = cJSON_CreateObject();
        cJSON_AddStringToObject(t_json, "guid", t->guid);
        cJSON_AddNumberToObject(t_json, "cond", t->cond);
        cJSON_AddNumberToObject(t_json, "expected_type", t->expected_type);

        switch (t->expected_type) {
            case ZBM_ATTR_TYPE_BOOL:
            case ZBM_ATTR_TYPE_U8:
            case ZBM_ATTR_TYPE_S8:
                cJSON_AddNumberToObject(t_json, "value", *(uint8_t*)t->p_expected_value);
                break;
            case ZBM_ATTR_TYPE_U16:
            case ZBM_ATTR_TYPE_S16:
                cJSON_AddNumberToObject(t_json, "value", *(uint16_t*)t->p_expected_value);
                break;
            case ZBM_ATTR_TYPE_CHAR_STRING:
            case ZBM_ATTR_TYPE_LONG_CHAR_STRING: {
                uint16_t size = zbm_get_attr_size(t->expected_type);
                char* str = (char*)t->p_expected_value;
                for (int i = 0; i < size; i++) {
                    if (str[i] == '\0') {
                        str = strndup(str, i);
                        cJSON_AddStringToObject(t_json, "value", str);
                        free(str);
                        goto end_allow_str;
                    }
                }
                cJSON_AddStringToObject(t_json, "value", (char*)t->p_expected_value);
            end_allow_str:
                break;
            }
            default:
                cJSON_AddNumberToObject(t_json, "value", *(uint8_t*)t->p_expected_value);
                break;
        }
        cJSON_AddItemToArray(allowing_json, t_json);
    }
    cJSON_AddItemToObject(json, "allowing_triggers", allowing_json);

    // === Действия ===
    cJSON* actions = cJSON_CreateArray();
    for (int i = 0; i < rule->action_count; i++) {
        const zb_action_t* a = &rule->actions[i];
        cJSON* a_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(a_json, "type", a->type);

        switch (a->type) {
            case ZB_ACTION_SEND_CMD_BY_GUID:
                cJSON_AddStringToObject(a_json, "cmd_guid", a->data.send_cmd.cmd_guid);
                if (a->data.send_cmd.params) {
                    cJSON_AddItemToObject(a_json, "params", cJSON_Duplicate(a->data.send_cmd.params, true));
                }
                break;
            case ZB_ACTION_SET_VAR_UINT8:
                cJSON_AddNumberToObject(a_json, "var_idx", a->data.set_uint8.var_idx);
                cJSON_AddNumberToObject(a_json, "value", a->data.set_uint8.value);
                break;
            case ZB_ACTION_SET_VAR_INT8:
                cJSON_AddNumberToObject(a_json, "var_idx", a->data.set_int8.var_idx);
                cJSON_AddNumberToObject(a_json, "value", a->data.set_int8.value);
                break;
            case ZB_ACTION_SET_VAR_UINT16:
                cJSON_AddNumberToObject(a_json, "var_idx", a->data.set_uint16.var_idx);
                cJSON_AddNumberToObject(a_json, "value", a->data.set_uint16.value);
                break;
            case ZB_ACTION_SET_VAR_STRING:
                cJSON_AddNumberToObject(a_json, "var_idx", a->data.set_str.var_idx);
                cJSON_AddStringToObject(a_json, "value", a->data.set_str.str);
                break;
            case ZB_ACTION_INC_VAR:
            case ZB_ACTION_DEC_VAR:
            case ZB_ACTION_TOGGLE_VAR:
                cJSON_AddNumberToObject(a_json, "var_idx", a->data.inc.var_idx);
                break;
            default:
                break;
        }
        cJSON_AddItemToArray(actions, a_json);
    }
    cJSON_AddItemToObject(json, "actions", actions);

    // === time_range ===
    cJSON* time_json = cJSON_CreateObject();

    // Всегда сохраняем from/to/days
    char from_str[9], to_str[9];
    snprintf(from_str, sizeof(from_str), "%02d:%02d", rule->time_range.from_hour, rule->time_range.from_min);
    snprintf(to_str, sizeof(to_str), "%02d:%02d", rule->time_range.to_hour, rule->time_range.to_min);
    cJSON_AddStringToObject(time_json, "from", from_str);
    cJSON_AddStringToObject(time_json, "to", to_str);

    cJSON* days_arr = cJSON_CreateArray();
    for (int i = 0; i < 7; i++) {
        if (rule->time_range.days_of_week & (1 << i)) {
            cJSON_AddItemToArray(days_arr, cJSON_CreateNumber(i));
        }
    }
    cJSON_AddItemToObject(time_json, "days", days_arr);

    // Сохраняем enabled — управляет, применяется ли это время
    cJSON_AddBoolToObject(time_json, "enabled", rule->time_range.enabled);

    cJSON_AddItemToObject(json, "time_range", time_json);
    return json;
}

// Вспомогательная: загружает одно правило из JSON-файла
static bool load_rule_from_file(const char* path, zb_rule_t* out_rule) {
    cJSON* json = read_json_from_file(path);
    if (!json) return false;

    bool success = rule_from_json(json, out_rule);
    cJSON_Delete(json);
    return success;
}

// Основная: загружает все правила из индекса
void zb_rules_load_all_from_storage(void) {
    ESP_LOGI(TAG, "Loading rules from SPIFFS...");

    // Читаем индекс
    cJSON* index = read_json_from_file(ZBM_RULES_INDEX_FILE);
    if (!index) {
        ESP_LOGW(TAG, "No rules index found. Starting with empty rules.");
        return;
    }

    int loaded = 0;
    for (int i = 0; i < cJSON_GetArraySize(index); i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        cJSON* item_id = cJSON_GetObjectItem(item, "id");
        cJSON* item_path = cJSON_GetObjectItem(item, "path");
        cJSON* item_enabled = cJSON_GetObjectItem(item, "enabled");

        if (!item_id || !item_path) {
            ESP_LOGW(TAG, "Invalid rule entry in index at %d", i);
            continue;
        }

        // Читаем полное правило
        zb_rule_t temp_rule;
        if (!load_rule_from_file(item_path->valuestring, &temp_rule)) {
            ESP_LOGE(TAG, "Failed to load rule: %s", item_id->valuestring);
            continue;
        }

        // Устанавливаем enabled из индекса
        temp_rule.enabled = item_enabled ? cJSON_IsTrue(item_enabled) : true;

        // Передаём в движок
        if (zb_automation_v2_add_rule(&temp_rule)) {
            ESP_LOGI(TAG, "✅ Loaded rule: %s (ID: %s)", temp_rule.name, temp_rule.id);
            loaded++;
        } else {
            ESP_LOGE(TAG, "Failed to add rule to engine: %s", temp_rule.id);
        }
    }

    cJSON_Delete(index);
    ESP_LOGI(TAG, "Loaded %d rules from storage", loaded);
}

// ========================================================
//                УПРАВЛЕНИЕ ПРАВИЛАМИ
// ========================================================

bool zb_automation_v2_add_rule(const zb_rule_t* rule) {
    if (!rule || zb_rules_count >= ZB_AUTO_MAX_RULES) return false;
    zb_rule_t* new_rule = calloc(1, sizeof(zb_rule_t));
    if (!new_rule) return false;
    memcpy(new_rule, rule, sizeof(zb_rule_t));

    // Копируем значения ожидаемых данных
    for (int i = 0; i < new_rule->trigger_count; i++) {
        zb_trigger_t* t = &new_rule->triggers[i];
        if (t->expected_size > 0 && t->p_expected_value) {
            void* copy = malloc(t->expected_size);
            if (copy) {
                memcpy(copy, t->p_expected_value, t->expected_size);
                t->p_expected_value = copy;
            }
        }
    }

    zb_rules[zb_rules_count++] = new_rule;
    return true;
}

bool zb_automation_v2_remove_rule(const char* id) {
    for (int i = 0; i < zb_rules_count; i++) {
        if (strcmp(zb_rules[i]->id, id) == 0) {
            // Освобождение значений
            for (int j = 0; j < zb_rules[i]->trigger_count; j++) {
                if (zb_rules[i]->triggers[j].p_expected_value) {
                    free(zb_rules[i]->triggers[j].p_expected_value);
                }
            }
            free(zb_rules[i]);
            memmove(&zb_rules[i], &zb_rules[i+1], (--zb_rules_count - i) * sizeof(zb_rule_t*));
            return true;
        }
    }
    return false;
}

bool zb_automation_v2_run_rule_now(const char* id) {
    for (int i = 0; i < zb_rules_count; i++) {
        if (strcmp(zb_rules[i]->id, id) == 0 && zb_rules[i]->enabled) {
            execute_rule(zb_rules[i], "manual");
            return true;
        }
    }
    return false;
}

// ========================================================
//                СОХРАНЕНИЕ ПРАВИЛ
// ========================================================

bool zb_automation_v2_update_rule_from_json(cJSON* json) {
    if (!json) return false;

    zb_rule_t temp_rule;
    if (!rule_from_json(json, &temp_rule)) {
        ESP_LOGE(TAG, "Failed to parse rule from JSON");
        return false;
    }

    // Проверяем, существует ли правило с таким ID
    for (int i = 0; i < zb_rules_count; i++) {
        if (strcmp(zb_rules[i]->id, temp_rule.id) == 0) {
            // Удаляем старое
            zb_automation_v2_remove_rule(temp_rule.id);
            break;
        }
    }

    // Добавляем новое
    bool added = zb_automation_v2_add_rule(&temp_rule);
    if (!added) {
        ESP_LOGE(TAG, "Failed to add rule to engine: %s", temp_rule.id);
    } else {
        ESP_LOGI(TAG, "✅ Rule updated in memory: %s", temp_rule.id);
    }

    return added;
}

bool zb_automation_v2_save_rule_to_storage(const char* id) {
    // Найти правило в памяти
    zb_rule_t* rule = NULL;
    for (int i = 0; i < zb_rules_count; i++) {
        if (strcmp(zb_rules[i]->id, id) == 0) {
            rule = zb_rules[i];
            break;
        }
    }
    if (!rule) return false;

    // Сериализовать
    cJSON* json = rule_to_json(rule);
    if (!json) return false;

    // Генерируем путь
    char file_name[32];
    snprintf(file_name, sizeof(file_name), "rule_%s.json", id);

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", SPIFFS_ZBM_CONF_MOUNT_POINT, file_name);

    // Записываем правило
    bool saved = write_json_to_file(path, json);
    cJSON_Delete(json);
    if (!saved) return false;

    // Обновляем индекс
    char index_path[64];
    snprintf(index_path, sizeof(index_path), "%s", ZBM_RULES_INDEX_FILE);

    cJSON* index = read_json_from_file(index_path);
    if (!index) index = cJSON_CreateArray();

    // Удаляем старую запись
    for (int i = 0; i < cJSON_GetArraySize(index); i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        cJSON* item_id = cJSON_GetObjectItem(item, "id");
        if (item_id && strcmp(item_id->valuestring, id) == 0) {
            cJSON_DeleteItemFromArray(index, i);
            break;
        }
    }

    // Добавляем новую
    cJSON* brief = cJSON_CreateObject();
    cJSON_AddStringToObject(brief, "id", rule->id);
    cJSON_AddStringToObject(brief, "name", rule->name);
    cJSON_AddBoolToObject(brief, "enabled", rule->enabled);
    cJSON_AddStringToObject(brief, "path", path);
    cJSON_AddStringToObject(brief, "updated_at", "now");
    cJSON_AddItemToArray(index, brief);

    bool index_saved = write_json_to_file(index_path, index);
    cJSON_Delete(index);

    ESP_LOGI(TAG, "Rule '%s' saved to storage: %s", id, index_saved ? "OK" : "FAIL");
    return index_saved;
}