#include "zbm_dev_base_dev_update.h"
#include "zbm_dev_types.h"
#include "esp_log.h"
#include "zb_manager_rules.h"

static const char* TAG = "ZBM_DEV_BASE_UPDATE_MODULE";

esp_err_t zbm_dev_base_dev_update_from_read_response(device_custom_t* dev, endpoint_custom_t* ep, zb_manager_cmd_read_attr_resp_message_t* read_resp);
esp_err_t zbm_dev_base_dev_update_from_report(device_custom_t* dev, endpoint_custom_t* ep, zb_manager_cmd_report_attr_resp_message_t* rep);

//================================================================================================================================
//=============================================== ZBM_DEV_BASE_DEV_UPDATE_FROM_READ_RESPONSE_NOT_SAFE ============================
//================================================================================================================================
esp_err_t zbm_dev_base_dev_update_from_read_response(device_custom_t* dev, endpoint_custom_t* ep, zb_manager_cmd_read_attr_resp_message_t* read_resp)
{
    esp_err_t result = ESP_FAIL;
    // Обработка On/Off кластера
    if (read_resp->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
        if (ep && ep->is_use_on_off_cluster && ep->server_OnOffClusterObj) {
            bool updated = false;

            for (int i = 0; i < read_resp->attr_count; i++) {
                zb_manager_read_resp_attr_t* attr = &read_resp->attr_arr[i];
                if (attr->attr_id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && attr->status == ESP_ZB_ZCL_STATUS_SUCCESS) {
                    if (attr->attr_value && attr->attr_len >= 1) {
                        bool new_state = *(bool*)attr->attr_value;
                        bool old_state = ep->server_OnOffClusterObj->on_off;

                        if (old_state != new_state) {
                            zb_manager_on_off_cluster_update_attribute(ep->server_OnOffClusterObj, attr->attr_id, attr->attr_value);
                            ESP_LOGI(TAG, "✅ OnOff updated via ReadAttr: 0x%04x (ep: %d) → %s (was: %s)",
                                            dev->short_addr, ep->ep_id,
                                            new_state ? "ON" : "OFF",
                                            old_state ? "ON" : "OFF");
                            updated = true;
                        }
                    }
                }
                // Триггер правил всегда, даже если статус не успех (можно логировать ошибки)
                zb_rule_trigger_state_update(read_resp->info.src_address.u.short_addr,read_resp->info.cluster, attr->attr_id,attr->attr_value,attr->attr_len,attr->attr_type);
            }

            if (!updated) {
                ESP_LOGD(TAG, "OnOff ReadAttr: no valid ON/OFF update");
            }
        }
    }
// Обновляем флаги
dev->has_pending_read = false;
dev->has_pending_response = true;
dev->lqi = 10;
dev->last_seen_ms = esp_log_timestamp();
dev->is_online = true;
ESP_LOGD(TAG, "Device 0x%04x: LQI=%d, Online=%s",read_resp->info.src_address.u.short_addr,dev->lqi,dev->is_online ? "YES" : "NO");
result = ESP_OK;
return result;
}

//================================================================================================================================
//===================================================== ZBM_DEV_BASE_UPDATE_DEV_FROM_REPORT_NOT_SAFE =============================
//================================================================================================================================
esp_err_t zbm_dev_base_dev_update_from_report(device_custom_t* dev, endpoint_custom_t* ep, zb_manager_cmd_report_attr_resp_message_t* rep)
{
    // Обновляем активность
    ESP_LOGI(TAG,"zbm_dev_base_dev_update_from_report: processing");
    esp_err_t result = ESP_FAIL;
            
            // 🔹 Basic Cluster (0x0000)
            if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_BASIC) {
                if (dev->server_BasicClusterObj == NULL) {
                    dev->server_BasicClusterObj = calloc(1, sizeof(zigbee_manager_basic_cluster_t));
                    if (dev->server_BasicClusterObj) {
                        zigbee_manager_basic_cluster_t cl = ZIGBEE_BASIC_CLUSTER_DEFAULT_INIT();
                        memcpy(dev->server_BasicClusterObj, &cl, sizeof(zigbee_manager_basic_cluster_t));
                    } else {
                        ESP_LOGE(TAG, "Failed to allocate Basic Cluster for 0x%04x", dev->short_addr);
                        return result;
                    }
                }
                if (dev->server_BasicClusterObj) {
                        zb_manager_basic_cluster_update_attribute(dev->server_BasicClusterObj, rep->attr.attr_id, rep->attr.attr_value);      
                }
            }
            // 🔹 Power Configuration (0x0001)
            else if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG) {
                if (dev->server_PowerConfigurationClusterObj == NULL) {
                    dev->server_PowerConfigurationClusterObj = calloc(1, sizeof(zb_manager_power_config_cluster_t));
                    if (dev->server_PowerConfigurationClusterObj) {
                        zb_manager_power_config_cluster_t cl = ZIGBEE_POWER_CONFIG_CLUSTER_DEFAULT_INIT();
                        memcpy(dev->server_PowerConfigurationClusterObj, &cl, sizeof(zb_manager_power_config_cluster_t));
                    } else {
                        ESP_LOGE(TAG, "Failed to allocate Power Config Cluster for 0x%04x", dev->short_addr);
                        return result;
                    }
                }
                if (dev->server_PowerConfigurationClusterObj) {
                    zb_manager_power_config_cluster_update_attribute(dev->server_PowerConfigurationClusterObj, rep->attr.attr_id, rep->attr.attr_value);
                        //ws_notify_device_update_unlocked(dev);
                        //log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);    
                }
            }
            // 🔹 Cluster на уровне endpoint'а
            else {
                if (!ep) {
                    ESP_LOGW(TAG, "ATTR_REPORT_EVENT: endpoint not found: short=0x%04x, ep=%d", rep->src_address.u.short_addr, rep->src_endpoint);
                    return result;
                }
                // 🔹 Temperature (0x0402)
                if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT) {
                    if (ep->server_TemperatureMeasurementClusterObj == NULL) {
                        ep->server_TemperatureMeasurementClusterObj = calloc(1, sizeof(zb_manager_temperature_measurement_cluster_t));
                        if (ep->server_TemperatureMeasurementClusterObj) {
                            zb_manager_temperature_measurement_cluster_t cl = ZIGBEE_TEMP_MEASUREMENT_CLUSTER_DEFAULT_INIT();
                            memcpy(ep->server_TemperatureMeasurementClusterObj, &cl, sizeof(zb_manager_temperature_measurement_cluster_t));
                        } else {
                            ESP_LOGE(TAG, "Failed to allocate Temp Meas Cluster");
                            return result;
                        }
                    }
                    if (ep->server_TemperatureMeasurementClusterObj) {
                        zb_manager_temp_meas_cluster_update_attribute(ep->server_TemperatureMeasurementClusterObj, rep->attr.attr_id, rep->attr.attr_value);
                        //ws_notify_device_update_unlocked(dev);
                        
                            // отправляем в обработчик сценариев
                            /*zb_rule_trigger_state_update(
                                rep->src_address.u.short_addr,
                                rep->cluster,
                                rep->attr.attr_id,
                                rep->attr.attr_value,
                                rep->attr.attr_len,
                                rep->attr.attr_type
                            );*/
                        //log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);
                    }
                }
                // 🔹 Humidity (0x0405)
                else if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT) {
                    if (ep->server_HumidityMeasurementClusterObj == NULL) 
                    {
                        ep->server_HumidityMeasurementClusterObj = calloc(1, sizeof(zb_manager_humidity_measurement_cluster_t));
                    }
                    if (ep->server_HumidityMeasurementClusterObj) {
                        zb_manager_humidity_measurement_cluster_t cl = ZIGBEE_HUMIDITY_MEASUREMENT_CLUSTER_DEFAULT_INIT();
                        memcpy(ep->server_HumidityMeasurementClusterObj, &cl, sizeof(zb_manager_humidity_measurement_cluster_t));
                    } else {
                        ESP_LOGE(TAG, "Failed to allocate Humidity Meas Cluster");
                        return result;
                    }
                    if (ep->server_HumidityMeasurementClusterObj) {
                        zb_manager_humidity_meas_cluster_update_attribute(ep->server_HumidityMeasurementClusterObj, rep->attr.attr_id, rep->attr.attr_value);
                        //ws_notify_device_update_unlocked(dev);
                            /*zb_rule_trigger_state_update(
                                rep->src_address.u.short_addr,
                                rep->cluster,
                                rep->attr.attr_id,
                                rep->attr.attr_value,
                                rep->attr.attr_len,
                                rep->attr.attr_type
                            );*/
                            //log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);
                            
                            // ✅ Уведомляем HA
                            //ha_device_updated(dev, ep);  
                    }
                }
                // 🔹 On/Off (0x0006)
                else if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
                    if (ep->server_OnOffClusterObj == NULL) {
                        ep->server_OnOffClusterObj = calloc(1, sizeof(zb_manager_on_off_cluster_t));
                        if (ep->server_OnOffClusterObj) {
                            zb_manager_on_off_cluster_t cl = ZIGBEE_ON_OFF_CLUSTER_DEFAULT_INIT();
                            memcpy(ep->server_OnOffClusterObj, &cl, sizeof(zb_manager_on_off_cluster_t));
                        } else {
                            ESP_LOGE(TAG, "Failed to allocate On/Off Cluster");
                            return result;
                        }
                    }
                    if (ep->server_OnOffClusterObj) {
                        
                            zb_manager_on_off_cluster_update_attribute(ep->server_OnOffClusterObj, rep->attr.attr_id, rep->attr.attr_value);
                            //ws_notify_device_update_unlocked(dev);
                            
                            /*zb_rule_trigger_state_update(
                                rep->src_address.u.short_addr,
                                rep->cluster,
                                rep->attr.attr_id,
                                rep->attr.attr_value,
                                rep->attr.attr_len,
                                rep->attr.attr_type
                            );*/
                            //log_zb_attribute(rep->cluster, &rep->attr, &rep->src_address, rep->src_endpoint);
                            //ws_notify_device_update(rep->src_address.u.short_addr);
                            //ws_notify_device_update_unlocked(rep->src_address.u.short_addr);
                            // ✅ Уведомляем HA
                            //ha_device_updated(dev, ep);
                        
                    }
                }
                // 🔹 Неизвестный кластер
                else {
                    ESP_LOGW(TAG, "Unhandled cluster ID: 0x%04x", rep->cluster);
                }
            }
            result = ESP_OK;
            dev->lqi = 10;
            dev->last_seen_ms = esp_log_timestamp();
            dev->is_online = true;
            return result;
}