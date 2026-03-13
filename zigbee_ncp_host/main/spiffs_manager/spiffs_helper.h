// spiffs_helper.h
#ifndef SPIFFS_HELPER_H
#define SPIFFS_HELPER_H

#include "esp_vfs.h"
#include "esp_spiffs.h"

// Монтирование
#define SPIFFS_CFG_MOUNT_POINT     "/spiffs_config"
#define SPIFFS_UI_MOUNT_POINT      "/spiffs_ui"
#define SPIFFS_QUIRKS_MOUNT_POINT  "/quirks"
#define SPIFFS_CERTS_MOUNT_POINT   "/certs" 

// Пути к файлам
#define CONFIG_FILE_PATH                                             "/spiffs_config/config.json"
#define ZB_MANAGER_JSON_DEVICES_FILE     SPIFFS_CFG_MOUNT_POINT      "/zb_manager_devices.json"
#define ZB_MANAGER_JSON_INDEX_FILE       SPIFFS_CFG_MOUNT_POINT      "/zb_devices_index.json"
#define ZB_MANAGER_WEB_SERVER_HOME_PAGE  SPIFFS_UI_MOUNT_POINT       "/index.html"
//#define ZB_MANAGER_QUIRKS_TUYA_JSON      SPIFFS_QUIRKS_MOUNT_POINT   "/tuya_models.json"
#define MQTT_ROOT_CERT_PATH              SPIFFS_CERTS_MOUNT_POINT    "/mqtt_root.crt"

#define ZB_MANAGER_RULES_JSON_FILE     SPIFFS_CFG_MOUNT_POINT        "/rules.json"
#define ZB_MANAGER_RULES_VARS_FILE     SPIFFS_CFG_MOUNT_POINT        "/virtual_vars.bin"
// Инициализация
esp_err_t init_spiffs(void);
bool is_certs_partition_ready(void);

#endif
