/// @brief [spiffs_helper.c] Вспомогательный модуль для работы с SPIFFS-разделами
/// Занимается инициализацией, проверкой и записью встроенных образов (UI, quirks) в SPIFFS

#include "esp_partition.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "zbm_spiffs_helper.h"
#include <inttypes.h>
#include <dirent.h>
#include "string.h"


/// @brief Тег для логирования в этом модуле
static const char *TAG = "ZBM_SPIFFS_HELPER";

/// @brief Конфигурация SPIFFS для хранения конфигурации устройства
esp_vfs_spiffs_conf_t zbm_spiff_conf = {
    .base_path = SPIFFS_ZBM_CONF_MOUNT_POINT,
    .partition_label = "zbm_conf",
    .max_files = 2,
    .format_if_mount_failed = true
};

/// @brief Конфигурация SPIFFS для хранения веб-интерфейса (UI)
esp_vfs_spiffs_conf_t zbm_spiff_UI_conf = {
    .base_path = SPIFFS_ZBM_UI_MOUNT_POINT,
    .partition_label = "zbm_ui",
    .max_files = 2,
    .format_if_mount_failed = true
};

/// @brief Конфигурация SPIFFS для хранения quirks (правил совместимости устройств)
esp_vfs_spiffs_conf_t zbm_spiff_quirks_conf = {
    .base_path = SPIFFS_ZBM_QUIRKS_MOUNT_POINT,  // ← Убрали дублирование пути!
    .partition_label = "zbm_quirks",             // ← Исправили: было "quirks"
    .max_files = 2,
    .format_if_mount_failed = true
};

/// @brief Конфигурация SPIFFS для хранения SSL-сертификатов
esp_vfs_spiffs_conf_t zbm_spiff_certs_conf = {
    .base_path = SPIFFS_ZBM_CERTS_MOUNT_POINT,
    .partition_label = "zbm_certs",              // ← Исправили: было "certs"
    .max_files = 2,
    .format_if_mount_failed = true
};

/// @brief Инициализирует все SPIFFS-разделы
/// @brief Инициализирует все SPIFFS-разделы с мониторингом памяти
esp_err_t init_spiffs(void)
{
    esp_err_t ret;
    size_t heap_before, heap_after;

    ESP_LOGI(TAG, "=== Начало инициализации SPIFFS ===");
    ESP_LOGI(TAG, "Free heap before: %d bytes", esp_get_free_heap_size());

    // === 1. zbm_conf ===
    heap_before = esp_get_free_heap_size();
    ret = esp_vfs_spiffs_register(&zbm_spiff_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount zbm_conf: %s", esp_err_to_name(ret));
        return ret;
    }
    heap_after = esp_get_free_heap_size();
    ESP_LOGI(TAG, "[1] Mounted zbm_conf, used: %d bytes", heap_before - heap_after);

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("zbm_conf", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS info (zbm_conf): %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS zbm_conf: total=%d, used=%d", total, used);
    }

    // === 2. zbm_ui ===
    heap_before = esp_get_free_heap_size();
    ret = esp_vfs_spiffs_register(&zbm_spiff_UI_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount zbm_ui: %s", esp_err_to_name(ret));
        return ret;
    }
    heap_after = esp_get_free_heap_size();
    ESP_LOGI(TAG, "[2] Mounted zbm_ui, used: %d bytes", heap_before - heap_after);

    ret = esp_spiffs_info("zbm_ui", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS info (zbm_ui): %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS zbm_ui: total=%d, used=%d", total, used);
    }

    // === 3. zbm_quirks ===
    heap_before = esp_get_free_heap_size();
    ret = esp_vfs_spiffs_register(&zbm_spiff_quirks_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount zbm_quirks: %s", esp_err_to_name(ret));
        return ret;
    }
    heap_after = esp_get_free_heap_size();
    ESP_LOGI(TAG, "[3] Mounted zbm_quirks, used: %d bytes", heap_before - heap_after);

    ret = esp_spiffs_info("zbm_quirks", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS info (zbm_quirks): %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS zbm_quirks: total=%d, used=%d", total, used);
    }

    // === 4. zbm_certs ===
    heap_before = esp_get_free_heap_size();
    ret = esp_vfs_spiffs_register(&zbm_spiff_certs_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount zbm_certs: %s", esp_err_to_name(ret));
        return ret;
    }
    heap_after = esp_get_free_heap_size();
    ESP_LOGI(TAG, "[4] Mounted zbm_certs, used: %d bytes", heap_before - heap_after);

    ret = esp_spiffs_info("zbm_certs", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS info (zbm_certs): %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS zbm_certs: total=%d, used=%d", total, used);
    }

    ESP_LOGI(TAG, "✅ Все SPIFFS-разделы успешно смонтированы");
    ESP_LOGI(TAG, "Free heap after: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Min free heap ever: %d bytes", esp_get_minimum_free_heap_size());

    return ESP_OK;
}

// UTILS
// === Проверка, существует ли файл ===
bool spiffs_file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// === Листинг файлов в директории ===
cJSON* spiffs_list_directory(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) return NULL;

    cJSON* files = cJSON_CreateArray();

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        // Проверяем длину пути
        size_t path_len = strlen(dir_path);
        size_t name_len = strlen(entry->d_name);

        if (path_len + 1 + name_len + 1 > 256) { // +1 для '/', +1 для '\0'
            ESP_LOGW("SPIFFS", "Path too long: %s/%s", dir_path, entry->d_name);
            continue;
        }

        char full_path[256];
        int n = snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        if (n < 0 || n >= sizeof(full_path)) {
            ESP_LOGW("SPIFFS", "snprintf failed or truncated: %s/%s", dir_path, entry->d_name);
            continue;
        }

        struct stat st;
        if (stat(full_path, &st) == 0) {
            cJSON* file = cJSON_CreateObject();
            cJSON_AddStringToObject(file, "name", entry->d_name);
            cJSON_AddNumberToObject(file, "size", st.st_size);
            cJSON_AddBoolToObject(file, "is_dir", S_ISDIR(st.st_mode));
            cJSON_AddItemToArray(files, file);
        }
    }

    closedir(dir);
    return files;
}