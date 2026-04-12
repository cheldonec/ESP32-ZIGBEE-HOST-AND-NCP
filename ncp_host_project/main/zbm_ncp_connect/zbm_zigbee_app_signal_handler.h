#ifndef ZBM_ZIGBEE_APP_SIGNAL_HANDLER_H

#define ZBM_ZIGBEE_APP_SIGNAL_HANDLER_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "zbm_ncp_connect.h"

typedef enum {
    ESP_ZB_ZDO_SIGNAL_DEFAULT_START                             = 0x00,
    ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP                              = 0x01,
    ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE                              = 0x02,
    ESP_ZB_ZDO_SIGNAL_LEAVE                                     = 0x03,
    ESP_ZB_ZDO_SIGNAL_ERROR                                     = 0x04,
    ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START                        = 0x05,
    ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT                             = 0x06,
    ESP_ZB_BDB_SIGNAL_TOUCHLINK_NWK_STARTED                     = 0x07,
    ESP_ZB_BDB_SIGNAL_TOUCHLINK_NWK_JOINED_ROUTER               = 0x08,
    ESP_ZB_BDB_SIGNAL_TOUCHLINK                                 = 0x09,
    ESP_ZB_BDB_SIGNAL_STEERING                                  = 0x0a,
    ESP_ZB_BDB_SIGNAL_FORMATION                                 = 0x0b,
    ESP_ZB_BDB_SIGNAL_FINDING_AND_BINDING_TARGET_FINISHED       = 0x0c,
    ESP_ZB_BDB_SIGNAL_FINDING_AND_BINDING_INITIATOR_FINISHED    = 0x0d,
    ESP_ZB_BDB_SIGNAL_TOUCHLINK_TARGET                          = 0x0e,
    ESP_ZB_BDB_SIGNAL_TOUCHLINK_NWK                             = 0x0f,
    ESP_ZB_BDB_SIGNAL_TOUCHLINK_TARGET_FINISHED                 = 0x10,
    ESP_ZB_BDB_SIGNAL_TOUCHLINK_ADD_DEVICE_TO_NWK               = 0x11,
    ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED                         = 0x12,
    ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION                          = 0x13,
    ESP_ZB_BDB_SIGNAL_WWAH_REJOIN_STARTED                       = 0x14,
    ESP_ZB_ZGP_SIGNAL_COMMISSIONING                             = 0x15,
    ESP_ZB_COMMON_SIGNAL_CAN_SLEEP                              = 0x16,
    ESP_ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY                   = 0x17,
    ESP_ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT                      = 0x18,
    ESP_ZB_SE_SIGNAL_SKIP_JOIN                                  = 0x19,
    ESP_ZB_SE_SIGNAL_REJOIN                                     = 0x1a,
    ESP_ZB_SE_SIGNAL_CHILD_REJOIN                               = 0x1b,
    ESP_ZB_SE_TC_SIGNAL_CHILD_JOIN_CBKE                         = 0x1c,
    ESP_ZB_SE_TC_SIGNAL_CHILD_JOIN_NON_CBKE                     = 0x1d,
    ESP_ZB_SE_SIGNAL_CBKE_FAILED                                = 0x1e,
    ESP_ZB_SE_SIGNAL_CBKE_OK                                    = 0x1f,
    ESP_ZB_SE_SIGNAL_SERVICE_DISCOVERY_START                    = 0x20,
    ESP_ZB_SE_SIGNAL_SERVICE_DISCOVERY_DO_BIND                  = 0x21,
    ESP_ZB_SE_SIGNAL_SERVICE_DISCOVERY_BIND_OK                  = 0x22,
    ESP_ZB_SE_SIGNAL_SERVICE_DISCOVERY_BIND_FAILED              = 0x23,
    ESP_ZB_SE_SIGNAL_SERVICE_DISCOVERY_BIND_INDICATION          = 0x24,
    ESP_ZB_SE_SIGNAL_SERVICE_DISCOVERY_OK                       = 0x25,
    ESP_ZB_SE_SIGNAL_SERVICE_DISCOVERY_FAILED                   = 0x26,
    ESP_ZB_SE_SIGNAL_APS_KEY_READY                              = 0x27,
    ESP_ZB_SE_SIGNAL_APS_KEY_FAIL                               = 0x28,
    ESP_ZB_SIGNAL_SUBGHZ_SUSPEND                                = 0x29,
    ESP_ZB_SIGNAL_SUBGHZ_RESUME                                 = 0x2a,
    ESP_ZB_MACSPLIT_DEVICE_BOOT                                 = 0x2b,
    ESP_ZB_MACSPLIT_DEVICE_READY_FOR_UPGRADE                    = 0x2c,
    ESP_ZB_MACSPLIT_DEVICE_FW_UPGRADE_EVENT                     = 0x2d,
    ESP_ZB_SIGNAL_NWK_INIT_DONE                                 = 0x2e,
    ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED                         = 0x2f,
    ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE                             = 0x30,
    ESP_ZB_NWK_SIGNAL_PANID_CONFLICT_DETECTED                   = 0x31,
    ESP_ZB_NLME_STATUS_INDICATION                               = 0x32,
    ESP_ZB_TCSWAP_DB_BACKUP_REQUIRED_SIGNAL                     = 0x33,
    ESP_ZB_TC_SWAPPED_SIGNAL                                    = 0x34,
    ESP_ZB_BDB_SIGNAL_TC_REJOIN_DONE                            = 0x35,
    ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS                        = 0x36,
    ESP_ZB_BDB_SIGNAL_STEERING_CANCELLED                        = 0x37,
    ESP_ZB_BDB_SIGNAL_FORMATION_CANCELLED                       = 0x38,
    ESP_ZB_SIGNAL_READY_TO_SHUT                                 = 0x39,
    ESP_ZB_SIGNAL_INTERPAN_PREINIT                              = 0x3a,
    ESP_ZB_ZGP_SIGNAL_MODE_CHANGE                               = 0x3b,
    ESP_ZB_ZDO_DEVICE_UNAVAILABLE                               = 0x3c,
    ESP_ZB_ZGP_SIGNAL_APPROVE_COMMISSIONING                     = 0x3d,
    ESP_ZB_SIGNAL_END                                           = 0x3e,
} local_esp_zb_app_signal_type_t;
/**
 * @brief The application signal struct for esp_zb_app_signal_handler
 *
 */
typedef struct local_esp_zb_app_signal_s {
    uint32_t *p_app_signal;   /*!< Application pointer signal type, refer to esp_zb_app_signal_type_t */
    esp_err_t esp_err_status; /*!< The error status of the each signal event, refer to esp_err_t */
} local_esp_zb_app_signal_t;

typedef struct {
    local_esp_zb_app_signal_type_t signal;        /*!< The signal type of Zigbee */
    const char *msg;                        /*!< The string of Zigbee signal */
} local_esp_zb_app_signal_msg_t;

/**
 * @brief The payload of ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED signal
 *
 */
typedef struct esp_zb_nwk_signal_device_associated_params_s {
    esp_zb_ieee_addr_t device_addr; /*!< address of associated device */
} esp_zb_nwk_signal_device_associated_params_t;


/**
 * @@brief The payload of ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE signal
 */
typedef struct esp_zb_zdo_signal_device_update_params_s {
    esp_zb_ieee_addr_t long_addr;   /*!< Long Address of the updated device */
    uint16_t short_addr;            /*!< Short Address of the updated device */
    uint8_t status;                 /*!< Indicates the updated status of the device, refer to esp_zb_zdo_update_dev_status_t */
    uint8_t tc_action;              /*!< Trust center action,  refer to esp_zb_zdo_update_dev_tc_action_t */
    uint16_t parent_short;          /*!< The short address of device's parent */
} esp_zb_zdo_signal_device_update_params_t;

/**
 * @brief The payload of ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE signal
 *
 * @note Stack passes this parameter to application when some device joins/rejoins to network.
 */
typedef struct esp_zb_zdo_signal_device_annce_params_s {
    uint16_t device_short_addr;           /*!< address of device that recently joined to network */
    esp_zb_ieee_addr_t   ieee_addr;       /*!< The 64-bit (IEEE) address assigned to the device. */
    uint8_t       capability;             /*!< The capability of the device. */
} esp_zb_zdo_signal_device_annce_params_t;


/**
 * @brief The payload of ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED signal
 * @note The authorization_type as following:
 *          0x00 = Authorization type for legacy devices ( < r21)
 *              Status:
 *                  0x00: Authorization success
 *                  0x01: Authorization failed
 *          0x01 = Authorization type for r21 device through TCLK
 *              Status:
 *                  0x00: Authorization success
 *                  0x01: Authorization timeout
 *                  0x02: Authorization failed
 *          0x02 = Authorization type for SE through CBKE
 *              Status:
 *                  0x00: Authorization success
 */
typedef struct esp_zb_zdo_signal_device_authorized_params_s {
    esp_zb_ieee_addr_t long_addr; /*!< Long Address of the updated device */
    uint16_t short_addr;          /*!< Short Address of the updated device */
    uint8_t authorization_type;   /*!< Type of the authorization procedure */
    uint8_t authorization_status; /*!< Status of the authorization procedure which depends on authorization_type */
} esp_zb_zdo_signal_device_authorized_params_t;

void *esp_zb_app_signal_get_params(uint32_t *signal_p);

bool zbm_zigbee_app_signal_handler(local_esp_zb_app_signal_t *signal_s);
#endif