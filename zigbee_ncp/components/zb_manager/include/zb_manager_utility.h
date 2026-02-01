#ifndef ZB_M_UTILITY_H
#define ZB_M_UTILITY_H
#include "esp_zigbee_core.h"
#include "zb_config.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"
//zb_manager_match_desc();


/******************************************* EXTERN LOG FUNCTIONS START ******************************************/

typedef void (*print_log_to_screen_CB_t)(char* text, uint32_t color);

/****************************************** MATCH_DESC START ******************************************/
typedef struct match_desc_resp_record_s{
    uint16_t dev_short_address;
    uint8_t endpoint;
}match_desc_resp_record_t;

typedef struct match_desc_resp_records_s{
    esp_err_t status;
    uint8_t resp_lqi;
    uint8_t tsn;                           // Transaction Sequence Number
    uint16_t count;
    match_desc_resp_record_t* records;
}match_desc_resp_records_t;

typedef uint8_t (*zb_manager_match_desc_func_t)(esp_zb_zdo_match_desc_req_param_t* param, uint16_t waiting_ms, void* user_ctx);
typedef void (*zb_manager_match_desc_func_CB_t)(match_desc_resp_records_t* result, void* user_ctx);
typedef void(*zb_manager_print_match_desc_resp_records_func_t)(void);
uint8_t zb_manager_match_desc_func(esp_zb_zdo_match_desc_req_param_t* param, uint16_t waiting_ms, void* user_ctx);
void zb_manager_print_match_desc_resp_records(void);
/****************************************** MATCH_DESC END ******************************************/

/****************************************** IEEE_ADDR START ******************************************/

typedef struct ieee_addr_resp_records_s{
    esp_err_t status;
    uint8_t resp_lqi;
    uint8_t tsn;                           // Transaction Sequence Number
    uint16_t short_addr;                   // nwk
    esp_zb_ieee_addr_t ieee_addr;           /*!< 64-bit address for the Remote Device. */
    uint16_t children_count;
    uint8_t children_rep_start_index;
    uint16_t* children;                     // short_addresses
}ieee_addr_resp_records_t;


typedef uint8_t (*zb_manager_ieee_addr_req_func_t)(esp_zb_zdo_ieee_addr_req_param_t* param, uint16_t waiting_ms, void* user_ctx);
typedef void (*zb_manager_ieee_addr_req_func_CB_t)(ieee_addr_resp_records_t* result, void* user_ctx);
typedef void(*zb_manager_print_ieee_addr_resp_records_func_t)(void);
uint8_t zb_manager_ieee_addr_func(esp_zb_zdo_ieee_addr_req_param_t* param, uint16_t waiting_ms, void* user_ctx);
void zb_manager_print_ieee_addr_resp_records(void);
/******************************************* IEEE_ADDR END ******************************************/

/******************************************* NODE_DESC START ******************************************/

typedef union {
    uint16_t flags;
    struct flags_values_s{
        uint8_t logic_type : 3;                             //logical type: 0 - ZC, 1 - ZR, 2 - ZED
        uint8_t complex_desc_available : 1;
        uint8_t user_desc_available : 1;
        uint8_t reserv1 : 3;
        uint8_t aps_flags : 3;
        uint8_t reserv2:1;
        uint8_t frequency_band : 4;
    }flags_values_t;
}node_desc_resp_flags_t;

typedef union {
    uint16_t server_mask;
    struct server_mask_values_s{
        uint8_t primary_trust_center : 1;
        uint8_t backup_trust_center : 1;
        uint8_t primary_binding_table_cache : 1;
        uint8_t backup_binding_table_cache : 1;
        uint8_t primary_discovery_cache : 1;
        uint8_t backup_discovery_cache : 1;
        uint8_t network_manager : 1;
        uint8_t reserv : 2;
        uint8_t stack_complians_rev :7;
    }server_mask_values_t;
}node_desc_resp_server_mask_t;

typedef union {
    uint8_t mac_capabilities;
    struct mac_capabilities_values_s{
        uint8_t reserv1 : 1;
        uint8_t ffd_or_rfd : 1;   // rfd - 0, ffd - 1  rfd - конечная точка, ffd - расширенные функции  
        uint8_t power_source : 1; // 1 - device is receiving power from the alternating current mains, otherwise - 0. (1 от сети переменного тока)
        uint8_t receive_on_when_idle : 1; //1 - device does not disable its receiver to save power during idle periods, otherwise - 0.
        uint8_t reserv2 : 1;
        uint8_t reserv3 : 1;
        uint8_t security_capability : 1;
        uint8_t allocated_address : 1; // 1 - device wants the coordinator to allocate 16bit short address as a result of the association procedure, 0 - address is self
                                        //selected while using NWK rejoin command.
    }mac_capabilities_values_t;
}node_desc_resp_mac_capabililities_t;

typedef struct node_desc_resp_record_s{
    esp_err_t status;
    uint8_t resp_lqi;
    uint8_t tsn;                           // Transaction Sequence Number
    node_desc_resp_flags_t node_flags;
    node_desc_resp_server_mask_t server_mask_flags;
    node_desc_resp_mac_capabililities_t mac_capabilities; 
    /*uint8_t flags_logic_type; //0:zc, 1:zr, 2:zed
    uint8_t flags_complex_desc_available; //0:false, 1:true
    uint8_t flags_user_desc_available; //0:false, 1:true
    uint8_t flags_aps_flags;
    uint8_t flags_frequency_band;*/
    /*uint8_t mac_ffd_or_rfd; //0:rfd, 1:ffd
    uint8_t mac_recieve_on_when_idle; //  1 - device does not disable its receiver to save power during idle periods, otherwise - 0.
    uint8_t mac_security_capability; // 0: no security, 1: device supports security
    uint8_t mac_allocated_address; // 0: allocated address, 1: unallocated address ( 1 - device wants the coordinator to allocate 16bit short address as a result of the association procedure, 0 - address is self
                                   // selected while using NWK rejoin command.)*/
    uint16_t manuf_code;
    uint8_t max_buff_size;
    uint16_t max_incoming_transfer_size;
    uint16_t max_outgoing_transfer_size;
    /*uint8_t server_mask_primary_trust_center;
    uint8_t server_mask_backup_trust_center;
    uint8_t server_mask_primary_binding_table_cache;
    uint8_t server_mask_backup_binding_table_cache;
    uint8_t server_mask_primary_discovery_cache;
    uint8_t server_mask_backup_discovery_cache;
    uint8_t server_mask_network_manager; // 0: no network manager, 1: network manager
    uint8_t server_mask_stack_complians_rev;*/
    uint8_t desc_capabilities_ext_active_ep_list_available;
    uint8_t desc_capabilities_ext_simple_desc_available;
    uint16_t nwk_addr;
}node_desc_resp_record_t;

typedef uint8_t (*zb_manager_node_desc_req_func_t)(esp_zb_zdo_node_desc_req_param_t* param, uint16_t waiting_ms, void* user_ctx);
typedef void (*zb_manager_node_desc_req_func_CB_t)(node_desc_resp_record_t* result, void* user_ctx);
typedef void(*zb_manager_print_node_desc_resp_records_func_t)(void);
uint8_t zb_manager_node_desc_func(esp_zb_zdo_node_desc_req_param_t* param, uint16_t waiting_ms, void* user_ctx);
void zb_manager_print_node_desc_resp_records(void);
/****************************************** NODE_DESC END ******************************************/

typedef struct simple_desc_resp_records_s {
    esp_err_t status;
    uint8_t resp_lqi;
    uint8_t tsn;                           // Transaction Sequence Number
    uint16_t short_addr;                   // nwk
    uint8_t    endpoint;                        /*!< Endpoint */
    uint16_t   app_profile_id;                  /*!< Application profile identifier */
    uint16_t   app_device_id;                   /*!< Application device identifier */
    uint32_t   app_device_version: 4;           /*!< Application device version */
    uint32_t   reserved: 4;                     /*!< Reserved */
    uint8_t    app_input_cluster_count;         /*!< Application input cluster count */
    uint8_t    app_output_cluster_count;        /*!< Application output cluster count */
    uint16_t   *input_clusters_list;            
    uint16_t   *output_clusters_list;
}simple_desc_resp_record_t;

typedef uint8_t (*zb_manager_simple_desc_req_func_t)(esp_zb_zdo_simple_desc_req_param_t* param, uint16_t waiting_ms, void* user_ctx);
typedef void (*zb_manager_simple_desc_req_func_CB_t)(simple_desc_resp_record_t* result, void* user_ctx);
uint8_t zb_manager_simple_desc_func(esp_zb_zdo_simple_desc_req_param_t* param, uint16_t waiting_ms, void* user_ctx);
#endif