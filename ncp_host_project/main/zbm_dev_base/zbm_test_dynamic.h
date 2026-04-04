// File: main/zbm_dev_base/zbm_test_dynamic.h
#ifndef ZBM_TEST_DYNAMIC_H
#define ZBM_TEST_DYNAMIC_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Запуск теста: создаёт устройство → ждёт репорт → выводит JSON
 */
void zbm_test_dynamic_run(void);

/**
 * @brief Остановка теста (очистка)
 */
void zbm_test_dynamic_stop(void);

#ifdef __cplusplus
}
#endif

#endif // ZBM_TEST_DYNAMIC_H