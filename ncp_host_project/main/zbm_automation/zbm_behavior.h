// main/zbm_behavior.h
#ifndef ZBM_BEHAVIOR_H
#define ZBM_BEHAVIOR_H

#include <stdint.h>
#include <stdbool.h>
#include "zbm_automation_v2.h"  // для zb_action_t, zb_condition_t и т.д.

#ifdef __cplusplus
extern "C" {
#endif

#define ZBM_BEHAVIOR_MAX_COUNT     16
#define ZBM_BEHAVIOR_ID_LEN        37
#define ZBM_BEHAVIOR_PATH_MAX      64
#define ZBM_BEHAVIOR_ACTIONS_MAX   ZB_AUTO_MAX_ACTIONS

// Один behaviour — как правило
typedef struct {
    char id[ZBM_BEHAVIOR_ID_LEN];
    char name[64];
    bool enabled;
    uint8_t action_count;
    zb_action_t actions[ZBM_BEHAVIOR_ACTIONS_MAX];

    // === НОВОЕ: условия (аналог триггеров из правила) ===
    uint8_t condition_count;
    zb_trigger_t conditions[ZB_AUTO_MAX_TRIGGERS];  // те же, что и в правиле
    zb_logic_op_t logic_op;  // ZB_LOGIC_OR / ZB_LOGIC_AND
} zbm_behavior_t;

/**
 * @brief Инициализация системы поведений (загрузка из SPIFFS)
 */
void zbm_behavior_init(void);

/**
 * @brief Загружает все поведения из SPIFFS
 */
void zbm_behavior_load_all_from_storage(void);

/**
 * @brief Находит поведение по ID
 */
const zbm_behavior_t* zbm_behavior_find(const char* behavior_id);

/**
 * @brief Выполняет поведение по ID
 * @return true — найдено и выполнено, false — нет или отключено
 */
bool zbm_behavior_execute(const char* behavior_id);

/**
 * @brief Выполняет поведение напрямую
 */
void zbm_behavior_run(const zbm_behavior_t* bhv);

/**
 * @brief Сохраняет поведение в файл + обновляет индекс
 */
bool zbm_behavior_save_to_storage(const zbm_behavior_t* bhv);

/**
 * @brief Удаляет поведение из хранилища
 */
bool zbm_behavior_remove_from_storage(const char* behavior_id);

#ifdef __cplusplus
}
#endif

#endif // ZBM_BEHAVIOR_H