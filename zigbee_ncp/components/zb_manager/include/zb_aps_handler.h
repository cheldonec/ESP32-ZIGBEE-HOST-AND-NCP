#ifndef ZB_APS_MODULE_H
#define ZB_APS_MODULE_H

#include "zboss_api.h"
#include "esp_zigbee_core.h"
/**************************************   APS  ****************************************************/
bool esp_zb_aps_data_indication_handler(esp_zb_apsde_data_ind_t ind);        // читаем чью-то индикацию
void esp_zb_aps_data_confirm_handler(esp_zb_apsde_data_confirm_t confirm);  // 

#endif