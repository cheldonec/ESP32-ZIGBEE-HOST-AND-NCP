// File: main/zbm_dev_base/zbm_attr_factory_on_off.h
#ifndef ZBM_ATTR_FACTORY_ON_OFF_H
#define ZBM_ATTR_FACTORY_ON_OFF_H

#include "zbm_attr_types.h"
#include "zbm_clusters_type.h"

/**
 * @brief Создаёт массив стандартных атрибутов для On/Off кластера
 * @param[out] count Указатель, куда запишется количество атрибутов
 * @return Массив zbm_cluster_attribute_t* или NULL при ошибке
 */
zbm_cluster_attribute_t** zbm_create_on_off_attr_array(zbm_cluster_role_t role_mask, uint8_t* count);

/**
 * @brief Создаёт массив стандартных команд для On/Off кластера
 * @param[out] count Указатель, куда запишется количество атрибутов
 * @return Массив zbm_cluster_standart_cmd_t* или NULL при ошибке
 */
zbm_cluster_standart_cmd_t** zbm_create_on_off_cmd_array(zbm_cluster_role_t role_mask, uint8_t* count);

#endif // ZBM_ATTR_FACTORY_ON_OFF_H