/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "esp_err.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "esp_zigbee_type.h"
#include "esp_zigbee_zcl_common.h"

/** Enum of the Zigbee ZCL address mode
 * @note Defined the ZCL command of address_mode.
 * @anchor esp_zb_zcl_address_mode_t
 */
typedef enum {
    ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT =        0x0,            /*!< DstAddress and DstEndpoint not present */
    ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT  =       0x1,            /*!< 16-bit group address for DstAddress; DstEndpoint not present */
    ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT =                  0x2,            /*!< 16-bit address for DstAddress and DstEndpoint present */
    ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT =                  0x3,            /*!< 64-bit extended address for DstAddress and DstEndpoint present */
} esp_zb_zcl_address_mode_t;

/**
 * @brief ZCL command direction enum
 * @anchor esp_zb_zcl_cmd_direction
 */
typedef enum {
    ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV = 0x00U, /*!< Command for cluster server side */
    ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI = 0x01U, /*!< Command for cluster client side */
} esp_zb_zcl_cmd_direction_t;

/**
 * @brief The Zigbee ZCL basic command info
 *
 */
typedef struct esp_zb_zcl_basic_cmd_s {
    esp_zb_addr_u dst_addr_u;                   /*!< Single short address or group address */
    uint8_t  dst_endpoint;                      /*!< Destination endpoint */
    uint8_t  src_endpoint;                      /*!< Source endpoint */
} esp_zb_zcl_basic_cmd_t;

/* ZCL basic cluster */

/**
 * @brief The Zigbee ZCL basic reset factory default command struct
 *
 */
typedef struct esp_zb_zcl_basic_fact_reset_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;           /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;         /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
} esp_zb_zcl_basic_fact_reset_cmd_t;

/* ZCL on/off cluster */

/**
 * @brief The Zigbee ZCL on-off command struct
 *
 */
typedef struct esp_zb_zcl_on_off_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;           /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;         /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t  on_off_cmd_id;                         /*!< command id for the on-off cluster command */
} esp_zb_zcl_on_off_cmd_t;

/**
 * @brief The Zigbee ZCL on-off off with effect command struct
 *
 */
typedef struct esp_zb_zcl_on_off_off_with_effect_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode; /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t effect_id;                      /*!< The field specifies the fading effect to use when switching the device off */
    uint8_t effect_variant;                 /*!< The field is used to indicate which variant of the effect, indicated in the Effect Identifier field, SHOULD be triggered. */
} esp_zb_zcl_on_off_off_with_effect_cmd_t;


/**
 * @brief The Zigbee ZCL on-off on with recall global scene command struct
 *
 */
typedef struct esp_zb_zcl_on_off_on_with_recall_global_scene_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode; /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
} esp_zb_zcl_on_off_on_with_recall_global_scene_cmd_t;

/**
 * @brief The Zigbee ZCL on-off on with timed off command struct
 *
 */
typedef struct esp_zb_zcl_on_off_on_with_timed_off_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode; /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t on_off_control;                 /*!< The field contains information on how the device is to be operated */
    uint16_t on_time;                       /*!< The field specifies the length of time (in 1/10ths second) that the device is to remain "on", i.e.,
                                                 with its OnOff attribute equal to 0x01, before automatically turning "off".*/
    uint16_t off_wait_time;                 /*!< The field specifies the length of time (in 1/10ths second) that the device SHALL remain "off", i.e.,
                                                 with its OnOff attribute equal to 0x00, and guarded to prevent an on command turning the device back "on" */
} esp_zb_zcl_on_off_on_with_timed_off_cmd_t;

/* ZCL identify cluster */

/**
 * @brief The Zigbee ZCL identify command struct
 *
 */
typedef struct esp_zb_zcl_identify_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;           /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;         /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint16_t identify_time;                         /*!< identify itself for specific time */
} esp_zb_zcl_identify_cmd_t;

/**
 * @brief The Zigbee ZCL identify trigger effect command strcut
 *
 */
typedef struct esp_zb_zcl_identify_trigger_effect_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode; /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t effect_id;                      /*!< The field specifies the identify effect to use, refer to esp_zb_zcl_identify_trigger_effect_s */
    uint8_t effect_variant;                 /*!< The field is used to indicate which variant of the effect, indicated in the effect identifier field, SHOULD be triggered */
} esp_zb_zcl_identify_trigger_effect_cmd_t;

/**
 * @brief The Zigbee ZCL identify query command struct
 *
 */
typedef struct esp_zb_zcl_identify_query_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;           /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;         /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
} esp_zb_zcl_identify_query_cmd_t;

/* ZCL level cluster */

/**
 * @brief The Zigbee ZCL level move to level command struct
 *
 */
typedef struct esp_zb_zcl_move_to_level_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t level;                                      /*!< level wants to move to */
    uint16_t transition_time;                           /*!< time wants to transition tenths of a second */
} esp_zb_zcl_move_to_level_cmd_t;

/**
 * @brief The Zigbee ZCL level move command struct
 *
 */
typedef struct esp_zb_zcl_level_move_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t move_mode;                                  /*!< move mode either up or down */
    uint8_t rate;                                       /*!< move rate wants to movement in units per second */
} esp_zb_zcl_level_move_cmd_t;

/**
 * @brief The Zigbee ZCL level step command struct
 *
 */
typedef struct esp_zb_zcl_level_step_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t step_mode;                                  /*!< step mode either up or down */
    uint8_t step_size;                                  /*!< step size wants to change*/
    uint16_t transition_time;                           /*!< time wants to transition tenths of a second */
} esp_zb_zcl_level_step_cmd_t;

/**
 * @brief The Zigbee ZCL level stop command struct
 *
 */
typedef struct esp_zb_zcl_level_stop_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
} esp_zb_zcl_level_stop_cmd_t;

/* ZCL color cluster */

/**
 * @brief The Zigbee ZCL color move to hue command struct
 *
 */
typedef struct esp_zb_zcl_color_move_to_hue_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t hue;                                        /*!< current value of hue */
    uint8_t direction;                                  /*!< direction */
    uint16_t transition_time;                           /*!< time wants to transition tenths of a second */
} esp_zb_zcl_color_move_to_hue_cmd_t;

/**
 * @brief The Zigbee ZCL color move hue command struct
 *
 */
typedef struct esp_zb_zcl_color_move_hue_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t move_mode;                                  /*!< move mode */
    uint8_t rate;                                       /*!< rate */
} esp_zb_zcl_color_move_hue_cmd_t;

/**
 * @brief The Zigbee ZCL color step hue command struct
 *
 */
typedef struct esp_zb_zcl_color_step_hue_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t step_mode;                                  /*!< step mode */
    uint8_t step_size;                                  /*!< step size */
    uint16_t transition_time;                           /*!< time wants to transition tenths of a second */
} esp_zb_zcl_color_step_hue_cmd_t;

/**
 * @brief The Zigbee ZCL color move to saturation command struct
 *
 */
typedef struct esp_zb_zcl_color_move_to_saturation_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t saturation;                                 /*!< current value of saturation */
    uint16_t transition_time;                           /*!< time wants to transition tenths of a second */
} esp_zb_zcl_color_move_to_saturation_cmd_t;

/**
 * @brief The Zigbee ZCL color move saturation command struct
 *
 */
typedef struct esp_zb_zcl_color_move_saturation_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t move_mode;                                  /*!< move mode */
    uint8_t rate;                                       /*!< rate */
} esp_zb_zcl_color_move_saturation_cmd_t;

/**
 * @brief The Zigbee ZCL color step saturation command struct
 *
 */
typedef struct esp_zb_zcl_color_step_saturation_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t step_mode;                                  /*!< step mode */
    uint8_t step_size;                                  /*!< step size */
    uint16_t transition_time;                           /*!< time wants to transition tenths of a second */
} esp_zb_zcl_color_step_saturation_cmd_t;

/**
 * @brief The Zigbee ZCL color move to hue and saturation command struct
 *
 */
typedef struct esp_zb_color_move_to_hue_saturation_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t hue;                                        /*!< current value of hue */
    uint8_t saturation;                                 /*!< current value of saturation */
    uint16_t transition_time;                           /*!< time wants to transition tenths of a second */
} esp_zb_color_move_to_hue_saturation_cmd_t;

/**
 * @brief The Zigbee ZCL color move to color command struct
 *
 */
typedef struct esp_zb_zcl_color_move_to_color_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint16_t color_x;                                   /*!< current value of chromaticity value x from (0 ~ 1) to (0 ~ 65535)*/
    uint16_t color_y;                                   /*!< current value of chromaticity value y from (0 ~ 1) to (0 ~ 65535)*/
    uint16_t transition_time;                           /*!< time wants to transition tenths of a second */
} esp_zb_zcl_color_move_to_color_cmd_t;

/**
 * @brief The Zigbee ZCL color move color command struct
 *
 */
typedef struct esp_zb_zcl_color_move_color_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint16_t rate_x;                                    /*!< specifies the rate of movement in steps per second of color x */
    uint16_t rate_y;                                    /*!< specifies the rate of movement in steps per second of color y */
} esp_zb_zcl_color_move_color_cmd_t;

/**
 * @brief The Zigbee ZCL color step color command struct
 *
 */
typedef struct esp_zb_zcl_color_step_color_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    int16_t step_x;                                     /*!< specifies the change to be added to color x */
    int16_t step_y;                                     /*!< specifies the change to be added to color x */
    uint16_t transition_time;                           /*!< time wants to transition tenths of a second */
} esp_zb_zcl_color_step_color_cmd_t;

/**
 * @brief The Zigbee ZCL color stop command struct
 *
 */
typedef struct esp_zb_zcl_color_stop_move_step_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;                   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;                 /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
} esp_zb_zcl_color_stop_move_step_cmd_t;

/**
 * @brief The Zigbee ZCL color move to color temperature command struct
 *
 */
typedef struct esp_zb_zcl_color_move_to_color_temperature_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode; /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint16_t color_temperature;             /*!< The field indicates the  color-temperature value */
    uint16_t transition_time;               /*!< The time wants to transition tenths of a second */
} esp_zb_zcl_color_move_to_color_temperature_cmd_t;

/**
 * @brief The Zigbee ZCL color enhanced move to hue command struct
 *
 */
typedef struct esp_zb_zcl_color_enhanced_move_to_hue_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode; /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint16_t enhanced_hue;                  /*!< The field specifies the target enhanced hue for the lamp */
    uint8_t direction;                      /*!< The direction */
    uint16_t transition_time;               /*!< The time wants to transition tenths of a second */
} esp_zb_zcl_color_enhanced_move_to_hue_cmd_t;

/**
 * @brief The Zigbee ZCL color enhanced move hue
 *
 */
typedef struct esp_zb_zcl_color_enhanced_move_hue_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode; /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t move_mode;                      /*!< The Move Mode, If the Move Mode field is equal to 0x00 (Stop), the Rate field SHALL be ignored */
    uint16_t rate;                          /*!< The field specifies the rate of movement in steps per second */
} esp_zb_zcl_color_enhanced_move_hue_cmd_t;

/**
 * @brief The Zigbee ZCL color enhanced step hue command struct
 *
 */
typedef struct esp_zb_zcl_color_enhanced_step_hue_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode; /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t step_mode;                      /*!< The Step Mode */
    uint16_t step_size;                     /*!< The Step Size specifies the change to be added to the current value of the device’s enhanced hue.*/
    uint16_t transition_time;               /*!< The time wants to transition tenths of a second  */
} esp_zb_zcl_color_enhanced_step_hue_cmd_t;

/**
 * @brief The Zigbee ZCL color enhanced move to hue saturation command struct
 *
 */
typedef struct esp_zb_zcl_color_enhanced_move_to_hue_saturation_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode; /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint16_t enhanced_hue;                  /*!< The Enhanced Hue specifies the target extended hue for the lamp */
    uint8_t saturation;                     /*!< The value of Saturation */
    uint16_t transition_time;               /*!< The time wants to transition tenths of a second */
} esp_zb_zcl_color_enhanced_move_to_hue_saturation_cmd_t;

/**
 * @brief The Zigbee ZCL color color loop set command struct
 *
 */
typedef struct esp_zb_zcl_color_color_loop_set_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode; /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t update_flags;                   /*!< The Update Flags field specifies which color loop attributes to update before the color loop is started */
    uint8_t action;                         /*!< The Action field specifies the action to take for the color loop,
                                                 if the Update Action sub-field of the Update Flags field is set to 1. */
    uint8_t direction;                      /*!< The Direction field of the color loop set command */
    uint16_t time;                          /*!< The Time field specifies the number of seconds over which to perform a full color loop,
                                                 if the Update Time field of the Update Flags field is set to 1. */
    uint16_t start_hue;                     /*!< The field specifies the starting hue to use for the color loop if the Update Start Hue field of the Update Flags field is set to 1 */
} esp_zb_zcl_color_color_loop_set_cmd_t;

/**
 * @brief The Zigbee ZCL color move color temperature command struct
 *
 */
typedef struct esp_zb_zcl_color_move_color_temperature_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode; /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t move_mode;                      /*!< The Move Mode field of the Move Hue command, if the Move Mode field is equal to 0x00, the Rate field SHALL be ignored. */
    uint16_t rate;                          /*!< The Rate field specifies the rate of movement in steps per second */
    uint16_t color_temperature_minimum;     /*!< The field specifies a lower bound on the Color-Temperature attribute */
    uint16_t color_temperature_maximum;     /*!< The field specifies a upper bound on the Color-Temperature attribute */
} esp_zb_zcl_color_move_color_temperature_cmd_t;

/**
 * @brief The Zigbee ZCL color step color temperature command struct
 *
 */
typedef struct esp_zb_zcl_color_step_color_temperature_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode; /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint8_t move_mode;                      /*!< The Move Mode field of the Step Hue command, If the Move Mode field is equal to 0x00, the Rate field SHALL be ignored. */
    uint16_t step_size;                     /*!< The Step Size field specifies the change to be added to (or subtracted from) the current
                                                 value of the device’s color temperature.*/
    uint16_t transition_time;               /*!< The time wants to transition tenths of a second  */
    uint16_t color_temperature_minimum;     /*!< The field specifies a lower bound on the Color-Temperature attribute*/
    uint16_t color_temperature_maximum;     /*!< The field specifies a upper bound on the Color-Temperature attribute*/
} esp_zb_zcl_color_step_color_temperature_cmd_t;

/**
 * @brief The Zigbee ZCL custom cluster command struct
 *
 * @note For string data type, the first byte should be the length of string.
 *       For array, array16, array32, and long string data types, the first 2 bytes should represent the number of elements in the array.
 *
 */
typedef struct esp_zb_zcl_custom_cluster_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;                   /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;                 /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint16_t profile_id;                                    /*!< Profile id */
    uint16_t cluster_id;                                    /*!< Cluster id */
    uint16_t custom_cmd_id;                                 /*!< Custom command id */
    esp_zb_zcl_cmd_direction_t direction;                   /*!< Direction of command */
    struct {
        esp_zb_zcl_attr_type_t type;                        /*!< The type of custom data, refer to esp_zb_zcl_attr_type_t */
        void *value;                                        /*!< The value of custom data */
        uint16_t size;
    } data;                                                 /*!< The custom data */
} esp_zb_zcl_custom_cluster_cmd_t;

/**
 * @brief The Zigbee ZCL custom cluster request command struct
 *
 */
typedef esp_zb_zcl_custom_cluster_cmd_t esp_zb_zcl_custom_cluster_cmd_req_t;

/* ZCL basic cluster list command */

/**
 * @brief   Send ZCL basic reset to factory default command
 *
 * @param[in]  cmd_req  pointer to the basic command @ref esp_zb_zcl_basic_fact_reset_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_basic_factory_reset_cmd_req(esp_zb_zcl_basic_fact_reset_cmd_t *cmd_req);

/* ZCL on off cluster list command */

/**
 * @brief   Send on-off command
 *
 * @param[in]  cmd_req  pointer to the on-off command @ref esp_zb_zcl_on_off_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_on_off_cmd_req(esp_zb_zcl_on_off_cmd_t *cmd_req);

/**
 * @brief Send on-off On With Timed Off command
 *
 * @note The On With Timed Off command allows devices to be turned on for a specific duration with a guarded off
         duration so that SHOULD the device be subsequently switched off.
 * @param[in] cmd_req pointer to the on-off on with timed off command @ref esp_zb_zcl_on_off_on_with_timed_off_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_on_off_on_with_timed_off_cmd_req(esp_zb_zcl_on_off_on_with_timed_off_cmd_t *cmd_req);

/* ZCL identify cluster list command */

/**
 * @brief   Send identify command
 *
 * @param[in]  cmd_req  pointer to the identify command @ref esp_zb_zcl_identify_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_identify_cmd_req(esp_zb_zcl_identify_cmd_t *cmd_req);

/**
 * @brief Send identify trigger effect command
 *
 * @param[in] cmd_req pointer to the identify trigger variant command refer to esp_zb_zcl_identify_trigger_variant_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_identify_trigger_effect_cmd_req(esp_zb_zcl_identify_trigger_effect_cmd_t *cmd_req);

/**
 * @brief   Send identify query command
 *
 * @param[in]  cmd_req  pointer to the identify query command @ref esp_zb_zcl_identify_query_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_identify_query_cmd_req(esp_zb_zcl_identify_query_cmd_t *cmd_req);

/* ZCL level control cluster list command */

/**
 * @brief   Send move to level command
 *
 * @param[in]  cmd_req  pointer to the move to level command @ref esp_zb_zcl_move_to_level_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_level_move_to_level_cmd_req(esp_zb_zcl_move_to_level_cmd_t *cmd_req);

/**
 * @brief   Send move to level with on/off effect command
 *
 * @param[in]  cmd_req  pointer to the move to level command @ref esp_zb_zcl_move_to_level_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_level_move_to_level_with_onoff_cmd_req(esp_zb_zcl_move_to_level_cmd_t *cmd_req);

/**
 * @brief   Send move level command
 *
 * @param[in]  cmd_req  pointer to the move level command @ref esp_zb_zcl_level_move_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_level_move_cmd_req(esp_zb_zcl_level_move_cmd_t *cmd_req);

/**
 * @brief   Send move level with on/off effect command
 *
 * @param[in]  cmd_req  pointer to the move level command @ref esp_zb_zcl_level_move_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_level_move_with_onoff_cmd_req(esp_zb_zcl_level_move_cmd_t *cmd_req);

/**
 * @brief   Send step level command
 *
 * @param[in]  cmd_req  pointer to the step level command @ref esp_zb_zcl_level_step_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_level_step_cmd_req(esp_zb_zcl_level_step_cmd_t *cmd_req);

/**
 * @brief   Send step level with on/off effect command
 *
 * @param[in]  cmd_req  pointer to the step level command @ref esp_zb_zcl_level_step_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_level_step_with_onoff_cmd_req(esp_zb_zcl_level_step_cmd_t *cmd_req);

/**
 * @brief   Send stop level command
 *
 * @param[in]  cmd_req  pointer to the stop level command @ref esp_zb_zcl_level_stop_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_level_stop_cmd_req(esp_zb_zcl_level_stop_cmd_t *cmd_req);

/* ZCL color control cluster list command */

/**
 * @brief   Send color move to hue command
 *
 * @param[in]  cmd_req  pointer to the move to hue command @ref esp_zb_zcl_color_move_to_hue_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_move_to_hue_cmd_req(esp_zb_zcl_color_move_to_hue_cmd_t *cmd_req);

/**
 * @brief   Send color move hue command
 *
 * @param[in]  cmd_req  pointer to the move hue command @ref esp_zb_zcl_color_move_hue_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_move_hue_cmd_req(esp_zb_zcl_color_move_hue_cmd_t *cmd_req);

/**
 * @brief   Send color step hue command
 *
 * @param[in]  cmd_req  pointer to the step hue command @ref esp_zb_zcl_color_step_hue_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_step_hue_cmd_req(esp_zb_zcl_color_step_hue_cmd_t *cmd_req);

/**
 * @brief   Send color move to saturation command
 *
 * @param[in]  cmd_req  pointer to the move to saturation command @ref esp_zb_zcl_color_move_to_saturation_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_move_to_saturation_cmd_req(esp_zb_zcl_color_move_to_saturation_cmd_t *cmd_req);

/**
 * @brief   Send color move saturation command
 *
 * @param[in]  cmd_req  pointer to the move saturation command @ref esp_zb_zcl_color_move_saturation_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_move_saturation_cmd_req(esp_zb_zcl_color_move_saturation_cmd_t *cmd_req);

/**
 * @brief   Send color step saturation command
 *
 * @param[in]  cmd_req  pointer to the step saturation command @ref esp_zb_zcl_color_step_saturation_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_step_saturation_cmd_req(esp_zb_zcl_color_step_saturation_cmd_t *cmd_req);

/**
 * @brief   Send color move to hue and saturation command
 *
 * @param[in]  cmd_req  pointer to the move to hue and saturation command @ref esp_zb_color_move_to_hue_saturation_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_move_to_hue_and_saturation_cmd_req(esp_zb_color_move_to_hue_saturation_cmd_t *cmd_req);

/**
 * @brief   Send color move to color command
 *
 * @param[in]  cmd_req  pointer to the move to color command @ref esp_zb_zcl_color_move_to_color_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_move_to_color_cmd_req(esp_zb_zcl_color_move_to_color_cmd_t *cmd_req);

/**
 * @brief   Send color move color command
 *
 * @param[in]  cmd_req  pointer to the move color command @ref esp_zb_zcl_color_move_color_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_move_color_cmd_req(esp_zb_zcl_color_move_color_cmd_t *cmd_req);

/**
 * @brief   Send color step color command
 *
 * @param[in]  cmd_req  pointer to the step color command @ref esp_zb_zcl_color_step_color_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_step_color_cmd_req(esp_zb_zcl_color_step_color_cmd_t *cmd_req);

/**
 * @brief   Send color stop color command
 *
 * @param[in]  cmd_req  pointer to the stop color command @ref esp_zb_zcl_color_stop_move_step_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_stop_move_step_cmd_req(esp_zb_zcl_color_stop_move_step_cmd_t *cmd_req);

/**
 * @brief   Send color control move to color temperature command(0x0a)
 *
 * @param[in]  cmd_req  pointer to the move to color temperature command @ref esp_zb_zcl_color_move_to_color_temperature_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_move_to_color_temperature_cmd_req(esp_zb_zcl_color_move_to_color_temperature_cmd_t *cmd_req);

/**
 * @brief   Send color control enhanced move to hue command(0x40)
 *
 * @param[in]  cmd_req  pointer to the enhanced move to hue command @ref esp_zb_zcl_color_enhanced_move_to_hue_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_enhanced_move_to_hue_cmd_req(esp_zb_zcl_color_enhanced_move_to_hue_cmd_t *cmd_req);

/**
 * @brief   Send color control enhanced move hue command(0x41)
 *
 * @param[in]  cmd_req  pointer to the enhanced move hue command @ref esp_zb_zcl_color_enhanced_move_hue_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_enhanced_move_hue_cmd_req(esp_zb_zcl_color_enhanced_move_hue_cmd_t *cmd_req);

/**
 * @brief   Send color control enhanced step hue command(0x42)
 *
 * @param[in]  cmd_req  pointer to the enhanced step hue command @ref esp_zb_zcl_color_enhanced_step_hue_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_enhanced_step_hue_cmd_req(esp_zb_zcl_color_enhanced_step_hue_cmd_t *cmd_req);

/**
 * @brief   Send color control move to hue and saturation command(0x43)
 *
 * @param[in]  cmd_req  pointer to the enhanced move to hue saturation command @ref esp_zb_zcl_color_enhanced_move_to_hue_saturation_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_enhanced_move_to_hue_saturation_cmd_req(esp_zb_zcl_color_enhanced_move_to_hue_saturation_cmd_t *cmd_req);

/**
 * @brief   Send color control color loop set command(0x44)
 *
 * @param[in]  cmd_req  pointer to the color loop set command @ref esp_zb_zcl_color_color_loop_set_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_color_loop_set_cmd_req(esp_zb_zcl_color_color_loop_set_cmd_t *cmd_req);

/**
 * @brief   Send color control move color temperature command(0x4b)
 *
 * @param[in]  cmd_req  pointer to the move color temperature command @ref esp_zb_zcl_color_move_color_temperature_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_move_color_temperature_cmd_req(esp_zb_zcl_color_move_color_temperature_cmd_t *cmd_req);

/**
 * @brief   Send color control step color temperature command(0x4c)
 *
 * @param[in]  cmd_req  pointer to the step color temperature command @ref esp_zb_zcl_color_step_color_temperature_cmd_s
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_color_step_color_temperature_cmd_req(esp_zb_zcl_color_step_color_temperature_cmd_t *cmd_req);

/**
 * @brief   Send custom cluster command request
 *
 * @param[in]  cmd_req  pointer to the send custom cluster command request, refer to esp_zb_zcl_custom_cluster_cmd_req_t
 *
 * @return The transaction sequence number
 */
uint8_t esp_zb_zcl_custom_cluster_cmd_req(esp_zb_zcl_custom_cluster_cmd_req_t *cmd_req);

/***************************************************** ZB_MANAGER_CMD ******************************************/
/**
 * @brief The Zigbee ZCL read attribute command struct
 *
 */
typedef struct esp_zb_zcl_read_attr_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;           /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;         /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint16_t clusterID;                             /*!< Cluster ID to read */
    struct {
        uint8_t manuf_specific   : 2;               /*!< Sent as manufacturer extension with code. */
        uint8_t direction        : 1;               /*!< The command direction, refer to esp_zb_zcl_cmd_direction_t */
        uint8_t dis_defalut_resp : 1;               /*!< Disable default response for this command. */
    };
    uint16_t manuf_code;                            /*!< The manufacturer code sent with the command. */
    uint8_t attr_number;                            /*!< Number of attribute in the attr_field */
    uint16_t *attr_field;                           /*!< Attribute identifier field to read */
} esp_zb_zcl_read_attr_cmd_t;

/**
 * @brief The Zigbee zcl cluster attribute value struct
 *
 */
 typedef struct esp_zb_zcl_attribute_data_s {
    esp_zb_zcl_attr_type_t type; /*!< The type of attribute, which can refer to esp_zb_zcl_attr_type_t */
    uint16_t size;               /*!< The value size of attribute  */
    void *value;                 /*!< The value of attribute, Note that if the type is string/array, the first byte of value indicates the string length */
} ESP_ZB_PACKED_STRUCT esp_zb_zcl_attribute_data_t;

/**
 * @brief The Zigbee zcl cluster attribute struct
 *
 */
typedef struct esp_zb_zcl_attribute_s {
    uint16_t id;                      /*!< The identify of attribute */
    esp_zb_zcl_attribute_data_t data; /*!< The data of attribute */
} esp_zb_zcl_attribute_t;

/**
 * @brief The variable of Zigbee zcl read attribute response
 *
 */
typedef struct esp_zb_zcl_read_attr_resp_variable_s {
    esp_zb_zcl_status_t status;                        /*!< The field specifies the status of the read operation on this attribute */
    esp_zb_zcl_attribute_t attribute;                  /*!< The field contain the current value of this attribute, @ref esp_zb_zcl_attribute_s */
    struct esp_zb_zcl_read_attr_resp_variable_s *next; /*!< Next variable */
} esp_zb_zcl_read_attr_resp_variable_t;

/**
 * @brief The frame header of Zigbee zcl command struct
 *
 * @note frame control field:
 * |----1 bit---|---------1 bit---------|---1 bit---|----------1 bit-----------|---4 bit---|
 * | Frame type | Manufacturer specific | Direction | Disable Default Response | Reserved  |
 *
 */
 typedef struct esp_zb_zcl_frame_header_s {
    uint8_t fc;          /*!< A 8-bit Frame control */
    uint16_t manuf_code; /*!< Manufacturer code */
    uint8_t tsn;         /*!< Transaction sequence number */
    int8_t rssi;         /*!< Signal strength */
} esp_zb_zcl_frame_header_t;

/**
 * @brief The Zigbee zcl cluster command properties struct
 *
 */
 typedef struct esp_zb_zcl_command_s {
    uint8_t id;        /*!< The command id */
    uint8_t direction; /*!< The command direction */
    uint8_t is_common; /*!< The command is common type */
} esp_zb_zcl_command_t;

/**
 * @brief The Zigbee zcl command basic application information struct
 *
 */
 typedef struct esp_zb_zcl_cmd_info_s {
    esp_zb_zcl_status_t status;       /*!< The status of command, which can refer to  esp_zb_zcl_status_t */
    esp_zb_zcl_frame_header_t header; /*!< The command frame properties, which can refer to esp_zb_zcl_frame_field_t */
    esp_zb_zcl_addr_t src_address;    /*!< The struct of address contains short and ieee address, which can refer to esp_zb_zcl_addr_s */
    uint16_t dst_address;             /*!< The destination short address of command */
    uint8_t src_endpoint;             /*!< The source endpoint of command */
    uint8_t dst_endpoint;             /*!< The destination endpoint of command */
    uint16_t cluster;                 /*!< The cluster id for command */
    uint16_t profile;                 /*!< The application profile identifier*/
    esp_zb_zcl_command_t command;     /*!< The properties of command */
} esp_zb_zcl_cmd_info_t;

/**
 * @brief The Zigbee zcl read attribute response struct
 *
 */
typedef struct esp_zb_zcl_cmd_read_attr_resp_message_s {
    esp_zb_zcl_cmd_info_t info;                      /*!< The basic information of reading attribute response message that refers to esp_zb_zcl_cmd_info_t */
    esp_zb_zcl_read_attr_resp_variable_t *variables; /*!< The variable items, @ref esp_zb_zcl_read_attr_resp_variable_s */
} esp_zb_zcl_cmd_read_attr_resp_message_t;

typedef struct zb_manager_attr_s {
    uint16_t                  attr_id;
    esp_zb_zcl_attr_type_t    attr_type;
    uint8_t                   attr_len;
    void*                     attr_value;
    uint8_t                   attr_value_buf[64];    // встроенный буфер (для пула
}zb_manager_attr_t;

typedef struct zb_manager_cmd_read_attr_resp_message_s {
    esp_zb_zcl_cmd_info_t           info;
    uint8_t                         attr_count;  // 
    zb_manager_attr_t*     attr_arr;
}zb_manager_cmd_read_attr_resp_message_t;
//return the transaction sequence number
uint8_t zb_manager_zcl_read_attr_cmd_req(esp_zb_zcl_read_attr_cmd_t *cmd_req);

/**
 * @brief Очищает массив  внутри zb_manager_cmd_read_attr_resp_message_t после передачи в event, сам объект не удаляется, так как 
 * после обработки в event он будет удалён автоматически
 */
/************ !!!!!!!!!!!!!!!!! Чистит только массив с атрибутами, сама структура удаляется из обработчика так как передача происходит чере esp_event_post */
void zb_manager_free_read_attr_resp_attr_array(zb_manager_cmd_read_attr_resp_message_t* resp);


typedef struct zbstring_s {
    uint8_t len;
    char data[];
} ESP_ZB_PACKED_STRUCT
zbstring_t;

uint16_t zb_manager_get_attr_data_size(esp_zb_zcl_attr_type_t attr_type, void* attr_data);

/**
 * @brief Get the size of a Zigbee ZCL attribute value by its type
 *
 * @param attr_type The attribute type (from esp_zb_zcl_attr_type_t)
 * @return uint8_t Size in bytes, or 1 for unknown types
 */
uint16_t zb_manager_get_zcl_attr_size(esp_zb_zcl_attr_type_t attr_type);
/**
 * @brief The Zigbee zcl report attribute response struct
 *
 */

 typedef struct zb_manager_cmd_report_attr_s {
    uint16_t                  attr_id;
    esp_zb_zcl_attr_type_t    attr_type;
    uint8_t                   attr_len;
    void*                     attr_value;
}zb_manager_cmd_report_attr_t;

typedef struct zb_manager_cmd_report_attr_resp_message_s {
    esp_zb_zcl_status_t status;       /*!< The status of the report attribute response, which can refer to esp_zb_zcl_status_t */
    esp_zb_zcl_addr_t src_address;    /*!< The struct of address contains short and ieee address, which can refer to esp_zb_zcl_addr_s */
    uint8_t src_endpoint;             /*!< The endpoint id which comes from report device */
    uint8_t dst_endpoint;             /*!< The destination endpoint id */
    uint16_t cluster;  
    zb_manager_attr_t     attr;
}zb_manager_cmd_report_attr_resp_message_t;

void zb_manager_free_report_attr_resp(zb_manager_cmd_report_attr_resp_message_t* resp);


//                                        === REPORTING CONFIG ===

//typedef enum {
//    ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV = 0x00U, /*!< Command for cluster server side */
//    ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI = 0x01U, /*!< Command for cluster client side */
//} local_esp_zb_zcl_cmd_direction_t;

//typedef enum {
//    ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT   =   0x0,  /*!< DstAddress and DstEndpoint not present,
//                                                                    only for APSDE-DATA request and confirm  */
//    ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT   =   0x1,  /*!< 16-bit group address for DstAddress; DstEndpoint not present */
//    ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT             =   0x2,  /*!< 16-bit address for DstAddress and DstEndpoint present */
//    ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT             =   0x3,  /*!< 64-bit extended address for DstAddress and DstEndpoint present */
//    ESP_ZB_APS_ADDR_MODE_64_PRESENT_ENDP_NOT_PRESENT =   0x4,  /*!< 64-bit extended address for DstAddress, but DstEndpoint NOT present,
//                                                                    only for APSDE indication */
//} local_esp_zb_aps_address_mode_t;


/**
 * @brief The Zigbee zcl configure report record struct
 *
 */
typedef struct esp_zb_zcl_config_report_record_s {
    esp_zb_zcl_report_direction_t direction; /*!< Direction field specifies whether values of the attribute are to be reported, or whether reports of the
                                                  attribute are to be received.*/
    uint16_t attributeID;                    /*!< Attribute ID to report */
    union {
        struct {
            uint8_t attrType;                /*!< Attribute type to report refer to zb_zcl_common.h zcl_attr_type */
            uint16_t min_interval;           /*!< Minimum reporting interval */
            uint16_t max_interval;           /*!< Maximum reporting interval */
            void *reportable_change;         /*!< Minimum change to attribute will result in report */
        };                                   /*!< Configurations to report sender. This is presented when the direction is ESP_ZB_ZCL_REPORT_DIRECTION_SEND,
                                              *   when the receiver is configuring the sender to report the attributes.
                                              */
        struct {
            uint16_t timeout;                /*!< Timeout period */
        };                                   /*!< Configurations to report receiver. This is presented when the direction is ESP_ZB_ZCL_REPORT_DIRECTION_RECV,
                                              *   when the sender is configuring the receiver to receive to attributes report.
                                              */
    };
} esp_zb_zcl_config_report_record_t;

/**
 * @brief The Zigbee ZCL configure report command struct
 *
 */
typedef struct esp_zb_zcl_config_report_cmd_s {
    esp_zb_zcl_basic_cmd_t zcl_basic_cmd;               /*!< Basic command info */
    esp_zb_zcl_address_mode_t address_mode;             /*!< APS addressing mode constants refer to esp_zb_zcl_address_mode_t */
    uint16_t clusterID;                                 /*!< Cluster ID to report */
    struct {
        uint8_t manuf_specific   : 2;                   /*!< Sent as manufacturer extension with code. */
        uint8_t direction        : 1;                   /*!< The command direction, refer to esp_zb_zcl_cmd_direction_t */
        uint8_t dis_default_resp : 1;                   /*!< Disable default response for this command. */
    };
    uint16_t manuf_code;                                /*!< The manufacturer code sent with the command. */
    uint16_t record_number;                             /*!< Number of report configuration record in the record_field */
    esp_zb_zcl_config_report_record_t *record_field;    /*!< Report configuration records, @ref esp_zb_zcl_config_report_record_s */
} esp_zb_zcl_config_report_cmd_t;

/** 
 * @brief Структура для хранения параметров запроса на configure_report через event_post
*/
typedef struct {
    uint16_t short_addr;
    uint8_t src_endpoint;
    uint16_t cluster_id;
} delayed_configure_report_req_t;


/**
 * @brief Send attribute reporting configuration to a device
 */
esp_err_t zb_manager_reporting_config_req(esp_zb_zcl_config_report_cmd_t *cmd_req);

esp_err_t zb_manager_configure_reporting_temperature(uint16_t short_addr, uint8_t endpoint);

esp_err_t zb_manager_configure_reporting_humidity(uint16_t short_addr, uint8_t endpoint);

esp_err_t zb_manager_configure_reporting_power(uint16_t short_addr, uint8_t endpoint);

esp_err_t zb_manager_configure_reporting_onoff(uint16_t short_addr, uint8_t endpoint);
    
// Samples
/*zb_manager_report_attr_cfg_t power_attrs[] = {
    {
        .attr_id = ESP_ZB_ZCL_ATTR_POWER_CONFIGURATION_BATTERY_VOLTAGE_ID,
        .attr_type = ESP_ZB_ZCL_DATA_TYPE_U8,
        .min_interval = 300,
        .max_interval = 3600,
        .rcv_size = 1,
        .reportable_change = {1}  // 0.1V
    },
    {
        .attr_id = ESP_ZB_ZCL_ATTR_POWER_CONFIGURATION_BATTERY_PERCENTAGE_REMAINING_ID,
        .attr_type = ESP_ZB_ZCL_DATA_TYPE_U8,
        .min_interval = 300,
        .max_interval = 3600,
        .rcv_size = 1,
        .reportable_change = {2}  // 1% = 2 × 0.5%
    }
};

zb_manager_send_reporting(0x7d74, 1, 1, ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, power_attrs, 2);

zb_manager_report_attr_cfg_t temp_attrs[] = {
    {
        .attr_id = ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MEASURED_VALUE_ID,
        .attr_type = ESP_ZB_ZCL_DATA_TYPE_S16,
        .min_interval = 60,
        .max_interval = 600,
        .rcv_size = 2,
        .reportable_change = {0x0a, 0x00}  // 1.0°C = 100 (0x64) → 0x64 = {0x64, 0x00}?
        // 0.01°C units → 100 = 1.00°C
        // 10 = 0.1°C
        // .reportable_change = {10, 0} → 10d = 0.10°C
    }
};

zb_manager_send_reporting(0x7d74, 1, 1, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, temp_attrs, 1);

zb_manager_report_attr_cfg_t onoff_attrs[] = {
    {
        .attr_id = ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
        .attr_type = ESP_ZB_ZCL_DATA_TYPE_BOOL,
        .min_interval = 1,
        .max_interval = 300,
        .rcv_size = 1,
        .reportable_change = {1}  // любое изменение
    }
};

zb_manager_send_reporting(0x7d74, 1, 1, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, onoff_attrs, 1);
*/

/**
 * @brief Структура для одного атрибута в ответе Configure Reporting
 */
typedef struct {
    uint8_t  direction;       /*!< Направление: 0 - от устройства, 1 - к устройству */
    uint16_t attr_id;         /*!< ID атрибута */
    uint8_t  status;          /*!< Статус настройки (ESP_ZB_ZCL_STATUS_SUCCESS и др.) */
} zb_manager_report_config_attr_status_t;

/**
 * @brief Сообщение для event loop: ответ на Configure Reporting
 */
typedef struct {
    esp_zb_zcl_status_t status;       /*!< Общий статус команды */
    uint16_t            cluster;      /*!< Кластер, к которому относится команда */
    uint16_t            short_addr;   /*!< Адрес устройства */
    uint8_t             attr_count;   /*!< Количество атрибутов */
    zb_manager_report_config_attr_status_t *attr_list; /*!< Массив статусов */
} zb_manager_cmd_report_config_resp_message_t;

/**
 * @brief Функция освобождения памяти
 */
void zb_manager_free_report_config_resp(zb_manager_cmd_report_config_resp_message_t *msg);

/**************************** Custom Cluster Report Event */
typedef struct {
    uint16_t short_addr;
    uint8_t  src_endpoint;
    uint8_t  dst_endpoint;
    uint16_t cluster_id;
    uint8_t  command_id;
    uint8_t  manuf_specific;
    uint16_t manuf_code;
    uint8_t  seq_num;
    int8_t   rssi;
    uint16_t data_len;
    uint8_t  *data;  // выделен отдельно
} zb_manager_custom_cluster_report_message_t;

void zb_manager_free_custom_cluster_report_message(zb_manager_custom_cluster_report_message_t *msg);

#ifdef __cplusplus
}
#endif
