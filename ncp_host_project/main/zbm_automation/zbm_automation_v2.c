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
#include "zbm_attr_types.h"
#include "zbm_web_server.h"


static const char* TAG = "ZB_AUTO_V2";

zb_rule_t* zb_rules[ZB_AUTO_MAX_RULES] = {0};
uint8_t zb_rules_count = 0;

zbm_virtual_var_t zbm_vars[ZB_AUTO_VAR_COUNT] = {0};


// ========================================================
//                ВИРТУАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ========================================================

/**
 * @brief Перевыделяет p_value в зависимости от data_type
 * @return true если успешно
 */
bool zbm_var_realloc_storage(zbm_virtual_var_t *var) {
    if (!var) return false;

    uint16_t new_size = 0;
    switch (var->data_type) {
        case ZBM_ATTR_TYPE_U8:
        case ZBM_ATTR_TYPE_S8:
        case ZBM_ATTR_TYPE_BOOL:
            new_size = 1;
            break;
        case ZBM_ATTR_TYPE_U16:
        case ZBM_ATTR_TYPE_S16:
            new_size = 2;
            break;
        case ZBM_ATTR_TYPE_CHAR_STRING:
            new_size = ZB_AUTO_VAR_STR_LEN;
            break;
        case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
            new_size = ZB_AUTO_VAR_LONG_STR_LEN;
            break;
        default:
            return false;
    }

    bool size_changed = (var->data_size != new_size);
    bool reallocated = false;

    if (size_changed || var->p_value == NULL) {
        void* new_ptr = realloc(var->p_value, new_size);
        if (new_size > 0 && new_ptr == NULL) {
            ESP_LOGE(TAG, "Failed to realloc p_value for var %d", var->idx);
            return false;
        }
        var->p_value = new_ptr;
        reallocated = true;
    }

    if (size_changed || var->p_init_value == NULL) {
        void* new_init_ptr = realloc(var->p_init_value, new_size);
        if (new_size > 0 && new_init_ptr == NULL) {
            ESP_LOGE(TAG, "Failed to realloc p_init_value for var %d", var->idx);
            return false;
        }
        var->p_init_value = new_init_ptr;
        reallocated = true;
    }

    if (reallocated) {
        var->data_size = new_size;
        // Если память выделена заново — обнуляем
        if (var->p_value) memset(var->p_value, 0, new_size);
        if (var->p_init_value) memset(var->p_init_value, 0, new_size);
    }

    return true;
}

// zbm_automation_v2.c
void zbm_var_init(void) {
    for (int i = 0; i < ZB_AUTO_VAR_COUNT; i++) {
        snprintf(zbm_vars[i].name, sizeof(zbm_vars[i].name), "var_%d", i);
        snprintf(zbm_vars[i].guid, sizeof(zbm_vars[i].guid), "var_%d", i);
        zbm_vars[i].data_type = ZBM_ATTR_TYPE_U8;
        zbm_vars[i].data_size = 0;
        zbm_vars[i].p_value = NULL;
        zbm_vars[i].p_init_value = NULL;
        zbm_vars[i].last_update_ms = 0;
        zbm_vars[i].idx = i;

        // Выделяем память для текущего и начального значения
        if (!zbm_var_realloc_storage(&zbm_vars[i])) {
            ESP_LOGE(TAG, "Failed to alloc storage for var %d", i);
            continue;
        }

        // Обнуляем init_value
        memset(zbm_vars[i].p_init_value, 0, zbm_vars[i].data_size);
    }

    // Загружаем из SPIFFS
    cJSON* json = read_json_from_file(ZBM_RULES_VARS_FILE);
    if (json && cJSON_IsArray(json)) {
        ESP_LOGI(TAG, "Loading variables from %s", ZBM_RULES_VARS_FILE);
        for (int i = 0; i < cJSON_GetArraySize(json); i++) {
            cJSON* item = cJSON_GetArrayItem(json, i);
            int idx = cJSON_GetObjectItem(item, "idx")->valueint;
            if (idx < 0 || idx >= ZB_AUTO_VAR_COUNT) continue;

            zbm_virtual_var_t* var = &zbm_vars[idx];

            // === Обновляем имя ===
            cJSON* name_obj = cJSON_GetObjectItem(item, "name");
            if (name_obj && cJSON_IsString(name_obj)) {
                strncpy(var->name, name_obj->valuestring, sizeof(var->name) - 1);
                var->name[sizeof(var->name) - 1] = '\0';
            }

            // === Обновляем тип ===
            cJSON* type_obj = cJSON_GetObjectItem(item, "type");
            if (type_obj) {
                zbm_attr_data_types_t new_type = (zbm_attr_data_types_t)type_obj->valueint;
                if (new_type != var->data_type) {
                    var->data_type = new_type;
                    if (!zbm_var_realloc_storage(var)) {
                        ESP_LOGE(TAG, "Failed to realloc storage for var %d (type %d)", idx, new_type);
                        continue;
                    }
                }
            }

            // === Устанавливаем init_value ===
            cJSON* init_value_obj = cJSON_GetObjectItem(item, "init_value");
            if (init_value_obj) {
                if (cJSON_IsNumber(init_value_obj)) {
                    int num = init_value_obj->valueint;
                    zbm_var_set_number(var->p_init_value, var->data_type, num);
                } else if (cJSON_IsString(init_value_obj)) {
                    if (var->data_type == ZBM_ATTR_TYPE_CHAR_STRING || var->data_type == ZBM_ATTR_TYPE_LONG_CHAR_STRING) {
                        strlcpy((char*)var->p_init_value, init_value_obj->valuestring, var->data_size);
                    }
                }
            }

            // Применяем init_value к p_value (если не было иного значения)
            if (var->p_value && var->p_init_value) {
                memcpy(var->p_value, var->p_init_value, var->data_size);
            }

            var->last_update_ms = esp_log_timestamp();
        }
    }
    cJSON_Delete(json);
}

void zbm_var_set_number(void* buf, zbm_attr_data_types_t type, int num) {
    switch (type) {
        case ZBM_ATTR_TYPE_U8:
            *(uint8_t*)buf = (uint8_t)num;
            break;
        case ZBM_ATTR_TYPE_S8:
            *(int8_t*)buf = (int8_t)num;
            break;
        case ZBM_ATTR_TYPE_U16:
            *(uint16_t*)buf = (uint16_t)num;
            break;
        case ZBM_ATTR_TYPE_S16:
            *(int16_t*)buf = (int16_t)num;
            break;
        default:
            *(uint8_t*)buf = (uint8_t)num;
            break;
    }
}

void zbm_vars_save_to_storage(void) {
    cJSON* arr = cJSON_CreateArray();
    for (int i = 0; i < ZB_AUTO_VAR_COUNT; i++) {
        cJSON* var = cJSON_CreateObject();
        cJSON_AddNumberToObject(var, "idx", i);
        cJSON_AddStringToObject(var, "name", zbm_vars[i].name);
        cJSON_AddNumberToObject(var, "type", zbm_vars[i].data_type);

        // Сохраняем init_value
        switch (zbm_vars[i].data_type) {
            case ZBM_ATTR_TYPE_U8:
            case ZBM_ATTR_TYPE_BOOL:
                cJSON_AddNumberToObject(var, "init_value", *(uint8_t*)zbm_vars[i].p_init_value);
                break;
            case ZBM_ATTR_TYPE_S8:
                cJSON_AddNumberToObject(var, "init_value", *(int8_t*)zbm_vars[i].p_init_value);
                break;
            case ZBM_ATTR_TYPE_U16:
            case ZBM_ATTR_TYPE_S16:
                cJSON_AddNumberToObject(var, "init_value", *(uint16_t*)zbm_vars[i].p_init_value);
                break;
            case ZBM_ATTR_TYPE_CHAR_STRING:
            case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
                cJSON_AddStringToObject(var, "init_value", (char*)zbm_vars[i].p_init_value);
                break;
            default:
                cJSON_AddNumberToObject(var, "init_value", *(uint8_t*)zbm_vars[i].p_init_value);
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

// Новая утилита
bool zbm_var_update_data(void* dst, zbm_attr_data_types_t type, void* src, uint16_t src_size) {
    if (!dst || !src) return false;

    switch (type) {
        case ZBM_ATTR_TYPE_U8:
        case ZBM_ATTR_TYPE_S8:
        case ZBM_ATTR_TYPE_BOOL:
            if (src_size == 1) {
                memcpy(dst, src, 1);
            } else {
                int num = (src_size == 1) ? 
                          (*(uint8_t*)src) : 
                          (src_size == 2 ? *(uint16_t*)src : atoi((char*)src));
                zbm_var_set_number(dst, type, num);
            }
            break;

        case ZBM_ATTR_TYPE_U16:
        case ZBM_ATTR_TYPE_S16:
            if (src_size == 2) {
                memcpy(dst, src, 2);
            } else {
                int num = (src_size == 1) ? *(uint8_t*)src : atoi((char*)src);
                zbm_var_set_number(dst, type, num);
            }
            break;

        case ZBM_ATTR_TYPE_CHAR_STRING:
        case ZBM_ATTR_TYPE_LONG_CHAR_STRING: {
            size_t max_len = (type == ZBM_ATTR_TYPE_CHAR_STRING) ? 
                             ZB_AUTO_VAR_STR_LEN : ZB_AUTO_VAR_LONG_STR_LEN;
            size_t len = (src_size < max_len) ? src_size : max_len - 1;
            memcpy(dst, src, len);
            ((char*)dst)[len] = '\0';  // гарантируем терминацию
            break;
        }

        default:
            return false;
    }

    return true;
}

bool zbm_var_set_config(uint8_t idx, const char* name, zbm_attr_data_types_t type, void* runtime_value, uint16_t runtime_size, void* init_value, uint16_t init_size)
{
    if (idx >= ZB_AUTO_VAR_COUNT) {
        ESP_LOGE(TAG, "Invalid var index: %d", idx);
        return false;
    }

    zbm_virtual_var_t* var = &zbm_vars[idx];
    bool updated = false;

    // === 1. Обновление имени ===
    if (name && strlen(name) > 0 && strcmp(var->name, name) != 0) {
        strncpy(var->name, name, sizeof(var->name) - 1);
        var->name[sizeof(var->name) - 1] = '\0';
        updated = true;
        ESP_LOGI(TAG, "Var %d: name updated to '%s'", idx, name);
    }

    // === 2. Обновление типа ===
    bool type_changed = false;
    if (type != ZBM_ATTR_TYPE_INVALID && var->data_type != type) {
        // Освобождаем старые буферы
        if (var->p_value) {
            free(var->p_value);
            var->p_value = NULL;
        }
        if (var->p_init_value) {
            free(var->p_init_value);
            var->p_init_value = NULL;
        }
        var->data_size = 0;
        var->data_type = type;
        type_changed = true;
        updated = true;
        ESP_LOGI(TAG, "Var %d: type changed to %d", idx, type);
    }

    // === 3. Вычисляем требуемый размер ===
    uint16_t required_size = 0;
    switch (var->data_type) {
        case ZBM_ATTR_TYPE_U8:
        case ZBM_ATTR_TYPE_S8:
        case ZBM_ATTR_TYPE_BOOL:
            required_size = 1;
            break;
        case ZBM_ATTR_TYPE_U16:
        case ZBM_ATTR_TYPE_S16:
            required_size = 2;
            break;
        case ZBM_ATTR_TYPE_CHAR_STRING:
            required_size = ZB_AUTO_VAR_STR_LEN;
            break;
        case ZBM_ATTR_TYPE_LONG_CHAR_STRING:
            required_size = ZB_AUTO_VAR_LONG_STR_LEN;
            break;
        default:
            ESP_LOGE(TAG, "Unsupported data type: %d", var->data_type);
            return false;
    }

    // === 4. Выделяем память для p_value и p_init_value, если нужно ===
    if (!var->p_value || var->data_size != required_size) {
        void* new_ptr = calloc(1, required_size);
        if (!new_ptr) {
            ESP_LOGE(TAG, "Failed to allocate p_value for var %d", idx);
            return false;
        }
        free(var->p_value);
        var->p_value = new_ptr;
        updated = true;
    }

    if (!var->p_init_value || var->data_size != required_size) {
        void* new_init_ptr = calloc(1, required_size);
        if (!new_init_ptr) {
            ESP_LOGE(TAG, "Failed to allocate p_init_value for var %d", idx);
            return false;
        }
        free(var->p_init_value);
        var->p_init_value = new_init_ptr;
        updated = true;
    }

    // Сохраняем размер
    var->data_size = required_size;

    // === 5. Применяем runtime_value (если передано) ===
    if (runtime_value && runtime_size > 0) {
        if (zbm_var_update_data(var->p_value, var->data_type, runtime_value, runtime_size)) {
            var->last_update_ms = esp_log_timestamp();
            updated = true;
            ESP_LOGI(TAG, "Var %d: runtime value updated", idx);
        } else {
            ESP_LOGW(TAG, "Var %d: failed to update runtime value", idx);
        }
    }

    // === 6. Применяем init_value (если передано) ===
    if (init_value && init_size > 0) {
        if (zbm_var_update_data(var->p_init_value, var->data_type, init_value, init_size)) {
            updated = true;
            ESP_LOGI(TAG, "Var %d: init_value updated", idx);
        } else {
            ESP_LOGW(TAG, "Var %d: failed to update init_value", idx);
        }
    }

    // === 7. Если было обновление — сохраняем в SPIFFS ===
    if (updated) {
        zbm_vars_save_to_storage();
    }

    return updated;
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
    bool is_var_change = false;
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
            {
                zbm_var_set_uint8(act->data.set_uint8.var_idx, act->data.set_uint8.value);
                cJSON* notify = cJSON_CreateObject();
                cJSON_AddNumberToObject(notify, "idx", act->data.set_uint8.var_idx);
                zbm_ws_send_sys_notify("var_updated", "Variable updated", notify);
                cJSON_Delete(notify);
            break;
            }
        case ZB_ACTION_SET_VAR_INT8:
            {
                zbm_var_set_int8(act->data.set_int8.var_idx, act->data.set_int8.value);
                cJSON* notify = cJSON_CreateObject();
                cJSON_AddNumberToObject(notify, "idx", act->data.set_int8.var_idx);
                zbm_ws_send_sys_notify("var_updated", "Variable updated", notify);
                cJSON_Delete(notify);
                break;
            }
        case ZB_ACTION_SET_VAR_UINT16:
            {
                zbm_var_set_uint16(act->data.set_uint16.var_idx, act->data.set_uint16.value);
                cJSON* notify = cJSON_CreateObject();
                cJSON_AddNumberToObject(notify, "idx", act->data.set_uint16.var_idx);
                zbm_ws_send_sys_notify("var_updated", "Variable updated", notify);
                cJSON_Delete(notify);
                break;
            }
        case ZB_ACTION_SET_VAR_STRING:
            {
                zbm_var_set_string(act->data.set_str.var_idx, act->data.set_str.str);
                cJSON* notify = cJSON_CreateObject();
                cJSON_AddNumberToObject(notify, "idx", act->data.set_str.var_idx);
                zbm_ws_send_sys_notify("var_updated", "Variable updated", notify);
                cJSON_Delete(notify);
                break;
            }
        case ZB_ACTION_INC_VAR:
            {
                uint8_t v = zbm_var_get_uint8(act->data.inc.var_idx);
                zbm_var_set_uint8(act->data.inc.var_idx, v + 1);
                cJSON* notify = cJSON_CreateObject();
                cJSON_AddNumberToObject(notify, "idx", act->data.inc.var_idx);
                zbm_ws_send_sys_notify("var_updated", "Variable updated", notify);
                cJSON_Delete(notify);
                break;
            }
        case ZB_ACTION_DEC_VAR: 
            {
                uint8_t v = zbm_var_get_uint8(act->data.dec.var_idx);
                zbm_var_set_uint8(act->data.dec.var_idx, v > 0 ? v - 1 : 0);
                cJSON* notify = cJSON_CreateObject();
                cJSON_AddNumberToObject(notify, "idx", act->data.dec.var_idx);
                zbm_ws_send_sys_notify("var_updated", "Variable updated", notify);
                cJSON_Delete(notify);
                break;
            }
        case ZB_ACTION_TOGGLE_VAR: 
            {
                uint8_t v = zbm_var_get_uint8(act->data.toggle.var_idx);
                zbm_var_set_uint8(act->data.toggle.var_idx, !v);
                cJSON* notify = cJSON_CreateObject();
                cJSON_AddNumberToObject(notify, "idx", act->data.toggle.var_idx);
                zbm_ws_send_sys_notify("var_updated", "Variable updated", notify);
                cJSON_Delete(notify);
                break;
            }
        default:
            ESP_LOGW(TAG, "Unknown action type: %d", act->type);
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
        }else if (data_type == ZBM_ATTR_TYPE_S16) {
            ESP_LOGI(TAG, "Value (S16): %d", *(const int16_t*)value);
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

        // 🔴 Сначала проверяем время
        if (!is_time_in_range(&rule->time_range)) {
            ESP_LOGD(TAG, "Rule '%s' skipped: not in time range", rule->name);
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
//                ИНИЦИАЛИЗАЦИЯ
// ========================================================

void zb_automation_v2_init(void) {
    
    zb_rules_count = 0;

    // Загружаем правила
    zb_rules_load_all_from_storage();
    
    // загрузка переменных и первая инициализация
    zbm_var_init();
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

