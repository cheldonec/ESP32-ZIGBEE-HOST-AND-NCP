#include "zb_manager_utility.h"
#include "zb_manager.h"
#include "zboss_api.h"
#include "zboss_api_zdo.h"
#include "zboss_api_buf.h"
#include "zboss_api_aps.h"
#include "esp_timer.h"
#include "esp_log.h"

/******************** SIMPLE_DESC_REQ используется без таймера!!!!! так как множественные запросы не мешают предудщим как например в ieee_addr_req или match_desc */
static const char *TAG = "zb_manager_utility_simple_desc_module";

static void clear_simple_desc_records(simple_desc_resp_record_t* records)
{
    if (records != NULL) {
        if (records->input_clusters_list != NULL) {
            free(records->input_clusters_list);
            records->input_clusters_list = NULL;
        }
        if (records->output_clusters_list != NULL) {
            free(records->output_clusters_list);
            records->output_clusters_list = NULL;
        }
        records->app_input_cluster_count = 0;
        records->app_output_cluster_count = 0;
        records->status = ESP_FAIL;
    }
}

//********************************************************* Таймер для отправки ответа **********************************************/
//static esp_timer_handle_t simple_desc_req_result_timer = NULL;

//********************************************************* Колбэк на zb_zdo_ieee_addr_req ****************************************/
void local_simple_desc_req_zboss_callback(zb_uint8_t param) {
    zb_zdo_simple_desc_resp_t* simple_desc_resp = (zb_zdo_simple_desc_resp_t*)zb_buf_begin(param);
    if (simple_desc_resp->hdr.status == ZB_ZDP_STATUS_SUCCESS)
    {
         zb_apsde_data_indication_t *ind = ZB_BUF_GET_PARAM(param,zb_apsde_data_indication_t);
          ESP_LOGW(TAG, "Received response for TSN: %02x, status %02x, header_nwk %04x, ep %02x, resp_lqi %d", simple_desc_resp->hdr.tsn, simple_desc_resp->hdr.status, (uint16_t)simple_desc_resp->hdr.nwk_addr, simple_desc_resp->simple_desc.endpoint, ind->lqi);
            
            zb_manager_obj.utility_results.last_simple_desc_resp_result.resp_lqi = ind->lqi;
            zb_manager_obj.utility_results.last_simple_desc_resp_result.tsn = simple_desc_resp->hdr.tsn;
            zb_manager_obj.utility_results.last_simple_desc_resp_result.short_addr = simple_desc_resp->hdr.nwk_addr;
            zb_manager_obj.utility_results.last_simple_desc_resp_result.status = ESP_OK;
            zb_manager_obj.utility_results.last_simple_desc_resp_result.endpoint = simple_desc_resp->simple_desc.endpoint;
            zb_manager_obj.utility_results.last_simple_desc_resp_result.app_profile_id = simple_desc_resp->simple_desc.app_profile_id;
            zb_manager_obj.utility_results.last_simple_desc_resp_result.app_device_id = simple_desc_resp->simple_desc.app_device_id;
            zb_manager_obj.utility_results.last_simple_desc_resp_result.app_input_cluster_count = simple_desc_resp->simple_desc.app_input_cluster_count;
            zb_manager_obj.utility_results.last_simple_desc_resp_result.app_output_cluster_count = simple_desc_resp->simple_desc.app_output_cluster_count;

            if (zb_manager_obj.utility_results.last_simple_desc_resp_result.app_input_cluster_count > 0)
            {
                zb_manager_obj.utility_results.last_simple_desc_resp_result.input_clusters_list = calloc(1, zb_manager_obj.utility_results.last_simple_desc_resp_result.app_input_cluster_count);
                memcpy(zb_manager_obj.utility_results.last_simple_desc_resp_result.input_clusters_list, simple_desc_resp->simple_desc.app_cluster_list,zb_manager_obj.utility_results.last_simple_desc_resp_result.app_input_cluster_count);
            }

            if(zb_manager_obj.utility_results.last_simple_desc_resp_result.app_output_cluster_count > 0)
            {
                zb_manager_obj.utility_results.last_simple_desc_resp_result.output_clusters_list = calloc(1, zb_manager_obj.utility_results.last_simple_desc_resp_result.app_output_cluster_count);
                memcpy(zb_manager_obj.utility_results.last_simple_desc_resp_result.output_clusters_list, 
                    simple_desc_resp->simple_desc.app_cluster_list + zb_manager_obj.utility_results.last_simple_desc_resp_result.app_input_cluster_count, 
                    zb_manager_obj.utility_results.last_simple_desc_resp_result.app_output_cluster_count);
            }

    }
    zb_buf_free(param);

}

//***************************************************** Колбэк на таймер ***********************************************************/
/*void simple_desc_req_result_timer_CB(void *arg) {
    ESP_LOGW("timer0", "simple_desc_req_result_timer timer alarm!");
    esp_timer_delete(simple_desc_req_result_timer);
    simple_desc_req_result_timer=NULL;
    if (zb_manager_obj.utility_functions_callbacks.simple_desc_req_callback != NULL) {
        zb_manager_obj.utility_functions_callbacks.simple_desc_req_callback(&(zb_manager_obj.utility_results.last_simple_desc_resp_result), arg);
    }
}*/

uint8_t zb_manager_simple_desc_func(esp_zb_zdo_simple_desc_req_param_t* param, uint16_t waiting_ms, void* user_ctx)
{
    // Проверка входных данных
    if (!param) {
        ESP_LOGE(TAG, "Invalid input parameters");
        return 0xff;
    }

    zb_bufid_t buf = zb_buf_get(ZB_FALSE, sizeof(zb_zdo_simple_desc_req_t));
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate buffer for ieee_addr_req request");
        return 0xff;
    }

    zb_zdo_simple_desc_req_t *simple_desc_req = NULL;
    simple_desc_req = ZB_BUF_GET_PARAM(buf, zb_zdo_simple_desc_req_t);

    if (!simple_desc_req) {
        zb_buf_free(buf);  // Освобождение буфера при ошибке
        return 0xff;
    }
    
    simple_desc_req->nwk_addr = param->addr_of_interest;
    simple_desc_req->endpoint = param->endpoint;

    /*if(simple_desc_req_result_timer != NULL){
        zb_buf_free(buf);  // Освобождение буфера при ошибке
        ESP_LOGE(TAG, "zb_zdo_simple_desc_req is already running, try later");
        return 0xff;
   }
   const esp_timer_create_args_t simple_desc_req_result_timer_args = {
            .callback = &simple_desc_req_result_timer_CB,
            .name = "one-shot",
            .arg = user_ctx,
    };
   ESP_ERROR_CHECK(esp_timer_create(&simple_desc_req_result_timer_args, &simple_desc_req_result_timer));
   ESP_ERROR_CHECK(esp_timer_start_once(simple_desc_req_result_timer, waiting_ms * 1000));
   ESP_LOGW(TAG, "simple_desc_req_result_timer timer stated");*/
   uint8_t tsn = 0xff;

   if(esp_zb_lock_acquire(100/portTICK_PERIOD_MS)){
        // очищаем поле с результатами match_desc_resp в главном объекте zb_manager
        clear_simple_desc_records(&zb_manager_obj.utility_results.last_simple_desc_resp_result);
        tsn = zb_zdo_simple_desc_req(buf, local_simple_desc_req_zboss_callback);
        esp_zb_lock_release();
    }
    if (tsn == 0xFF) {
        zb_buf_free(buf);  // Освобождение буфера при ошибке
        ESP_LOGE(TAG, "Failed to send simple_desc_req request can not now, returned tsn: %d", tsn);
        return 0xff;
    }
    zb_manager_obj.utility_results.last_simple_desc_resp_result.tsn = tsn;
    ESP_LOGW(TAG, "SEND simple_desc_req for TSN: %d", tsn);
    return tsn;
    return 0;
}