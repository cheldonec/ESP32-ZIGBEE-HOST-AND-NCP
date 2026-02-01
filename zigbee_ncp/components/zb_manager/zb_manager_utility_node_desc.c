#include "zb_manager_utility.h"
#include "zb_manager.h"
#include "zboss_api.h"
#include "zboss_api_zdo.h"
#include "zboss_api_buf.h"
#include "zboss_api_aps.h"
#include "esp_timer.h"
#include "esp_log.h"
//#include "esp_timer.h"
#include "driver/gptimer.h"

static const char *TAG = "zb_manager_utility_node_desc_module";

static void clear_node_desc_record(node_desc_resp_record_t* record)
{
    if (record != NULL) {
        record->status = ESP_FAIL;
    }
}

//********************************************************* Таймер для отправки ответа **********************************************/
static esp_timer_handle_t node_desc_result_timer = NULL;

//********************************************************* Колбэк на zb_zdo_node_desc_req ****************************************/
void local_node_desc_zboss_callback(zb_uint8_t param) {
    zb_zdo_node_desc_resp_t* resp = (zb_zdo_node_desc_resp_t*)zb_buf_begin(param);
    if ((resp->hdr.status == ESP_ZB_ZDP_STATUS_SUCCESS)&&(node_desc_result_timer!=NULL)) {
        zb_manager_obj.utility_results.last_node_desc_resp_result.status = ESP_OK;
        zb_apsde_data_indication_t *ind = ZB_BUF_GET_PARAM(param,zb_apsde_data_indication_t);
        zb_manager_obj.utility_results.last_node_desc_resp_result.resp_lqi = ind->lqi;
         ESP_LOGW(TAG, "Received response for TSN: %02x, status %02x, dev %04x, resp_lqi %d", 
        resp->hdr.tsn, resp->hdr.status, resp->hdr.nwk_addr, ind->lqi);
        zb_manager_obj.utility_results.last_node_desc_resp_result.node_flags.flags = resp->node_desc.node_desc_flags;
        zb_manager_obj.utility_results.last_node_desc_resp_result.server_mask_flags.server_mask = resp->node_desc.server_mask;
        
        zb_manager_obj.utility_results.last_node_desc_resp_result.mac_capabilities.mac_capabilities = resp->node_desc.mac_capability_flags;
        zb_manager_obj.utility_results.last_node_desc_resp_result.manuf_code = resp->node_desc.manufacturer_code;
        zb_manager_obj.utility_results.last_node_desc_resp_result.max_buff_size = resp->node_desc.max_buf_size;
        zb_manager_obj.utility_results.last_node_desc_resp_result.max_incoming_transfer_size = resp->node_desc.max_incoming_transfer_size;
        zb_manager_obj.utility_results.last_node_desc_resp_result.max_outgoing_transfer_size = resp->node_desc.max_outgoing_transfer_size;

        zb_manager_obj.utility_results.last_node_desc_resp_result.desc_capabilities_ext_active_ep_list_available = resp->node_desc.desc_capability_field & 1;
        zb_manager_obj.utility_results.last_node_desc_resp_result.desc_capabilities_ext_simple_desc_available = (resp->node_desc.desc_capability_field & 2) >> 1;

        ESP_LOGW(TAG, "Received logictype: %d and band %d power_source %d manuf_code %04x",zb_manager_obj.utility_results.last_node_desc_resp_result.node_flags.flags_values_t.logic_type,
        zb_manager_obj.utility_results.last_node_desc_resp_result.node_flags.flags_values_t.frequency_band, 
        zb_manager_obj.utility_results.last_node_desc_resp_result.mac_capabilities.mac_capabilities_values_t.power_source, 
        zb_manager_obj.utility_results.last_node_desc_resp_result.manuf_code);
    }
     zb_buf_free(param);
}

//***************************************************** Колбэк на таймер ***********************************************************/
void node_desc_result_timer_CB(void *arg) {
    ESP_LOGW("timer0", "node_desc_result_timer timer alarm!");
    esp_timer_delete(node_desc_result_timer);
    node_desc_result_timer=NULL;
    
    if (zb_manager_obj.utility_functions_callbacks.node_desc_req_callback != NULL) {
        zb_manager_obj.utility_functions_callbacks.node_desc_req_callback(&(zb_manager_obj.utility_results.last_node_desc_resp_result), arg);
    }
}

uint8_t zb_manager_node_desc_func(esp_zb_zdo_node_desc_req_param_t* param, uint16_t waiting_ms, void* user_ctx)
{
    // Проверка входных данных
    if (!param) {
        ESP_LOGE(TAG, "Invalid input parameters");
        return 0xff;
    }
    zb_bufid_t buf = zb_buf_get(ZB_FALSE, sizeof(zb_zdo_node_desc_req_t));
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate buffer for node_desc request");
        return 0xff;
    }
    zb_zdo_node_desc_req_t * node_desc_req = NULL;
    node_desc_req = zb_buf_initial_alloc(buf, sizeof(zb_zdo_node_desc_req_t));
    //node_desc_req = ZB_BUF_GET_PARAM(buf, zb_zdo_node_desc_req_t);
    if (!node_desc_req) {
        zb_buf_free(buf);  // Освобождение буфера при ошибке
        return 0xff;
    }
    node_desc_req->nwk_addr = param->dst_nwk_addr;
    
    if(node_desc_result_timer != NULL){
        zb_buf_free(buf);  // Освобождение буфера при ошибке
        ESP_LOGE(TAG, "zb_zdo_node_desc_req is already running, try later");
        return 0xff;
   }
   const esp_timer_create_args_t node_desc_result_timer_args = {
            .callback = &node_desc_result_timer_CB,
            .name = "one-shot",
            .arg = user_ctx,
    };
   ESP_ERROR_CHECK(esp_timer_create(&node_desc_result_timer_args, &node_desc_result_timer));
   ESP_ERROR_CHECK(esp_timer_start_once(node_desc_result_timer, waiting_ms * 1000));
   ESP_LOGW(TAG, "node_desc_result_timer timer stated");

   uint8_t tsn = 0xff;

   if(esp_zb_lock_acquire(100/portTICK_PERIOD_MS)){
        // очищаем поле с результатами match_desc_resp в главном объекте zb_manager
        clear_node_desc_record(&zb_manager_obj.utility_results.last_node_desc_resp_result);
        tsn = zb_zdo_node_desc_req(buf, local_node_desc_zboss_callback);
        esp_zb_lock_release();
    }
    if (tsn == 0xFF) {
        zb_buf_free(buf);  // Освобождение буфера при ошибке
        ESP_LOGE(TAG, "Failed to send node_desc request can not now, returned tsn: %d", tsn);
        return 0xff;
    }
     
    zb_manager_obj.utility_results.last_node_desc_resp_result.tsn = tsn;
    ESP_LOGW(TAG, "SEND node_desc for TSN: %d", tsn);
    return tsn;
}
void zb_manager_print_node_desc_resp_records(void)
{

}