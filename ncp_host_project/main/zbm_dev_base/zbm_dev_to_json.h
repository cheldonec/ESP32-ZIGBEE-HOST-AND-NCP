#ifndef ZBM_DEV_TO_JSON_H

#define ZBM_DEV__TO_JSON_H

#include "cJSON.h"
#include "zbm_dev_types.h"

// === Функция: сериализация устройства в cJSON (краткая структура) ===
cJSON* device_to_brief_json(zbm_dev_t* dev);

// === Функция: сериализация устройства в cJSON (полная структура) ===
cJSON* device_to_json(zbm_dev_t* dev);



#endif