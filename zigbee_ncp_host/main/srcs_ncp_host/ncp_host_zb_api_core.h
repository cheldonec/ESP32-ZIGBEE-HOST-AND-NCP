#ifndef NCP_HOST_ZB_API_CORE_H
#define NCP_HOST_ZB_API_CORE_H 
#include <math.h>
#include <stdint.h>
#include "stdbool.h"
#include "esp_err.h"
#include "esp_event.h"
//#include "event_post_send.h"
#include "ncp_host_zb_api_zdo.h"
#include "zcl/esp_zigbee_zcl_command.h"
#include "esp_zigbee_type.h"
#include "zb_manager_config_platform.h"
#include "ncp_host_zb_api.h"

/** Enum of the Zigbee network device type
 * @anchor esp_zb_nwk_device_type_t
 */
typedef enum {
    ESP_ZB_DEVICE_TYPE_COORDINATOR = 0x0,       /*!<  Device - Coordinator */
    ESP_ZB_DEVICE_TYPE_ROUTER  = 0x1,           /*!<  Device - Router */
    ESP_ZB_DEVICE_TYPE_ED = 0x2,                /*!<  Device - End device */
    ESP_ZB_DEVICE_TYPE_NONE = 0x3,              /*!<  Unknown Device */
} esp_zb_nwk_device_type_t;

/**
 * @brief The Zigbee Coordinator/ Router device configuration.
 *
 */
typedef struct {
    uint8_t max_children; /*!< Max number of the children */
} esp_zb_zczr_cfg_t;

/**
 * @brief The Zigbee End device configuration.
 *
 */
typedef struct {
    uint8_t ed_timeout; /*!< Set End Device Timeout, refer to esp_zb_aging_timeout_t */
    uint32_t keep_alive; /*!< Set Keep alive Timeout, in milliseconds, with a maximum value of 65,000,000,000.*/
} esp_zb_zed_cfg_t;

/**
 * @brief The Zigbee device configuration.
 * @note  For esp_zb_role please refer defined by esp_zb_nwk_device_type_t.
 */
typedef struct esp_zb_cfg_s {
    esp_zb_nwk_device_type_t esp_zb_role; /*!< The nwk device type */
    bool install_code_policy;             /*!< Allow install code security policy or not */
    union {
        esp_zb_zczr_cfg_t zczr_cfg; /*!< The Zigbee zc/zr device configuration */
        esp_zb_zed_cfg_t zed_cfg;   /*!< The Zigbee zed device configuration */
    } nwk_cfg;                      /*!< Union of the network configuration */
} esp_zb_cfg_t;

/**
 * @brief The application signal struct for esp_zb_app_signal_handler
 *
 */
typedef struct esp_zb_app_signal_s {
    uint32_t *p_app_signal;   /*!< Application pointer signal type, refer to esp_zb_app_signal_type_t */
    esp_err_t esp_err_status; /*!< The error status of the each signal event, refer to esp_err_t */
} local_esp_zb_app_signal_t;


//==============================================================================================================================//

void *esp_zb_app_signal_get_params(uint32_t *signal_p);

/**
 * @brief  Start top level commissioning procedure with specified mode mask.
 *
 * @param[in] mode_mask commissioning modes refer to esp_zb_bdb_commissioning_mode
 */

esp_err_t esp_zb_bdb_start_top_level_commissioning(uint8_t mode_mask);

void esp_zb_scheduler_alarm(esp_zb_callback_t cb, uint8_t param, uint32_t time);

esp_err_t esp_zb_set_primary_network_channel_set(uint32_t channel_mask);

uint16_t esp_zb_get_short_address(void);

void esp_zb_get_long_address(esp_zb_ieee_addr_t addr);

uint16_t esp_zb_address_short_by_ieee(esp_zb_ieee_addr_t address);

esp_err_t esp_zb_ieee_address_by_short(uint16_t short_addr, uint8_t *ieee_addr);

uint16_t esp_zb_get_pan_id(void);

void esp_zb_get_extended_pan_id(esp_zb_ieee_addr_t ext_pan_id);

uint8_t esp_zb_get_current_channel(void);

void esp_zb_stack_main_loop(void); // in ncp_host_zb_api.c

void esp_zb_init(esp_zb_cfg_t *nwk_cfg);

esp_err_t esp_zb_start(bool autostart);

// Определение базового типа событий
//ESP_EVENT_DECLARE_BASE(ZB_ACTION_HANDLER_EVENTS);
#define ZB_HANDLER_EVENTS   (esp_event_base_t)("ZB_HANDLER_EVENTS")
//const esp_event_base_t zb_action_handler_event_group = "ZB_ACTION_HANDLER_EVENTS";

// события для процессов worker и pairing
typedef enum {
    ZB_PAIRING_DEVICE_ASSOCIATED,
    ZB_PAIRING_DEVICE_UPDATE,
    ZB_PAIRING_ATTR_READ_RESP,
    ZB_PAIRING_ACTIVE_EP_RESP,
    ZB_PAIRING_DELAYED_SIMPLE_DESC_REQ,
    ZB_PAIRING_SIMPLE_DESC_RESP,
    ZB_PAIRING_DELAYED_NODE_DESC_REQ,
    ZB_PAIRING_NODE_DESC_RESP,
    ZB_ACTION_ATTR_READ_RESP,
    ZB_ACTION_ATTR_REPORT,
    ZB_ACTION_CUSTOM_CLUSTER_REPORT,
    ZB_ACTION_DELAYED_NODE_DESC_REQ,
    ZB_ACTION_DELAYED_BIND_REQ,                  // only dev bind coordinator
    ZB_ACTION_DELAYED_BIND_DEV_DEV_REQ,          // dev bind dev and can coordinator
    ZB_ACTION_BIND_RESP,
    ZB_ACTION_UNBIND_RESP,
    ZB_ACTION_DELAYED_CONFIG_REPORT_REQ,
    ZB_ACTION_NETWORK_IS_OPEN,
    ZB_ACTION_NETWORK_IS_CLOSE,
    // ...
    ZB_PAIRING_DEVICE_ASSOCIATED_EVENT,
    ZB_PAIRING_DEVICE_UPDATE_EVENT,
    //ZB_PAIRING_DELAYED_START_APPEND_DEVICE,
    ZB_PAIRING_DEVICE_ANNCE_EVENT,
    ZB_PAIRING_DEVICE_AUTHORIZED_EVENT,
} zb_manager_internal_event_t;

// Определение конкретных событий
enum {
    ATTR_REPORT_EVENT,
    ATTR_READ_RESP,                                 // ответ на zm_manager_zcl_read_attr_cmd_req
    REPORT_CONFIG_RESP,                             // ответ на zm_manager_zcl_report_config_cmd_req
    ACTIVE_EP_RESP,                                 // ответ на zb_manager_zdo_active_ep_req
    SIMPLE_DESC_RESP,                               // ответ на zb_manager_zdo_simple_desc_req
    NODE_DESC_RESP,                                 // ответ на zb_manager_zdo_node_desc_req
    BIND_RESP,                                      // ответ на esp_zb_zdo_device_bind_req
    DELAYED_NODE_DESC_REQ_LOCAL,                    // запрос через event 
    DELAYED_DEVICE_BIND_COORD_REQ_LOCAL,             // запрос через event 
    DELAYED_CONFIG_REPORT_REQ_LOCAL,                 // запрос через event 
    APPENDING_TIMEOUT_EVENT,            // таймер ожидания добавления устройства
    NETWORK_IS_OPEN,
    NETWORK_IS_CLOSE,
    NEW_DEVICE_JOINED,
    NEW_DEVICE_JOINED_FAIL,
};

typedef void (*zb_manager_event_action_handler_t)(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);

extern zb_manager_event_action_handler_t zb_manager_user_action_event_handler;
esp_err_t zb_manager_register_user_event_handler(zb_manager_event_action_handler_t user_event_handler, void* user_ctx);

void zb_manager_event_action_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);

esp_err_t zb_manager_register_event_action_handler(zb_manager_event_action_handler_t user_event_handler, void *user_ctx);

void zm_user_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);

// main zb manager app_signal_handler return false if need user app_signal_handler else true

typedef void (*local_esp_zb_app_signal_handler_t)(local_esp_zb_app_signal_t *signal_s);

extern local_esp_zb_app_signal_handler_t zb_manager_user_app_signal_handler;

esp_err_t zb_manager_register_user_app_signal_handler(local_esp_zb_app_signal_handler_t user_app_signal_handler);
bool zb_manager_app_signal_handler(local_esp_zb_app_signal_t *signal_s);


esp_err_t zb_manager_init(void);

esp_err_t zb_manager_start(void);

esp_err_t zb_manager_open_network(uint8_t seconds);

esp_err_t zb_manager_close_network(void);

#endif