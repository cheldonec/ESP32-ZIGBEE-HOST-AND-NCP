#ifndef ZB_MANAGER_DEVICES_H
#define ZB_MANAGER_DEVICES_H
#include "zb_manager_clusters.h"
#include "zbm_dev_types.h"
#include "zbm_dev_base.h"
#include "zbm_dev_append_sheduler.h"

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


//int ieee_addr_compare(esp_zb_ieee_addr_t *a, esp_zb_ieee_addr_t *b);



typedef struct {
    uint8_t ep_id;
    uint8_t status;
}dev_annce_simple_desc_controll_t;



/*typedef struct {
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
}device_appending_sheduler_t;*/


endpoint_custom_t* RemoteDeviceEndpointCreate(uint8_t ep_id); // создаёт пустую точку 0xff далее её надо заполнить или при создании устройства или при чтении из файла
esp_err_t RemoteDeviceEndpointDelete(endpoint_custom_t* ep_object);
device_custom_t*   RemoteDeviceCreate(esp_zb_ieee_addr_t ieee_addr); // скорее всего применение из ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED
esp_err_t RemoteDeviceDelete(device_custom_t* dev_object);

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
//void ieee_to_str(char* out, const esp_zb_ieee_addr_t addr);

//bool str_to_ieee(const char* str, esp_zb_ieee_addr_t addr);

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

/**
 * @brief Безопасная версия: возвращает JSON всех устройств с мьютексом
 * @return Указатель на cJSON объект или NULL при ошибке
 */
cJSON* zb_manager_get_full_json_from_remote_devices_array_safe(void);

/**
 * @brief Формирует JSON-объект со всеми устройствами (без мьютекса)
 * @return Указатель на cJSON объект или NULL при ошибке
 */
cJSON* zb_manager_get_full_json_from_remote_devices_array(void);

cJSON *zb_manager_get_device_json(device_custom_t *dev);
cJSON *zb_manager_get_device_json_safe(device_custom_t *dev);



esp_err_t json_load_and_print(const char *filepath);


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

/*#define LOAD_LAST_UPDATE(item, obj) do { \
cJSON *ts = cJSON_GetObjectItem(item, "last_update_ms"); \
    if (ts && cJSON_IsNumber(ts)) { (obj)->last_update_ms = ts->valueint; } \
    else { (obj)->last_update_ms = esp_log_timestamp(); } \
} while(0)*/

#endif