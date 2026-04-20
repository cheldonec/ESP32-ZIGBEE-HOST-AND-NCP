// main/zbm_automation/zbm_automation_v2.c
#include "zbm_automation_v2.h"
#include "zbm_behavior.h"
#include "zbm_core_sync.h"
#include "zbm_ncp_connect.h"
#include "ncp_host_zb_api_to_ncp.h"
#include "esp_log.h"
#include "esp_err.h"
#include "string.h"
#include "cJSON.h"
#include "zbm_spiffs_helper.h"

static const char* TAG = "ZB_AUTO_V2";

zb_rule_t* zb_rules[ZB_AUTO_MAX_RULES] = {0};
uint8_t zb_rules_count = 0;

zbm_virtual_var_t zbm_vars[ZB_AUTO_VAR_COUNT] = {0};

// === Парсер JSON -> zb_rule_t (V2) ===
bool rule_from_json(cJSON* json, zb_rule_t* out_rule) {
    if (!json || !out_rule) return false;

    memset(out_rule, 0, sizeof(zb_rule_t));

    // Обязательные поля
    cJSON* id = cJSON_GetObjectItem(json, "id");
    cJSON* name = cJSON_GetObjectItem(json, "name");
    if (!id || !cJSON_IsString(id) || !name || !cJSON_IsString(name)) {
        ESP_LOGE(TAG, "Missing or invalid 'id' or 'name'");
        return false;
    }

    strncpy(out_rule->id, id->valuestring, sizeof(out_rule->id) - 1);
    strncpy(out_rule->name, name->valuestring, sizeof(out_rule->name) - 1);

    cJSON* enabled = cJSON_GetObjectItem(json, "enabled");
    out_rule->enabled = !cJSON_IsFalse(enabled);  // defaults to true

    cJSON* prio = cJSON_GetObjectItem(json, "priority");
    out_rule->priority = prio ? (int8_t)prio->valueint : 0;  // по умолчанию 0

    cJSON* exec_mode_obj = cJSON_GetObjectItem(json, "exec_mode");
    out_rule->exec_mode = exec_mode_obj ? (zb_rule_execution_mode_t)exec_mode_obj->valueint : ZB_RULE_EXEC_FIRST;

    cJSON* logic_obj = cJSON_GetObjectItem(json, "allowing_logic");
    out_rule->allowing_logic_op = logic_obj ? (zb_logic_op_t)logic_obj->valueint : ZB_LOGIC_OR;

    // === Парсинг побуждающего триггера (один) ===
    cJSON* cause_obj = cJSON_GetObjectItem(json, "cause_trigger");
    if (!cause_obj || !cJSON_IsObject(cause_obj)) {
        ESP_LOGE(TAG, "Missing 'cause_trigger'");
        return false;
    }

    cJSON* guid = cJSON_GetObjectItem(cause_obj, "guid");
    cJSON* cond = cJSON_GetObjectItem(cause_obj, "cond");
    cJSON* expected_type = cJSON_GetObjectItem(cause_obj, "expected_type");
    cJSON* value = cJSON_GetObjectItem(cause_obj, "value");

    if (!guid || !cJSON_IsString(guid) || !cond || !cJSON_IsNumber(cond) || !expected_type || !cJSON_IsNumber(expected_type)) {
        ESP_LOGE(TAG, "Invalid cause_trigger");
        return false;
    }

    strncpy(out_rule->cause_trigger.guid, guid->valuestring, sizeof(out_rule->cause_trigger.guid) - 1);
    out_rule->cause_trigger.cond = (zb_condition_t)cond->valueint;
    out_rule->cause_trigger.expected_type = (zbm_attr_data_types_t)expected_type->valueint;

    // Вычисляем размер
    size_t val_size = 0;
    switch (out_rule->cause_trigger.expected_type) {
        case ZBM_ATTR_TYPE_BOOL:
        case ZBM_ATTR_TYPE_U8:
        case ZBM_ATTR_TYPE_S8:
            val_size = 1;
            break;
        case ZBM_ATTR_TYPE_U16:
        case ZBM_ATTR_TYPE_S16:
            val_size = 2;
            break;
        case ZBM_ATTR_TYPE_CHAR_STRING:
        case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
            val_size = strlen(value->valuestring) + 1;
            break;
        default:
            val_size = 1;
            break;
    }
    out_rule->cause_trigger.expected_size = val_size;

    out_rule->cause_trigger.p_expected_value = malloc(val_size);
    if (!out_rule->cause_trigger.p_expected_value) return false;

    // Копируем значение
    switch (out_rule->cause_trigger.expected_type) {
        case ZBM_ATTR_TYPE_BOOL:
        case ZBM_ATTR_TYPE_U8:
        case ZBM_ATTR_TYPE_S8:
            *(uint8_t*)out_rule->cause_trigger.p_expected_value = (uint8_t)value->valueint;
            break;
        case ZBM_ATTR_TYPE_U16:
        case ZBM_ATTR_TYPE_S16:
            *(uint16_t*)out_rule->cause_trigger.p_expected_value = (uint16_t)value->valueint;
            break;
        case ZBM_ATTR_TYPE_CHAR_STRING:
        case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
            strcpy((char*)out_rule->cause_trigger.p_expected_value, value->valuestring);
            break;
        default:
            *(uint8_t*)out_rule->cause_trigger.p_expected_value = (uint8_t)value->valueint;
            break;
    }

    // === Парсинг разрешающих триггеров (allowing) ===
    cJSON* allowing = cJSON_GetObjectItem(json, "allowing_triggers");
    if (cJSON_IsArray(allowing)) {
        int count = cJSON_GetArraySize(allowing);
        out_rule->trigger_count = (count > ZB_AUTO_MAX_TRIGGERS) ? ZB_AUTO_MAX_TRIGGERS : count;

        for (int i = 0; i < out_rule->trigger_count; i++) {
            cJSON* t = cJSON_GetArrayItem(allowing, i);
            zb_trigger_t* trig = &out_rule->triggers[i];

            guid = cJSON_GetObjectItem(t, "guid");
            cond = cJSON_GetObjectItem(t, "cond");
            expected_type = cJSON_GetObjectItem(t, "expected_type");
            value = cJSON_GetObjectItem(t, "value");

            if (!guid || !cJSON_IsString(guid) || !cond || !cJSON_IsNumber(cond) || !expected_type || !cJSON_IsNumber(expected_type)) {
                ESP_LOGW(TAG, "Invalid allowing trigger at %d", i);
                continue;
            }

            strncpy(trig->guid, guid->valuestring, sizeof(trig->guid) - 1);
            trig->cond = (zb_condition_t)cond->valueint;
            trig->expected_type = (zbm_attr_data_types_t)expected_type->valueint;

            val_size = 0;
            switch (trig->expected_type) {
                case ZBM_ATTR_TYPE_BOOL:
                case ZBM_ATTR_TYPE_U8:
                case ZBM_ATTR_TYPE_S8: val_size = 1; break;
                case ZBM_ATTR_TYPE_U16:
                case ZBM_ATTR_TYPE_S16: val_size = 2; break;
                case ZBM_ATTR_TYPE_CHAR_STRING:
                case ZBM_ATTR_TYPE_LONG_CHAR_STRING: val_size = strlen(value->valuestring) + 1; break;
                default: val_size = 1;
            }

            trig->expected_size = val_size;
            trig->p_expected_value = malloc(val_size);
            if (!trig->p_expected_value) return false;

            switch (trig->expected_type) {
                case ZBM_ATTR_TYPE_BOOL:
                case ZBM_ATTR_TYPE_U8:
                case ZBM_ATTR_TYPE_S8:
                    *(uint8_t*)trig->p_expected_value = (uint8_t)value->valueint;
                    break;
                case ZBM_ATTR_TYPE_U16:
                case ZBM_ATTR_TYPE_S16:
                    *(uint16_t*)trig->p_expected_value = (uint16_t)value->valueint;
                    break;
                case ZBM_ATTR_TYPE_CHAR_STRING:
                case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
                    strcpy((char*)trig->p_expected_value, value->valuestring);
                    break;
                default:
                    *(uint8_t*)trig->p_expected_value = (uint8_t)value->valueint;
                    break;
            }
        }
    }

    // === Парсинг действий ===
    cJSON* actions = cJSON_GetObjectItem(json, "actions");
    if (cJSON_IsArray(actions)) {
        int count = cJSON_GetArraySize(actions);
        out_rule->action_count = (count > ZB_AUTO_MAX_ACTIONS) ? ZB_AUTO_MAX_ACTIONS : count;

        for (int i = 0; i < out_rule->action_count; i++) {
            cJSON* a = cJSON_GetArrayItem(actions, i);
            zb_action_t* act = &out_rule->actions[i];

            cJSON* type = cJSON_GetObjectItem(a, "type");
            if (!type || !cJSON_IsNumber(type)) continue;

            act->type = (zb_action_type_t)type->valueint;

            switch (act->type) {
                case ZB_ACTION_SEND_CMD_BY_GUID: {
                    cJSON* guid = cJSON_GetObjectItem(a, "cmd_guid");
                    if (guid && cJSON_IsString(guid)) {
                        strncpy(act->data.send_cmd.cmd_guid, guid->valuestring, sizeof(act->data.send_cmd.cmd_guid) - 1);
                    }
                    cJSON* params = cJSON_GetObjectItem(a, "params");
                    if (params) {
                        act->data.send_cmd.params = cJSON_Duplicate(params, true);
                    } else {
                        act->data.send_cmd.params = cJSON_CreateObject();
                    }
                    break;
                }
                case ZB_ACTION_SET_VAR_UINT8:
                    act->data.set_uint8.var_idx = (uint8_t)cJSON_GetObjectItem(a, "var_idx")->valueint;
                    act->data.set_uint8.value = (uint8_t)cJSON_GetObjectItem(a, "value")->valueint;
                    break;
                case ZB_ACTION_SET_VAR_INT8:
                    act->data.set_int8.var_idx = (uint8_t)cJSON_GetObjectItem(a, "var_idx")->valueint;
                    act->data.set_int8.value = (int8_t)cJSON_GetObjectItem(a, "value")->valueint;
                    break;
                case ZB_ACTION_SET_VAR_UINT16:
                    act->data.set_uint16.var_idx = (uint8_t)cJSON_GetObjectItem(a, "var_idx")->valueint;
                    act->data.set_uint16.value = (uint16_t)cJSON_GetObjectItem(a, "value")->valueint;
                    break;
                case ZB_ACTION_SET_VAR_STRING: {
                    act->data.set_str.var_idx = (uint8_t)cJSON_GetObjectItem(a, "var_idx")->valueint;
                    cJSON* str_val = cJSON_GetObjectItem(a, "value");
                    if (str_val && cJSON_IsString(str_val)) {
                        strncpy(act->data.set_str.str, str_val->valuestring, sizeof(act->data.set_str.str) - 1);
                    }
                    break;
                }
                case ZB_ACTION_INC_VAR:
                case ZB_ACTION_DEC_VAR:
                case ZB_ACTION_TOGGLE_VAR:
                    act->data.inc.var_idx = (uint8_t)cJSON_GetObjectItem(a, "var_idx")->valueint;
                    break;
                default:
                    ESP_LOGW(TAG, "Unknown action type: %d", act->type);
                    break;
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
    cJSON_AddNumberToObject(json, "allowing_logic", rule->allowing_logic_op);

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
        case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
            cJSON_AddStringToObject(cause_json, "value", (char*)rule->cause_trigger.p_expected_value);
            break;
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
            case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
                cJSON_AddStringToObject(t_json, "value", (char*)t->p_expected_value);
                break;
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
//                ВИРТУАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ========================================================

void zbm_var_init(void) {
    for (int i = 0; i < ZB_AUTO_VAR_COUNT; i++) {
        snprintf(zbm_vars[i].name, sizeof(zbm_vars[i].name), "var_%d", i);
        snprintf(zbm_vars[i].guid, sizeof(zbm_vars[i].guid), "var_%d", i);  // Инициализация GUID
        zbm_vars[i].data_type = ZBM_ATTR_TYPE_U8;
        zbm_vars[i].data_size = 1;
        zbm_vars[i].p_value = calloc(1, 1);
        zbm_vars[i].last_update_ms = 0;
        zbm_vars[i].idx = i;
    }

    // Загружаем из SPIFFS
    cJSON* json = read_json_from_file(ZBM_RULES_VARS_FILE);
    if (json && cJSON_IsArray(json)) {
        ESP_LOGI(TAG, "Loading variables from %s", ZBM_RULES_VARS_FILE);
        for (int i = 0; i < cJSON_GetArraySize(json); i++) {
            cJSON* item = cJSON_GetArrayItem(json, i);
            int idx = cJSON_GetObjectItem(item, "idx")->valueint;
            if (idx >= 0 && idx < ZB_AUTO_VAR_COUNT) {
                // Восстанавливаем имя, если есть
                cJSON* name_obj = cJSON_GetObjectItem(item, "name");
                if (name_obj && cJSON_IsString(name_obj)) {
                    strncpy(zbm_vars[idx].name, name_obj->valuestring, sizeof(zbm_vars[idx].name) - 1);
                }
                cJSON* type_obj = cJSON_GetObjectItem(item, "type");
                if (type_obj) zbm_vars[idx].data_type = (zbm_attr_data_types_t)type_obj->valueint;

                cJSON* value = cJSON_GetObjectItem(item, "value");
                if (cJSON_IsNumber(value)) {
                    uint16_t size = 1;
                    if (zbm_vars[idx].data_type == ZBM_ATTR_TYPE_U16 ||
                        zbm_vars[idx].data_type == ZBM_ATTR_TYPE_S16)
                        size = 2;
                    zbm_var_update_value(&zbm_vars[idx], &value->valueint, size);
                } else if (cJSON_IsString(value)) {
                    zbm_var_update_value(&zbm_vars[idx], (void*)value->valuestring, strlen(value->valuestring) + 1);
                }
            }
        }
    }
    cJSON_Delete(json);
}

void zbm_vars_save_to_storage(void) {
    cJSON* arr = cJSON_CreateArray();
    for (int i = 0; i < ZB_AUTO_VAR_COUNT; i++) {
        cJSON* var = cJSON_CreateObject();
        cJSON_AddNumberToObject(var, "idx", i);
        cJSON_AddStringToObject(var, "name", zbm_vars[i].name);
        cJSON_AddNumberToObject(var, "type", zbm_vars[i].data_type);

        switch (zbm_vars[i].data_type) {
            case ZBM_ATTR_TYPE_U8:
            case ZBM_ATTR_TYPE_BOOL:
                cJSON_AddNumberToObject(var, "value", *(uint8_t*)zbm_vars[i].p_value);
                break;
            case ZBM_ATTR_TYPE_S8:
                cJSON_AddNumberToObject(var, "value", *(int8_t*)zbm_vars[i].p_value);
                break;
            case ZBM_ATTR_TYPE_U16:
            case ZBM_ATTR_TYPE_S16:
                cJSON_AddNumberToObject(var, "value", *(uint16_t*)zbm_vars[i].p_value);
                break;
            case ZBM_ATTR_TYPE_CHAR_STRING:
            case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
                cJSON_AddStringToObject(var, "value", (char*)zbm_vars[i].p_value);
                break;
            default:
                cJSON_AddNumberToObject(var, "value", *(uint8_t*)zbm_vars[i].p_value);
                break;
        }
        cJSON_AddItemToArray(arr, var);
    }

    write_json_to_file(ZBM_RULES_VARS_FILE, arr);
    cJSON_Delete(arr);
    ESP_LOGI(TAG, "Variables saved to %s", ZBM_RULES_VARS_FILE);
}

void zbm_var_update_value(zbm_virtual_var_t* var, void* value, uint16_t size) {
    if (var->p_value && var->data_size != size) {
        free(var->p_value);
        var->p_value = NULL;
    }
    if (!var->p_value) {
        var->p_value = calloc(1, size);
        if (!var->p_value) return;
        var->data_size = size;
    }
    memcpy(var->p_value, value, size);
    var->last_update_ms = esp_log_timestamp();

    // Отправляем в автоматизацию
    char var_guid[32];
    snprintf(var_guid, sizeof(var_guid), "var_%d", var->idx);
    zb_automation_v2_on_data_change(ZBM_DATA_SRC_VAR, var_guid, var->data_type, value, size);
}

void zbm_var_set_uint8(uint8_t idx, uint8_t value) {
    if (idx >= ZB_AUTO_VAR_COUNT) return;
    zbm_var_update_value(&zbm_vars[idx], &value, 1);
}

void zbm_var_set_int8(uint8_t idx, int8_t value) {
    if (idx >= ZB_AUTO_VAR_COUNT) return;
    zbm_var_update_value(&zbm_vars[idx], &value, 1);
}

void zbm_var_set_uint16(uint8_t idx, uint16_t value) {
    if (idx >= ZB_AUTO_VAR_COUNT) return;
    zbm_var_update_value(&zbm_vars[idx], &value, 2);
}

void zbm_var_set_string(uint8_t idx, const char* str) {
    if (idx >= ZB_AUTO_VAR_COUNT || !str) return;
    uint16_t len = strlen(str) + 1;
    zbm_var_update_value(&zbm_vars[idx], (void*)str, len);
}

uint8_t zbm_var_get_uint8(uint8_t idx) {
    if (idx >= ZB_AUTO_VAR_COUNT || !zbm_vars[idx].p_value) return 0;
    return *(uint8_t*)zbm_vars[idx].p_value;
}

bool zbm_var_compare(uint8_t idx, void* value, zbm_attr_data_types_t type) {
    if (idx >= ZB_AUTO_VAR_COUNT || !zbm_vars[idx].p_value || !value) return false;
    void* current = zbm_vars[idx].p_value;
    switch (type) {
        case ZBM_ATTR_TYPE_U8:
            return *(uint8_t*)current == *(uint8_t*)value;
        case ZBM_ATTR_TYPE_S8:
            return *(int8_t*)current == *(int8_t*)value;
        case ZBM_ATTR_TYPE_U16:
            return *(uint16_t*)current == *(uint16_t*)value;
        case ZBM_ATTR_TYPE_CHAR_STRING:
            return strcmp((char*)current, (char*)value) == 0;
        default:
            ESP_LOGW(TAG, "Unsupported compare type: 0x%02X", type);
            return false;
    }
}

// ========================================================
//                СРАВНЕНИЕ ЗНАЧЕНИЙ
// ========================================================

bool value_matches(void* val1, void* val2, zbm_attr_data_types_t type, zb_condition_t cond) {
    switch (type) {
        case ZBM_ATTR_TYPE_U8:
        case ZBM_ATTR_TYPE_BOOL:
        case ZBM_ATTR_TYPE_T8BIT:
        case ZBM_ATTR_TYPE_T8BIT_ENUM:
        case ZBM_ATTR_TYPE_T8BITMAP: {
            uint8_t a = *(uint8_t*)val1;
            uint8_t b = *(uint8_t*)val2;
            switch (cond) {
                case ZB_COND_EQ: return a == b;
                case ZB_COND_NE: return a != b;
                case ZB_COND_GT: return a > b;
                case ZB_COND_LT: return a < b;
                case ZB_COND_GTE: return a >= b;
                case ZB_COND_LTE: return a <= b;
            }
            break;
        }
        case ZBM_ATTR_TYPE_S8: {
            int8_t a = *(int8_t*)val1;
            int8_t b = *(int8_t*)val2;
            switch (cond) {
                case ZB_COND_EQ: return a == b;
                case ZB_COND_NE: return a != b;
                case ZB_COND_GT: return a > b;
                case ZB_COND_LT: return a < b;
                case ZB_COND_GTE: return a >= b;
                case ZB_COND_LTE: return a <= b;
            }
            break;
        }
        case ZBM_ATTR_TYPE_U16:
        case ZBM_ATTR_TYPE_CLUSTER_ID:
        case ZBM_ATTR_TYPE_ATTRIBUTE_ID:
        case ZBM_ATTR_TYPE_T16BIT:
        case ZBM_ATTR_TYPE_T16BIT_ENUM:
        case ZBM_ATTR_TYPE_T16BITMAP: {
            uint16_t a = *(uint16_t*)val1;
            uint16_t b = *(uint16_t*)val2;
            switch (cond) {
                case ZB_COND_EQ: return a == b;
                case ZB_COND_NE: return a != b;
                case ZB_COND_GT: return a > b;
                case ZB_COND_LT: return a < b;
                case ZB_COND_GTE: return a >= b;
                case ZB_COND_LTE: return a <= b;
            }
            break;
        }
        case ZBM_ATTR_TYPE_CHAR_STRING:
        case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
        case ZBM_ATTR_TYPE_OCTET_STRING:
        case ZBM_ATTR_TYPE_LONG_OCTET_STRING: {
            char* a = (char*)val1;
            char* b = (char*)val2;
            int cmp = strcmp(a, b);
            switch (cond) {
                case ZB_COND_EQ: return cmp == 0;
                case ZB_COND_NE: return cmp != 0;
                case ZB_COND_GT: return cmp > 0;
                case ZB_COND_LT: return cmp < 0;
                default: return false;
            }
            break;
        }
        default:
            ESP_LOGW(TAG, "Unsupported type in condition: 0x%02X", type);
            return false;
    }
    return false;
}

// ========================================================
//                ВЫПОЛНЕНИЕ ДЕЙСТВИЙ
// ========================================================

void zb_automation_v2_execute_action(const zb_action_t* act) {
    switch (act->type) {
        case ZB_ACTION_SEND_CMD_BY_GUID: {
            cJSON* req = cJSON_CreateObject();
            cJSON_AddStringToObject(req, "guid", act->data.send_cmd.cmd_guid);
            cJSON_AddItemReferenceToObject(req, "params", act->data.send_cmd.params);
            uint8_t tsn = zbm_to_ncp_req_send_zcl_cmd_from_ws_json(req);
            cJSON_Delete(req);
            if (tsn != 0xFF) {
                ESP_LOGI(TAG, "✅ Sent ZCL cmd via GUID: %s (TSN=%d)", act->data.send_cmd.cmd_guid, tsn);
            }
            break;
        }
        case ZB_ACTION_SET_VAR_UINT8:
            zbm_var_set_uint8(act->data.set_uint8.var_idx, act->data.set_uint8.value);
            break;
        case ZB_ACTION_SET_VAR_INT8:
            zbm_var_set_int8(act->data.set_int8.var_idx, act->data.set_int8.value);
            break;
        case ZB_ACTION_SET_VAR_UINT16:
            zbm_var_set_uint16(act->data.set_uint16.var_idx, act->data.set_uint16.value);
            break;
        case ZB_ACTION_SET_VAR_STRING:
            zbm_var_set_string(act->data.set_str.var_idx, act->data.set_str.str);
            break;
        case ZB_ACTION_INC_VAR: {
                        uint8_t v = zbm_var_get_uint8(act->data.inc.var_idx);
            zbm_var_set_uint8(act->data.inc.var_idx, v + 1);
            break;
        }
        case ZB_ACTION_DEC_VAR: {
            uint8_t v = zbm_var_get_uint8(act->data.dec.var_idx);
            zbm_var_set_uint8(act->data.dec.var_idx, v > 0 ? v - 1 : 0);
            break;
        }
        case ZB_ACTION_TOGGLE_VAR: {
            uint8_t v = zbm_var_get_uint8(act->data.toggle.var_idx);
            zbm_var_set_uint8(act->data.toggle.var_idx, !v);
            break;
        }
        default:
            ESP_LOGW(TAG, "Unknown action type: %d", act->type);
    }
}

static void execute_rule(const zb_rule_t* rule, const char* trigger_guid) {
    ESP_LOGI(TAG, "🔥 Rule fired: %s (triggered by: %s)", rule->name, trigger_guid);
    ws_notify_automation_rule_fired(rule->id, trigger_guid);
    for (int i = 0; i < rule->action_count; i++) {
        zb_automation_v2_execute_action(&rule->actions[i]);
    }
}

// ========================================================
//                ОБРАБОТКА ОБНОВЛЕНИЙ
// ========================================================

void zb_automation_v2_on_data_change(
    zbm_data_source_t src_type,
    const char* guid,
    zbm_attr_data_types_t data_type,
    const void* value,
    uint16_t size)
{
    // 🔹 Основная точка входа для всех изменений данных:
    // - от Zigbee устройств (атрибуты, репорты),
    // - от виртуальных переменных,
    // - от других источников.
    ESP_LOGI(TAG, "Data change: %s, type: %d, size: %u", guid, data_type, size);
    if (value) {
        // Выводим значение, если это число (упрощённо)
        if (data_type == ZBM_ATTR_TYPE_U8 || data_type == ZBM_ATTR_TYPE_BOOL) {
            ESP_LOGI(TAG, "Value (U8): %u", *(const uint8_t*)value);
        } else if (data_type == ZBM_ATTR_TYPE_S8) {
            ESP_LOGI(TAG, "Value (S8): %d", *(const int8_t*)value);
        } else if (data_type == ZBM_ATTR_TYPE_U16) {
            ESP_LOGI(TAG, "Value (U16): %u", *(const uint16_t*)value);
        } else if (data_type == ZBM_ATTR_TYPE_CHAR_STRING) {
            ESP_LOGI(TAG, "Value (STR): %s", (const char*)value);
        }
    }

    // === 🔸 Проверка: есть ли поведение (behavior)? ===
    // ⚠️ Если behavior_enabled — автоматизация НЕ БУДЕТ СРАБАТЫВАТЬ!
    zbm_cluster_attribute_t* attr = zbm_find_attr_by_guid_safe(guid);
    if (attr && attr->behavior_enabled && strlen(attr->behavior_id) > 0) {
        if (zbm_behavior_execute(attr->behavior_id)) {
            ESP_LOGD(TAG, "✅ Behavior '%s' handled event for %s → automation skipped", attr->behavior_id, guid);
            return; // ❌ Ранний выход! Правила не будут проверяться
        }
    }

    zbm_cluster_custom_report_cmd_t* rep = zbm_find_custom_report_by_guid_safe(guid);
    if (rep && rep->behavior_enabled && strlen(rep->behavior_id) > 0) {
        if (zbm_behavior_execute(rep->behavior_id)) {
            ESP_LOGD(TAG, "✅ Behavior '%s' handled report %s → automation skipped", rep->behavior_id, guid);
            return; // ❌ Ещё один ранний выход
        }
    }

    // === 🔸 Проверка: это виртуальная переменная? ===
    bool is_var = false;
    uint8_t var_idx = 0;
    if (strncmp(guid, "var_", 4) == 0) {
        var_idx = atoi(guid + 4);
        if (var_idx < ZB_AUTO_VAR_COUNT) {
            is_var = true;
            ESP_LOGD(TAG, "Event from virtual variable var_%d", var_idx);
        }
    }

    // === 🔸 Поиск подходящих правил ===
    zb_rule_t* candidates[ZB_AUTO_MAX_RULES];
    int count = 0;

    ESP_LOGI(TAG, "🔍 Checking %d rules for trigger match...", zb_rules_count);

    for (int i = 0; i < zb_rules_count; i++) {
        zb_rule_t* rule = zb_rules[i];
        if (!rule) continue;
        if (!rule->enabled) {
            ESP_LOGD(TAG, "Rule '%s' is disabled → skip", rule->name);
            continue;
        }

        // 🟡 Проверка 1: совпадает ли cause_trigger.guid?
        if (strcmp(rule->cause_trigger.guid, guid) != 0) {
            // Раскомментируй, если нужно видеть все несовпадения
            // ESP_LOGD(TAG, "GUID mismatch: rule='%s' vs event='%s'", rule->cause_trigger.guid, guid);
            continue;
        }
        ESP_LOGI(TAG, "✅ GUID match for rule '%s'", rule->name);

        // 🟡 Проверка 2: совпадает ли тип данных?
        if (rule->cause_trigger.expected_type != data_type) {
            ESP_LOGW(TAG, "❌ Type mismatch: expected=%d, got=%d", rule->cause_trigger.expected_type, data_type);
            continue;
        }

        // 🟡 Проверка 3: совпадает ли размер?
        if (rule->cause_trigger.expected_size != size) {
            ESP_LOGW(TAG, "❌ Size mismatch: expected=%u, got=%u", rule->cause_trigger.expected_size, size);
            continue;
        }

        // 🟡 Проверка 4: проходит ли условие (eq, gt, etc)?
        bool cause_match = value_matches((void*)value, rule->cause_trigger.p_expected_value, data_type, rule->cause_trigger.cond);
        if (!cause_match) {
            ESP_LOGW(TAG, "❌ Cause condition failed: value does not match expected");
            continue;
        }
        ESP_LOGI(TAG, "✅ Cause condition passed for rule '%s'", rule->name);

        // === 🔸 Проверка разрешающих триггеров (allowing_triggers) ===
        uint8_t allowing_count = rule->trigger_count;
        uint8_t matched_count = 0;

        for (int j = 0; j < allowing_count; j++) {
            zb_trigger_t* t = &rule->triggers[j];
            bool match = false;

            if (strncmp(t->guid, "var_", 4) == 0) {
                // Переменная
                int idx = atoi(t->guid + 4);
                if (idx >= 0 && idx < ZB_AUTO_VAR_COUNT && zbm_vars[idx].p_value) {
                    if (value_matches(zbm_vars[idx].p_value, t->p_expected_value, t->expected_type, t->cond)) {
                        match = true;
                    }
                }
            } else {
                // Атрибут
                zbm_cluster_attribute_t* a = zbm_find_attr_by_guid_safe(t->guid);
                if (a && a->p_value && a->data_type == t->expected_type) {
                    if (value_matches(a->p_value, t->p_expected_value, t->expected_type, t->cond)) {
                        match = true;
                    }
                }
                if (!match) {
                    // Репорт
                    zbm_cluster_custom_report_cmd_t* r = zbm_find_custom_report_by_guid_safe(t->guid);
                    if (r && r->p_value && r->data_type == (zbm_cmd_data_types_t)t->expected_type) {
                        if (value_matches(r->p_value, t->p_expected_value, t->expected_type, t->cond)) {
                            match = true;
                        }
                    }
                }
            }

            if (match) {
                matched_count++;
                ESP_LOGD(TAG, "✅ Allowing trigger %d matched", j);
            } else {
                ESP_LOGD(TAG, "❌ Allowing trigger %d NOT matched", j);
            }
        }

        bool allowing_matched = false;
        if (allowing_count == 0) {
            allowing_matched = true;
            ESP_LOGD(TAG, "No allowing triggers → considered matched");
        } else if (rule->allowing_logic_op == ZB_LOGIC_OR) {
            allowing_matched = (matched_count > 0);
            ESP_LOGD(TAG, "OR logic: %d of %d matched → result=%s", matched_count, allowing_count, allowing_matched ? "true" : "false");
        } else {
            allowing_matched = (matched_count == allowing_count);
            ESP_LOGD(TAG, "AND logic: %d of %d matched → result=%s", matched_count, allowing_count, allowing_matched ? "true" : "false");
        }

        if (allowing_matched) {
            candidates[count++] = rule;
            ESP_LOGI(TAG, "✅ Rule '%s' added to execution candidates", rule->name);
        } else {
            ESP_LOGW(TAG, "❌ Rule '%s' failed allowing triggers", rule->name);
        }
    }

    if (count == 0) {
        ESP_LOGI(TAG, "❌ No rules triggered by this event");
        return;
    }

    // === 🔸 Сортировка по приоритету (от высшего к низшему) ===
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (candidates[i]->priority < candidates[j]->priority) {
                zb_rule_t* tmp = candidates[i];
                candidates[i] = candidates[j];
                candidates[j] = tmp;
            }
        }
    }

    // === 🔸 Выполнение правил ===
    if (candidates[0]->exec_mode == ZB_RULE_EXEC_FIRST) {
        execute_rule(candidates[0], guid);
    } else {
        for (int i = 0; i < count; i++) {
            execute_rule(candidates[i], guid);
        }
    }
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
//                ИНИЦИАЛИЗАЦИЯ
// ========================================================

void zb_automation_v2_init(void) {
    zbm_var_init();
    zb_rules_count = 0;

    // Загружаем правила
    zb_rules_load_all_from_storage();

    // Инициализируем поведения
    zbm_behavior_init();

    ESP_LOGI(TAG, "✅ Automation V2 initialized with %d max rules", ZB_AUTO_MAX_RULES);
}

// Внутренняя: парсинг действия из JSON
bool zb_automation_v2_rule_from_json_action(cJSON* json, zb_action_t* out_act) {
    if (!json || !out_act) return false;
    memset(out_act, 0, sizeof(zb_action_t));

    cJSON* type_obj = cJSON_GetObjectItem(json, "type");
    if (!type_obj || !cJSON_IsNumber(type_obj)) return false;
    out_act->type = (zb_action_type_t)type_obj->valueint;

    switch (out_act->type) {
        case ZB_ACTION_SEND_CMD_BY_GUID: {
            cJSON* guid = cJSON_GetObjectItem(json, "cmd_guid");
            if (guid && cJSON_IsString(guid)) {
                strncpy(out_act->data.send_cmd.cmd_guid, guid->valuestring, sizeof(out_act->data.send_cmd.cmd_guid) - 1);
            }
            cJSON* params = cJSON_GetObjectItem(json, "params");
            if (params) {
                out_act->data.send_cmd.params = cJSON_Duplicate(params, true);
            } else {
                out_act->data.send_cmd.params = cJSON_CreateObject();
            }
            break;
        }
        case ZB_ACTION_SET_VAR_UINT8:
            out_act->data.set_uint8.var_idx = cJSON_GetObjectItem(json, "var_idx")->valueint;
            out_act->data.set_uint8.value = cJSON_GetObjectItem(json, "value")->valueint;
            break;
        case ZB_ACTION_SET_VAR_INT8:
            out_act->data.set_int8.var_idx = cJSON_GetObjectItem(json, "var_idx")->valueint;
            out_act->data.set_int8.value = cJSON_GetObjectItem(json, "value")->valueint;
            break;
        case ZB_ACTION_SET_VAR_UINT16:
            out_act->data.set_uint16.var_idx = cJSON_GetObjectItem(json, "var_idx")->valueint;
            out_act->data.set_uint16.value = cJSON_GetObjectItem(json, "value")->valueint;
            break;
        case ZB_ACTION_SET_VAR_STRING:
            out_act->data.set_str.var_idx = cJSON_GetObjectItem(json, "var_idx")->valueint;
            cJSON* str_val = cJSON_GetObjectItem(json, "value");
            if (str_val && cJSON_IsString(str_val)) {
                strncpy(out_act->data.set_str.str, str_val->valuestring, sizeof(out_act->data.set_str.str) - 1);
            }
            break;
        case ZB_ACTION_INC_VAR:
        case ZB_ACTION_DEC_VAR:
        case ZB_ACTION_TOGGLE_VAR:
            out_act->data.inc.var_idx = cJSON_GetObjectItem(json, "var_idx")->valueint;
            break;
        default:
            return false;
    }
    return true;
}

// Сериализация действия в JSON
cJSON* zb_automation_v2_rule_to_json_action(const zb_action_t* act) {
    if (!act) return NULL;
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "type", act->type);

    switch (act->type) {
        case ZB_ACTION_SEND_CMD_BY_GUID:
            cJSON_AddStringToObject(obj, "cmd_guid", act->data.send_cmd.cmd_guid);
            if (act->data.send_cmd.params) {
                cJSON_AddItemToObject(obj, "params", cJSON_Duplicate(act->data.send_cmd.params, true));
            }
            break;
        case ZB_ACTION_SET_VAR_UINT8:
            cJSON_AddNumberToObject(obj, "var_idx", act->data.set_uint8.var_idx);
            cJSON_AddNumberToObject(obj, "value", act->data.set_uint8.value);
            break;
        case ZB_ACTION_SET_VAR_INT8:
            cJSON_AddNumberToObject(obj, "var_idx", act->data.set_int8.var_idx);
            cJSON_AddNumberToObject(obj, "value", act->data.set_int8.value);
            break;
        case ZB_ACTION_SET_VAR_UINT16:
            cJSON_AddNumberToObject(obj, "var_idx", act->data.set_uint16.var_idx);
            cJSON_AddNumberToObject(obj, "value", act->data.set_uint16.value);
            break;
        case ZB_ACTION_SET_VAR_STRING:
            cJSON_AddNumberToObject(obj, "var_idx", act->data.set_str.var_idx);
            cJSON_AddStringToObject(obj, "value", act->data.set_str.str);
            break;
        case ZB_ACTION_INC_VAR:
        case ZB_ACTION_DEC_VAR:
        case ZB_ACTION_TOGGLE_VAR:
            cJSON_AddNumberToObject(obj, "var_idx", act->data.inc.var_idx);
            break;
    }
    return obj;
}