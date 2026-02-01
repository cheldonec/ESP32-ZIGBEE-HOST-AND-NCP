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

static const char *TAG = "zb_manager_utility_ieee_addr_module";

static void clear_ieee_addr_records(ieee_addr_resp_records_t* records)
{
    if (records != NULL) {
        if (records->children != NULL) {
            free(records->children);
            records->children = NULL;
        }
        records->children_count = 0;
        records->status = ESP_FAIL;
    }
}

//********************************************************* Таймер для отправки ответа **********************************************/
static esp_timer_handle_t ieee_addr_req_result_timer = NULL;

//********************************************************* Колбэк на zb_zdo_ieee_addr_req ****************************************/
void local_ieee_addr_req_zboss_callback(zb_uint8_t param) {
    zb_zdo_ieee_addr_resp_t* ieee_adr_resp = (zb_zdo_ieee_addr_resp_t*)zb_buf_begin(param);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, ieee_adr_resp, sizeof(zb_zdo_ieee_addr_resp_t) + 6, ESP_LOG_INFO);

        if ((ieee_adr_resp->status == ESP_ZB_ZDP_STATUS_SUCCESS)&&(ieee_addr_req_result_timer!=NULL)) 
        {
            zb_apsde_data_indication_t *ind = ZB_BUF_GET_PARAM(param,zb_apsde_data_indication_t);
            ESP_LOGW(TAG, "Received response for TSN: %02x, status %02x, dev_ieee %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, short %04x, resp_lqi %d", 
            ieee_adr_resp->tsn, ieee_adr_resp->status, ieee_adr_resp->ieee_addr_remote_dev[7],
            ieee_adr_resp->ieee_addr_remote_dev[6], ieee_adr_resp->ieee_addr_remote_dev[5], ieee_adr_resp->ieee_addr_remote_dev[4], ieee_adr_resp->ieee_addr_remote_dev[3],
            ieee_adr_resp->ieee_addr_remote_dev[2], ieee_adr_resp->ieee_addr_remote_dev[1], ieee_adr_resp->ieee_addr_remote_dev[0], ieee_adr_resp->nwk_addr_remote_dev, ind->lqi);
            zb_manager_obj.utility_results.last_ieee_addr_resp_result.status = ESP_OK;
            
            zb_manager_obj.utility_results.last_ieee_addr_resp_result.resp_lqi = ind->lqi;
            zb_manager_obj.utility_results.last_ieee_addr_resp_result.tsn = ieee_adr_resp->tsn;
            zb_manager_obj.utility_results.last_ieee_addr_resp_result.short_addr = ieee_adr_resp->nwk_addr_remote_dev;
            memcpy(zb_manager_obj.utility_results.last_ieee_addr_resp_result.ieee_addr, ieee_adr_resp->ieee_addr_remote_dev, 8);
            
            uint8_t *children_count = (uint8_t*)(ieee_adr_resp + 1);
            ESP_LOGW(TAG,"childrens %d", *children_count);

            if (*children_count > 0) 
            {
                zb_uint8_t *children_rep_start_index = (zb_uint8_t*)(children_count + 1);
                //ESP_LOG_BUFFER_HEX_LEVEL(TAG, children_rep_start_index, sizeof(uint8_t) + 2, ESP_LOG_INFO);
                ESP_LOGW(TAG,"childrens_rep_start_index %d", *children_rep_start_index);

                zb_manager_obj.utility_results.last_ieee_addr_resp_result.children_count = *children_count;
                zb_manager_obj.utility_results.last_ieee_addr_resp_result.children_rep_start_index = *children_rep_start_index;
                zb_manager_obj.utility_results.last_ieee_addr_resp_result.children = calloc(1, sizeof(zb_uint16_t) * (*children_count));
                
                uint16_t *children_list = (zb_uint16_t*)(children_rep_start_index + 1);
                //ESP_LOG_BUFFER_HEX_LEVEL(TAG, children_list, sizeof(uint16_t), ESP_LOG_INFO);
                for (int i = 0; i < *children_count; i++) 
                {
                    zb_manager_obj.utility_results.last_ieee_addr_resp_result.children[i] = children_list[i];
                    ESP_LOGW(TAG,"short address of child %d: %04x", i, zb_manager_obj.utility_results.last_ieee_addr_resp_result.children[i]);
                }
            }
    }
    zb_buf_free(param);
}

//***************************************************** Колбэк на таймер ***********************************************************/
void ieee_addr_req_result_timer_CB(void *arg) {
    ESP_LOGW("timer0", "ieee_addr_req_result_timer timer alarm!");
    esp_timer_delete(ieee_addr_req_result_timer);
    ieee_addr_req_result_timer=NULL;
    if (zb_manager_obj.utility_functions_callbacks.ieee_addr_req_callback != NULL) {
        zb_manager_obj.utility_functions_callbacks.ieee_addr_req_callback(&(zb_manager_obj.utility_results.last_ieee_addr_resp_result), arg);
    }
}

uint8_t zb_manager_ieee_addr_func(esp_zb_zdo_ieee_addr_req_param_t* param, uint16_t waiting_ms, void* user_ctx)
{
    // Проверка входных данных
    if (!param) {
        ESP_LOGE(TAG, "Invalid input parameters");
        return 0xff;
    }
    zb_bufid_t buf = zb_buf_get(ZB_FALSE, sizeof(zb_zdo_ieee_addr_req_param_t));
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate buffer for ieee_addr_req request");
        return 0xff;
    }
    
    zb_zdo_ieee_addr_req_param_t * ieee_addr_req = NULL;
    ieee_addr_req = ZB_BUF_GET_PARAM(buf, zb_zdo_ieee_addr_req_param_t);

    /*ieee_addr_req = zb_buf_initial_alloc(buf, sizeof(zb_zdo_ieee_addr_req_param_t ));*/
    if (!ieee_addr_req) {
        zb_buf_free(buf);  // Освобождение буфера при ошибке
        return 0xff;
    }
    ieee_addr_req->dst_addr = param->addr_of_interest;
    ieee_addr_req->nwk_addr = param->dst_nwk_addr;
    ieee_addr_req->request_type = param->request_type;
    ieee_addr_req->start_index = param->start_index; //zb_zdo_ieee_addr_req

    if(ieee_addr_req_result_timer != NULL){
        zb_buf_free(buf);  // Освобождение буфера при ошибке
        ESP_LOGE(TAG, "zb_zdo_ieee_addr_req is already running, try later");
        return 0xff;
   }
   const esp_timer_create_args_t ieee_addr_req_result_timer_args = {
            .callback = &ieee_addr_req_result_timer_CB,
            .name = "one-shot",
            .arg = user_ctx,
    };
   ESP_ERROR_CHECK(esp_timer_create(&ieee_addr_req_result_timer_args, &ieee_addr_req_result_timer));
   ESP_ERROR_CHECK(esp_timer_start_once(ieee_addr_req_result_timer, waiting_ms * 1000));
   ESP_LOGW(TAG, "ieee_addr_req_result_timer timer stated");
   uint8_t tsn = 0xff;

   if(esp_zb_lock_acquire(100/portTICK_PERIOD_MS)){
        // очищаем поле с результатами match_desc_resp в главном объекте zb_manager
        clear_ieee_addr_records(&zb_manager_obj.utility_results.last_ieee_addr_resp_result);
        tsn = zb_zdo_ieee_addr_req(buf, local_ieee_addr_req_zboss_callback);
        esp_zb_lock_release();
    }
    if (tsn == 0xFF) {
        zb_buf_free(buf);  // Освобождение буфера при ошибке
        ESP_LOGE(TAG, "Failed to send ieee_addr_req request can not now, returned tsn: %d", tsn);
        return 0xff;
    }
     
    zb_manager_obj.utility_results.last_ieee_addr_resp_result.tsn = tsn;
    ESP_LOGW(TAG, "SEND ieee_addr_req for TSN: %d", tsn);
    return tsn;
}

void zb_manager_print_ieee_addr_resp_records(void)
{

}