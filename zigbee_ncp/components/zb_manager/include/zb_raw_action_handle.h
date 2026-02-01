#ifndef ZIGBEE_RAW_HANDLE_MODULE_H
#define ZIGBEE_RAW_HANDLE_MODULE_H


/**************************************  RAW DATA ************************************************/
typedef struct raw_data_ind_s {
    uint8_t status;             /*!< The status of the incoming frame processing, 0: on success */
    uint8_t  seq_number;        // tsn
    uint8_t dst_addr_mode;      /*!< The addressing mode for the destination address used in this primitive and of the APDU that has been received,
                                     refer to esp_zb_aps_address_mode_t */
    uint16_t dst_short_addr;    /*!< The individual device address or group address to which the ASDU is directed.*/
    uint8_t dst_endpoint;       /*!< The target endpoint on the local entity to which the ASDU is directed.*/
    uint8_t src_addr_mode;      /*!< Reserved, The addressing mode for the source address used in this primitive and of the APDU that has been received.*/
    uint16_t src_short_addr;    /*!< The individual device address of the entity from which the ASDU has been received.*/
    uint8_t src_endpoint;       /*!< The number of the individual endpoint of the entity from which the ASDU has been received.*/
    uint16_t profile_id;        /*!< The identifier of the profile from which this frame originated.*/
    uint16_t cluster_id;        /*!< The identifier of the received object.*/
    bool     is_common_command; // общая команда?
    uint8_t  cmd_id;
    uint32_t data_length;       /*!< The number of octets comprising the ASDU being indicated by the APSDE.*/
    uint8_t *data;              /*!< The set of octets comprising the ASDU being indicated by the APSDE. */
    uint8_t security_status;    /*!< UNSECURED if the ASDU was received without any security. SECURED_NWK_KEY if the received ASDU was secured with the NWK key.*/
    int8_t rssi;
    int rx_time;                /*!< Reserved, a time indication for the received packet based on the local clock */
    uint16_t manuf_code;
} raw_data_ind_t;

bool raw_command_handler(uint8_t bufid);

#endif