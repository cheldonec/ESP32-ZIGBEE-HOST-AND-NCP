#ifndef ZB_M_EP_GEN_H
#define ZB_M_EP_GEN_H
#include "esp_zigbee_core.h"
#include "zb_config.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"

#define HA_REMOTE_DEVICE_ENDPOINT1        1           /* esp switch device endpoint */
#define HA_REMOTE_DEVICE_ENDPOINT2        2           /* esp switch device endpoint */
//#define ESP_MANUFACTURER_NAME "\x09""ESPRESSIF"      /* Customized manufacturer name */
//#define ESP_MANUFACTURER_NAME "\x11""CheldonecCo"      /* Customized manufacturer name */
//#define ESP_MODEL_IDENTIFIER "\x07"CONFIG_IDF_TARGET /* Customized model identifier */
//#define ESP_MODEL_IDENTIFIER "\x09""TEST_ED_1" /* Customized model identifier */
#define ESP_MODEL_DATE_CODE "2025090300000000"
//#define ESP_MANUFACTURER_VERSION_DETAILS "ZB_Monitor"
//#define ESP_DEVICE_SN "cheldonec_0001"
//#define ESP_DEVICE_PRODUCT_LABEL "ZB_Monitor"
//#define ESP_DEVICE_LOCAL_DESCRIPTION "ZB_Monitor"
//#define BASIC_SW_BUILD_ID "500001"

#define LOCAL_REMOTE_ENDPOINT1_CONFIG()                              \
{                                                                   \
    .endpoint = HA_REMOTE_DEVICE_ENDPOINT1,                          \
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,                      \   
    .app_device_id = ESP_ZB_HA_REMOTE_CONTROL_DEVICE_ID,            \
    .app_device_version = 0,                                        \
}                                    

#define LOCAL_REMOTE_ENDPOINT2_CONFIG()                              \
{                                                                   \
    .endpoint = HA_REMOTE_DEVICE_ENDPOINT2,                          \
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,                      \   
    .app_device_id = ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID,            \
    .app_device_version = 0,                                        \
}                                  

typedef struct custom_zb_endpoint_add_str_s{
    //uint8_t     endpoint;
    uint8_t     inputClusterCount;                      /*!< The number of cluster IDs in inputClusterList */
    uint16_t    *inputClusterList;                      /*!< Input cluster IDs the endpoint will accept */
    uint8_t     outputClusterCount;                     /*!< The number of cluster IDs in outputClusterList */
    uint16_t    *outputClusterList;                     /*!< Output cluster IDs the endpoint will accept */ 
} custom_zb_endpoint_add_str_t;

extern esp_zb_ep_list_t *zb_manager_ep_gen_list;

typedef esp_err_t (*zb_manager_generate_endpoint_func_t)(custom_zb_endpoint_add_str_t* ep_str, esp_zb_endpoint_config_t endpoint_config);
esp_err_t zb_manager_generate_endpoint(custom_zb_endpoint_add_str_t* ep_str, esp_zb_endpoint_config_t endpoint_config);

esp_zb_attribute_list_t *generate_basic_cluster_zcl8(esp_zb_zcl_cluster_role_t role);
esp_zb_attribute_list_t *generate_identify_cluster_zcl8(esp_zb_zcl_cluster_role_t role);
esp_zb_attribute_list_t *generate_on_off_cluster_zcl8(esp_zb_zcl_cluster_role_t role);

esp_zb_ep_list_t *generate_local_endpoints_desc_for_sample(void);

#endif