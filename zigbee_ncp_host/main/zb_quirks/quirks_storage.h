// quirks_storage.h
#pragma once
#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize quirks system (load index)
 */
esp_err_t quirks_init(void);

/**
 * @brief Load TUYA quirks index from SPIFFS
 */
esp_err_t quirks_load_index(void);

/**
 * @brief Get quirk template by model_id (loads file on demand)
 * @param model_id e.g. "TS0041"
 * @return cJSON* pointer to template (must be freed by caller!), or NULL
 */
cJSON *quirks_get_device_template(const char *model_id, const char *manufacturer_name);

/**
 * @brief Apply template to a new device (fills endpoints, commands, etc.)
 * @param dev_json Target device (as in API) — will be modified
 * @param template_quirk Source template from quirks
 * @return ESP_OK on success
 */
esp_err_t quirks_apply_template_to_device(cJSON *dev_json, const cJSON *template_quirk);

/**
 * @brief Save or update a device template
 * @param model_id Model ID (e.g. "TS0041")
 * @param template Template JSON (will be saved to file)
 * @param manufacturer Optional manufacturer name
 * @return ESP_OK on success
 */
esp_err_t quirks_save_device_template(const char *model_id, const char *manufacturer, const cJSON *template);

/**
 * @brief Add new model to index
 * @param model_id
 * @param manufacturer
 * @param filename
 * @return ESP_OK or error
 */
esp_err_t quirks_index_add_model(const char *model_id, const char *manufacturer, const char *filename);

/**
 * @brief Remove model from index and delete file
 */
esp_err_t quirks_remove_device_template(const char *model_id);

/*
// При первом появлении неизвестного варианта
if (!quirks_get_device_template(model_id, manufacturer_name)) {
    ESP_LOGW(TAG, "New variant detected: %s / %s", model_id, manufacturer_name);
    quirks_save_device_as_variant_template(model_id, manufacturer_name, current_device_json);
}
*/
esp_err_t quirks_save_device_as_variant_template(const char *model_id, const char *manufacturer_name, const cJSON *device_json);

/*
/quirks/
├── index.json
├── TS0041.json                          ← общий шаблон
├── TS0041__TZ3000_fa9mlvja.json         ← специфичный
├── TS0041__TZ3000_abc12345.json         ← ещё одна версия
└── TS0042.json
*/


#ifdef __cplusplus
}
#endif