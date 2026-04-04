// main/temp/rules/zb_manager_rules.h
#ifndef ZB_MANAGER_RULES_H
#define ZB_MANAGER_RULES_H

#include <stdint.h>
#include <stdbool.h>
#include "cJSON.h"
#include <time.h>
#include "esp_err.h"
#include "zbm_clusters_type.h"
#include "zbm_attr_types.h"
//#include "esp_zigbee_zcl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Максимальное количество правил
 */
#define ZB_RULE_MAX_COUNT         32

/**
 * @brief Максимальное количество триггеров и действий
 */
#define ZB_RULE_MAX_TRIGGERS      8
#define ZB_RULE_MAX_ACTIONS       4

/**
 * @brief Количество виртуальных переменных
 */
#define ZB_VIRTUAL_VAR_COUNT      32

/**
 * @brief Глобальные виртуальные переменные системы
 */
extern uint8_t virtual_var[ZB_VIRTUAL_VAR_COUNT];

// Удобные макросы
#define SYS_VAR(i) ((uint8_t)(i))           // 0–15
#define USR_VAR(i) ((uint8_t)(16 + (i)))     // 16–31
#define IS_VALID_VAR(idx) ((idx) < ZB_VIRTUAL_VAR_COUNT)

/**
 * @brief Семантические индексы виртуальных переменных
 */
typedef enum {
    VAR_MORNING_MODE,
    VAR_HOME_OCCUPIED,
    VAR_AWAY_MODE,
    VAR_HOLIDAY_MODE,
    VAR_NIGHT_MODE,
    VAR_GARAGE_OPEN,
    VAR_ALARM_ARMED,
    VAR_BUTTON_CLICK_COUNT,
    VAR_LIGHT_OVERRIDE,
    VAR_SYSTEM_READY,
    VAR_WINDOW_OPEN,
    VAR_BOILER_ACTIVE,
    VAR_SUNSET_REACHED,
    VAR_TV_MODE,
    VAR_DOOR_OPENED_TODAY,
    VAR_LAST_TRIGGER,

    // Допускается до ZB_VIRTUAL_VAR_COUNT-1
} zb_virtual_var_index_t;

/**
 * @brief Типы триггеров
 */
typedef enum {
    ZB_RULE_TRIGGER_DEVICE_STATE,
    ZB_RULE_TRIGGER_TIME_RANGE,
    ZB_RULE_TRIGGER_ILLUMINANCE,
    ZB_RULE_TRIGGER_MOTION,
    ZB_RULE_TRIGGER_SUNRISE_SUNSET,
    ZB_RULE_TRIGGER_BUTTON_PRESS,
    ZB_RULE_TRIGGER_DEVICE_UNAVAILABLE,
    ZB_RULE_TRIGGER_VIRTUAL_VAR,
} zb_rule_trigger_type_t;

/**
 * @brief Типы условий
 */
typedef enum {
    ZB_RULE_COND_EQ,  // ==
    ZB_RULE_COND_NE,  // !=
    ZB_RULE_COND_GT,  // >
    ZB_RULE_COND_LT,  // <
    ZB_RULE_COND_GTE, // >=
    ZB_RULE_COND_LTE, // <=
} zb_rule_condition_t;

/**
 * @brief Типы действий
 */
typedef enum {
    ZB_RULE_ACTION_DEVICE_CMD,
    ZB_RULE_ACTION_RUN_SCENE,
    ZB_RULE_ACTION_HTTP_REQUEST,
    ZB_RULE_ACTION_DELAY,
    ZB_RULE_ACTION_SET_VIRTUAL_VAR,
    ZB_RULE_ACTION_INC_VIRTUAL_VAR,
    ZB_RULE_ACTION_DEC_VIRTUAL_VAR,
    ZB_RULE_ACTION_TOGGLE_VIRTUAL_VAR,
} zb_rule_action_type_t;

/**
 * @brief Описание действия
 */
typedef struct {
    zb_rule_action_type_t type;
    union {
        struct {
            uint16_t short_addr;
            uint8_t endpoint;
            uint8_t cmd_id;
        } device_cmd;

        struct {
            int scene_id;
        } run_scene;

        struct {
            char url[128];
            char method[8]; // GET, POST
        } http_request;

        struct {
            uint32_t seconds;
        } delay;

        struct {
            uint8_t var_index;
            uint8_t value;
        } set_virtual_var;
    } data;
} zb_rule_action_t;

/**
 * @brief Описание триггера
 */
typedef struct {
    zb_rule_trigger_type_t type;
    union {
        struct {
            uint16_t short_addr;
            uint8_t endpoint_id;
            uint16_t cluster_id;
            uint16_t attr_id;
            zb_rule_condition_t cond;
            zbm_attr_data_types_t expected_type;
            uint8_t expected_len;
            void* p_expected_value;   // указатель на значение (выделяется отдельно)
        } device_state;

        struct {
            char from[6];             // "HH:MM"
            char to[6];               // "HH:MM"
            uint8_t days_of_week;     // битовая маска: Пн=0x01, ..., Вс=0x40
            uint32_t delay_sec;       // задержка после входа в интервал
        } time_range;

        struct {
            uint16_t short_addr;
            uint8_t endpoint_id;
            uint16_t threshold;       // in lux
            zb_rule_condition_t cond;
        } illuminance;

        struct {
            uint16_t short_addr;
            uint8_t endpoint_id;
            uint16_t timeout_sec;
        } motion;

        struct {
            char event[16];           // "sunrise", "sunset"
            int offset_min;           // ± minutes
        } sun_event;

        struct {
            uint8_t var_index;
            zb_rule_condition_t cond;
            uint8_t value;
        } virtual_var;

        struct {
            uint16_t short_addr;
            uint16_t cluster_id;      // ✅ по ID
            uint16_t timeout_ms;
        } device_unavailable;
    } data;
} zb_rule_trigger_t;

/**
 * @brief Логика срабатывания триггеров
 */
typedef enum {
    ZB_RULE_TRIGGER_LOGIC_ANY,  // OR — хотя бы один
    ZB_RULE_TRIGGER_LOGIC_ALL,  // AND — все
} zb_rule_trigger_logic_t;

/**
 * @brief Полное правило автоматизации
 */
typedef struct zb_rule_s {
    char id[37];                  // UUID or custom ID
    char name[64];
    char module[32];
    uint8_t priority;             // 1 (high) to 5 (low)
    bool enabled;
    zb_rule_trigger_logic_t trigger_logic;

    zb_rule_trigger_t triggers[ZB_RULE_MAX_TRIGGERS];
    uint8_t trigger_count;

    zb_rule_action_t actions[ZB_RULE_MAX_ACTIONS];
    uint8_t action_count;
} zb_rule_t;

/**
 * @brief Глобальный массив правил (указатели)
 */
extern zb_rule_t** rules_array;
extern uint8_t rules_count;

/**
 * @brief Отложенные действия
 */
typedef struct {
    char rule_id[32];
    time_t fire_at;
} delayed_action_t;

extern delayed_action_t delayed_actions[8];
extern uint8_t delayed_count;

// ===================================================================
//                         API: Правила
// ===================================================================

/**
 * @brief Инициализация движка правил
 */
void zb_rule_engine_init(void);

/**
 * @brief Добавить правило (копирует содержимое)
 */
bool zb_rule_engine_add_rule(const zb_rule_t* rule);

/**
 * @brief Обновить правило по ID
 */
bool zb_rule_engine_update_rule(const char* rule_id, const zb_rule_t* updated_rule);

/**
 * @brief Удалить правило по ID
 */
bool zb_rule_engine_remove_rule(const char* rule_id);

/**
 * @brief Удалить все правила
 */
bool zb_rule_engine_remove_all_rules(void);

/**
 * @brief Получить правило по ID
 */
const zb_rule_t* zb_rule_engine_get_rule(const char* rule_id);

/**
 * @brief Обработать событие (например, изменение состояния устройства)
 */
void zb_rule_engine_process_event(cJSON* event);

/**
 * @brief Запустить правило вручную
 */
bool zb_automation_run_rule_now(const char* rule_id);

// ===================================================================
//                         API: Триггеры событий
// ===================================================================

/**
 * @brief Вызвать при изменении атрибута устройства
 */
void zb_rule_trigger_state_update(
    uint16_t short_addr,
    uint16_t cluster_id,
    uint16_t attr_id,
    void* data,
    uint8_t data_len,
    zbm_attr_data_types_t attr_type
);

/**
 * @brief Вызвать при недоступности устройства
 */
void zb_rule_trigger_device_unavailable(uint16_t short_addr, uint16_t cluster_id);

/**
 * @brief Проверить временные триггеры (вызывать раз в секунду или минуту)
 */
void check_time_triggers(void);

/**
 * @brief Планирование отложенного действия
 */
void schedule_delayed_action(const char* rule_id, uint32_t delay_sec);

/**
 * @brief Обработка отложенных действий
 */
void process_delayed_actions(void);

// ===================================================================
//                         API: Виртуальные переменные
// ===================================================================

/**
 * @brief Установить значение переменной
 */
void zb_rule_set_var(int idx, uint8_t value);

/**
 * @brief Увеличить переменную
 */
void zb_rule_inc_var(int idx);

/**
 * @brief Уменьшить переменную
 */
void zb_rule_dec_var(int idx);

/**
 * @brief Переключить переменную
 */
void zb_rule_toggle_var(int idx);

// ===================================================================
//                         API: JSON (только для HTTP)
// ===================================================================

/**
 * @brief Преобразовать правило в JSON (для API)
 */
cJSON* rule_to_json(const zb_rule_t* rule);

/**
 * @brief Преобразовать JSON в правило (только для API)
 */
bool rule_from_json(cJSON* json, zb_rule_t* out_rule);

/**
 * @brief Уведомить WebSocket о изменении правил
 */
void ws_notify_rules_update(void);

/**
 * @brief Уведомить WebSocket о изменении переменных
 */
void ws_notify_virtual_vars_update(void);

#ifdef __cplusplus
}
#endif

#endif // ZB_MANAGER_RULES_H