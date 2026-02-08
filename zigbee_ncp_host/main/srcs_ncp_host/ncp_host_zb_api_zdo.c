#include "ncp_host_zb_api_zdo.h"
#include "ncp_host_zb_api.h"
#include "esp_log.h"
#include "esp_err.h"
#include "string.h"
#include "zb_manager_ncp_host.h"
static const char* TAG = "NCP_HOST_ZB_API_ZDO";


const char *esp_zb_zdo_signal_to_string(local_esp_zb_app_signal_type_t signal)
{
    return "The signal type of Zigbee";
}
//================================================================================================================================
//================================================ ACTIVE_EP_REQ / ACTIVE_EP_RESP ================================================
//================================================================================================================================

void zb_manager_free_active_ep_resp_ep_array(zb_manager_active_ep_resp_message_t *resp)
{
    if (!resp) return;

    if (resp->ep_list) {
        free(resp->ep_list);
        resp->ep_list = NULL;
    }

}

esp_err_t zb_manager_zdo_active_ep_req(local_esp_zb_zdo_active_ep_req_param_t *param, local_esp_zb_zdo_active_ep_callback_t user_cb, void *user_ctx)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);
    ESP_LOGI("HOST_ZDO_COMMAND_MODULE", "zb_manager_zdo_active_ep_req");
    typedef struct {
        esp_zb_user_cb_t find_usr;
        uint16_t dst_nwk_addr;              /*!< NWK address that request sent to */
    } __attribute__ ((packed)) esp_zb_zdo_active_ep_t;

    //ESP_LOGI(TAG, "esp_zb_zdo_active_ep_req _before zdo_data");
    esp_zb_zdo_active_ep_t zdo_data = {
        .find_usr = {
            .user_cb = (uint32_t)user_cb,
            .user_ctx = (uint32_t)user_ctx,
        },
       .dst_nwk_addr = param->addr_of_interest,
    };
    uint16_t inlen = sizeof(esp_zb_zdo_active_ep_t);  // параметры не передаются, как выше в esp_zb_zdo_match_cluster
    uint8_t  *input = calloc(1, inlen);
    if (input) {
        //ESP_LOGI(TAG, "esp_zb_zdo_active_ep_req _before memcopy");
        memcpy(input, &zdo_data, sizeof(esp_zb_zdo_active_ep_t));
        //ESP_LOGI(TAG, "esp_zb_zdo_active_ep_req _after memcopy");
        if (zigbee_ncp_module_state == WORKING)
        {
            if (esp_host_zb_output(ZB_MANAGER_ACTIVE_EP_REQ_CMD, input, inlen, &output, &outlen)==ESP_OK){
                ESP_LOGI(TAG, "esp_zb_zdo_active_ep_req _after esp_host_zb_output");
            }
        }else {
            ESP_LOGW(TAG, "esp_zb_zdo_active_ep_req zigbee_ncp_module_state != WORKING");
        }
        //ESP_LOGI(TAG, "esp_zb_zdo_active_ep_req _after esp_host_zb_output");
        free(input);
        input = NULL;
    }
    return ESP_OK;    
}


//================================================================================================================================
//============================================= SIMPLE_DESC_REQ / SIMPLE_DESC_RESP ===============================================
//================================================================================================================================
void zb_manager_free_simple_desc_resp(zb_manager_simple_desc_resp_message_t* resp)
{
    if (!resp) return;
    if (resp->simple_desc) {
        free(resp->simple_desc);
        resp->simple_desc = NULL;
    }
}

esp_err_t zb_manager_zdo_simple_desc_req(local_esp_zb_zdo_simple_desc_req_param_t *cmd_req, local_esp_zb_zdo_simple_desc_callback_t user_cb, void *user_ctx)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);
    typedef struct {
        esp_zb_user_cb_t user_data;
        uint16_t addr_of_interest;
        uint8_t endpoint;
    }  __attribute__ ((packed)) zb_manager_simple_desc_pack_req_t;
    
    zb_manager_simple_desc_pack_req_t zdo_data = {
        .user_data = {
            .user_cb = (uint32_t)user_cb,
            .user_ctx = (uint32_t)user_ctx,
        },
        .addr_of_interest = cmd_req->addr_of_interest,
        .endpoint = cmd_req->endpoint,
    };
    uint16_t inlen = sizeof(zb_manager_simple_desc_pack_req_t);
    uint8_t  *input = calloc(1, inlen);
    if (input) {
        memcpy(input, &zdo_data, sizeof(zb_manager_simple_desc_pack_req_t));
        if (zigbee_ncp_module_state == WORKING)
        {
            esp_host_zb_output(ZB_MANAGER_SIMPLE_DESC_REQ_CMD, input, inlen, &output, &outlen);
        }
        free(input);
        input = NULL;
    }
    return ESP_OK;
}


//================================================================================================================================
//============================================= BIND_REQ / BIND_RESP ===============================================
//================================================================================================================================

void zb_manager_free_bind_resp(zb_manager_bind_resp_message_t* resp)
{
    /*if (!resp) return;
    free(resp);*/
}

esp_err_t zb_manager_zdo_device_bind_req(local_esp_zb_zdo_bind_req_param_t *cmd_req, local_esp_zb_zdo_bind_callback_t user_cb, void *user_ctx)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    typedef struct {
        local_esp_zb_zdo_bind_req_param_t bind_req;
        esp_zb_user_cb_t            bind_usr;
    } esp_zb_zdo_bind_req_t;

    esp_zb_zdo_bind_req_t zdo_data = {
        .bind_usr = {
            .user_ctx = (uint32_t)user_ctx,
            .user_cb = (uint32_t)user_cb,
        },
    };

    memcpy(&zdo_data.bind_req, cmd_req, sizeof(local_esp_zb_zdo_bind_req_param_t));
    esp_err_t err = esp_host_zb_output(ESP_NCP_ZDO_BIND_SET, &zdo_data, sizeof(esp_zb_zdo_bind_req_t), &output, &outlen);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send bind_req");
    } else {
        ESP_LOGI(TAG, "bind_req sent ");
    }

    return err;
}


//================================================================================================================================
//============================================= NODE_DESC_REQ / NODE_DESC_RESP ===============================================
//================================================================================================================================

void zb_manager_free_node_desc_resp(zb_manager_node_desc_resp_message_t* resp)
{
    if (!resp) return;

}

esp_err_t zb_manager_zdo_node_desc_req(uint16_t short_addr)
{
    typedef struct {
        uint16_t nwk_addr;
    } ESP_ZNSP_ZB_PACKED_STRUCT zb_node_desc_req_t;

    zb_node_desc_req_t req = {
        .nwk_addr = short_addr,
    };

    esp_err_t err = ESP_FAIL;
    if (zigbee_ncp_module_state == WORKING)
        {
            err = esp_host_zb_output(ZB_MANAGER_NODE_DESC_REQ, &req, sizeof(req), NULL, NULL);
        }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send node_desc_req to 0x%04x", short_addr);
    } else {
        ESP_LOGI(TAG, "node_desc_req sent to 0x%04x", short_addr);
    }

    return err;
}

//================================================================================================================================
//============================================= MATCH_CLUSTER_REQ / MATCH_CLUSTER_RESP ===============================================
//================================================================================================================================

esp_err_t zb_manager_zdo_match_desc_cluster(local_esp_zb_zdo_match_desc_req_param_t *param, local_esp_zb_zdo_match_desc_callback_t user_cb, void *user_ctx)
{
    uint8_t output = 0;
    uint16_t outlen = sizeof(uint8_t);

    typedef struct {
        esp_zb_user_cb_t find_usr;
        uint16_t dst_nwk_addr;              /*!< NWK address that request sent to */
        uint16_t addr_of_interest;          /*!< NWK address of interest */
        uint16_t profile_id;                /*!< Profile ID to be match at the destination which refers to esp_zb_af_profile_id_t */
        uint8_t num_in_clusters;            /*!< The number of input clusters provided for matching cluster server */
        uint8_t num_out_clusters;           /*!< The number of output clusters provided for matching cluster client */
    } __attribute__ ((packed)) esp_zb_zdo_match_desc_t;

    esp_zb_zdo_match_desc_t zdo_data = {
        .find_usr = {
            .user_cb = (uint32_t)user_cb,
            .user_ctx = (uint32_t)user_ctx,
        },
        .dst_nwk_addr = param->dst_nwk_addr,
        .addr_of_interest = param->addr_of_interest,
        .profile_id = param->profile_id,
        .num_in_clusters = param->num_in_clusters,
        .num_out_clusters = param->num_out_clusters,
    };
    uint16_t clusters_len = (param->num_in_clusters + param->num_out_clusters) * sizeof(uint16_t);
    uint16_t inlen = sizeof(esp_zb_zdo_match_desc_t) + clusters_len;
    uint8_t  *input = calloc(1, inlen);
    if (input) {
        memcpy(input, &zdo_data, sizeof(esp_zb_zdo_match_desc_t));
        if (param->cluster_list && clusters_len) {
            memcpy(input + sizeof(esp_zb_zdo_match_desc_t), param->cluster_list, clusters_len);
        }

        if (zigbee_ncp_module_state == WORKING)
        {
            esp_host_zb_output(ESP_NCP_ZDO_FIND_MATCH, input, inlen, &output, &outlen);
        }

        free(input);
        input = NULL;
    }

    return ESP_OK;
}