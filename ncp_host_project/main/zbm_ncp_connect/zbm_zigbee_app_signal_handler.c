#include "zbm_zigbee_app_signal_handler.h"
#include "esp_log.h"
#include "esp_err.h"
#include "zbm_ncp_connect.h"
#include "ncp_host_zb_api_to_ncp.h"
#include "zbm_core_sync.h"
#include "zbm_zigbee_structures.h"
#include "zbm_coordinator.h"
#include "zbm_web_server.h" 

static const char *TAG = "ZBM_ZIGBEE_APP_SIGNAL_HANDLER";

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
        case ESP_ZB_BDB_SIGNAL_FORMATION:
        {
            ESP_LOGI(TAG, "ESP_ZB_BDB_SIGNAL_FORMATION:");
            if (err_status == ESP_OK) {
                esp_zb_ieee_addr_t extended_pan_id;
                zbm_to_ncp_req_get_extended_pan_id(extended_pan_id);

                // === Обновляем координатор ===
                zbm_coordinator.zb_pan_id = zbm_to_ncp_req_get_pan_id();
                zbm_coordinator.zb_radio_channel = zbm_to_ncp_req_get_current_channel();

                // Копируем IEEE адрес (он уже должен быть инициализирован, но перестрахуемся)
                zbm_to_ncp_cmd_get_local_long_addr(zbm_coordinator.zb_ieee_addr);

                // Копируем Extended PAN ID
                memcpy(zbm_coordinator.zb_extended_pan_id, extended_pan_id, 8);

                // Обновляем короткий адрес (всегда 0x0000)
                zbm_coordinator.zb_short_address = zbm_to_ncp_req_get_network_short_addr();

                ESP_LOGI(TAG, "Formed network successfully");
                ESP_LOGI(TAG, "  PAN ID: 0x%04hx", zbm_coordinator.zb_pan_id);
                ESP_LOGI(TAG, "  Channel: %d", zbm_coordinator.zb_radio_channel);
                ESP_LOGI(TAG, "  Short Addr: 0x%04hx", zbm_coordinator.zb_short_address);
                ESP_LOGI(TAG, "  IEEE: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                        zbm_coordinator.zb_ieee_addr[0], zbm_coordinator.zb_ieee_addr[1],
                        zbm_coordinator.zb_ieee_addr[2], zbm_coordinator.zb_ieee_addr[3],
                        zbm_coordinator.zb_ieee_addr[4], zbm_coordinator.zb_ieee_addr[5],
                        zbm_coordinator.zb_ieee_addr[6], zbm_coordinator.zb_ieee_addr[7]);
                ESP_LOGI(TAG, "  Ext PAN ID: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                        zbm_coordinator.zb_extended_pan_id[0], zbm_coordinator.zb_extended_pan_id[1],
                        zbm_coordinator.zb_extended_pan_id[2], zbm_coordinator.zb_extended_pan_id[3],
                        zbm_coordinator.zb_extended_pan_id[4], zbm_coordinator.zb_extended_pan_id[5],
                        zbm_coordinator.zb_extended_pan_id[6], zbm_coordinator.zb_extended_pan_id[7]);

                // === Сохраняем в SPIFFS ===
                if (zbm_save_coordinator_to_spiffs(&zbm_coordinator)) {
                    ESP_LOGI(TAG, "✅ Coordinator saved to SPIFFS");
                } else {
                    ESP_LOGE(TAG, "❌ Failed to save coordinator to SPIFFS");
                }

                // === 🚀 Отправляем системное уведомление в UI ===
                cJSON *data = cJSON_CreateObject();
                cJSON_AddNumberToObject(data, "channel", zbm_coordinator.zb_radio_channel);
                cJSON_AddNumberToObject(data, "pan_id", zbm_coordinator.zb_pan_id);
                cJSON_AddStringToObject(data, "ieee", "0x");
                char ieee_str[17];
                snprintf(ieee_str, sizeof(ieee_str), "%02X%02X%02X%02X%02X%02X%02X%02X",
                        zbm_coordinator.zb_ieee_addr[0], zbm_coordinator.zb_ieee_addr[1],
                        zbm_coordinator.zb_ieee_addr[2], zbm_coordinator.zb_ieee_addr[3],
                        zbm_coordinator.zb_ieee_addr[4], zbm_coordinator.zb_ieee_addr[5],
                        zbm_coordinator.zb_ieee_addr[6], zbm_coordinator.zb_ieee_addr[7]);
                cJSON_AddStringToObject(data, "ieee", ieee_str);

                zbm_ws_send_sys_notify("zigbee_network_up", "Zigbee network formed successfully", data);
                cJSON_Delete(data);
            } else {
                ESP_LOGE(TAG, "❌ Network formation failed, status: %s", esp_err_to_name(err_status));
                // ❌ Ошибка формирования — тоже сообщим в UI
                cJSON *data = cJSON_CreateObject();
                cJSON_AddStringToObject(data, "error", esp_err_to_name(err_status));

                zbm_ws_send_sys_notify("zigbee_network_error", "Failed to form Zigbee network", data);
                cJSON_Delete(data);
                // Можно перезапустить формирование
            }
            break;
        }
        case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        {
            ESP_LOGI(TAG, "ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:");
            if (err_status == ESP_OK) {
                uint8_t duration = 0;
                
                uint16_t pan_id = zbm_to_ncp_req_get_pan_id();
                if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
                    duration = *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p);
                    ESP_LOGW(TAG, "Network(0x%04hx) is open for %d seconds", pan_id, duration);
                    isZigbeeNetworkOpened = true;
                    // 🔔 Сеть открыта для подключения
                    cJSON *data = cJSON_CreateObject();
                    cJSON_AddNumberToObject(data, "duration", duration);
                    cJSON_AddNumberToObject(data, "pan_id", pan_id);

                    zbm_ws_send_sys_notify("zigbee_permit_join_started", "Zigbee network is now open for device joining", data);
                    cJSON_Delete(data);
                }else
                {
                   isZigbeeNetworkOpened = false;
                   // 🔒 Сеть закрыта
                    cJSON *data = cJSON_CreateObject();
                    cJSON_AddNumberToObject(data, "pan_id", pan_id);

                    zbm_ws_send_sys_notify("zigbee_permit_join_stopped", "Zigbee network closed for new devices", data); 
                    cJSON_Delete(data);
                }
            }
            break;
        }
        case ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED:
        {
            ESP_LOGI(TAG, "ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED:");
            if (err_status == ESP_OK) {
                esp_zb_nwk_signal_device_associated_params_t* dev_assoc = (esp_zb_nwk_signal_device_associated_params_t*)esp_zb_app_signal_get_params(p_sg_p);
                zbm_dev_t* existing_dev = NULL;
                existing_dev = zbm_find_device_in_devdb_by_ieee_safe(dev_assoc->device_addr);
                if (existing_dev)
                {
                    existing_dev->device_registered_status = 0;
                    

                    // удаляем из хэш таблиц
                    //zbm_remove_device_from_devdb_and_guiddb_by_short_safe(old_short_addr);
                    
                }else
                {
                   zbm_dev_t* new_device = NULL;
                   new_device = zbm_dev_create_and_add_to_devdb_by_ieee_safe(dev_assoc->device_addr);
                   if (!new_device) {
                        ESP_LOGE(TAG, "❌ Failed to create device by IEEE");
                        break;
                    }
                    ESP_LOGI(TAG, "✅ Device added to devdb: %s", new_device->friendly_name);
                    break;
                }
                ESP_LOGI(TAG, "✅ Device allready in devdb and set device_registered_status = 0: %s", existing_dev->friendly_name);      
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
                if(device_in_base != NULL) break; // значит уже есть в базе и просто устройство вышло на связь

                device_in_base = zbm_find_device_in_devdb_by_ieee_safe(dev_update->long_addr);
                if (!device_in_base)
                {
                    ESP_LOGW(TAG, "Device not found in devdb");
                    break;
                }

                // сохраняем старый адрес
                uint16_t old_short_addr = 0x0000;
                old_short_addr = device_in_base->short_addr;

                // обновляем короткий адрес
                bool short_updated = false;
                short_updated = zbm_dev_update_short_addr_safe(device_in_base, dev_update->short_addr, dev_update->long_addr); // функция также чистит guid и создаёт новые
                if (short_updated)
                {
                    ESP_LOGI(TAG, "✅ Short address updated");
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

                    if (cmd_req.attr_field != NULL)
                    {
                        free(cmd_req.attr_field);
                        cmd_req.attr_field = NULL;
                    }
                    
                    if (old_short_addr != 0x0000)
                    {
                        zbm_save_device_to_spiffs_safe(device_in_base); // сперва удаляет по длинному потом сохраняет заново
                    }
                    break;
                }
            }
            break;
        }
        case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        {
            
        }
        case ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED:
        {
            ESP_LOGI(TAG, "ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED:");
            esp_zb_zdo_signal_device_authorized_params_t* dev_auth = (esp_zb_zdo_signal_device_authorized_params_t*)esp_zb_app_signal_get_params(p_sg_p);
            if (err_status == ESP_OK) {
                if (dev_auth->authorization_status == 0x00) {
                    zbm_dev_t* device_in_base = NULL;
                    device_in_base = zbm_find_device_in_devdb_by_short_safe(dev_auth->short_addr);
                    if (device_in_base) {
                        device_in_base->device_registered_status = 1;
                    }
                    if (dev_auth->authorization_type == 0x00)
                    {
                        ESP_LOGI(TAG, "ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED with Authorization type for legacy devices ( < r21) 0x%4x", dev_auth->short_addr); // 0x00 - legacy devices, 0x01 - new devices, 0x02 - all devices (legacy and new
                    }else
                    if (dev_auth->authorization_type == 0x01)
                    {
                        ESP_LOGI(TAG, "ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED with Authorization type for r21 device through TCLK 0x%4x", dev_auth->short_addr);
                    }else
                    if (dev_auth->authorization_type == 0x02)
                    {
                        ESP_LOGI(TAG, "ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED with Authorization type for SE through CBKE 0x%4x", dev_auth->short_addr);
                    }
                    // здесь запускаем цепочку activeep->simpledesc->discoveryattr->readattr
                    if(device_in_base)
                    {
                        esp_err_t ret = zbm_to_ncp_req_active_endpoint_req(dev_auth->short_addr, NULL, &device_in_base->short_addr);
                        if (ret == ESP_OK) {
                            ESP_LOGI(TAG, "✅ Active Endpoint Request sent");
                        } else {
                            ESP_LOGE(TAG, "❌ Failed to send Active Endpoint Request");
                        }
                    }
                }
            }
            break;
        }
        default:
        /*ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));*/
        break;
    }
    return true;
}