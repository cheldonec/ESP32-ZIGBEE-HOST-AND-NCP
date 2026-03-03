// zb_manager_rule_editor_api.c

#include "zb_manager_rule_editor_api.h"
#include "esp_log.h"
#include "cJSON.h"
#include "zb_manager_rules.h"
#include "string.h"

static const char* TAG = "RULE_EDITOR_API";



// === GET /api/rules ===
// получить все правила
esp_err_t api_get_rules_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "HTTP_GET /api/rules api_get_rules_handler");
    ESP_LOGI(TAG, "📊 rules_count = %d", rules_count);

    cJSON* rules_json = cJSON_CreateArray();
    if (!rules_json) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON alloc failed");
        return ESP_FAIL;
    }

    int added = 0;
    for (int i = 0; i < rules_count; i++) {
        if (!rules_array[i]) {
            ESP_LOGW(TAG, "⚠️ rules_array[%d] == NULL", i);
            continue;
        }
        cJSON* r = rule_to_json(rules_array[i]);
        if (!r) {
            ESP_LOGE(TAG, "❌ rule_to_json вернул NULL для правила на индексе %d", i);
            continue;
        }
        cJSON_AddItemToArray(rules_json, r);
        added++;
        ESP_LOGI(TAG, "✅ Добавлено правило: %s", rules_array[i]->name);
    }

    ESP_LOGI(TAG, "📤 Отправляем %d правил клиенту", added);

    char* rendered = cJSON_PrintUnformatted(rules_json);
    cJSON_Delete(rules_json);

    if (!rendered) {
        ESP_LOGE(TAG, "❌ cJSON_PrintUnformatted вернул NULL");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON print failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, rendered);
    free(rendered);
    return ESP_OK;
}


// === POST /api/rules ===
esp_err_t api_post_rule_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "HTTP_POST /api/rules api_post_rule_handler");
    char buf[2048];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0 || ret >= 2048) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON* json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    zb_rule_t new_rule = {0};
    cJSON* id = cJSON_GetObjectItem(json, "id");
    cJSON* name = cJSON_GetObjectItem(json, "name");
    cJSON* module = cJSON_GetObjectItem(json, "module");
    cJSON* priority = cJSON_GetObjectItem(json, "priority");
    cJSON* enabled = cJSON_GetObjectItem(json, "enabled");

    if (!id || !name || !module || !priority) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing required fields");
        return ESP_FAIL;
    }

    strncpy(new_rule.id, id->valuestring, sizeof(new_rule.id) - 1);
    strncpy(new_rule.name, name->valuestring, sizeof(new_rule.name) - 1);
    strncpy(new_rule.module, module->valuestring, sizeof(new_rule.module) - 1);
    new_rule.priority = priority->valueint;
    new_rule.enabled = enabled ? enabled->valueint : true;

    // === Триггеры ===
    cJSON* triggers = cJSON_GetObjectItem(json, "triggers");
    cJSON* trig_item = NULL;
    cJSON_ArrayForEach(trig_item, triggers) {
        if (new_rule.trigger_count >= 8) break;
        zb_rule_trigger_t* t = &new_rule.triggers[new_rule.trigger_count];

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
        }
        else if (strcmp(type->valuestring, "time_range") == 0) {
            t->type = ZB_RULE_TRIGGER_TIME_RANGE;
            cJSON* from = cJSON_GetObjectItem(trig_item, "from");
            cJSON* to = cJSON_GetObjectItem(trig_item, "to");
            cJSON* days = cJSON_GetObjectItem(trig_item, "days_of_week");
            cJSON* delay = cJSON_GetObjectItem(trig_item, "delay_sec");

            if (from) strncpy(t->data.time_range.from, from->valuestring, 5);
            if (to) strncpy(t->data.time_range.to, to->valuestring, 5);
            t->data.time_range.from[5] = '\0';
            t->data.time_range.to[5] = '\0';

            t->data.time_range.days_of_week = days ? days->valueint : 0xFF; // все дни
            t->data.time_range.delay_sec = delay ? delay->valueint : 0;
        }
        new_rule.trigger_count++;
    }

    // === Действия ===
    cJSON* actions = cJSON_GetObjectItem(json, "actions");
    cJSON* act_item = NULL;
    cJSON_ArrayForEach(act_item, actions) {
        if (new_rule.action_count >= 4) break;
        zb_rule_action_t* a = &new_rule.actions[new_rule.action_count];

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
        new_rule.action_count++;
    }

    cJSON_Delete(json);

    bool success = zb_rule_engine_add_rule(&new_rule);
    if (success) {
        httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    } else {
        httpd_resp_sendstr(req, "{\"status\":\"fail\", \"reason\": \"limit_reached\"}");
    }
    return ESP_OK;
}

// === PUT /api/rules ===
// === PUT /api/rules ===
esp_err_t api_put_rule_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "HTTP_PUT /api/rules api_put_rule_handler");
    char buf[2048];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Read failed");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON* json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON* id_json = cJSON_GetObjectItem(json, "id");
    if (!id_json || !id_json->valuestring) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No ID provided");
        return ESP_FAIL;
    }
    const char* rule_id = id_json->valuestring;

    // 🔎 Найти правило по ID
    int index = -1;
    for (int i = 0; i < rules_count; i++) {
        if (strcmp(rules_array[i]->id, rule_id) == 0) { 
            index = i;
            break;
        }
    }
    if (index == -1) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Rule not found");
        return ESP_FAIL;
    }

    // 🧱 Заполнить new_rule из JSON (копия логики из POST)
    zb_rule_t new_rule = {0};

    // Обязательные поля
    cJSON* name = cJSON_GetObjectItem(json, "name");
    cJSON* module = cJSON_GetObjectItem(json, "module");
    cJSON* priority = cJSON_GetObjectItem(json, "priority");
    cJSON* enabled = cJSON_GetObjectItem(json, "enabled");

    if (!name || !module || !priority) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing required fields");
        return ESP_FAIL;
    }

    strncpy(new_rule.id, rule_id, sizeof(new_rule.id) - 1);
    strncpy(new_rule.name, name->valuestring, sizeof(new_rule.name) - 1);
    strncpy(new_rule.module, module->valuestring, sizeof(new_rule.module) - 1);
    new_rule.priority = priority->valueint;
    new_rule.enabled = enabled ? enabled->valueint : true;

    // === Триггеры ===
    cJSON* triggers = cJSON_GetObjectItem(json, "triggers");
    if (triggers && cJSON_IsArray(triggers)) {
        cJSON* trig_item = NULL;
        cJSON_ArrayForEach(trig_item, triggers) {
            if (new_rule.trigger_count >= ZB_RULE_MAX_TRIGGERS) break;
            zb_rule_trigger_t* t = &new_rule.triggers[new_rule.trigger_count];

            cJSON* type = cJSON_GetObjectItem(trig_item, "type");
            if (!type || !cJSON_IsString(type)) continue;

            if (strcmp(type->valuestring, "device_state") == 0) {
                t->type = ZB_RULE_TRIGGER_DEVICE_STATE;
                cJSON* short_addr = cJSON_GetObjectItem(trig_item, "short");
                cJSON* ep_id = cJSON_GetObjectItem(trig_item, "endpoint_id");
                cJSON* cl_type = cJSON_GetObjectItem(trig_item, "cluster_type");
                cJSON* cond = cJSON_GetObjectItem(trig_item, "condition");
                cJSON* value = cJSON_GetObjectItem(trig_item, "value");

                t->data.device_state.short_addr = short_addr ? short_addr->valueint : 0;
                t->data.device_state.endpoint_id = ep_id ? ep_id->valueint : 1;
                if (cl_type) strncpy(t->data.device_state.cluster_type, cl_type->valuestring, sizeof(t->data.device_state.cluster_type) - 1);
                t->data.device_state.cluster_type[sizeof(t->data.device_state.cluster_type) - 1] = '\0';

                if (cond && cJSON_IsString(cond)) {
                    if (strcmp(cond->valuestring, "eq") == 0) t->data.device_state.cond = ZB_RULE_COND_EQ;
                    else if (strcmp(cond->valuestring, "ne") == 0) t->data.device_state.cond = ZB_RULE_COND_NE;
                    else if (strcmp(cond->valuestring, "gt") == 0) t->data.device_state.cond = ZB_RULE_COND_GT;
                    else if (strcmp(cond->valuestring, "lt") == 0) t->data.device_state.cond = ZB_RULE_COND_LT;
                    else if (strcmp(cond->valuestring, "gte") == 0) t->data.device_state.cond = ZB_RULE_COND_GTE;
                    else if (strcmp(cond->valuestring, "lte") == 0) t->data.device_state.cond = ZB_RULE_COND_LTE;
                }
                t->data.device_state.value = value ? value->valuedouble : 0;
            }
            else if (strcmp(type->valuestring, "time_range") == 0) {
                t->type = ZB_RULE_TRIGGER_TIME_RANGE;
                cJSON* from = cJSON_GetObjectItem(trig_item, "from");
                cJSON* to = cJSON_GetObjectItem(trig_item, "to");
                cJSON* days = cJSON_GetObjectItem(trig_item, "days_of_week");
                cJSON* delay = cJSON_GetObjectItem(trig_item, "delay_sec");

                if (from) strncpy(t->data.time_range.from, from->valuestring, 5);
                if (to) strncpy(t->data.time_range.to, to->valuestring, 5);
                t->data.time_range.from[5] = '\0';
                t->data.time_range.to[5] = '\0';

                t->data.time_range.days_of_week = days ? days->valueint : 0xFF; // все дни
                t->data.time_range.delay_sec = delay ? delay->valueint : 0;
            }
            new_rule.trigger_count++;
        }
    }

    // === Действия ===
    cJSON* actions = cJSON_GetObjectItem(json, "actions");
    if (actions && cJSON_IsArray(actions)) {
        cJSON* act_item = NULL;
        cJSON_ArrayForEach(act_item, actions) {
            if (new_rule.action_count >= ZB_RULE_MAX_ACTIONS) break;
            zb_rule_action_t* a = &new_rule.actions[new_rule.action_count];

            cJSON* act_type = cJSON_GetObjectItem(act_item, "type");
            if (!act_type || !cJSON_IsString(act_type)) continue;

            if (strcmp(act_type->valuestring, "device_command") == 0) {
                a->type = ZB_RULE_ACTION_DEVICE_CMD;
                cJSON* short_addr = cJSON_GetObjectItem(act_item, "short");
                cJSON* endpoint = cJSON_GetObjectItem(act_item, "endpoint");
                cJSON* cmd_id = cJSON_GetObjectItem(act_item, "cmd_id");

                a->data.device_cmd.short_addr = short_addr ? short_addr->valueint : 0;
                a->data.device_cmd.endpoint = endpoint ? endpoint->valueint : 1;
                a->data.device_cmd.cmd_id = cmd_id ? cmd_id->valueint : 0;
            }
            new_rule.action_count++;
        }
    }

    cJSON_Delete(json);

    // ✅ Копируем содержимое в существующий указатель
    memcpy(rules_array[index], &new_rule, sizeof(zb_rule_t));

    // 💾 Сохраняем в SPIFFS
    zb_rule_engine_save_to_spiffs();

    // 📡 Опционально: отправь событие через WebSocket (если есть шина)
    // ws_send_rules_update(); // если реализовано

    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}


// === DELETE /api/rules/:id ===
esp_err_t api_delete_rule_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "HTTP_DELETE /api/rules api_delete_rule_handler");
    const char* uri = req->uri;
    const char* id = strrchr(uri, '/') + 1;
    if (!id || strlen(id) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid ID");
        return ESP_FAIL;
    }

    bool removed = zb_rule_engine_remove_rule(id);
    if (removed) {
        httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    } else {
        httpd_resp_sendstr(req, "{\"status\":\"not_found\"}");
    }
    return ESP_OK;
}

esp_err_t api_delete_all_rules_handler(httpd_req_t *req)
{
    zb_rule_engine_remove_all_rules();
    httpd_resp_sendstr(req, "{\"status\":\"ok\", \"message\":\"All rules deleted\"}");
    return ESP_OK;
}


// === POST /api/rules/run/:id ===
esp_err_t api_run_rule_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "HTTP_POST /api/rules api_run_rule_handler");
    const char *uri = req->uri;
    const char *id = strrchr(uri, '/') + 1; // извлечь id после последнего '/'
    if (!id || strlen(id) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid rule ID");
        return ESP_FAIL;
    }

    ESP_LOGI("RULE_RUN", "Запуск правила: %s", id);

    // Вызываем функцию из zb_manager_rules.h
    bool success = zb_automation_run_rule_now(id); // или как у тебя называется
    // Если у тебя нет такой функции — см. ниже, я её напишу

    httpd_resp_set_type(req, "application/json");
    if (success) {
        httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    } else {
        httpd_resp_sendstr(req, "{\"status\":\"not_found\"}");
    }
    return ESP_OK;
}
