#ifndef ZBM_COORDINATOR_H

#define ZBM_COORDINATOR_H

#include "zbm_dev_types.h"
#include "ncp_host_zb_api.h"
#include "cJSON.h"
typedef enum {
    ZBM_COORDINATOR_WIFI_MODE_AP = 0,
    ZBM_COORDINATOR_WIFI_MODE_STA = 1,
}zbm_coordinator_wifi_mode_t;

typedef struct zbm_coordinator_s {
    char*                       friendly_name;
    uint16_t                    zb_short_address;
    uint8_t                     zb_ieee_addr[8]; // ieee адрес координатор
    uint16_t                    zb_pan_id;
    uint8_t                     zb_extended_pan_id[8];
    uint8_t                     zb_radio_channel;
    zbm_coordinator_wifi_mode_t wifi_mode;           // режим в реальном времени, если удалось прочитать STA настройки то STA, если нет то AP
    esp_host_zb_endpoint_t      zb_endpoint;
    char*                       wifi_ap_ssid;
    char*                       wifi_ap_password;
    uint8_t                     wifi_ap_channel;
    uint8_t                     wifi_ap_max_connections;
    char*                       wifi_sta_ssid;
    char*                       wifi_sta_password;
    uint8_t                     wifi_sta_channel;
    uint8_t                     is_sta_valid;                // если хоть раз было подключение после перезапуска то 1 значит  STA SSID и пароль валидные

    //char*                       ssdp_friendly_name;     // например: "Zigbee Gateway"
    char*                       ssdp_manufacturer;      // "CheldonecCo"
    char*                       ssdp_model_name;        // "Zigbee NCP Host"
    char*                       ssdp_model_number;      // "1.0"
    char*                       ssdp_serial_number;     // "00000001"
    char*                       ssdp_server_name;       // "Linux/ESP32 UPnP/1.1 ZBM-GW/1.0"
    char*                       ssdp_schema_url;        // "/description.xml" (можно не менять)
    char*                       ssdp_presentation_url;  // "/" или "/ui"
    // === hostname для mDNS ===
    char*                       hostname;  // например: "zigbee-gateway"
}zbm_coordinator_t;

extern zbm_coordinator_t zbm_coordinator;

/**
 * @brief Сериализует координатор в cJSON
 * @param coord Указатель на координатор
 * @return cJSON* или NULL при ошибке
 */
cJSON* zbm_coordinator_to_json(const zbm_coordinator_t* coord);

/**
 * @brief Создаёт координатор из cJSON
 * @param json Указатель на JSON-объект
 * @return zbm_coordinator_t* или NULL при ошибке
 * @note Результат нужно освобождать через zbm_free_coordinator()
 */
zbm_coordinator_t* zbm_coordinator_from_json(const cJSON* json);

/**
 * @brief Сохраняет координатор в SPIFFS
 * @param coord Указатель на координатор
 * @return true при успехе
 */
bool zbm_save_coordinator_to_spiffs(const zbm_coordinator_t* coord);

/**
 * @brief Загружает координатор из SPIFFS
 * @return zbm_coordinator_t* или NULL
 * @note Результат нужно освобождать через zbm_free_coordinator()
 */
zbm_coordinator_t* zbm_load_coordinator_from_spiffs(void);

/**
 * @brief Освобождает память, выделенную под координатор
 * @param coord Указатель на координатор
 */
void zbm_free_coordinator(zbm_coordinator_t* coord);


#endif