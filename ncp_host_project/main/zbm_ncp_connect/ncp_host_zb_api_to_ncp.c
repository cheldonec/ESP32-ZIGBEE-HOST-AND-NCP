#include "ncp_host_zb_api_to_ncp.h"
#include "ncp_host_zb_api.h"
//#include "zbm_ncp_connect.h"
#include "zbm_zigbee_structures.h"
#include "string.h"
#include "esp_log.h"
#include "esp_err.h"


static const char* TAG = "NCP_HOST_ZB_API_TO_NCP";

esp_err_t zbm_to_ncp_cmd_init_zigbee_stack(void)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ZB_MANAGER_INIT_CMD, NULL, 0, &output, &outlen);

    return output;
}

esp_err_t zbm_to_ncp_cmd_register_endpoint(esp_host_zb_endpoint_t *endpoint)
{
    typedef struct {
        uint8_t     endpoint;                               /*!< The application endpoint to be added */
        uint16_t    profileId;                              /*!< The endpoint's application profile */
        uint16_t    deviceId;                               /*!< The endpoint's device ID within the application profile */
        uint8_t     appFlags;                               /*!< The device version and flags indicating description availability */
        uint8_t     inputClusterCount;                      /*!< The number of cluster IDs in inputClusterList */
        uint8_t     outputClusterCount;                     /*!< The number of cluster IDs in outputClusterList */
    } __attribute__ ((packed)) esp_endpoint_t;
 
    uint16_t data_len = 0;
    uint16_t data_head_len = sizeof(esp_endpoint_t);
    uint16_t inputClusterLength = endpoint->inputClusterCount * sizeof(uint16_t);
    uint16_t outputClusterLength = endpoint->outputClusterCount * sizeof(uint16_t);
    uint8_t *input = calloc(1, data_head_len + inputClusterLength + outputClusterLength);

    if (input) {
        esp_endpoint_t esp_endpoint = {
            .endpoint = endpoint->endpoint,
            .profileId = endpoint->profileId,
            .deviceId = endpoint->deviceId,
            .appFlags = endpoint->appFlags,
            .inputClusterCount = endpoint->inputClusterCount,
            .outputClusterCount = endpoint->outputClusterCount,
        };
        data_len = data_head_len + inputClusterLength + outputClusterLength;
        memcpy(input, &esp_endpoint, data_head_len);
        if (inputClusterLength) {
            memcpy(input + data_head_len, endpoint->inputClusterList, inputClusterLength);

        }

        if (outputClusterLength) {
            memcpy(input + data_head_len + inputClusterLength, endpoint->outputClusterList, outputClusterLength);
        }
    }

    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ESP_NCP_ZCL_ENDPOINT_ADD, input, data_len, &output, &outlen);

    if (input) {
        free(input);
        input = NULL;
    }

    return ESP_OK;
}

esp_err_t zbm_to_ncp_cmd_start_zigbee_stack(void)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ZB_MANAGER_START_CMD, NULL, 0, &output, &outlen);

    return output;
}

esp_err_t zbm_to_ncp_req_get_coord_long_addr(esp_zb_ieee_addr_t ieee_addr)
{
    uint16_t outlen = sizeof(esp_zb_ieee_addr_t);
    return esp_host_zb_output(ESP_NCP_NETWORK_LONG_ADDRESS_GET, NULL, 0, ieee_addr, &outlen);
}

esp_err_t zbm_to_ncp_req_get_extended_pan_id(esp_zb_ieee_addr_t ext_pan_id)
{
    uint16_t outlen = sizeof(esp_zb_ieee_addr_t);
    return esp_host_zb_output(ESP_NCP_NETWORK_EXTENDED_PAN_ID_GET, NULL, 0, ext_pan_id, &outlen);
}

uint16_t zbm_to_ncp_req_get_pan_id(void)
{
    uint16_t output = 0;
    uint16_t outlen = sizeof(uint16_t);

    esp_host_zb_output(ESP_NCP_NETWORK_PAN_ID_GET, NULL, 0, &output, &outlen);

    return output;
}

uint8_t zbm_to_ncp_req_get_current_channel(void)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    esp_host_zb_output(ESP_NCP_NETWORK_CHANNEL_GET, NULL, 0, &output, &outlen);

    return output;
}

uint16_t zbm_to_ncp_req_get_network_short_addr(void)
{
    uint16_t output = 0;
    uint16_t outlen = sizeof(uint16_t);
    
    esp_host_zb_output(ESP_NCP_NETWORK_SHORT_ADDRESS_GET, NULL, 0, &output, &outlen);

    return output;
}

uint8_t zbm_to_ncp_req_read_attributes(esp_zb_zcl_read_attr_cmd_t *cmd_req)
{
    ESP_LOGI(TAG, "Try to Read ATTR");
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);
    typedef struct {
        esp_zb_zcl_basic_cmd_t  zcl_basic_cmd;      /*!< Basic command info */
        uint8_t                 address_mode;       /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
        uint16_t                cluster_id;         /*!< Cluster ID to read */
        uint8_t                 attr_number;        /*!< Number of attribute in the attr_field */
        uint8_t                 flags;
        uint16_t                manuf_code;
    } __attribute__ ((packed)) esp_host_zb_read_attr_t;

    esp_host_zb_read_attr_t req ={
        .zcl_basic_cmd = {
            .dst_addr_u   = cmd_req->zcl_basic_cmd.dst_addr_u,
            //.dst_addr_u   = cmd_req->zcl_basic_cmd.dst_addr_u,
            .dst_endpoint = cmd_req->zcl_basic_cmd.dst_endpoint,
            .src_endpoint = cmd_req->zcl_basic_cmd.src_endpoint,
        },
        .address_mode = cmd_req->address_mode,
        .cluster_id   = cmd_req->clusterID,
        .attr_number  = cmd_req->attr_number,
        .flags = (cmd_req->manuf_specific & 0x03) |
                 ((cmd_req->direction & 0x01) << 2) |
                 ((cmd_req->dis_defalut_resp & 0x01) << 3),
        .manuf_code = cmd_req->manuf_code,
    };

    /*if (req.address_mode == ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT) {
        memcpy(&req.zcl_basic_cmd.dst_addr_u.addr_long, &cmd_req->zcl_basic_cmd.dst_addr_u.addr_long, 8);
    } else req.zcl_basic_cmd.dst_addr_u.addr_short = cmd_req->zcl_basic_cmd.dst_addr_u.addr_short;*/

    uint16_t attr_list_size = cmd_req->attr_number * sizeof(uint16_t);
    uint16_t inlen = sizeof(esp_host_zb_read_attr_t) + attr_list_size ;
    uint8_t  *input = calloc(1, inlen);
    if (!input) {
        ESP_LOGE(TAG, "Failed to allocate input buffer");
        return 0xFF;
    }
    
    memcpy(input, &req, sizeof(esp_host_zb_read_attr_t));
    memcpy(input + sizeof(esp_host_zb_read_attr_t), cmd_req->attr_field,cmd_req->attr_number * sizeof(uint16_t));
    esp_err_t err = ESP_FAIL;
    if (zigbee_ncp_module_state == WORKING)
        {
            err = esp_host_zb_output(ESP_NCP_ZCL_ATTR_READ_CMD, input, inlen, &output, &outlen);
        }
    free(input);
    input = NULL;

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send read_attr_cmd_req");
        return 0xFF;
    }
    
    ESP_LOGI(TAG, "Read Attr Req sent (TSN: %d) to 0x%04x, EP: %d, Cluster: 0x%04x, Attr Count: %d",
             output,
             req.zcl_basic_cmd.dst_addr_u.addr_short,
             req.zcl_basic_cmd.dst_endpoint,
             req.cluster_id,
             req.attr_number);
    //return ESP_OK;
    return output;
}