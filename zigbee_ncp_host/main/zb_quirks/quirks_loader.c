// quirks_loader.c
#include "esp_log.h"
#include "esp_vfs.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "errno.h"
#include "spiffs_helper.h"  // ← используем путь

#include "quirks_storage.h"

static const char *TAG = "QUIRKS";

cJSON *g_tuya_quirks = NULL;

esp_err_t quirks_load_tuya_database(void)
{
    ESP_LOGI(TAG, "Loading TUYA quirks database...");

    if (!esp_spiffs_mounted("quirks")) {
        ESP_LOGE(TAG, "SPIFFS partition 'quirks' not mounted");
        return ESP_FAIL;
    }

    FILE *f = fopen("/quirks/tuya_models.json", "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open /quirks/tuya_models.json");
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    if (!buf) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        ESP_LOGE(TAG, "Failed to parse tuya_models.json");
        return ESP_FAIL;
    }

    // === ИЗМЕНИТЬ: извлекаем массив "models" ===
    cJSON *models_array = cJSON_GetObjectItem(root, "models");
    if (!models_array || !cJSON_IsArray(models_array)) {
        ESP_LOGE(TAG, "❌ 'models' array not found in JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    if (g_tuya_quirks) cJSON_Delete(g_tuya_quirks);
    g_tuya_quirks = cJSON_Duplicate(models_array, 1);  // ← дублируем массив
    cJSON_Delete(root);

    ESP_LOGI(TAG, "✅ TUYA quirks loaded successfully (%d models)", cJSON_GetArraySize(g_tuya_quirks));
    return ESP_OK;
}


cJSON *quirks_get_quirk_by_model(const char *model_id)
{
    if (!g_tuya_quirks || !model_id) return NULL;

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, g_tuya_quirks) {
        cJSON *models = cJSON_GetObjectItem(item, "model_ids");
        if (models && cJSON_IsArray(models)) {
            cJSON *model = NULL;
            cJSON_ArrayForEach(model, models) {
                if (cJSON_IsString(model) && model->valuestring && strcmp(model->valuestring, model_id) == 0) {
                    return item;
                }
            }
        }
    }
    return NULL;
}

cJSON *quirks_get_dp_info(const cJSON *quirk, uint8_t dp_id)
{
    cJSON *dp_map = cJSON_GetObjectItem((cJSON *)quirk, "dp_map");
    if (!cJSON_IsArray(dp_map)) return NULL;

    cJSON *dp = NULL;
    cJSON_ArrayForEach(dp, dp_map) {
        cJSON *id = cJSON_GetObjectItem(dp, "dp");
        if (cJSON_IsNumber(id) && id->valueint == dp_id) {
            return dp;
        }
    }
    return NULL;
}

esp_err_t quirks_save_tuya_database(cJSON *db)
{
    if (!db) {
        ESP_LOGE(TAG, "❌ Cannot save: db is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // === 1. Смонтирован ли раздел 'quirks'? ===
    if (!esp_spiffs_mounted("quirks")) {
        ESP_LOGE(TAG, "❌ SPIFFS partition 'quirks' not mounted");
        return ESP_FAIL;
    }

    // === 2. Сериализуем JSON в строку ===
    char *json_str = cJSON_Print(db);
    if (!json_str) {
        ESP_LOGE(TAG, "❌ Failed to print JSON");
        return ESP_ERR_NO_MEM;
    }

    // === 3. Открываем файл для записи ===
    FILE *f = fopen("/quirks/tuya_models.json", "w");
    if (!f) {
        ESP_LOGE(TAG, "❌ Failed to open /quirks/tuya_models.json for writing");
        free(json_str);
        return ESP_FAIL;
    }

    // === 4. Записываем ===
    size_t len = strlen(json_str);
    if (fwrite(json_str, 1, len, f) != len) {
        ESP_LOGE(TAG, "❌ Failed to write all data to /quirks/tuya_models.json");
        fclose(f);
        free(json_str);
        return ESP_FAIL;
    }

    // === 5. Закрываем ===
    fclose(f);
    free(json_str);

    ESP_LOGI(TAG, "✅ TUYA quirks database saved to /quirks/tuya_models.json");

    // === 6. Опционально: перечитать, чтобы проверить? ===
    // quirks_load_tuya_database();  // если хочешь обновить g_tuya_quirks

    return ESP_OK;
}
