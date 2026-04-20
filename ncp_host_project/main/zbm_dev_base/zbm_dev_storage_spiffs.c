// main/zbm_dev_storage_spiffs.c — чистый C (без лямбд)

#include "zbm_dev_storage_spiffs.h"
#include "zbm_device_db.h"
#include "zbm_dev_types.h"
#include "zbm_core_sync.h"
#include "zbm_spiffs_helper.h"
#include "cJSON.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "zbm_dev_from_json.h"

static const char* TAG = "DEV_STORAGE";

#define DEVICE_INDEX_PATH ZBM_DEV_INDEX_FILE
#define DEVICE_FILE_FORMAT SPIFFS_ZBM_CONF_MOUNT_POINT "/dev_0x%04X.json"


static uint8_t hex_char_to_byte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

static void* parse_hex_string(const char* hex, uint16_t size) {
    void* data = calloc(1, size);
    if (!data) return NULL;
    for (int i = 0; i < size; i++) {
        ((uint8_t*)data)[i] = (hex_char_to_byte(hex[i*2]) << 4) | hex_char_to_byte(hex[i*2+1]);
    }
    return data;
}
// === Форматирование IEEE в строку ===
static void format_ieee_str(char* buf, size_t size, const uint8_t* ieee) {
    snprintf(buf, size, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             ieee[0], ieee[1], ieee[2], ieee[3],
             ieee[4], ieee[5], ieee[6], ieee[7]);
}






void zbm_load_all_devices_from_spiffs_and_restore(void) {
    ESP_LOGI(TAG, "🔁 Starting SPIFFS load and restore...");

    // === 1. Восстановление устройств из индекса ===
    cJSON* index = read_json_from_file(DEVICE_INDEX_PATH);
    if (!index || !cJSON_IsArray(index)) {
        ESP_LOGI(TAG, "No valid device index found. Skipping device restore.");
        cJSON_Delete(index);
    } else {
        int count = cJSON_GetArraySize(index);
        ESP_LOGI(TAG, "Found %d devices in index. Restoring...", count);

        for (int i = 0; i < count; i++) {
            cJSON* item = cJSON_GetArrayItem(index, i);
            cJSON* ieee_item = cJSON_GetObjectItem(item, "ieee_addr");
            cJSON* file_item = cJSON_GetObjectItem(item, "file");

            if (!cJSON_IsString(ieee_item) || !cJSON_IsString(file_item)) {
                continue;
            }

            // Путь к файлу
            char path[64];
            snprintf(path, sizeof(path), SPIFFS_ZBM_CONF_MOUNT_POINT "/%s", file_item->valuestring);

            if (!spiffs_file_exists(path)) {
                ESP_LOGW(TAG, "File missing: %s", path);
                continue;
            }

            // Загружаем JSON
            cJSON* json = read_json_from_file(path);
            if (!json) {
                ESP_LOGE(TAG, "Failed to parse: %s", path);
                continue;
            }

            // Проверяем, нет ли уже такого устройства
            uint8_t ieee[8];
            int scanned = sscanf(ieee_item->valuestring, "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX",
                                 &ieee[0], &ieee[1], &ieee[2], &ieee[3],
                                 &ieee[4], &ieee[5], &ieee[6], &ieee[7]);
            if (scanned != 8) {
                cJSON_Delete(json);
                continue;
            }

            zbm_dev_t* existing = zbm_find_device_in_devdb_by_ieee_safe(ieee);
            if (existing) {
                ESP_LOGD(TAG, "Device already in runtime: %s → skip restore", ieee_item->valuestring);
                cJSON_Delete(json);
                continue;
            }

            // Восстанавливаем
            zbm_dev_t* restored = restore_device_from_json(json);
            if (restored) {
                ESP_LOGI(TAG, "✅ Restored device from SPIFFS: %s (short=0x%04X)", ieee_item->valuestring, restored->short_addr);
                //zbm_guid_db_dump_all();
            } else {
                ESP_LOGE(TAG, "❌ Failed to restore device: %s", ieee_item->valuestring);
            }

            cJSON_Delete(json);
        }
        cJSON_Delete(index);
    }

    // === 2. Загрузка или создание координатора по умолчанию ===
zbm_coordinator_t* loaded_coord = zbm_load_coordinator_from_spiffs();
if (loaded_coord) {
    // --- Zigbee параметры ---
    memcpy(&zbm_coordinator.zb_ieee_addr, loaded_coord->zb_ieee_addr, 8);
    zbm_coordinator.zb_pan_id = loaded_coord->zb_pan_id;
    zbm_coordinator.zb_radio_channel = loaded_coord->zb_radio_channel;

    // friendly_name
    if (zbm_coordinator.friendly_name) {
        free(zbm_coordinator.friendly_name);
    }
    zbm_coordinator.friendly_name = loaded_coord->friendly_name ? strdup(loaded_coord->friendly_name) : NULL;

    // Endpoint
    zbm_coordinator.zb_endpoint.endpoint = loaded_coord->zb_endpoint.endpoint;
    zbm_coordinator.zb_endpoint.profileId = loaded_coord->zb_endpoint.profileId;
    zbm_coordinator.zb_endpoint.deviceId = loaded_coord->zb_endpoint.deviceId;
    zbm_coordinator.zb_endpoint.appFlags = loaded_coord->zb_endpoint.appFlags;

    // Input clusters
    if (zbm_coordinator.zb_endpoint.inputClusterList) {
        free(zbm_coordinator.zb_endpoint.inputClusterList);
    }
    if (loaded_coord->zb_endpoint.inputClusterList && loaded_coord->zb_endpoint.inputClusterCount > 0) {
        size_t size = loaded_coord->zb_endpoint.inputClusterCount * sizeof(uint16_t);
        zbm_coordinator.zb_endpoint.inputClusterList = malloc(size);
        if (zbm_coordinator.zb_endpoint.inputClusterList) {
            memcpy(zbm_coordinator.zb_endpoint.inputClusterList, loaded_coord->zb_endpoint.inputClusterList, size);
            zbm_coordinator.zb_endpoint.inputClusterCount = loaded_coord->zb_endpoint.inputClusterCount;
        }
    } else {
        zbm_coordinator.zb_endpoint.inputClusterList = NULL;
        zbm_coordinator.zb_endpoint.inputClusterCount = 0;
    }

    // Output clusters
    if (zbm_coordinator.zb_endpoint.outputClusterList) {
        free(zbm_coordinator.zb_endpoint.outputClusterList);
    }
    if (loaded_coord->zb_endpoint.outputClusterList && loaded_coord->zb_endpoint.outputClusterCount > 0) {
        size_t size = loaded_coord->zb_endpoint.outputClusterCount * sizeof(uint16_t);
        zbm_coordinator.zb_endpoint.outputClusterList = malloc(size);
        if (zbm_coordinator.zb_endpoint.outputClusterList) {
            memcpy(zbm_coordinator.zb_endpoint.outputClusterList, loaded_coord->zb_endpoint.outputClusterList, size);
            zbm_coordinator.zb_endpoint.outputClusterCount = loaded_coord->zb_endpoint.outputClusterCount;
        }
    } else {
        zbm_coordinator.zb_endpoint.outputClusterList = NULL;
        zbm_coordinator.zb_endpoint.outputClusterCount = 0;
    }

    // --- Wi-Fi AP настройки ---
    if (zbm_coordinator.wifi_ap_ssid) {
        free(zbm_coordinator.wifi_ap_ssid);
    }
    if (zbm_coordinator.wifi_ap_password) {
        free(zbm_coordinator.wifi_ap_password);
    }
    zbm_coordinator.wifi_ap_ssid = loaded_coord->wifi_ap_ssid ? strdup(loaded_coord->wifi_ap_ssid) : NULL;
    zbm_coordinator.wifi_ap_password = loaded_coord->wifi_ap_password ? strdup(loaded_coord->wifi_ap_password) : NULL;
    zbm_coordinator.wifi_ap_channel = loaded_coord->wifi_ap_channel;
    zbm_coordinator.wifi_ap_max_connections = loaded_coord->wifi_ap_max_connections;

    // --- Wi-Fi STA настройки ---
    if (zbm_coordinator.wifi_sta_ssid) {
        free(zbm_coordinator.wifi_sta_ssid);
    }
    if (zbm_coordinator.wifi_sta_password) {
        free(zbm_coordinator.wifi_sta_password);
    }
    zbm_coordinator.wifi_sta_ssid = loaded_coord->wifi_sta_ssid ? strdup(loaded_coord->wifi_sta_ssid) : NULL;
    zbm_coordinator.wifi_sta_password = loaded_coord->wifi_sta_password ? strdup(loaded_coord->wifi_sta_password) : NULL;
    zbm_coordinator.wifi_sta_channel = loaded_coord->wifi_sta_channel;

    zbm_coordinator.hostname = loaded_coord->hostname ? strdup(loaded_coord->hostname) : NULL;

    // Освобождаем временный объект
    zbm_free_coordinator(loaded_coord);

    ESP_LOGI(TAG, "✅ Coordinator loaded from SPIFFS with Wi-Fi settings");
    
    ESP_LOGI(TAG, "   🔹 Zigbee PAN ID: 0x%04X, Channel: %u", zbm_coordinator.zb_pan_id, zbm_coordinator.zb_radio_channel);
    ESP_LOGI(TAG, "   🔹 Friendly name: %s", zbm_coordinator.friendly_name ? zbm_coordinator.friendly_name : "null");

    // === Логируем AP настройки ===
    ESP_LOGI(TAG, "   📶 Wi-Fi AP SSID: %s", zbm_coordinator.wifi_ap_ssid ? zbm_coordinator.wifi_ap_ssid : "null");
    if (zbm_coordinator.wifi_ap_password && strlen(zbm_coordinator.wifi_ap_password) > 0) {
        ESP_LOGI(TAG, "   🔐 Wi-Fi AP password: ******** (%d chars)", strlen(zbm_coordinator.wifi_ap_password));
    } else {
        ESP_LOGW(TAG, "   ⚠️  Wi-Fi AP has no password (open network)");
    }

    ESP_LOGI(TAG, "   🔹 Wi-Fi AP channel: %u, Max connections: %u", 
            zbm_coordinator.wifi_ap_channel, zbm_coordinator.wifi_ap_max_connections);

    // === Логируем STA настройки ===
    if (zbm_coordinator.wifi_sta_ssid && strlen(zbm_coordinator.wifi_sta_ssid) > 0) {
        ESP_LOGI(TAG, "   🌐 Wi-Fi STA SSID: %s", zbm_coordinator.wifi_sta_ssid);
        if (zbm_coordinator.wifi_sta_password && strlen(zbm_coordinator.wifi_sta_password) > 0) {
            ESP_LOGI(TAG, "   🔐 Wi-Fi STA password: ******** (%d chars)", strlen(zbm_coordinator.wifi_sta_password));
        } else {
            ESP_LOGW(TAG, "   ⚠️  Wi-Fi STA password not set!");
        }
        ESP_LOGI(TAG, "   🔹 Wi-Fi STA channel: %u", zbm_coordinator.wifi_sta_channel);
    } else {
        ESP_LOGI(TAG, "   🌐 Wi-Fi STA SSID: not configured (AP mode only)");
    }
} else {
    // === Создаём координатор по умолчанию ===
    ESP_LOGI(TAG, "🔧 No coordinator found. Creating default configuration.");

    zbm_coordinator.zb_short_address = 0x0000;
    zbm_coordinator.zb_pan_id = 0x1A62;  // Пример: 6754
    zbm_coordinator.zb_radio_channel = 11;

    uint8_t default_ieee[8] = {0x00, 0x12, 0x4B, 0x00, 0x00, 0x00, 0x00, 0x01};
    memcpy(zbm_coordinator.zb_ieee_addr, default_ieee, 8);

    uint8_t default_ext_pan[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x00};
    memcpy(zbm_coordinator.zb_extended_pan_id, default_ext_pan, 8);

    if (zbm_coordinator.friendly_name) {
        free(zbm_coordinator.friendly_name);
    }
    zbm_coordinator.friendly_name = strdup("Zigbee Coordinator");

    // Endpoint: используем стандартные списки
    extern uint16_t inputClusterEP1[];
    extern uint16_t outputClusterEP1[];
    extern esp_host_zb_endpoint_t host_endpoint1;

    zbm_coordinator.zb_endpoint.endpoint = host_endpoint1.endpoint;
    zbm_coordinator.zb_endpoint.profileId = host_endpoint1.profileId;
    zbm_coordinator.zb_endpoint.deviceId = host_endpoint1.deviceId;
    zbm_coordinator.zb_endpoint.appFlags = host_endpoint1.appFlags;

    // Input clusters
    zbm_coordinator.zb_endpoint.inputClusterCount = host_endpoint1.inputClusterCount;
    size_t in_size = host_endpoint1.inputClusterCount * sizeof(uint16_t);
    zbm_coordinator.zb_endpoint.inputClusterList = malloc(in_size);
    if (zbm_coordinator.zb_endpoint.inputClusterList) {
        memcpy(zbm_coordinator.zb_endpoint.inputClusterList, inputClusterEP1, in_size);
    }

    // Output clusters
    zbm_coordinator.zb_endpoint.outputClusterCount = host_endpoint1.outputClusterCount;
    size_t out_size = host_endpoint1.outputClusterCount * sizeof(uint16_t);
    zbm_coordinator.zb_endpoint.outputClusterList = malloc(out_size);
    if (zbm_coordinator.zb_endpoint.outputClusterList) {
        memcpy(zbm_coordinator.zb_endpoint.outputClusterList, outputClusterEP1, out_size);
    }

    // === Wi-Fi по умолчанию ===
    zbm_coordinator.wifi_ap_ssid = strdup("Zigbee-Gateway-Setup");
    zbm_coordinator.wifi_ap_password = strdup("12345678");
    zbm_coordinator.wifi_ap_channel = 6;
    zbm_coordinator.wifi_ap_max_connections = 4;

    zbm_coordinator.wifi_sta_ssid = NULL;
    zbm_coordinator.wifi_sta_password = NULL;
    zbm_coordinator.wifi_sta_channel = 0;

    // Сохраняем дефолтный координатор
    if (zbm_save_coordinator_to_spiffs(&zbm_coordinator)) {
        ESP_LOGI(TAG, "✅ Default coordinator saved to SPIFFS");
    } else {
        ESP_LOGE(TAG, "❌ Failed to save default coordinator");
    }
}

ESP_LOGI(TAG, "✅ SPIFFS restore completed.");
}

void zbm_load_all_devices_from_spiffs_and_restore_old(void) {
    ESP_LOGI(TAG, "🔁 Starting SPIFFS load and restore...");

    cJSON* index = read_json_from_file(DEVICE_INDEX_PATH);
    if (!index || !cJSON_IsArray(index)) {
        ESP_LOGI(TAG, "No valid device index found. Skipping restore.");
        cJSON_Delete(index);
        return;
    }

    int count = cJSON_GetArraySize(index);
    ESP_LOGI(TAG, "Found %d devices in index. Restoring...", count);

    for (int i = 0; i < count; i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        cJSON* ieee_item = cJSON_GetObjectItem(item, "ieee_addr");
        cJSON* file_item = cJSON_GetObjectItem(item, "file");

        if (!cJSON_IsString(ieee_item) || !cJSON_IsString(file_item)) {
            continue;
        }

        // Путь к файлу
        char path[64];
        snprintf(path, sizeof(path), SPIFFS_ZBM_CONF_MOUNT_POINT "/%s", file_item->valuestring);

        if (!spiffs_file_exists(path)) {
            ESP_LOGW(TAG, "File missing: %s", path);
            continue;
        }

        // Загружаем JSON
        cJSON* json = read_json_from_file(path);
        if (!json) {
            ESP_LOGE(TAG, "Failed to parse: %s", path);
            continue;
        }

        // Проверяем, нет ли уже такого устройства
        uint8_t ieee[8];
        int scanned = sscanf(ieee_item->valuestring, "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX",
                             &ieee[0], &ieee[1], &ieee[2], &ieee[3],
                             &ieee[4], &ieee[5], &ieee[6], &ieee[7]);
        if (scanned != 8) {
            cJSON_Delete(json);
            continue;
        }

        zbm_dev_t* existing = zbm_find_device_in_devdb_by_ieee_safe(ieee);
        if (existing) {
            ESP_LOGD(TAG, "Device already in runtime: %s → skip restore", ieee_item->valuestring);
            cJSON_Delete(json);
            continue;
        }

        // Восстанавливаем
        zbm_dev_t* restored = restore_device_from_json(json);
        if (restored) {
            ESP_LOGI(TAG, "✅ Restored device from SPIFFS: %s (short=0x%04X)", ieee_item->valuestring, restored->short_addr);
        } else {
            ESP_LOGE(TAG, "❌ Failed to restore device: %s", ieee_item->valuestring);
        }

        cJSON_Delete(json);
    }

    cJSON_Delete(index);
    ESP_LOGI(TAG, "✅ SPIFFS restore completed.");
}



// === Получение имени файла по IEEE ===
static void get_filename_by_ieee(char* buf, size_t size, const uint8_t* ieee) {
    char ieee_clean[24];
    format_ieee_str(ieee_clean, sizeof(ieee_clean), ieee);
    // Заменяем : на _
    for (char* c = ieee_clean; *c; ++c) {
        if (*c == ':') {
            *c = '_';
        }
    }
    snprintf(buf, size, SPIFFS_ZBM_CONF_MOUNT_POINT "/dev_%s.json", ieee_clean);
}






// === Удаление старого файла устройства по IEEE ===
static void remove_device_file_by_ieee(const uint8_t* ieee_addr) {
    char old_path[64];
    get_filename_by_ieee(old_path, sizeof(old_path), ieee_addr);
    if (spiffs_file_exists(old_path)) {
        remove(old_path);
        ESP_LOGI(TAG, "Removed old device file: %s", old_path);
    }
}

// === Вспомогательная функция для foreach: собирает объекты в массив ===
static void collect_device_for_index(zbm_dev_t* dev, void* ctx) {
    cJSON* index_array = (cJSON*)ctx;

    cJSON* obj = cJSON_CreateObject();
    if (!obj) return;

    cJSON_AddNumberToObject(obj, "short_addr", dev->short_addr);

    char ieee_str[24];
    format_ieee_str(ieee_str, sizeof(ieee_str), dev->ieee_addr);
    cJSON_AddStringToObject(obj, "ieee_addr", ieee_str);

    char filename[64];
    snprintf(filename, sizeof(filename), "dev_0x%04X.json", dev->short_addr);
    cJSON_AddStringToObject(obj, "file", filename);

    cJSON_AddItemToArray(index_array, obj);
}

// === Обновление индекса ===
static bool update_devices_index(void) {
    cJSON* index_array = cJSON_CreateArray();
    if (!index_array) {
        return false;
    }

    zbm_device_db_foreach(collect_device_for_index, index_array);

    // Используем .tmp в той же папке
    char tmp_path[64];
    snprintf(tmp_path, sizeof(tmp_path), "%s/.tmp_zbm_index.json", SPIFFS_ZBM_CONF_MOUNT_POINT);

    bool success = write_json_to_file(tmp_path, index_array);
    cJSON_Delete(index_array);

    if (!success) {
        ESP_LOGE(TAG, "Failed to write temporary index file: %s", tmp_path);
        return false;
    }

    // Удаляем старый индекс, если существует
    if (spiffs_file_exists(DEVICE_INDEX_PATH)) {
        if (remove(DEVICE_INDEX_PATH) != 0) {
            ESP_LOGE(TAG, "Failed to remove old index: %s", DEVICE_INDEX_PATH);
            remove(tmp_path);  // чистим временный
            return false;
        }
    }

    // Теперь пробуем переименовать
    if (rename(tmp_path, DEVICE_INDEX_PATH) != 0) {
        ESP_LOGE(TAG, "Failed to rename temp index to %s", DEVICE_INDEX_PATH);
        remove(tmp_path);
        return false;
    }

    ESP_LOGI(TAG, "Device index updated");
    return true;
}

// === Сохранение устройства ===
bool zbm_save_device_to_spiffs_safe(zbm_dev_t* dev) {
    if (!dev || dev->short_addr == ZBM_ADDR_UNKNOWN) {
        ESP_LOGW(TAG, "Cannot save: invalid device or unknown short addr");
        return false;
    }

    zbm_core_sync_lock();

    // Путь к новому файлу
    char new_path[64];
    snprintf(new_path, sizeof(new_path), DEVICE_FILE_FORMAT, dev->short_addr);

    // Удаляем старый файл, если IEEE уже был сохранён под другим short_addr
    remove_device_file_by_ieee(dev->ieee_addr);

    // Сериализация
    cJSON* json = device_to_json(dev);
    if (!json) {
        zbm_core_sync_unlock();
        ESP_LOGE(TAG, "Failed to serialize device 0x%04X", dev->short_addr);
        return false;
    }

    bool saved = write_json_to_file(new_path, json);
    cJSON_Delete(json);

    if (saved) {
        saved = update_devices_index(); // обновляем индекс
        if (!saved) {
            ESP_LOGE(TAG, "Failed to update devices index");
        }
    } else {
        ESP_LOGE(TAG, "Failed to save device file: %s", new_path);
    }

    zbm_core_sync_unlock();
    return saved;
}


// Вспомогательная функция для foreach: сохраняет одно устройство
static void save_device_foreach(zbm_dev_t* dev, void* ctx) {
    bool* success = (bool*)ctx;
    if (!zbm_save_device_to_spiffs_safe(dev)) {
        ESP_LOGE(TAG, "Failed to save device in bulk: 0x%04X", dev->short_addr);
        *success = false;
    }
}

// === Сохранение всех устройств в SPIFFS ===
bool zbm_save_all_devices_to_spiffs_safe(void) {
    ESP_LOGI(TAG, "🔁 Saving all devices to SPIFFS...");

    zbm_core_sync_lock();

    // Проверим, есть ли устройства вообще
    size_t dev_count = zbm_device_db_count();
    if (dev_count == 0) {
        ESP_LOGI(TAG, "🟡 No devices to save — device database is empty");
        zbm_core_sync_unlock();
        return true; // Это не ошибка! Просто нечего сохранять
    }

    bool success = true;
    zbm_device_db_foreach(save_device_foreach, &success);

    if (success) {
        ESP_LOGI(TAG, "✅ All %zu devices saved successfully", dev_count);
    } else {
        ESP_LOGE(TAG, "❌ Failed to save one or more devices");
    }

    zbm_core_sync_unlock();
    return success;
}

// === Загрузка устройства по short_addr ===
cJSON* zbm_load_device_json_by_short_safe(uint16_t short_addr) {
    if (short_addr == ZBM_ADDR_UNKNOWN) {
        return NULL;
    }

    char path[64];
    snprintf(path, sizeof(path), DEVICE_FILE_FORMAT, short_addr);
    return read_json_from_file(path);
}

// === Загрузка устройства по IEEE ===
static cJSON* zbm_load_device_json_by_ieee(const uint8_t* ieee_addr) {
    if (!ieee_addr) {
        return NULL;
    }

    // Пробуем найти по имени файла IEEE
    /*char ieee_path[64];
    get_filename_by_ieee(ieee_path, sizeof(ieee_path), ieee_addr);
    if (spiffs_file_exists(ieee_path)) {
        ESP_LOGI(TAG, "Found device by IEEE path: %s", ieee_path);
        return read_json_from_file(ieee_path);
    }*/

    // Ищем в индексе
    cJSON* index = read_json_from_file(DEVICE_INDEX_PATH);
    if (!index || !cJSON_IsArray(index)) {
        cJSON_Delete(index);
        return NULL;
    }

    cJSON* result = NULL;
    char ieee_str[24];
    format_ieee_str(ieee_str, sizeof(ieee_str), ieee_addr);

    int count = cJSON_GetArraySize(index);
    for (int i = 0; i < count; i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        cJSON* stored_ieee_item = cJSON_GetObjectItem(item, "ieee_addr");
        cJSON* filename_item = cJSON_GetObjectItem(item, "file");

        if (!cJSON_IsString(stored_ieee_item) || !cJSON_IsString(filename_item)) {
            continue;
        }

        if (strcmp(stored_ieee_item->valuestring, ieee_str) == 0) {
            char path[64];
            snprintf(path, sizeof(path), SPIFFS_ZBM_CONF_MOUNT_POINT "/%s", filename_item->valuestring);
            result = read_json_from_file(path);
            break;
        }
    }

    cJSON_Delete(index);
    return result;
}

cJSON* zbm_load_device_json_by_ieee_safe(const uint8_t* ieee_addr) {
    if (!ieee_addr) return NULL;

    zbm_core_sync_lock();
    cJSON* result = zbm_load_device_json_by_ieee(ieee_addr); // внутренняя версия
    zbm_core_sync_unlock();
    return result;
}

// === Загрузка всех устройств из индекса ===
/*void zbm_load_all_devices_from_spiffs(void) {
    cJSON* index = read_json_from_file(DEVICE_INDEX_PATH);
    if (!index || !cJSON_IsArray(index)) {
        ESP_LOGI(TAG, "No valid device index found");
        cJSON_Delete(index);
        return;
    }

    int count = cJSON_GetArraySize(index);
    ESP_LOGI(TAG, "Loading %d devices from index", count);

    for (int i = 0; i < count; i++) {
        cJSON* item = cJSON_GetArrayItem(index, i);
        cJSON* short_addr_item = cJSON_GetObjectItem(item, "short_addr");
        cJSON* filename_item = cJSON_GetObjectItem(item, "file");

        if (!cJSON_IsNumber(short_addr_item) || !cJSON_IsString(filename_item)) {
            continue;
        }

        uint16_t short_addr = (uint16_t)short_addr_item->valuedouble;
        const char* filename = filename_item->valuestring;

        char path[64];
        snprintf(path, sizeof(path), SPIFFS_ZBM_CONF_MOUNT_POINT "/%s", filename);

        if (!spiffs_file_exists(path)) {
            ESP_LOGW(TAG, "Missing device file: %s", path);
            continue;
        }

        // Здесь можно вызвать восстановление модели из JSON
        ESP_LOGD(TAG, "Device file present: %s", path);
    }

    cJSON_Delete(index);
}*/