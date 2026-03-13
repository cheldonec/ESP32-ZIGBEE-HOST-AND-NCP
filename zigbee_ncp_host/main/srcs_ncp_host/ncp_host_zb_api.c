/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_system.h"
#include "esp_random.h"

#include "ncp_host.h"
#include "ncp_host_zb_api_core.h"
#include "ncp_host_zb_api.h"
#include "ncp_host_zb_api_zdo.h"

#include "zb_manager_clusters.h"
#include "zb_manager_tuya_dp.h"
#include "zb_manager_config_platform.h"
//#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_command.h"
//#include "event_post_send.h"
#include "zb_manager_action_handler_worker.h"
#include "zb_manager_pairing.h"
#include "zb_manager_ncp_host.h"

static const char *TAG = "ncp_host_zb_api.c";
typedef struct {
    esp_zb_ieee_addr_t  extendedPanId;                      /*!< The network's extended PAN identifier */
    uint16_t            panId;                              /*!< The network's PAN identifier */
    uint8_t             radioChannel;                       /*!< A radio channel */
} esp_host_zb_network_t;

/**
 * @brief Type to represent the sync event between the host and BUS.
 *
 */
typedef struct {
    uint16_t        id;                                     /*!< The frame ID */
    uint16_t        size;                                   /*!< Data size on the event */
    void            *data;                                  /*!< Data on the event */
} esp_host_zb_ctx_t;

static esp_host_zb_network_t        s_host_zb_network;
QueueHandle_t                output_queue = NULL;           /*!< The queue handler for wait response */
QueueHandle_t                notify_queue = NULL;           /*!< The queue handler for wait notification */
SemaphoreHandle_t            lock_semaphore = NULL;

static esp_err_t esp_host_zb_form_network_fn(const uint8_t *input, uint16_t inlen)
{
    typedef struct {
        esp_zb_ieee_addr_t  extendedPanId;                  /*!< The network's extended PAN identifier */
        uint16_t            panId;                          /*!< The network's PAN identifier */
        uint8_t             radioChannel;                   /*!< A radio channel */
    } ESP_ZNSP_ZB_PACKED_STRUCT esp_zb_form_network_t;

    esp_zb_form_network_t *form_network = (esp_zb_form_network_t *)input;
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_BDB_SIGNAL_FORMATION,
        .msg = NULL,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = ESP_OK,
    };

    memcpy(s_host_zb_network.extendedPanId, form_network->extendedPanId, sizeof(esp_zb_ieee_addr_t));
    s_host_zb_network.panId = form_network->panId;
    s_host_zb_network.radioChannel = form_network->radioChannel;

    zb_manager_app_signal_handler(&app_signal);

    return ESP_OK;
}

static esp_err_t esp_host_zb_joining_network_fn(const uint8_t *input, uint16_t inlen)
{
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE,
        .msg = (const char *)input,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = ESP_OK,
    };

    zb_manager_app_signal_handler(&app_signal);

    return ESP_OK;
}


static esp_err_t esp_host_zb_permit_joining_fn(const uint8_t *input, uint16_t inlen)
{
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS,
        .msg = (const char *)input,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = ESP_OK,
    };

    if (zb_manager_app_signal_handler(&app_signal) == false)
    {
        zb_manager_user_app_signal_handler((&app_signal));
    }

    return ESP_OK;
}

static esp_err_t esp_host_zb_leave_network_fn(const uint8_t *input, uint16_t inlen)
{
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_ZDO_SIGNAL_LEAVE,
        .msg = (const char *)input,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = ESP_OK,
    };

    if (zb_manager_app_signal_handler(&app_signal) == false)
    {
        zb_manager_user_app_signal_handler((&app_signal));
    }

    return ESP_OK;
}

static esp_err_t esp_host_zb_set_bind_fn(const uint8_t *input, uint16_t inlen)
{
    typedef struct {
        local_esp_zb_zdp_status_t    zdo_status;
        esp_zb_user_cb_t       bind_usr;
    } ESP_ZNSP_ZB_PACKED_STRUCT esp_zb_zdo_bind_desc_t;

    esp_zb_zdo_bind_desc_t *zdo_bind_desc = (esp_zb_zdo_bind_desc_t *)input;

    // 1. Вызываем user_cb
    if (zdo_bind_desc->bind_usr.user_cb) {
        local_esp_zb_zdo_bind_callback_t zdo_bind_desc_callback = (local_esp_zb_zdo_bind_callback_t)zdo_bind_desc->bind_usr.user_cb;
        zdo_bind_desc_callback(zdo_bind_desc->zdo_status, (void *)zdo_bind_desc->bind_usr.user_ctx);
    }

    // 2. Формируем структуру для отправки в action worker
    zb_manager_bind_resp_message_t* resp_msg = calloc(1, sizeof(zb_manager_bind_resp_message_t));
    if (!resp_msg) {
        ESP_LOGE(TAG, "Failed to allocate resp_msg for BIND_RESP");
        return ESP_ERR_NO_MEM;
    }

    resp_msg->status = zdo_bind_desc->zdo_status;
    resp_msg->user_ctx = (void*)zdo_bind_desc->bind_usr.user_ctx;

    // 3. Отправляем в action worker
    bool post_ok = zb_manager_post_to_action_worker(ZB_ACTION_BIND_RESP, resp_msg, sizeof(*resp_msg));
    if (!post_ok) {
        ESP_LOGE(TAG, "Failed to post ZB_ACTION_BIND_RESP");
        zb_manager_free_bind_resp(resp_msg);
        free(resp_msg);
        resp_msg = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ZB_ACTION_BIND_RESP posted: status=0x%02x", resp_msg->status);
    free(resp_msg);
    resp_msg = NULL;
    return ESP_OK;
}

static esp_err_t esp_host_zb_set_unbind_fn(const uint8_t *input, uint16_t inlen)
{
    typedef struct {
        local_esp_zb_zdp_status_t    zdo_status;
        esp_zb_user_cb_t       bind_usr;
    } ESP_ZNSP_ZB_PACKED_STRUCT esp_zb_zdo_unbind_desc_t;

    esp_zb_zdo_unbind_desc_t *zdo_unbind_desc = (esp_zb_zdo_unbind_desc_t *)input;

    // 1. Вызываем user_cb
    if (zdo_unbind_desc->bind_usr.user_cb) {
        local_esp_zb_zdo_bind_callback_t zdo_unbind_callback = (local_esp_zb_zdo_bind_callback_t)zdo_unbind_desc->bind_usr.user_cb;
        zdo_unbind_callback(zdo_unbind_desc->zdo_status, (void *)zdo_unbind_desc->bind_usr.user_ctx);
    }

    // 2. Формируем сообщение для action worker
    zb_manager_bind_resp_message_t* resp_msg = calloc(1, sizeof(zb_manager_bind_resp_message_t));
    if (!resp_msg) {
        ESP_LOGE(TAG, "Failed to allocate resp_msg for UNBIND_RESP");
        return ESP_ERR_NO_MEM;
    }

    resp_msg->status = zdo_unbind_desc->zdo_status;
    resp_msg->user_ctx = (void*)zdo_unbind_desc->bind_usr.user_ctx;

    // 3. Отправляем в action worker
    bool post_ok = zb_manager_post_to_action_worker(ZB_ACTION_UNBIND_RESP, resp_msg, sizeof(*resp_msg));
    if (!post_ok) {
        ESP_LOGE(TAG, "Failed to post ZB_ACTION_UNBIND_RESP");
        free(resp_msg);
        resp_msg = NULL;
        return ESP_FAIL;
    }
    free(resp_msg);
    resp_msg = NULL;
    ESP_LOGI(TAG, "ZB_ACTION_UNBIND_RESP posted: status=0x%02x", resp_msg->status);
    return ESP_OK;
}

static esp_err_t esp_host_zb_find_match_fn(const uint8_t *input, uint16_t inlen)
{
    typedef struct {
        local_esp_zb_zdp_status_t zdo_status;
        uint16_t            addr;
        uint8_t             endpoint;
        esp_zb_user_cb_t    find_usr;
    } ESP_ZNSP_ZB_PACKED_STRUCT esp_zb_zdo_match_desc_t;

    esp_zb_zdo_match_desc_t *zdo_match_desc = (esp_zb_zdo_match_desc_t *)input;

    if (zdo_match_desc->find_usr.user_cb) {
        local_esp_zb_zdo_match_desc_callback_t zdo_match_desc_callback = (local_esp_zb_zdo_match_desc_callback_t)zdo_match_desc->find_usr.user_cb;
        zdo_match_desc_callback(zdo_match_desc->zdo_status, zdo_match_desc->addr, zdo_match_desc->endpoint, (void *)zdo_match_desc->find_usr.user_ctx);
    }

    return ESP_OK;
}

static esp_err_t zb_manager_report_attr_event_fn(const uint8_t *input, uint16_t inlen)
{
    typedef struct {
        esp_zb_zcl_status_t status;       /*!< The status of the report attribute response, which can refer to esp_zb_zcl_status_t */
        esp_zb_zcl_addr_t src_address;    /*!< The struct of address contains short and ieee address, which can refer to esp_zb_zcl_addr_s */
        uint8_t src_endpoint;             /*!< The endpoint id which comes from report device */
        uint8_t dst_endpoint;             /*!< The destination endpoint id */
        uint16_t cluster;                 /*!< The cluster id that reported */
    } ESP_ZNSP_ZB_PACKED_STRUCT esp_ncp_zb_report_attr_t;

    typedef struct {
        uint16_t id;                                    /*!< The identify of attribute */
        uint8_t  type;                                  /*!< The type of attribute, which can refer to esp_zb_zcl_attr_type_t */
        uint8_t  size;                                  /*!< The value size of attribute  */
    } ESP_ZNSP_ZB_PACKED_STRUCT esp_ncp_zb_attr_data_t;

    if (inlen < sizeof(esp_ncp_zb_report_attr_t) + sizeof(esp_ncp_zb_attr_data_t)) {
        ESP_LOGE(TAG, "Input too short: %u", inlen);
        return ESP_ERR_INVALID_SIZE;
    }

    // 1. Копируем RAW-данные
    uint8_t *raw_copy = malloc(inlen);
    if (!raw_copy) {
        ESP_LOGE(TAG, "Failed to allocate raw_copy");
        return ESP_ERR_NO_MEM;
    }
    memcpy(raw_copy, input, inlen);
    /************************************ */
    esp_ncp_zb_report_attr_t* cmd_info = (esp_ncp_zb_report_attr_t*)raw_copy;
    // ищем по короткому, если есть, то отправляем в worker иначе в pairing
    device_custom_t *dev_info = NULL;
    dev_info = zbm_dev_base_find_device_by_short_safe(cmd_info->src_address.u.short_addr);
    //dev_info = zbm_dev_base_find_device_by_short_safe(cmd_read_attr_resp_mess->info.src_address.u.short_addr);
    bool post_ok = false;
    if(dev_info)
    {
        ESP_LOGI(TAG, "ZB_ACTION_ATTR_REPORT zb_manager_post_to_action_worker");
        post_ok = zb_manager_post_to_action_worker(ZB_ACTION_ATTR_REPORT, raw_copy, inlen);
    }else{
        ESP_LOGI(TAG, "ZB_ACTION_ATTR_REPORT zb_manager_post_to_pairing_worker");
        post_ok = zb_manager_post_to_pairing_worker(ZB_PAIRING_ATTR_REPORT_EVENT, raw_copy, inlen);
    }

    if (!post_ok) {
            ESP_LOGE(TAG, "Failed to post ZB_ACTION_ATTR_REPORT");
            free(raw_copy);
            raw_copy = NULL;
            return ESP_FAIL;
        }else{
            free(raw_copy);
            raw_copy = NULL;
        }

    /************************************** */
    // 2. Отправляем в action worker
    /*bool post_ok = zb_manager_post_to_action_worker(ZB_ACTION_ATTR_REPORT, raw_copy, inlen);
    if (!post_ok) {
        ESP_LOGE(TAG, "Failed to post ZB_ACTION_ATTR_REPORT");
        free(raw_copy);
        raw_copy = NULL;
        return ESP_FAIL;
    }else {
        free (raw_copy);
        raw_copy = NULL;
    }*/

    ESP_LOGI(TAG, "ZB_ACTION_ATTR_REPORT posted (raw, len=%u)", inlen);
    return ESP_OK;
}




static esp_err_t zb_manager_dev_annce_event_fn(const uint8_t *input, uint16_t inlen)
{
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE,
        .msg = (const char *)input,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = ESP_OK,
    };

    if (zb_manager_app_signal_handler(&app_signal) == false)
    {
        zb_manager_user_app_signal_handler((&app_signal));
    }

    return ESP_OK;
}

static esp_err_t zb_manager_dev_assoc_event_fn(const uint8_t *input, uint16_t inlen)
{
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_NWK_SIGNAL_DEVICE_ASSOCIATED,
        .msg = (const char *)input,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = ESP_OK,
    };

    if (zb_manager_app_signal_handler(&app_signal) == false)
    {
        zb_manager_user_app_signal_handler((&app_signal));
    }

    return ESP_OK;
}

static esp_err_t zb_manager_dev_update_event_fn(const uint8_t *input, uint16_t inlen)
{
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_ZDO_SIGNAL_DEVICE_UPDATE,
        .msg = (const char *)input,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = ESP_OK,
    };

    if (zb_manager_app_signal_handler(&app_signal) == false)
    {
        zb_manager_user_app_signal_handler((&app_signal));
    }

    return ESP_OK;
}

static esp_err_t zb_manager_dev_auth_event_fn(const uint8_t *input, uint16_t inlen)
{
    local_esp_zb_app_signal_msg_t signal_msg = {
        .signal = ESP_ZB_ZDO_SIGNAL_DEVICE_AUTHORIZED,
        .msg = (const char *)input,
    };

    local_esp_zb_app_signal_t app_signal = {
        .p_app_signal = (uint32_t *)&signal_msg,
        .esp_err_status = ESP_OK,
    };

    if (zb_manager_app_signal_handler(&app_signal) == false)
    {
        zb_manager_user_app_signal_handler((&app_signal));
    }

    return ESP_OK;
}




static esp_err_t zb_manager_simple_desc_resp_fn(const uint8_t *input, uint16_t inlen)
{
    ESP_LOGW(TAG, "zb_manager_simple_desc_resp_fn");
    typedef struct {
        local_esp_zb_zdp_status_t zdo_status;
        esp_zb_user_cb_t    find_usr;
        local_esp_zb_af_simple_desc_1_1_t simple_desc; 
    } ESP_ZNSP_ZB_PACKED_STRUCT zb_manager_simple_desc_resp_pack_t;

    zb_manager_simple_desc_resp_pack_t *pkg  = (zb_manager_simple_desc_resp_pack_t *)input;
    // 1. Вызываем callback (user_cb) с user_ctx
    if (pkg ->find_usr.user_cb) {
        local_esp_zb_zdo_simple_desc_callback_t zdo_simple_desc_callback = (local_esp_zb_zdo_simple_desc_callback_t)pkg ->find_usr.user_cb;
        zdo_simple_desc_callback(pkg ->zdo_status, (local_esp_zb_af_simple_desc_1_1_t*)(&pkg ->simple_desc), (void *)pkg ->find_usr.user_ctx);
    }

    // формируем буфер для отправки в очередь
    // находим размер пакета
    
    //определяем размер esp_zb_af_simple_desc_1_1_t
    size_t simple_desc_size = sizeof(local_esp_zb_af_simple_desc_1_1_t) + 
        (pkg->simple_desc.app_input_cluster_count + pkg->simple_desc.app_output_cluster_count) * sizeof(uint16_t);


    size_t packet_size = sizeof(local_esp_zb_zdp_status_t) + simple_desc_size + sizeof(esp_zb_user_cb_t);

    // выделяем память для пакета
    uint8_t* packet = calloc(1,packet_size);
    if (!packet) {
         ESP_LOGE(TAG, "Failed to allocate memory for packet");
         return ESP_ERR_NO_MEM;
    }
    zb_manager_simple_desc_resp_message_t* resp_msg = (zb_manager_simple_desc_resp_message_t*)packet;
    // копируем статус
    memcpy(&resp_msg->status, &pkg->zdo_status, sizeof(local_esp_zb_zdp_status_t));

    // копируем simple_desc
   local_esp_zb_af_simple_desc_1_1_t* desc_copy = NULL;
    desc_copy = calloc(1, simple_desc_size);
    if (!desc_copy) {
        ESP_LOGE(TAG, "Failed to allocate memory for simple_desc copy");
        return ESP_ERR_NO_MEM;
    }
    // 4. Копируем всю структуру (включая кластеры)
    memcpy(desc_copy, &pkg->simple_desc, simple_desc_size);
    resp_msg->simple_desc = desc_copy;
    //memcpy(&resp_msg->simple_desc, &pkg->simple_desc, simple_desc_size);

    // копируем колбэк
    if (pkg ->find_usr.user_cb) {
        local_esp_zb_zdo_simple_desc_callback_t zdo_simple_desc_callback = (local_esp_zb_zdo_simple_desc_callback_t)pkg ->find_usr.user_cb;
        //memcpy(resp_msg->user_cb, &zdo_simple_desc_callback, sizeof(local_esp_zb_zdo_simple_desc_callback_t));
        resp_msg->user_cb = zdo_simple_desc_callback;
        // вызов колбэка
        //zdo_simple_desc_callback(pkg ->zdo_status, (local_esp_zb_af_simple_desc_1_1_t*)(&pkg ->simple_desc), (void *)pkg ->find_usr.user_ctx);
    }

    // копируем user_ctx
    if (pkg ->find_usr.user_ctx) {
        ESP_LOGW(TAG, "❌ zb_manager_simple_desc_resp_fn: sheduler %p", (void *)pkg ->find_usr.user_ctx);
        uint32_t sheduler_pointer = (uint32_t)pkg ->find_usr.user_ctx;

        //memcpy(resp_msg->user_ctx, (void *)pkg ->find_usr.user_ctx, sizeof(void *));
        resp_msg->user_ctx = (void *)pkg ->find_usr.user_ctx;
    }
    
    // присваеваем поля
    //zb_manager_simple_desc_resp_message_t* resp_msg = (zb_manager_simple_desc_resp_message_t*)packet;

     
    
    // 6. Отправляем в event loop
    /*bool post_ok = eventLoopPost(ZB_HANDLER_EVENTS, SIMPLE_DESC_RESP, resp_msg, sizeof(zb_manager_simple_desc_resp_message_t), portMAX_DELAY);
    if (!post_ok) {
        ESP_LOGE(TAG, "Failed to post SIMPLE_DESC_RESP event");
        zb_manager_free_simple_desc_resp(resp_msg);
        free(resp_msg);
        resp_msg = NULL;
        return ESP_FAIL;
    }*/

    
    bool post_ok = false;
    post_ok = zb_manager_post_to_pairing_worker(ZB_PAIRING_SIMPLE_DESC_RESP, resp_msg, packet_size);
    if (!post_ok) {
        ESP_LOGE(TAG, "Failed to post SIMPLE_DESC_RESP event");
        free(packet);
        packet = NULL;
        resp_msg = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}


/**
 * @brief Обработчик Active EP Response
 * 
 * Парсит входящий ZDO-ответ на запрос
 * esp_err_t zb_manager_zdo_active_ep_req(esp_zb_zdo_active_ep_req_param_t *cmd_req, esp_zb_zdo_active_ep_callback_t user_cb, void *user_ctx);
 * который подразумевает вызов CB
 * при обработке сперва вызывается CB + void *user_ctx
 * потом ответ отправляется в event loop zb_manager_active_ep_resp_message_t + void *user_ctx
 */
static esp_err_t zb_manager_active_ep_resp_fn(const uint8_t *input, uint16_t inlen)
{
    ESP_LOGI(TAG, "zb_manager_active_ep_resp_fn: inlen=%d", inlen);
    
    // структура входящих данных
    typedef struct {
        local_esp_zb_zdp_status_t zdo_status;
        uint8_t             ep_count;
        esp_zb_user_cb_t    find_usr;
        //uint8_t*            ep_id_list; 
    } ESP_ZNSP_ZB_PACKED_STRUCT esp_zb_zdo_active_ep_t;

    // 1. готовим вызов CB
    uint16_t outlen = inlen;// - sizeof(esp_zb_zdo_active_ep_t); // add ep_id_list
    uint8_t *output_for_cb = calloc(1, outlen);
    if (output_for_cb){
        memcpy(output_for_cb, input, outlen);
        esp_zb_zdo_active_ep_t *zdo_active_ep = (esp_zb_zdo_active_ep_t *)output_for_cb;
        //вызываем CB
        if(zdo_active_ep->find_usr.user_cb){
            local_esp_zb_zdo_active_ep_callback_t active_ep_cb = (local_esp_zb_zdo_active_ep_callback_t)zdo_active_ep->find_usr.user_cb;
            active_ep_cb(zdo_active_ep->zdo_status, zdo_active_ep->ep_count, output_for_cb + sizeof(esp_zb_zdo_active_ep_t), (void*)zdo_active_ep->find_usr.user_ctx);
        }
    free(output_for_cb);
    output_for_cb = NULL;
    }
    
    //2. готовим ответ в event loop

    // 1. Выделяем основную структуру
    zb_manager_active_ep_resp_message_t *resp_msg = calloc(1, sizeof(zb_manager_active_ep_resp_message_t));
    if (!resp_msg) {
        ESP_LOGE(TAG, "Failed to allocate resp_msg");
        return ESP_ERR_NO_MEM;
    }
    esp_zb_zdo_active_ep_t *zdo_active_ep = (esp_zb_zdo_active_ep_t *)input;
    resp_msg->status = *(local_esp_zb_zdp_status_t*)zdo_active_ep;
    resp_msg->ep_count = zdo_active_ep->ep_count;
    resp_msg->user_ctx = (void*)zdo_active_ep->find_usr.user_ctx;

    const uint8_t *ptr = input + sizeof(esp_zb_zdo_active_ep_t);
    
    // 3. Выделяем массив endpoint'ов
    resp_msg->ep_list = calloc(resp_msg->ep_count, sizeof(uint8_t));
    if (!resp_msg->ep_list) {
        ESP_LOGE(TAG, "Failed to allocate ep_list");
        free(resp_msg);
        resp_msg = NULL;
        return ESP_ERR_NO_MEM;
    }

    // 4. Копируем endpoint'ы
    memcpy(resp_msg->ep_list, input + sizeof(esp_zb_zdo_active_ep_t), resp_msg->ep_count);
    ptr += resp_msg->ep_count;
    
    // 5. Логируем (опционально)
    ESP_LOGI(TAG, "Active EP Response: status=0x%02x, count=%d,", resp_msg->status, resp_msg->ep_count);
    //ESP_LOG_BUFFER_HEX_LEVEL(TAG, resp_msg->ep_list, resp_msg->ep_count, ESP_LOG_INFO);

    // 6. ✅ Отправляем в event loop
    /*bool post_ok = eventLoopPost(ZB_HANDLER_EVENTS, ACTIVE_EP_RESP, resp_msg, sizeof(zb_manager_active_ep_resp_message_t), portMAX_DELAY);
    if (!post_ok) {
        ESP_LOGE(TAG, "Failed to post ACTIVE_EP_RESP_EVENT");
        // Освобождаем при ошибке
        zb_manager_free_active_ep_resp_ep_array(resp_msg);
        free(resp_msg);
        resp_msg = NULL;
        return ESP_FAIL;
    }*/
    
    // отправляем в pairing
    size_t total_size = sizeof(zb_manager_active_ep_resp_message_t) + resp_msg->ep_count * sizeof(uint8_t);
    bool post_ok = zb_manager_post_to_pairing_worker(ZB_PAIRING_ACTIVE_EP_RESP, resp_msg, total_size);
    if (!post_ok) {
        ESP_LOGE(TAG, "❌ Failed to post ZB_PAIRING_ACTIVE_EP_RESP to pairing worker");
        // Освобождаем при ошибке
        zb_manager_free_active_ep_resp_ep_array(resp_msg);
        free(resp_msg);
        resp_msg = NULL;
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "✅ ZB_PAIRING_ACTIVE_EP_RESP Posted to pairing worker");
    }
    return ESP_OK;
}

/**
 * @brief Обработчик ZB_MANAGER_NODE_DESC_RSP от NCP
 */
static esp_err_t zb_manager_node_desc_resp_fn(const uint8_t *input, uint16_t inlen)
{
    // Внутренняя структура для приёма данных от NCP
    typedef struct {
        uint8_t zdo_status;
        uint16_t nwk_addr;
        struct {
            uint16_t node_desc_flags;
            uint8_t mac_capability_flags;
            uint16_t manufacturer_code;
            uint8_t max_buf_size;
            uint16_t max_incoming_transfer_size;
            uint16_t server_mask;
            uint16_t max_outgoing_transfer_size;
            uint8_t desc_capability_field;
        } ESP_ZNSP_ZB_PACKED_STRUCT node_desc;
    } ESP_ZNSP_ZB_PACKED_STRUCT zb_manager_node_desc_resp_t;

    const size_t min_size = sizeof(zb_manager_node_desc_resp_t);
    if (inlen < min_size) {
        ESP_LOGE(TAG, "NODE_DESC_RSP: invalid length %u (expected >= %u)", inlen, min_size);
        return ESP_ERR_INVALID_SIZE;
    }

    zb_manager_node_desc_resp_t *pkg = (zb_manager_node_desc_resp_t *)input;

    // Выделяем сообщение для event loop
    zb_manager_node_desc_resp_message_t *resp_msg = calloc(1, sizeof(zb_manager_node_desc_resp_message_t));
    if (!resp_msg) {
        ESP_LOGE(TAG, "Failed to allocate resp_msg for NODE_DESC_RESP");
        return ESP_ERR_NO_MEM;
    }

    // Заполняем поля
    resp_msg->status = pkg->zdo_status;
    resp_msg->short_addr = pkg->nwk_addr;

    // Копируем ВСЕ поля node_desc
    resp_msg->node_desc.node_desc_flags = pkg->node_desc.node_desc_flags;
    resp_msg->node_desc.mac_capability_flags = pkg->node_desc.mac_capability_flags;
    resp_msg->node_desc.manufacturer_code = pkg->node_desc.manufacturer_code;
    resp_msg->node_desc.max_buf_size = pkg->node_desc.max_buf_size;
    resp_msg->node_desc.max_incoming_transfer_size = pkg->node_desc.max_incoming_transfer_size;
    resp_msg->node_desc.server_mask = pkg->node_desc.server_mask;
    resp_msg->node_desc.max_outgoing_transfer_size = pkg->node_desc.max_outgoing_transfer_size;
    resp_msg->node_desc.desc_capability_field = pkg->node_desc.desc_capability_field;

    ESP_LOGI(TAG, "NODE_DESC_RSP: 0x%04x, manuf=0x%04x, mac_cap=0x%02x, server_mask=0x%04x, status=0x%02x",
             pkg->nwk_addr,
             pkg->node_desc.manufacturer_code,
             pkg->node_desc.mac_capability_flags,
             pkg->node_desc.server_mask,
             pkg->zdo_status);

    // Отправляем в event loop
    /*bool post_ok = eventLoopPost(ZB_HANDLER_EVENTS, NODE_DESC_RESP, resp_msg, sizeof(*resp_msg), portMAX_DELAY);
    if (!post_ok) {
        ESP_LOGE(TAG, "Failed to post NODE_DESC_RESP event");
        free(resp_msg);
        return ESP_FAIL;
    }*/

    bool post_ok = zb_manager_post_to_pairing_worker(ZB_PAIRING_NODE_DESC_RESP, resp_msg, sizeof(*resp_msg));
    if (!post_ok) {
        ESP_LOGE(TAG, "Failed to post ZB_PAIRING_NODE_DESC_RESP event");
        free(resp_msg);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t zb_manager_read_attr_resp_fn(const uint8_t *input, uint16_t inlen)
{
    ESP_LOGI(TAG, "zb_manager_read_attr_resp_fn: inlen=%d", inlen);
    ESP_LOG_BUFFER_HEX_LEVEL("NCP RAW", input, inlen, ESP_LOG_INFO);

    if (inlen < sizeof(esp_zb_zcl_cmd_info_t) + 1) {
        ESP_LOGE(TAG, "Invalid length: %u", inlen);
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t *input_copy = malloc(inlen);
    if (!input_copy) {
        ESP_LOGE(TAG, "Failed to allocate input_copy");
        return ESP_ERR_NO_MEM;
    }
    memcpy(input_copy, input, inlen);

    esp_zb_zcl_cmd_read_attr_resp_message_t* cmd_read_attr_resp_mess = NULL;
    cmd_read_attr_resp_mess = (esp_zb_zcl_cmd_read_attr_resp_message_t*)input_copy;

    // ищем по короткому, если есть, то отправляем в worker иначе в pairing
    device_custom_t *dev_info = NULL;
    dev_info = zbm_dev_base_find_device_by_short_safe(cmd_read_attr_resp_mess->info.src_address.u.short_addr);
    //dev_info = zbm_dev_base_find_device_by_short_safe(cmd_read_attr_resp_mess->info.src_address.u.short_addr);
    bool post_ok = false;
    if(dev_info)
    {
        ESP_LOGI(TAG, "ZB_ACTION_ATTR_READ_RESP zb_manager_post_to_action_worker");
        post_ok = zb_manager_post_to_action_worker(ZB_ACTION_ATTR_READ_RESP, input_copy, inlen);
    }else{
        ESP_LOGI(TAG, "ZB_ACTION_ATTR_READ_RESP zb_manager_post_to_pairing_worker");
        post_ok = zb_manager_post_to_pairing_worker(ZB_PAIRING_ATTR_READ_RESP, input_copy, inlen);
    }

    if (!post_ok) {
            ESP_LOGE(TAG, "Failed to post ZB_ACTION_ATTR_READ_RESP");
            free(input_copy);
            input_copy = NULL;
            return ESP_FAIL;
        }else{
            free(input_copy);
            input_copy = NULL;
        }
    ESP_LOGI(TAG, "ZB_ACTION_ATTR_READ_RESP posted with raw buffer");
    return ESP_OK;
}



static esp_err_t zb_manager_read_attr_resp_fn_old(const uint8_t *input, uint16_t inlen)
{
    ESP_LOGI(TAG, "zb_manager_read_attr_resp_fn: inlen=%d", inlen);
    ESP_LOG_BUFFER_HEX_LEVEL("NCP RAW", input, inlen, ESP_LOG_INFO);
    //ESP_LOGI(TAG, "Parsed attr_count = %d", attr_count);


    zb_manager_cmd_read_attr_resp_message_t* resp_msg = NULL;
    resp_msg = calloc(1, sizeof(zb_manager_cmd_read_attr_resp_message_t));
    if (!resp_msg) {
        ESP_LOGE(TAG, "Failed to allocate resp_msg");
        return ESP_ERR_NO_MEM;
    }

    // Копируем общую информацию
    esp_zb_zcl_cmd_info_t* cmd_info = (esp_zb_zcl_cmd_info_t*)input;
    memcpy(&resp_msg->info, cmd_info, sizeof(esp_zb_zcl_cmd_info_t));

    // Читаем attr_count
    uint8_t attr_count = *(uint8_t*)(input + sizeof(esp_zb_zcl_cmd_info_t));
    resp_msg->attr_count = attr_count;
    ESP_LOGW(TAG, "Attribute count: %d", attr_count);

    // Выделяем массив атрибутов
    resp_msg->attr_arr = calloc(attr_count, sizeof(zb_manager_attr_t));
    if (!resp_msg->attr_arr) {
        ESP_LOGE(TAG, "Failed to allocate attr_arr");
        free(resp_msg);
        return ESP_ERR_NO_MEM;
    }

    uint8_t* pointer = (uint8_t*)(input + sizeof(esp_zb_zcl_cmd_info_t) + sizeof(uint8_t));
    bool alloc_failed = false;

    for (uint8_t i = 0; i < attr_count; i++) {
        zb_manager_read_resp_attr_t* attr = &resp_msg->attr_arr[i];

        attr->attr_id  = *((uint16_t*)pointer);
        pointer += sizeof(uint16_t);
        attr->attr_type = *((esp_zb_zcl_attr_type_t*)pointer);
        pointer += sizeof(esp_zb_zcl_attr_type_t);
        attr->attr_len = *(uint8_t*)pointer;
        pointer += sizeof(uint8_t);

        if (attr->attr_len > 0) {
            attr->attr_value = calloc(1, attr->attr_len);
            if (!attr->attr_value) {
                ESP_LOGE(TAG, "Failed to allocate attr_value for attr_id=0x%04x", attr->attr_id);
                alloc_failed = true;
                break;
            }
            memcpy(attr->attr_value, pointer, attr->attr_len);
            pointer += attr->attr_len;
        } else {
            attr->attr_value = NULL;
        }
    }

    // Если ошибка выделения
    if (alloc_failed) {
        zb_manager_free_read_attr_resp_attr_array(resp_msg);
        free(resp_msg);
        return ESP_ERR_NO_MEM;
    }
   

    // ✅ Отправляем в action worker
    
    bool post_ok = zb_manager_post_to_action_worker(ZB_ACTION_ATTR_READ_RESP, resp_msg, sizeof(*resp_msg));
    //bool post_ok = zb_manager_post_to_action_worker(ZB_ACTION_ATTR_READ_RESP, resp_msg, inlen);
    if (!post_ok) {
        ESP_LOGE(TAG, "Failed to post ZB_ACTION_ATTR_READ_RESP");
        // ❌ Только если ошибка — освобождаем
        zb_manager_free_read_attr_resp_attr_array(resp_msg);
        free(resp_msg);
        return ESP_FAIL;
    }

    free (resp_msg);
    resp_msg = NULL;

    // ✅ Успех: post_to_action_worker() сам скопировал и будет освобождать
    // → НЕ вызываем free(resp_msg) здесь!
    ESP_LOGI(TAG, "ZB_ACTION_ATTR_READ_RESP posted successfully");
    return ESP_OK;
}




/**
 * @brief Обработчик ZB_MANAGER_REPORT_CONFIG_RESP (0x555C)
 */
static esp_err_t zb_manager_report_config_resp_fn(const uint8_t *input, uint16_t inlen)
{
    ESP_LOGI(TAG, "zb_manager_report_config_resp_fn: received %u bytes", inlen);

    // Структура заголовка
    typedef struct {
        esp_zb_zcl_cmd_info_t info;
        uint8_t attr_count;
    } ESP_ZNSP_ZB_PACKED_STRUCT esp_zb_report_config_resp_hdr_t;

    const size_t hdr_len = sizeof(esp_zb_report_config_resp_hdr_t);
    const size_t item_len = 4; // direction (1) + attr_id (2) + status (1)

    // Валидация минимальной длины
    if (inlen < hdr_len) {
        ESP_LOGE(TAG, "Invalid length (too short): %u", inlen);
        return ESP_ERR_INVALID_SIZE;
    }

    const esp_zb_report_config_resp_hdr_t *hdr = (const esp_zb_report_config_resp_hdr_t *)input;
    const uint8_t attr_count = hdr->attr_count;

    ESP_LOGI(TAG, "Report Config Response: status=0x%02x, cluster=0x%04x, src_addr=0x%04x, attr_count=%d",
             hdr->info.status, hdr->info.cluster, hdr->info.src_address.u.short_addr, attr_count);

    // Проверка, хватает ли места для всех элементов
    if (inlen < hdr_len + attr_count * item_len) {
        ESP_LOGE(TAG, "Buffer too short: need %u, got %u", hdr_len + attr_count * item_len, inlen);
        return ESP_ERR_INVALID_SIZE;
    }

    const uint8_t *ptr = input + hdr_len;

    // Парсим и логируем
    for (uint8_t i = 0; i < attr_count; i++) {
        uint8_t direction = ptr[0];
        uint16_t attr_id = (ptr[2] << 8) | ptr[1];  // Безопасное чтение uint16 (little-endian)
        uint8_t status = ptr[3];

        ESP_LOGI(TAG, "  [%d] Dir=%d, Attr=0x%04x, Status=0x%02x", i, direction, attr_id, status);
        ptr += item_len;

    }

    // Подготовка сообщения для event loop
    zb_manager_cmd_report_config_resp_message_t *msg = calloc(1, sizeof(*msg));
    if (!msg) {
        ESP_LOGE(TAG, "Failed to allocate zb_manager_cmd_report_config_resp_message_t");
        return ESP_ERR_NO_MEM;
    }

    msg->status = hdr->info.status;
    msg->cluster = hdr->info.cluster;
    msg->short_addr = hdr->info.src_address.u.short_addr;
    msg->attr_count = attr_count;

    msg->attr_list = calloc(attr_count, sizeof(zb_manager_report_config_attr_status_t));
    if (!msg->attr_list) {
        free(msg);
        ESP_LOGE(TAG, "Failed to allocate attr_list");
        return ESP_ERR_NO_MEM;
    }

    ptr = input + hdr_len;
    for (uint8_t i = 0; i < attr_count; i++) {
        msg->attr_list[i].direction = ptr[0];
        msg->attr_list[i].attr_id = (ptr[2] << 8) | ptr[1];
        msg->attr_list[i].status = ptr[3];
        ptr += item_len;
    }

    // Отправляем в event loop
    /*bool post_ok = eventLoopPost(ZB_HANDLER_EVENTS, REPORT_CONFIG_RESP, msg, sizeof(*msg), portMAX_DELAY);
    if (!post_ok) {
        ESP_LOGE(TAG, "Failed to post REPORT_CONFIG_RESP event");
        zb_manager_free_report_config_resp(msg);
        return ESP_FAIL;
    }*/

    return ESP_OK;
}

/**
 * @brief Обработчик ZB_MANAGER_CUSTOM_CLUSTER_REPORT от NCP
 */
/**
 * @brief Обработчик ZB_MANAGER_CUSTOM_CLUSTER_REPORT от NCP
 */
static esp_err_t zb_manager_custom_cluster_rep_event_fn(const uint8_t *input, uint16_t inlen)
{
    /*typedef struct {
    uint16_t short_addr;
    uint8_t  src_endpoint;
    uint8_t  dst_endpoint;
    uint16_t cluster_id; 
    uint8_t  command_id;
    uint8_t  manuf_specific;
    uint16_t manuf_code;
    uint8_t  seq_num;
    int8_t   rssi;
    uint16_t data_len;
    uint8_t  data[32];  // variable length
} ESP_ZNSP_ZB_PACKED_STRUCT zb_manager_custom_cluster_report_t;

    const size_t hdr_len = offsetof(zb_manager_custom_cluster_report_t, data);
    if (inlen < hdr_len) {
        ESP_LOGE(TAG, "CUSTOM_CLUSTER: invalid length %u < %u", inlen, hdr_len);
        return ESP_ERR_INVALID_SIZE;
    }

    const zb_manager_custom_cluster_report_t *report = (zb_manager_custom_cluster_report_t *)input;

    // Определяем реальную длину данных
    uint16_t actual_data_len = inlen - hdr_len;
    if (report->data_len != actual_data_len) {
        ESP_LOGW(TAG, "Data length mismatch: reported=%u, actual=%u → using actual", report->data_len, actual_data_len);
        // Можно обрезать или доверять actual
    }

    // Логируем получение
    ESP_LOGI(TAG, "📥 CUSTOM_CLUSTER: short=0x%04x, ep=%d→%d, manuf=0x%04x, cmd=0x%02x, len=%u, rssi=%d",
             report->short_addr, report->src_endpoint, report->dst_endpoint,
             report->manuf_code, report->command_id, actual_data_len, report->rssi);

    if (actual_data_len > 0) {
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, report->data, actual_data_len, ESP_LOG_INFO);
    }

    // Готовим сообщение для action worker
    zb_manager_custom_cluster_report_message_t *msg = calloc(1, sizeof(*msg));
    if (!msg) {
        ESP_LOGE(TAG, "❌ Failed to allocate msg for CUSTOM_CLUSTER");
        return ESP_ERR_NO_MEM;
    }

    // Копируем все поля
    msg->short_addr     = report->short_addr;
    msg->src_endpoint   = report->src_endpoint;
    msg->dst_endpoint   = report->dst_endpoint;
    msg->command_id     = report->command_id;
    msg->manuf_specific = report->manuf_specific;
    msg->manuf_code     = report->manuf_code;
    msg->seq_num        = report->seq_num;
    msg->rssi           = report->rssi;
    msg->data_len       = actual_data_len;

    if (actual_data_len > 0) {
        msg->data = malloc(actual_data_len);
        if (!msg->data) {
            ESP_LOGE(TAG, "❌ Failed to allocate data for CUSTOM_CLUSTER");
            free(msg);
            return ESP_ERR_NO_MEM;
        }
        memcpy(msg->data, report->data, actual_data_len);
    } else {
        msg->data = NULL;
    }
    uint16_t full_size = sizeof(*msg) + actual_data_len;
    // Отправляем в action worker
    bool post_ok = zb_manager_post_to_action_worker(ZB_ACTION_CUSTOM_CLUSTER_REPORT, msg, full_size);
    if (!post_ok) {
        ESP_LOGE(TAG, "❌ Failed to post ZB_ACTION_CUSTOM_CLUSTER_REPORT");
        zb_manager_free_custom_cluster_report_message(msg);
        free(msg);
        return ESP_FAIL;
    }else{
        zb_manager_free_custom_cluster_report_message(msg);
        free(msg);
        msg = NULL;
    }

    ESP_LOGI(TAG, "✅ ZB_ACTION_CUSTOM_CLUSTER_REPORT posted to worker");
*/
    return ESP_OK;
}

static esp_err_t zb_manager_disc_attr_resp_fn(const uint8_t *input, uint16_t inlen)
{
    ESP_LOGI(TAG, "Received ZB_MANAGER_DISCOVERY_ATTR_RESP, len=%u", inlen);

    if (inlen < sizeof(esp_zb_zcl_cmd_info_t) + 1) {
        ESP_LOGE(TAG, "Invalid length for discover attr response");
        return ESP_ERR_INVALID_SIZE;
    }

    // Копируем RAW-данные, чтобы передать в worker
    uint8_t *raw_copy = malloc(inlen);
    if (!raw_copy) {
        ESP_LOGE(TAG, "Failed to allocate raw_copy");
        return ESP_ERR_NO_MEM;
    }
    memcpy(raw_copy, input, inlen);

    // Проверим: устройство уже известно? Если да — шлём в action_worker, иначе в pairing?
    esp_zb_zcl_cmd_info_t *info = (esp_zb_zcl_cmd_info_t *)raw_copy;
    device_custom_t *dev_info = zbm_dev_base_find_device_by_short_safe(info->src_address.u.short_addr);

    bool post_ok = false;
    if (dev_info) {
        ESP_LOGI(TAG, "Forwarding discovery response to action_worker (short=0x%04x)", info->src_address.u.short_addr);
        post_ok = zb_manager_post_to_action_worker(ZB_ACTION_DISCOVER_ATTR_RESP, raw_copy, inlen);
    } else {
        ESP_LOGI(TAG, "Device not found → forwarding to pairing_worker");
        post_ok = zb_manager_post_to_pairing_worker(ZB_ACTION_DISCOVER_ATTR_RESP, raw_copy, inlen);
    }

    if (!post_ok) {
        ESP_LOGE(TAG, "Failed to post discovery response to worker");
        free(raw_copy);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ZB_ACTION_DISCOVER_ATTR_RESP posted successfully");
    return ESP_OK;
}

/**
 * @brief Обработчик ZB_MANAGER_NOSTANDART_CLUSTER_CMD_REPORT от NCP
 */
static esp_err_t zb_manager_nostandart_cluster_cmd_resp_fn(const uint8_t *input, uint16_t inlen)
{
    typedef struct {
        esp_zb_zcl_status_t status;
        esp_zb_zcl_addr_t src_address;
        uint8_t src_endpoint;
        uint8_t dst_endpoint;
        uint16_t cluster;
        uint8_t command_id;
        uint8_t data_len;
        uint8_t data[64]; // variable, but capped
    } ESP_ZNSP_ZB_PACKED_STRUCT zb_ncp_nostandart_cmd_t;

    const size_t hdr_len = offsetof(zb_ncp_nostandart_cmd_t, data);
    if (inlen < hdr_len) {
        ESP_LOGE(TAG, "NOSTANDART_CMD: invalid length %u < %u", inlen, hdr_len);
        return ESP_ERR_INVALID_SIZE;
    }

    const zb_ncp_nostandart_cmd_t *cmd = (const zb_ncp_nostandart_cmd_t *)input;

    // Проверка длины пейлоада
    uint8_t actual_len = (cmd->data_len > 64) ? 64 : cmd->data_len;
    if (hdr_len + actual_len > inlen) {
        ESP_LOGE(TAG, "NOSTANDART_CMD: data overflows buffer");
        return ESP_ERR_INVALID_SIZE;
    }

    // Логируем
    ESP_LOGI(TAG, "🔧 NOSTANDART CMD: short=0x%04x, ep=%d→%d, cluster=0x%04x, cmd=0x%02x, len=%u",
             cmd->src_address.u.short_addr,
             cmd->src_endpoint,
             cmd->dst_endpoint,
             cmd->cluster,
             cmd->command_id,
             actual_len);

    if (actual_len > 0) {
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, cmd->data, actual_len, ESP_LOG_INFO);
    }

    // Подготавливаем сообщение для action worker
    zb_manager_cmd_nostandart_cluster_resp_message_t *msg = calloc(1, sizeof(*msg));
    if (!msg) {
        ESP_LOGE(TAG, "❌ Failed to allocate msg for NOSTANDART_CMD");
        return ESP_ERR_NO_MEM;
    }

    // Копируем все поля
    msg->status = cmd->status;
    memcpy(&msg->src_address, &cmd->src_address, sizeof(esp_zb_zcl_addr_t));
    msg->src_endpoint = cmd->src_endpoint;
    msg->dst_endpoint = cmd->dst_endpoint;
    msg->cluster = cmd->cluster;
    msg->cmd_id = cmd->command_id;
    msg->cmd_payload_len = actual_len;
    memcpy(msg->cmd_payload, cmd->data, actual_len);

    // Отправляем в action worker
    bool post_ok = zb_manager_post_to_action_worker(ZB_ACTION_NOSTANDART_CLUSTER_CMD_RESP, msg, sizeof(*msg));
    if (!post_ok) {
        ESP_LOGE(TAG, "❌ Failed to post ZB_ACTION_NOSTANDART_CLUSTER_CMD_RESP");
        free(msg);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "✅ ZB_ACTION_NOSTANDART_CLUSTER_CMD_RESP posted");
    return ESP_OK;
}

static const esp_host_zb_func_t host_zb_func_table[] = {
    {ESP_NCP_NETWORK_FORMNETWORK, esp_host_zb_form_network_fn},
    {ESP_NCP_NETWORK_JOINNETWORK, esp_host_zb_joining_network_fn},
    {ESP_NCP_NETWORK_PERMIT_JOINING, esp_host_zb_permit_joining_fn},
    {ESP_NCP_NETWORK_LEAVENETWORK, esp_host_zb_leave_network_fn},
    {ESP_NCP_ZDO_BIND_SET, esp_host_zb_set_bind_fn},
    {ESP_NCP_ZDO_UNBIND_SET, esp_host_zb_set_unbind_fn},
    {ESP_NCP_ZDO_FIND_MATCH, esp_host_zb_find_match_fn},
    {ESP_NCP_ZCL_ATTR_REPORT_EVENT, zb_manager_report_attr_event_fn},
    {ESP_NCP_ZCL_ATTR_READ_RESP, zb_manager_read_attr_resp_fn},
    {ZB_MANAGER_DEV_ANNCE_EVENT, zb_manager_dev_annce_event_fn},
    {ZB_MANAGER_DEV_ASSOCIATED_EVENT, zb_manager_dev_assoc_event_fn},
    {ZB_MANAGER_DEV_UPDATE_EVENT, zb_manager_dev_update_event_fn},
    {ZB_MANAGER_DEV_AUTH_EVENT, zb_manager_dev_auth_event_fn},
    {ZB_MANAGER_ACTIVE_EP_RESP, zb_manager_active_ep_resp_fn},
    {ZB_MANAGER_SIMPLE_DESC_RESP, zb_manager_simple_desc_resp_fn},
    {ZB_MANAGER_NODE_DESC_RSP, zb_manager_node_desc_resp_fn},
    {ZB_MANAGER_REPORT_CONFIG_RESP, zb_manager_report_config_resp_fn},
    {ZB_MANAGER_CUSTOM_CLUSTER_REPORT , zb_manager_custom_cluster_rep_event_fn }, 
    {ZB_MANAGER_DISCOVERY_ATTR_RESP, zb_manager_disc_attr_resp_fn},
    {ZB_MANAGER_NOSTANDART_CLUSTER_CMD_REPORT, zb_manager_nostandart_cluster_cmd_resp_fn},
};

esp_err_t esp_host_zb_input(esp_host_header_t *host_header, const void *buffer, uint16_t len)
{
    QueueHandle_t queue = (host_header->flags.type == ESP_ZNSP_TYPE_NOTIFY) ? notify_queue : output_queue;
    BaseType_t ret = 0;
    esp_host_zb_ctx_t host_ctx = {
        .id = host_header->id,
        .size = len,
    };

    if (buffer) {
        host_ctx.data = calloc(1, len);
        memcpy(host_ctx.data, buffer, len);
    }

    if (xPortInIsrContext() == pdTRUE) {
        ret = xQueueSendFromISR(queue, &host_ctx, NULL);
    } else {
        ret = xQueueSend(queue, &host_ctx, 0);
    }
    return (ret == pdTRUE) ? ESP_OK : ESP_FAIL ;
}

esp_err_t esp_host_zb_output(uint16_t id, const void *buffer, uint16_t len, void *output, uint16_t *outlen)
{
    
    
        esp_host_header_t data_header = {
        .id = id,
        .sn = esp_random() % 0xFF,
        .len = len,
        .flags = {
            .version = 0,
            }
        };
        data_header.flags.type = ESP_ZNSP_TYPE_REQUEST;

        xSemaphoreTakeRecursive(lock_semaphore, portMAX_DELAY);
        esp_host_frame_output(&data_header, buffer, len);

        esp_host_zb_ctx_t host_ctx;
        xQueueReceive(output_queue, &host_ctx, portMAX_DELAY);
        if (host_ctx.data) {
            if ((host_ctx.id == id)) {
                if (output) {
                    memcpy(output, host_ctx.data, host_ctx.size);
                }

                if (outlen) {
                    *outlen = host_ctx.size;
                }
            }

            free(host_ctx.data);
            host_ctx.data = NULL;
        }
        xSemaphoreGiveRecursive(lock_semaphore);
    
    return  ESP_OK;
}

/*void *esp_zb_app_signal_get_params(uint32_t *signal_p)
{
    local_esp_zb_app_signal_msg_t *app_signal_msg = (local_esp_zb_app_signal_msg_t *)signal_p;

    return app_signal_msg ? (void *)app_signal_msg->msg : (void *)app_signal_msg;
}*/

void esp_zb_stack_cleanup(void)
{
    if (output_queue) {
        vQueueDelete(output_queue);
        output_queue = NULL;
        ESP_LOGD(TAG, "✅ Cleaned up output_queue");
    }
    if (notify_queue) {
        vQueueDelete(notify_queue);
        notify_queue = NULL;
        ESP_LOGD(TAG, "✅ Cleaned up notify_queue");
    }
    if (lock_semaphore) {
        vSemaphoreDelete(lock_semaphore);
        lock_semaphore = NULL;
        ESP_LOGD(TAG, "✅ Cleaned up lock_semaphore");
    }
}

void esp_zb_stack_main_loop(void)
{
    esp_host_zb_ctx_t host_ctx;
    while (1) {
        // Проверяем, не пришло ли уведомление о рестарте NCP
        if (zigbee_ncp_module_state == WORKING)
        {
            uint32_t notify_value = ulTaskNotifyTake(pdTRUE, 100); // ждём 100 мс
            if (notify_value > 0 || g_zigbee_restarting) {
                ESP_LOGW(TAG, "🔄 Zigbee stack main loop interrupted for restart");
                zb_manager_ncp_host_restart_on_ncp_foulture();
                if (host_ctx.data) {
                free(host_ctx.data);
                host_ctx.data = NULL;
                }
                continue; // выходим из цикла
            }
        }

       if (xQueueReceive(notify_queue, &host_ctx, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
       }

       for (int i = 0; i <= sizeof(host_zb_func_table) / sizeof(host_zb_func_table[0]); i ++) {
            if (host_ctx.id != host_zb_func_table[i].id) {
                continue;
            }

            host_zb_func_table[i].set_func(host_ctx.data, host_ctx.size);
            break;
        }

        if (host_ctx.data) {
            free(host_ctx.data);
            host_ctx.data = NULL;
        }
    }
}

void esp_zb_main_loop_iteration(void)
{
    esp_zb_stack_main_loop();
}

esp_err_t esp_zb_device_register(esp_zb_ep_list_t *ep_list)
{
    return ESP_OK;
}

esp_err_t esp_zb_platform_config(esp_zb_platform_config_t *config)
{
    ESP_LOGI(TAG, "start esp_zb_platform_config -> cleanup");
    //esp_zb_stack_cleanup();  //

    ESP_LOGI(TAG, "start esp_zb_platform_config -> esp_host_init");
    ESP_ERROR_CHECK(esp_host_init(config->host_config.host_mode));
    ESP_LOGI(TAG, "start esp_zb_platform_config -> esp_host_start");
    ESP_ERROR_CHECK(esp_host_start());

    ESP_LOGI(TAG, "start esp_zb_platform_config -> output_queue = xQueueCreate");
    output_queue = xQueueCreate(HOST_EVENT_QUEUE_LEN, sizeof(esp_host_zb_ctx_t));
    ESP_LOGI(TAG, "start esp_zb_platform_config -> notify_queue = xQueueCreate");
    notify_queue = xQueueCreate(HOST_EVENT_QUEUE_LEN, sizeof(esp_host_zb_ctx_t));
    ESP_LOGI(TAG, "start esp_zb_platform_config -> lock_semaphore = xSemaphoreCreateRecursiveMutex");
    lock_semaphore = xSemaphoreCreateRecursiveMutex();

    return ESP_OK;
}


