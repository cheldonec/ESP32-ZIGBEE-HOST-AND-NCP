#ifndef ZBM_DEV_POLLING_H

#define ZBM_DEV_POLLING_H

#include "zbm_dev_types.h"

// Таймауты по умолчанию (в миллисекундах)
#define ZB_DEVICE_DEFAULT_TIMEOUT_MS        60000       // 60 сек — обычные устройства
#define ZB_DEVICE_SENSOR_TIMEOUT_MS         1800000      // 30 мин — батарейные датчики
#define ZB_DEVICE_SWITCH_TIMEOUT_MS         600000      // 10 мин — выключатели
#define ZB_DEVICE_ROUTER_TIMEOUT_MS         300000       // 5 мин — роутеры/шлюзы
#define ZB_DEVICE_TUYA_COIL_TIMEOUT_MS      180000      // 3 мин — tuya rele

uint32_t zbm_dev_get_timeout_for_device_id(uint16_t device_id);
void zbm_dev_configure_device_timeout(device_custom_t* dev);

esp_err_t zbm_dev_pooling_init(uint16_t pooling_period_in_seconds);

#endif