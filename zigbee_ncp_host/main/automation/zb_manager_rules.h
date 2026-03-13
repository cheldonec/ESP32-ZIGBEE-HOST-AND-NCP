// main/rules/zb_manager_rules.h
#ifndef ZB_MANAGER_RULES_H
#define ZB_MANAGER_RULES_H

#include <stdint.h>
#include <stdbool.h>
#include "cJSON.h"
#include "esp_err.h"
#include "esp_zigbee_zcl_common.h"
#include "zb_manager_clusters.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Максимальное количество правил
 */
#define ZB_RULE_MAX_COUNT 32

/**
 * @brief Максимальное количество триггеров и действий
 */
#define ZB_RULE_MAX_TRIGGERS 4
#define ZB_RULE_MAX_ACTIONS  4

/**
 * @brief Количество виртуальных переменных
 */
#define ZB_VIRTUAL_VAR_COUNT 32
/**
 * @brief Глобальные виртуальные переменные системы
 * Используются для хранения состояния: "дома/нет", "ночь", "отпуск" и т.д.
 */
extern uint8_t virtual_var[ZB_VIRTUAL_VAR_COUNT];

// Макросы для удобства
#define SYS_VAR(i) ((uint8_t)(i))                    // 0–15
#define USR_VAR(i) ((uint8_t)(16 + (i)))             // 16–31
#define IS_VALID_VAR(idx) ((idx) < ZB_VIRTUAL_VAR_COUNT)

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

/**
 * @brief Семантические индексы виртуальных переменных
 * Позволяют использовать понятные имена вместо цифр
 */
typedef enum {
    VAR_MORNING_MODE,           // 0 — 🌅 утро началось
    VAR_HOME_OCCUPIED,          // 1 — 👨‍👩‍👧‍👦 кто-то дома
    VAR_AWAY_MODE,              // 2 — 🏠❌ режим "дома никого"
    VAR_HOLIDAY_MODE,           // 3 — 🎉 праздник/выходной
    VAR_NIGHT_MODE,             // 4 — 🌙 тёмное время суток
    VAR_GARAGE_OPEN,            // 5 — 🚗🚪 ворота открыты
    VAR_ALARM_ARMED,            // 6 — 🔔 сигнализация активна
    VAR_BUTTON_CLICK_COUNT,     // 7 — 🔢 счётчик кликов кнопки
    VAR_LIGHT_OVERRIDE,         // 8 — ✋ ручное управление светом
    VAR_SYSTEM_READY,           // 9 — ✅ все устройства онлайн
    VAR_WINDOW_OPEN,            // 10 — 🪟 окно открыто
    VAR_BOILER_ACTIVE,          // 11 — 🔥 котёл работает
    VAR_SUNSET_REACHED,         // 12 — 🌇 закат прошёл
    VAR_TV_MODE,                // 13 — 📺 режим просмотра ТВ
    VAR_DOOR_OPENED_TODAY,      // 14 — 🚪 дверь открывалась сегодня
    VAR_LAST_TRIGGER,           // 15 — 🔄 ID последнего сработавшего правила

    // ВАЖНО: Не более ZB_VIRTUAL_VAR_COUNT (32)
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
            // params могут быть добавлены при необходимости
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
            uint8_t var_index;           // 0, 1, 2
            uint8_t value;               // значение для записи
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
            char cluster_type[32];     // "on_off", "illuminance", "temperature"
            zb_rule_condition_t cond;
            double value;              // порог (для сравнения)
        } device_state;

        struct {
            char from[6];           // "08:00"
            char to[6];             // "09:00"
            uint8_t days_of_week;   // битовая маска: 0b00101010 = Пн, Ср, Пт
            uint32_t delay_sec;     // задержка в секундах после входа в интервал
        } time_range;

        struct {
            uint16_t short_addr;
            uint8_t endpoint_id;
            uint16_t threshold; // in lux
            zb_rule_condition_t cond;
        } illuminance;

        struct {
            uint16_t short_addr;
            uint8_t endpoint_id;
            uint16_t timeout_sec;
        } motion;

        struct {
            char event[16]; // "sunrise", "sunset"
            int offset_min; // ± minutes
        } sun_event;

        struct {
            uint8_t var_index;           // 0, 1, 2
            zb_rule_condition_t cond;
            uint8_t value;               // порог сравнения
        } virtual_var;

        /**
         * @brief Триггер: устройство недоступно
         */
        struct {
            uint16_t short_addr;      // адрес устройства (0x1234)
            char cluster_type[32];    // тип кластера: "temperature", "on_off", и т.д.
            uint16_t timeout_ms;      // порог времени без ответа в миллисекундах (опционально)
        } device_unavailable;

    } data;
} zb_rule_trigger_t;

typedef enum {
    ZB_RULE_TRIGGER_LOGIC_ANY,  // any = OR
    ZB_RULE_TRIGGER_LOGIC_ALL   // all = AND
} zb_rule_trigger_logic_t;

/**
 * @brief Полное правило автоматизации
 */
typedef struct {
    char id[37];           // уникальный ID правила
    char name[64];         // читаемое имя
    char module[32];       // логическая группа: "light", "security", etc.
    uint8_t priority;      // приоритет: 1 (высший) ... 5 (низший)
    bool enabled;          // включено ли правило
    zb_rule_trigger_logic_t trigger_logic;
    zb_rule_trigger_t triggers[ZB_RULE_MAX_TRIGGERS];   // максимум 8 триггеров
    uint8_t trigger_count;           // текущее количество

    zb_rule_action_t actions[ZB_RULE_MAX_ACTIONS];     // максимум 4 действия
    uint8_t action_count;            // текущее количество
} zb_rule_t;

extern uint8_t rules_count;
//extern zb_rule_t rules[ZB_RULE_MAX_COUNT];
extern zb_rule_t** rules_array ;

typedef struct {
    char rule_id[32];
    time_t fire_at;
} delayed_action_t;

extern delayed_action_t delayed_actions[8];
extern uint8_t delayed_count;

void schedule_delayed_action(const char* rule_id, uint32_t delay_sec);
void process_delayed_actions(void);

/**
 * @brief Инициализация движка правил: загружает из SPIFFS
 */
void zb_rule_engine_init(void);

/**
 * @brief Сохранить правила в SPIFFS
 * @return ESP_OK или ошибка
 */
esp_err_t zb_rule_engine_save_to_spiffs(void);

/**
 * @brief Загрузить правила из SPIFFS
 * @return ESP_OK или ESP_ERR_NOT_FOUND (если файл не существует)
 */
esp_err_t zb_rule_engine_load_from_spiffs(void);

/**
 * @brief Добавить правило + сохранить на диск
 */
bool zb_rule_engine_add_rule(const zb_rule_t* rule);

bool zb_rule_engine_update_rule(const char* rule_id, const zb_rule_t* updated_rule);
/**
 * @brief Удалить правило по ID + сохранить
 */
bool zb_rule_engine_remove_rule(const char* rule_id);

/**
 * @brief Удалить все правила
 */
bool zb_rule_engine_remove_all_rules(void);

/**
 * @brief Отправить уведомление о обновлении правил ( WebSocket)
 */
void ws_notify_rules_update(void); // должна реализоваться в webserver.c

void ws_notify_virtual_vars_update(void); // должна реализоваться в webserver.c
/**
 * @brief Получить правило по ID
 */
const zb_rule_t* zb_rule_engine_get_rule(const char* rule_id);

/**
 * @brief Обработать событие (например, изменение датчика)
 */
void zb_rule_engine_process_event(cJSON* event);

/**
 * @brief Запустить правило вручную (по ID)
 */
bool zb_automation_run_rule_now(const char* rule_id);

/**
 * @brief Генерирует и отправляет событие для движка правил
 */
void zb_rule_trigger_state_update_double(uint16_t short_addr, const char* cluster_type, double value);


/**
 * @brief Генерирует и отправляет событие для движка правил
 * @param short_addr Адрес устройства
 * @param cluster_id ID кластера (например, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF)
 * @param attr_id ID атрибута (например, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID)
 * @param data Указатель на значение атрибута
 * @param data_len Длина данных
 * @param attr_type Тип атрибута (из esp_zb_zcl_attr_type_t)
 */
void zb_rule_trigger_state_update(uint16_t short_addr, esp_zb_zcl_cluster_id_t cluster_id, uint16_t attr_id, void* data, uint8_t data_len,
    esp_zb_zcl_attr_type_t attr_type
);

cJSON* rule_to_json(const zb_rule_t* rule);

// проверка правил с триггерами вынесена в общую видимость для запуска при старте SNTP
void check_time_triggers(void);

#ifdef __cplusplus
}
#endif

#endif // ZB_MANAGER_RULES_H
