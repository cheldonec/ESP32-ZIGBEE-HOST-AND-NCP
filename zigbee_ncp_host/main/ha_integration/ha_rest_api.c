#include "ha_rest_api.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "zb_manager_devices.h"  // RemoteDevicesArray
#include "quirks_storage.h"

static const char* TAG = "ha_rest";
static httpd_handle_t server = NULL;

esp_err_t ha_devices_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < RemoteDevicesCount; i++) {
        device_custom_t *dev = RemoteDevicesArray[i];
        if (!dev || !dev->is_online) continue;
        for (int j = 0; j < dev->endpoints_count; j++) {
            endpoint_custom_t *ep = dev->endpoints_array[j];
            if (!ep || !ep->is_use_on_off_cluster || !ep->server_OnOffClusterObj) continue;

            cJSON *entity = cJSON_CreateObject();
            char unique_id[64];
            snprintf(unique_id, sizeof(unique_id), "esp32_zb_%04x_ep%d", dev->short_addr, ep->ep_id);
            cJSON_AddStringToObject(entity, "name", dev->friendly_name[0] ? dev->friendly_name : "Unknown");
            cJSON_AddStringToObject(entity, "state", ep->server_OnOffClusterObj->on_off ? "on" : "off");
            cJSON_AddStringToObject(entity, "device_class", "switch");
            cJSON_AddStringToObject(entity, "platform", "rest");
            cJSON_AddStringToObject(entity, "unique_id", unique_id);
            cJSON_AddItemToArray(root, entity);
        }
    }

    char *rendered = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, rendered);
    free(rendered);
    cJSON_Delete(root);
    return ESP_OK;
}

void ha_rest_api_init(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    //if (httpd_start(&server, &config) == ESP_OK) {
        /*httpd_register_uri_handler(server, &(httpd_uri_t){
            .uri = "/api/ha/devices",
            .method = HTTP_GET,
            .handler = ha_devices_handler,
        });*/
        ESP_LOGI(TAG, "REST API started at /api/ha/devices");
   // }
}
