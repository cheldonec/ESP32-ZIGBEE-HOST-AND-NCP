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

static const char *TAG = "zb_manager_utility_match_desc_module";

static void clear_match_desc_records(match_desc_resp_records_t* records)
{
    if (records != NULL) {
        if (records->records != NULL) {
            free(records->records);
            records->records = NULL;
        }
        records->count = 0;
        records->status = ESP_FAIL;
    }
}

//********************************************************* Таймер для отправки ответа **********************************************/
static esp_timer_handle_t match_desc_result_timer = NULL;

//********************************************************* Колбэк на zb_zdo_match_desc_req ****************************************/
void local_match_desc_zboss_callback(zb_uint8_t param) {
    zb_zdo_match_desc_resp_t* match_resp = (zb_zdo_match_desc_resp_t*)zb_buf_begin(param);
    if ((match_resp->status == ESP_ZB_ZDP_STATUS_SUCCESS)&&(match_desc_result_timer!=NULL)) {
        if (match_resp->match_len > 0) {
        zb_manager_obj.utility_results.last_match_desc_resp_result.status = ESP_OK;
        zb_apsde_data_indication_t *ind = ZB_BUF_GET_PARAM(param,zb_apsde_data_indication_t);
        zb_manager_obj.utility_results.last_match_desc_resp_result.resp_lqi = ind->lqi;
        ESP_LOGW(TAG, "Received response for TSN: %02x, status %02x, dev %04x, ep_count %02x, resp_lqi %d", 
        match_resp->tsn, match_resp->status, match_resp->nwk_addr, match_resp->match_len, ind->lqi);
        
        for (int i = 0; i < match_resp->match_len; i++) {
            if (zb_manager_obj.utility_results.last_match_desc_resp_result.count == 0) {
                zb_manager_obj.utility_results.last_match_desc_resp_result.records = calloc(1, sizeof(match_desc_resp_record_t));
                zb_manager_obj.utility_results.last_match_desc_resp_result.count = 1;
            } else {
                zb_manager_obj.utility_results.last_match_desc_resp_result.count = zb_manager_obj.utility_results.last_match_desc_resp_result.count +1;
                zb_manager_obj.utility_results.last_match_desc_resp_result.records = realloc(zb_manager_obj.utility_results.last_match_desc_resp_result.records, zb_manager_obj.utility_results.last_match_desc_resp_result.count * sizeof(match_desc_resp_record_t));
            }
        zb_manager_obj.utility_results.last_match_desc_resp_result.records[zb_manager_obj.utility_results.last_match_desc_resp_result.count-1].dev_short_address = match_resp->nwk_addr;
        zb_manager_obj.utility_results.last_match_desc_resp_result.records[zb_manager_obj.utility_results.last_match_desc_resp_result.count-1].endpoint = *(uint8_t*)(match_resp + 1 + i);
        ESP_LOGW(TAG, "Device %04x, endpoint %02x",zb_manager_obj.utility_results.last_match_desc_resp_result.records[zb_manager_obj.utility_results.last_match_desc_resp_result.count-1].dev_short_address,
            zb_manager_obj.utility_results.last_match_desc_resp_result.records[zb_manager_obj.utility_results.last_match_desc_resp_result.count-1].endpoint); 
        }
       }
    }
    zb_buf_free(param);
}

//***************************************************** Колбэк на таймер ***********************************************************/
void match_desc_result_timer_CB(void *arg) {
    ESP_LOGW("timer0", "match_desc_result_timer timer alarm!");
    esp_timer_delete(match_desc_result_timer);
    match_desc_result_timer=NULL;
    
    if (zb_manager_obj.utility_functions_callbacks.match_desc_callback != NULL) {
        zb_manager_obj.utility_functions_callbacks.match_desc_callback(&(zb_manager_obj.utility_results.last_match_desc_resp_result), arg);
    }
}

//***************************************************** Функция отправки запроса zb_zdo_match_desc_req ********************************/
uint8_t zb_manager_match_desc_func(esp_zb_zdo_match_desc_req_param_t* param, uint16_t waiting_ms, void* user_ctx)
{
    // Проверка входных данных
    if (!param) {
        ESP_LOGE(TAG, "Invalid input parameters");
        return 0xff;
    }
    zb_bufid_t buf = zb_buf_get(ZB_FALSE, sizeof(zb_zdo_match_desc_param_t));
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate buffer for match descriptor request");
        return 0xff;
    }
    zb_zdo_match_desc_param_t* match_req = NULL;
    match_req = zb_buf_initial_alloc(buf, sizeof(zb_zdo_match_desc_param_t) + (1)* sizeof(zb_uint16_t));
    if (!match_req) {
        zb_buf_free(buf);  // Освобождение буфера при ошибке
        return 0xff;
    }

    match_req->nwk_addr = param->dst_nwk_addr;
    match_req->addr_of_interest = param->addr_of_interest;
    match_req->profile_id = param->profile_id;
    match_req->num_in_clusters = param->num_in_clusters;
    match_req->num_out_clusters = param->num_out_clusters;
    // Копирование списка кластеров
    if (param->num_in_clusters > 0) {
        memcpy(match_req->cluster_list, 
               param->cluster_list, 
               param->num_in_clusters * sizeof(zb_uint16_t));
    }
    
    if (param->num_out_clusters > 0) {
        memcpy(match_req->cluster_list + param->num_in_clusters, 
               param->cluster_list + param->num_in_clusters, 
               param->num_out_clusters * sizeof(zb_uint16_t));
    }
   
   if(match_desc_result_timer != NULL){
    zb_buf_free(buf);  // Освобождение буфера при ошибке
        ESP_LOGE(TAG, "zb_zdo_match_desc_req is already running, try later");
        return 0xff;
   }
   
   const esp_timer_create_args_t match_desc_result_timer_args = {
            .callback = &match_desc_result_timer_CB,
            .name = "one-shot",
            .arg = user_ctx,
    };

    ESP_ERROR_CHECK(esp_timer_create(&match_desc_result_timer_args, &match_desc_result_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(match_desc_result_timer, waiting_ms * 1000));
    ESP_LOGW(TAG, "match_desc_result_timer timer stated");
    uint8_t tsn = 0xff;
    
    if(esp_zb_lock_acquire(100/portTICK_PERIOD_MS)){
        // очищаем поле с результатами match_desc_resp в главном объекте zb_manager
        clear_match_desc_records(&zb_manager_obj.utility_results.last_match_desc_resp_result);
        tsn = zb_zdo_match_desc_req(buf, local_match_desc_zboss_callback);
        esp_zb_lock_release();
    }

    if (tsn == 0xFF) {
        zb_buf_free(buf);  // Освобождение буфера при ошибке
        ESP_LOGE(TAG, "Failed to send match descriptor request can not now, returned tsn: %d", tsn);
        return 0xff;
    }
     
    zb_manager_obj.utility_results.last_match_desc_resp_result.tsn = tsn;
    ESP_LOGI(TAG, "SEND esp_zb_zdo_match_cluster for TSN: %d", tsn);
    return tsn;
}

void zb_manager_print_match_desc_resp_records(void)
{
    if (zb_manager_obj.utility_results.last_match_desc_resp_result.count > 0) {
        ESP_LOGI(TAG, "Match descriptor response records PRINT:");
        for (int i = 0; i < zb_manager_obj.utility_results.last_match_desc_resp_result.count; i++) {
            ESP_LOGI(TAG, "Device %04x, endpoint %02x", zb_manager_obj.utility_results.last_match_desc_resp_result.records[i].dev_short_address,
                     zb_manager_obj.utility_results.last_match_desc_resp_result.records[i].endpoint);
        }
    } else {
        ESP_LOGI(TAG, "No match descriptor response records found.");
    }
}

