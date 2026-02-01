#ifndef ZB_MANAGER_CONFIG_H
#define ZB_MANAGER_CONFIG_H
#include "esp_zigbee_core.h"
#if CONFIG_OPENTHREAD_SPINEL_ONLY
#include "esp_radio_spinel.h"
#endif

/* Zigbee configuration */
#define MAX_CHILDREN                    64          /* the max amount of connected devices */
#define INSTALLCODE_POLICY_ENABLE       false    /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                   3000    /* 3000 millisecond */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     (1l << 25) /* Zigbee primary channel mask use in the example */
#define ESP_ZB_SECONDARY_CHANNEL_MASK   (1l << 13)  /* Zigbee primary channel mask use in the example */

#define ESP_ZB_ZED_CONFIG()                                         \
    {                                                               \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,           \
        .nwk_cfg.zed_cfg = {                                        \
            .ed_timeout = ED_AGING_TIMEOUT,                         \
            .keep_alive = ED_KEEP_ALIVE,                            \
        },                                                          \
    }

#define ESP_ZB_ZC_CONFIG()                                                              \
    {                                                                                   \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,                                  \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,                               \
        .nwk_cfg.zczr_cfg = {                                                           \
            .max_children = MAX_CHILDREN,                                               \
        },                                                                              \
    }

#define ESP_ZB_ZCR_CONFIG()                                                             \
    {                                                                                   \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,                                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,                               \
        .nwk_cfg.zczr_cfg = {                                                           \
            .max_children = MAX_CHILDREN,                                               \
        },                                                                              \
    }

#ifdef CONFIG_ZB_RADIO_NATIVE
#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                     \
    }
#else
#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_UART_RCP,                   \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_UART_RCP,                   \
            .radio_uart_config = {                              \
            .port = 1,                                          \
            .uart_config =                                      \
                {                                               \
                    .baud_rate = 460800,                        \
                    .data_bits = UART_DATA_8_BITS,              \
                    .parity = UART_PARITY_DISABLE,              \
                    .stop_bits = UART_STOP_BITS_1,              \
                    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,      \
                    .rx_flow_ctrl_thresh = 0,                   \
                    .source_clk = UART_SCLK_DEFAULT,            \
                },                                              \
            .rx_pin = CONFIG_PIN_TO_RCP_TX,                    \
            .tx_pin = CONFIG_PIN_TO_RCP_RX,                    \
        },                                                      \
    }

#define ESP_ZB_RCP_UPDATE_CONFIG()                                                                                                  \
    {                                                                                                                               \
        .rcp_type = RCP_TYPE_ESP32H2_UART, .uart_rx_pin = CONFIG_PIN_TO_RCP_TX, .uart_tx_pin = CONFIG_PIN_TO_RCP_RX,              \
        .uart_port = 1, .uart_baudrate = 115200, .reset_pin = CONFIG_PIN_TO_RCP_RESET, .boot_pin = CONFIG_PIN_TO_RCP_BOOT,   \
        .update_baudrate = 460800, .firmware_dir = "/rcp_fw/ot_rcp", .target_chip = ESP32H2_CHIP,                                   \
    }
#define RCP_VERSION_MAX_SIZE            80
#endif
#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,   \
    }

    #endif