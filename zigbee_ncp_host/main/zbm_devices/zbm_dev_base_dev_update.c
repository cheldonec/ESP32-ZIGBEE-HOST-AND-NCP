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
    uint16_t short_addr = read_resp->info.src_address.u.short_addr;
    uint8_t endpoint_id = read_resp->info.src_endpoint;
    uint16_t cluster_id = read_resp->info.cluster;

    ESP_LOGI(TAG, "Processing ReadAttrResp for 0x%04x (ep=%d, cluster=0x%04x, attr_count=%d)",
             short_addr, endpoint_id, cluster_id, read_resp->attr_count);

   
    if (!ep) {
        ESP_LOGW(TAG, "ReadAttrResp: endpoint %d not found for device 0x%04x", endpoint_id, short_addr);
        return ESP_ERR_NOT_FOUND;
    }
    

    bool updated = false;

    // 🔹 Обработка атрибутов
    for (int i = 0; i < read_resp->attr_count; i++) {
        zb_manager_read_resp_attr_t* attr = &read_resp->attr_arr[i];

        // Пропускаем ошибки, но всё равно триггерим правила
        if (attr->status != ESP_ZB_ZCL_STATUS_SUCCESS) {
            ESP_LOGW(TAG, "Attr 0x%04x read failed: status=0x%02x", attr->attr_id, attr->status);
            zb_rule_trigger_state_update(short_addr, cluster_id, attr->attr_id, NULL, 0, attr->attr_type);
            continue;
        }

        if (attr->attr_value == NULL || attr->attr_len == 0) {
            ESP_LOGW(TAG, "Attr 0x%04x: value is NULL or len=0", attr->attr_id);
            zb_rule_trigger_state_update(short_addr, cluster_id, attr->attr_id, NULL, 0, attr->attr_type);
            continue;
        }

        void *value = attr->attr_value;

        // 🔹 Basic Cluster (0x0000)
        if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_BASIC) {
            if (dev->server_BasicClusterObj == NULL) {
                dev->server_BasicClusterObj = calloc(1, sizeof(zigbee_manager_basic_cluster_t));
                if (dev->server_BasicClusterObj) {
                    zigbee_manager_basic_cluster_t cl = ZIGBEE_BASIC_CLUSTER_DEFAULT_INIT();
                    memcpy(dev->server_BasicClusterObj, &cl, sizeof(zigbee_manager_basic_cluster_t));
                } else {
                    ESP_LOGE(TAG, "Failed to allocate Basic Cluster");
                    continue;
                }
            }
            if (dev->server_BasicClusterObj) {
                zb_manager_basic_cluster_update_attribute(dev->server_BasicClusterObj, attr->attr_id, attr->attr_type, value, attr->attr_len);
                updated = true;
            }
        }

        // 🔹 Power Configuration (0x0001)
        else if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG) {
            if (dev->server_PowerConfigurationClusterObj == NULL) {
                dev->server_PowerConfigurationClusterObj = calloc(1, sizeof(zb_manager_power_config_cluster_t));
                if (dev->server_PowerConfigurationClusterObj) {
                    zb_manager_power_config_cluster_t cl = ZIGBEE_POWER_CONFIG_CLUSTER_DEFAULT_INIT();
                    memcpy(dev->server_PowerConfigurationClusterObj, &cl, sizeof(zb_manager_power_config_cluster_t));
                } else {
                    ESP_LOGE(TAG, "Failed to allocate Power Config Cluster");
                    continue;
                }
            }
            if (dev->server_PowerConfigurationClusterObj) {
                zb_manager_power_config_cluster_update_attribute(dev->server_PowerConfigurationClusterObj, attr->attr_id, attr->attr_type, value, attr->attr_len);
                updated = true;
            }
        }

        // 🔹 Temperature Measurement (0x0402)
        else if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT) {
            if (ep->server_TemperatureMeasurementClusterObj == NULL) {
                ep->server_TemperatureMeasurementClusterObj = calloc(1, sizeof(zb_manager_temperature_measurement_cluster_t));
                if (ep->server_TemperatureMeasurementClusterObj) {
                    zb_manager_temperature_measurement_cluster_t cl = ZIGBEE_TEMP_MEASUREMENT_CLUSTER_DEFAULT_INIT();
                    memcpy(ep->server_TemperatureMeasurementClusterObj, &cl, sizeof(zb_manager_temperature_measurement_cluster_t));
                } else {
                    ESP_LOGE(TAG, "Failed to allocate Temp Meas Cluster");
                    continue;
                }
            }
            if (ep->server_TemperatureMeasurementClusterObj) {
                zb_manager_temp_meas_cluster_update_attribute(ep->server_TemperatureMeasurementClusterObj, attr->attr_id, attr->attr_type, value, attr->attr_len);
                updated = true;
            }
        }

        // 🔹 Humidity Measurement (0x0405)
        else if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT) {
            if (ep->server_HumidityMeasurementClusterObj == NULL) {
                ep->server_HumidityMeasurementClusterObj = calloc(1, sizeof(zb_manager_humidity_measurement_cluster_t));
                if (ep->server_HumidityMeasurementClusterObj) {
                    zb_manager_humidity_measurement_cluster_t cl = ZIGBEE_HUMIDITY_MEASUREMENT_CLUSTER_DEFAULT_INIT();
                    memcpy(ep->server_HumidityMeasurementClusterObj, &cl, sizeof(zb_manager_humidity_measurement_cluster_t));
                } else {
                    ESP_LOGE(TAG, "Failed to allocate Humidity Meas Cluster");
                    continue;
                }
            }
            if (ep->server_HumidityMeasurementClusterObj) {
                zb_manager_humidity_meas_cluster_update_attribute(ep->server_HumidityMeasurementClusterObj, attr->attr_id, attr->attr_type, value, attr->attr_len);
                updated = true;
            }
        }

        // 🔹 On/Off (0x0006)
        else if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
            if (ep->server_OnOffClusterObj == NULL) {
                ep->server_OnOffClusterObj = calloc(1, sizeof(zb_manager_on_off_cluster_t));
                if (ep->server_OnOffClusterObj) {
                    zb_manager_on_off_cluster_t cl = ZIGBEE_ON_OFF_CLUSTER_DEFAULT_INIT();
                    memcpy(ep->server_OnOffClusterObj, &cl, sizeof(zb_manager_on_off_cluster_t));
                } else {
                    ESP_LOGE(TAG, "Failed to allocate On/Off Cluster");
                    continue;
                }
            }
            if (ep->server_OnOffClusterObj) {
                zb_manager_on_off_cluster_update_attribute(ep->server_OnOffClusterObj, attr->attr_id, attr->attr_type, value, attr->attr_len);
                updated = true;
            }
        }

        // 🔹 Неизвестный кластер → Custom Cluster
        else {
            ESP_LOGW(TAG, "ReadResp: Unhandled cluster ID: 0x%04x", cluster_id);

            // Ищем или создаём кастомный кластер
            cluster_custom_t *custom_cluster = NULL;
            for (int j = 0; j < ep->UnKnowninputClusterCount; j++) {
                if (ep->UnKnowninputClusters_array[j] && ep->UnKnowninputClusters_array[j]->id == cluster_id) {
                    custom_cluster = ep->UnKnowninputClusters_array[j];
                    break;
                }
            }

            if (!custom_cluster) {
                custom_cluster = calloc(1, sizeof(cluster_custom_t));
                if (!custom_cluster) {
                    ESP_LOGE(TAG, "Failed to allocate custom cluster 0x%04x", cluster_id);
                    continue;
                }

                custom_cluster->id = cluster_id;
                snprintf(custom_cluster->cluster_id_text, sizeof(custom_cluster->cluster_id_text), "Custom_0x%04X", cluster_id);
                custom_cluster->is_use_on_device = 1;
                custom_cluster->role_mask = 1; // SERVER
                custom_cluster->manuf_code = 0;
                custom_cluster->attr_count = 0;
                custom_cluster->attr_array = NULL;

                void *new_array = realloc(ep->UnKnowninputClusters_array,
                                          (ep->UnKnowninputClusterCount + 1) * sizeof(cluster_custom_t*));
                if (!new_array) {
                    free(custom_cluster);
                    ESP_LOGE(TAG, "Failed to realloc UnKnowninputClusters_array");
                    continue;
                }

                ep->UnKnowninputClusters_array = (cluster_custom_t**)new_array;
                ep->UnKnowninputClusters_array[ep->UnKnowninputClusterCount] = custom_cluster;
                ep->UnKnowninputClusterCount++;

                ESP_LOGI(TAG, "🆕 Created custom cluster 0x%04x for ReadAttrResp", cluster_id);
            }

            // Добавляем/обновляем атрибут
            attribute_custom_t *custom_attr = zbm_cluster_find_custom_attribute(custom_cluster, attr->attr_id);
            if (!custom_attr) {
                esp_err_t err = zbm_cluster_add_custom_attribute(custom_cluster, attr->attr_id, attr->attr_type);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to add custom attr 0x%04x", attr->attr_id);
                    continue;
                }
                custom_attr = zbm_cluster_find_custom_attribute(custom_cluster, attr->attr_id);
            }

            if (custom_attr) {
                zbm_update_custom_attribute_value(custom_attr, (const uint8_t*)value, attr->attr_len);
                ESP_LOGI(TAG, "📊 Updated custom attr 0x%04x in cluster 0x%04x via ReadAttr", attr->attr_id, cluster_id);
            } else {
                ESP_LOGE(TAG, "Failed to find custom attr 0x%04x", attr->attr_id);
                continue;
            }
        }

        // 🔁 Триггер правил для каждого успешного атрибута
        zb_rule_trigger_state_update(short_addr, cluster_id, attr->attr_id, value, attr->attr_len, attr->attr_type);
    }

    // Обновляем статус устройства
    dev->has_pending_read = false;
    dev->has_pending_response = true;
    dev->lqi = 10;
    dev->last_seen_ms = esp_log_timestamp();
    dev->is_online = true;

    ESP_LOGD(TAG, "Device 0x%04x updated via ReadAttr: LQI=%d, Online=%s",
             short_addr, dev->lqi, dev->is_online ? "YES" : "NO");

    result = updated ? ESP_OK : ESP_FAIL;

    return result;
}



//================================================================================================================================
//===================================================== ZBM_DEV_BASE_UPDATE_DEV_FROM_REPORT_NOT_SAFE =============================
//================================================================================================================================
esp_err_t zbm_dev_base_dev_update_from_report(device_custom_t* dev, endpoint_custom_t* ep, zb_manager_cmd_report_attr_resp_message_t* rep)
{
    ESP_LOGI(TAG, "zbm_dev_base_dev_update_from_report: processing cluster=0x%04x, attr=0x%04x", rep->cluster, rep->attr.attr_id);
    
    esp_err_t result = ESP_FAIL;
    uint16_t short_addr = rep->src_address.u.short_addr;
    uint8_t endpoint_id = rep->src_endpoint;

    if (!ep) {
        ESP_LOGW(TAG, "Report: endpoint %d not found for device 0x%04x", endpoint_id, short_addr);
        return ESP_ERR_NOT_FOUND;
    }

    void *value = rep->attr.attr_value;
    uint8_t data_len = rep->attr.attr_len;
    esp_zb_zcl_attr_type_t attr_type = rep->attr.attr_type;

    // 🔹 Basic Cluster (0x0000)
    if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_BASIC) {
        if (dev->server_BasicClusterObj == NULL) {
            dev->server_BasicClusterObj = calloc(1, sizeof(zigbee_manager_basic_cluster_t));
            if (dev->server_BasicClusterObj) {
                zigbee_manager_basic_cluster_t cl = ZIGBEE_BASIC_CLUSTER_DEFAULT_INIT();
                memcpy(dev->server_BasicClusterObj, &cl, sizeof(zigbee_manager_basic_cluster_t));
            } else {
                ESP_LOGE(TAG, "Failed to allocate Basic Cluster for 0x%04x", short_addr);
                return result;
            }
        }
        if (dev->server_BasicClusterObj) {
            zb_manager_basic_cluster_update_attribute(dev->server_BasicClusterObj, rep->attr.attr_id, attr_type, value, data_len);
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
                ESP_LOGE(TAG, "Failed to allocate Power Config Cluster for 0x%04x", short_addr);
                return result;
            }
        }
        if (dev->server_PowerConfigurationClusterObj) {
            zb_manager_power_config_cluster_update_attribute(dev->server_PowerConfigurationClusterObj, rep->attr.attr_id, attr_type, value, data_len);
        }
    }
    // 🔹 Temperature Measurement (0x0402)
    else if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT) {
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
            zb_manager_temp_meas_cluster_update_attribute(ep->server_TemperatureMeasurementClusterObj, rep->attr.attr_id, attr_type, value, data_len);
        }
    }
    // 🔹 Humidity Measurement (0x0405)
    else if (rep->cluster == ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT) {
        if (ep->server_HumidityMeasurementClusterObj == NULL) {
            ep->server_HumidityMeasurementClusterObj = calloc(1, sizeof(zb_manager_humidity_measurement_cluster_t));
            if (ep->server_HumidityMeasurementClusterObj) {
                zb_manager_humidity_measurement_cluster_t cl = ZIGBEE_HUMIDITY_MEASUREMENT_CLUSTER_DEFAULT_INIT();
                memcpy(ep->server_HumidityMeasurementClusterObj, &cl, sizeof(zb_manager_humidity_measurement_cluster_t));
            } else {
                ESP_LOGE(TAG, "Failed to allocate Humidity Meas Cluster");
                return result;
            }
        }
        if (ep->server_HumidityMeasurementClusterObj) {
            zb_manager_humidity_meas_cluster_update_attribute(ep->server_HumidityMeasurementClusterObj, rep->attr.attr_id, attr_type, value, data_len);
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
            zb_manager_on_off_cluster_update_attribute(ep->server_OnOffClusterObj, rep->attr.attr_id, attr_type, value, data_len);
        }
    }
    // 🔹 Неизвестный кластер → Custom Cluster
    else {
        cluster_custom_t *custom_cluster = NULL;
        for (int i = 0; i < ep->UnKnowninputClusterCount; i++) {
            if (ep->UnKnowninputClusters_array[i] && ep->UnKnowninputClusters_array[i]->id == rep->cluster) {
                custom_cluster = ep->UnKnowninputClusters_array[i];
                break;
            }
        }

        if (!custom_cluster) {
            custom_cluster = calloc(1, sizeof(cluster_custom_t));
            if (!custom_cluster) {
                ESP_LOGE(TAG, "Failed to allocate custom cluster 0x%04x", rep->cluster);
                return ESP_ERR_NO_MEM;
            }

            custom_cluster->id = rep->cluster;
            snprintf(custom_cluster->cluster_id_text, sizeof(custom_cluster->cluster_id_text), "Custom_0x%04X", rep->cluster);
            custom_cluster->is_use_on_device = 1;
            custom_cluster->role_mask = 1; // SERVER
            custom_cluster->manuf_code = 0;
            custom_cluster->attr_count = 0;
            custom_cluster->attr_array = NULL;

            void *new_array = realloc(ep->UnKnowninputClusters_array,
                                    (ep->UnKnowninputClusterCount + 1) * sizeof(cluster_custom_t*));
            if (!new_array) {
                free(custom_cluster);
                ESP_LOGE(TAG, "Failed to realloc custom_cluster_array");
                return ESP_ERR_NO_MEM;
            }

            ep->UnKnowninputClusters_array = (cluster_custom_t**)new_array;
            ep->UnKnowninputClusters_array[ep->UnKnowninputClusterCount] = custom_cluster;
            ep->UnKnowninputClusterCount++;

            ESP_LOGI(TAG, "🆕 Created custom cluster 0x%04x for device 0x%04x", rep->cluster, short_addr);
        }

        attribute_custom_t *custom_attr = zbm_cluster_find_custom_attribute(custom_cluster, rep->attr.attr_id);
        if (!custom_attr) {
            esp_err_t err = zbm_cluster_add_custom_attribute(custom_cluster, rep->attr.attr_id, rep->attr.attr_type);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to add custom attr 0x%04x", rep->attr.attr_id);
                return err;
            }
            custom_attr = zbm_cluster_find_custom_attribute(custom_cluster, rep->attr.attr_id);
        }

        if (custom_attr) {
            zbm_update_custom_attribute_value(custom_attr, (const uint8_t*)value, data_len);
            ESP_LOGI(TAG, "📊 Updated custom attr 0x%04x in cluster 0x%04x", rep->attr.attr_id, rep->cluster);
        } else {
            ESP_LOGE(TAG, "Failed to find or create attr 0x%04x", rep->attr.attr_id);
            return ESP_ERR_NOT_FOUND;
        }
    }

    // ✅ Триггерим правило НЕЗАВИСИМО от типа кластера (если он отслеживается)
    zb_rule_trigger_state_update(
        short_addr,
        rep->cluster,
        rep->attr.attr_id,
        value,           // может быть NULL — обрабатывается внутри
        data_len,
        attr_type
    );

    // Обновляем статус устройства
    dev->lqi = 10;
    dev->last_seen_ms = esp_log_timestamp();
    dev->is_online = true;

    result = ESP_OK;
    return result;
}