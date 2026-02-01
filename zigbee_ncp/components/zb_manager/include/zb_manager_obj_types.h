#ifndef ZB_M_OBJ_TYPES_H
#define ZB_M_OBJ_TYPES_H
#include "esp_zigbee_core.h"
#include "zb_manager_endpoints_gen.h"
#include "zb_manager_utility.h"
#include "zb_manager_devices.h"

/************************************  ACTION HANDLERS BEGIN ***************************************/
/******************************  ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID ***********************************/
typedef esp_err_t (*info_attr_set_value_handler_t)(const esp_zb_zcl_set_attr_value_message_t *message);

/******************************  ESP_ZB_CORE_REPORT_ATTR_CB_ID **************************************/
typedef esp_err_t (*info_attr_reporting_handler_t)(const esp_zb_zcl_report_attr_message_t *message);

/******************************  ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID **************************************/
typedef esp_err_t (*resp_to_cmd_attr_read_handler_t)(const esp_zb_zcl_cmd_read_attr_resp_message_t *message);

/******************************  ESP_ZB_CORE_CMD_WRITE_ATTR_RESP_CB_ID **************************************/
typedef esp_err_t (*resp_to_cmd_attr_write_handler_t)(const esp_zb_zcl_cmd_write_attr_resp_message_t *message);

/******************************  ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID **************************************/
typedef esp_err_t (*resp_to_cmd_config_report_handler_t)(const esp_zb_zcl_cmd_config_report_resp_message_t *message);

/******************************  ESP_ZB_CORE_CMD_DISC_ATTR_RESP_CB_ID **************************************/
typedef esp_err_t (*resp_to_cmd_attr_discover_handler_t)(const esp_zb_zcl_cmd_discover_attributes_resp_message_t *message);

/******************************  ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID **************************************/
typedef esp_err_t (*defresp_to_cmd_handler_t)(const esp_zb_zcl_cmd_default_resp_message_t *message);

/******************************  ESP_ZB_CORE_CMD_IAS_ZONE_ZONE_STATUS_CHANGE_NOT_ID ***********************************/
typedef esp_err_t (*info_ias_zone_status_change_handler_t)(const esp_zb_zcl_ias_zone_status_change_notification_message_t *message);

/******************************  ESP_ZB_CORE_CMD_IAS_ZONE_ZONE_ENROLL_REQUEST_ID ***********************************/
typedef esp_err_t (*input_req_ias_zone_enroll_handler_t)(const esp_zb_zcl_ias_zone_enroll_request_message_t *message);

typedef struct zb_manager_action_handlers_s {
    info_attr_set_value_handler_t               info_attr_set_value_handler_f;          //esp_zb_zcl_set_attr_value_message_t
    info_attr_reporting_handler_t               info_attr_reporting_handler_f;          //esp_zb_zcl_report_attr_message_t
    resp_to_cmd_attr_read_handler_t             resp_to_cmd_attr_read_handler_f;        //esp_zb_zcl_cmd_read_attr_resp_message_t
    resp_to_cmd_attr_write_handler_t            resp_to_cmd_attr_write_handler_f;       //esp_zb_zcl_cmd_write_attr_resp_message_t
    resp_to_cmd_config_report_handler_t         resp_to_cmd_config_report_handler_f;    //esp_zb_zcl_cmd_config_report_resp_message_t
    resp_to_cmd_attr_discover_handler_t         resp_to_cmd_attr_discover_handler_f;    //esp_zb_zcl_cmd_discover_attributes_resp_message_t
    defresp_to_cmd_handler_t                    defresp_to_cmd_handler_f;               //esp_zb_zcl_cmd_default_resp_message_t
    info_ias_zone_status_change_handler_t       info_ias_zone_status_change_handler_f;  //esp_zb_zcl_ias_zone_status_change_notification_message_t
    input_req_ias_zone_enroll_handler_t         input_req_ias_zone_enroll_handler_f;    //esp_zb_zcl_ias_zone_enroll_request_message_t
}zb_manager_action_handlers_t;
/************************************  ACTION HANDLERS END ***************************************/


/********************************** ZB_UTILITY FUNCTIONS BEGIN *************************************/
typedef struct zb_manager_utility_functions_s {
     // Params (esp_zb_zcl_cmd_match_desc_t req, uint8_t waiting_ms, void* user_data) 
     // Result tsn (If return 0xff, then no match_desc available now)   
    zb_manager_match_desc_func_t                      cmd_to_call_match_desc_func;
    zb_manager_print_match_desc_resp_records_func_t   print_last_match_desc_resp_result_func;

    // Params (esp_zb_zdo_ieee_addr_req_param_t req, uint8_t waiting_ms, void* user_data) 
     // Result tsn (If return 0xff, then no ieee_addr_req available now)   
    zb_manager_ieee_addr_req_func_t                   cmd_to_call_ieee_addr_req_func;
    zb_manager_print_ieee_addr_resp_records_func_t    print_last_ieee_addr_resp_result_func;
    
    // Params (esp_zb_zdo_node_desc_req_param_t req, uint8_t waiting_ms, void* user_data) 
     // Result tsn (If return 0xff, then no node_desc_req available now) 
    zb_manager_node_desc_req_func_t                   cmd_to_call_node_desc_req_func;
    zb_manager_print_node_desc_resp_records_func_t    print_last_node_desc_resp_result_func;

    zb_manager_simple_desc_req_func_t                 cmd_to_call_simple_desc_req_func;
}zb_manager_utility_functions_t;

typedef struct zb_manager_utility_functions_callbacks_s {
    zb_manager_match_desc_func_CB_t                   match_desc_callback; //
    zb_manager_ieee_addr_req_func_CB_t                ieee_addr_req_callback; //
    zb_manager_node_desc_req_func_CB_t                node_desc_req_callback; //
    zb_manager_simple_desc_req_func_CB_t              simple_desc_req_callback;
    print_log_to_screen_CB_t                          print_log_to_screen_callback; // 
}zb_manager_utility_functions_callbacks_t;

typedef struct zb_manager_utility_values_s {
    match_desc_resp_records_t                         last_match_desc_resp_result;
    ieee_addr_resp_records_t                          last_ieee_addr_resp_result;
    node_desc_resp_record_t                           last_node_desc_resp_result;
    simple_desc_resp_record_t                         last_simple_desc_resp_result;
}zb_manager_utility_values_t;

/********************************** ZB_UTILITY FUNCTIONS END *************************************/

/*********************************** ENDPOINTS AND CLUSTERS GENERATOR BEGIN *****************************/

typedef struct zb_manager_endpoints_func_s {
    /** @brief Генерация эндпоинтов с кластерами и атрибутами
     * 
     * @param[in] ep_str - структура с описанием эндпоинта @ref custom_zb_endpoint_add_str_t
     * @param[in] endpoint_config - конфигурация эндпоинта @ref esp_zb_endpoint_config_t
     * @return esp_err_t ESP_OK - если эндпоинт успешно создан, иначе ESP_FAIL
     * @example: Sample in @ref zb_manager.h
     */
    zb_manager_generate_endpoint_func_t         generate_endpoint_func;
}zb_manager_endpoints_func_t;
/*********************************** ENDPOINTS AND CLUSTERS GENERATOR END *****************************/

typedef struct zb_manager_s {
    uint8_t RemoteDevicesCount;
    device_custom_t** RemoteDevicesArray;
    zb_manager_endpoints_func_t endpoints_func; // автосоздание при вызове zb_manager_init(zb_manager_t* zb_manager)
    zb_manager_utility_functions_t utility_functions; //автосоздание при вызове zb_manager_init(zb_manager_t* zb_manager)
    zb_manager_utility_functions_callbacks_t utility_functions_callbacks; //создаёт пользователь, иначе отработают встроенные функции
    zb_manager_utility_values_t utility_results;
    esp_zb_ep_list_t *esp_local_zb_ep_list;       // автосоздание при вызове zb_manager_init(zb_manager_t* zb_manager)
    zb_manager_action_handlers_t action_handlers; //если не задан, то по умолчанию используется встроенный обработчик
    esp_zb_zcl_raw_command_callback_t raw_command_handler; //если не задан, то по умолчанию используется встроенный обработчик
}zb_manager_t;

#endif