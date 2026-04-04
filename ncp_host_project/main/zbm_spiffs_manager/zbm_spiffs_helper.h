// spiffs_helper.h
#ifndef ZBM_SPIFFS_HELPER_H
#define ZBM_SPIFFS_HELPER_H

#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "cJSON.h"

// Монтирование
#define SPIFFS_ZBM_CONF_MOUNT_POINT     "/zbm_conf"
#define SPIFFS_ZBM_UI_MOUNT_POINT      "/zbm_ui"
#define SPIFFS_ZBM_QUIRKS_MOUNT_POINT  "/zbm_quirks"
#define SPIFFS_ZBM_CERTS_MOUNT_POINT   "/zbm_certs" 

// Пути к файлам
// конфиги
// основные параметры
#define ZBM_CONFIG_FILE_PATH                    SPIFFS_ZBM_CONF_MOUNT_POINT    "/zbm_config.json"

// список сопряжённых устройств
#define ZBM_DEV_INDEX_FILE                      SPIFFS_ZBM_CONF_MOUNT_POINT    "/zbm_devices_index.json"

// web server root
#define ZBM_WEB_SERVER_HOME_PAGE                SPIFFS_ZBM_UI_MOUNT_POINT      "/index.html"

//quirks
#define ZBM_QUIRKS_INDEX_JSON                   SPIFFS_ZBM_QUIRKS_MOUNT_POINT "/zbm_quirks_index.json"

//#define ZB_MANAGER_QUIRKS_TUYA_JSON      SPIFFS_QUIRKS_MOUNT_POINT   "/tuya_models.json"
//#define MQTT_ROOT_CERT_PATH              SPIFFS_CERTS_MOUNT_POINT       "/mqtt_root.crt"

//#define ZB_MANAGER_RULES_JSON_FILE     SPIFFS_CFG_MOUNT_POINT           "/rules.json"
//#define ZB_MANAGER_RULES_VARS_FILE     SPIFFS_CFG_MOUNT_POINT           "/virtual_vars.bin"
// Инициализация
esp_err_t init_spiffs(void);
//bool is_certs_partition_ready(void);

// Utils
bool spiffs_file_exists(const char* path);
cJSON* spiffs_list_directory(const char* dir_path);

#endif
