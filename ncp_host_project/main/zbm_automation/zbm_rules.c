// main/zbm_automation/zbm_rules.c
#include "zbm_rules.h"
#include "zbm_automation.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <time.h>
#include <math.h>  // Для fabs()
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "ZB_RULE";

// Внешние переменные
uint8_t virtual_var[ZB_VIRTUAL_VAR_COUNT] = {0};
zb_rule_t** rules_array = NULL;
uint8_t rules_count = 0;

delayed_action_t delayed_actions[8] = {0};
uint8_t delayed_count = 0;

static void execute_rule_actions(const zb_rule_t* rule);
static bool rule_matches_trigger(const zb_rule_t* rule, uint16_t short_addr,
                                 uint16_t cluster_id, uint16_t attr_id,
                                 void* data, uint8_t data_len, zbm_attr_data_types_t attr_type);

// ===================================================================
//                         Вспомогательные функции
// ===================================================================

/**
 * @brief Сравнивает два значения по вашему типу zbm_attr_data_types_t
 */
static bool value_compare(void* val1, void* val2, zbm_attr_data_types_t type, zb_rule_condition_t cond) {
    switch (type) {
        case ZBM_ATTR_TYPE_BOOL:
        case ZBM_ATTR_TYPE_U8:
        case ZBM_ATTR_TYPE_T8BIT:
        case ZBM_ATTR_TYPE_T8BIT_ENUM:
        case ZBM_ATTR_TYPE_T8BITMAP: {
            uint8_t a = *(uint8_t*)val1;
            uint8_t b = *(uint8_t*)val2;
            switch (cond) {
                case ZB_RULE_COND_EQ: return a == b;
                case ZB_RULE_COND_NE: return a != b;
                case ZB_RULE_COND_GT: return a > b;
                case ZB_RULE_COND_LT: return a < b;
                case ZB_RULE_COND_GTE: return a >= b;
                case ZB_RULE_COND_LTE: return a <= b;
            }
            break;
        }

        case ZBM_ATTR_TYPE_S8: {
            int8_t a = *(int8_t*)val1;
            int8_t b = *(int8_t*)val2;
            switch (cond) {
                case ZB_RULE_COND_EQ: return a == b;
                case ZB_RULE_COND_NE: return a != b;
                case ZB_RULE_COND_GT: return a > b;
                case ZB_RULE_COND_LT: return a < b;
                case ZB_RULE_COND_GTE: return a >= b;
                case ZB_RULE_COND_LTE: return a <= b;
            }
            break;
        }

        case ZBM_ATTR_TYPE_U16:
        case ZBM_ATTR_TYPE_T16BIT:
        case ZBM_ATTR_TYPE_T16BIT_ENUM:
        case ZBM_ATTR_TYPE_T16BITMAP:
        case ZBM_ATTR_TYPE_CLUSTER_ID:
        case ZBM_ATTR_TYPE_ATTRIBUTE_ID: {
            uint16_t a = *(uint16_t*)val1;
            uint16_t b = *(uint16_t*)val2;
            switch (cond) {
                case ZB_RULE_COND_EQ: return a == b;
                case ZB_RULE_COND_NE: return a != b;
                case ZB_RULE_COND_GT: return a > b;
                case ZB_RULE_COND_LT: return a < b;
                case ZB_RULE_COND_GTE: return a >= b;
                case ZB_RULE_COND_LTE: return a <= b;
            }
            break;
        }

        case ZBM_ATTR_TYPE_S16: {
            int16_t a = *(int16_t*)val1;
            int16_t b = *(int16_t*)val2;
            switch (cond) {
                case ZB_RULE_COND_EQ: return a == b;
                case ZB_RULE_COND_NE: return a != b;
                case ZB_RULE_COND_GT: return a > b;
                case ZB_RULE_COND_LT: return a < b;
                case ZB_RULE_COND_GTE: return a >= b;
                case ZB_RULE_COND_LTE: return a <= b;
            }
            break;
        }

        case ZBM_ATTR_TYPE_U32:
        case ZBM_ATTR_TYPE_S32:
        case ZBM_ATTR_TYPE_T32BIT:
        case ZBM_ATTR_TYPE_T32BITMAP:
        case ZBM_ATTR_TYPE_UTC_TIME:
        case ZBM_ATTR_TYPE_DATE:
        case ZBM_ATTR_TYPE_TIME_OF_DAY:
        case ZBM_ATTR_TYPE_BACNET_OID: {
            uint32_t a = *(uint32_t*)val1;
            uint32_t b = *(uint32_t*)val2;
            switch (cond) {
                case ZB_RULE_COND_EQ: return a == b;
                case ZB_RULE_COND_NE: return a != b;
                case ZB_RULE_COND_GT: return a > b;
                case ZB_RULE_COND_LT: return a < b;
                case ZB_RULE_COND_GTE: return a >= b;
                case ZB_RULE_COND_LTE: return a <= b;
            }
            break;
        }

        case ZBM_ATTR_TYPE_SINGLE: {
            float f_a = *(float*)val1;
            float f_b = *(float*)val2;
            double a = (double)f_a;
            double b = (double)f_b;
            switch (cond) {
                case ZB_RULE_COND_EQ: return fabs(a - b) < 1e-6;
                case ZB_RULE_COND_NE: return fabs(a - b) >= 1e-6;
                case ZB_RULE_COND_GT: return a > b;
                case ZB_RULE_COND_LT: return a < b;
                case ZB_RULE_COND_GTE: return a > b || fabs(a - b) < 1e-6;
                case ZB_RULE_COND_LTE: return a < b || fabs(a - b) < 1e-6;
            }
            break;
        }

        case ZBM_ATTR_TYPE_DOUBLE: {
            double a = *(double*)val1;
            double b = *(double*)val2;
            switch (cond) {
                case ZB_RULE_COND_EQ: return fabs(a - b) < 1e-9;
                case ZB_RULE_COND_NE: return fabs(a - b) >= 1e-9;
                case ZB_RULE_COND_GT: return a > b;
                case ZB_RULE_COND_LT: return a < b;
                case ZB_RULE_COND_GTE: return a > b || fabs(a - b) < 1e-9;
                case ZB_RULE_COND_LTE: return a < b || fabs(a - b) < 1e-9;
            }
            break;
        }

        // Поддержка строк
        case ZBM_ATTR_TYPE_CHAR_STRING:
        case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
        case ZBM_ATTR_TYPE_OCTET_STRING:
        case ZBM_ATTR_TYPE_LONG_OCTET_STRING: {
            char* a = (char*)val1;
            char* b = (char*)val2;
            int cmp = strcmp(a, b);
            switch (cond) {
                case ZB_RULE_COND_EQ: return cmp == 0;
                case ZB_RULE_COND_NE: return cmp != 0;
                case ZB_RULE_COND_GT: return cmp > 0;
                case ZB_RULE_COND_LT: return cmp < 0;
                default: return false;
            }
            break;
        }

        default:
            ESP_LOGW(TAG, "Unsupported attribute type for comparison: 0x%02x", type);
            return false;
    }
    return false;
}

// ===================================================================
//                         Движок правил
// ===================================================================

static bool rule_matches_trigger(const zb_rule_t* rule, uint16_t short_addr,
                                 uint16_t cluster_id, uint16_t attr_id,
                                 void* data, uint8_t data_len, zbm_attr_data_types_t attr_type) {
    bool any_match = false;
    bool all_match = true;

    for (int i = 0; i < rule->trigger_count; i++) {
        const zb_rule_trigger_t* t = &rule->triggers[i];
        bool match = false;

        if (t->type == ZB_RULE_TRIGGER_DEVICE_STATE) {
            const zb_rule_trigger_t* dev_trig = &rule->triggers[i];

            if (dev_trig->data.device_state.short_addr != short_addr &&
                dev_trig->data.device_state.short_addr != 0) continue;
            if (dev_trig->data.device_state.cluster_id != cluster_id) continue;
            if (dev_trig->data.device_state.attr_id != attr_id) continue;
            if (dev_trig->data.device_state.expected_type != attr_type) continue;
            if (dev_trig->data.device_state.expected_len != data_len) continue;

            match = value_compare(data, dev_trig->data.device_state.p_expected_value,
                                  attr_type, dev_trig->data.device_state.cond);
        }
        else if (t->type == ZB_RULE_TRIGGER_VIRTUAL_VAR) {
            uint8_t current_val = virtual_var[t->data.virtual_var.var_index];
            uint8_t target_val = t->data.virtual_var.value;
            switch (t->data.virtual_var.cond) {
                case ZB_RULE_COND_EQ: match = (current_val == target_val); break;
                case ZB_RULE_COND_NE: match = (current_val != target_val); break;
                case ZB_RULE_COND_GT: match = (current_val > target_val); break;
                case ZB_RULE_COND_LT: match = (current_val < target_val); break;
                case ZB_RULE_COND_GTE: match = (current_val >= target_val); break;
                case ZB_RULE_COND_LTE: match = (current_val <= target_val); break;
            }
        }
        else if (t->type == ZB_RULE_TRIGGER_DEVICE_UNAVAILABLE) {
            match = (t->data.device_unavailable.short_addr == short_addr &&
                     t->data.device_unavailable.cluster_id == cluster_id);
        }

        if (match) {
            any_match = true;
        } else {
            all_match = false;
        }
    }

    return (rule->trigger_logic == ZB_RULE_TRIGGER_LOGIC_ANY) ? any_match : all_match;
}

static void execute_rule_actions(const zb_rule_t* rule) {
    for (int i = 0; i < rule->action_count; i++) {
        const zb_rule_action_t* a = &rule->actions[i];

        switch (a->type) {
            case ZB_RULE_ACTION_DEVICE_CMD: {
                zb_automation_request_t req = {0};
                req.short_addr = a->data.device_cmd.short_addr;
                req.endpoint_id = a->data.device_cmd.endpoint;
                req.cmd_id = a->data.device_cmd.cmd_id;
                zb_automation_send_command(&req);
                ESP_LOGI(TAG, "✅ Action: device_cmd %04x ep=%d cmd=%d", req.short_addr, req.endpoint_id, req.cmd_id);
                break;
            }

            case ZB_RULE_ACTION_SET_VIRTUAL_VAR:
                virtual_var[a->data.set_virtual_var.var_index] = a->data.set_virtual_var.value;
                ws_notify_virtual_vars_update();
                ESP_LOGI(TAG, "📌 VAR[%d] = %d", a->data.set_virtual_var.var_index, a->data.set_virtual_var.value);
                break;

            case ZB_RULE_ACTION_INC_VIRTUAL_VAR:
                if (virtual_var[a->data.set_virtual_var.var_index] < 255) {
                    virtual_var[a->data.set_virtual_var.var_index]++;
                    ws_notify_virtual_vars_update();
                    ESP_LOGI(TAG, "📈 VAR[%d]++ → %d", a->data.set_virtual_var.var_index, virtual_var[a->data.set_virtual_var.var_index]);
                }
                break;

            case ZB_RULE_ACTION_DEC_VIRTUAL_VAR:
                if (virtual_var[a->data.set_virtual_var.var_index] > 0) {
                    virtual_var[a->data.set_virtual_var.var_index]--;
                    ws_notify_virtual_vars_update();
                    ESP_LOGI(TAG, "📉 VAR[%d]-- → %d", a->data.set_virtual_var.var_index, virtual_var[a->data.set_virtual_var.var_index]);
                }
                break;

            case ZB_RULE_ACTION_TOGGLE_VIRTUAL_VAR:
                virtual_var[a->data.set_virtual_var.var_index] = !virtual_var[a->data.set_virtual_var.var_index];
                ws_notify_virtual_vars_update();
                ESP_LOGI(TAG, "🔁 VAR[%d] toggled → %d", a->data.set_virtual_var.var_index, virtual_var[a->data.set_virtual_var.var_index]);
                break;

            default:
                ESP_LOGW(TAG, "Unknown action type: %d", a->type);
                break;
        }
    }
}

// ===================================================================
//                         Обработка событий
// ===================================================================

void zb_rule_trigger_state_update(uint16_t short_addr, uint16_t cluster_id,
                                  uint16_t attr_id, void* data, uint8_t data_len,
                                  zbm_attr_data_types_t attr_type) {
    for (int i = 0; i < rules_count; i++) {
        zb_rule_t* rule = rules_array[i];
        if (!rule || !rule->enabled) continue;

        if (rule_matches_trigger(rule, short_addr, cluster_id, attr_id, data, data_len, attr_type)) {
            execute_rule_actions(rule);
            return;
        }
    }
}

void zb_rule_trigger_device_unavailable(uint16_t short_addr, uint16_t cluster_id) {
    for (int i = 0; i < rules_count; i++) {
        zb_rule_t* rule = rules_array[i];
        if (!rule || !rule->enabled) continue;

        for (int j = 0; j < rule->trigger_count; j++) {
            if (rule->triggers[j].type == ZB_RULE_TRIGGER_DEVICE_UNAVAILABLE) {
                if (rule->triggers[j].data.device_unavailable.short_addr == short_addr &&
                    rule->triggers[j].data.device_unavailable.cluster_id == cluster_id) {
                    execute_rule_actions(rule);
                    return;
                }
            }
        }
    }
}

// ===================================================================
//                         Временные триггеры
// ===================================================================

void check_time_triggers(void) {
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    int current_sec = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
    uint8_t day_bit = 1 << ((timeinfo.tm_wday == 0) ? 6 : timeinfo.tm_wday - 1);

    for (int i = 0; i < rules_count; i++) {
        zb_rule_t* rule = rules_array[i];
        if (!rule || !rule->enabled) continue;

        for (int j = 0; j < rule->trigger_count; j++) {
            if (rule->triggers[j].type != ZB_RULE_TRIGGER_TIME_RANGE) continue;

            const zb_rule_trigger_t* tr = &rule->triggers[j];

            if (!(tr->data.time_range.days_of_week & day_bit)) continue;

            int from_h, from_m, to_h, to_m;
            if (sscanf(tr->data.time_range.from, "%d:%d", &from_h, &from_m) != 2) continue;
            if (sscanf(tr->data.time_range.to, "%d:%d", &to_h, &to_m) != 2) continue;

            int from_sec = from_h * 3600 + from_m * 60;
            int to_sec = to_h * 3600 + to_m * 60;

            bool in_range;
            if (from_sec <= to_sec) {
                in_range = (current_sec >= from_sec && current_sec <= to_sec);
            } else {
                in_range = (current_sec >= from_sec || current_sec <= to_sec);
            }

            if (in_range) {
                if (tr->data.time_range.delay_sec == 0) {
                    execute_rule_actions(rule);
                } else {
                    schedule_delayed_action(rule->id, tr->data.time_range.delay_sec);
                }
                break;
            }
        }
    }
}

void schedule_delayed_action(const char* rule_id, uint32_t delay_sec) {
    if (delayed_count >= 8) return;
    if (strlen(rule_id) >= 32) return;

    for (int i = 0; i < delayed_count; i++) {
        if (strcmp(delayed_actions[i].rule_id, rule_id) == 0) {
            delayed_actions[i].fire_at = time(NULL) + delay_sec;
            return;
        }
    }

    strcpy(delayed_actions[delayed_count].rule_id, rule_id);
    delayed_actions[delayed_count].fire_at = time(NULL) + delay_sec;
    delayed_count++;
}

void process_delayed_actions(void) {
    time_t now = time(NULL);
    for (int i = 0; i < delayed_count; i++) {
        if (delayed_actions[i].fire_at <= now) {
            const zb_rule_t* rule = zb_rule_engine_get_rule(delayed_actions[i].rule_id);
            if (rule && rule->enabled) {
                execute_rule_actions(rule);
            }
            memmove(&delayed_actions[i], &delayed_actions[i+1], (--delayed_count - i) * sizeof(delayed_action_t));
            i--;
        }
    }
}

// ===================================================================
//                         Управление переменными
// ===================================================================

void zb_rule_set_var(int idx, uint8_t value) {
    if (idx < ZB_VIRTUAL_VAR_COUNT) {
        virtual_var[idx] = value;
        ws_notify_virtual_vars_update();
    }
}

void zb_rule_inc_var(int idx) {
    if (idx < ZB_VIRTUAL_VAR_COUNT && virtual_var[idx] < 255) {
        virtual_var[idx]++;
        ws_notify_virtual_vars_update();
    }
}

void zb_rule_dec_var(int idx) {
    if (idx < ZB_VIRTUAL_VAR_COUNT && virtual_var[idx] > 0) {
        virtual_var[idx]--;
        ws_notify_virtual_vars_update();
    }
}

void zb_rule_toggle_var(int idx) {
    if (idx < ZB_VIRTUAL_VAR_COUNT) {
        virtual_var[idx] = !virtual_var[idx];
        ws_notify_virtual_vars_update();
    }
}

// ===================================================================
//                         Управление правилами
// ===================================================================

void zb_rule_engine_init(void) {
    rules_array = calloc(ZB_RULE_MAX_COUNT, sizeof(zb_rule_t*));
    if (!rules_array) {
        ESP_LOGE(TAG, "Failed to allocate rules array");
    }
    // Переменные уже проинициализированы
}

bool zb_rule_engine_add_rule(const zb_rule_t* rule_template) {
    if (!rule_template || rules_count >= ZB_RULE_MAX_COUNT) return false;

    zb_rule_t* new_rule = calloc(1, sizeof(zb_rule_t));
    if (!new_rule) return false;

    memcpy(new_rule, rule_template, sizeof(zb_rule_t));

    // Копируем p_expected_value для каждого device_state триггера
    for (int i = 0; i < new_rule->trigger_count; i++) {
        if (new_rule->triggers[i].type == ZB_RULE_TRIGGER_DEVICE_STATE) {
            void* src = new_rule->triggers[i].data.device_state.p_expected_value;
            if (src && new_rule->triggers[i].data.device_state.expected_len > 0) {
                void* copy = malloc(new_rule->triggers[i].data.device_state.expected_len);
                if (copy) {
                    memcpy(copy, src, new_rule->triggers[i].data.device_state.expected_len);
                    new_rule->triggers[i].data.device_state.p_expected_value = copy;
                }
            }
        }
    }

    rules_array[rules_count++] = new_rule;
    ws_notify_rules_update();
    return true;
}

bool zb_rule_engine_update_rule(const char* rule_id, const zb_rule_t* updated_rule) {
    for (int i = 0; i < rules_count; i++) {
        if (strcmp(rules_array[i]->id, rule_id) == 0) {
            // Освобождаем старые p_expected_value
            for (int j = 0; j < rules_array[i]->trigger_count; j++) {
                if (rules_array[i]->triggers[j].type == ZB_RULE_TRIGGER_DEVICE_STATE) {
                    if (rules_array[i]->triggers[j].data.device_state.p_expected_value) {
                        free(rules_array[i]->triggers[j].data.device_state.p_expected_value);
                        rules_array[i]->triggers[j].data.device_state.p_expected_value = NULL;
                    }
                }
            }
            free(rules_array[i]);
            return zb_rule_engine_add_rule(updated_rule);
        }
    }
    return false;
}

bool zb_rule_engine_remove_rule(const char* rule_id) {
    for (int i = 0; i < rules_count; i++) {
        if (strcmp(rules_array[i]->id, rule_id) == 0) {
            // Освобождаем p_expected_value
            for (int j = 0; j < rules_array[i]->trigger_count; j++) {
                if (rules_array[i]->triggers[j].type == ZB_RULE_TRIGGER_DEVICE_STATE) {
                    if (rules_array[i]->triggers[j].data.device_state.p_expected_value) {
                        free(rules_array[i]->triggers[j].data.device_state.p_expected_value);
                    }
                    rules_array[i]->triggers[j].data.device_state.p_expected_value = NULL;
                }
            }
            free(rules_array[i]);
            memmove(&rules_array[i], &rules_array[i+1], (rules_count - i - 1) * sizeof(zb_rule_t*));
            rules_count--;
            ws_notify_rules_update();
            return true;
        }
    }
    return false;
}

bool zb_rule_engine_remove_all_rules(void) {
    for (int i = 0; i < rules_count; i++) {
        if (rules_array[i]) {
            for (int j = 0; j < rules_array[i]->trigger_count; j++) {
                if (rules_array[i]->triggers[j].type == ZB_RULE_TRIGGER_DEVICE_STATE) {
                    if (rules_array[i]->triggers[j].data.device_state.p_expected_value) {
                        free(rules_array[i]->triggers[j].data.device_state.p_expected_value);
                    }
                }
            }
            free(rules_array[i]);
        }
    }
    rules_count = 0;
    ws_notify_rules_update();
    return true;
}

const zb_rule_t* zb_rule_engine_get_rule(const char* rule_id) {
    for (int i = 0; i < rules_count; i++) {
        if (rules_array[i] && strcmp(rules_array[i]->id, rule_id) == 0) {
            return rules_array[i];
        }
    }
    return NULL;
}

bool zb_automation_run_rule_now(const char* rule_id) {
    const zb_rule_t* rule = zb_rule_engine_get_rule(rule_id);
    if (rule && rule->enabled) {
        execute_rule_actions(rule);
        return true;
    }
    return false;
}

// ===================================================================
//                         JSON (API)
// ===================================================================

bool rule_from_json(cJSON* json, zb_rule_t* out_rule) {
    if (!json || !out_rule) return false;

    memset(out_rule, 0, sizeof(zb_rule_t));

    // Обязательные поля
    cJSON* id = cJSON_GetObjectItem(json, "id");
    cJSON* name = cJSON_GetObjectItem(json, "name");
    if (!id || !name) return false;

    strncpy(out_rule->id, id->valuestring, sizeof(out_rule->id) - 1);
    strncpy(out_rule->name, name->valuestring, sizeof(out_rule->name) - 1);

    cJSON* module = cJSON_GetObjectItem(json, "module");
    if (module) {
        strncpy(out_rule->module, module->valuestring, sizeof(out_rule->module) - 1);
    }

    cJSON* priority = cJSON_GetObjectItem(json, "priority");
    out_rule->priority = priority ? (uint8_t)priority->valuedouble : 3;

    cJSON* enabled = cJSON_GetObjectItem(json, "enabled");
    out_rule->enabled = enabled ? (bool)enabled->valueint : true;

    cJSON* trigger_logic = cJSON_GetObjectItem(json, "trigger_logic");
    if (trigger_logic && strcmp(trigger_logic->valuestring, "all") == 0) {
        out_rule->trigger_logic = ZB_RULE_TRIGGER_LOGIC_ALL;
    } else {
        out_rule->trigger_logic = ZB_RULE_TRIGGER_LOGIC_ANY;
    }

    // Парсинг триггеров
    cJSON* triggers = cJSON_GetObjectItem(json, "triggers");
    if (cJSON_IsArray(triggers)) {
        out_rule->trigger_count = cJSON_GetArraySize(triggers);
        if (out_rule->trigger_count > ZB_RULE_MAX_TRIGGERS) {
            out_rule->trigger_count = ZB_RULE_MAX_TRIGGERS;
        }

        for (int i = 0; i < out_rule->trigger_count; i++) {
            cJSON* t = cJSON_GetArrayItem(triggers, i);
            cJSON* type = cJSON_GetObjectItem(t, "type");
            if (!type) continue;

            zb_rule_trigger_t* trig = &out_rule->triggers[i];

            if (strcmp(type->valuestring, "device_state") == 0) {
                trig->type = ZB_RULE_TRIGGER_DEVICE_STATE;
                trig->data.device_state.short_addr = (uint16_t)cJSON_GetObjectItem(t, "short_addr")->valuedouble;
                trig->data.device_state.cluster_id = (uint16_t)cJSON_GetObjectItem(t, "cluster_id")->valuedouble;
                trig->data.device_state.attr_id = (uint16_t)cJSON_GetObjectItem(t, "attr_id")->valuedouble;
                trig->data.device_state.cond = (zb_rule_condition_t)cJSON_GetObjectItem(t, "cond")->valuedouble;
                trig->data.device_state.expected_type = (zbm_attr_data_types_t)cJSON_GetObjectItem(t, "expected_type")->valuedouble;
                trig->data.device_state.expected_len = (uint8_t)cJSON_GetObjectItem(t, "expected_len")->valuedouble;

                // === Копируем значение в зависимости от типа ===
                void* copy = NULL;
                uint8_t len = trig->data.device_state.expected_len;

                if (len > 0) {
                    copy = malloc(len);
                    if (!copy) continue;
                    memset(copy, 0, len); // обнуляем
                }

                if (trig->data.device_state.expected_type == ZBM_ATTR_TYPE_CHAR_STRING ||
                    trig->data.device_state.expected_type == ZBM_ATTR_TYPE_LONG_CHAR_STRING ||
                    trig->data.device_state.expected_type == ZBM_ATTR_TYPE_OCTET_STRING ||
                    trig->data.device_state.expected_type == ZBM_ATTR_TYPE_LONG_OCTET_STRING) {

                    cJSON* val_str = cJSON_GetObjectItem(t, "expected_value");
                    if (val_str && val_str->valuestring) {
                        size_t str_len = strlen(val_str->valuestring);
                        size_t copy_len = (str_len < len) ? str_len : len - 1;
                        memcpy(copy, val_str->valuestring, copy_len);
                    }
                }
                else if (trig->data.device_state.expected_type == ZBM_ATTR_TYPE_SINGLE) {
                    double val = cJSON_GetObjectItem(t, "expected_value")->valuedouble;
                    *(float*)copy = (float)val;
                }
                else if (trig->data.device_state.expected_type == ZBM_ATTR_TYPE_DOUBLE) {
                    double val = cJSON_GetObjectItem(t, "expected_value")->valuedouble;
                    *(double*)copy = val;
                }
                else {
                    // Для всех остальных числовых типов
                    double val = cJSON_GetObjectItem(t, "expected_value")->valuedouble;
                    switch (trig->data.device_state.expected_type) {
                        case ZBM_ATTR_TYPE_BOOL:
                        case ZBM_ATTR_TYPE_U8:
                        case ZBM_ATTR_TYPE_S8:
                        case ZBM_ATTR_TYPE_T8BIT:
                        case ZBM_ATTR_TYPE_T8BIT_ENUM:
                        case ZBM_ATTR_TYPE_T8BITMAP:
                            *(uint8_t*)copy = (uint8_t)val; break;
                        case ZBM_ATTR_TYPE_U16:
                        case ZBM_ATTR_TYPE_S16:
                        case ZBM_ATTR_TYPE_T16BIT:
                        case ZBM_ATTR_TYPE_T16BIT_ENUM:
                        case ZBM_ATTR_TYPE_T16BITMAP:
                        case ZBM_ATTR_TYPE_CLUSTER_ID:
                        case ZBM_ATTR_TYPE_ATTRIBUTE_ID:
                            *(uint16_t*)copy = (uint16_t)val; break;
                        case ZBM_ATTR_TYPE_U32:
                        case ZBM_ATTR_TYPE_S32:
                        case ZBM_ATTR_TYPE_T32BIT:
                        case ZBM_ATTR_TYPE_T32BITMAP:
                        case ZBM_ATTR_TYPE_UTC_TIME:
                        case ZBM_ATTR_TYPE_DATE:
                        case ZBM_ATTR_TYPE_TIME_OF_DAY:
                        case ZBM_ATTR_TYPE_BACNET_OID:
                            *(uint32_t*)copy = (uint32_t)val; break;
                        default:
                            *(uint8_t*)copy = (uint8_t)val; break;
                    }
                }

                trig->data.device_state.p_expected_value = copy;
            }
            else if (strcmp(type->valuestring, "virtual_var") == 0) {
                trig->type = ZB_RULE_TRIGGER_VIRTUAL_VAR;
                trig->data.virtual_var.var_index = (uint8_t)cJSON_GetObjectItem(t, "var_index")->valuedouble;
                trig->data.virtual_var.value = (uint8_t)cJSON_GetObjectItem(t, "value")->valuedouble;
                trig->data.virtual_var.cond = (zb_rule_condition_t)cJSON_GetObjectItem(t, "cond")->valuedouble;
            }
            else if (strcmp(type->valuestring, "time_range") == 0) {
                trig->type = ZB_RULE_TRIGGER_TIME_RANGE;
                const char* from = cJSON_GetObjectItem(t, "from")->valuestring;
                const char* to = cJSON_GetObjectItem(t, "to")->valuestring;
                strncpy(trig->data.time_range.from, from, sizeof(trig->data.time_range.from) - 1);
                strncpy(trig->data.time_range.to, to, sizeof(trig->data.time_range.to) - 1);
                trig->data.time_range.days_of_week = (uint8_t)cJSON_GetObjectItem(t, "days_of_week")->valuedouble;
                trig->data.time_range.delay_sec = (uint32_t)cJSON_GetObjectItem(t, "delay_sec")->valuedouble;
            }
            // Добавьте другие типы при необходимости
        }
    }

    // Парсинг действий (оставь как есть — он не требует изменений)
    cJSON* actions = cJSON_GetObjectItem(json, "actions");
    if (cJSON_IsArray(actions)) {
        out_rule->action_count = cJSON_GetArraySize(actions);
        if (out_rule->action_count > ZB_RULE_MAX_ACTIONS) {
            out_rule->action_count = ZB_RULE_MAX_ACTIONS;
        }

        for (int i = 0; i < out_rule->action_count; i++) {
            cJSON* a = cJSON_GetArrayItem(actions, i);
            cJSON* type = cJSON_GetObjectItem(a, "type");
            if (!type) continue;

            zb_rule_action_t* act = &out_rule->actions[i];

            if (strcmp(type->valuestring, "device_cmd") == 0) {
                act->type = ZB_RULE_ACTION_DEVICE_CMD;
                act->data.device_cmd.short_addr = (uint16_t)cJSON_GetObjectItem(a, "short_addr")->valuedouble;
                act->data.device_cmd.endpoint = (uint8_t)cJSON_GetObjectItem(a, "endpoint")->valuedouble;
                act->data.device_cmd.cmd_id = (uint8_t)cJSON_GetObjectItem(a, "cmd_id")->valuedouble;
            }
            else if (strcmp(type->valuestring, "set_var") == 0) {
                act->type = ZB_RULE_ACTION_SET_VIRTUAL_VAR;
                act->data.set_virtual_var.var_index = (uint8_t)cJSON_GetObjectItem(a, "var_index")->valuedouble;
                act->data.set_virtual_var.value = (uint8_t)cJSON_GetObjectItem(a, "value")->valuedouble;
            }
            else if (strcmp(type->valuestring, "inc_var") == 0) {
                act->type = ZB_RULE_ACTION_INC_VIRTUAL_VAR;
                act->data.set_virtual_var.var_index = (uint8_t)cJSON_GetObjectItem(a, "var_index")->valuedouble;
            }
            else if (strcmp(type->valuestring, "dec_var") == 0) {
                act->type = ZB_RULE_ACTION_DEC_VIRTUAL_VAR;
                act->data.set_virtual_var.var_index = (uint8_t)cJSON_GetObjectItem(a, "var_index")->valuedouble;
            }
            else if (strcmp(type->valuestring, "toggle_var") == 0) {
                act->type = ZB_RULE_ACTION_TOGGLE_VIRTUAL_VAR;
                act->data.set_virtual_var.var_index = (uint8_t)cJSON_GetObjectItem(a, "var_index")->valuedouble;
            }
        }
    }

    return true;
}

// ===================================================================
//                         JSON (API)
// ===================================================================

cJSON* rule_to_json(const zb_rule_t* rule) {
    if (!rule) return NULL;

    cJSON* obj = cJSON_CreateObject();
    if (!obj) return NULL;

    cJSON_AddStringToObject(obj, "id", rule->id);
    cJSON_AddStringToObject(obj, "name", rule->name);
    cJSON_AddStringToObject(obj, "module", rule->module);
    cJSON_AddNumberToObject(obj, "priority", rule->priority);
    cJSON_AddBoolToObject(obj, "enabled", rule->enabled);
    cJSON_AddStringToObject(obj, "trigger_logic",
        rule->trigger_logic == ZB_RULE_TRIGGER_LOGIC_ALL ? "all" : "any");

    // === Триггеры ===
    cJSON* triggers = cJSON_CreateArray();
    for (int i = 0; i < rule->trigger_count; i++) {
        const zb_rule_trigger_t* t = &rule->triggers[i];
        cJSON* t_obj = cJSON_CreateObject();

        switch (t->type) {
            case ZB_RULE_TRIGGER_DEVICE_STATE: {
                cJSON_AddStringToObject(t_obj, "type", "device_state");
                cJSON_AddNumberToObject(t_obj, "short_addr", t->data.device_state.short_addr);
                cJSON_AddNumberToObject(t_obj, "cluster_id", t->data.device_state.cluster_id);
                cJSON_AddNumberToObject(t_obj, "attr_id", t->data.device_state.attr_id);
                cJSON_AddNumberToObject(t_obj, "cond", t->data.device_state.cond);
                cJSON_AddNumberToObject(t_obj, "expected_type", t->data.device_state.expected_type);
                cJSON_AddNumberToObject(t_obj, "expected_len", t->data.device_state.expected_len);

                if (t->data.device_state.p_expected_value && t->data.device_state.expected_len > 0) {
                    uint8_t val = *(uint8_t*)t->data.device_state.p_expected_value;
                    cJSON_AddNumberToObject(t_obj, "expected_value", val);
                }
                break;
            }

            case ZB_RULE_TRIGGER_VIRTUAL_VAR: {
                cJSON_AddStringToObject(t_obj, "type", "virtual_var");
                cJSON_AddNumberToObject(t_obj, "var_index", t->data.virtual_var.var_index);
                cJSON_AddNumberToObject(t_obj, "value", t->data.virtual_var.value);
                cJSON_AddNumberToObject(t_obj, "cond", t->data.virtual_var.cond);
                break;
            }

            case ZB_RULE_TRIGGER_TIME_RANGE: {
                cJSON_AddStringToObject(t_obj, "type", "time_range");
                cJSON_AddStringToObject(t_obj, "from", t->data.time_range.from);
                cJSON_AddStringToObject(t_obj, "to", t->data.time_range.to);
                cJSON_AddNumberToObject(t_obj, "days_of_week", t->data.time_range.days_of_week);
                cJSON_AddNumberToObject(t_obj, "delay_sec", t->data.time_range.delay_sec);
                break;
            }

            case ZB_RULE_TRIGGER_DEVICE_UNAVAILABLE: {
                cJSON_AddStringToObject(t_obj, "type", "device_unavailable");
                cJSON_AddNumberToObject(t_obj, "short_addr", t->data.device_unavailable.short_addr);
                cJSON_AddNumberToObject(t_obj, "cluster_id", t->data.device_unavailable.cluster_id);
                break;
            }

            default:
                cJSON_AddStringToObject(t_obj, "type", "unknown");
                break;
        }
        cJSON_AddItemToArray(triggers, t_obj);
    }
    cJSON_AddItemToObject(obj, "triggers", triggers);

    // === Действия ===
    cJSON* actions = cJSON_CreateArray();
    for (int i = 0; i < rule->action_count; i++) {
        const zb_rule_action_t* a = &rule->actions[i];
        cJSON* a_obj = cJSON_CreateObject();

        switch (a->type) {
            case ZB_RULE_ACTION_DEVICE_CMD:
                cJSON_AddStringToObject(a_obj, "type", "device_cmd");
                cJSON_AddNumberToObject(a_obj, "short_addr", a->data.device_cmd.short_addr);
                cJSON_AddNumberToObject(a_obj, "endpoint", a->data.device_cmd.endpoint);
                cJSON_AddNumberToObject(a_obj, "cmd_id", a->data.device_cmd.cmd_id);
                break;

            case ZB_RULE_ACTION_SET_VIRTUAL_VAR:
                cJSON_AddStringToObject(a_obj, "type", "set_var");
                cJSON_AddNumberToObject(a_obj, "var_index", a->data.set_virtual_var.var_index);
                cJSON_AddNumberToObject(a_obj, "value", a->data.set_virtual_var.value);
                break;

            case ZB_RULE_ACTION_INC_VIRTUAL_VAR:
                cJSON_AddStringToObject(a_obj, "type", "inc_var");
                cJSON_AddNumberToObject(a_obj, "var_index", a->data.set_virtual_var.var_index);
                break;

            case ZB_RULE_ACTION_DEC_VIRTUAL_VAR:
                cJSON_AddStringToObject(a_obj, "type", "dec_var");
                cJSON_AddNumberToObject(a_obj, "var_index", a->data.set_virtual_var.var_index);
                break;

            case ZB_RULE_ACTION_TOGGLE_VIRTUAL_VAR:
                cJSON_AddStringToObject(a_obj, "type", "toggle_var");
                cJSON_AddNumberToObject(a_obj, "var_index", a->data.set_virtual_var.var_index);
                break;

            default:
                cJSON_AddStringToObject(a_obj, "type", "unknown");
                break;
        }
        cJSON_AddItemToArray(actions, a_obj);
    }
    cJSON_AddItemToObject(obj, "actions", actions);

    return obj;
}