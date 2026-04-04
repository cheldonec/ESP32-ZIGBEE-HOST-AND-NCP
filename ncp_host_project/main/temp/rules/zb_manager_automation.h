#ifndef ZB_MANAGER_AUTOMATION_H
#define ZB_MANAGER_AUTOMATION_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Идентификаторы команд для автоматизации
 * Формат: ZB_AUTO_CL_<CLUSTER>_<CMD>
 */
typedef enum {
    ZB_AUTO_CL_ON_OFF_CMD_OFF                    = 0x00,
    ZB_AUTO_CL_ON_OFF_CMD_ON                     = 0x01,
    ZB_AUTO_CL_ON_OFF_CMD_TOGGLE                 = 0x02,
    ZB_AUTO_CL_ON_OFF_CMD_OFF_WITH_EFFECT        = 0x40,
    ZB_AUTO_CL_ON_OFF_CMD_ON_WITH_RECALL_GLOBAL_SCENE = 0x41,
    ZB_AUTO_CL_ON_OFF_CMD_ON_WITH_TIMED_OFF      = 0x42,

    // Identify
    ZB_AUTO_CL_IDENTIFY_CMD_IDENTIFY             = 0x00,

    // Level Control
    ZB_AUTO_CL_LEVEL_CMD_MOVE_TO_LEVEL           = 0x04,
    ZB_AUTO_CL_LEVEL_CMD_MOVE                    = 0x01,
    ZB_AUTO_CL_LEVEL_CMD_STEP                    = 0x02,
    ZB_AUTO_CL_LEVEL_CMD_STOP                    = 0x03,

    ZB_AUTO_CMD_INVALID = 255,
} zb_automation_cmd_id_t;

/**
 * @brief Типы параметров команд
 */
typedef enum {
    ZB_AUTO_PARAM_NONE,
    ZB_AUTO_PARAM_UINT8,
    ZB_AUTO_PARAM_UINT16,
    ZB_AUTO_PARAM_STRUCT_TIMED_OFF,
    ZB_AUTO_PARAM_STRUCT_IDENTIFY,
    ZB_AUTO_PARAM_STRUCT_MOVE_TO_LEVEL,
} zb_automation_param_type_t;

/**
 * @brief Структура параметров: ON_WITH_TIMED_OFF
 */
typedef struct {
    uint16_t on_time;         // in 0.1 seconds
    uint16_t off_wait_time;   // in 0.1 seconds
    uint8_t  on_off_control;
} zb_auto_params_timed_off_t;

/**
 * @brief Структура параметров: IDENTIFY
 */
typedef struct {
    uint16_t identify_time;   // in seconds
} zb_auto_params_identify_t;

/**
 * @brief Структура параметров: MOVE_TO_LEVEL
 */
typedef struct {
    uint8_t level;
    uint16_t transition_time; // in 0.1 seconds
} zb_auto_params_move_to_level_t;

/**
 * @brief Объединение всех типов параметров
 */
typedef union {
    uint8_t u8;
    uint16_t u16;
    zb_auto_params_timed_off_t timed_off;
    zb_auto_params_identify_t identify;
    zb_auto_params_move_to_level_t move_to_level;
} zb_automation_param_value_t;

/**
 * @brief Описание одной автоматизируемой команды
 */
typedef struct {
    zb_automation_cmd_id_t cmd_id;
    const char* name;               // "ON", "TOGGLE"
    const char* friendly_name;      // "Включить", "Переключить"
    uint16_t cluster_id;            // например, 0x0006
    zb_automation_param_type_t param_type;
    size_t param_size;              // sizeof(...), или 0
    bool is_client_cmd;             // true если команда от клиента (например, Identify)
} zb_automation_command_t;

/**
 * @brief Полная команда с адресацией
 */
typedef struct {
    uint16_t short_addr;
    uint8_t endpoint_id;
    zb_automation_cmd_id_t cmd_id;
    bool has_params;
    zb_automation_param_value_t params;
} zb_automation_request_t;

// ===================================================================
//                          API
// ===================================================================

/**
 * @brief Получить список команд для заданного типа устройства
 * @param device_type строка, например "on_off_light"
 * @param count указатель, куда запишется количество команд
 * @return массив команд или NULL
 */
const zb_automation_command_t* zb_automation_get_commands_for_device_type(
    const char* device_type,
    size_t* count);

/**
 * @brief Получить команду по её ID
 */
const zb_automation_command_t* zb_automation_get_command_by_id(zb_automation_cmd_id_t cmd_id);

/**
 * @brief Отправить команду на устройство
 */
esp_err_t zb_automation_send_command(const zb_automation_request_t* req);

/**
 * @brief Преобразовать команду в JSON (для веб-интерфейса)
 * @param cmd Указатель на команду
 * @return cJSON* объект или NULL
 */
cJSON* zb_automation_command_to_json(const zb_automation_command_t* cmd);

/**
 * @brief Преобразовать запрос в JSON (для логирования/отладки)
 */
cJSON* zb_automation_request_to_json(const zb_automation_request_t* req);

/**
 * @brief Распарсить команду из JSON (например, от веба)
 * @param json cJSON объект
 * @param req Заполняемая структура
 * @return ESP_OK или ошибка
 */
esp_err_t zb_automation_request_from_json(cJSON* json, zb_automation_request_t* req);

/**
 * @brief Получить все доступные команды в виде JSON массива
 * @param device_type тип устройства
 * @return cJSON* массив команд
 */
cJSON* zb_automation_get_commands_json(const char* device_type);

#ifdef __cplusplus
}
#endif

#endif // ZB_MANAGER_AUTOMATION_H
