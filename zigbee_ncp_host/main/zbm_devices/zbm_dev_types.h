#ifndef ZBM_DEV_TYPES_H
#define ZBM_DEV_TYPES_H

#include "stdint.h"
#include "esp_zigbee_type.h"
#include "zb_manager_clusters.h"

typedef struct attribute_custom_s{
    uint16_t                    id;
    char                        attr_id_text[64];
    uint8_t                     type;       /*!< Attribute type see zcl_attr_type */
    uint8_t                     acces;      /*!< Attribute access options according to esp_zb_zcl_attr_access_t */
    uint16_t                    size;
    uint8_t                     is_void_pointer; /*!if (attr_type < 0x41U && attr_type > 0x51U) is_void_pointer = 0 // 0x41U - 0x51U размер в себе имеют и поэтому они будут через указатель*/
    uint16_t                    manuf_code;
    uint16_t                    parent_cluster_id;
    void*                       p_value;
}attribute_custom_t;


typedef struct cluster_custom_s{
    uint16_t                    id;
    char                        cluster_id_text[64];
    uint8_t                     is_use_on_device;         // 0 no 1-yes
    uint8_t                     role_mask; // esp_zb_zcl_cluster_role_t;
    uint16_t                    manuf_code;
    uint16_t                    attr_count;
    attribute_custom_t**        attr_array;
    // Надо добавить специфичные команды
}cluster_custom_t;

typedef struct endpoint_custom_s{
    uint8_t                                             ep_id;
    uint8_t                                             is_use_on_device;         // 0 no 1-yes
    //char                                                friendly_name_len;
    char                                                friendly_name[125];
    uint16_t                                            deviceId;           //esp_zb_ha_standard_devices_t
    char                                                device_Id_text[64];
    //uint16_t                                            owner_dev_short;
    uint8_t                                             UnKnowninputClusterCount;
    cluster_custom_t**                                  UnKnowninputClusters_array;
    uint8_t                                             UnKnownoutputClusterCount;
    cluster_custom_t**                                  UnKnownoutputClusters_array;// скорее всего достаточно просто 0x0001, 0x0003, 0x0004 без объектов, просто перечисления, чтобы знать, кто кого биндить может
    uint8_t                                             specific_data_rec_count; 
    //endpoint_specific_data_rule_t** specific_data_array;
    uint8_t                                             endpoint_commands_count;
    //endpoint_command_t**        endpoint_commands_array;
    // кластеры, которые уже описаны и готовы к использованию
    //uint8_t                                             is_use_basic_cluster; // для сохранения и чтения из файла
    //zigbee_manager_basic_cluster_t*                     server_BasicClusterObj;
    uint8_t                                             is_use_identify_cluster; // для сохранения и чтения из файла
    zb_manager_identify_cluster_t*                      server_IdentifyClusterObj;
    uint8_t                                             is_use_temperature_measurement_cluster; // для сохранения и чтения из файла
    zb_manager_temperature_measurement_cluster_t*       server_TemperatureMeasurementClusterObj;
    //uint8_t                                             is_temp_cluster_binded;                     // сохраняем статус бинда
    uint8_t is_use_humidity_measurement_cluster;
    zb_manager_humidity_measurement_cluster_t*          server_HumidityMeasurementClusterObj;

    bool is_use_on_off_cluster;
    zb_manager_on_off_cluster_t*                        server_OnOffClusterObj;
    
    uint16_t*                                           output_clusters_array;
    uint8_t                                             output_clusters_count;
    //bool is_use_power_configuration_cluster;
    //zb_manager_power_config_cluster_t*           server_PowerConfigurationClusterObj;

    // Если нужно — callback и контекст на будущее, когда esp в роли EndDevice будет
    zigbee_on_off_apply_cb_t on_off_apply_cb;
    void* on_off_user_data;
}endpoint_custom_t;

typedef struct device_custom_s{
    uint8_t                                 is_in_build_status;
    //uint8_t                                 manuf_name_len;
    //uint8_t*                                manuf_name;
    uint8_t                                 index_in_array;
    uint8_t                                 friendly_name_len;
    char                                    friendly_name[125];
    //uint8_t                                 appending_ep_data_counter;  // используется при добавлении устройств, в конфиге не хранится
    uint16_t                                short_addr;             //
    uint16_t                                parent_short_addr;                                   
    esp_zb_ieee_addr_t                      ieee_addr;
    uint8_t                                 capability;
    uint8_t                                 lqi;                // уровень качества связи
    uint32_t                                last_seen_ms;         // время последнего контакт
    uint32_t                                device_timeout_ms;
    bool                                    is_online;
    zigbee_manager_basic_cluster_t*         server_BasicClusterObj;
    zb_manager_power_config_cluster_t*      server_PowerConfigurationClusterObj;
    uint8_t                                 endpoints_count;
    endpoint_custom_t**                     endpoints_array;
    uint16_t                                manufacturer_code;
    bool                                    has_pending_read;         // флаг, была ли команда на чтение атрибутов, исп-я при старте ESP для online статуса небатареечных устройств
    bool                                    has_pending_response;     // флаг, было ли получение ответа на запросы, исп-я при старте ESP для online статуса небатареечных устройств
    uint32_t                                last_pending_read_ms;
    uint32_t                                last_status_print_log_time;
    uint32_t                                last_bind_attempt_ms;
    //uint8_t                                 control_dev_annce_simple_desc_req_count;
    //dev_annce_simple_desc_controll_t**      control_dev_annce_simple_desc_req_array;
}device_custom_t;

#endif