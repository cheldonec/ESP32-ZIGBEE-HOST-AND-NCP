#ifndef ZB_MANAGER_DEVICES_H
#define ZB_MANAGER_DEVICES_H
#include "zb_manager_clusters.h"
//#include "zb_manager_tuya_dp.h"

#include "ncp_host_zb_api_core.h"
//#include "ha_integration.h"

//#
//#include ""
#include "esp_timer.h"
#include "cJSON.h"
#include "spiffs_helper.h"

// Таймауты по умолчанию (в миллисекундах)
#define ZB_DEVICE_DEFAULT_TIMEOUT_MS        60000       // 60 сек — обычные устройства
#define ZB_DEVICE_SENSOR_TIMEOUT_MS         1800000      // 30 мин — батарейные датчики
#define ZB_DEVICE_SWITCH_TIMEOUT_MS         600000      // 10 мин — выключатели
#define ZB_DEVICE_ROUTER_TIMEOUT_MS         300000       // 5 мин — роутеры/шлюзы
#define ZB_DEVICE_TUYA_COIL_TIMEOUT_MS      180000      // 3 мин — tuya rele


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
    char                                    friendly_name[125];
    //uint8_t                                 appending_ep_data_counter;  // используется при добавлении устройств, в конфиге не хранится
    uint16_t                                short_addr;             //                                   
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

typedef struct {
    uint8_t                                 index_in_sheduler_array;
    device_custom_t*                        appending_device;
    esp_timer_handle_t                      appending_controll_timer;
    esp_zb_zcl_read_attr_cmd_t              tuya_magic_read_attr_cmd_param;    //
    uint8_t                                 tuya_magic_req_tsn;
    uint8_t                                 tuya_magic_resp_tsn;
    uint8_t                                 tuya_magic_status;         // 0-инициализация 1 - в процессе, 2 - готово
    uint8_t                                 tuya_magic_try_count;
    
    local_esp_zb_zdo_active_ep_req_param_t        active_ep_req;
   
    uint8_t                                 active_ep_req_status;      // 0-инициализация 1 - в процессе, 2 - готово
    uint8_t                                 simple_desc_req_count;
    
    local_esp_zb_zdo_simple_desc_req_param_t*     simple_desc_req_list;
    
    
    uint8_t*                                simple_desc_req_simple_desc_req_list_status;    // 0-инициализация 1 - в процессе, 2 - готово
    //esp_zb_zdo_bind_req_param_t*            bind_req_list;
    //uint8_t                                 bind_req_count;
    //uint8_t*                                bind_req_list_status; // 0-инициализация 1 - в процессе, 2 - готово
}device_appending_sheduler_t;


endpoint_custom_t* RemoteDeviceEndpointCreate(uint8_t ep_id); // создаёт пустую точку 0xff далее её надо заполнить или при создании устройства или при чтении из файла
esp_err_t RemoteDeviceEndpointDelete(endpoint_custom_t* ep_object);
device_custom_t*   RemoteDeviceCreate(esp_zb_ieee_addr_t ieee_addr); // скорее всего применение из ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED

/****************************************  Массивы для хранения устройств и для процесса добавления  *********************************/
#define REMOTE_DEVICES_COUNT (16)
#define DEVICE_APPEND_SHEDULER_COUNT (2)
extern uint8_t RemoteDevicesCount;
extern device_custom_t** RemoteDevicesArray;
extern SemaphoreHandle_t g_device_array_mutex; // mutex для массива устройств
#define DEVICE_ARRAY_LOCK10()   if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return
#define DEVICE_ARRAY_LOCK20()   if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(20)) != pdTRUE) return
#define DEVICE_ARRAY_LOCK1000()   if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) return
#define ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS        1000
#define ZB_DEVICE_MUTEX_TIMEOUT_SHORT_MS       100
#define DEVICE_ARRAY_UNLOCK() xSemaphoreGive(g_device_array_mutex)


#define ZB_SAVE_CMD_QUEUE_SIZE  3
#define ZB_SAVE_CMD_SAVE        1

extern uint8_t DeviceAppendShedulerCount;
extern device_appending_sheduler_t** DeviceAppendShedulerArray;
extern QueueHandle_t zb_manager_save_JSon_cmd_queue;

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

/* Load and Save Functions*/
void ieee_to_str(char* out, const esp_zb_ieee_addr_t addr);

bool str_to_ieee(const char* str, esp_zb_ieee_addr_t addr);

void zb_manager_update_device_lqi(uint16_t short_addr, uint8_t lqi);

uint32_t get_timeout_for_device_id(uint16_t device_id);

void zb_manager_configure_device_timeout(device_custom_t* dev);

/**
 * @brief Обновляет статус онлайн/офлайн устройства
 * @param dev Указатель на устройство
 * @return true если онлайн, false если офлайн
 */
bool zb_manager_update_device_online_status(device_custom_t* dev);

void zb_manager_update_device_activity(uint16_t short_addr, uint8_t lqi);

const char* zb_manager_get_ha_device_type_name(uint16_t device_id);
//esp_err_t zb_manager_save_devices_to_json(const char *filepath);

// поиск по короткому адресу с кэшем
// Для вызова извне (с мьютексом)
device_custom_t* zb_manager_find_device_by_short_safe(uint16_t short_addr);

// Только для вызова из-под мьютекса!
device_custom_t* zb_manager_find_device_by_short(uint16_t short_addr);

// поиск по короткому адресу и endpoint
endpoint_custom_t* zb_manager_find_endpoint(uint16_t short_addr, uint8_t endpoint_id);

endpoint_custom_t* zb_manager_find_endpoint_safe(uint16_t short_addr, uint8_t endpoint_id);
/**
 * @brief Автоопределение manufacturer_code по IEEE-адресу
 */
uint16_t zb_manager_guess_manufacturer_code(const uint8_t ieee_addr[8]);

esp_err_t zb_manager_queue_save_request(void);
esp_err_t zb_manager_print_RemoteDevicesArray (void);
esp_err_t zb_manager_load_devices_from_json(const char *filepath);

esp_err_t json_load_and_print(const char *filepath);

#define LOAD_STRING(json_obj, key, dst) \
    do { \
        cJSON *item = cJSON_GetObjectItem(json_obj, key); \
        if (item && cJSON_IsString(item) && item->valuestring) { \
            strncpy(dst, item->valuestring, sizeof(dst) - 1); \
            dst[sizeof(dst) - 1] = '\0'; \
        } \
    } while(0)

// переопределение некоторых устройств в конце сопряжения перед полным сохранением (например tuya выключатель говорит, что он на батарейках, хотя он single_pfase)  
void zb_manager_apply_device_fixups(device_custom_t *dev);
bool zb_manager_is_device_always_powered(device_custom_t *dev);
/*
zb_manager_load_devices_from_json("/spiffs/devices.json");
zb_manager_save_devices_to_json("/spiffs/devices.json");
*/

//=== МАКРОСЫ ДЛЯ JSON ===
// === Утилиты для загрузки из JSON ===
#define LOAD_NUMBER(json_obj, key, dst)                 \
    do {                                                \
        cJSON *item = cJSON_GetObjectItem(json_obj, key); \
        if (item && cJSON_IsNumber(item)) {             \
            dst = (typeof(dst))item->valueint;          \
        }                                               \
    } while(0)

#define LOAD_STRING(json_obj, key, dst)                 \
    do {                                                \
        cJSON *item = cJSON_GetObjectItem(json_obj, key); \
        if (item && cJSON_IsString(item) && item->valuestring) { \
            strncpy(dst, item->valuestring, sizeof(dst) - 1); \
            dst[sizeof(dst) - 1] = '\0';                \
        }                                               \
    } while(0)

#define LOAD_LAST_UPDATE(item, obj) do { \
cJSON *ts = cJSON_GetObjectItem(item, "last_update_ms"); \
    if (ts && cJSON_IsNumber(ts)) { (obj)->last_update_ms = ts->valueint; } \
    else { (obj)->last_update_ms = esp_log_timestamp(); } \
} while(0)

#endif