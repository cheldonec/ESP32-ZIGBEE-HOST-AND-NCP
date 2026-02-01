#include "zb_manager_devices.h"
#include "string.h"

static const char *TAG = "zb_manager_devices_module";

/**
 * @brief Функция сравнения двух IEEE-адресов для использования с lfind/bsearch
 * 
 * @param a Указатель на первый IEEE-адрес (const void*)
 * @param b Указатель на второй IEEE-адрес (const void*)
 * @return int 0 если адреса равны, не ноль если различаются
 */
int ieee_addr_compare(esp_zb_ieee_addr_t *a, esp_zb_ieee_addr_t *b)
{
    return memcmp((const uint8_t *)a, (const uint8_t *)b, sizeof(esp_zb_ieee_addr_t));
}

uint8_t RemoteDevicesCount = 0;
device_custom_t** RemoteDevicesArray = NULL;

uint8_t DeviceAppendShedulerCount = 0;
device_appending_sheduler_t** DeviceAppendShedulerArray;

esp_err_t zb_manager_devices_init(void)
{
    RemoteDevicesCount = REMOTE_DEVICES_COUNT;
    RemoteDevicesArray = calloc(RemoteDevicesCount, sizeof(device_custom_t*));
    if (RemoteDevicesArray == NULL) return ESP_FAIL;
    
    for (int i = 0; i < RemoteDevicesCount; i++) RemoteDevicesArray[i] = NULL;

    DeviceAppendShedulerCount = 5;
    DeviceAppendShedulerArray = calloc(DeviceAppendShedulerCount, sizeof(device_appending_sheduler_t*));
    if(DeviceAppendShedulerArray == NULL) return ESP_FAIL;
    for (int i = 0; i < DeviceAppendShedulerCount; i++) DeviceAppendShedulerArray[i] = NULL;
    
    return ESP_OK;
}

esp_err_t zb_manager_delete_appending_sheduler(device_appending_sheduler_t* sheduler)
{
    if(sheduler != NULL)
    {
        free(sheduler->simple_desc_req_list);
        sheduler->simple_desc_req_list = NULL;
        free(sheduler->simple_desc_req_simple_desc_req_list_status);
        sheduler->simple_desc_req_simple_desc_req_list_status = NULL;
        free(sheduler);
        sheduler = NULL;
    }
    return ESP_OK;
}

endpoint_custom_t* RemoteDeviceEndpointCreate(uint8_t ep_id)
{
    endpoint_custom_t* new_ep = NULL;
    new_ep = calloc(1,sizeof(endpoint_custom_t));
    if (new_ep==NULL) return NULL;
    new_ep->ep_id = ep_id;
    new_ep->is_use_on_device = 0;
    //new_ep->friendly_name_len = 0;
    //new_ep->friendly_name = NULL;
    //new_ep->deviceId = device_id;
    //new_ep->owner_dev_short = owner_dev_short;
    new_ep->UnKnowninputClusterCount = 0;
    new_ep->UnKnowninputClusters_array = NULL;
    new_ep->UnKnownoutputClusterCount = 0;
    new_ep->UnKnownoutputClusters_array = NULL;
    new_ep->is_use_basic_cluster = 0;
    new_ep->server_BasicClusterObj = NULL;
    new_ep->is_use_identify_cluster = 0;
    new_ep->server_IdentifyClusterObj = NULL;
    new_ep->is_use_temperature_measurement_cluster = 0;
    new_ep->server_TemperatureMeasurementClusterObj = NULL;
    return new_ep;
}

esp_err_t RemoteDeviceEndpointDelete(endpoint_custom_t* ep_object)
{
    ESP_LOGW(TAG, "RemoteDeviceEndpointDelete  Функция не реализована!!!!!!!!!!");
    return ESP_OK;
}

device_custom_t*   RemoteDeviceCreate(esp_zb_ieee_addr_t ieee_addr)
{
    device_custom_t* new_dev = NULL;
    new_dev = calloc(1,sizeof(device_custom_t));
    if(new_dev == NULL) return NULL; //
    memcpy(new_dev->ieee_addr, ieee_addr, sizeof(esp_zb_ieee_addr_t));
    new_dev->is_in_build_status = 1;
    //new_dev->manuf_name_len = 0;
    //new_dev->manuf_name = NULL;
    new_dev->friendly_name_len = sizeof(new_dev->friendly_name);
    //new_dev->friendly_name = NULL;
    new_dev->short_addr = 0xffff;
    new_dev->capability = 0;
    new_dev->endpoints_count = 0;
    new_dev->endpoints_array = NULL;
    //new_dev->dev_ieee_addr = ieee_addr;
    return new_dev;
}
/**************************************************** Temperature Sensor *************************************************************/
zb_manager_temperature_sensor_ep_t* temp_sensor_ep_create(void)
{
    zigbee_manager_basic_cluster_t                  basic_cluster       = ZIGBEE_BASIC_CLUSTER_DEFAULT_INIT();
    zb_manager_identify_cluster_t                   identify_cluster    = ZIGBEE_IDENTIFY_CLUSTER_DEFAULT_INIT();
    zb_manager_temperature_measurement_cluster_t    temp_server_cluster = ZIGBEE_TEMP_MEASUREMENT_CLUSTER_DEFAULT_INIT();
    zb_manager_temperature_sensor_ep_t* result = calloc(1,sizeof(zb_manager_temperature_sensor_ep_t));
    result->dev_ieee_addr[7] = 0x00;
    result->dev_ieee_addr[6] = 0x00;
    result->dev_ieee_addr[5] = 0x00;
    result->dev_ieee_addr[4] = 0x00;
    result->dev_ieee_addr[3] = 0x00;
    result->dev_ieee_addr[2] = 0x00;
    result->dev_ieee_addr[1] = 0x00;
    result->dev_ieee_addr[0] = 0x00;
    result->dev_endpoint = 0x01;
    result->dev_basic_cluster = &basic_cluster;
    result->dev_identify_cluster = &identify_cluster;
    result->dev_temperature_measurement_server_cluster = &temp_server_cluster;
    return result;
}

esp_err_t temp_sensor_ep_delete(zb_manager_temperature_sensor_ep_t* ep)
{
    esp_err_t err = ESP_FAIL;
    free(ep->dev_basic_cluster);
    free(ep->dev_identify_cluster);
    free(ep->dev_temperature_measurement_server_cluster);
    ep->dev_basic_cluster = NULL;
    ep->dev_identify_cluster = NULL;
    ep->dev_temperature_measurement_server_cluster = NULL;
    free(ep);
    ep = NULL;
    err = ESP_OK;
    return err;
}