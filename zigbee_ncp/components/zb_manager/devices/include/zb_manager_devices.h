#ifndef ZB_MANAGER_DEVICES_H
#define ZB_MANAGER_DEVICES_H
#include "zb_manager_clusters.h"
#include "esp_zigbee_core.h"
#include "esp_timer.h"
int ieee_addr_compare(esp_zb_ieee_addr_t *a, esp_zb_ieee_addr_t *b);



typedef struct attribute_custom_s{
    uint16_t                    id;
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
    uint8_t                                             is_use_basic_cluster; // для сохранения и чтения из файла
    zigbee_manager_basic_cluster_t*                     server_BasicClusterObj;
    uint8_t                                             is_use_identify_cluster; // для сохранения и чтения из файла
    zb_manager_identify_cluster_t*                      server_IdentifyClusterObj;
    uint8_t                                             is_use_temperature_measurement_cluster; // для сохранения и чтения из файла
    zb_manager_temperature_measurement_cluster_t*       server_TemperatureMeasurementClusterObj;
    //uint8_t                                             is_temp_cluster_binded;                     // сохраняем статус бинда
}endpoint_custom_t;

typedef struct {
    uint8_t ep_id;
    uint8_t status;
}dev_annce_simple_desc_controll_t;

typedef struct device_custom_s{
    uint8_t                                 is_in_build_status;
    //uint8_t                                 manuf_name_len;
    //uint8_t*                                manuf_name;
    uint8_t                                 index_in_array;
    uint8_t                                 friendly_name_len;
    char                                    friendly_name[64];
    //uint8_t                                 appending_ep_data_counter;  // используется при добавлении устройств, в конфиге не хранится
    uint16_t                                short_addr;             //                                   
    esp_zb_ieee_addr_t                      ieee_addr;
    uint8_t                                 capability;
    zigbee_manager_basic_cluster_t*         server_BasicClusterObj;
    uint8_t                                 endpoints_count;
    endpoint_custom_t**                     endpoints_array;
    //uint8_t                                 control_dev_annce_simple_desc_req_count;
    //dev_annce_simple_desc_controll_t**      control_dev_annce_simple_desc_req_array;
}device_custom_t;

typedef struct {
    uint8_t                                 index_in_sheduler_array;
    device_custom_t*                        appending_device;
    esp_timer_handle_t                      appending_controll_timer;
    esp_zb_zcl_read_attr_cmd_t              tuya_magic_read_attr_cmd_param;    //
    uint8_t                                 tuya_magic_req_tsn;
    uint8_t                                 tuya_magic_resp_tsn;
    uint8_t                                 tuya_magic_status;         // 0-инициализация 1 - в процессе, 2 - готово
    uint8_t                                 tuya_magic_try_count;
    esp_zb_zdo_active_ep_req_param_t        active_ep_req;
    uint8_t                                 active_ep_req_status;      // 0-инициализация 1 - в процессе, 2 - готово
    uint8_t                                 simple_desc_req_count;
    esp_zb_zdo_simple_desc_req_param_t*     simple_desc_req_list;
    uint8_t*                                simple_desc_req_simple_desc_req_list_status;    // 0-инициализация 1 - в процессе, 2 - готово
    //esp_zb_zdo_bind_req_param_t*            bind_req_list;
    //uint8_t                                 bind_req_count;
    //uint8_t*                                bind_req_list_status; // 0-инициализация 1 - в процессе, 2 - готово
}device_appending_sheduler_t;


endpoint_custom_t* RemoteDeviceEndpointCreate(uint8_t ep_id); // создаёт пустую точку 0xff далее её надо заполнить или при создании устройства или при чтении из файла
esp_err_t RemoteDeviceEndpointDelete(endpoint_custom_t* ep_object);
device_custom_t*   RemoteDeviceCreate(esp_zb_ieee_addr_t ieee_addr); // скорее всего применение из ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED

/****************************************  Массивы для хранения устройств и для процесса добавления  *********************************/
#define REMOTE_DEVICES_COUNT (32)
#define DEVICE_APPEND_SHEDULER_COUNT (5)
extern uint8_t RemoteDevicesCount;
extern device_custom_t** RemoteDevicesArray;

extern uint8_t DeviceAppendShedulerCount;
extern device_appending_sheduler_t** DeviceAppendShedulerArray;


esp_err_t zb_manager_delete_appending_sheduler(device_appending_sheduler_t* sheduler);
esp_err_t zb_manager_devices_init(void);


typedef struct build_dev_simple_desc_user_ctx_s{
    device_custom_t* parent_dev;
    endpoint_custom_t* ep;
    uint8_t try_if_error_timeout;
}build_dev_simple_desc_user_ctx_t;
/**************************************************** Temperature Sensor *************************************************************/

typedef struct zb_manager_temperature_sensor_ep_s{
    esp_zb_ieee_addr_t                                dev_ieee_addr;
    uint8_t                                           dev_endpoint;
    zigbee_manager_basic_cluster_t*                   dev_basic_cluster;
    zb_manager_identify_cluster_t*                    dev_identify_cluster;
    zb_manager_temperature_measurement_cluster_t*     dev_temperature_measurement_server_cluster;
}zb_manager_temperature_sensor_ep_t;

zb_manager_temperature_sensor_ep_t* temp_sensor_ep_create(void);
esp_err_t temp_sensor_ep_delete(zb_manager_temperature_sensor_ep_t* ep);



#endif