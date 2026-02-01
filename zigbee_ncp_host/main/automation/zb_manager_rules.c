// main/rules/zb_manager_rules.c
#include "zb_manager_rules.h"
#include "esp_log.h"
#include "zb_manager_automation.h"
#include "cJSON.h"
#include <string.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include "spiffs_helper.h"

static const char* TAG = "ZB_RULE";

#define RULES_NVS_NAMESPACE "rules"
#define RULES_JSON_KEY      "rules_list"

//zb_rule_t rules[ZB_RULE_MAX_COUNT];
uint8_t rules_count = 0;
zb_rule_t** rules_array = NULL;
// Внутренние функции
static bool rule_matches_trigger(const zb_rule_t* rule, cJSON* event);
static void execute_rule_actions(const zb_rule_t* rule);
static void rules_task(void* pvParameters);

// ===================================================================
//                         Инициализация
// ===================================================================

void zb_rule_engine_init(void)
{
    // Инициализируем массив
    rules_array = calloc(ZB_RULE_MAX_COUNT, sizeof(zb_rule_t*));
    if (!rules_array) {
        ESP_LOGE(TAG, "Failed to allocate rules array");
        return;
    }

    // Загружаем из SPIFFS
    esp_err_t err = zb_rule_engine_load_from_spiffs();
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No rules file found, starting with empty rule set");
        rules_count = 0;
    }

    xTaskCreate(rules_task, "zb_rules_task", 4096, NULL, 5, NULL);
}

/**
 * @brief Проверить, срабатывает ли правило на событие
 */
static bool rule_matches_trigger(const zb_rule_t* rule, cJSON* event) {
    // Пример: событие {"event":"state_update","short":12345,"clusters":[{"type":"illuminance","value":40}]}
    cJSON* short_addr_obj = cJSON_GetObjectItem(event, "short");
    if (!short_addr_obj) return false;
    uint16_t short_addr = short_addr_obj->valueint;

    cJSON* clusters = cJSON_GetObjectItem(event, "clusters");
    if (!clusters) return false;

    for (int i = 0; i < rule->trigger_count; i++) {
        const zb_rule_trigger_t* t = &rule->triggers[i];
        if (t->type != ZB_RULE_TRIGGER_DEVICE_STATE) continue;

        if (t->data.device_state.short_addr != short_addr) continue;

        cJSON* cl_item = NULL;
        cJSON_ArrayForEach(cl_item, clusters) {
            cJSON* type = cJSON_GetObjectItem(cl_item, "type");
            cJSON* value = cJSON_GetObjectItem(cl_item, "value");
            if (!type || !value) continue;

            if (strcmp(type->valuestring, t->data.device_state.cluster_type) == 0) {
                double val = value->valuedouble;
                switch (t->data.device_state.cond) {
                    case ZB_RULE_COND_EQ: return val == t->data.device_state.value;
                    case ZB_RULE_COND_NE: return val != t->data.device_state.value;
                    case ZB_RULE_COND_GT: return val > t->data.device_state.value;
                    case ZB_RULE_COND_LT: return val < t->data.device_state.value;
                    case ZB_RULE_COND_GTE: return val >= t->data.device_state.value;
                    case ZB_RULE_COND_LTE: return val <= t->data.device_state.value;
                }
            }
        }
    }
    return false;
}

/**
 * @brief Выполнить действия правила
 */
static void execute_rule_actions(const zb_rule_t* rule) {
    for (int i = 0; i < rule->action_count; i++) {
        const zb_rule_action_t* a = &rule->actions[i];
        if (a->type == ZB_RULE_ACTION_DEVICE_CMD) {
            zb_automation_request_t req = {0};
            req.short_addr = a->data.device_cmd.short_addr;
            req.endpoint_id = a->data.device_cmd.endpoint;
            req.cmd_id = a->data.device_cmd.cmd_id;
            zb_automation_send_command(&req);
            ESP_LOGI(TAG, "✅ Executed action: device_cmd %04x ep=%d cmd=%d", req.short_addr, req.endpoint_id, req.cmd_id);
        }
    }
}

/**
 * @brief Задача для фоновой обработки (если нужна очередь)
 */
static void rules_task(void* pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Проверить все правила на событие
 * Вызывается из zb_manager_devices при изменении состояния
 */
void zb_rule_engine_process_event(cJSON* event) {
    if (!event) return;

    int highest_priority = 6;
    const zb_rule_t* winner = NULL;

    for (int i = 0; i < rules_count; i++) {
        zb_rule_t* rule = rules_array[i];
        if (!rule) continue;

        if (!rule->enabled) continue;
        if (rule_matches_trigger(rule, event)) {  // ✅ Передаём rule, а не &rule
            if (rule->priority < highest_priority) {
                highest_priority = rule->priority;
                winner = rule;  // ✅ Присваиваем rule, а не &rule
            }
        }
    }

    if (winner) {
        execute_rule_actions(winner);
    }
}


// ===================================================================
//                      Работа с SPIFFS
// ===================================================================

esp_err_t zb_rule_engine_load_from_spiffs(void)
{
     FILE* f = fopen(ZB_MANAGER_RULES_JSON_FILE, "r");
    if (!f) return ESP_ERR_NOT_FOUND;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    ESP_LOGI(TAG, "📂 Найден файл %s, размер: %ld байт", ZB_MANAGER_RULES_JSON_FILE, len);

    char* json_str = malloc(len + 1);
    if (!json_str) { 
        fclose(f); 
        ESP_LOGE(TAG, "❌ Не удалось выделить память под JSON");
        return ESP_ERR_NO_MEM; 
    }
    fread(json_str, 1, len, f);
    json_str[len] = '\0';
    fclose(f);

    ESP_LOGI(TAG, "📥 JSON содержимое: %s", json_str); //

    cJSON* root = cJSON_Parse(json_str);
    free(json_str);
    if (!root){
        ESP_LOGE(TAG, "❌ Ошибка парсинга JSON");
        return ESP_ERR_INVALID_ARG;
    } 

    int count = cJSON_GetArraySize(root);
    rules_count = 0;

    ESP_LOGI(TAG, "🔍 Парсим %d правил из JSON", count);

    for (int i = 0; i < count && i < ZB_RULE_MAX_COUNT; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        zb_rule_t* rule = calloc(1, sizeof(zb_rule_t));  // ← выделяем память
        if (!rule) continue;

        // Парсим поля...
        cJSON* id = cJSON_GetObjectItem(item, "id");
        if (!id) { free(rule); continue; }
        strncpy(rule->id, id->valuestring, 31);

        cJSON* name = cJSON_GetObjectItem(item, "name");
        strncpy(rule->name, name ? name->valuestring : "Unnamed", 63);

        cJSON* module = cJSON_GetObjectItem(item, "module");
        strncpy(rule->module, module ? module->valuestring : "other", 31);

        rule->priority = cJSON_GetObjectItem(item, "priority")->valueint;
        rule->enabled = cJSON_GetObjectItem(item, "enabled") ? cJSON_IsTrue(cJSON_GetObjectItem(item, "enabled")) : true;

        // === Триггеры ===
        cJSON* triggers = cJSON_GetObjectItem(item, "triggers");
        rule->trigger_count = 0;
        cJSON* trig_item = NULL;
        cJSON_ArrayForEach(trig_item, triggers) {
            if (rule->trigger_count >= 8) break;
            zb_rule_trigger_t* t = &rule->triggers[rule->trigger_count];

            cJSON* type = cJSON_GetObjectItem(trig_item, "type");
            if (!type) continue;

            if (strcmp(type->valuestring, "device_state") == 0) {
                t->type = ZB_RULE_TRIGGER_DEVICE_STATE;
                cJSON* short_addr = cJSON_GetObjectItem(trig_item, "short");
                cJSON* ep_id = cJSON_GetObjectItem(trig_item, "endpoint_id");
                cJSON* cl_type = cJSON_GetObjectItem(trig_item, "cluster_type");
                cJSON* cond = cJSON_GetObjectItem(trig_item, "condition");
                cJSON* value = cJSON_GetObjectItem(trig_item, "value");

                t->data.device_state.short_addr = short_addr ? short_addr->valueint : 0;
                t->data.device_state.endpoint_id = ep_id ? ep_id->valueint : 1;
                if (cl_type) strncpy(t->data.device_state.cluster_type, cl_type->valuestring, 31);
                if (cond) {
                    if (strcmp(cond->valuestring, "eq") == 0) t->data.device_state.cond = ZB_RULE_COND_EQ;
                    else if (strcmp(cond->valuestring, "ne") == 0) t->data.device_state.cond = ZB_RULE_COND_NE;
                    else if (strcmp(cond->valuestring, "gt") == 0) t->data.device_state.cond = ZB_RULE_COND_GT;
                    else if (strcmp(cond->valuestring, "lt") == 0) t->data.device_state.cond = ZB_RULE_COND_LT;
                    else if (strcmp(cond->valuestring, "gte") == 0) t->data.device_state.cond = ZB_RULE_COND_GTE;
                    else if (strcmp(cond->valuestring, "lte") == 0) t->data.device_state.cond = ZB_RULE_COND_LTE;
                }
                t->data.device_state.value = value ? value->valuedouble : 0;
            } else if (strcmp(type->valuestring, "time_range") == 0) {
                t->type = ZB_RULE_TRIGGER_TIME_RANGE;
                cJSON* from = cJSON_GetObjectItem(trig_item, "from");
                cJSON* to = cJSON_GetObjectItem(trig_item, "to");
                if (from) strncpy(t->data.time_range.from, from->valuestring, 5);
                if (to) strncpy(t->data.time_range.to, to->valuestring, 5);
            }
            rule->trigger_count++;
        }

        // === Действия ===
        cJSON* actions = cJSON_GetObjectItem(item, "actions");
        rule->action_count = 0;
        cJSON* act_item = NULL;
        cJSON_ArrayForEach(act_item, actions) {
            if (rule->action_count >= 4) break;
            zb_rule_action_t* a = &rule->actions[rule->action_count];

            cJSON* act_type = cJSON_GetObjectItem(act_item, "type");
            if (!act_type) continue;

            if (strcmp(act_type->valuestring, "device_command") == 0) {
                a->type = ZB_RULE_ACTION_DEVICE_CMD;
                cJSON* short_addr = cJSON_GetObjectItem(act_item, "short");
                cJSON* ep = cJSON_GetObjectItem(act_item, "endpoint");
                cJSON* cmd_id = cJSON_GetObjectItem(act_item, "cmd_id");

                a->data.device_cmd.short_addr = short_addr ? short_addr->valueint : 0;
                a->data.device_cmd.endpoint = ep ? ep->valueint : 1;
                a->data.device_cmd.cmd_id = cmd_id ? cmd_id->valueint : 0;
            }
            rule->action_count++;
        }

        //rules_count++;
        // ✅ ДОБАВЛЯЕМ ПРАВИЛО В МАССИВ
        if (rules_count >= ZB_RULE_MAX_COUNT) {
            ESP_LOGE(TAG, "❌ Достигнут лимит правил: %d", ZB_RULE_MAX_COUNT);
            free(rule);
        } else {
            rules_array[rules_count] = rule;
            ESP_LOGI(TAG, "✅ Загружено правило '%s' на индекс %d", rule->name, rules_count);
            rules_count++;
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %d rules from SPIFFS", rules_count);
    return ESP_OK;
}

esp_err_t zb_rule_engine_save_to_spiffs(void)
{
    FILE* f = fopen(ZB_MANAGER_RULES_JSON_FILE, "w");
    if (!f) {
        ESP_LOGE("RULE_SAVE", "❌ Не удалось открыть %s для записи", ZB_MANAGER_RULES_JSON_FILE);
        return ESP_FAIL;
    }

    cJSON* root = cJSON_CreateArray();
    if (!root) {
        fclose(f);
        ESP_LOGE("RULE_SAVE", "❌ Не удалось создать JSON массив");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI("RULE_SAVE", "💾 Сохранение %d правил", rules_count);

    for (int i = 0; i < rules_count; i++) {
        if (rules_array[i] == NULL) {
            ESP_LOGW("RULE_SAVE", "⚠️ Пропуск NULL правила на индексе %d", i);
            continue;
        }
        cJSON* item = rule_to_json(rules_array[i]);
        if (!item) {
            ESP_LOGE("RULE_SAVE", "❌ rule_to_json вернул NULL для правила %s", rules_array[i]->id);
            continue;
        }
        cJSON_AddItemToArray(root, item);
    }

    char* json_str = cJSON_PrintUnformatted(root);
    if (!json_str) {
        fclose(f);
        cJSON_Delete(root);
        ESP_LOGE("RULE_SAVE", "❌ cJSON_PrintUnformatted вернул NULL");
        return ESP_ERR_NO_MEM;
    }

    // Выведем длину строки для контроля
    ESP_LOGI("RULE_SAVE", "📄 JSON строка: %d байт", strlen(json_str));

    fprintf(f, "%s", json_str);
    free(json_str);
    fclose(f);
    cJSON_Delete(root);

    ESP_LOGI("RULE_SAVE", "✅ Успешно сохранено в %s", ZB_MANAGER_RULES_JSON_FILE);
    return ESP_OK;
}



bool zb_rule_engine_add_rule(const zb_rule_t* rule_template)
{
    if (!rule_template) {
        ESP_LOGE(TAG, "❌ rule_template is NULL");
        return false;
    }

    if (rules_count >= ZB_RULE_MAX_COUNT) {
        ESP_LOGW(TAG, "❌ Достигнут лимит правил: %d", ZB_RULE_MAX_COUNT);
        return false;
    }

    zb_rule_t* new_rule = calloc(1, sizeof(zb_rule_t));
    if (!new_rule) {
        ESP_LOGE(TAG, "❌ Не удалось выделить память под новое правило");
        return false;
    }

    // Копируем поля по одному, безопасно
    memcpy(new_rule, rule_template, sizeof(zb_rule_t));

    // Генерируем ID, если не задан
    if (strlen(new_rule->id) == 0) {
        snprintf(new_rule->id, sizeof(new_rule->id), "rule_%04x", 
                 (unsigned int)(esp_log_timestamp() & 0xFFFF));
        ESP_LOGI(TAG, "🆔 Сгенерирован ID: %s", new_rule->id);
    }

    // Сохраняем
    rules_array[rules_count] = new_rule;
    ESP_LOGI(TAG, "✅ Добавлено правило '%s' на индекс %d", new_rule->name, rules_count);
    rules_count++;

    // Сохраняем на диск и уведомляем
    esp_err_t save_err = zb_rule_engine_save_to_spiffs();
    if (save_err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Ошибка сохранения в SPIFFS: %s", esp_err_to_name(save_err));
        // Но правило уже в памяти — можно попробовать ещё раз
    } else {
        ESP_LOGI(TAG, "💾 Правило сохранено в %s", ZB_MANAGER_RULES_JSON_FILE);
    }

    ws_notify_rules_update();

    return true;
}



bool zb_rule_engine_update_rule(const char* rule_id, const zb_rule_t* updated_rule)
{
    // Проверки входных данных
    if (!rule_id || strlen(rule_id) == 0) {
        ESP_LOGE(TAG, "❌ rule_id is NULL or empty");
        return false;
    }

    if (!updated_rule) {
        ESP_LOGE(TAG, "❌ updated_rule is NULL");
        return false;
    }

    if (!rules_array) {
        ESP_LOGE(TAG, "❌ rules_array not initialized");
        return false;
    }

    // Поиск правила по ID
    for (int i = 0; i < rules_count; i++) {
        if (!rules_array[i]) {
            ESP_LOGW(TAG, "⚠️ rules_array[%d] is NULL", i);
            continue;
        }

        if (strcmp(rules_array[i]->id, rule_id) == 0) {
            ESP_LOGI(TAG, "🔄 Обновляем правило: %s (index=%d)", rule_id, i);

            // Копируем всё содержимое
            memcpy(rules_array[i], updated_rule, sizeof(zb_rule_t));

            // Гарантируем, что ID совпадает (на случай, если изменился)
            strncpy(rules_array[i]->id, rule_id, sizeof(rules_array[i]->id) - 1);
            rules_array[i]->id[sizeof(rules_array[i]->id) - 1] = '\0';

            // Сохраняем на диск
            esp_err_t save_err = zb_rule_engine_save_to_spiffs();
            if (save_err != ESP_OK) {
                ESP_LOGE(TAG, "❌ Ошибка при сохранении после обновления: %s", esp_err_to_name(save_err));
                return false;
            }

            ESP_LOGI(TAG, "✅ Правило '%s' успешно обновлено", rule_id);
            ws_notify_rules_update();

            return true;
        }
    }

    ESP_LOGW(TAG, "❌ Правило с ID '%s' не найдено", rule_id);
    return false;
}



bool zb_rule_engine_remove_rule(const char* rule_id)
{
    for (int i = 0; i < rules_count; i++) {
        if (strcmp(rules_array[i]->id, rule_id) == 0) {
            free(rules_array[i]);  // ← освобождаем память

            // Сдвигаем хвост
            memmove(&rules_array[i], &rules_array[i+1],
                    (rules_count - i - 1) * sizeof(zb_rule_t*));
            rules_count--;

            zb_rule_engine_save_to_spiffs();
            ws_notify_rules_update();
            return true;
        }
    }
    return false;
}

// Удалить все
bool zb_rule_engine_remove_all_rules(void)
{
    for (int i = 0; i < rules_count; i++) {
        free(rules_array[i]);
    }
    rules_count = 0;
    zb_rule_engine_save_to_spiffs();
    ws_notify_rules_update();
    return true;
}


const zb_rule_t* zb_rule_engine_get_rule(const char* rule_id)
{
    for (int i = 0; i < rules_count; i++) {
        if (strcmp(rules_array[i]->id, rule_id) == 0) {
            return rules_array[i];
        }
    }
    return NULL;
}


bool zb_automation_run_rule_now(const char* rule_id)
{
    const zb_rule_t* rule = zb_rule_engine_get_rule(rule_id);
    if (!rule || !rule->enabled) {
        ESP_LOGW("RULE_RUN", "Правило не найдено или выключено: %s", rule_id);
        return false;
    }

    ESP_LOGI("RULE_RUN", "✅ Ручной запуск: %s", rule->name);
    execute_rule_actions(rule); // вызываем действия
    return true;
}

// Вспомогательная: сериализация правила в JSON
cJSON* rule_to_json(const zb_rule_t* rule) {
    if (!rule) {
        ESP_LOGE("RULE_JSON", "❌ rule == NULL");
        return cJSON_CreateObject();
    }
    ESP_LOGI("RULE_JSON", "🔄 Сериализация правила: %s (id=%s)", rule->name, rule->id);

    cJSON* r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "id", rule->id);
    cJSON_AddStringToObject(r, "name", rule->name);
    cJSON_AddStringToObject(r, "module", rule->module);
    cJSON_AddNumberToObject(r, "priority", rule->priority);
    cJSON_AddBoolToObject(r, "enabled", rule->enabled);

    // Триггеры
    cJSON* triggers = cJSON_CreateArray();
    for (int j = 0; j < rule->trigger_count; j++) {
        const zb_rule_trigger_t* t = &rule->triggers[j];
        cJSON* trig = cJSON_CreateObject();

        ESP_LOGI("RULE_JSON", "  ➡️ Триггер %d: type=%d", j, t->type);  // ← Лог типа

        switch (t->type) {
            case ZB_RULE_TRIGGER_DEVICE_STATE:
                cJSON_AddStringToObject(trig, "type", "device_state");
                cJSON_AddNumberToObject(trig, "short", t->data.device_state.short_addr);
                cJSON_AddNumberToObject(trig, "endpoint_id", t->data.device_state.endpoint_id);
                cJSON_AddStringToObject(trig, "cluster_type", t->data.device_state.cluster_type);
                const char* cond_str = "eq";
                switch (t->data.device_state.cond) {
                    case ZB_RULE_COND_EQ: cond_str = "eq"; break;
                    case ZB_RULE_COND_NE: cond_str = "ne"; break;
                    case ZB_RULE_COND_GT: cond_str = "gt"; break;
                    case ZB_RULE_COND_LT: cond_str = "lt"; break;
                    case ZB_RULE_COND_GTE: cond_str = "gte"; break;
                    case ZB_RULE_COND_LTE: cond_str = "lte"; break;
                }
                cJSON_AddStringToObject(trig, "condition", cond_str);
                cJSON_AddNumberToObject(trig, "value", t->data.device_state.value);
                break;

            default:
                ESP_LOGW("RULE_JSON", "⚠️ Неизвестный тип триггера: %d", t->type);
                cJSON_AddStringToObject(trig, "type", "unknown");
                break;
        }
        cJSON_AddItemToArray(triggers, trig);
    }
    cJSON_AddItemToObject(r, "triggers", triggers);

    // Действия
    cJSON* actions = cJSON_CreateArray();
    for (int j = 0; j < rule->action_count; j++) {
        const zb_rule_action_t* a = &rule->actions[j];
        cJSON* act = cJSON_CreateObject();
        switch (a->type) {
            case ZB_RULE_ACTION_DEVICE_CMD:
                cJSON_AddStringToObject(act, "type", "device_command");
                cJSON_AddNumberToObject(act, "short", a->data.device_cmd.short_addr);
                cJSON_AddNumberToObject(act, "endpoint", a->data.device_cmd.endpoint);
                cJSON_AddNumberToObject(act, "cmd_id", a->data.device_cmd.cmd_id);
                break;

            default:
                cJSON_AddStringToObject(act, "type", "unknown");
                break;
        }
        cJSON_AddItemToArray(actions, act);
    }
    cJSON_AddItemToObject(r, "actions", actions);

    return r;
}

void zb_rule_trigger_state_update_double(uint16_t short_addr, const char* cluster_type, double value)
{
    ESP_LOGI(TAG, "🔄 zb_rule_trigger_state_update_double: %d, %s, %f", short_addr, cluster_type, value);
    cJSON* event = cJSON_CreateObject();
    if (!event) return;

    cJSON_AddStringToObject(event, "event", "state_update");
    cJSON_AddNumberToObject(event, "short", short_addr);

    cJSON* clusters = cJSON_CreateArray();
    cJSON* cl = cJSON_CreateObject();
    cJSON_AddStringToObject(cl, "type", cluster_type);
    cJSON_AddNumberToObject(cl, "value", value);
    cJSON_AddItemToArray(clusters, cl);
    cJSON_AddItemToObject(event, "clusters", clusters);

    // Передаём событие в движок правил
    zb_rule_engine_process_event(event);

    // Очищаем память
    cJSON_Delete(event);
}

void zb_rule_trigger_state_update(
    uint16_t short_addr,
    esp_zb_zcl_cluster_id_t cluster_id,
    uint16_t attr_id,
    void* data,
    uint8_t data_len,
    esp_zb_zcl_attr_type_t attr_type)
{
    if (!data || data_len == 0) {
        ESP_LOGW(TAG, "No data to trigger rule");
        return;
    }

    // Определяем строковый тип кластера для JSON
    const char* cluster_type_str = NULL;
    bool is_state_attr = false; // Только атрибуты состояния (не команды)

    // Сначала проверим, интересует ли нас этот атрибут
    if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF && attr_id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
        cluster_type_str = "on_off";
        is_state_attr = true;
    }
    else if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT && attr_id == ATTR_TEMP_MEASUREMENT_VALUE_ID) {
        cluster_type_str = "temperature";
        is_state_attr = true;
    }
    else if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT && attr_id == ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID) {
        cluster_type_str = "humidity";
        is_state_attr = true;
    }
    /*else if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT && attr_id == ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASURED_VALUE_ID) {
        cluster_type_str = "illuminance";
        is_state_attr = true;
    }*/
    else if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG && attr_id == ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID) {
        cluster_type_str = "battery";
        is_state_attr = true;
    }
    // Добавь другие при необходимости: occupancy, voltage и т.д.

    if (!is_state_attr) {
        ESP_LOGD(TAG, "Attribute not tracked for rules: cluster=0x%04x, attr=0x%04x", cluster_id, attr_id);
        return;
    }

    // Теперь извлекаем значение в double (универсальный формат для сравнения)
    double numeric_value = 0.0;
    bool parsed = false;

    if (attr_type == ESP_ZB_ZCL_ATTR_TYPE_BOOL && data_len >= 1) {
        numeric_value = *(uint8_t*)data ? 1.0 : 0.0;
        parsed = true;
    }
    else if (attr_type == ESP_ZB_ZCL_ATTR_TYPE_U8 && data_len >= 1) {
        numeric_value = *(uint8_t*)data;
        parsed = true;
    }
    else if (attr_type == ESP_ZB_ZCL_ATTR_TYPE_S8 && data_len >= 1) {
        numeric_value = *(int8_t*)data;
        parsed = true;
    }
    else if (attr_type == ESP_ZB_ZCL_ATTR_TYPE_U16 && data_len >= 2) {
        numeric_value = *(uint16_t*)data;
        parsed = true;
    }
    else if (attr_type == ESP_ZB_ZCL_ATTR_TYPE_S16 && data_len >= 2) {
        numeric_value = *(int16_t*)data;
        parsed = true;
    }
    else if (attr_type == ESP_ZB_ZCL_ATTR_TYPE_SINGLE && data_len >= 4) {
        numeric_value = (double)*(float*)data;
        parsed = true;
    }
    // Можно добавить другие типы по мере необходимости

    if (!parsed) {
        ESP_LOGW(TAG, "Unsupported attr type 0x%02x or invalid length %d", attr_type, data_len);
        return;
    }

    // Для некоторых значений — масштабирование
    if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT) {
        numeric_value /= 100.0;  // Convert from centi-degrees
    }
    else if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG && attr_id == ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID) {
        numeric_value /= 2.0;  // 0..200 → 0..100%
    }

    ESP_LOGI(TAG, "🔄 Rule trigger: %s = %.3f (dev=0x%04x)", cluster_type_str, numeric_value, short_addr);

    // Формируем JSON событие
    cJSON* event = cJSON_CreateObject();
    if (!event) return;

    cJSON_AddStringToObject(event, "event", "state_update");
    cJSON_AddNumberToObject(event, "short", short_addr);

    cJSON* clusters = cJSON_CreateArray();
    cJSON* cl = cJSON_CreateObject();
    cJSON_AddStringToObject(cl, "type", cluster_type_str);
    cJSON_AddNumberToObject(cl, "value", numeric_value);
    cJSON_AddItemToArray(clusters, cl);
    cJSON_AddItemToObject(event, "clusters", clusters);

    // Отправляем в движок
    zb_rule_engine_process_event(event);

    // Очищаем
    cJSON_Delete(event);
}
