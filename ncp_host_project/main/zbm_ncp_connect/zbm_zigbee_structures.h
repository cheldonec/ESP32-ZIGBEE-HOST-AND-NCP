#ifndef ZBM_ZIGBEE_STRUCTURES
#define ZBM_ZIGBEE_STRUCTURES
#include <stdint.h>
#include "zbm_attr_types.h"
#include "zbm_cmd_types.h"

#define ESP_ZB_PACKED_STRUCT __attribute__ ((packed))

typedef uint8_t esp_zb_64bit_addr_t[8];
typedef esp_zb_64bit_addr_t esp_zb_ieee_addr_t;

/**
 * @brief Type to represent source address of ZCL message
 * @note Address type refer @ref esp_zb_zcl_address_type_t
 */
typedef struct esp_zb_zcl_addr_s {
    uint8_t addr_type;                  /*!< address type see esp_zb_zcl_address_type_t */
    union {
        uint16_t short_addr;            /*!< Zigbee short address */
        uint32_t src_id;                /*!< Source ID of ZGPD */
        esp_zb_ieee_addr_t ieee_addr;   /*!< Full IEEE-address of ZGPD */
    } u;                                /*!< Union of the address */
} ESP_ZB_PACKED_STRUCT
esp_zb_zcl_addr_t;

/**
 * @brief ZCL status values
 * @anchor esp_zb_zcl_status
 */
typedef enum {
    ESP_ZB_ZCL_STATUS_SUCCESS               = 0x00U,     /*!< ZCL Success */
    ESP_ZB_ZCL_STATUS_FAIL                  = 0x01U,     /*!< ZCL Fail */
    ESP_ZB_ZCL_STATUS_NOT_AUTHORIZED        = 0x7EU,     /*!< Server is not authorized to upgrade the client */
    ESP_ZB_ZCL_STATUS_MALFORMED_CMD         = 0x80U,     /*!< Malformed command */
    ESP_ZB_ZCL_STATUS_UNSUP_CLUST_CMD       = 0x81U,     /*!< Unsupported cluster command */
    ESP_ZB_ZCL_STATUS_UNSUP_GEN_CMD         = 0x82U,     /*!< Unsupported general command */
    ESP_ZB_ZCL_STATUS_UNSUP_MANUF_CLUST_CMD = 0x83U,     /*!< Unsupported manuf-specific clust command */
    ESP_ZB_ZCL_STATUS_UNSUP_MANUF_GEN_CMD   = 0x84U,     /*!< Unsupported manuf-specific general command */
    ESP_ZB_ZCL_STATUS_INVALID_FIELD         = 0x85U,     /*!< Invalid field */
    ESP_ZB_ZCL_STATUS_UNSUP_ATTRIB          = 0x86U,     /*!< Unsupported attribute */
    ESP_ZB_ZCL_STATUS_INVALID_VALUE         = 0x87U,     /*!< Invalid value */
    ESP_ZB_ZCL_STATUS_READ_ONLY             = 0x88U,     /*!< Read only */
    ESP_ZB_ZCL_STATUS_INSUFF_SPACE          = 0x89U,     /*!< Insufficient space */
    ESP_ZB_ZCL_STATUS_DUPE_EXISTS           = 0x8aU,     /*!< Duplicate exists */
    ESP_ZB_ZCL_STATUS_NOT_FOUND             = 0x8bU,     /*!< Not found */
    ESP_ZB_ZCL_STATUS_UNREPORTABLE_ATTRIB   = 0x8cU,     /*!< Unreportable attribute */
    ESP_ZB_ZCL_STATUS_INVALID_TYPE          = 0x8dU,     /*!< Invalid type */
    ESP_ZB_ZCL_STATUS_WRITE_ONLY            = 0x8fU,     /*!< Write only */
    ESP_ZB_ZCL_STATUS_INCONSISTENT          = 0x92U,     /*!< Supplied values are inconsistent */
    ESP_ZB_ZCL_STATUS_ACTION_DENIED         = 0x93U,
    ESP_ZB_ZCL_STATUS_TIMEOUT               = 0x94U,     /*!< Timeout */
    ESP_ZB_ZCL_STATUS_ABORT                 = 0x95U,     /*!< Abort */
    ESP_ZB_ZCL_STATUS_INVALID_IMAGE         = 0x96U,     /*!< Invalid OTA upgrade image */
    ESP_ZB_ZCL_STATUS_WAIT_FOR_DATA         = 0x97U,     /*!< Server does not have data block available yet */
    ESP_ZB_ZCL_STATUS_NO_IMAGE_AVAILABLE    = 0x98U,
    ESP_ZB_ZCL_STATUS_REQUIRE_MORE_IMAGE    = 0x99U,
    ESP_ZB_ZCL_STATUS_NOTIFICATION_PENDING  = 0x9AU,
    ESP_ZB_ZCL_STATUS_HW_FAIL               = 0xc0U,     /*!< Hardware failure */
    ESP_ZB_ZCL_STATUS_SW_FAIL               = 0xc1U,     /*!< Software failure */
    ESP_ZB_ZCL_STATUS_CALIB_ERR             = 0xc2U,     /*!< Calibration error */
    ESP_ZB_ZCL_STATUS_UNSUP_CLUST           = 0xc3U,     /*!< Cluster is not found on the target endpoint */
    ESP_ZB_ZCL_STATUS_LIMIT_REACHED         = 0xc4U,     /*!< Cluster is not found on the target endpoint */
} esp_zb_zcl_status_t;

/**
 * @brief ZCL attribute access values
 * @anchor esp_zb_zcl_attr_access
 */
typedef enum {
    ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY  = 0x01U,   /*!< Attribute is read only */
    ESP_ZB_ZCL_ATTR_ACCESS_WRITE_ONLY = 0x02U,   /*!< Attribute is write only */
    ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE = 0x03U,   /*!< Attribute is read/write */
    ESP_ZB_ZCL_ATTR_ACCESS_REPORTING  = 0x04U,   /*!< Attribute is allowed for reporting */
    ESP_ZB_ZCL_ATTR_ACCESS_SINGLETON  = 0x08U,   /*!< Attribute is singleton */
    ESP_ZB_ZCL_ATTR_ACCESS_SCENE      = 0x10U,   /*!< Attribute is accessed through scene */
    ESP_ZB_ZCL_ATTR_MANUF_SPEC        = 0x20U,   /*!< Attribute is manufacturer specific */
    ESP_ZB_ZCL_ATTR_ACCESS_INTERNAL   = 0x40U,   /*!< Internal access only Attribute */
} esp_zb_zcl_attr_access_t;

/**
 * @brief The Zigbee address union consist of 16 bit short address and 64 bit long address.
 *
 */
typedef union {
    uint16_t  addr_short;                           /*!< Zigbee short address */
    esp_zb_ieee_addr_t addr_long;                   /*!< Zigbee long address */
} esp_zb_addr_u;

/** Enum of the Zigbee ZCL address mode
 * @note Defined the ZCL command of address_mode.
 * @anchor esp_zb_zcl_address_mode_t
 */
typedef enum {
    ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT =        0x0,            /*!< DstAddress and DstEndpoint not present */
    ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT  =       0x1,            /*!< 16-bit group address for DstAddress; DstEndpoint not present */
    ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT =                  0x2,            /*!< 16-bit address for DstAddress and DstEndpoint present */
    ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT =                  0x3,            /*!< 64-bit extended address for DstAddress and DstEndpoint present */
} esp_zb_zcl_address_mode_t;

/**
 * @brief The Zigbee ZCL basic command info
 *
 */
typedef struct esp_zb_zcl_basic_cmd_s {
    esp_zb_addr_u dst_addr_u;                   /*!< Single short address or group address */
    uint8_t  dst_endpoint;                      /*!< Destination endpoint */
    uint8_t  src_endpoint;                      /*!< Source endpoint */
} esp_zb_zcl_basic_cmd_t;

/**
 * @brief The Zigbee ZCL read attribute command struct
 *
 */
typedef struct esp_zb_zcl_read_attr_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;           /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;         /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint16_t clusterID;                             /*!< Cluster ID to read */
    struct {
        uint8_t manuf_specific   : 2;               /*!< Sent as manufacturer extension with code. */
        uint8_t direction        : 1;               /*!< The command direction, refer to esp_zb_zcl_cmd_direction_t */
        uint8_t dis_defalut_resp : 1;               /*!< Disable default response for this command. */
    };
    uint16_t manuf_code;                            /*!< The manufacturer code sent with the command. */
    uint8_t attr_number;                            /*!< Number of attribute in the attr_field */
    uint16_t *attr_field;                           /*!< Attribute identifier field to read */
} esp_zb_zcl_read_attr_cmd_t;


/**
 * @brief The Zigbee zcl cluster attribute value struct
 *
 */
 typedef struct esp_zb_zcl_attribute_data_s {
    zbm_attr_data_types_t type; /*!< The type of attribute, which can refer to esp_zb_zcl_attr_type_t */
    uint16_t size;               /*!< The value size of attribute  */
    void *value;                 /*!< The value of attribute, Note that if the type is string/array, the first byte of value indicates the string length */
} ESP_ZB_PACKED_STRUCT esp_zb_zcl_attribute_data_t;

/**
 * @brief The Zigbee zcl cluster attribute struct
 *
 */
typedef struct esp_zb_zcl_attribute_s {
    uint16_t id;                      /*!< The identify of attribute */
    esp_zb_zcl_attribute_data_t data; /*!< The data of attribute */
} esp_zb_zcl_attribute_t;

/**
 * @brief The variable of Zigbee zcl read attribute response
 *
 */
typedef struct esp_zb_zcl_read_attr_resp_variable_s {
    esp_zb_zcl_status_t status;                        /*!< The field specifies the status of the read operation on this attribute */
    esp_zb_zcl_attribute_t attribute;                  /*!< The field contain the current value of this attribute, @ref esp_zb_zcl_attribute_s */
    struct esp_zb_zcl_read_attr_resp_variable_s *next; /*!< Next variable */
} esp_zb_zcl_read_attr_resp_variable_t;

/**
 * @brief The frame header of Zigbee zcl command struct
 *
 * @note frame control field:
 * |----1 bit---|---------1 bit---------|---1 bit---|----------1 bit-----------|---4 bit---|
 * | Frame type | Manufacturer specific | Direction | Disable Default Response | Reserved  |
 *
 */
 typedef struct esp_zb_zcl_frame_header_s {
    uint8_t fc;          /*!< A 8-bit Frame control */
    uint16_t manuf_code; /*!< Manufacturer code */
    uint8_t tsn;         /*!< Transaction sequence number */
    int8_t rssi;         /*!< Signal strength */
} esp_zb_zcl_frame_header_t;

/**
 * @brief The Zigbee zcl cluster command properties struct
 *
 */
 typedef struct esp_zb_zcl_command_s {
    uint8_t id;        /*!< The command id */
    uint8_t direction; /*!< The command direction */
    uint8_t is_common; /*!< The command is common type */
} esp_zb_zcl_command_t;

/**
 * @brief The Zigbee zcl command basic application information struct
 *
 */
 typedef struct esp_zb_zcl_cmd_info_s {
    esp_zb_zcl_status_t status;       /*!< The status of command, which can refer to  esp_zb_zcl_status_t */
    esp_zb_zcl_frame_header_t header; /*!< The command frame properties, which can refer to esp_zb_zcl_frame_field_t */
    esp_zb_zcl_addr_t src_address;    /*!< The struct of address contains short and ieee address, which can refer to esp_zb_zcl_addr_s */
    uint16_t dst_address;             /*!< The destination short address of command */
    uint8_t src_endpoint;             /*!< The source endpoint of command */
    uint8_t dst_endpoint;             /*!< The destination endpoint of command */
    uint16_t cluster;                 /*!< The cluster id for command */
    uint16_t profile;                 /*!< The application profile identifier*/
    esp_zb_zcl_command_t command;     /*!< The properties of command */
} esp_zb_zcl_cmd_info_t;

/**
 * @brief The Zigbee zcl read attribute response struct
 *
 */
typedef struct esp_zb_zcl_cmd_read_attr_resp_message_s {
    esp_zb_zcl_cmd_info_t info;                      /*!< The basic information of reading attribute response message that refers to esp_zb_zcl_cmd_info_t */
    esp_zb_zcl_read_attr_resp_variable_t *variables; /*!< The variable items, @ref esp_zb_zcl_read_attr_resp_variable_s */
} esp_zb_zcl_cmd_read_attr_resp_message_t;

/**
 * @brief The Zigbee zcl attribute report message struct
 *
 */
typedef struct esp_zb_zcl_report_attr_message_s {
    esp_zb_zcl_status_t status;       /*!< The status of the report attribute response, which can refer to esp_zb_zcl_status_t */
    esp_zb_zcl_addr_t src_address;    /*!< The struct of address contains short and ieee address, which can refer to esp_zb_zcl_addr_s */
    uint8_t src_endpoint;             /*!< The endpoint id which comes from report device */
    uint8_t dst_endpoint;             /*!< The destination endpoint id */
    uint16_t cluster;                 /*!< The cluster id that reported */
    esp_zb_zcl_attribute_t attribute; /*!< The attribute entry of report response */
} esp_zb_zcl_report_attr_message_t;


/**
 * @brief Frame Controll
 *
 */
typedef struct zbm_frame_control_s{
    uint8_t frame_type       : 2;               /*!< The frame type, refer to esp_zb_zcl_frame_type_t */
    uint8_t manuf_specific   : 1;               /*!< Sent as manufacturer extension with code. */
    uint8_t direction        : 1;               /*!< The command direction, refer to esp_zb_zcl_cmd_direction_t */
    uint8_t dis_defalut_resp : 1;               /*!< Disable default response for this command. */
    uint8_t reserved         : 3;
}zbm_frame_control_t;

/**
 * @brief Отправка кластерной команды Zigbee например ON/OFF
 *
 */
typedef struct zbm_send_zcl_cmd_to_cluster_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;           /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;         /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint16_t clusterID;                             /*!< Cluster ID to read */
    zbm_frame_control_t frame_control;              /*!< Frame Controll */
    uint16_t manuf_code;                            /*!< The manufacturer code sent with the command. */
    zbm_cluster_standart_cmd_t *cmd_object;
} zbm_send_zcl_cmd_to_cluster_cmd_t;


typedef struct esp_zb_zcl_disc_attr_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode; /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint16_t cluster_id;                    /*!< The cluster identifier for which the attribute is discovered. */
    struct {
        uint8_t manuf_specific   : 2;       /*!< Sent as manufacturer extension with code. */
        uint8_t direction        : 1;       /*!< The command direction, refer to esp_zb_zcl_cmd_direction_t */
        uint8_t dis_defalut_resp : 1;       /*!< Disable default response for this command. */
    };
    uint16_t manuf_code;                    /*!< The manufacturer code sent with the command. */
    uint16_t start_attr_id;                 /*!< The attribute identifier at which to begin the attribute discover */
    uint8_t max_attr_number;                /*!< The maximum number of attribute identifiers that are to be returned in the resulting Discover Attributes Response command*/
} esp_zb_zcl_disc_attr_cmd_t;

#endif