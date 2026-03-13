#ifndef ZBM_CUSTOM_COMMANDS_H

#define ZBM_CUSTOM_COMMANDS_H

#include "stdint.h"
/**
 * @brief Структура для описания кастомной команды
 */
typedef struct cluster_custom_command_s {
    uint8_t                     cmd_id;
    char                        cmd_friendly_name[64];
    uint8_t                     cmd_payload_len;
    uint8_t                     cmd_payload[64];      // buffer for payload
} cluster_custom_command_t;

#endif