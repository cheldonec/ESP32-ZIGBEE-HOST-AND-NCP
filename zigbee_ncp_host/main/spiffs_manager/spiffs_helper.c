/// @brief [spiffs_helper.c] Вспомогательный модуль для работы с SPIFFS-разделами
/// Занимается инициализацией, проверкой и записью встроенных образов (UI, quirks) в SPIFFS

#include "esp_partition.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "spiffs_helper.h"
#include <inttypes.h>

/// @brief Тег для логирования в этом модуле
static const char *TAG = "SPIFFS_HELPER";

/// @brief Указатель на начало встроенного образа UI в памяти (связан с spiffs_ui.bin)
extern const uint8_t spiffs_ui_bin_start[] asm("_binary_spiffs_ui_bin_start");
/// @brief Указатель на конец встроенного образа UI в памяти
extern const uint8_t spiffs_ui_bin_end[]   asm("_binary_spiffs_ui_bin_end");
/// @brief Указатель на начало встроенного образа quirks в памяти (связан с quirks.bin)
extern const uint8_t spiffs_quirks_bin_start[] asm("_binary_spiffs_quirks_bin_start");
/// @brief Указатель на конец встроенного образа quirks в памяти
extern const uint8_t spiffs_quirks_bin_end[]   asm("_binary_spiffs_quirks_bin_end");

/// @brief [spiffs_helper.c] Проверяет, установлен ли UI (по флагу в NVS)
/// @return true, если UI уже установлен
static bool is_ui_installed(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("sys", NVS_READONLY, &nvs);
    if (err != ESP_OK) return false;

    uint8_t installed = 0;
    err = nvs_get_u8(nvs, "ui_installed", &installed);
    nvs_close(nvs);
    return (err == ESP_OK && installed == 1);
}

/// @brief [spiffs_helper.c] Проверяет, установлены ли quirks (по флагу в NVS)
/// @return true, если quirks уже установлены
static bool is_quirks_installed(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("sys", NVS_READONLY, &nvs);
    if (err != ESP_OK) return false;

    uint8_t installed = 0;
    err = nvs_get_u8(nvs, "quirks_installed", &installed);
    nvs_close(nvs);
    return (err == ESP_OK && installed == 1);
}

/// @brief [spiffs_helper.c] Помечает UI как установленный (записывает флаг в NVS)
/// @return ESP_OK при успехе
static esp_err_t mark_ui_installed(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("sys", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_u8(nvs, "ui_installed", 1);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

/// @brief [spiffs_helper.c] Помечает quirks как установленные (записывает флаг в NVS)
/// @return ESP_OK при успехе
static esp_err_t mark_quirks_installed(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("sys", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_u8(nvs, "quirks_installed", 1);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

/// @brief [spiffs_helper.c] Записывает встроенный образ UI в SPIFFS-раздел `spiffs_ui`
/// @return ESP_OK при успехе, ошибка иначе
static esp_err_t write_embedded_spiffs_to_flash(void)
{
    if (is_ui_installed()) {
        ESP_LOGI(TAG, "UI already installed, skipping");
        return ESP_OK;
    }

    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs_ui"
    );
    if (!partition) {
        ESP_LOGE(TAG, "spiffs_ui partition not found");
        return ESP_FAIL;
    }

    size_t size = spiffs_ui_bin_end - spiffs_ui_bin_start;
    ESP_LOGI(TAG, "Erasing spiffs_ui partition (%" PRIu32 " bytes)", partition->size);
    esp_err_t err = esp_partition_erase_range(partition, 0, partition->size);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "Writing UI to spiffs_ui (%" PRIu32 " bytes)", (uint32_t)size);
    err = esp_partition_write(partition, 0, spiffs_ui_bin_start, size);
    if (err != ESP_OK) return err;

    err = mark_ui_installed();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Marked UI as installed");
    }

    return ESP_OK;
}

/// @brief [spiffs_helper.c] Записывает встроенный образ quirks в SPIFFS-раздел `quirks`
/// @return ESP_OK при успехе, ошибка иначе
static esp_err_t write_embedded_quirks_to_flash(void)
{
    if (is_quirks_installed()) {
        ESP_LOGI(TAG, "Quirks already installed, skipping");
        return ESP_OK;
    }

    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "quirks"
    );
    if (!partition) {
        ESP_LOGE(TAG, "quirks partition not found");
        return ESP_FAIL;
    }

    size_t size = spiffs_quirks_bin_end - spiffs_quirks_bin_start;
    ESP_LOGI(TAG, "Erasing quirks partition (%" PRIu32 " bytes)", partition->size);
    esp_err_t err = esp_partition_erase_range(partition, 0, partition->size);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "Writing quirks to flash (%" PRIu32 " bytes)", (uint32_t)size);
    err = esp_partition_write(partition, 0, spiffs_quirks_bin_start, size);
    if (err != ESP_OK) return err;

    err = mark_quirks_installed();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Marked quirks as installed");
    }

    return ESP_OK;
}

/// @brief Конфигурация SPIFFS для хранения конфигурации устройства
esp_vfs_spiffs_conf_t zb_manager_spiff_conf = {
    .base_path = SPIFFS_CFG_MOUNT_POINT,
    .partition_label = "spiffs_config",
    .max_files = 2,
    .format_if_mount_failed = true
};

/// @brief Конфигурация SPIFFS для хранения веб-интерфейса (UI)
esp_vfs_spiffs_conf_t zb_manager_spiff_UI_conf = {
    .base_path = SPIFFS_UI_MOUNT_POINT,
    .partition_label = "spiffs_ui",
    .max_files = 2,
    .format_if_mount_failed = true
};

/// @brief Конфигурация SPIFFS для хранения quirks (правил совместимости устройств)
esp_vfs_spiffs_conf_t zb_manager_spiff_quirks_conf = {
    .base_path = SPIFFS_QUIRKS_MOUNT_POINT,
    .partition_label = "quirks",
    .max_files = 2,
    .format_if_mount_failed = true
};

/// @brief Конфигурация SPIFFS для хранения SSL-сертификатов
esp_vfs_spiffs_conf_t zb_manager_spiff_certs_conf = {
    .base_path = SPIFFS_CERTS_MOUNT_POINT,
    .partition_label = "certs",
    .max_files = 2,
    .format_if_mount_failed = true
};

/// @brief [spiffs_helper.c] Инициализирует все SPIFFS-разделы и записывает встроенные образы при необходимости
/// @return ESP_OK при успехе, ошибка иначе
esp_err_t init_spiffs(void)
{
    esp_err_t ret;

    // === 1. spiffs_config ===
    ret = esp_vfs_spiffs_register(&zb_manager_spiff_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init SPIFFS_CFG: %s", esp_err_to_name(ret));
        return ret;
    }
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(zb_manager_spiff_conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS_CFG info: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SPIFFS_CFG: total=%d, used=%d", total, used);

    // === 2. spiffs_ui ===
    ret = esp_vfs_spiffs_register(&zb_manager_spiff_UI_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init SPIFFS_UI: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_spiffs_info(zb_manager_spiff_UI_conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS_UI info: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SPIFFS_UI: total=%d, used=%d", total, used);

    // === 3. quirks ===
    ret = esp_vfs_spiffs_register(&zb_manager_spiff_quirks_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init QUIRKS: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_spiffs_info(zb_manager_spiff_quirks_conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get QUIRKS info: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "QUIRKS: total=%d, used=%d", total, used);

    // === 4. certs ===
    /*ret = esp_vfs_spiffs_register(&zb_manager_spiff_certs_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init CERTS: %s", esp_err_to_name(ret));
        return ret;
    }
    total = 0, used = 0;
    ret = esp_spiffs_info(zb_manager_spiff_certs_conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get CERTS info: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "CERTS: total=%d, used=%d", total, used);*/

    ESP_LOGI(TAG, "✅ All SPIFFS partitions initialized and data installed");
    return ESP_OK;
}

/// @brief [spiffs_helper.c] Проверяет, смонтирован ли раздел с сертификатами
/// @return true, если раздел смонтирован
bool is_certs_partition_ready(void) {
    return esp_spiffs_mounted(zb_manager_spiff_certs_conf.partition_label);
}
