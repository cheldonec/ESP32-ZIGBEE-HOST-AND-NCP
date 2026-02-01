#ifndef ZB_MANAGER_NCP_HOST_H

#define ZB_MANAGER_NCP_HOST_H

//#include "esp_zigbee_core.h"
#include "ncp_host_zb_api_core.h"
#include "ncp_host_zb_api.h"
#include "zb_manager_devices.h"
#include "zb_manager_pairing.h"
#include "zb_manager_action_handler_worker.h"

extern esp_zb_ieee_addr_t LocalIeeeAdr; //esp_zb_get_ieee_addr(void);

extern bool g_zigbee_restarting; // для рестарта NCP и всего zigbee после сбоя NCP
extern TaskHandle_t xZB_TaskHandle; // для управления xZB_Handle (esp_zb_task)

typedef enum {
    NOT_INIT = 0,
    STARTED = 1,
    FOULTED = 2,
    RESTARTING = 3,
    RESTARTED = 3,
    WORKING = 4,
}zigbee_ncp_module_state_e;
//extern uint8_t local_RemoteDevicesCount;
//extern device_custom_t** local_RemoteDevicesArray;
//extern uint8_t local_DeviceAppendShedulerCount;
//extern device_appending_sheduler_t** local_DeviceAppendShedulerArray;

extern bool isZigbeeNetworkOpened;
extern zigbee_ncp_module_state_e zigbee_ncp_module_state;
#define ZIGBEE_STACK_SIZE 8 * 1024

#define ZIGBEE_TASK_PRIORITY 10

#define ESP_ZB_ZC_CONFIG()                                                              \
    {                                                                                   \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,                                  \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,                               \
        .nwk_cfg.zczr_cfg = {                                                           \
            .max_children = MAX_CHILDREN,                                               \
        },                                                                              \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = RADIO_MODE_UART_NCP,                      \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_mode = HOST_CONNECTION_MODE_UART,                 \
    }
    
/*
esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
   
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
*/
// Инициализация базы устройств
esp_err_t zb_manager_init_devices_base(void);


// Запуск ZIGBEE
// 1. Запускает HOST
extern esp_err_t esp_zb_platform_config(esp_zb_platform_config_t *config);
/************************************************************************/

//2. Запускаем основной поток
esp_err_t zb_manager_start_main_task(uint8_t core);
/***********************************************************************/
// 3. потом отправляет команду на  NCP 
extern esp_err_t zb_manager_init(void); // Initialize the Zigbee Manager "esp_zigbee_core.h" 
/************************************************************************/

// Создаёт Endpoint
/*
uint16_t inputCluster[] = {ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF};
    uint16_t outputCluster[] = {ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY};
    esp_host_zb_endpoint_t host_endpoint = {
        .endpoint = endpoint_id,
        .profileId = ESP_ZB_AF_HA_PROFILE_ID,
        .deviceId = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
        .appFlags = 0,
        .inputClusterCount = sizeof(inputCluster) / sizeof(inputCluster[0]),
        .inputClusterList = inputCluster,
        .outputClusterCount = sizeof(outputCluster) / sizeof(outputCluster[0]),
        .outputClusterList = outputCluster,
    };

    esp_host_zb_ep_create(&host_endpoint);
    */
/* 4. Создаём Endpoint*/
extern esp_err_t esp_host_zb_ep_create(esp_host_zb_endpoint_t *endpoint);
/************************************************************************/

/* 5. Стартуем zigbee на NCP (команда на  NCP) */
extern esp_err_t zb_manager_start(void); // Start the Zigbee Manager "esp_zigbee_core.h"
/************************************************************************/

esp_err_t zb_manager_ncp_host_fast_start(esp_host_zb_endpoint_t *endpoint1, esp_host_zb_endpoint_t *endpoint2, esp_host_zb_endpoint_t *endpoint3, esp_host_zb_endpoint_t *endpoint4);

esp_err_t zb_manager_ncp_host_restart_on_ncp_foulture(void); // рестарт NCP на случай если он завис
extern bool zb_manager_app_signal_handler(local_esp_zb_app_signal_t *signal_s);

extern void zb_manager_event_action_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);



#endif // ZB_MANAGER_NCP_HOST_H