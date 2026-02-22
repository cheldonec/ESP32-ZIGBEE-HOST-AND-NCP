#ifndef ZB_MANAGER_CLUSTERS_H
#define ZB_MANAGER_CLUSTERS_H
//#include "esp_zigbee_core.h"
#include "ncp_host_zb_api_core.h"
#include "zb_manager_basic_cluster.h"
#include "zb_manager_identify_cluster.h"
#include "zb_manager_temperature_meas_cluster.h"
#include "zb_manager_humidity_meas_cluster.h"
#include "zb_manager_on_off_cluster.h"
#include "zb_manager_power_config_cluster.h"




const char* zb_manager_get_cluster_name(uint16_t clusterID);

const char* zb_manager_get_attr_name(uint16_t clusterID, uint16_t attr_id);


/**
 * @brief Логирует Zigbee атрибут с декодированием значения
 * 
 * @param cluster_id      ID кластера
 * @param attr            Указатель на атрибут (zb_manager_attr_t)
 * @param src_addr        Указатель на адрес устройства (может быть NULL)
 * @param endpoint        Source endpoint (0 если неизвестен)
 */
void log_zb_attribute(uint16_t cluster_id,const zb_manager_cmd_report_attr_t *attr,const esp_zb_zcl_addr_t *src_addr,uint8_t endpoint);


#endif