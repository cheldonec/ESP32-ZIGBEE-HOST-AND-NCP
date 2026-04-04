// File: main/zbm_dev_base/zbm_attr_factory_humidity_meas.h
#ifndef ZBM_ATTR_FACTORY_HUMIDITY_MEAS_H
#define ZBM_ATTR_FACTORY_HUMIDITY_MEAS_H

#include "zbm_clusters_type.h"

/**
 * @brief Создаёт массив стандартных атрибутов для Relative Humidity Measurement Cluster (0x0405)
 * @param count Указатель, куда запишется количество созданных атрибутов
 * @return Массив указателей на zbm_cluster_attribute_t или NULL при ошибке
 */
zbm_cluster_attribute_t** zbm_create_humidity_meas_attr_array(zbm_cluster_role_t role_mask, uint8_t* count);

#endif // ZBM_ATTR_FACTORY_HUMIDITY_MEAS_H