#include "zbm_zigbee_app_signal_handler.h"
#include "esp_log.h"
#include "esp_err.h"
#include "zbm_ncp_connect.h"
#include "ncp_host_zb_api_to_ncp.h"
#include "zbm_core_sync.h"
#include "zbm_zigbee_structures.h"

static const char *TAG = "ZBM_ZIGBEE_APP_SIHNAL_HANDLER";

void *esp_zb_app_signal_get_params(uint32_t *signal_p)
{
    local_esp_zb_app_signal_msg_t *app_signal_msg = (local_esp_zb_app_signal_msg_t *)signal_p;

    return app_signal_msg ? (void *)app_signal_msg->msg : (void *)app_signal_msg;
}

bool zbm_zigbee_app_signal_handler(local_esp_zb_app_signal_t *signal_s)
{
    uint32_t *p_sg_p       = signal_s->p_app_signal;
    esp_err_t err_status = signal_s->esp_err_status;
    local_esp_zb_app_signal_type_t sig_type = *p_sg_p;

    switch (sig_type){
        case ESP_ZB_BDB_SIGNAL_FORMATION://zbm_to_ncp_req_get_extended_pan_id
        {
            ESP_LOGI(TAG, "ESP_ZB_BDB_SIGNAL_FORMATION:");
            if (err_status == ESP_OK) {
                esp_zb_ieee_addr_t extended_pan_id;
                zbm_to_ncp_req_get_extended_pan_id(extended_pan_id);
                ESP_LOGI(TAG, "Formed network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                    extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                    extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                    zbm_to_ncp_req_get_pan_id(), zbm_to_ncp_req_get_current_channel(), zbm_to_ncp_req_get_network_short_addr());
                    
            } else {
                ESP_LOGI(TAG, "Restart network formation (status: %d)", err_status);
            }
            break;
        }
        case ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED:
        {
            ESP_LOGI(TAG, "ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED:");
            if (err_status == ESP_OK) {
                esp_zb_nwk_signal_device_associated_params_t* dev_assoc = (esp_zb_nwk_signal_device_associated_params_t*)esp_zb_app_signal_get_params(p_sg_p);
                zbm_dev_t* new_device = NULL;
                new_device = zbm_dev_create_and_add_to_devdb_by_ieee_safe(dev_assoc->device_addr);
                if (!new_device) {
                    ESP_LOGE(TAG, "❌ Failed to create device by IEEE");
                    break;
                }
                ESP_LOGI(TAG, "✅ Device added to devdb: %s", new_device->friendly_name);     
            }
            break;
        }
        case ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE:
        {
            ESP_LOGI(TAG, "ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE:");
            if (err_status == ESP_OK) {
                esp_zb_zdo_signal_device_update_params_t* dev_update = (esp_zb_zdo_signal_device_update_params_t*)esp_zb_app_signal_get_params(p_sg_p);
                zbm_dev_t* device_in_base = NULL;
                device_in_base = zbm_find_device_in_devdb_by_short_safe(dev_update->short_addr);
                if(device_in_base != NULL) break;
                device_in_base = zbm_find_device_in_devdb_by_ieee_safe(dev_update->long_addr);
                if (!device_in_base)
                {
                    ESP_LOGW(TAG, "Device not found in devdb");
                    break;
                }
                bool short_updated = false;
                short_updated = zbm_dev_update_short_addr_safe(device_in_base, dev_update->short_addr, dev_update->long_addr);
                if (short_updated)
                {
                    ESP_LOGI(TAG, "✅ Short address updated");
                    // test short
                    // Поиск устройства по short_addr
                    zbm_dev_t* dev_obj = zbm_find_device_in_devdb_by_short_safe(dev_update->short_addr);
                    if (!dev_obj) {
                        ESP_LOGW(TAG, "Device with short address 0x%04x not found in devdb", dev_update->short_addr);
                        // Всё равно парсим, чтобы освободить память
                    }
                    //create cmd params
                    esp_zb_zcl_read_attr_cmd_t cmd_req;
                    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
                    cmd_req.zcl_basic_cmd.src_endpoint = 1;
                    cmd_req.zcl_basic_cmd.dst_endpoint = 1;
                    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = dev_update->short_addr;
                    cmd_req.clusterID = ZBM_CLUSTER_ID_BASIC;
                    cmd_req.dis_defalut_resp = 1;
                    cmd_req.manuf_specific = 0;
                    cmd_req.manuf_code = 0x0000;
                    uint16_t attributes[]={0x0004, 0x0000, 0x0001, 0x0005, 0x0007, 0xfffe};
                    cmd_req.attr_number = sizeof(attributes) / sizeof(attributes[0]);
                    cmd_req.attr_field = calloc(1,sizeof(attributes[0]) * cmd_req.attr_number);
                    memcpy(cmd_req.attr_field, attributes, sizeof(uint16_t) * cmd_req.attr_number);
                    //send cmd
                    uint8_t tsn = 0xff;
                    tsn = zbm_to_ncp_req_read_attributes(&cmd_req);
                    if (tsn != 0xff)
                    {
                        ESP_LOGI(TAG, "ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE Send Tuya Magic wiyh TSN = %d", tsn );
                    }else
                    {
                        ESP_LOGW(TAG, "ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE Error Sending Tuya Magic");
                    } 
                    break;
                }
            }
            break;
        }
        case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        {
            break;
        }
        case ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED:
        {
            break;
        }
        default:
        /*ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));*/
        break;
    }
    return true;
}