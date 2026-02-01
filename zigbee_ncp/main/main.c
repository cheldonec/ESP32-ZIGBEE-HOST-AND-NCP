/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ncp_zb_manager_main.h"
#include "zb_manager.h"
//#include "esp_ncp_frame.h"

static const char *TAG = "MAIN_MODULE";



void app_main(void)
{
    //ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGW(TAG, "RAM control on start %lu", esp_get_free_heap_size());
    ESP_ERROR_CHECK(esp_ncp_init(NCP_HOST_CONNECTION_MODE_UART));
    ESP_ERROR_CHECK(esp_ncp_start());
    ESP_LOGW(TAG, "RAM control after NCP Start %lu", esp_get_free_heap_size());

    //esp_zb_set_log_level(ESP_ZB_LOG_LEVEL_DEBUG);
    /**************** ZIGBEE ****************/
    /*zb_manager_init();
    // EP1
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
    
    // EP2
    uint16_t inputClusterEP2[] = {0x0001, 0x0006};// 
    uint16_t outputClusterEP2[] = {0x0006};// 
    custom_zb_endpoint_add_str_t endpoint2 = {
        .inputClusterCount = sizeof(inputClusterEP2) / sizeof(inputClusterEP2[0]),
        .inputClusterList = inputClusterEP2,
        .outputClusterCount = sizeof(outputClusterEP2) / sizeof(outputClusterEP2[0]),
        .outputClusterList = outputClusterEP2,
    };
    esp_zb_endpoint_config_t endpoint2_config = LOCAL_REMOTE_ENDPOINT2_CONFIG();
    zb_manager_obj.endpoints_func.generate_endpoint_func(&endpoint2,endpoint2_config);
    
    //zb_manager_register_match_desc_cb(match_desc_callback);
    //zb_manager_register_ieee_addr_req_cb(ieee_addr_callback);
    //zb_manager_register_print_log_to_screen_cb(print_log_to_screen_cb);

    zb_manager_start();*/
    /*zb_manager_init END */
}
