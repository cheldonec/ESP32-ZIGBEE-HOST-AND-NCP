// quirks_loader.c
#include "esp_log.h"
#include "esp_vfs.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "errno.h"
#include "spiffs_helper.h"
#include "quirks_storage.h"

static const char *TAG = "QUIRKS";

static cJSON *g_quirks_index = NULL;
static bool g_initialized = false;

//#define QUIRKS_DIR "/quirks"
#define INDEX_FILE SPIFFS_QUIRKS_MOUNT_POINT   "/index.json"

// Внутренняя функция: чтение JSON из файла
static cJSON *load_json_from_file(const char *path) {
    if (!esp_spiffs_mounted("quirks")) {
        ESP_LOGE(TAG, "SPIFFS 'quirks' not mounted");
        return NULL;
    }

    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(buf);
    free(buf);

    if (!json) {
        ESP_LOGE(TAG, "Failed to parse %s", path);
    }

    return json;
}

// Внутренняя функция: запись JSON в файл
static esp_err_t save_json_to_file(const char *path, const cJSON *json) {
    FILE *f = fopen(path, "w");
    if (!f) return ESP_FAIL;

    char *str = cJSON_Print(json);
    if (!str) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    fwrite(str, 1, strlen(str), f);
    free(str);
    fclose(f);

    return ESP_OK;
}

esp_err_t quirks_load_index(void) {
    ESP_LOGI(TAG, "Loading quirks index...");

    if (!esp_spiffs_mounted("quirks")) {
        ESP_LOGE(TAG, "SPIFFS partition 'quirks' not mounted");
        return ESP_FAIL;
    }

    cJSON *index = load_json_from_file(INDEX_FILE);
    if (!index) {
        ESP_LOGE(TAG, "Failed to load index.json");
        return ESP_FAIL;
    }

    cJSON *models_arr = cJSON_GetObjectItem(index, "models");
    if (!models_arr || !cJSON_IsArray(models_arr)) {
        ESP_LOGE(TAG, "Invalid index.json: missing 'models' array");
        cJSON_Delete(index);
        return ESP_FAIL;
    }

    if (g_quirks_index) cJSON_Delete(g_quirks_index);
    g_quirks_index = index;

    ESP_LOGI(TAG, "✅ Quirks index loaded (%d models)", cJSON_GetArraySize(models_arr));
    return ESP_OK;
}

esp_err_t quirks_init(void) {
    if (g_initialized) return ESP_OK;

    esp_err_t err = quirks_load_index();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No index found, creating empty one");
        // Можно создать дефолтный index
    }

    g_initialized = true;
    return ESP_OK;
}

cJSON *quirks_get_device_template(const char *model_id, const char *manufacturer_name)
{
    if (!g_initialized) quirks_init();
    if (!model_id) return NULL;

    cJSON *template = NULL;

    // 1. Пытаемся загрузить специфичный шаблон: model + manufacturer
    if (manufacturer_name && strlen(manufacturer_name) > 0) {
        char specific_filename[64];
        snprintf(specific_filename, sizeof(specific_filename), "%s__%s.json", model_id, manufacturer_name);

        char path[128];
        snprintf(path, sizeof(path), "%s/%s", SPIFFS_QUIRKS_MOUNT_POINT, specific_filename);

        template = load_json_from_file(path);
        if (template) {
            ESP_LOGI(TAG, "✅ Loaded specific template: %s", specific_filename);
            return template;
        } else {
            ESP_LOGD(TAG, "No specific template for %s (%s)", model_id, manufacturer_name);
        }
    }

    // 2. Грузим общий шаблон по model_id
    cJSON *models = cJSON_GetObjectItem(g_quirks_index, "models");
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, models) {
        cJSON *mid = cJSON_GetObjectItem(item, "model_id");
        if (cJSON_IsString(mid) && mid->valuestring && strcmp(mid->valuestring, model_id) == 0) {
            cJSON *filename = cJSON_GetObjectItem(item, "filename");
            if (!filename || !cJSON_IsString(filename)) return NULL;

            char path[64];
            snprintf(path, sizeof(path), "%s/%s", SPIFFS_QUIRKS_MOUNT_POINT, filename->valuestring);

            template = load_json_from_file(path);
            if (template) {
                ESP_LOGI(TAG, "✅ Loaded generic template: %s", filename->valuestring);
            } else {
                ESP_LOGE(TAG, "Failed to load generic template from %s", path);
            }
            return template;  // Caller must free!
        }
    }

    return NULL;
}

esp_err_t quirks_apply_template_to_device(cJSON *dev_json, const cJSON *template_quirk) {
    if (!dev_json || !template_quirk) {
        ESP_LOGE(TAG, "Invalid args to apply template");
        return ESP_ERR_INVALID_ARG;
    }

    // === Обновляем ВСЕ нужные поля устройства ===
    const char *top_level_fields[] = {
        "friendly_name",
        "is_in_build_status",
        "device_timeout_ms",
        "manufacturer_code",
        "capability",
        "lqi",
        "short_addr"  // обычно 0 в шаблоне, но на всякий случай
    };

    for (int i = 0; i < sizeof(top_level_fields) / sizeof(top_level_fields[0]); i++) {
        cJSON *item = cJSON_GetObjectItem((cJSON *)template_quirk, top_level_fields[i]);
        if (item) {
            cJSON *dup = cJSON_Duplicate(item, 1);
            cJSON *old = cJSON_GetObjectItem(dev_json, top_level_fields[i]);
            if (old) cJSON_Delete(old);
            cJSON_AddItemToObject(dev_json, top_level_fields[i], dup);
            ESP_LOGD(TAG, "Applied top-level field: %s", top_level_fields[i]);
        }
    }

    // === Теперь копируем кластеры и эндпоинты ===
    const char *complex_objects[] = {
        "device_basic_cluster",
        "device_power_config_cluster",
        "endpointscount",
        "endpoints"
    };

    for (int i = 0; i < 4; i++) {
        cJSON *obj = cJSON_GetObjectItem((cJSON *)template_quirk, complex_objects[i]);
        if (obj) {
            cJSON *dup = cJSON_Duplicate(obj, 1);
            cJSON *old = cJSON_GetObjectItem(dev_json, complex_objects[i]);
            if (old) cJSON_Delete(old);
            cJSON_AddItemToObject(dev_json, complex_objects[i], dup);
            ESP_LOGD(TAG, "Applied complex object: %s", complex_objects[i]);
        }
    }

    ESP_LOGI(TAG, "✅ Template fully applied to device");
    return ESP_OK;
}

esp_err_t quirks_index_add_model(const char *model_id, const char *manufacturer, const char *filename) {
    cJSON *models = cJSON_GetObjectItem(g_quirks_index, "models");
    if (!models || !cJSON_IsArray(models)) return ESP_ERR_INVALID_STATE;

    // Проверим, нет ли уже такой модели
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, models) {
        cJSON *mid = cJSON_GetObjectItem(item, "model_id");
        if (cJSON_IsString(mid) && mid->valuestring && strcmp(mid->valuestring, model_id) == 0) {
            ESP_LOGW(TAG, "Model %s already in index", model_id);
            return ESP_OK; // обновим?
        }
    }

    cJSON *new_item = cJSON_CreateObject();
    cJSON_AddStringToObject(new_item, "model_id", model_id);
    cJSON_AddStringToObject(new_item, "manufacturer", manufacturer ? manufacturer : "Unknown");
    cJSON_AddStringToObject(new_item, "filename", filename);
    cJSON_AddNumberToObject(new_item, "tpl_id", 1000 + cJSON_GetArraySize(models));

    cJSON_AddItemToArray(models, new_item);

    // Обновим счётчик
    cJSON_Delete(cJSON_GetObjectItem(g_quirks_index, "count"));
    cJSON_AddNumberToObject(g_quirks_index, "count", cJSON_GetArraySize(models));

    return save_json_to_file(INDEX_FILE, g_quirks_index);
}

esp_err_t quirks_save_device_template(const char *model_id, const char *manufacturer, const cJSON *template) {
    if (!model_id || !template) return ESP_ERR_INVALID_ARG;

    char filename[32];
    snprintf(filename, sizeof(filename), "%s.json", model_id);

    char path[64];
    snprintf(path, sizeof(path), "%s/%s", SPIFFS_QUIRKS_MOUNT_POINT, filename);

    esp_err_t err = save_json_to_file(path, template);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save template to %s", path);
        return err;
    }

    // Добавить в индекс
    err = quirks_index_add_model(model_id, manufacturer, filename);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update index for %s", model_id);
    }

    ESP_LOGI(TAG, "✅ Template saved: %s", model_id);
    return ESP_OK;
}

esp_err_t quirks_remove_device_template(const char *model_id) {
    if (!model_id) return ESP_ERR_INVALID_ARG;

    cJSON *models = cJSON_GetObjectItem(g_quirks_index, "models");
    cJSON *item = NULL;
    cJSON *to_remove = NULL;

    cJSON_ArrayForEach(item, models) {
        cJSON *mid = cJSON_GetObjectItem(item, "model_id");
        if (cJSON_IsString(mid) && mid->valuestring && strcmp(mid->valuestring, model_id) == 0) {
            to_remove = item;
            break;
        }
    }

    if (!to_remove) return ESP_ERR_NOT_FOUND;

    cJSON *filename_obj = cJSON_GetObjectItem(to_remove, "filename");
    if (cJSON_IsString(filename_obj)) {
        char path[64];
        snprintf(path, sizeof(path), "%s/%s", SPIFFS_QUIRKS_MOUNT_POINT, filename_obj->valuestring);
        unlink(path); // удалить файл
    }

    cJSON_DeleteItemFromArray(models, (int)(item - models->child));
    cJSON_Delete(cJSON_GetObjectItem(g_quirks_index, "count"));
    cJSON_AddNumberToObject(g_quirks_index, "count", cJSON_GetArraySize(models));

    save_json_to_file(INDEX_FILE, g_quirks_index);
    ESP_LOGI(TAG, "✅ Template removed: %s", model_id);
    return ESP_OK;
}

esp_err_t quirks_save_device_as_variant_template(const char *model_id, const char *manufacturer_name, const cJSON *device_json)
{
    if (!model_id || !manufacturer_name || !device_json) return ESP_ERR_INVALID_ARG;

    char filename[64];
    snprintf(filename, sizeof(filename), "%s__%s.json", model_id, manufacturer_name);

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", SPIFFS_QUIRKS_MOUNT_POINT, filename);

    // Сохраняем весь device_json как шаблон
    esp_err_t err = save_json_to_file(path, device_json);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save variant template: %s", path);
        return err;
    }

    // Добавить в индекс? (опционально)
    quirks_index_add_model(model_id, "Tuya", filename);  // можно добавить флаг "variant"

    ESP_LOGI(TAG, "✅ Saved variant template: %s", filename);
    return ESP_OK;
}