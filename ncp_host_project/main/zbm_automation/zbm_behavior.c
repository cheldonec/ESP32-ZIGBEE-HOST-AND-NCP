// main/zbm_behavior.c
#include "zbm_behavior.h"
#include "zbm_automation_v2.h"
#include "zbm_spiffs_helper.h"
#include "esp_log.h"
#include "string.h"
#include "cJSON.h"
#include "zbm_core_sync.h"

static const char* TAG = "ZBM_BEHAVIOR";

static zbm_behavior_t s_behaviors[ZBM_BEHAVIOR_MAX_COUNT] = {0};
static uint8_t s_behavior_count = 0;

// Внутренняя: копирование действия
static void copy_action(zb_action_t* dst, const zb_action_t* src) {
    memcpy(dst, src, sizeof(zb_action_t));
    if (src->type == ZB_ACTION_SEND_CMD_BY_GUID && src->data.send_cmd.params) {
        dst->data.send_cmd.params = cJSON_Duplicate(src->data.send_cmd.params, true);
    }
}

// Внутренняя: освобождение ресурсов действия
static void free_action(zb_action_t* act) {
    if (act->type == ZB_ACTION_SEND_CMD_BY_GUID && act->data.send_cmd.params) {
        cJSON_Delete(act->data.send_cmd.params);
        act->data.send_cmd.params = NULL;
    }
}

// Внутренняя: очистка поведения
static void clear_behavior(zbm_behavior_t* bhv) {
    for (int i = 0; i < bhv->action_count; i++) {
        free_action(&bhv->actions[i]);
    }
    for (int i = 0; i < bhv->condition_count; i++) {
        if (bhv->conditions[i].p_expected_value) {
            free(bhv->conditions[i].p_expected_value);
            bhv->conditions[i].p_expected_value = NULL;
        }
    }
    memset(bhv, 0, sizeof(zbm_behavior_t));
    bhv->enabled = true;
    bhv->logic_op = ZB_LOGIC_OR;  // значение по умолчанию
}

void zbm_behavior_init(void) {
    s_behavior_count = 0;
    ESP_LOGI(TAG, "Behavior system initializing...");
    zbm_behavior_load_all_from_storage();
    ESP_LOGI(TAG, "✅ Behavior system initialized (%d behaviors loaded)", s_behavior_count);
}

const zbm_behavior_t* zbm_behavior_find(const char* behavior_id) {
    for (int i = 0; i < s_behavior_count; i++) {
        if (strcmp(s_behaviors[i].id, behavior_id) == 0 && s_behaviors[i].enabled) {
            return &s_behaviors[i];
        }
    }
    return NULL;
}

void zbm_behavior_run(const zbm_behavior_t* bhv) {
    // Если есть условия — проверяем
    if (bhv->condition_count > 0) {
        uint8_t matched = 0;
        uint8_t total = 0;

        for (int i = 0; i < bhv->condition_count; i++) {
            const zb_trigger_t* t = &bhv->conditions[i];
            total++;

            bool match = false;

            if (strncmp(t->guid, "var_", 4) == 0) {
                int idx = atoi(t->guid + 4);
                if (idx >= 0 && idx < ZB_AUTO_VAR_COUNT && zbm_vars[idx].p_value) {
                    match = value_matches(zbm_vars[idx].p_value, t->p_expected_value, t->expected_type, t->cond);
                }
            } else {
                zbm_cluster_attribute_t* attr = zbm_find_attr_by_guid_safe(t->guid);
                if (attr && attr->p_value && attr->data_type == t->expected_type) {
                    match = value_matches(attr->p_value, t->p_expected_value, t->expected_type, t->cond);
                }
                if (!match) {
                    zbm_cluster_custom_report_cmd_t* rep = zbm_find_custom_report_by_guid_safe(t->guid);
                    if (rep && rep->p_value && rep->data_type == (zbm_cmd_data_types_t)t->expected_type) {
                        match = value_matches(rep->p_value, t->p_expected_value, t->expected_type, t->cond);
                    }
                }
            }

            if (match) matched++;
        }

        bool allow = false;
        if (bhv->logic_op == ZB_LOGIC_OR) {
            allow = matched > 0;
        } else {
            allow = matched == total;
        }

        if (!allow) return;  // условия не выполнены
    }

    // Выполняем действия
    for (int i = 0; i < bhv->action_count; i++) {
        zb_automation_v2_execute_action(&bhv->actions[i]);
    }
    ESP_LOGI(TAG, "🔥 Behavior '%s' executed", bhv->id);
    ws_notify_automation_rule_fired(bhv->id, "behavior");
}

bool zbm_behavior_execute(const char* behavior_id) {
    const zbm_behavior_t* bhv = zbm_behavior_find(behavior_id);
    if (!bhv) {
        ESP_LOGW(TAG, "Behavior not found or disabled: %s", behavior_id);
        return false;
    }
    zbm_behavior_run(bhv);
    return true;
}

// === Загрузка из SPIFFS ===

static bool load_behavior_from_file(const char* path, zbm_behavior_t* out_bhv) {
    cJSON* json = read_json_from_file(path);
    if (!json) return false;

    memset(out_bhv, 0, sizeof(zbm_behavior_t));
    out_bhv->enabled = true;

    cJSON* id = cJSON_GetObjectItem(json, "id");
    cJSON* name = cJSON_GetObjectItem(json, "name");
    cJSON* enabled = cJSON_GetObjectItem(json, "enabled");
    cJSON* actions = cJSON_GetObjectItem(json, "actions");

    if (!id || !cJSON_IsString(id) || !name || !cJSON_IsString(name)) {
        cJSON_Delete(json);
        return false;
    }

    strncpy(out_bhv->id, id->valuestring, sizeof(out_bhv->id) - 1);
    strncpy(out_bhv->name, name->valuestring, sizeof(out_bhv->name) - 1);
    if (enabled) out_bhv->enabled = cJSON_IsTrue(enabled);

    if (cJSON_IsArray(actions)) {
        int count = cJSON_GetArraySize(actions);
        out_bhv->action_count = (count > ZBM_BEHAVIOR_ACTIONS_MAX) ? ZBM_BEHAVIOR_ACTIONS_MAX : count;

        for (int i = 0; i < out_bhv->action_count; i++) {
            cJSON* a = cJSON_GetArrayItem(actions, i);
            if (!a) continue;
            if (!zb_automation_v2_rule_from_json_action(a, &out_bhv->actions[i])) {  // см. ниже
                out_bhv->action_count = i;
                break;
            }
        }
    }

    // === Парсинг условий (опционально) ===
    cJSON* conditions = cJSON_GetObjectItem(json, "conditions");
    if (cJSON_IsArray(conditions)) {
        int count = cJSON_GetArraySize(conditions);
        out_bhv->condition_count = (count > ZB_AUTO_MAX_TRIGGERS) ? ZB_AUTO_MAX_TRIGGERS : count;

        for (int i = 0; i < out_bhv->condition_count; i++) {
            cJSON* t_json = cJSON_GetArrayItem(conditions, i);
            zb_trigger_t* t = &out_bhv->conditions[i];

            cJSON* guid = cJSON_GetObjectItem(t_json, "guid");
            cJSON* cond = cJSON_GetObjectItem(t_json, "cond");
            cJSON* expected_type = cJSON_GetObjectItem(t_json, "expected_type");
            cJSON* value = cJSON_GetObjectItem(t_json, "value");

            if (!guid || !cJSON_IsString(guid) || !cond || !cJSON_IsNumber(cond) || !expected_type || !cJSON_IsNumber(expected_type)) {
                continue;
            }

            strncpy(t->guid, guid->valuestring, sizeof(t->guid) - 1);
            t->cond = (zb_condition_t)cond->valueint;
            t->expected_type = (zbm_attr_data_types_t)expected_type->valueint;

            size_t val_size = 1;
            switch (t->expected_type) {
                case ZBM_ATTR_TYPE_U8: case ZBM_ATTR_TYPE_BOOL: case ZBM_ATTR_TYPE_S8: val_size = 1; break;
                case ZBM_ATTR_TYPE_U16: case ZBM_ATTR_TYPE_S16: val_size = 2; break;
                case ZBM_ATTR_TYPE_CHAR_STRING: case ZBM_ATTR_TYPE_LONG_CHAR_STRING: val_size = strlen(value->valuestring) + 1; break;
                default: val_size = 1; break;
            }
            t->expected_size = val_size;
            t->p_expected_value = malloc(val_size);
            if (!t->p_expected_value) continue;

            switch (t->expected_type) {
                case ZBM_ATTR_TYPE_BOOL: case ZBM_ATTR_TYPE_U8: case ZBM_ATTR_TYPE_S8:
                    *(uint8_t*)t->p_expected_value = (uint8_t)value->valueint; break;
                case ZBM_ATTR_TYPE_U16: case ZBM_ATTR_TYPE_S16:
                    *(uint16_t*)t->p_expected_value = (uint16_t)value->valueint; break;
                case ZBM_ATTR_TYPE_CHAR_STRING: case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
                    strcpy((char*)t->p_expected_value, value->valuestring); break;
                default:
                    *(uint8_t*)t->p_expected_value = (uint8_t)value->valueint; break;
            }
        }
    }

    // Логика объединения
    cJSON* logic_obj = cJSON_GetObjectItem(json, "logic_op");
    out_bhv->logic_op = logic_obj ? (zb_logic_op_t)logic_obj->valueint : ZB_LOGIC_OR;

    cJSON_Delete(json);
    return true;
}

void zbm_behavior_load_all_from_storage(void) {
    cJSON* index = read_json_from_file(BEHAVIORS_INDEX_FILE);
    if (!index) {
        ESP_LOGW(TAG, "No behaviors index found at %s", BEHAVIORS_INDEX_FILE);
        return;
    }

    int loaded = 0;
    for (int i = 0; i < cJSON_GetArraySize(index); i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        cJSON* item_id = cJSON_GetObjectItem(item, "id");
        cJSON* item_path = cJSON_GetObjectItem(item, "path");
        cJSON* item_enabled = cJSON_GetObjectItem(item, "enabled");

        if (!item_id || !item_path) continue;

        if (s_behavior_count >= ZBM_BEHAVIOR_MAX_COUNT) {
            ESP_LOGE(TAG, "Too many behaviors. Max: %d", ZBM_BEHAVIOR_MAX_COUNT);
            break;
        }

        zbm_behavior_t temp_bhv;
        if (load_behavior_from_file(item_path->valuestring, &temp_bhv)) {
            // Обновляем enabled из индекса
            if (item_enabled) temp_bhv.enabled = cJSON_IsTrue(item_enabled);

            memcpy(&s_behaviors[s_behavior_count], &temp_bhv, sizeof(zbm_behavior_t));
            s_behavior_count++;
            ESP_LOGI(TAG, "✅ Loaded behavior: %s", temp_bhv.name);
            loaded++;
        } else {
            ESP_LOGE(TAG, "Failed to load behavior: %s", item_id->valuestring);
        }
    }

    cJSON_Delete(index);
    ESP_LOGI(TAG, "Loaded %d behaviors", loaded);
}

// === Сохранение ===

bool zbm_behavior_save_to_storage(const zbm_behavior_t* bhv) {
    // Создаём JSON
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "id", bhv->id);
    cJSON_AddStringToObject(json, "name", bhv->name);
    cJSON_AddBoolToObject(json, "enabled", bhv->enabled);

    cJSON* actions = cJSON_CreateArray();
    for (int i = 0; i < bhv->action_count; i++) {
        cJSON* a = zb_automation_v2_rule_to_json_action(&bhv->actions[i]);  // см. ниже
        if (a) cJSON_AddItemToArray(actions, a);
    }
    cJSON_AddItemToObject(json, "actions", actions);

    // Сохранение условий
    if (bhv->condition_count > 0) {
        cJSON* conditions = cJSON_CreateArray();
        for (int i = 0; i < bhv->condition_count; i++) {
            const zb_trigger_t* t = &bhv->conditions[i];
            cJSON* t_json = cJSON_CreateObject();
            cJSON_AddStringToObject(t_json, "guid", t->guid);
            cJSON_AddNumberToObject(t_json, "cond", t->cond);
            cJSON_AddNumberToObject(t_json, "expected_type", t->expected_type);
            switch (t->expected_type) {
                case ZBM_ATTR_TYPE_U8: case ZBM_ATTR_TYPE_BOOL: case ZBM_ATTR_TYPE_S8:
                    cJSON_AddNumberToObject(t_json, "value", *(uint8_t*)t->p_expected_value);
                    break;
                case ZBM_ATTR_TYPE_U16: case ZBM_ATTR_TYPE_S16:
                    cJSON_AddNumberToObject(t_json, "value", *(uint16_t*)t->p_expected_value);
                    break;
                case ZBM_ATTR_TYPE_CHAR_STRING: case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
                    cJSON_AddStringToObject(t_json, "value", (char*)t->p_expected_value);
                    break;
                default:
                    cJSON_AddNumberToObject(t_json, "value", *(uint8_t*)t->p_expected_value);
                    break;
            }
            cJSON_AddItemToArray(conditions, t_json);
        }
        cJSON_AddItemToObject(json, "conditions", conditions);
        cJSON_AddNumberToObject(json, "logic_op", bhv->logic_op);
    }

    // Путь: /spiffs/behaviors/{id}.json
    char path[ZBM_BEHAVIOR_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s.json", SPIFFS_ZBM_CONF_MOUNT_POINT, bhv->id);

    bool saved = write_json_to_file(path, json);
    cJSON_Delete(json);

    if (!saved) return false;

    // Обновляем индекс
    cJSON* index = read_json_from_file(BEHAVIORS_INDEX_FILE);
    if (!index) index = cJSON_CreateArray();

    // Удаляем старую запись
    for (int i = 0; i < cJSON_GetArraySize(index); i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        if (strcmp(cJSON_GetObjectItem(item, "id")->valuestring, bhv->id) == 0) {
            cJSON_DeleteItemFromArray(index, i);
            break;
        }
    }

    // Добавляем новую
    cJSON* entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "id", bhv->id);
    cJSON_AddStringToObject(entry, "path", path);
    cJSON_AddBoolToObject(entry, "enabled", bhv->enabled);
    cJSON_AddItemToArray(index, entry);

    write_json_to_file(BEHAVIORS_INDEX_FILE, index);
    cJSON_Delete(index);

    ESP_LOGI(TAG, "Behavior '%s' saved to %s", bhv->id, path);
    return true;
}

bool zbm_behavior_remove_from_storage(const char* behavior_id) {
    char path[ZBM_BEHAVIOR_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s.json", SPIFFS_ZBM_CONF_MOUNT_POINT, behavior_id);
    unlink(path);

    // Обновляем индекс
    cJSON* index = read_json_from_file(BEHAVIORS_INDEX_FILE);
    if (!index) return true;

    bool found = false;
    for (int i = 0; i < cJSON_GetArraySize(index); i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        if (strcmp(cJSON_GetObjectItem(item, "id")->valuestring, behavior_id) == 0) {
            cJSON_DeleteItemFromArray(index, i);
            found = true;
            break;
        }
    }

    if (found) {
        write_json_to_file(BEHAVIORS_INDEX_FILE, index);
    }
    cJSON_Delete(index);
    return found;
}