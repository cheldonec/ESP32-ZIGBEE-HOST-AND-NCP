#ifndef ZB_MANAGER_H
#define ZB_MANAGER_H
#include "esp_zigbee_core.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"
#include "zb_manager_endpoints_gen.h"
#include "zb_manager_utility.h"
#include "zb_manager_obj_types.h"
#include "zb_manager_cluster_data_helper.h"
#include "esp_ncp_frame.h"
#include "esp_ncp_zb.h"
#include "zb_manager_devices.h"
#include "zboss_api.h"


#define ZIGBEE_STACK_SIZE 16 * 1024
#define ZIGBEE_EVENTS_STACK_SIZE 16 * 1024

// сообщения как вхоящие, так и исходящие
typedef struct zb_event_msg_s {
    esp_event_base_t event_base;
    int32_t event_id;
    size_t event_data_size;
    void* event_data;    
}zb_event_msg_t;

#define ZB_EVENTS_QUEUE_SIZE 128
#define ZB_EVENTS_QUEUE_ITEM_SIZE sizeof(zb_event_msg_t)
#define ZB_EVENTS_QUEUE_READ_TICS        (1000/ portTICK_PERIOD_MS) //100


#define EVENT_LOOP_POST_DELAY 100



// #define REMOTE_DEVICES_COUNT (32) // define in zb_manager_devices.h
/*bool zb_event_msg_post(esp_event_base_t event_base, int32_t event_id, void* event_data, size_t event_data_size, TickType_t ticks_to_wait);

//EVENT from ZB_EVENTS MODULE
ESP_EVENT_DECLARE_BASE(ZB_EVENT_MANAGER_BASE);
typedef enum {
    ZB_EVENT_MANAGER_INIT_START,
    ZB_EVENT_MANAGER_INIT_DONE,
}zb_events_t;


typedef struct zb_manager_init_s {
    esp_zb_ep_list_t *esp_local_zb_ep_list;
    esp_zb_core_action_callback_t action_handler; //если не задан, то по умолчанию используется встроенный обработчик
    esp_zb_zcl_raw_command_callback_t raw_command_handler; //если не задан, то по умолчанию используется встроенный обработчик
}zb_manager_init_t;

esp_err_t zb_event_manager_init(zb_manager_init_t init_params);*/

extern zb_manager_t zb_manager_obj;

/**
* @brief Пример: 
esp_err_t  err = zb_manager_init();
*/
esp_err_t zb_manager_init(void);

/**
* @brief Пример: 
esp_err_t  err = zb_manager_start();
*/
esp_err_t zb_manager_start(void);

extern void esp_zb_bdb_set_legacy_device_onboarding_key(const uint8_t key[16]);

/************************************ !!!!!!!!!!!!!! Должны быть обязательно созданы пользователем и зарегистрированы */
/**
* @brief Пример: esp_err_t zb_manager_register_match_desc_cb(zb_manager_zb_manager_match_desc_func_CB_t match_desc_callback);
void match_desc_callback(match_desc_resp_records_t* result, void* user_ctx)
{
    if (user_ctx != NULL) {
        uint8_t* t = (uint8_t*)user_ctx;
        ESP_LOGW(TAG, "match_desc_callback with user_ctx: %02x and tsn: %02x", *t, result->tsn);
    }else ESP_LOGW(TAG, "match_desc_callback without user_ctx and tsn: %02x" , result->tsn);
    zb_manager_obj.utility_functions.print_last_match_desc_resp_result_func();
}
    zb_manager_register_match_desc_cb(match_desc_callback);
*/
esp_err_t zb_manager_register_match_desc_cb(zb_manager_match_desc_func_CB_t match_desc_callback);

esp_err_t zb_manager_register_ieee_addr_req_cb(zb_manager_ieee_addr_req_func_CB_t ieee_addr_req_callback);

esp_err_t zb_manager_register_node_desc_cb(zb_manager_node_desc_req_func_CB_t node_desc_callback);

esp_err_t zb_manager_register_simple_desc_cb(zb_manager_simple_desc_req_func_CB_t simple_desc_callback);

esp_err_t zb_manager_register_print_log_to_screen_cb(print_log_to_screen_CB_t print_log_to_screen_callback);

/*******************************************************************************************************************************************************
***  API  ***                             zb_manager_obj.endpoints_func.generate_endpoint_func                                                       ***
********************************************************************************************************************************************************
* @brief Генерация эндпоинтов с кластерами и атрибутами, должна быть вызвана после zb_manager_init() и до zb_manager_start()
* esp_err_t zb_manager_obj.endpoints_func.generate_endpoint_func(custom_zb_endpoint_add_str_t* ep_str, esp_zb_endpoint_config_t endpoint_config);
* @param[in] ep_str - структура с описанием эндпоинта @ref custom_zb_endpoint_add_str_t
* @param[in] endpoint_config - конфигурация эндпоинта @ref esp_zb_endpoint_config_t
* @return ESP_OK - если эндпоинт успешно создан, иначе ESP_FAIL
* Sample:
        #define LOCAL_REMOTE_ENDPOINT1_CONFIG()                              \
        {                                                                    \
            .endpoint = HA_REMOTE_DEVICE_ENDPOINT1,                          \
            .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,                       \   
            .app_device_id = ESP_ZB_HA_REMOTE_CONTROL_DEVICE_ID,             \
            .app_device_version = 0,                                         \
        }
            
        uint16_t inputClusterEP1[] = {0x0000, 0x0003};// 
        uint16_t outputClusterEP1[] = {0x0003};// 
        custom_zb_endpoint_add_str_t endpoint1 = {
            .inputClusterCount = sizeof(inputClusterEP1) / sizeof(inputClusterEP1[0]),
            .inputClusterList = inputClusterEP1,
            .outputClusterCount = sizeof(outputClusterEP1) / sizeof(outputClusterEP1[0]),
            .outputClusterList = outputClusterEP1,
        };
        esp_zb_endpoint_config_t endpoint1_config = LOCAL_REMOTE_ENDPOINT1_CONFIG();
        zb_manager_obj.endpoints_func.generate_endpoint_func(&endpoint1,endpoint1_config);
*****************************************************************************************
* @details При успешном выполнении функции, создается эндпоинт с кластерами и атрибутами согласно спецификации ZCL8
* Регистрация эндпоинтов происходит после вызова zb_manager_start()
*/

/*******************************************************************************************************************************************************
***  API  ***                             zb_manager_obj.utility_functions.cmd_to_call_match_desc_func                                               ***
******************************************************************************************************************************************************** 
* @brief uint8_t zb_manager_obj.utility_functions.cmd_to_call_match_desc_func(esp_zb_zdo_match_desc_req_param_t* param, uint8_t waiting_seconds, void* user_ctx);
* @param[in] param - параметры запроса @ref esp_zb_zdo_match_desc_req_param_t
* @param[in] waiting_seconds - время ожидания ответа от устройства в секундах
* @param[in] user_ctx - контекст, который будет передан в callback
* @return tsn 0 - если запрос отправлен успешно, иначе 0xff неудачно
* Sample:
        uint16_t cluster_list[] = {ESP_ZB_ZCL_CLUSTER_ID_ON_OFF};
        esp_zb_zdo_match_desc_req_param_t req ={
            .dst_nwk_addr = 0xFFFF,
            .addr_of_interest = 0xFFFF,
            .profile_id = ESP_ZB_AF_HA_PROFILE_ID,
            .num_in_clusters = 1,
            .num_out_clusters = 0,
            .cluster_list = cluster_list,
        };
        uint8_t t = 10;
        zb_manager_obj.utility_functions.cmd_to_call_match_desc_func(&req, 3, (void*)&t);
        
* @details При успешном запуске запроса, функция возвращает tsn. Спустя waiting_seconds секунд, возвращается результат запроса. 
* @note Результат возвращается в callback, который зарегистрирован пользователем. Например так :
    void match_desc_callback(match_desc_resp_records_t* result, void* user_ctx)
    {
        if (user_ctx != NULL) {
            uint8_t* t = (uint8_t*)user_ctx;
            ESP_LOGW(TAG, "match_desc_callback with user_ctx: %02x and tsn: %02x", *t, result->tsn);
        }else ESP_LOGW(TAG, "match_desc_callback without user_ctx and tsn: %02x" , result->tsn);
        zb_manager_obj.utility_functions.print_last_match_desc_resp_result_func();
    }
    zb_manager_register_match_desc_cb(match_desc_callback);

Результат запроса также сохраняется в zb_manager_obj.match_desc_resp_result @ref match_desc_resp_records_t и доступен до запуска следующего запроса 
zb_manager_obj.utility_functions.cmd_to_call_match_desc_func 
**********************************************************************************************************************************************************
*
*
*
*
*
*
******************************************************************************************************************/

#endif

