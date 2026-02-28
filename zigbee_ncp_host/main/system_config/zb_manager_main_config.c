// zb_manager_main_config.c
#include "zb_manager_main_config.h"
#include "esp_log.h"
#include "cJSON.h"
#include "spiffs_helper.h"
#include <string.h>
#include <sys/stat.h>

static const char* TAG = "SYS_CONFIG";



zb_manager_app_main_config_t g_app_config;

static void set_default_config() {
    // HA
    g_app_config.ha_config.ha_enabled = true;
    strcpy(g_app_config.ha_config.mqtt.broker, "core-mosquitto");
    g_app_config.ha_config.mqtt.port = 1883;
    g_app_config.ha_config.mqtt.mqtt_enabled = true;
    g_app_config.ha_config.mqtt.discovery = true;
    g_app_config.ha_config.mqtt.availability = true;
    strcpy(g_app_config.ha_config.mqtt.username, "");
    strcpy(g_app_config.ha_config.mqtt.password, "");
    strcpy(g_app_config.ha_config.language, "ru");

    // Web UI
    strcpy(g_app_config.web_ui_config.theme, "dark");
}

bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

void load_zb_manager_app_main_config() {
    if (!file_exists(CONFIG_FILE_PATH)) {
        ESP_LOGW(TAG, "Config file not found, using defaults: %s", CONFIG_FILE_PATH);
        set_default_config();
        save_zb_manager_app_main_config();
        return;
    }

    FILE* f = fopen(CONFIG_FILE_PATH, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", CONFIG_FILE_PATH);
        set_default_config();
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* data = malloc(len + 1);
    if (!data) {
        fclose(f);
        set_default_config();
        return;
    }

    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON* json = cJSON_Parse(data);
    free(data);

    if (!json) {
        ESP_LOGE(TAG, "Invalid JSON in config");
        set_default_config();
        return;
    }

    cJSON* ha = cJSON_GetObjectItem(json, "ha");
    if (ha) {
        cJSON* item;
        if ((item = cJSON_GetObjectItem(ha, "enabled"))) g_app_config.ha_config.ha_enabled = item->valueint;
        cJSON* mqtt = cJSON_GetObjectItem(ha, "mqtt");
        if (mqtt) {
            if ((item = cJSON_GetObjectItem(mqtt, "enabled"))) g_app_config.ha_config.mqtt.mqtt_enabled = item->valueint;
            if ((item = cJSON_GetObjectItem(mqtt, "broker"))) strncpy(g_app_config.ha_config.mqtt.broker, item->valuestring, 63);
            if ((item = cJSON_GetObjectItem(mqtt, "port"))) g_app_config.ha_config.mqtt.port = item->valueint;
            if ((item = cJSON_GetObjectItem(mqtt, "username"))) strncpy(g_app_config.ha_config.mqtt.username, item->valuestring, 31);
            if ((item = cJSON_GetObjectItem(mqtt, "password"))) strncpy(g_app_config.ha_config.mqtt.password, item->valuestring, 63);
            if ((item = cJSON_GetObjectItem(mqtt, "discovery"))) g_app_config.ha_config.mqtt.discovery = item->valueint;
            if ((item = cJSON_GetObjectItem(mqtt, "availability"))) g_app_config.ha_config.mqtt.availability = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(ha, "language"))) strncpy(g_app_config.ha_config.language, item->valuestring, 7);
    }

    cJSON* web = cJSON_GetObjectItem(json, "web");
    cJSON* item;
    if (web) {
        if ((item = cJSON_GetObjectItem(web, "theme"))) strncpy(g_app_config.web_ui_config.theme, item->valuestring, 7);
    }

    cJSON_Delete(json);
    ESP_LOGI(TAG, "✅ Global config loaded from %s", CONFIG_FILE_PATH);
}

void save_zb_manager_app_main_config() {
    cJSON* json = cJSON_CreateObject();

    cJSON* ha = cJSON_CreateObject();
    cJSON_AddBoolToObject(ha, "enabled", g_app_config.ha_config.ha_enabled);

    cJSON* mqtt = cJSON_CreateObject();
    cJSON_AddBoolToObject(mqtt, "enabled", g_app_config.ha_config.mqtt.mqtt_enabled);
    cJSON_AddStringToObject(mqtt, "broker", g_app_config.ha_config.mqtt.broker);
    cJSON_AddNumberToObject(mqtt, "port", g_app_config.ha_config.mqtt.port);
    cJSON_AddStringToObject(mqtt, "username", g_app_config.ha_config.mqtt.username);
    cJSON_AddStringToObject(mqtt, "password", g_app_config.ha_config.mqtt.password);
    cJSON_AddBoolToObject(mqtt, "discovery", g_app_config.ha_config.mqtt.discovery);
    cJSON_AddBoolToObject(mqtt, "availability", g_app_config.ha_config.mqtt.availability);
    cJSON_AddItemToObject(ha, "mqtt", mqtt);

    cJSON_AddStringToObject(ha, "language", g_app_config.ha_config.language);
    cJSON_AddItemToObject(json, "ha", ha);

    cJSON* web = cJSON_CreateObject();
    cJSON_AddStringToObject(web, "theme", g_app_config.web_ui_config.theme);
    cJSON_AddItemToObject(json, "web", web);

    char* rendered = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (!rendered) {
        ESP_LOGE(TAG, "Failed to render config JSON");
        return;
    }

    FILE* f = fopen(CONFIG_FILE_PATH, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for write", CONFIG_FILE_PATH);
        free(rendered);
        return;
    }

    fwrite(rendered, 1, strlen(rendered), f);
    fclose(f);
    free(rendered);

    ESP_LOGI(TAG, "✅ Global config saved to %s", CONFIG_FILE_PATH);
}
