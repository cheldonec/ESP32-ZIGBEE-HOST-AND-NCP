// main/zbm_dev_storage.h — обновлённый

#ifndef ZBM_DEV_STORAGE_SPIFFS_H
#define ZBM_DEV_STORAGE_SPIFFS_H

#include "zbm_dev_types.h"
#include "zbm_coordinator.h"
#include <stdbool.h>
#include "cJSON.h"

/**
 * @brief Сохраняет устройство в SPIFFS: dev_0x1234.json + обновляет индекс
 * @param dev Указатель на устройство
 * @return true при успехе
 */
bool zbm_save_device_to_spiffs_safe(zbm_dev_t* dev);

/**
 * @brief Сохраняет все устройства из базы в SPIFFS (с обновлением индекса)
 * @return true если все устройства успешно сохранены
 */
bool zbm_save_all_devices_to_spiffs_safe(void);

/**
 * @brief Загружает JSON устройства по короткому адресу
 * @param short_addr Короткий адрес
 * @return cJSON* или NULL
 */
cJSON* zbm_load_device_json_by_short_safe(uint16_t short_addr);

/**
 * @brief Загружает JSON устройства по IEEE-адресу (полезно при пересопряжении)
 * @param ieee_addr Указатель на 8 байт IEEE
 * @return cJSON* или NULL
 */
cJSON* zbm_load_device_json_by_ieee_safe(const uint8_t* ieee_addr);

/**
 * @brief Загружает все устройства из индекса (пока только логирование)
 */
//void zbm_load_all_devices_from_spiffs(void);

void zbm_load_all_devices_from_spiffs_and_restore(void);

#endif