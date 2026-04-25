// main/zbm_automation/zbm_automation_v2.h
#ifndef ZBM_AUTOMATION_V2_H
#define ZBM_AUTOMATION_V2_H

#include <stdint.h>
#include <stdbool.h>
#include "zbm_attr_types.h"
#include "zbm_cmd_types.h"
#include "cJSON.h"


// ========================================================
//                КОНФИГУРАЦИЯ
// ========================================================

#define ZB_AUTO_MAX_RULES       32
#define ZB_AUTO_MAX_TRIGGERS    4
#define ZB_AUTO_MAX_ACTIONS     8
#define ZB_AUTO_VAR_COUNT       32

// ========================================================
//                ВИРТУАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ========================================================

typedef struct {
    char guid[64];                    // формат: "var_0", "var_1", ...
    char name[32];                    // имя переменной
    zbm_attr_data_types_t data_type;  // тип данных
    uint16_t data_size;               // размер
    void* p_value;                    // значение (выделено отдельно)
    void* p_init_value;               // начальное значение при старте
    uint64_t last_update_ms;
    uint8_t idx;
} zbm_virtual_var_t;

extern zbm_virtual_var_t zbm_vars[ZB_AUTO_VAR_COUNT];

// Утилиты
void zbm_var_init(void);
void zbm_var_set_number(void* buf, zbm_attr_data_types_t type, int num);
// обновляет и запускае правила
void zbm_var_update_value(zbm_virtual_var_t* var, void* value, uint16_t size);

// обновляет во время настроек из UI
bool zbm_var_set_config(uint8_t idx, const char* name, zbm_attr_data_types_t type, void* runtime_value, uint16_t runtime_size, void* init_value, uint16_t init_size);
bool zbm_var_update_data(void* dst, zbm_attr_data_types_t type, void* src, uint16_t src_size);

void zbm_vars_save_to_storage(void);
void zbm_var_set_uint8(uint8_t idx, uint8_t value);
void zbm_var_set_int8(uint8_t idx, int8_t value);
void zbm_var_set_uint16(uint8_t idx, uint16_t value);
void zbm_var_set_string(uint8_t idx, const char* str);
uint8_t zbm_var_get_uint8(uint8_t idx);
bool zbm_var_compare(uint8_t idx, void* value, zbm_attr_data_types_t type);

// ========================================================
//                УСЛОВИЯ
// ========================================================

typedef enum {
    ZB_COND_EQ,  // ==
    ZB_COND_NE,  // !=
    ZB_COND_GT,  // >
    ZB_COND_LT,  // <
    ZB_COND_GTE, // >=
    ZB_COND_LTE, // <=
} zb_condition_t;

// ========================================================
//                ТРИГГЕРЫ
// ========================================================
typedef enum {
    ZB_TRIGGER_CAUSING,   // побуждающий (вызывает проверку)
    ZB_TRIGGER_ALLOWING,  // разрешающий (доп. условие)
} zb_trigger_mode_t;

typedef struct {
    char guid[64];                      // ссылка на атрибут или репорт
    zb_condition_t cond;
    zbm_attr_data_types_t expected_type;
    uint16_t expected_size;
    void* p_expected_value;             // выделяется динамически
    zb_trigger_mode_t mode;
} zb_trigger_t;

// ========================================================
//                ДЕЙСТВИЯ
// ========================================================

typedef enum {
    ZB_ACTION_SEND_CMD_BY_GUID,
    ZB_ACTION_SET_VAR_UINT8,
    ZB_ACTION_SET_VAR_INT8,
    ZB_ACTION_SET_VAR_UINT16,
    ZB_ACTION_SET_VAR_STRING,
    ZB_ACTION_INC_VAR,
    ZB_ACTION_DEC_VAR,
    ZB_ACTION_TOGGLE_VAR,
} zb_action_type_t;

typedef struct {
    zb_action_type_t type;
    union {
        struct {
            char cmd_guid[64];
            cJSON* params;  // массив параметров: { "value": 123 } или строка
        } send_cmd;
        struct {
            uint8_t var_idx;
            uint8_t value;
        } set_uint8;
        struct {
            uint8_t var_idx;
            int8_t value;
        } set_int8;
        struct {
            uint8_t var_idx;
            uint16_t value;
        } set_uint16;
        struct {
            uint8_t var_idx;
            char str[64];
        } set_str;
        struct {
            uint8_t var_idx;
        } inc, dec, toggle;
    } data;
} zb_action_t;

// ========================================================
//                ПРАВИЛО
// ========================================================

// выполнить только приоритетное правило или все, которые удовлетворяют побуждающему триггеру
typedef enum {
    ZB_RULE_EXEC_FIRST,     // default
    ZB_RULE_EXEC_ALL,
} zb_rule_execution_mode_t;

typedef enum {
    ZB_LOGIC_OR,  // хотя бы один совпал
    ZB_LOGIC_AND  // все должны совпасть
} zb_logic_op_t;

typedef struct {
    bool enabled;                   // false = always active
    uint8_t from_hour;              // 0–23
    uint8_t from_min;               // 0–59
    uint8_t to_hour;                // 0–23
    uint8_t to_min;                 // 0–59
    uint8_t days_of_week;           // битовая маска: bit0=понедельник, bit6=воскресенье, bit7=всегда
} zb_time_range_t;

typedef struct {
    char id[37];
    char name[64];
    bool enabled;
    int8_t priority;
    zb_rule_execution_mode_t exec_mode;
    zb_logic_op_t allowing_logic_op;            // применяется ТОЛЬКО к triggers[]
    zb_trigger_t cause_trigger;                 // один обязательный побуждающий триггер
    uint8_t trigger_count;
    uint8_t action_count;                       // только разрешающие условия
    zb_trigger_t triggers[ZB_AUTO_MAX_TRIGGERS];    // только разрешающие условия
    zb_action_t actions[ZB_AUTO_MAX_ACTIONS];
    zb_time_range_t time_range;
} zb_rule_t;

extern zb_rule_t* zb_rules[ZB_AUTO_MAX_RULES];
extern uint8_t zb_rules_count;


void zb_rules_load_all_from_storage(void);

bool rule_from_json(cJSON* json, zb_rule_t* out_rule);

cJSON* rule_to_json(const zb_rule_t* rule);

bool is_time_in_range(const zb_time_range_t* tr);

void execute_rule(const zb_rule_t* rule, const char* trigger_guid);

// ========================================================
//                API
// ========================================================
// Тип источника данных при обновлении
typedef enum {
    ZBM_DATA_SRC_ATTR,
    ZBM_DATA_SRC_CUSTOM_REPORT,
    ZBM_DATA_SRC_VAR,
} zbm_data_source_t;

#define ZB_AUTO_VAR_STR_LEN 64
#define ZB_AUTO_VAR_LONG_STR_LEN 256
bool zbm_var_realloc_storage(zbm_virtual_var_t *var);

void zb_automation_v2_init(void);

// функции для rest api 
bool zb_automation_v2_update_rule_from_json(cJSON* json);
bool zb_automation_v2_save_rule_to_storage(const char* id);



void zb_automation_v2_execute_action(const zb_action_t* act);

bool zb_automation_v2_rule_from_json_action(cJSON* json, zb_action_t* out_act);

cJSON* zb_automation_v2_rule_to_json_action(const zb_action_t* act);

void zb_automation_v2_on_data_change(zbm_data_source_t src_type,const char* guid,zbm_attr_data_types_t data_type,const void* value,uint16_t size);

bool zb_automation_v2_add_rule(const zb_rule_t* rule);

bool zb_automation_v2_remove_rule(const char* id);

bool zb_automation_v2_run_rule_now(const char* id);

bool value_matches(void* val1, void* val2, zbm_attr_data_types_t type, zb_condition_t cond);

// Уведомления WebSocket (реализуется в другом модуле)
void ws_notify_automation_rule_fired(const char* rule_id, const char* trigger_guid);


#endif // ZBM_AUTOMATION_V2_H