#pragma once
#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Global quirks database (assigned after load)
 */
extern cJSON *g_tuya_quirks;

/**
 * @brief Load TUYA quirks from /quirks partition
 */
esp_err_t quirks_load_tuya_database(void);

/**
 * @brief Save updated quirks database
 */
esp_err_t quirks_save_tuya_database(cJSON *db);
/*
пример использования
// Где-то в веб-обработчике
cJSON *new_quirk = cJSON_CreateObject();
cJSON_AddItemToArray(g_tuya_quirks, new_quirk);
// ... заполни

esp_err_t save_res = quirks_save_tuya_database(g_tuya_quirks);
if (save_res == ESP_OK) {
    ESP_LOGI(TAG, "Quirk database updated and saved");
}
*/

// Найти DP в квирке
cJSON *quirks_get_dp_info(const cJSON *quirk, uint8_t dp_id);

cJSON *quirks_get_quirk_by_model(const char *model_id);

#ifdef __cplusplus
}
#endif
