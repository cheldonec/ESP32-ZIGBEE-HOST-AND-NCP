#include "zbm_coordinator.h"
#include "zbm_coordinator.h"
#include "zbm_spiffs_helper.h"
#include "cJSON.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char* TAG = "ZBM_COORD";

#define COORDINATOR_FILE_PATH SPIFFS_ZBM_CONF_MOUNT_POINT "/coordinator_0x0000.json"



uint16_t inputClusterEP1[] = {0x0000, 0x0001, 0x0003, 0x0004, 0x0005, 0x0006, 0x0402, 0x0405, 0xEF00};
    uint16_t outputClusterEP1[] = {0x0000, 0x0001, 0x0003, 0x0004, 0x0005, 0x0006, 0x0402, 0x0405, 0xEF00};
    esp_host_zb_endpoint_t host_endpoint1 = {
        .endpoint = 1,
        .profileId = 0x0104U,                           //HA profile ID
        .deviceId = ZBM_DEVICE_TYPE_REMOTE_CONTROL,
        .appFlags = 0,
        .inputClusterCount = sizeof(inputClusterEP1) / sizeof(inputClusterEP1[0]),
        .inputClusterList = inputClusterEP1,
        .outputClusterCount = sizeof(outputClusterEP1) / sizeof(outputClusterEP1[0]),
        .outputClusterList = outputClusterEP1,
    };

zbm_coordinator_t zbm_coordinator;

// === 1. Сериализация координатора в JSON ===
cJSON* zbm_coordinator_to_json(const zbm_coordinator_t* coord) {
    if (!coord) return NULL;

    cJSON* json = cJSON_CreateObject();
    if (!json) return NULL;

    if (coord->friendly_name) {
        cJSON_AddStringToObject(json, "friendly_name", coord->friendly_name);
    }

    cJSON_AddNumberToObject(json, "short_addr", coord->zb_short_address);
    cJSON_AddNumberToObject(json, "pan_id", coord->zb_pan_id);
    cJSON_AddNumberToObject(json, "radio_channel", coord->zb_radio_channel);

    // IEEE адрес
    char ieee_str[24];
    snprintf(ieee_str, sizeof(ieee_str), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             coord->zb_ieee_addr[0], coord->zb_ieee_addr[1], coord->zb_ieee_addr[2],
             coord->zb_ieee_addr[3], coord->zb_ieee_addr[4], coord->zb_ieee_addr[5],
             coord->zb_ieee_addr[6], coord->zb_ieee_addr[7]);
    cJSON_AddStringToObject(json, "ieee_addr", ieee_str);

    // Extended PAN ID
    char ext_pan_str[24];
    snprintf(ext_pan_str, sizeof(ext_pan_str), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             coord->zb_extended_pan_id[0], coord->zb_extended_pan_id[1], coord->zb_extended_pan_id[2],
             coord->zb_extended_pan_id[3], coord->zb_extended_pan_id[4], coord->zb_extended_pan_id[5],
             coord->zb_extended_pan_id[6], coord->zb_extended_pan_id[7]);
    cJSON_AddStringToObject(json, "ext_pan_id", ext_pan_str);

    // Endpoint (упрощённо)
    cJSON* ep = cJSON_CreateObject();
    if (ep) {
        cJSON_AddNumberToObject(ep, "endpoint", coord->zb_endpoint.endpoint);
        cJSON_AddNumberToObject(ep, "profile_id", coord->zb_endpoint.profileId);
        cJSON_AddNumberToObject(ep, "device_id", coord->zb_endpoint.deviceId);
        cJSON_AddNumberToObject(ep, "app_flags", coord->zb_endpoint.appFlags);
        cJSON_AddNumberToObject(ep, "input_cluster_count", coord->zb_endpoint.inputClusterCount);
        cJSON_AddNumberToObject(ep, "output_cluster_count", coord->zb_endpoint.outputClusterCount);

        // Input clusters
        if (coord->zb_endpoint.inputClusterList && coord->zb_endpoint.inputClusterCount > 0) {
            int* temp = malloc(coord->zb_endpoint.inputClusterCount * sizeof(int));
            if (temp) {
                for (int i = 0; i < coord->zb_endpoint.inputClusterCount; i++) {
                    temp[i] = (int)(coord->zb_endpoint.inputClusterList[i]);
                }
                cJSON* in_clusters = cJSON_CreateIntArray(temp, coord->zb_endpoint.inputClusterCount);
                cJSON_AddItemToObject(ep, "input_clusters", in_clusters);
                free(temp);
            }
        }

        // Output clusters
        if (coord->zb_endpoint.outputClusterList && coord->zb_endpoint.outputClusterCount > 0) {
            int* temp = malloc(coord->zb_endpoint.outputClusterCount * sizeof(int));
            if (temp) {
                for (int i = 0; i < coord->zb_endpoint.outputClusterCount; i++) {
                    temp[i] = (int)(coord->zb_endpoint.outputClusterList[i]);
                }
                cJSON* out_clusters = cJSON_CreateIntArray(temp, coord->zb_endpoint.outputClusterCount);
                cJSON_AddItemToObject(ep, "output_clusters", out_clusters);
                free(temp);
            }
        }

        cJSON_AddItemToObject(json, "endpoint", ep);

        // === Wi-Fi AP ===
        cJSON_AddStringToObject(json, "wifi_ap_ssid", coord->wifi_ap_ssid ? coord->wifi_ap_ssid : "Zigbee-Gateway-Setup");
        cJSON_AddStringToObject(json, "wifi_ap_password", coord->wifi_ap_password ? coord->wifi_ap_password : "12345678");
        cJSON_AddNumberToObject(json, "wifi_ap_channel", coord->wifi_ap_channel);
        cJSON_AddNumberToObject(json, "wifi_ap_max_connections", coord->wifi_ap_max_connections);
        cJSON_AddNumberToObject(json, "wifi_ap_channel", coord->wifi_ap_channel);
        cJSON_AddNumberToObject(json, "wifi_ap_max_connections", coord->wifi_ap_max_connections);

        // === Wi-Fi STA ===
        cJSON_AddStringToObject(json, "wifi_sta_ssid", coord->wifi_sta_ssid ? coord->wifi_sta_ssid : "");

        cJSON_AddStringToObject(json, "wifi_sta_password", coord->wifi_sta_password  ? coord->wifi_sta_password : "");
        
        cJSON_AddNumberToObject(json, "wifi_sta_channel", coord->wifi_sta_channel);

        cJSON_AddNumberToObject(json, "is_sta_valid", coord->is_sta_valid ? 1 : 0);

        if (coord->wifi_mode == ZBM_COORDINATOR_WIFI_MODE_AP) {
            cJSON_AddStringToObject(json, "wifi_mode", "ap");
        }else {
            cJSON_AddStringToObject(json, "wifi_mode", "sta");
        }

        // === SSDP ===
        //cJSON_AddStringToObject(json, "ssdp_friendly_name", coord->ssdp_friendly_name ? coord->ssdp_friendly_name : "ESP32 Zigbee Gateway");
        cJSON_AddStringToObject(json, "ssdp_manufacturer", coord->ssdp_manufacturer ? coord->ssdp_manufacturer : "CheldonecCo");
        cJSON_AddStringToObject(json, "ssdp_model_name", coord->ssdp_model_name ? coord->ssdp_model_name : "Zigbee NCP Host");
        cJSON_AddStringToObject(json, "ssdp_model_number", coord->ssdp_model_number ? coord->ssdp_model_number : "1.0");
        cJSON_AddStringToObject(json, "ssdp_serial_number", coord->ssdp_serial_number ? coord->ssdp_serial_number : "00000001");
        cJSON_AddStringToObject(json, "ssdp_server_name", coord->ssdp_server_name ? coord->ssdp_server_name : "Linux/ESP32 UPnP/1.1 ZBM-GW/1.0");
        cJSON_AddStringToObject(json, "ssdp_schema_url", coord->ssdp_schema_url ? coord->ssdp_schema_url : "/description.xml");
        cJSON_AddStringToObject(json, "ssdp_presentation_url", coord->ssdp_presentation_url ? coord->ssdp_presentation_url : "/");
        cJSON_AddStringToObject(json, "hostname", coord->hostname ? coord->hostname : "esp32-zigbee");
        
    }

    return json;
}

// === 2. Десериализация координатора из JSON ===
zbm_coordinator_t* zbm_coordinator_from_json(const cJSON* json) {
    if (!json) return NULL;

    zbm_coordinator_t* coord = calloc(1, sizeof(zbm_coordinator_t));
    if (!coord) return NULL;

    coord->zb_short_address = 0x0000; // По определению

    // friendly_name
    cJSON* name_item = cJSON_GetObjectItem(json, "friendly_name");
    if (cJSON_IsString(name_item) && name_item->valuestring) {
        coord->friendly_name = strdup(name_item->valuestring);
    }

    // short_addr (должен быть 0x0000)
    cJSON* addr_item = cJSON_GetObjectItem(json, "short_addr");
    if (cJSON_IsNumber(addr_item)) {
        coord->zb_short_address = (uint16_t)addr_item->valuedouble;
        if (coord->zb_short_address != 0x0000) {
            ESP_LOGW(TAG, "Warning: coordinator short address is not 0x0000, but 0x%04X", coord->zb_short_address);
        }
    }

    // pan_id
    cJSON* pan_item = cJSON_GetObjectItem(json, "pan_id");
    if (cJSON_IsNumber(pan_item)) {
        coord->zb_pan_id = (uint16_t)pan_item->valuedouble;
    }

    // radio_channel
    cJSON* chan_item = cJSON_GetObjectItem(json, "radio_channel");
    if (cJSON_IsNumber(chan_item)) {
        coord->zb_radio_channel = (uint8_t)chan_item->valuedouble;
    }

    // IEEE
    cJSON* ieee_item = cJSON_GetObjectItem(json, "ieee_addr");
    if (cJSON_IsString(ieee_item) && ieee_item->valuestring) {
        int scanned = sscanf(ieee_item->valuestring, "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX",
                             &coord->zb_ieee_addr[0], &coord->zb_ieee_addr[1], &coord->zb_ieee_addr[2],
                             &coord->zb_ieee_addr[3], &coord->zb_ieee_addr[4], &coord->zb_ieee_addr[5],
                             &coord->zb_ieee_addr[6], &coord->zb_ieee_addr[7]);
        if (scanned != 8) {
            ESP_LOGE(TAG, "Failed to parse IEEE address: %s", ieee_item->valuestring);
            free(coord);
            return NULL;
        }
    }

    // Extended PAN ID
    cJSON* ext_pan_item = cJSON_GetObjectItem(json, "ext_pan_id");
    if (cJSON_IsString(ext_pan_item) && ext_pan_item->valuestring) {
        int scanned = sscanf(ext_pan_item->valuestring, "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX",
                             &coord->zb_extended_pan_id[0], &coord->zb_extended_pan_id[1], &coord->zb_extended_pan_id[2],
                             &coord->zb_extended_pan_id[3], &coord->zb_extended_pan_id[4], &coord->zb_extended_pan_id[5],
                             &coord->zb_extended_pan_id[6], &coord->zb_extended_pan_id[7]);
        if (scanned != 8) {
            ESP_LOGE(TAG, "Failed to parse Ext PAN ID: %s", ext_pan_item->valuestring);
            free(coord);
            return NULL;
        }
    }

    // Endpoint
    cJSON* ep_obj = cJSON_GetObjectItem(json, "endpoint");
    if (cJSON_IsObject(ep_obj)) {
        cJSON* ep_num = cJSON_GetObjectItem(ep_obj, "endpoint");
        if (cJSON_IsNumber(ep_num)) {
            coord->zb_endpoint.endpoint = (uint8_t)ep_num->valuedouble;
        }

        cJSON* profile_id = cJSON_GetObjectItem(ep_obj, "profile_id");
        if (cJSON_IsNumber(profile_id)) {
            coord->zb_endpoint.profileId = (uint16_t)profile_id->valuedouble;
        }

        cJSON* dev_id = cJSON_GetObjectItem(ep_obj, "device_id");
        if (cJSON_IsNumber(dev_id)) {
            coord->zb_endpoint.deviceId = (uint16_t)dev_id->valuedouble;
        }

        cJSON* app_flags = cJSON_GetObjectItem(ep_obj, "app_flags");
        if (cJSON_IsNumber(app_flags)) {
            coord->zb_endpoint.appFlags = (uint8_t)app_flags->valuedouble;
        }

        // Input clusters
        cJSON* in_clusters = cJSON_GetObjectItem(ep_obj, "input_clusters");
        if (cJSON_IsArray(in_clusters)) {
            int count = cJSON_GetArraySize(in_clusters);
            coord->zb_endpoint.inputClusterCount = (uint8_t)count;
            coord->zb_endpoint.inputClusterList = calloc(count, sizeof(uint16_t));
            if (coord->zb_endpoint.inputClusterList) {
                for (int i = 0; i < count; i++) {
                    cJSON* item = cJSON_GetArrayItem(in_clusters, i);
                    if (cJSON_IsNumber(item)) {
                        coord->zb_endpoint.inputClusterList[i] = (uint16_t)item->valuedouble;
                    }
                }
            }
        }

        // Output clusters
        cJSON* out_clusters = cJSON_GetObjectItem(ep_obj, "output_clusters");
        if (cJSON_IsArray(out_clusters)) {
            int count = cJSON_GetArraySize(out_clusters);
            coord->zb_endpoint.outputClusterCount = (uint8_t)count;
            coord->zb_endpoint.outputClusterList = calloc(count, sizeof(uint16_t));
            if (coord->zb_endpoint.outputClusterList) {
                for (int i = 0; i < count; i++) {
                    cJSON* item = cJSON_GetArrayItem(out_clusters, i);
                    if (cJSON_IsNumber(item)) {
                        coord->zb_endpoint.outputClusterList[i] = (uint16_t)item->valuedouble;
                    }
                }
            }
        }
    }

    // === Wi-Fi AP ===
    cJSON* ap_ssid_item = cJSON_GetObjectItem(json, "wifi_ap_ssid");
    if (cJSON_IsString(ap_ssid_item) && ap_ssid_item->valuestring && strlen(ap_ssid_item->valuestring) > 0) {
        coord->wifi_ap_ssid = strdup(ap_ssid_item->valuestring);
    }

    cJSON* ap_pass_item = cJSON_GetObjectItem(json, "wifi_ap_password");
    if (cJSON_IsString(ap_pass_item) && ap_pass_item->valuestring) {
        coord->wifi_ap_password = strdup(ap_pass_item->valuestring);
    }

    cJSON* ap_chan_item = cJSON_GetObjectItem(json, "wifi_ap_channel");
    if (cJSON_IsNumber(ap_chan_item)) {
        coord->wifi_ap_channel = (uint8_t)ap_chan_item->valuedouble;
    } else {
        coord->wifi_ap_channel = 6; // дефолт
    }

    cJSON* ap_max_conn_item = cJSON_GetObjectItem(json, "wifi_ap_max_connections");
    if (cJSON_IsNumber(ap_max_conn_item)) {
        coord->wifi_ap_max_connections = (uint8_t)ap_max_conn_item->valuedouble;
    } else {
        coord->wifi_ap_max_connections = 4;
    }

    // === Wi-Fi STA ===
    cJSON* sta_ssid_item = cJSON_GetObjectItem(json, "wifi_sta_ssid");
    if (cJSON_IsString(sta_ssid_item) && sta_ssid_item->valuestring && strlen(sta_ssid_item->valuestring) > 0) {
        coord->wifi_sta_ssid = strdup(sta_ssid_item->valuestring);
    }

    cJSON* sta_pass_item = cJSON_GetObjectItem(json, "wifi_sta_password");
    if (cJSON_IsString(sta_pass_item) && sta_pass_item->valuestring && strlen(sta_pass_item->valuestring) > 0) {
        coord->wifi_sta_password = strdup(sta_pass_item->valuestring);
    }

    cJSON* sta_chan_item = cJSON_GetObjectItem(json, "wifi_sta_channel");
    if (cJSON_IsNumber(sta_chan_item)) {
        coord->wifi_sta_channel = (uint8_t)sta_chan_item->valuedouble;
    }

    //при загрузке всегда AP далее при загрузке режим поменяется если есть STA настройки
    coord->wifi_mode = ZBM_COORDINATOR_WIFI_MODE_AP;

    coord->is_sta_valid = 0; // также считаем, что после перезапуска STA невалидный

    // === SSDP ===
    cJSON* item = NULL;

    /*item = cJSON_GetObjectItem(json, "ssdp_friendly_name");
    if (cJSON_IsString(item) && item->valuestring && strlen(item->valuestring) > 0) {
        coord->ssdp_friendly_name = strdup(item->valuestring);
    }*/

    item = cJSON_GetObjectItem(json, "ssdp_manufacturer");
    if (cJSON_IsString(item) && item->valuestring && strlen(item->valuestring) > 0) {
        coord->ssdp_manufacturer = strdup(item->valuestring);
    }

    item = cJSON_GetObjectItem(json, "ssdp_model_name");
    if (cJSON_IsString(item) && item->valuestring && strlen(item->valuestring) > 0) {
        coord->ssdp_model_name = strdup(item->valuestring);
    }

    item = cJSON_GetObjectItem(json, "ssdp_model_number");
    if (cJSON_IsString(item) && item->valuestring && strlen(item->valuestring) > 0) {
        coord->ssdp_model_number = strdup(item->valuestring);
    }

    item = cJSON_GetObjectItem(json, "ssdp_serial_number");
    if (cJSON_IsString(item) && item->valuestring && strlen(item->valuestring) > 0) {
        coord->ssdp_serial_number = strdup(item->valuestring);
    }

    item = cJSON_GetObjectItem(json, "ssdp_server_name");
    if (cJSON_IsString(item) && item->valuestring && strlen(item->valuestring) > 0) {
        coord->ssdp_server_name = strdup(item->valuestring);
    }

    item = cJSON_GetObjectItem(json, "ssdp_schema_url");
    if (cJSON_IsString(item) && item->valuestring && strlen(item->valuestring) > 0) {
        coord->ssdp_schema_url = strdup(item->valuestring);
    } else {
        coord->ssdp_schema_url = strdup("/description.xml");
    }

    item = cJSON_GetObjectItem(json, "ssdp_presentation_url");
    if (cJSON_IsString(item) && item->valuestring && strlen(item->valuestring) > 0) {
        coord->ssdp_presentation_url = strdup(item->valuestring);
    } else {
        coord->ssdp_presentation_url = strdup("/");
    }

    // === Hostname ===
    cJSON* host_item = cJSON_GetObjectItem(json, "hostname");
    if (cJSON_IsString(host_item) && host_item->valuestring && strlen(host_item->valuestring) > 0) {
        coord->hostname = strdup(host_item->valuestring);
    } else {
        coord->hostname = strdup("esp32-zigbee"); // дефолт
    }
    ESP_LOGI(TAG, "Loaded hostname from JSON: %s", coord->hostname);


    return coord;
}

// === 3. Сохранение координатора в SPIFFS ===
bool zbm_save_coordinator_to_spiffs(const zbm_coordinator_t* coord) {
    if (!coord) {
        ESP_LOGE(TAG, "Cannot save: null coordinator");
        return false;
    }

    cJSON* json = zbm_coordinator_to_json(coord);
    if (!json) {
        ESP_LOGE(TAG, "Failed to serialize coordinator");
        return false;
    }

    char* str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!str) {
        ESP_LOGE(TAG, "Failed to print JSON");
        return false;
    }

    FILE* f = fopen(COORDINATOR_FILE_PATH, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for write: %s", COORDINATOR_FILE_PATH);
        free(str);
        return false;
    }

    fwrite(str, 1, strlen(str), f);
    fclose(f);
    free(str);

    ESP_LOGI(TAG, "Coordinator saved to %s", COORDINATOR_FILE_PATH);
    return true;
}

// === 4. Загрузка координатора из SPIFFS ===
zbm_coordinator_t* zbm_load_coordinator_from_spiffs(void) {
    ESP_LOGI(TAG, "🔍 Checking for coordinator file: %s", COORDINATOR_FILE_PATH);
    if (!spiffs_file_exists(COORDINATOR_FILE_PATH)) {
        ESP_LOGI(TAG, "❌ No coordinator file found: %s", COORDINATOR_FILE_PATH);
        return NULL;
    }
    ESP_LOGI(TAG, "✅ File exists, proceeding to load...");

    FILE* f = fopen(COORDINATOR_FILE_PATH, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open coordinator file: %s", COORDINATOR_FILE_PATH);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size <= 0 || size > 2048) {
        fclose(f);
        ESP_LOGE(TAG, "Invalid coordinator file size: %ld", size);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        ESP_LOGE(TAG, "Memory allocation failed");
        return NULL;
    }

    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    cJSON* json = cJSON_Parse(buffer);
    free(buffer);

    if (!json) {
        ESP_LOGE(TAG, "Failed to parse coordinator JSON");
        return NULL;
    }

    zbm_coordinator_t* coord = zbm_coordinator_from_json(json);
    cJSON_Delete(json);

    if (coord) {
        ESP_LOGI(TAG, "Coordinator loaded from %s", COORDINATOR_FILE_PATH);
    } else {
        ESP_LOGE(TAG, "Failed to create coordinator from JSON");
    }

    return coord;
}

void zbm_free_coordinator(zbm_coordinator_t* coord) {
    if (!coord) return;

    if (coord->friendly_name) {free(coord->friendly_name); coord->friendly_name = NULL;}
    if (coord->wifi_ap_ssid)  {free(coord->wifi_ap_ssid);  coord->wifi_ap_ssid = NULL;}
    if (coord->wifi_ap_password) {free(coord->wifi_ap_password); coord->wifi_ap_password = NULL;}
    if (coord->wifi_sta_ssid) {free(coord->wifi_sta_ssid); coord->wifi_sta_ssid = NULL;}
    if (coord->wifi_sta_password) {free(coord->wifi_sta_password); coord->wifi_sta_password = NULL; }

    // === Освобождаем SSDP-строки ===
    //if (coord->ssdp_friendly_name) {free(coord->ssdp_friendly_name); coord->ssdp_friendly_name = NULL;}
    if (coord->ssdp_manufacturer) {free(coord->ssdp_manufacturer); coord->ssdp_manufacturer = NULL;}
    if (coord->ssdp_model_name) {free(coord->ssdp_model_name); coord->ssdp_model_name = NULL;}
    if (coord->ssdp_model_number) {free(coord->ssdp_model_number); coord->ssdp_model_number = NULL;}
    if (coord->ssdp_serial_number) {free(coord->ssdp_serial_number); coord->ssdp_serial_number = NULL;}
    if (coord->ssdp_server_name) {free(coord->ssdp_server_name); coord->ssdp_server_name = NULL;}
    if (coord->ssdp_schema_url) {free(coord->ssdp_schema_url); coord->ssdp_schema_url = NULL;}
    if (coord->ssdp_presentation_url) {free(coord->ssdp_presentation_url); coord->ssdp_presentation_url = NULL; }

    if (coord->zb_endpoint.inputClusterList) {free(coord->zb_endpoint.inputClusterList); coord->zb_endpoint.inputClusterList = NULL;}
    if (coord->zb_endpoint.outputClusterList) {free(coord->zb_endpoint.outputClusterList); coord->zb_endpoint.outputClusterList = NULL;}

    if (coord->hostname) { free(coord->hostname); coord->hostname = NULL; }

    free(coord);
    coord = NULL;
}