#ifndef ZB_MANAGER_MAIN_CONFIG_H
#define ZB_MANAGER_MAIN_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool mqtt_enabled;
    char broker[64];
    uint16_t port;
    char username[32];
    char password[64];
    bool discovery;
    bool availability;
} ha_mqtt_config_t;

typedef struct {
    bool ha_enabled;
    ha_mqtt_config_t mqtt;
    char language[8]; // "ru", "en", "de"  
} app_ha_config_t;

typedef struct {
    char theme[8];    // "dark", "light"
}app_web_ui_config_t;

typedef struct {
    app_ha_config_t         ha_config;
    app_web_ui_config_t     web_ui_config;
}zb_manager_app_main_config_t;

void load_zb_manager_app_main_config(void);
void save_zb_manager_app_main_config(void);

extern zb_manager_app_main_config_t g_app_config;

#endif