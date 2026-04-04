#ifndef ZBM_DEV_FROM_JSON_H

#define ZBM_DEV_FROM_JSON_H

#include "cJSON.h"
#include "zbm_dev_types.h"

// Основная функция восстановления устройства из JSON
zbm_dev_t* restore_device_from_json(cJSON* json);

#endif