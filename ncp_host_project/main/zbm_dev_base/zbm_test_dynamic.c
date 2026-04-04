// File: main/zbm_dev_base/zbm_test_dynamic.c
#include "zbm_test_dynamic.h"

#include "zbm_core_sync.h"
#include "zbm_dev_types.h"
#include "zbm_clusters_type.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "ZBM_TEST_DYN";

static esp_timer_handle_t s_dynamic_timer = NULL;
static zbm_dev_t* s_test_device = NULL;

// Уникальный IEEE для теста
static const uint8_t TEST_IEEE[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03};
static const uint16_t TEST_SHORT_ADDR = 0x1234;

// ===================================================================
// === Вспомогательные функции ========================================
// ===================================================================
extern void zbm_device_db_foreach_safe(void (*visitor)(zbm_dev_t*, void*), void* ctx);

// Вспомогательная функция для логирования
void log_device_visitor(zbm_dev_t* dev, void* ctx) {
    char ieee_str[24];
    snprintf(ieee_str, sizeof(ieee_str), "%02X:%02X..%02X",
             dev->ieee_addr[0], dev->ieee_addr[1], dev->ieee_addr[7]);
    ESP_LOGI("DB_CHECK", "  → dev=%p, short=0x%04X, ieee=%s", dev, dev->short_addr, ieee_str);
}

// Выводит текущее состояние БД
void dump_device_db(const char* step_name) {
    ESP_LOGI("DB_DUMP", "--- Device DB state: %s ---", step_name);
    zbm_device_db_foreach_safe(log_device_visitor, NULL);
}

static char* device_to_json_char(zbm_dev_t* dev) {
    // временно не выполняем
    //return NULL;
    if (!dev) return NULL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "friendly_name", dev->friendly_name);
    cJSON_AddNumberToObject(root, "short_addr", dev->short_addr);
    cJSON_AddNumberToObject(root, "lqi", dev->lqi);
    cJSON_AddBoolToObject(root, "is_online", dev->is_online);

    char ieee_str[24];
    snprintf(ieee_str, sizeof(ieee_str), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             dev->ieee_addr[0], dev->ieee_addr[1], dev->ieee_addr[2], dev->ieee_addr[3],
             dev->ieee_addr[4], dev->ieee_addr[5], dev->ieee_addr[6], dev->ieee_addr[7]);
    cJSON_AddStringToObject(root, "ieee_addr", ieee_str);

    cJSON* endpoints = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "endpoints", endpoints);

    for (uint8_t ep_idx = 0; ep_idx < dev->endpoints_count; ep_idx++) {
        zbm_dev_endpoint_t* ep = dev->endpoints_array[ep_idx];
        if (!ep) continue;

        cJSON* jep = cJSON_CreateObject();
        cJSON_AddNumberToObject(jep, "device_id", ep->device_id);
        cJSON_AddStringToObject(jep, "device_type", get_device_type_name(ep->device_id));

        cJSON* clusters = cJSON_CreateArray();
        cJSON_AddItemToObject(jep, "clusters", clusters);

        for (int cl_idx = 0; cl_idx < ep->standart_cluster_count; cl_idx++) {
            zbm_standart_cluster_t* cluster = ep->standart_cluster_array[cl_idx];
            if (!cluster) continue;

            cJSON* jcl = cJSON_CreateObject();
            cJSON_AddNumberToObject(jcl, "id", cluster->id);
            cJSON_AddStringToObject(jcl, "role", 
                (cluster->role_mask & ZBM_CLUSTER_ROLE_CLIENT) ? 
                ((cluster->role_mask & ZBM_CLUSTER_ROLE_SERVER) ? "server+client" : "client") :
                "server");
            cJSON_AddStringToObject(jcl, "name", zbm_get_cluster_friendlyname(cluster->id));

            // Атрибуты
            cJSON* attrs = cJSON_CreateArray();
            cJSON_AddItemToObject(jcl, "attributes", attrs);
            for (int a_idx = 0; a_idx < cluster->attr_count; a_idx++) {
                zbm_cluster_attribute_t* attr = cluster->attr_array[a_idx];
                if (!attr) continue;

                cJSON* jattr = cJSON_CreateObject();
                cJSON_AddNumberToObject(jattr, "id", attr->id);
                cJSON_AddStringToObject(jattr, "name", attr->friendlyname ? attr->friendlyname : "Unknown");
                cJSON_AddStringToObject(jattr, "guid", attr->guid);

                switch (attr->data_type) {
                    case ZBM_ATTR_TYPE_BOOL: {
                        bool val = *(bool*)attr->p_value;
                        cJSON_AddBoolToObject(jattr, "value", val);
                        break;
                    }
                    case ZBM_ATTR_TYPE_U16: {
                        uint16_t val = *(uint16_t*)attr->p_value;
                        cJSON_AddNumberToObject(jattr, "value", val);
                        break;
                    }
                    default:
                        cJSON_AddStringToObject(jattr, "value", "<raw>");
                        break;
                }
                cJSON_AddItemToArray(attrs, jattr);
            }

            // Команды
            cJSON* cmds = cJSON_CreateArray();
            cJSON_AddItemToObject(jcl, "commands", cmds);
            for (int c_idx = 0; c_idx < cluster->standart_cmd_count; c_idx++) {
                zbm_cluster_standart_cmd_t* cmd = cluster->standart_cmd_array[c_idx];
                if (!cmd) continue;
                cJSON* jcmd = cJSON_CreateObject();
                cJSON_AddNumberToObject(jcmd, "id", cmd->id);
                cJSON_AddStringToObject(jcmd, "name", cmd->friendlyname ? cmd->friendlyname : "Unknown");
                cJSON_AddStringToObject(jcmd, "guid", cmd->guid);
                cJSON_AddItemToArray(cmds, jcmd);
            }

            // Кастомные репорты
            cJSON* custom_reports = cJSON_CreateArray();
            cJSON_AddItemToObject(jcl, "custom_reports", custom_reports);
            for (int r_idx = 0; r_idx < cluster->custom_report_cmd_count; r_idx++) {
                zbm_cluster_custom_report_cmd_t* rep = cluster->custom_report_cmd_array[r_idx];
                if (!rep) continue;
                cJSON* jrep = cJSON_CreateObject();
                cJSON_AddNumberToObject(jrep, "id", rep->id);
                cJSON_AddStringToObject(jrep, "name", rep->friendlyname ? rep->friendlyname : "Unknown");
                cJSON_AddStringToObject(jrep, "guid", rep->guid);
                if (rep->data_size == 1) {
                    cJSON_AddNumberToObject(jrep, "value", *(uint8_t*)rep->p_value);
                } else {
                    cJSON_AddStringToObject(jrep, "value", "<raw>");
                }
                cJSON_AddItemToArray(custom_reports, jrep);
            }

            cJSON_AddItemToArray(clusters, jcl);
        }
        cJSON_AddItemToArray(endpoints, jep);
    }

    char* json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

// ===================================================================
// === Шаг 7: Сохранение в SPIFFS и завершение =======================
// ===================================================================

void step_save_and_finish(void* arg) {
    (void)arg;
    ESP_LOGI(TAG, "--- Step 7: Saving device to SPIFFS ---");
    bool saved = zbm_save_device_to_spiffs_safe(s_test_device);
    if (saved) {
        ESP_LOGI(TAG, "💾 Device saved to SPIFFS: dev_0x%04X.json", s_test_device->short_addr);
    } else {
        ESP_LOGE(TAG, "❌ Failed to save device to SPIFFS");
    }
    ESP_LOGI(TAG, "🎉 All dynamic tests completed successfully!");
}

// ===================================================================
// === Шаг 6: Тест Simple Descriptor ================================
// ===================================================================

void step_test_simple_descriptor(void* arg) {
    (void)arg;
    ESP_LOGI(TAG, "--- Step 6: Applying Simple Descriptor to endpoint 4 ---");

    uint8_t endpoint_id = 4;
    uint16_t device_id = ZBM_DEVICE_TYPE_ON_OFF_LIGHT;

    uint16_t input_clusters[] = {
        ZBM_CLUSTER_ID_ON_OFF,
        ZBM_CLUSTER_ID_LEVEL_CONTROL,
        ZBM_CLUSTER_ID_IDENTIFY
    };
    uint8_t in_count = 3;

    uint16_t output_clusters[] = {
        ZBM_CLUSTER_ID_OTA_UPGRADE,
        ZBM_CLUSTER_ID_GROUPS,
        ZBM_CLUSTER_ID_SCENES
    };
    uint8_t out_count = 3;

    zbm_device_apply_simple_descriptor_safe(
        s_test_device,
        endpoint_id,
        device_id,
        input_clusters, in_count,
        output_clusters, out_count
    );

    zbm_guid_db_update_device_guids_safe(s_test_device);
    ESP_LOGI(TAG, "✅ Simple Descriptor applied to endpoint %d", endpoint_id);

    char* json = device_to_json_char(s_test_device);
    if (json) {
        ESP_LOGI(TAG, "📋 Device JSON:\n%s", json);
        free(json);
    }

    // === 🔍 Проверка перед сохранением ===
    dump_device_db("Before Save to SPIFFS");

    // Запуск сохранения
    const esp_timer_create_args_t timer_args = {
        .callback = step_save_and_finish,
        .name = "save_step"
    };
    esp_timer_handle_t timer;
    esp_err_t err = esp_timer_create(&timer_args, &timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to create save timer: %s", esp_err_to_name(err));
        return;
    }
    err = esp_timer_start_once(timer, 100000); // 0.1 сек
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to start save timer: %s", esp_err_to_name(err));
        esp_timer_delete(timer);
    } else {
        ESP_LOGI(TAG, "⏰ Scheduled save to SPIFFS...");
    }
}

// ===================================================================
// === Шаг 5: OnOff report в новый эндпоинт ==========================
// ===================================================================

void step_test_onoff_report_to_new_endpoint(void* arg) {
    (void)arg;
    ESP_LOGI(TAG, "--- Step 5: Simulating OnOff report to endpoint 3 ---");

    uint8_t endpoint_id = 3;
    uint16_t cluster_id = ZBM_CLUSTER_ID_ON_OFF;
    uint16_t attr_id = 0x0000;
    bool on_value = true;

    uint8_t result = zbm_device_apply_reported_value_safe(
        s_test_device,
        endpoint_id,
        cluster_id,
        ZBM_CLUSTER_ROLE_SERVER,
        attr_id,
        "OnOff",
        0,
        ZBM_ATTR_TYPE_BOOL,
        sizeof(bool),
        &on_value
    );

    if (result == 1) {
        ESP_LOGI(TAG, "✅ Created OnOff cluster and attribute in endpoint %d", endpoint_id);
    } else {
        ESP_LOGI(TAG, "🔄 Updated OnOff value in endpoint %d", endpoint_id);
    }

    zbm_guid_db_update_device_guids_safe(s_test_device);

    char* json = device_to_json_char(s_test_device);
    if (json) {
        ESP_LOGI(TAG, "📋 Device JSON:\n%s", json);
        free(json);
    }

    // === 🔍 Проверка после OnOff report ===
    dump_device_db("After OnOff Report to EP3");

    // Запуск Simple Descriptor
    const esp_timer_create_args_t timer_args = {
        .callback = step_test_simple_descriptor,
        .name = "simple_desc_step"
    };
    esp_timer_handle_t timer;
    esp_err_t err = esp_timer_create(&timer_args, &timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to create timer: %s", esp_err_to_name(err));
        return;
    }
    err = esp_timer_start_once(timer, 100000); // 0.1 сек
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to start timer: %s", esp_err_to_name(err));
        esp_timer_delete(timer);
    } else {
        ESP_LOGI(TAG, "⏰ Scheduled Simple Descriptor test...");
    }
}

// ===================================================================
// === Шаг 4: Добавление эндпоинта ==================================
// ===================================================================

void step_test_add_endpoint(void* arg) {
    (void)arg;
    ESP_LOGI(TAG, "--- Step 4: Adding endpoint 3 (Dimmable Light) ---");

    bool added = zbm_device_manager_add_endpoint_safe(
        s_test_device,
        3,
        ZBM_DEVICE_TYPE_DIMMABLE_LIGHT,
        "Dimmable Light EP"
    );

    if (added) {
        ESP_LOGI(TAG, "✅ Endpoint 3 added successfully");
    } else {
        ESP_LOGE(TAG, "❌ Failed to add endpoint 3");
        return;
    }

    // Попытка дубликата
    added = zbm_device_manager_add_endpoint_safe(s_test_device, 3, ZBM_DEVICE_TYPE_ON_OFF_LIGHT, "Duplicate");
    if (!added) {
        ESP_LOGI(TAG, "✅ Correctly rejected duplicate endpoint ID 3");
    } else {
        ESP_LOGE(TAG, "❌ ERROR: Duplicate endpoint should be rejected!");
        return;
    }

    char* json = device_to_json_char(s_test_device);
    if (json) {
        ESP_LOGI(TAG, "📋 Device JSON:\n%s", json);
        free(json);
    }

    // === 🔍 Проверка после добавления эндпоинта ===
    dump_device_db("After Adding Endpoint 3");

    // Запуск следующего шага
    const esp_timer_create_args_t timer_args = {
        .callback = step_test_onoff_report_to_new_endpoint,
        .name = "onoff_ep3_step"
    };
    esp_timer_handle_t timer;
    esp_err_t err = esp_timer_create(&timer_args, &timer);
    if (err != ESP_OK) return;
    esp_timer_start_once(timer, 100000);
}

// ===================================================================
// === Шаг 3: Кастомный репорт (Tuya 0xFD) ==========================
// ===================================================================

void step_send_custom_report(void* arg) {
    (void)arg;
    ESP_LOGI(TAG, "--- Step 3: Simulating Tuya custom report (0xFD) ---");

    uint8_t endpoint_id = 1;
    uint16_t cluster_id = ZBM_CLUSTER_ID_ON_OFF;
    uint8_t cmd_id = 0xFD;
    uint8_t data_value = 0x02;

    uint8_t result = zbm_update_cluster_custom_report_safe(
        s_test_device,
        endpoint_id,
        cluster_id,
        ZBM_CLUSTER_ROLE_SERVER,
        cmd_id,
        "Tuya Command 0xFD",
        ZBM_CMD_DATA_TYPE_U8,
        sizeof(data_value),
        &data_value
    );

    if (result == 1) {
        ESP_LOGI(TAG, "✅ Custom report 0xFD created with value 0x%02X", data_value);
    } else {
        ESP_LOGI(TAG, "🔄 Custom report 0xFD updated");
    }

    char* json = device_to_json_char(s_test_device);
    if (json) {
        ESP_LOGI(TAG, "📋 Device JSON:\n%s", json);
        free(json);
    }

    // === 🔍 Проверка после custom report ===
    dump_device_db("After Custom Report (0xFD)");

    // Запуск добавления эндпоинта
    const esp_timer_create_args_t timer_args = {
        .callback = step_test_add_endpoint,
        .name = "ep_add_step"
    };
    esp_timer_handle_t timer;
    esp_err_t err = esp_timer_create(&timer_args, &timer);
    if (err != ESP_OK) return;
    esp_timer_start_once(timer, 100000);
}

// ===================================================================
// === Шаг 2: Применение Simple Descriptor к endpoint 1 ==============
// ===================================================================

void step_apply_initial_descriptor(void* arg) {
    (void)arg;
    ESP_LOGI(TAG, "--- Step 2: Applying initial Simple Descriptor to endpoint 1 ---");

    uint8_t endpoint_id = 1;
    uint16_t device_id = ZBM_DEVICE_TYPE_ON_OFF_SWITCH;

    uint16_t input_clusters[] = { ZBM_CLUSTER_ID_BASIC, ZBM_CLUSTER_ID_IDENTIFY, ZBM_CLUSTER_ID_ON_OFF };
    uint8_t in_count = 3;

    uint16_t output_clusters[] = { ZBM_CLUSTER_ID_OTA_UPGRADE };
    uint8_t out_count = 1;

    zbm_device_apply_simple_descriptor_safe(
        s_test_device,
        endpoint_id,
        device_id,
        input_clusters, in_count,
        output_clusters, out_count
    );

    zbm_guid_db_update_device_guids_safe(s_test_device);

    char* json = device_to_json_char(s_test_device);
    if (json) {
        ESP_LOGI(TAG, "📋 Device JSON:\n%s", json);
        free(json);
    }

    // === 🔍 Проверка после descriptor ===
    dump_device_db("After Simple Descriptor");

    // Запуск кастомного репорта
    const esp_timer_create_args_t timer_args = {
        .callback = step_send_custom_report,
        .name = "custom_report_step"
    };
    esp_timer_handle_t timer;
    esp_err_t err = esp_timer_create(&timer_args, &timer);
    if (err != ESP_OK) return;
    esp_timer_start_once(timer, 100000);
}

// ===================================================================
// === Шаг 1: Создание устройства с short_addr сразу ================
// ===================================================================

void step_create_device_with_short(void) {
    ESP_LOGI(TAG, "--- Step 1: Creating device with IEEE + short_addr immediately ---");

    // Удаляем старое (на всякий случай)
    //zbm_device_manager_remove_by_ieee_safe(TEST_IEEE);

    // Создаём устройство и полностью его добавляет в хэш таблицу также происходит zbm_guid_db_update_device_guids
    // например при DEVICE_ASSOCIATED_EVENT
    s_test_device = zbm_dev_create_and_add_to_devdb_by_ieee_safe(TEST_IEEE);
    if (!s_test_device) {
        ESP_LOGE(TAG, "❌ Failed to create device by IEEE");
        return;
    }

    char* json1 = device_to_json_char(s_test_device);
    if (json1) {
        ESP_LOGI(TAG, "📋 zbm_dev_create_and_add_to_devdb_by_ieee_safe(TEST_IEEE) JSON:\n%s", json1);
        free(json1);
    }

    // обновляем short_addr
    // например при DEVICE_UPDATE_EVENT после DEVICE_ASSOCIATED_EVENT
    zbm_dev_update_short_addr_safe(s_test_device, TEST_SHORT_ADDR, TEST_IEEE);
    char* json2 = device_to_json_char(s_test_device);
    if (json2) {
        ESP_LOGI(TAG, "📋 zbm_dev_update_short_addr_safe JSON:\n%s", json2);
        free(json2);
    }

    // Обновляем имя
    // изменилось поле friendly_name, поэтому надо вызвать zbm_guid_db_update_device_guids_safe

    if (s_test_device->friendly_name) {
        free(s_test_device->friendly_name);
    }
    char temp_name[32];
    snprintf(temp_name, sizeof(temp_name), "Dev 0x%04X (%02X..%02X)",
             TEST_SHORT_ADDR, TEST_IEEE[0], TEST_IEEE[7]);
    s_test_device->friendly_name = strdup(temp_name);

    ESP_LOGI(TAG, "🆕 Device created: %s (IEEE: DE:AD..03, short: 0x%04X)",
             s_test_device->friendly_name, s_test_device->short_addr);

    // Регистрируем все GUID (сперва удаляет по короткому, потом перерегистрирует)
    zbm_guid_db_update_device_guids_safe(s_test_device);
    char* json3 = device_to_json_char(s_test_device);
    if (json3) {
        ESP_LOGI(TAG, "📋 zbm_guid_db_update_device_guids_safe ПОСЛЕ Установки friendlyname JSON:\n%s", json3);
        free(json3);
    }

    // === 🔍 Проверка: сколько устройств в базе? ===
    dump_device_db("After creation");

    // Переходим к шагу 2
    const esp_timer_create_args_t timer_args = {
        .callback = step_apply_initial_descriptor,
        .name = "desc_step"
    };
    esp_timer_handle_t timer;
    esp_err_t err = esp_timer_create(&timer_args, &timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to create timer: %s", esp_err_to_name(err));
        return;
    }
    esp_timer_start_once(timer, 100000); // 0.1 сек
}

// ===================================================================
// === API ===========================================================
// ===================================================================

void zbm_test_dynamic_run(void) {
    ESP_LOGI(TAG, "🚀 Starting corrected dynamic test...");

    // Убедись, что sync инициализирован
    // zbm_core_sync_init(); // должно быть вызвано ранее

    step_create_device_with_short();
}

void zbm_test_dynamic_stop(void) {
    if (s_test_device) {
        ESP_LOGI(TAG, "🧹 Cleaning up test device...");
        zbm_device_manager_remove_by_ieee_safe(TEST_IEEE);
        s_test_device = NULL;
    }
    ESP_LOGI(TAG, "⏹️ Dynamic test stopped.");
}