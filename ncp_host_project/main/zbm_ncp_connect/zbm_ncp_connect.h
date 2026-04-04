#ifndef ZBM_NCP_CONNECT_H

#define ZBM_NCP_CONNECT_H
#include "freertos/FreeRTOS.h"
#include "ncp_host.h"
#include "zbm_zigbee_structures.h"



typedef enum {
    NOT_INIT = 0,
    STARTED = 1,
    FOULTED = 2,
    RESTARTING = 3,
    RESTARTED = 3,
    WORKING = 4,
}zigbee_ncp_module_state_e;

extern esp_zb_ieee_addr_t CoordinatorIeeeAdr; 

extern zigbee_ncp_module_state_e zigbee_ncp_module_state;

extern bool g_zigbee_restarting; // для рестарта NCP и всего zigbee после сбоя NCP
extern bool isZigbeeNetworkOpened;

extern TaskHandle_t xZB_TaskHandle; // для управления xZB_Handle (esp_zb_task)

extern QueueHandle_t                ncp_host_output_queue;           /*!< The queue handler for wait response */
extern QueueHandle_t                ncp_host_notify_queue;           /*!< The queue handler for wait notification */
extern SemaphoreHandle_t            ncp_host_lock_semaphore;

#define ZIGBEE_STACK_SIZE 8 * 1024

#define ZIGBEE_TASK_PRIORITY 10

//========================================== FUNCTIONS ===================================
esp_err_t zbm_ncp_connect_start(void);

esp_err_t zbm_host_reinit_on_ncp_foulture(void);  

//====================================== #defines and enums ==============================


typedef enum {
    RADIO_MODE_UART_NCP   = 0x0,                        /*!< UART connection to a 15.4 Network Co-processor */
} esp_zb_radio_mode_t;

typedef struct {
    esp_zb_radio_mode_t             radio_mode;         /*!< The radio mode */
} esp_zb_ncp_config_t;

typedef struct {
    esp_host_connection_mode_t      host_mode;         /*!< The host connection mode */
} esp_zb_host_config_t;

typedef struct {
    esp_zb_ncp_config_t              radio_config;      /*!< The radio configuration */
    esp_zb_host_config_t             host_config;       /*!< The host connection configuration */
} esp_zb_platform_config_t;

#define DEVICE_TYPE_COORDINATOR         0x0
#define INSTALLCODE_POLICY_ENABLE       false


#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = RADIO_MODE_UART_NCP,                      \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_mode = HOST_CONNECTION_MODE_UART,                 \
    }
#endif
