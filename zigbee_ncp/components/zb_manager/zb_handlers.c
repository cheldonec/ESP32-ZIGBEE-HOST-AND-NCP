//#include "esp_zigbee_core.h"
#include "zb_handlers.h"
#include "esp_log.h"
#include "esp_check.h"
#include "string.h"
#include "zb_manager_endpoints_gen.h"
#include "zb_manager.h"
#include "esp_random.h"



static const char *TAG = "zb_handlers_module";

typedef struct custom_zb_nlme_status_indication_s
{
  uint8_t status; /**< Error code associated with the failure */
  uint16_t network_addr;  /**< Network device address associated with the status information */
  uint8_t unknown_command_id; //< Unknown command ID
  uint8_t unknown_byte;
  esp_zb_ieee_addr_t ieee_address;
} custom_zb_nlme_status_indication_t;



#define BDB_FIND_BIND_TARGET_DURATION (180) // 180 seconds
esp_err_t zb_bdb_enter_finding_binding_target(uint8_t ep)
{
    if (esp_zb_bdb_dev_joined()) {
        return esp_zb_bdb_finding_binding_start_target(ep, BDB_FIND_BIND_TARGET_DURATION);
    } else {
        return ESP_ERR_INVALID_STATE;
    }
}
/************************ indication for end device END ***************************/
static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG, "Failed to start Zigbee bdb commissioning");
}

bool find_binding_initiator_callback(int16_t status, esp_zb_ieee_addr_t ieee_addr, uint8_t ep_id, uint16_t cluster_id)
{
    ESP_LOGW(TAG, "Find binding initiator callback");
    if (status == ESP_OK) {
        ESP_LOGW(TAG, "Find binding initiator callback successful!");
        ESP_LOGW(TAG, "Resp from device: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, ep: %d, cluster: 0x%04x",
                 ieee_addr[7], ieee_addr[6], ieee_addr[5], ieee_addr[4],
                 ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0],
                 ep_id, cluster_id);
    } else {
        ESP_LOGE(TAG, "Find binding initiator callback failed with status: %d", status);
    }
    return true;
}

// Запуск F&B как Initiator (инициатор)
esp_err_t start_finding_binding(uint8_t local_ep)
{
    uint8_t endpoint = local_ep; // Ваш endpoint

    if (esp_zb_bdb_dev_joined()) {
        ESP_LOGW(TAG, "Start Finding & Binding as Initiator on endpoint %d", endpoint);

        // Запускаем F&B initiator на 60 секунд
        return esp_zb_bdb_finding_binding_start_initiator(endpoint, find_binding_initiator_callback);
    } else {
        ESP_LOGW(TAG, "Device not joined to network, cannot start F&B");
        return ESP_ERR_INVALID_STATE;
    }
}

#ifdef CONFIG_ZB_MANAGER_ZC

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_nwk_signal_device_associated_params_t*  dev_assoc_params = NULL;
    esp_zb_zdo_signal_device_annce_params_t*       dev_annce_params = NULL;
    esp_zb_zdo_signal_device_authorized_params_t*  dev_auth_params = NULL;
    esp_zb_zdo_signal_device_update_params_t*      dev_update_params = NULL;
    esp_zb_zdo_signal_leave_indication_params_t*   leave_ind_params = NULL;
    esp_zb_zdo_device_unavailable_params_t*        dev_unavalible = NULL;
    char buff[256];
    esp_ncp_header_t ncp_header = { 
        .sn = esp_random() % 0xFF,
    };
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        //esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        //if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device first start");
                esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
                //esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION); 
        //}
    break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            //esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network formation");
                esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
                //esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
            } else {
                //esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
                esp_zb_bdb_open_network(60);
                ESP_LOGI(TAG, "Device rebooted");
                
                
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status: %s, retrying", esp_zb_zdo_signal_to_string(sig_type),
                     esp_err_to_name(err_status));
            //esp_zb_bdb_open_network(60);
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_FORMATION:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t ieee_address;
            esp_zb_get_long_address(ieee_address);
            ESP_LOGI(TAG, "Formed network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04x, Channel:%d, Short Address: 0x%04x)",
                     ieee_address[7], ieee_address[6], ieee_address[5], ieee_address[4],
                     ieee_address[3], ieee_address[2], ieee_address[1], ieee_address[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            typedef struct {
                    esp_zb_ieee_addr_t  extendedPanId;                     
                    uint16_t            panId;                              
                    uint8_t             radioChannel;                       
                } ESP_NCP_ZB_PACKED_STRUCT esp_ncp_zb_formnetwork_parameters_t;

                esp_ncp_zb_formnetwork_parameters_t parameters = { 
                    .panId = esp_zb_get_pan_id(),
                    .radioChannel = esp_zb_get_current_channel(),
                };
                esp_zb_get_extended_pan_id(parameters.extendedPanId);

                ESP_LOGI(TAG, "Formed network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d)",
                        parameters.extendedPanId[7], parameters.extendedPanId[6], parameters.extendedPanId[5], parameters.extendedPanId[4],
                        parameters.extendedPanId[3], parameters.extendedPanId[2], parameters.extendedPanId[1], parameters.extendedPanId[0],
                        parameters.panId, parameters.radioChannel);
                ncp_header.id = ESP_NCP_NETWORK_FORMNETWORK;
                esp_ncp_noti_input(&ncp_header, &parameters, sizeof(esp_ncp_zb_formnetwork_parameters_t));
        } else {
            ESP_LOGI(TAG, "Restart network formation (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Network steering started");
        } else {
            ESP_LOGW(TAG, "Not found network");
        }
        break;
    case ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED:
       
        if(err_status == ESP_OK)
            {
                ESP_LOGI(TAG, "ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED:");
                dev_assoc_params = (esp_zb_nwk_signal_device_associated_params_t*)esp_zb_app_signal_get_params(p_sg_p);
                
                sprintf(buff, "DEVICE_ASSOCIATED : %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", dev_assoc_params->device_addr[7], 
                    dev_assoc_params->device_addr[6], dev_assoc_params->device_addr[5], dev_assoc_params->device_addr[4],
                    dev_assoc_params->device_addr[3], dev_assoc_params->device_addr[2], dev_assoc_params->device_addr[1], dev_assoc_params->device_addr[0]);
                //  создаём устройство !!! Проверено
                // 1. ищем по списку
                bool device_found = false;         // если будет найдено, то меняем статус is_in_build_status, а значит после dev_annce надо будет его опросить как новое
                /*for (int i = 0; i < zb_manager_obj.RemoteDevicesCount; i++)
                {
                    if (zb_manager_obj.RemoteDevicesArray[i]!=NULL) 
                    {
                        if (ieee_addr_compare(&zb_manager_obj.RemoteDevicesArray[i]->ieee_addr, &dev_assoc_params->device_addr) == 0)
                        {
                            device_found = true;
                            ESP_LOGW(TAG, "ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED  device found in RemoteDevList FLAG Update set enable!!!");
                            if (zb_manager_obj.RemoteDevicesArray[i]->endpoints_count > 0)
                            {
                                for (int j = 0; j < zb_manager_obj.RemoteDevicesArray[i]->endpoints_count; j++)
                                {
                                    RemoteDeviceEndpointDelete(zb_manager_obj.RemoteDevicesArray[i]->endpoints_array[j]);
                                }
                            }
                            zb_manager_obj.RemoteDevicesArray[i]->is_in_build_status = true;
                            //memcpy(temp_ieee, zb_manager_obj.RemoteDevicesArray[i], 8);
                            break;
                        }
                    }
                }*/
                // 2 если не нашли, то надо найти свободную ячейку и создать новое
                /*if (device_found == false)
                {
                    uint8_t free_index = REMOTE_DEVICES_COUNT + 1; // несуществующий индекс
                    for(int i = 0; i < REMOTE_DEVICES_COUNT; i++)
                    {  
                        if (zb_manager_obj.RemoteDevicesArray[i] == NULL)
                        {
                            free_index = i;
                            break;
                        }
                    }
                    if (free_index < REMOTE_DEVICES_COUNT + 1)
                    {
                        esp_zb_ieee_addr_t temp_ieee;
                        memcpy(temp_ieee, dev_assoc_params->device_addr, sizeof(esp_zb_ieee_addr_t));
                        zb_manager_obj.RemoteDevicesArray[free_index] = RemoteDeviceCreate(temp_ieee);
                        if (zb_manager_obj.RemoteDevicesArray[free_index] != NULL) 
                        {
                            zb_manager_obj.RemoteDevicesArray[free_index]->is_in_build_status = true;
                            ESP_LOGI(TAG, "Создано пустое устройство под индексом %d (mac: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)", free_index, dev_assoc_params->device_addr[7], 
                            dev_assoc_params->device_addr[6], dev_assoc_params->device_addr[5], dev_assoc_params->device_addr[4],
                            dev_assoc_params->device_addr[3], dev_assoc_params->device_addr[2], dev_assoc_params->device_addr[1], dev_assoc_params->device_addr[0]);
                        }
                    } else ESP_LOGW(TAG, "ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED Список устройств занят, для добавления надо расширить список");
                }*/
                ncp_header.id = ZB_MANAGER_DEV_ASSOCIATED_EVENT;
                esp_ncp_noti_input(&ncp_header, dev_assoc_params, sizeof(esp_zb_nwk_signal_device_associated_params_t));
            }
        break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE:
       
        if(err_status == ESP_OK)
            {
                ESP_LOGI(TAG, "ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE:");
                dev_update_params = (esp_zb_zdo_signal_device_update_params_t *)esp_zb_app_signal_get_params(p_sg_p);
                /*sprintf(buff, "DEVICE_UPDATE : %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x (short: 0x%04x) (update_status: 0x%02hx)", 
                dev_update_params->long_addr[7], dev_update_params->long_addr[6], dev_update_params->long_addr[5], dev_update_params->long_addr[4],
                     dev_update_params->long_addr[3], dev_update_params->long_addr[2], dev_update_params->long_addr[1], dev_update_params->long_addr[0], 
                     dev_update_params->short_addr, dev_update_params->status);*/
                //zb_manager_obj.utility_functions_callbacks.print_log_to_screen_callback(buff, 0xA7BFC1);
                ESP_LOGI(TAG, "device update (short: 0x%04x) (update_status: 0x%02hx) (mac: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x) (status:0x%02x)", 
                dev_update_params->short_addr, dev_update_params->status, dev_update_params->long_addr[7], dev_update_params->long_addr[6], dev_update_params->long_addr[5], dev_update_params->long_addr[4],
                     dev_update_params->long_addr[3], dev_update_params->long_addr[2], dev_update_params->long_addr[1], dev_update_params->long_addr[0],dev_update_params->status);

                ncp_header.id = ZB_MANAGER_DEV_UPDATE_EVENT;
                esp_ncp_noti_input(&ncp_header, dev_update_params, sizeof(esp_zb_zdo_signal_device_update_params_t));
            }
        dev_update_params = NULL;
        break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        if(err_status == ESP_OK)
            {
                ESP_LOGW(TAG, "ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:");
                dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);

                /*sprintf(buff, "DEVICE_ANNCE : %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x (short: 0x%04x)", 
                dev_annce_params->ieee_addr[7], dev_annce_params->ieee_addr[6], dev_annce_params->ieee_addr[5], dev_annce_params->ieee_addr[4],
                     dev_annce_params->ieee_addr[3], dev_annce_params->ieee_addr[2], dev_annce_params->ieee_addr[1], dev_annce_params->ieee_addr[0], 
                     dev_annce_params->device_short_addr);*/
                //zb_manager_obj.utility_functions_callbacks.print_log_to_screen_callback(buff, 0xA7BFC1);

                ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04x) (mac: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)", dev_annce_params->device_short_addr, dev_annce_params->ieee_addr[7], dev_annce_params->ieee_addr[6], dev_annce_params->ieee_addr[5], dev_annce_params->ieee_addr[4],
                     dev_annce_params->ieee_addr[3], dev_annce_params->ieee_addr[2], dev_annce_params->ieee_addr[1], dev_annce_params->ieee_addr[0]);
                
                //ncp_header.id = ESP_NCP_NETWORK_JOINNETWORK;
                //esp_ncp_noti_input(&ncp_header, dev_annce_params, sizeof(esp_zb_zdo_signal_device_annce_params_t));

                
                ncp_header.id = ESP_NCP_NETWORK_JOINNETWORK;
                esp_ncp_noti_input(&ncp_header, dev_annce_params, sizeof(esp_zb_zdo_signal_device_annce_params_t));
            }
        dev_annce_params = NULL;
        break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED:
        if(err_status == ESP_OK)
            {    
                dev_auth_params = (esp_zb_zdo_signal_device_authorized_params_t *)esp_zb_app_signal_get_params(p_sg_p);

                /*sprintf(buff, "DEVICE_AUTHORIZED : %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x (short: 0x%04x) (auth_type: 0x%02x) (auth_status: 0x%02x)", 
                dev_auth_params->long_addr[7], dev_auth_params->long_addr[6], dev_auth_params->long_addr[5], dev_auth_params->long_addr[4],
                     dev_auth_params->long_addr[3], dev_auth_params->long_addr[2], dev_auth_params->long_addr[1], dev_auth_params->long_addr[0], 
                     dev_auth_params->short_addr, dev_auth_params->authorization_type, dev_auth_params->authorization_status);
                //zb_manager_obj.utility_functions_callbacks.print_log_to_screen_callback(buff, 0xA7BFC1);*/

                ESP_LOGI(TAG, "device authorized (short: 0x%04x) (auth_type: 0x%02x) (auth_status: 0x%02x) (mac: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)", 
                dev_auth_params->short_addr, dev_auth_params->authorization_type, dev_auth_params->authorization_status, dev_auth_params->long_addr[7], dev_auth_params->long_addr[6], dev_auth_params->long_addr[5], dev_auth_params->long_addr[4],
                     dev_auth_params->long_addr[3], dev_auth_params->long_addr[2], dev_auth_params->long_addr[1], dev_auth_params->long_addr[0]);

                ncp_header.id = ZB_MANAGER_DEV_AUTH_EVENT;
                esp_ncp_noti_input(&ncp_header, dev_auth_params, sizeof(esp_zb_zdo_signal_device_authorized_params_t));
            }

        dev_auth_params = NULL;
        break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        ESP_LOGI(TAG, "3.ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:");
        char ComPortInfoMsg[128];
        if (err_status == ESP_OK) {
            uint8_t *parameters = esp_zb_app_signal_get_params(p_sg_p);
                if (*parameters) {
                    ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *parameters);
                } else {
                    ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
                }
                ncp_header.id = ESP_NCP_NETWORK_PERMIT_JOINING;
                esp_ncp_noti_input(&ncp_header, parameters, sizeof(uint8_t));
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
        ESP_LOGI(TAG, "Production configuration is ready");
        if (err_status == ESP_OK) {
            /*app_production_config_t *prod_cfg = (app_production_config_t *)esp_zb_app_signal_get_params(p_sg_p);
            if (prod_cfg->version == APP_PROD_CFG_CURRENT_VERSION) {
                ESP_LOGI(TAG, "Manufacturer_code: 0x%x, manufacturer_name:%s", prod_cfg->manuf_code, prod_cfg->manuf_name);
                esp_zb_set_node_descriptor_manufacturer_code(prod_cfg->manuf_code);
            }
            prod_cfg = NULL;*/
        } else {
            ESP_LOGW(TAG, "Production configuration is not present");
        }
        break;
    case ESP_ZB_ZDO_DEVICE_UNAVAILABLE:
        if(err_status == ESP_OK)
            {    
                dev_unavalible = (esp_zb_zdo_device_unavailable_params_t *)esp_zb_app_signal_get_params(p_sg_p);
                ESP_LOGI(TAG, "ESP_ZB_ZDO_DEVICE_UNAVAILABLE: 0x%x", dev_unavalible->short_addr);
            }
        dev_unavalible = NULL;
        break;

    case ESP_ZB_ZDO_SIGNAL_LEAVE:
        ESP_LOGI(TAG, "ESP_ZB_ZDO_SIGNAL_LEAVE:");
        leave_ind_params = (esp_zb_zdo_signal_leave_indication_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        if (!leave_ind_params->rejoin) {
            esp_zb_ieee_addr_t leave_addr;
            memcpy(leave_addr, leave_ind_params->device_addr, sizeof(esp_zb_ieee_addr_t));
            ESP_LOGI(TAG, "Zigbee Node is leaving network: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)",
                     leave_addr[7], leave_addr[6], leave_addr[5], leave_addr[4],
                     leave_addr[3], leave_addr[2], leave_addr[1], leave_addr[0]);
        }
        leave_ind_params = NULL;
        break;

    case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION: // если rejoin = 1 то устройство ушло спать, если 0, то покидает сеть и надо отпустить
        ESP_LOGI(TAG, "ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION:");
        
        //esp_zb_bdb_open_network(10);
        /*leave_ind_params = (esp_zb_zdo_signal_leave_indication_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        if(err_status == ESP_OK)
            { 
                if (leave_ind_params->rejoin == 0) {
                esp_zb_ieee_addr_t leave_addr;
                memcpy(leave_addr, leave_ind_params->device_addr, sizeof(esp_zb_ieee_addr_t));
                ESP_LOGI(TAG, "Zigbee Node is leaving network: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)",
                     leave_addr[7], leave_addr[6], leave_addr[5], leave_addr[4],
                     leave_addr[3], leave_addr[2], leave_addr[1], leave_addr[0]);
            } else esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        }*/

        leave_ind_params = NULL;
        break;
    case ESP_ZB_NLME_STATUS_INDICATION:
        custom_zb_nlme_status_indication_t* nlme_status = (custom_zb_nlme_status_indication_t*)esp_zb_app_signal_get_params(p_sg_p);
        /*ESP_LOGI(TAG, "Device nlme_status_indication (status code: %02x) (network address: 0x%04hx) (unknown command id: %02x) (dev mac: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)",
        nlme_status->status,nlme_status->network_addr, nlme_status->unknown_command_id,nlme_status->ieee_address[7], nlme_status->ieee_address[6], 
        nlme_status->ieee_address[5], nlme_status->ieee_address[4],nlme_status->ieee_address[3], nlme_status->ieee_address[2], 
        nlme_status->ieee_address[1], nlme_status->ieee_address[0]);*/
        nlme_status = NULL;
        break;
    case ESP_ZB_BDB_SIGNAL_FINDING_AND_BINDING_INITIATOR_FINISHED:
        ESP_LOGW(TAG, "ESP_ZB_BDB_SIGNAL_FINDING_AND_BINDING_INITIATOR_FINISHED");
        break;  
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
                 //esp_zb_get_long_address
        break;
    }
}
#endif

#ifdef CONFIG_ZB_MANAGER_ZR
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_leave_indication_params_t *leave_ind_params = NULL;
    esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;
    esp_ncp_header_t ncp_header = { 
        .sn = esp_random() % 0xFF,
    };
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:// добавлено для роутера
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        //esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    if (err_status == ESP_OK) {
        ESP_LOGI(TAG, "Device first start");
        esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
    }
    break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:// добавлено для роутера
     if (err_status == ESP_OK) {
            //ESP_LOGI(TAG, "Deferred driver initialization %s", deferred_driver_init() ? "failed" : "successful");
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
                //esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status: %s, retrying", esp_zb_zdo_signal_to_string(sig_type),
                     esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
   
    case ESP_ZB_BDB_SIGNAL_STEERING: 
    if (err_status != ESP_OK) {
            ESP_LOGW(TAG, "Stack %s failure with %s status, steering",esp_zb_zdo_signal_to_string(sig_type), esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        } else {
           
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            esp_zb_zdo_match_desc_req_param_t  find_req;
            find_req.addr_of_interest = 0xfffd;
            find_req.dst_nwk_addr = 0xffff;
            
            //esp_zb_zdo_find_on_off_light(&find_req, user_find_cb, NULL);
        }
        break;
   
    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)", dev_annce_params->device_short_addr);
        break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE:
            leave_ind_params = (esp_zb_zdo_signal_leave_indication_params_t *)esp_zb_app_signal_get_params(p_sg_p);
            ESP_LOGI(TAG, "Leave Indication parameters (short: 0x%04hx)", leave_ind_params->short_addr);
            ncp_header.id = ESP_NCP_NETWORK_LEAVENETWORK;
            esp_ncp_noti_input(&ncp_header, leave_ind_params, sizeof(esp_zb_zdo_signal_leave_indication_params_t));
            break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION:
        leave_ind_params = (esp_zb_zdo_signal_leave_indication_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        if (!leave_ind_params->rejoin) {
            esp_zb_ieee_addr_t leave_addr;
            memcpy(leave_addr, leave_ind_params->device_addr, sizeof(esp_zb_ieee_addr_t));
            ESP_LOGI(TAG, "Zigbee Node is leaving network: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)",
                     leave_addr[7], leave_addr[6], leave_addr[5], leave_addr[4],
                     leave_addr[3], leave_addr[2], leave_addr[1], leave_addr[0]);
            ncp_header.id = ESP_NCP_NETWORK_JOINNETWORK;
            esp_ncp_noti_input(&ncp_header, dev_annce_params, sizeof(esp_zb_zdo_signal_device_annce_params_t));
        }
        break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:// добавлено для роутера
        if (err_status == ESP_OK) {
            if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
                ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
            } else {
                ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
            }
            uint8_t *parameters = esp_zb_app_signal_get_params(p_sg_p);
                if (*parameters) {
                    ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *parameters);
                } else {
                    ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
                }
                ncp_header.id = ESP_NCP_NETWORK_PERMIT_JOINING;
                esp_ncp_noti_input(&ncp_header, parameters, sizeof(uint8_t));
        }
        break;
    case ESP_ZB_NLME_STATUS_INDICATION:
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}
#endif

#ifdef CONFIG_ZB_ZED
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_leave_params_t *leave_params = NULL;
    esp_ncp_header_t ncp_header = { 
        .sn = esp_random() % 0xFF,
    };
    //esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;
    switch (sig_type) {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:// добавлено для роутера
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        //esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    if (err_status == ESP_OK) {
        ESP_LOGI(TAG, "Device first start");
        esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
    }
    
    break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
     if (err_status == ESP_OK) {
            
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                //esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
                esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
                //ESP_LOGI(TAG, "Ready for F&B target: %d", zb_bdb_enter_finding_binding_target(HA_REMOTE_DEVICE_ENDPOINT));
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status: %s, retrying", esp_zb_zdo_signal_to_string(sig_type),
                     esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
                     //ESP_LOGI(TAG, "Ready for F&B target: %d", zb_bdb_enter_finding_binding_target(HA_REMOTE_DEVICE_ENDPOINT));
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
   
    case ESP_ZB_ZDO_SIGNAL_LEAVE:
        leave_params = (esp_zb_zdo_signal_leave_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        if (leave_params->leave_type == ESP_ZB_NWK_LEAVE_TYPE_RESET) {
            ESP_LOGI(TAG, "Reset device");
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}
#endif

/******************************************************************************************************************** */
/******************************************************************************************************************** */
/******************************************************************************************************************** */
/******************************************************************************************************************** */
/******************************************************************************************************************** */
/******************************************************************************************************************** */
/*************************************            ACTION HANDLERS             *************************************** */
/******************************************************************************************************************** */
/******************************************************************************************************************** */
/******************************************************************************************************************** */
/******************************************************************************************************************** */
static esp_err_t esp_ncp_zb_read_attr_resp_handler(const esp_zb_zcl_cmd_read_attr_resp_message_t *message, uint8_t **output, uint16_t *outlen)
{
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Read attribute response: status(%d), cluster(0x%x)", message->info.status, message->info.cluster);
    
    uint16_t data_head_len = sizeof(esp_zb_zcl_cmd_info_t);
    uint16_t id_len = sizeof(uint16_t), TYPE_LEN = sizeof(esp_zb_zcl_attr_type_t), SIZE_LEN = sizeof(uint8_t);
    uint8_t index = 0;
    uint16_t length = (data_head_len + 1);
    uint8_t *outbuf = calloc(1, length);
    uint8_t *variables_data = NULL;
    uint8_t  variables_data_len = 0;
    uint8_t  variables_len = (id_len + TYPE_LEN + SIZE_LEN);

    if (outbuf) {
        memcpy(outbuf, &message->info, data_head_len);
        for (esp_zb_zcl_read_attr_resp_variable_t *variables = message->variables; variables != NULL; variables = variables->next) {
            ESP_LOGI(TAG, "attribute(0x%x), type(0x%x), value(%d)", variables->attribute.id, variables->attribute.data.type, variables->attribute.data.value ? *(uint8_t *)variables->attribute.data.value : 0);
            
            variables_data_len = (variables_len + variables->attribute.data.size);
            length += variables_data_len;
            
            outbuf = realloc(outbuf, length);
            variables_data = &outbuf[length - variables_data_len];

            memcpy(variables_data, &variables->attribute.id, variables_len);
            variables_data += variables_len;

            memcpy(variables_data, variables->attribute.data.value, variables->attribute.data.size);
            index ++;
        }
        outbuf[data_head_len] = index;
    }

    *output = outbuf;
    *outlen = length;

    return ESP_OK;
}

static esp_err_t esp_ncp_zb_write_attr_resp_handler(const esp_zb_zcl_cmd_write_attr_resp_message_t *message, uint8_t **output, uint16_t *outlen)
{
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    
    ESP_LOGI(TAG, "Write attribute response: status(%d), cluster(0x%x)", message->info.status, message->info.cluster);

    uint16_t data_head_len = sizeof(esp_zb_zcl_cmd_info_t);
    uint16_t id_len = sizeof(uint16_t), status_len = sizeof(esp_zb_zcl_status_t);
    uint8_t index = 0;
    uint16_t length = (data_head_len + 1);
    uint8_t *outbuf = calloc(1, length);
    uint8_t variables_data_len = (id_len + status_len);

    if (outbuf) {
        memcpy(outbuf, &message->info, data_head_len);
        for (esp_zb_zcl_write_attr_resp_variable_t *variables = message->variables; variables != NULL; variables = variables->next) {
            ESP_LOGI(TAG, "status(0x%x), attribute(0x%x)", variables->status, variables->attribute_id);
            length += variables_data_len;
            
            outbuf = realloc(outbuf, length);
            memcpy(&outbuf[length - variables_data_len], &variables->status, variables_data_len);

            index ++;
        }
        outbuf[data_head_len] = index;
    }

    *output = outbuf;
    *outlen = length;

    return ESP_OK;
}

static esp_err_t esp_ncp_zb_report_configure_resp_handler(const esp_zb_zcl_cmd_config_report_resp_message_t *message, uint8_t **output, uint16_t *outlen)
{
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    // Не проверяем статус — даже ошибка должна быть отправлена

    ESP_LOGI(TAG, "Configure report response: status(%d), cluster(0x%04x)", message->info.status, message->info.cluster);

    // Структуры для сериализации
    typedef struct {
        esp_zb_zcl_cmd_info_t info;
        uint8_t count;
    } ESP_NCP_ZB_PACKED_STRUCT esp_ncp_zb_report_config_resp_hdr_t;

    typedef struct {
        uint8_t direction;
        uint16_t attr_id;
        uint8_t status;
    } ESP_NCP_ZB_PACKED_STRUCT esp_ncp_zb_report_config_resp_item_t;

    // Шаг 1: Подсчитаем количество переменных
    uint8_t item_count = 0;
    const esp_zb_zcl_config_report_resp_variable_t *var = message->variables;
    while (var) {
        item_count++;
        var = var->next;
    }

    // Проверка: не слишком ли много?
    if (item_count > 10) {
        ESP_LOGE(TAG, "Too many report config items: %u", item_count);
        return ESP_ERR_INVALID_SIZE;
    }

    // Шаг 2: Вычислим общий размер
    uint16_t hdr_len = sizeof(esp_ncp_zb_report_config_resp_hdr_t);
    uint16_t item_len = sizeof(esp_ncp_zb_report_config_resp_item_t);
    uint16_t total_len = hdr_len + item_len * item_count;

    // Шаг 3: Выделим память один раз
    uint8_t *outbuf = calloc(1, total_len);
    if (!outbuf) {
        ESP_LOGE(TAG, "Failed to allocate outbuf");
        return ESP_ERR_NO_MEM;
    }

    // Шаг 4: Заполним заголовок
    esp_ncp_zb_report_config_resp_hdr_t *hdr = (esp_ncp_zb_report_config_resp_hdr_t *)outbuf;
    hdr->info = message->info;
    hdr->count = item_count;

    // Шаг 5: Заполним элементы
    var = message->variables;
    esp_ncp_zb_report_config_resp_item_t *item = (esp_ncp_zb_report_config_resp_item_t *)(outbuf + hdr_len);
    while (var) {
        item->direction = var->direction;
        item->attr_id = var->attribute_id;
        item->status = var->status;

        ESP_LOGI(TAG, "  [Dir=%d] Attr=0x%04x, Status=0x%02x", item->direction, item->attr_id, item->status);

        item++;
        var = var->next;
    }

    // Шаг 6: Установим выходные параметры
    *output = outbuf;
    *outlen = total_len;

    return ESP_OK;
}

static esp_err_t esp_ncp_zb_disc_attr_resp_handler(const esp_zb_zcl_cmd_discover_attributes_resp_message_t *message, uint8_t **output, uint16_t *outlen)
{
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    
    ESP_LOGI(TAG, "Discover attribute response: status(%d), cluster(0x%x)", message->info.status, message->info.cluster);

    uint16_t data_head_len = sizeof(esp_zb_zcl_cmd_info_t);
    uint16_t id_len = sizeof(uint16_t), status_len = sizeof(esp_zb_zcl_attr_type_t);
    uint8_t index = 0;
    uint16_t length = (data_head_len + 1);
    uint8_t *outbuf = calloc(1, length);
    uint8_t variables_data_len = (id_len + status_len);

    if (outbuf) {
        memcpy(outbuf, &message->info, data_head_len);
        for (esp_zb_zcl_disc_attr_variable_t *variables = message->variables; variables != NULL; variables = variables->next) {
            ESP_LOGI(TAG, "attribute(0x%x), data_type(0x%0x)", variables->attr_id, variables->data_type);
            length += variables_data_len;
            
            outbuf = realloc(outbuf, length);
            memcpy(&outbuf[length - variables_data_len], &variables->attr_id, variables_data_len);

            index ++;
        }
        outbuf[data_head_len] = index;
    }

    *output = outbuf;
    *outlen = length;

    return ESP_OK;
}

static esp_err_t esp_ncp_zb_report_attr_handler_new(const esp_zb_zcl_report_attr_message_t *message, uint8_t **output, uint16_t *outlen)
{
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Error status: %d", message->status);

    // Валидация размера значения
    if (message->attribute.data.size > 64) {
        ESP_LOGE(TAG, "Attribute size too large: %u", message->attribute.data.size);
        return ESP_ERR_INVALID_SIZE;
    }

    // Определяем структуры
    typedef struct {
        esp_zb_zcl_status_t status;
        esp_zb_zcl_addr_t src_address;
        uint8_t src_endpoint;
        uint8_t dst_endpoint;
        uint16_t cluster;
    } ESP_NCP_ZB_PACKED_STRUCT esp_ncp_zb_report_attr_t;

    typedef struct {
        uint16_t id;
        uint8_t  type;
        uint8_t  size;
    } ESP_NCP_ZB_PACKED_STRUCT esp_ncp_zb_attr_data_t;

    // Вычисляем длины
    uint16_t hdr_len = sizeof(esp_ncp_zb_report_attr_t);
    uint16_t attr_hdr_len = sizeof(esp_ncp_zb_attr_data_t);
    uint8_t value_len = (message->attribute.data.value && message->attribute.data.size > 0) ? message->attribute.data.size : 0;
    uint16_t total_len = hdr_len + attr_hdr_len + value_len;

    // Выделяем буфер
    uint8_t *outbuf = calloc(1, total_len);
    if (!outbuf) {
        ESP_LOGE(TAG, "Failed to allocate outbuf");
        return ESP_ERR_NO_MEM;
    }

    // 1. Копируем основной заголовок
    esp_ncp_zb_report_attr_t *hdr = (esp_ncp_zb_report_attr_t *)outbuf;
    hdr->status = message->status;
    memcpy(&hdr->src_address, &message->src_address, sizeof(hdr->src_address));
    hdr->src_endpoint = message->src_endpoint;
    hdr->dst_endpoint = message->dst_endpoint;
    hdr->cluster = message->cluster;

    // 2. Копируем заголовок атрибута
    esp_ncp_zb_attr_data_t *attr_hdr = (esp_ncp_zb_attr_data_t *)(outbuf + hdr_len);
    attr_hdr->id = message->attribute.id;
    attr_hdr->type = message->attribute.data.type;
    attr_hdr->size = value_len;

    // 3. Копируем значение (если есть)
    if (value_len > 0 && message->attribute.data.value) {
        memcpy(outbuf + hdr_len + attr_hdr_len, message->attribute.data.value, value_len);
    }

    // 4. Устанавливаем выходные параметры
    *output = outbuf;
    *outlen = total_len;

    ESP_LOGI(TAG, "Serialized report_attr: cluster=0x%04x, attr_id=0x%04x, type=0x%02x, size=%u",
             hdr->cluster, attr_hdr->id, attr_hdr->type, attr_hdr->size);

    return ESP_OK;
}


/******************************************************** С NCP ***********************************************************/
esp_err_t esp_ncp_zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    esp_ncp_header_t ncp_header = { 
        .sn = esp_random() % 0xFF,
    };
    uint8_t *output = NULL;
    uint16_t outlen = 0;

    switch (callback_id) {
        case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
            ncp_header.id = ESP_NCP_ZCL_ATTR_READ_RESP;
            ret = esp_ncp_zb_read_attr_resp_handler((esp_zb_zcl_cmd_read_attr_resp_message_t *)message, &output, &outlen);
            break;
        case ESP_ZB_CORE_CMD_WRITE_ATTR_RESP_CB_ID:
            ncp_header.id = ESP_NCP_ZCL_ATTR_WRITE_RESP;
            ret = esp_ncp_zb_write_attr_resp_handler((esp_zb_zcl_cmd_write_attr_resp_message_t *)message, &output, &outlen);
            break;
        case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID:
            ncp_header.id = ZB_MANAGER_REPORT_CONFIG_RESP;
            ret = esp_ncp_zb_report_configure_resp_handler((esp_zb_zcl_cmd_config_report_resp_message_t *)message, &output, &outlen);
            
            break;
        case ESP_ZB_CORE_CMD_DISC_ATTR_RESP_CB_ID:
            ncp_header.id = ESP_NCP_ZCL_ATTR_DISC_RESP;
            ret = esp_ncp_zb_disc_attr_resp_handler((esp_zb_zcl_cmd_discover_attributes_resp_message_t *)message, &output, &outlen);
            break;
        case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
            ncp_header.id = ESP_NCP_ZCL_ATTR_REPORT_EVENT;
            ret = esp_ncp_zb_report_attr_handler_new((esp_zb_zcl_report_attr_message_t *)message, &output, &outlen);
            break;
        case ESP_ZB_CORE_IDENTIFY_EFFECT_CB_ID:
        ESP_LOGW(TAG, "Receive Zigbee action ESP_ZB_CORE_IDENTIFY_EFFECT_CB_ID (0x%x) callback", callback_id);
        //ret = zb_identify_effect_handler((esp_zb_zcl_identify_effect_message_t *)message);
        esp_zb_zcl_identify_effect_message_t * id_msg = (esp_zb_zcl_identify_effect_message_t *)message;
        
        break;
        case ESP_ZB_CORE_CMD_CUSTOM_CLUSTER_REQ_CB_ID: {
    esp_zb_zcl_custom_cluster_command_message_t *custom_cluster_req = (esp_zb_zcl_custom_cluster_command_message_t *)message;

    esp_zb_zcl_cmd_info_t *info = &custom_cluster_req->info;
    esp_zb_zcl_frame_header_t *frame = &info->header;
    esp_zb_zcl_addr_t *src_addr = &info->src_address;

    ESP_LOGI(TAG, "✅ CUSTOM CLUSTER REQUEST RECEIVED");
    ESP_LOGI(TAG, "  Status: 0x%02x", info->status);
    
    // ✅ Исправлено: `u.short_addr` — правильно
    ESP_LOGI(TAG, "  Source: short=0x%04x", src_addr->u.short_addr);

    // ❌ Убрано: `ieee_addr` — его может не быть в `esp_zb_zcl_addr_t`
    // Если нужно IEEE — нужно получать отдельно через ZDO

    ESP_LOGI(TAG, "  Endpoints: src=%d, dst=%d", info->src_endpoint, info->dst_endpoint);
    ESP_LOGI(TAG, "  Cluster: 0x%04x, Profile: 0x%04x", info->cluster, info->profile);
    ESP_LOGI(TAG, "  Command ID: 0x%02x", info->command.id);
    ESP_LOGI(TAG, "  Frame: fc=0x%02x, manuf_code=0x%04x, tsn=%u, rssi=%d dBm", 
             frame->fc, frame->manuf_code, frame->tsn, frame->rssi);
    ESP_LOGI(TAG, "  Destination short: 0x%04x", info->dst_address);

    // ✅ Убрано: info->command.type — в новых версиях его нет
    // Вместо этого — можно логировать тип по контексту: read/write/report и т.п.

    if (custom_cluster_req->data.value && custom_cluster_req->data.size > 0) {
        ESP_LOGI(TAG, "  Custom Data: size=%u bytes, ptr=%p", custom_cluster_req->data.size, custom_cluster_req->data.value);
        // Дамп первых 16 байт
        uint8_t *data = (uint8_t *)custom_cluster_req->data.value;
        char hex_str[64] = {0};
        size_t len = custom_cluster_req->data.size > 16 ? 16 : custom_cluster_req->data.size;
        for (int i = 0; i < len; i++) {
            sprintf(&hex_str[i * 2], "%02x", data[i]);
        }
        ESP_LOGI(TAG, "  Data (hex, first %u bytes): %s", len, hex_str);
    } else {
        ESP_LOGI(TAG, "  Custom Data: none or empty");
    }

    // --- Твой обработчик ---
    // zb_manager_process_custom_cluster_command(custom_cluster_req);

    break;
}


        default:
            ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
            break;
    }

    if (output) {
        esp_ncp_noti_input(&ncp_header, output, outlen);
        free(output);
        output = NULL;
    }

    return ret;
}

/******************************************************** БЕЗ NCP ***********************************************************/
esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
  
    esp_zb_zcl_cmd_read_attr_resp_message_t* cmd_read_attr_resp_mess = NULL;
    esp_zb_zcl_cmd_discover_attributes_resp_message_t* cmd_disc_attr_resp_mess = NULL;
    esp_zb_zcl_cmd_default_resp_message_t* cmd_default_resp_mess = NULL;
    esp_zb_zcl_report_attr_message_t* report_attr_mess = NULL;
    esp_zb_zcl_ias_zone_enroll_request_message_t* ias_zone_enroll_req = NULL;
    esp_zb_zcl_ias_zone_status_change_notification_message_t* ias_zone_notify = NULL;
    esp_zb_zcl_cmd_write_attr_resp_message_t* cmd_write_attr_resp_mess =NULL;
    esp_zb_zcl_cmd_config_report_resp_message_t* cmd_config_report_resp_mess = NULL;
    char buffer[256];
    esp_err_t ret = ESP_OK;

    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
    ESP_LOGW(TAG, "Receive Zigbee action ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID (0x%x) callback", callback_id);
    break;
    case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
        report_attr_mess = (esp_zb_zcl_report_attr_message_t*)message;
        if (report_attr_mess->status == ESP_ZB_ZCL_STATUS_SUCCESS)
        {
            ESP_LOGW(TAG, "Receive Zigbee action ESP_ZB_CORE_REPORT_ATTR_CB_ID (0x%x) callback", callback_id);
            sprintf(buffer, "Получен статус атрибута (REPORT_ATTR) : short: 0x%04x cluster: %s [0x%04x] attribute: %s [0x%04x] attr_type: %s attr_size_fromdata:%d attr_size:%d", 
                report_attr_mess->src_address.u.short_addr, zb_manager_get_cluster_name(report_attr_mess->cluster), 
                report_attr_mess->cluster, zb_manager_get_attr_name(report_attr_mess->cluster, report_attr_mess->attribute.id),
                report_attr_mess->attribute.id, zb_manager_attr_type_to_string(report_attr_mess->attribute.data.type), zb_manager_get_attr_data_size(report_attr_mess->attribute.data.type,report_attr_mess->attribute.data.value), report_attr_mess->attribute.data.size);
                //zb_manager_obj.utility_functions_callbacks.print_log_to_screen_callback(buffer, 0xA7BFC1);
        }
    
        //ret = zb_attribute_reporting_handler((esp_zb_zcl_report_attr_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
        cmd_read_attr_resp_mess = (esp_zb_zcl_cmd_read_attr_resp_message_t *)message;
        if (cmd_read_attr_resp_mess->info.status == ESP_ZB_ZCL_STATUS_SUCCESS) 
        {
            ESP_LOGW(TAG, "ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID(0x%4x) callback and status(0x%4x)", callback_id, cmd_read_attr_resp_mess->info.status);
            if(cmd_read_attr_resp_mess->variables != NULL)
            {
                esp_zb_zcl_read_attr_resp_variable_t *variable = cmd_read_attr_resp_mess->variables;
                while (variable) 
                {
                    if (variable->status == ESP_ZB_ZCL_STATUS_SUCCESS) 
                    {
                        sprintf(buffer, "Получен ответ на (CMD_READ_ATT) : (short: 0x%04x) (cluster: %s [0x%04x]) (attribute: %s [0x%04x])", 
                cmd_read_attr_resp_mess->info.src_address.u.short_addr, zb_manager_get_cluster_name(cmd_read_attr_resp_mess->info.cluster), 
                cmd_read_attr_resp_mess->info.cluster, zb_manager_get_attr_name(cmd_read_attr_resp_mess->info.cluster, variable->attribute.id),
                variable->attribute.id);
                //zb_manager_obj.utility_functions_callbacks.print_log_to_screen_callback(buffer, 0xA7BFC1);
                       switch (cmd_read_attr_resp_mess->info.cluster)
                       {
                       case ESP_ZB_ZCL_CLUSTER_ID_BASIC:
                        if (variable->attribute.id == ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID && variable->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING && variable->attribute.data.value)
                        {
                            zbstring_t *zbstr = (zbstring_t *)variable->attribute.data.value;
                            char *string = (char*)malloc(zbstr->len + 1);
                            memcpy(string, zbstr->data, zbstr->len);
                            string[zbstr->len] = '\0';
                            ESP_LOGI(TAG, "Peer Manufacturer is \"%s\"", string);
                            free(string);
                        }else

                       break;
                       
                       default:
                        break;
                       }
                    }
                }
            }
            

        }
    ESP_LOGW(TAG, "Receive Zigbee action ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID (0x%x) callback", callback_id);
        //ret = zb_read_attr_resp_handler((esp_zb_zcl_cmd_read_attr_resp_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID:
    ESP_LOGW(TAG, "Receive Zigbee action ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID (0x%x) callback", callback_id);
        //ret = zb_configure_report_resp_handler((esp_zb_zcl_cmd_config_report_resp_message_t *)message);
        break;
    case ESP_ZB_CORE_IDENTIFY_EFFECT_CB_ID:
    ESP_LOGW(TAG, "Receive Zigbee action ESP_ZB_CORE_IDENTIFY_EFFECT_CB_ID (0x%x) callback", callback_id);
        //ret = zb_identify_effect_handler((esp_zb_zcl_identify_effect_message_t *)message);
        esp_zb_zcl_identify_effect_message_t * id_msg = (esp_zb_zcl_identify_effect_message_t *)message;
        
        break;
    case ESP_ZB_CORE_CMD_CUSTOM_CLUSTER_REQ_CB_ID:{
        esp_zb_zcl_custom_cluster_command_message_t* custom_cluster_req = (esp_zb_zcl_custom_cluster_command_message_t*)message;
        
        break;
    }
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

void identify_handler(uint8_t identify_on)
{
    ESP_LOGW(TAG, "Receive Identify action callback with id state 0x%02x", identify_on);
}

void identify_handler_req(bool identify_on)
{
    if (identify_on == true) {
        ESP_LOGW(TAG, "Receive Identify Query callback with id state TRUE");
    } else {
    ESP_LOGW(TAG, "Receive Identify Query callback with id state FALSE");
    }
}