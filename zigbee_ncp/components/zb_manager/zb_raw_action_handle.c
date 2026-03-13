
#include "esp_log.h"
#include "zboss_api.h"
#include "zb_raw_action_handle.h"
#include "zb_manager_tuya_helper.h"
#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "zb_manager_lqi_rssi_cache.h"
#include "esp_random.h"

static const char *TAG = "ZB_RAW_ACTION_HANDLER_MODULE";

uint8_t BUTTON_STATE_VM_TEST = 0;

typedef struct button_device_params_s {
    esp_zb_ieee_addr_t ieee_addr;
    uint8_t  endpoint;
    uint16_t short_addr;
} button_device_params_t;

typedef struct zdo_info_ctx_s {
    uint8_t endpoint;
    uint16_t short_addr;
} zdo_info_user_ctx_t;

button_device_params_t button;
/*static void bind_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx)
{
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Bind response from address(0x%x), endpoint(%d) with status(%d)", button.short_addr,
                 button.endpoint, zdo_status);
    }
}
    */
/*static void switch_ieee_cb(esp_zb_zdp_status_t zdo_status, esp_zb_ieee_addr_t ieee_addr, void *user_ctx)
{
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        
        ESP_LOGI(TAG, "swichIEEE address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                 ieee_addr[7], ieee_addr[6], ieee_addr[5], ieee_addr[4],
                 ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]);
       */
        /* биндим кнопку на свич */
        /*esp_zb_zdo_bind_req_param_t bind_req;
        memcpy(&(bind_req.src_address), button.ieee_addr, sizeof(esp_zb_ieee_addr_t));
        bind_req.src_endp = button.endpoint;
        bind_req.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF;
        bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
        memcpy(&(bind_req.dst_address_u.addr_long), ieee_addr, sizeof(esp_zb_ieee_addr_t));
        esp_zb_get_long_address(bind_req.dst_address_u.addr_long);
        bind_req.dst_endp = 1;
        bind_req.req_dst_addr = button.short_addr;
        //static zdo_info_user_ctx_t test_info_ctx;
        //test_info_ctx.endpoint = HA_ONOFF_SWITCH_ENDPOINT;
        //test_info_ctx.short_addr = on_off_light.short_addr;
        esp_zb_zdo_device_bind_req(&bind_req, bind_cb, NULL);
    }
}*/
/*static void btn_ieee_cb(esp_zb_zdp_status_t zdo_status, esp_zb_ieee_addr_t ieee_addr, void *user_ctx)
{
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        memcpy(&(button.ieee_addr), ieee_addr, sizeof(esp_zb_ieee_addr_t));
        ESP_LOGI(TAG, "button IEEE address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                 ieee_addr[7], ieee_addr[6], ieee_addr[5], ieee_addr[4],
                 ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]);
        // получаем длинный свича
        esp_zb_zdo_ieee_addr_req_param_t ieee_req;
        ieee_req.addr_of_interest = 0xb898;
        ieee_req.dst_nwk_addr = 0xb898;
        ieee_req.request_type = 0;
        ieee_req.start_index = 0;
        esp_zb_zdo_ieee_addr_req(&ieee_req, switch_ieee_cb, NULL);*/
        /* bind the on-off light to on-off switch */
        /*esp_zb_zdo_bind_req_param_t bind_req;
        memcpy(&(bind_req.src_address), button.ieee_addr, sizeof(esp_zb_ieee_addr_t));
        bind_req.src_endp = button.endpoint;
        bind_req.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF;
        bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
        esp_zb_get_long_address(bind_req.dst_address_u.addr_long);
        //bind_req.dst_endp = HA_ONOFF_SWITCH_ENDPOINT;
        //bind_req.req_dst_addr = on_off_light.short_addr;
        static zdo_info_user_ctx_t test_info_ctx;
        //test_info_ctx.endpoint = HA_ONOFF_SWITCH_ENDPOINT;
        //test_info_ctx.short_addr = on_off_light.short_addr;
        //esp_zb_zdo_device_bind_req(&bind_req, bind_cb, (void *) & (test_info_ctx));
    }
}*/

void send_read_attr_response(zb_uint8_t param)
{
    //zb_zcl_parsed_hdr_t *cmd_info = ZB_ZCL_PARSED_HDR_GET(param);
    zb_bufid_t zb_buf = param;
    zb_zcl_parsed_hdr_t *cmd_info = ZB_BUF_GET_PARAM(zb_buf, zb_zcl_parsed_hdr_t);
    uint16_t data_len = zb_buf_len(zb_buf);
    uint8_t data[data_len];
    memcpy(&data, zb_buf_begin(zb_buf), sizeof(data));
    ESP_LOGI(TAG,"send_read_attr_response");
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, &data, data_len, ESP_LOG_INFO);

    zb_bufid_t buf_id = zb_buf_get(ZB_TRUE, sizeof(zb_zcl_read_attr_res_t)); // Резервный размер

    if (!buf_id) {
        ESP_LOGE(TAG, "Failed to allocate buffer!");
        zb_buf_free(param);
        return;
    }
    
    char *manufacturer_name = "Espressif";
    
    zb_zcl_read_attr_res_t *resp = zb_buf_initial_alloc(buf_id, sizeof(zb_zcl_read_attr_res_t) + strlen(manufacturer_name) + 1);
    memcpy(resp->attr_value, manufacturer_name, strlen(manufacturer_name) + 1);
    resp->status = ZB_ZCL_STATUS_SUCCESS;
    resp->attr_id = ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID; // Пример атрибута
    resp->attr_type = ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING;
    
    // Отправляем ответ
    zb_addr_u addr;
    addr.addr_short = cmd_info->addr_data.common_data.source.u.short_addr;
    //addr.addr_short = 0x0000;
    //memcpy(&(addr.addr_long), &cmd_info->addr_data.common_data.source.u.ieee_addr, sizeof(esp_zb_ieee_addr_t));
    //zb_get_long_address(addr.addr_long);
    /*zb_zcl_send_cmd_tsn(
        buf_id,
        &addr,                  // Адрес отправителя (цель)
        ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        //ZB_APS_ADDR_MODE_64_ENDP_PRESENT,     // Используем 64-битный адрес
        cmd_info->addr_data.common_data.src_endpoint,               // Эндпоинт отправителя
        ZB_ZCL_FRAME_DIRECTION_TO_CLI,              // Ответ клиенту
        cmd_info->addr_data.common_data.dst_endpoint,               // Эндпоинт получателя
        resp,                                 // Данные ответа
        sizeof(*resp) + strlen(manufacturer_name) + 1, // Размер данных
        NULL,
        ZB_ZCL_CLUSTER_ID_BASIC,          // Кластер
        ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
        ZB_ZCL_CMD_READ_ATTRIB_RESP,       // Команда: Read Attributes Response
        0,                                    // TSN (автоматически назначен)
        NULL
    );*/
    //zb_buf_free(param);
    //zb_buf_free(buf_id ); //
    //return true;
}

bool raw_command_handler(uint8_t bufid)
{
    zb_bufid_t zb_buf = bufid;
    zb_zcl_parsed_hdr_t *cmd_info = ZB_BUF_GET_PARAM(zb_buf, zb_zcl_parsed_hdr_t);

    // Получаем payload и его длину
    uint8_t *payload = zb_buf_begin(zb_buf);
    uint16_t payload_len = zb_buf_len(zb_buf); // Уже только данные!

    // Проверка: не больше 64 байт
    if (payload_len > 64) {
        ESP_LOGW(TAG, "Payload too long (%u > 64), truncating", payload_len);
        payload_len = 64;
    }

    // Фильтр: только vendor-specific команды
    if (cmd_info->is_common_command == false) {
        ESP_LOGI(TAG, "📨 Intercepted non-standard cluster command: cluster=0x%04x, cmd=0x%02x, payload_len=%u",
                 cmd_info->cluster_id, cmd_info->cmd_id, payload_len);

        // Логируем payload — вы уже это делаете
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, payload, payload_len, ESP_LOG_INFO);

        // Подготавливаем структуру
        zb_manager_cmd_nostandart_cluster_resp_packed_message_t report = {0};

        report.status = ESP_ZB_ZCL_STATUS_SUCCESS;
        report.src_address.u.short_addr = cmd_info->addr_data.common_data.source.u.short_addr;
        report.src_endpoint = cmd_info->addr_data.common_data.src_endpoint;
        report.dst_endpoint = cmd_info->addr_data.common_data.dst_endpoint;
        report.cluster = cmd_info->cluster_id;
        report.cmd_id = cmd_info->cmd_id;
        report.cmd_payload_len = (uint8_t)payload_len;
        memcpy(report.cmd_payload, payload, payload_len);

        // NCP заголовок
        esp_ncp_header_t ncp_header = {
            .sn = esp_random() % 0xFF,
            .id = ZB_MANAGER_NOSTANDART_CLUSTER_CMD_REPORT,
        };

        // Отправляем на хост
        esp_ncp_noti_input(&ncp_header, &report, sizeof(report));

        ESP_LOGI(TAG, "✅ Sent to host: src_short=0x%04x, ep=%d, cluster=0x%04x, cmd=0x%02x, payload_len=%u",
                 report.src_address.u.short_addr,
                 report.src_endpoint,
                 report.cluster,
                 report.cmd_id,
                 report.cmd_payload_len);

        //zb_buf_free(zb_buf);
        //return true; // ⛔ Блокируем стандартную обработку
    }

    //zb_buf_free(zb_buf);
    return false;
}

/********************* */
bool raw_command_handler_temp_working(uint8_t bufid)
{
    zb_bufid_t zb_buf = bufid;
    uint16_t data_len = zb_buf_len(zb_buf);
    uint8_t data[data_len];
    memcpy(&data, zb_buf_begin(zb_buf), sizeof(data));

    zb_zcl_parsed_hdr_t *cmd_info = ZB_BUF_GET_PARAM(zb_buf, zb_zcl_parsed_hdr_t);
    ESP_LOGI(TAG, "ZCL cmd: clid=0x%04x, cmd=0x%02x, src=0x%04x, ep=%u, len=%u, manuf=0x%04x",
             cmd_info->cluster_id, cmd_info->cmd_id,
             cmd_info->addr_data.common_data.source.u.short_addr,
             cmd_info->addr_data.common_data.src_endpoint,
             zb_buf_len(zb_buf),
             cmd_info->manuf_specific);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, &data, data_len, ESP_LOG_INFO);
        
    update_device_rssi(cmd_info->addr_data.common_data.source.u.short_addr, cmd_info->rssi);

    // Проверяем: Tuya DP?
    /*if (cmd_info->cluster_id == 0xEF00 && cmd_info->cmd_id == 0x02) {
        // Определяем длину ZCL header
        //uint8_t zcl_header_len = 3; // базовый: Frame Control + TSN + Command ID
        //if (cmd_info->manuf_specific) {
            //zcl_header_len += 2; // + Manufacturer Code
        //}

        //uint16_t total_len = zb_buf_len(zb_buf);
        //if (zcl_header_len >= total_len) {
        //    ESP_LOGW(TAG, "ZCL header too long: %u >= %u", zcl_header_len, total_len);
        //    zb_buf_free(zb_buf);
        //    return true;
        //}

       // uint8_t *payload = zb_buf_begin(zb_buf) + zcl_header_len;
        //uint16_t payload_len = total_len - zcl_header_len;

        uint8_t lqi = 0;
        int8_t rssi = cmd_info->rssi;
        get_device_lqi_rssi(cmd_info->addr_data.common_data.source.u.short_addr, &lqi, &rssi);

        uint8_t* data_for_parser = NULL;
        data_for_parser = calloc(data_len, sizeof(uint8_t));
        if (data_for_parser == NULL) {
            ESP_LOGW(TAG, "❌ TUYA DP: parsing failed");
            zb_buf_free(zb_buf);
            return true;
        }
        memcpy(data_for_parser, data, data_len);

        //ESP_LOGI(TAG, "Tuya DP payload (len=%u):", payload_len);
        ESP_LOGI(TAG, "Tuya DP payload (len=%u):", data_len);
        if (parse_and_send_tuya_dp(
                cmd_info->addr_data.common_data.source.u.short_addr,
                cmd_info->addr_data.common_data.src_endpoint,
                data_for_parser, data_len,
                rssi, lqi) == ESP_OK) {
            ESP_LOGI(TAG, "✅ TUYA DP: parsed and sent");
        } else {
            ESP_LOGW(TAG, "❌ TUYA DP: parsing failed");
        }
        free(data_for_parser);
        data_for_parser = NULL;
        zb_buf_free(zb_buf);
        return true;
    }*/

    return false;
}

bool raw_command_handler_old(uint8_t bufid)
{
    zb_bufid_t zb_buf = bufid;
    uint16_t data_len = zb_buf_len(zb_buf);
    uint8_t data[data_len];
    zb_zcl_parsed_hdr_t *cmd_info = ZB_BUF_GET_PARAM(zb_buf, zb_zcl_parsed_hdr_t);
    memcpy(&data, zb_buf_begin(zb_buf), sizeof(data));
    /* обновляем RSSI*/
    update_device_rssi((uint16_t)cmd_info->addr_data.common_data.source.u.short_addr, (int8_t)cmd_info->rssi);



    raw_data_ind_t raw_data;
    raw_data.seq_number = (uint8_t)cmd_info->seq_number;
    raw_data.profile_id = (uint16_t)cmd_info->profile_id;
    raw_data.dst_short_addr = (uint16_t)cmd_info->addr_data.common_data.dst_addr;
    raw_data.dst_endpoint  = (uint8_t)cmd_info->addr_data.common_data.dst_endpoint;
    raw_data.src_short_addr = (uint16_t)cmd_info->addr_data.common_data.source.u.short_addr;
    raw_data.src_endpoint = (uint8_t)cmd_info->addr_data.common_data.src_endpoint;
    raw_data.cluster_id = (uint16_t)cmd_info->cluster_id;
    raw_data.is_common_command = (bool)cmd_info->is_common_command;
    raw_data.cmd_id = (uint8_t)cmd_info->cmd_id;
    raw_data.data_length = data_len;
    raw_data.manuf_code = (uint16_t)cmd_info->manuf_specific;
    raw_data.rssi = (int8_t)cmd_info->rssi;
    //memcpy(&raw_data.data, &data, data_len);


    ESP_LOGI(TAG,"RAW_DATA_INDICATION profileId: %04x clusterId: %04x  is_common_cmd: %d cmdId: 0x%02x srcAddr: %04x srcEndpoint: %02x dstAddr: %04x  dstEndpoint: %02x dataLen: %d manuf_code: %04x",
                raw_data.profile_id, raw_data.cluster_id,raw_data.is_common_command, raw_data.cmd_id, raw_data.src_short_addr, raw_data.src_endpoint, 
                raw_data.dst_short_addr, raw_data.dst_endpoint, (int)raw_data.data_length, raw_data.manuf_code);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, &data, raw_data.data_length, ESP_LOG_INFO);

    // ЕСЛИ ЭТО TUYA DP - шлём репорт на хост
    if (cmd_info->cluster_id == 0xEF00 && cmd_info->cmd_id == 0x02) {
        uint8_t lqi = 0;
        int8_t rssi = 0;
        get_device_lqi_rssi(cmd_info->addr_data.common_data.source.u.short_addr, &lqi, &rssi);

        parse_and_send_tuya_dp(
            cmd_info->addr_data.common_data.source.u.short_addr,
            cmd_info->addr_data.common_data.src_endpoint,
            data, data_len,
            rssi, lqi
        );
        ESP_LOGW(TAG,"RAW_DATA_INDICATION profileId ЭТО TUYA DP )");
        zb_buf_free(zb_buf);
        return true; // блокируем стандартную обработку
    }
    /*void zb_zcl_send_cmd_tsn(
  zb_uint8_t param,
  const zb_addr_u *dst_addr,
  zb_aps_addr_mode_t dst_addr_mode,
  zb_uint8_t dst_ep,
  zb_zcl_frame_direction_t direction,
  zb_uint8_t src_ep,
  const void *payload,
  zb_uint8_t payload_size,
  zb_zcl_put_payload_cb_t put_payload,
  zb_zcl_cluster_id_t cluster_id,
  zb_zcl_disable_default_response_t def_resp,
  zb_uint8_t cmd_id,
  zb_uint8_t tsn,
  zb_callback_t cb
);*/
//zb_zcl_read_attr_res_t
//

//
    //zb_buf_free(zb_buf);
    return false;
    /********** */
}