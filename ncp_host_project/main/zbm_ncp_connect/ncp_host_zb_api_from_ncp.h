#ifndef NCP_HOST_ZB_API_FROM_NCP_H

#define NCP_HOST_ZB_API_FROM_NCP_H

#include <stdint.h>
#include "esp_err.h"
/**
 * @brief A function for process Zigbee stack.
 *
 * @param[in]  input    The pointer to storage the data
 * @param[in]  inlen    The length to storage the data
 *
 * @return 
 *     - ESP_OK on success
 *     - others: refer to esp_err.h
 */
typedef esp_err_t (*host_zb_fn)(const uint8_t *input, uint16_t inlen);

/**
 * @brief Type to represent the protocol frame process function.
 *
 */
typedef struct {
    uint16_t    id;                                     /*!< The frame ID for request */
    host_zb_fn  set_func;                               /*!< A function for process Zigbee stack */
} esp_host_zb_func_t;

extern const esp_host_zb_func_t host_zb_api_from_ncp_func_table[];
extern const uint8_t host_zb_api_from_ncp_func_table_size;

/**
 * @brief ZDP status values
 * @anchor esp_zb_zdp_status
 * @note the status feedback for the zdo command
 */
typedef enum {
    ESP_ZB_ZDP_STATUS_SUCCESS               = 0x00,   /*!< The requested operation or transmission was completed successfully. */
    ESP_ZB_ZDP_STATUS_INV_REQUESTTYPE       = 0x80,   /*!< The supplied request type was invalid. */
    ESP_ZB_ZDP_STATUS_DEVICE_NOT_FOUND      = 0x81,   /*!< The requested device did not exist on a device following a child descriptor request to a parent.*/
    ESP_ZB_ZDP_STATUS_INVALID_EP            = 0x82,   /*!< The supplied endpoint was equal to 0x00 or between 0xf1 and 0xff. */
    ESP_ZB_ZDP_STATUS_NOT_ACTIVE            = 0x83,   /*!< The requested endpoint is not described by simple descriptor. */
    ESP_ZB_ZDP_STATUS_NOT_SUPPORTED         = 0x84,   /*!< The requested optional feature is not supported on the target device. */
    ESP_ZB_ZDP_STATUS_TIMEOUT               = 0x85,   /*!< A timeout has occurred with the requested operation. */
    ESP_ZB_ZDP_STATUS_NO_MATCH              = 0x86,   /*!< The end device bind request was unsuccessful due to a failure to match any suitable clusters.*/
    ESP_ZB_ZDP_STATUS_NO_ENTRY              = 0x88,   /*!< The unbind request was unsuccessful due to the coordinator or source device not having an entry in its binding table to unbind.*/
    ESP_ZB_ZDP_STATUS_NO_DESCRIPTOR         = 0x89,   /*!< A child descriptor was not available following a discovery request to a parent. */
    ESP_ZB_ZDP_STATUS_INSUFFICIENT_SPACE    = 0x8a,   /*!< The device does not have storage space to support the requested operation. */
    ESP_ZB_ZDP_STATUS_NOT_PERMITTED         = 0x8b,   /*!< The device is not in the proper state to support the requested operation. */
    ESP_ZB_ZDP_STATUS_TABLE_FULL            = 0x8c,   /*!< The device does not have table space to support the operation. */
    ESP_ZB_ZDP_STATUS_NOT_AUTHORIZED        = 0x8d,   /*!< The permissions configuration table on the target indicates that the request is not authorized from this device.*/
    ESP_ZB_ZDP_STATUS_BINDING_TABLE_FULL    = 0x8e,   /*!< The device doesn't have binding table space to support the operation */
    ESP_ZB_ZDP_STATUS_INVALID_INDEX         = 0x8f,   /*!< The index in the received command is out of bounds. */
} local_esp_zb_zdp_status_t;

//ZDO

typedef void (*local_esp_zb_zdo_active_ep_callback_t)(local_esp_zb_zdp_status_t zdo_status, uint8_t ep_count, uint8_t *ep_id_list, void *user_ctx);

/*****************************************************************************************************************************
 * @brief Structure of simple descriptor request of ZCL command
 */
typedef struct local_esp_zb_af_simple_desc_1_1_t {
    uint8_t    endpoint;                        /*!< Endpoint */
    uint16_t   app_profile_id;                  /*!< Application profile identifier */
    uint16_t   app_device_id;                   /*!< Application device identifier */
    uint32_t    app_device_version: 4;          /*!< Application device version */
    uint32_t    reserved: 4;                    /*!< Reserved */
    uint8_t    app_input_cluster_count;         /*!< Application input cluster count */
    uint8_t    app_output_cluster_count;        /*!< Application output cluster count */
    uint16_t   app_cluster_list[2];             /*!< Application input and output cluster list */
} __attribute__ ((packed)) local_esp_zb_af_simple_desc_1_1_t;
/**
 * @brief A ZDO simple descriptor request callback for user to get simple desc info.
 */
typedef void (*local_esp_zb_zdo_simple_desc_callback_t)(local_esp_zb_zdp_status_t zdo_status, local_esp_zb_af_simple_desc_1_1_t *simple_desc, void *user_ctx);

/*****************************************************************************************************************************
 * @brief Structure of discovery attributes request of ZCL command
 */

#endif