#include "zbm_dev_base.h"
#include "zbm_dev_base_utils.h"
#include "zbm_dev_polling.h"
#include "zbm_dev_base_dev_update.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "spiffs_helper.h"
#include "esp_log.h"
#include "zbm_dev_polling.h"
#include "zbm_dev_append_sheduler.h"

//#include "zb_manager_clusters.h"


static const char *TAG = "ZBM_DEV_BASE_MODULE";

//=== МАКРОСЫ ДЛЯ JSON ===
// === Утилиты для загрузки из JSON ===
#define LOAD_NUMBER(json_obj, key, dst)                 \
    do {                                                \
        cJSON *item = cJSON_GetObjectItem(json_obj, key); \
        if (item && cJSON_IsNumber(item)) {             \
            dst = (typeof(dst))item->valueint;          \
        }                                               \
    } while(0)

#define LOAD_STRING(json_obj, key, dst)                 \
    do {                                                \
        cJSON *item = cJSON_GetObjectItem(json_obj, key); \
        if (item && cJSON_IsString(item) && item->valuestring) { \
            strncpy(dst, item->valuestring, sizeof(dst) - 1); \
            dst[sizeof(dst) - 1] = '\0';                \
        }                                               \
    } while(0)

static SemaphoreHandle_t zbm_g_device_array_mutex = NULL; // мьютекс для защиты массива устройств

static QueueHandle_t zbm_base_dev_save_to_file_cmd_queue = NULL; // очередь для сохранения устройств в файл

static uint8_t zbm_RemoteDevicesCount = 0;

static device_custom_t** zbm_RemoteDevicesArray = NULL;

// Буфер под служебные данные задачи
    static StaticTask_t xZB_SaveTaskBuffer;
    // Буфер под стек задачи
    static StackType_t xZB_SaveTaskStack[SAVE_TASK_STACK_SIZE];

    static TaskHandle_t xZB_SaveTaskHandle;
    TaskHandle_t xZB_SaveTaskHandle_link = NULL; // для управления xZB_Handle

esp_err_t           zbm_dev_base_init(uint8_t core_id);
static esp_err_t    zbm_dev_base_endpoint_delete(endpoint_custom_t* ep_object);
esp_err_t           zbm_dev_base_dev_delete_safe(device_custom_t* dev_object);
device_custom_t*    zbm_dev_base_create_new_device_obj(esp_zb_ieee_addr_t ieee_addr);
esp_err_t           zbm_dev_base_dev_obj_append_safe(device_custom_t* dev_object);
endpoint_custom_t*  zbm_dev_base_create_new_endpoint_obj(uint8_t ep_id);
device_custom_t*    zbm_dev_base_find_device_by_short_safe(uint16_t short_addr);
device_custom_t*    zbm_dev_base_find_device_by_long_safe(esp_zb_ieee_addr_t *ieee);

static void         zbm_dev_base_free_no_standart_attribute_array(attribute_custom_t **attr_array, uint16_t count);
static esp_err_t    zbm_dev_base_dev_delete(device_custom_t* dev_object);
cJSON               *zbm_dev_base_device_to_json(device_custom_t* dev);
cJSON               *zbm_dev_base_to_json_safe();
cJSON               *zbm_base_dev_short_list_for_webserver(void);
static void         zbm_save_task_result_cb(esp_err_t result, const char *file_path);
static void         zbm_dev_get_filename_by_ieee(const uint8_t *ieee_addr, char *buf, size_t buf_size); // new
static esp_err_t    zbm_dev_base_load_device_from_json(device_custom_t *dev, cJSON *dev_json); //new
static esp_err_t    zbm_dev_save_single_device(device_custom_t *dev);  // new
static esp_err_t    zbm_dev_base_save_new(const char *index_filepath);  //new
static esp_err_t    zbm_dev_base_save(const char *filepath);
static esp_err_t    zbm_dev_base_load_new(const char *index_filepath); // new
static esp_err_t    zbm_dev_base_load(const char *filepath);
esp_err_t           zbm_dev_base_queue_save_req_cmd();
// utils
void                ieee_to_str(char* out, const esp_zb_ieee_addr_t addr);
bool                str_to_ieee(const char* str, esp_zb_ieee_addr_t addr);
esp_err_t           zbm_dev_base_dev_update_from_read_response_safe(zb_manager_cmd_read_attr_resp_message_t* read_resp);
esp_err_t           zbm_dev_base_dev_update_from_report_notify_safe(zb_manager_cmd_report_attr_resp_message_t *rep);

esp_err_t           zbm_dev_base_dev_update_friendly_name(uint16_t short_addr, const char* name);
esp_err_t           zbm_dev_base_endpoint_update_friendly_name(uint16_t short_addr, uint8_t ep_id, const char* name);

//================================================================================================================================
//============================================================= ZBM_DEV_BASE_SAVE_TASK ===========================================
//================================================================================================================================
static void zbm_save_task_result_cb(esp_err_t result, const char *file_path)
{
    if (result == ESP_OK)
    {
        ESP_LOGD(TAG, "Devices successfully saved to %s", file_path);
        //json_load_and_print(ZB_MANAGER_JSON_DEVICES_FILE);
    } else {
        ESP_LOGE(TAG, "Failed to save devices to %s", file_path);
    }
}

static void zbm_save_task(void *pvParameters)
{
    uint8_t cmd;
    const char *file_path = (const char *)pvParameters;

    ESP_LOGI(TAG, "Zigbee Save Task started");

    while (1) {
        // Ожидаем команду
        if (xQueueReceive(zbm_base_dev_save_to_file_cmd_queue, &cmd, portMAX_DELAY) == pdPASS) {
            if (cmd == SAVE_TASK_CMD_SAVE) {
                ESP_LOGD(TAG, "Executing deferred save to JSON");
                esp_err_t err = zbm_dev_base_save_new(file_path);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Devices successfully saved to %s", file_path);
                    zbm_save_task_result_cb(ESP_OK, file_path);
                    
                } else {
                    ESP_LOGE(TAG, "Failed to save devices to %s", file_path);
                }
            }
        }
    }
}
//********************************************************************************************************************************

//================================================================================================================================
//============================================================= ZBM_DEV_BASE_INIT ================================================
//================================================================================================================================
esp_err_t zbm_dev_base_init(uint8_t core_id)
{
    zbm_RemoteDevicesCount = REMOTE_DEVICES_COUNT;
    if (zbm_RemoteDevicesArray == NULL)
    {
        zbm_RemoteDevicesArray = calloc(zbm_RemoteDevicesCount, sizeof(device_custom_t*));
    }
    if (zbm_RemoteDevicesArray == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for RemoteDevicesArray!");
        return ESP_FAIL;
    }
    
    for (int i = 0; i < zbm_RemoteDevicesCount; i++) zbm_RemoteDevicesArray[i] = NULL;

    // Создаём мьютекс для доступа к массиву устройств
    zbm_g_device_array_mutex = xSemaphoreCreateMutex();
    if (zbm_g_device_array_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create device array mutex");
        return ESP_FAIL;
    }

    // создаём очередь для команд сохранения
    if (zbm_base_dev_save_to_file_cmd_queue == NULL) {
        zbm_base_dev_save_to_file_cmd_queue = xQueueCreate(SAVE_TASK_CMD_QUEUE_SIZE, sizeof(uint8_t));
        if (zbm_base_dev_save_to_file_cmd_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create save command queue");
            return ESP_ERR_NO_MEM;
        }else{
            ESP_LOGW(TAG, "Save command queue created");
        }
    }
        // Создаём задачу сохранения
        xZB_SaveTaskHandle = xTaskCreateStaticPinnedToCore(zbm_save_task, "zbm_save_task", SAVE_TASK_STACK_SIZE, (void*)ZB_MANAGER_JSON_INDEX_FILE, SAVE_TASK_PRIORITY, xZB_SaveTaskStack, &xZB_SaveTaskBuffer, core_id);
        if (xZB_SaveTaskHandle == NULL)
        {
            ESP_LOGE(TAG, "Failed to create Zigbee task");
            return ESP_FAIL;
        }else
        {
            xZB_SaveTaskHandle_link = xZB_SaveTaskHandle; // сохраняем указатель для управления
        }

        //временная очистка
        esp_err_t load_base = ESP_FAIL;
        ESP_LOGW(TAG, "before zbm_dev_base_load_new");
        load_base = zbm_dev_base_load_new(ZB_MANAGER_JSON_INDEX_FILE); // ← используем индекс
        if (load_base != ESP_OK) 
        {
            ESP_LOGW(TAG, "zbm_dev_base_load_new failed, creating fresh index and device files...");
            esp_err_t save_err = zbm_dev_base_save_new(ZB_MANAGER_JSON_INDEX_FILE);
            if (save_err == ESP_OK)
            {
                ESP_LOGI(TAG, "Fresh device index and files created");
            }
            else 
            {
                ESP_LOGE(TAG, "Failed to create fresh device files");
                return ESP_FAIL;
            }
        }
    zbm_dev_pooling_init(30);
    return zbm_dev_pairing_shedulers_init();
}
//********************************************************************************************************************************

//================================================================================================================================
//========================================================= ZBM_DEV_BASE_ENDPOINT_DELETE =========================================
//================================================================================================================================
static esp_err_t zbm_dev_base_endpoint_delete(endpoint_custom_t* ep_object)
{
    ESP_LOGW(TAG, "RemoteDeviceEndpointDelete");
    if (!ep_object) return ESP_FAIL;

    free(ep_object->server_TemperatureMeasurementClusterObj);
    free(ep_object->server_HumidityMeasurementClusterObj);
    free(ep_object->server_OnOffClusterObj);
    free(ep_object->server_IdentifyClusterObj);
    free(ep_object->output_clusters_array);
    ep_object->output_clusters_array = NULL;
    ep_object->output_clusters_count = 0;
    //free(ep_object->server_LevelControlClusterObj);
    if(ep_object->UnKnowninputClusterCount != NULL)
    {
       for (int i = 0; i < ep_object->UnKnowninputClusterCount; i++) {
        cluster_custom_t *cl = ep_object->UnKnowninputClusters_array[i];
            if (cl) {
                // Освободи атрибуты, если они есть
                for (int j = 0; j < cl->attr_count; j++) {
                    free(cl->attr_array[j]->p_value);
                    free(cl->attr_array[j]);
                }
                free(cl->attr_array);
                free(cl);
            }
        }
        free(ep_object->UnKnowninputClusters_array); 
    }
    if(ep_object->UnKnownoutputClusters_array != NULL)
    {
       for (int i = 0; i < ep_object->UnKnownoutputClusterCount; i++) {
        cluster_custom_t *cl = ep_object->UnKnownoutputClusters_array[i];
            if (cl) {
                // Освободи атрибуты, если они есть
                for (int j = 0; j < cl->attr_count; j++) {
                    free(cl->attr_array[j]->p_value);
                    free(cl->attr_array[j]);
                }
                free(cl->attr_array);
                free(cl);
            }
        }
        free(ep_object->UnKnownoutputClusters_array); 
    }

    
    free(ep_object);
    ep_object = NULL;
    return ESP_OK;
}
//********************************************************************************************************************************

//================================================================================================================================
//======================================================= ZBM_DEV_BASE_DEV_DELETE ================================================
//================================================================================================================================
static void zbm_dev_base_free_no_standart_attribute_array(attribute_custom_t **attr_array, uint16_t count) {
    if (!attr_array) return;
    for (int i = 0; i < count; i++) {
        if (attr_array[i]) {
            if (attr_array[i]->p_value) free(attr_array[i]->p_value);
            free(attr_array[i]);
        }
    }
    free(attr_array);
}

static esp_err_t zbm_dev_base_dev_delete(device_custom_t* dev_object)
{
    if (dev_object->server_BasicClusterObj != NULL) {
        zbm_dev_base_free_no_standart_attribute_array(dev_object->server_BasicClusterObj->nostandart_attr_array,
                                      dev_object->server_BasicClusterObj->nostandart_attr_count);
        free(dev_object->server_BasicClusterObj);
        dev_object->server_BasicClusterObj = NULL;
    }
    if (dev_object->server_PowerConfigurationClusterObj != NULL) {
        zbm_dev_base_free_no_standart_attribute_array(dev_object->server_PowerConfigurationClusterObj->nostandart_attr_array,
                                      dev_object->server_PowerConfigurationClusterObj->nostandart_attr_count);
        free(dev_object->server_PowerConfigurationClusterObj);
        dev_object->server_PowerConfigurationClusterObj = NULL;
    }
    if (dev_object->endpoints_count > 0) {
        for (int j = 0; j < dev_object->endpoints_count; j++)
        {
            zbm_dev_base_endpoint_delete(dev_object->endpoints_array[j]);
        }
        dev_object->endpoints_count = 0;
        free (dev_object->endpoints_array);
        dev_object->endpoints_array = NULL;
    }
    free(dev_object);
    dev_object = NULL;
    return ESP_OK;
}
//********************************************************************************************************************************

//================================================================================================================================
//======================================================= ZBM_DEV_BASE_DEV_DELETE_SAFE ===========================================
//================================================================================================================================
esp_err_t  zbm_dev_base_dev_delete_safe(device_custom_t* dev_object)
{
    esp_err_t result = ESP_FAIL;
    if (dev_object == NULL) return ESP_ERR_INVALID_ARG;

    if (xSemaphoreTake(zbm_g_device_array_mutex, pdMS_TO_TICKS(ZBM_BASE_MUTEX_TIMEOUT_LONG_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex in zb_manager_get_full_json_from_remote_devices_array_safe");
        return ESP_FAIL;
    }
    uint8_t index = 0xff;
    device_custom_t *dev = NULL;
    for (int i = 0; i < zbm_RemoteDevicesCount; i++) {
        if (zbm_RemoteDevicesArray[i] == dev_object)
        {
            dev = zbm_RemoteDevicesArray[i];
            index = i;
            break;
        }
    }       
    if (dev && index != 0xff)
    {
        result = zbm_dev_base_dev_delete(dev_object);
        zbm_RemoteDevicesArray[index] = NULL;
    }
    
    xSemaphoreGive(zbm_g_device_array_mutex);
    return result;
}
//********************************************************************************************************************************

//================================================================================================================================
//================================================== ZBM_DEV_BASE_GET_FILENAME_FROM_IEEE =========================================
//================================================================================================================================
void zbm_dev_get_filename_by_ieee(const uint8_t *ieee_addr, char *buf, size_t buf_size)
{
    snprintf(buf, buf_size, "/spiffs_config/dev_%02x%02x%02x%02x%02x%02x%02x%02x.json",
             ieee_addr[7], ieee_addr[6], ieee_addr[5], ieee_addr[4],
             ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]);
}
//********************************************************************************************************************************

//================================================================================================================================
//================================================== ZBM_DEV_BASE_LOAD_DEV_FROM_JSON =============================================
//================================================================================================================================
/**
 * @brief Загружает поля устройства из cJSON объекта (используется при парсинге)
 * @note Не выделяет память под dev — ожидает, что dev уже выделен
 */
esp_err_t zbm_dev_base_load_device_from_json(device_custom_t *dev, cJSON *dev_json)
{
    if (!dev || !dev_json) return ESP_ERR_INVALID_ARG;

    // Уже заполнены: IEEE, short_addr, friendly_name — перед вызовом
    // Здесь продолжаем заполнять остальные поля

    dev->is_in_build_status = cJSON_GetObjectItem(dev_json, "is_in_build_status")->valueint;
    dev->index_in_array = cJSON_GetObjectItem(dev_json, "index_in_array")->valueint;
    dev->capability = cJSON_GetObjectItem(dev_json, "capability")->valueint;
    dev->lqi = cJSON_GetObjectItem(dev_json, "lqi")->valueint;

    cJSON *last_seen_item = cJSON_GetObjectItem(dev_json, "last_seen_ms");
    if (last_seen_item) {
        dev->last_seen_ms = 0;
        dev->is_online = false;
    }

    cJSON *timeout_item = cJSON_GetObjectItem(dev_json, "device_timeout_ms");
    if (timeout_item && cJSON_IsNumber(timeout_item)) {
        dev->device_timeout_ms = timeout_item->valueint;
    } else {
        dev->device_timeout_ms = ZB_DEVICE_DEFAULT_TIMEOUT_MS;
    }

    // Manufacturer code
    cJSON *manuf_code_obj = cJSON_GetObjectItem(dev_json, "manufacturer_code");
    if (manuf_code_obj) {
        dev->manufacturer_code = (uint16_t)manuf_code_obj->valueint;
    }

    // Device-level Basic Cluster
    cJSON *dev_basic = cJSON_GetObjectItem(dev_json, "device_basic_cluster");
    if (dev_basic) {
            dev->server_BasicClusterObj = calloc(1, sizeof(zigbee_manager_basic_cluster_t));
            if (dev->server_BasicClusterObj) {
                dev->server_BasicClusterObj->zcl_version = cJSON_GetObjectItem(dev_basic, "zcl_version")->valueint;
                dev->server_BasicClusterObj->application_version = cJSON_GetObjectItem(dev_basic, "app_version")->valueint;
                dev->server_BasicClusterObj->stack_version = cJSON_GetObjectItem(dev_basic, "stack_version")->valueint;
                dev->server_BasicClusterObj->hw_version = cJSON_GetObjectItem(dev_basic, "hw_version")->valueint;

                LOAD_STRING(dev_basic, "manufacturer_name", dev->server_BasicClusterObj->manufacturer_name);
                LOAD_STRING(dev_basic, "model_id", dev->server_BasicClusterObj->model_identifier);
                LOAD_STRING(dev_basic, "date_code", dev->server_BasicClusterObj->date_code);
                LOAD_STRING(dev_basic, "location", dev->server_BasicClusterObj->location_description);

                cJSON *ps_item = cJSON_GetObjectItem(dev_basic, "power_source");
                if (ps_item && cJSON_IsNumber(ps_item)) {
                    dev->server_BasicClusterObj->power_source = ps_item->valueint;
                }

                dev->server_BasicClusterObj->physical_environment = cJSON_GetObjectItem(dev_basic, "env")->valueint;
                dev->server_BasicClusterObj->device_enabled = cJSON_GetObjectItem(dev_basic, "enabled")->type == cJSON_True;

                cJSON *last_update_item = cJSON_GetObjectItem(dev_basic, "last_update_ms");
                if (last_update_item && cJSON_IsNumber(last_update_item)) {
                    dev->server_BasicClusterObj->last_update_ms = last_update_item->valueint;
                } else {
                    dev->server_BasicClusterObj->last_update_ms = esp_log_timestamp();
                }

                // === Загрузка нестандартных атрибутов ===
                cJSON *nostandart_attrs = cJSON_GetObjectItem(dev_basic, "nostandart_attributes");
                if (nostandart_attrs && cJSON_IsArray(nostandart_attrs)) {
                    int count = cJSON_GetArraySize(nostandart_attrs);
                    dev->server_BasicClusterObj->nostandart_attr_count = count;
                    dev->server_BasicClusterObj->nostandart_attr_array = calloc(count, sizeof(attribute_custom_t*));
                    if (dev->server_BasicClusterObj->nostandart_attr_array) {
                        bool alloc_failed = false;
                        for (int a = 0; a < count; a++) {
                            cJSON *attr_obj = cJSON_GetArrayItem(nostandart_attrs, a);
                            if (!attr_obj) continue;

                            attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
                            if (!attr) {
                                alloc_failed = true;
                                continue;
                            }

                            attr->id = cJSON_GetObjectItem(attr_obj, "id")->valueint;
                            LOAD_STRING(attr_obj, "attr_id_text", attr->attr_id_text);
                            attr->type = cJSON_GetObjectItem(attr_obj, "type")->valueint;
                            attr->acces = cJSON_GetObjectItem(attr_obj, "acces")->valueint;
                            attr->size = cJSON_GetObjectItem(attr_obj, "size")->valueint;
                            attr->is_void_pointer = cJSON_GetObjectItem(attr_obj, "is_void_pointer")->valueint;
                            attr->manuf_code = cJSON_GetObjectItem(attr_obj, "manuf_code")->valueint;
                            attr->parent_cluster_id = cJSON_GetObjectItem(attr_obj, "parent_cluster_id")->valueint;

                            // p_value — пока NULL, можно загрузить позже по типу
                            attr->p_value = NULL;

                            dev->server_BasicClusterObj->nostandart_attr_array[a] = attr;
                        }
                        if (alloc_failed) {
                            ESP_LOGW(TAG, "Partial failure loading nostandart_attributes for Basic cluster");
                        }
                    }
                }

            }
        }

    // Power Configuration Cluster
    cJSON *dev_power_config_item = cJSON_GetObjectItem(dev_json, "device_power_config_cluster");
    if (dev_power_config_item) {
            dev->server_PowerConfigurationClusterObj = calloc(1, sizeof(zb_manager_power_config_cluster_t));
            if (dev->server_PowerConfigurationClusterObj) {
                LOAD_NUMBER(dev_power_config_item, "battery_voltage", dev->server_PowerConfigurationClusterObj->battery_voltage);
                LOAD_NUMBER(dev_power_config_item, "battery_percentage", dev->server_PowerConfigurationClusterObj->battery_percentage);
                LOAD_STRING(dev_power_config_item, "battery_manufacturer", dev->server_PowerConfigurationClusterObj->battery_manufacturer);
                LOAD_NUMBER(dev_power_config_item, "battery_size", dev->server_PowerConfigurationClusterObj->battery_size);
                LOAD_NUMBER(dev_power_config_item, "battery_a_hr_rating", dev->server_PowerConfigurationClusterObj->battery_a_hr_rating);
                LOAD_NUMBER(dev_power_config_item, "battery_quantity", dev->server_PowerConfigurationClusterObj->battery_quantity);
                LOAD_NUMBER(dev_power_config_item, "battery_rated_voltage", dev->server_PowerConfigurationClusterObj->battery_rated_voltage);
                LOAD_NUMBER(dev_power_config_item, "battery_alarm_mask", dev->server_PowerConfigurationClusterObj->battery_alarm_mask);
                LOAD_NUMBER(dev_power_config_item, "battery_voltage_min_th", dev->server_PowerConfigurationClusterObj->battery_voltage_min_th);
                LOAD_NUMBER(dev_power_config_item, "battery_voltage_th1", dev->server_PowerConfigurationClusterObj->battery_voltage_th1);
                LOAD_NUMBER(dev_power_config_item, "battery_voltage_th2", dev->server_PowerConfigurationClusterObj->battery_voltage_th2);
                LOAD_NUMBER(dev_power_config_item, "battery_voltage_th3", dev->server_PowerConfigurationClusterObj->battery_voltage_th3);
                LOAD_NUMBER(dev_power_config_item, "battery_percentage_min_th", dev->server_PowerConfigurationClusterObj->battery_percentage_min_th);
                LOAD_NUMBER(dev_power_config_item, "battery_percentage_th1", dev->server_PowerConfigurationClusterObj->battery_percentage_th1);
                LOAD_NUMBER(dev_power_config_item, "battery_percentage_th2", dev->server_PowerConfigurationClusterObj->battery_percentage_th2);
                LOAD_NUMBER(dev_power_config_item, "battery_percentage_th3", dev->server_PowerConfigurationClusterObj->battery_percentage_th3);
                LOAD_NUMBER(dev_power_config_item, "battery_alarm_state", dev->server_PowerConfigurationClusterObj->battery_alarm_state);
                LOAD_NUMBER(dev_power_config_item, "mains_voltage", dev->server_PowerConfigurationClusterObj->mains_voltage);
                LOAD_NUMBER(dev_power_config_item, "mains_frequency", dev->server_PowerConfigurationClusterObj->mains_frequency);
                LOAD_NUMBER(dev_power_config_item, "mains_alarm_mask", dev->server_PowerConfigurationClusterObj->mains_alarm_mask);
                LOAD_NUMBER(dev_power_config_item, "mains_voltage_min_th", dev->server_PowerConfigurationClusterObj->mains_voltage_min_th);
                LOAD_NUMBER(dev_power_config_item, "mains_voltage_max_th", dev->server_PowerConfigurationClusterObj->mains_voltage_max_th);
                LOAD_NUMBER(dev_power_config_item, "mains_dwell_trip_point", dev->server_PowerConfigurationClusterObj->mains_dwell_trip_point);

                cJSON *last_update_item = cJSON_GetObjectItem(dev_power_config_item, "last_update_ms");
                if (last_update_item && cJSON_IsNumber(last_update_item)) {
                    dev->server_PowerConfigurationClusterObj->last_update_ms = last_update_item->valueint;
                } else {
                    dev->server_PowerConfigurationClusterObj->last_update_ms = esp_log_timestamp();
                }

                // === Загрузка нестандартных атрибутов ===
                cJSON *nostandart_attrs = cJSON_GetObjectItem(dev_power_config_item, "nostandart_attributes");
                if (nostandart_attrs && cJSON_IsArray(nostandart_attrs)) {
                    int count = cJSON_GetArraySize(nostandart_attrs);
                    dev->server_PowerConfigurationClusterObj->nostandart_attr_count = count;
                    dev->server_PowerConfigurationClusterObj->nostandart_attr_array = calloc(count, sizeof(attribute_custom_t*));
                    if (dev->server_PowerConfigurationClusterObj->nostandart_attr_array) {
                        bool alloc_failed = false;
                        for (int a = 0; a < count; a++) {
                            cJSON *attr_obj = cJSON_GetArrayItem(nostandart_attrs, a);
                            if (!attr_obj) continue;

                            attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
                            if (!attr) {
                                alloc_failed = true;
                                continue;
                            }

                            attr->id = cJSON_GetObjectItem(attr_obj, "id")->valueint;
                            LOAD_STRING(attr_obj, "attr_id_text", attr->attr_id_text);
                            attr->type = cJSON_GetObjectItem(attr_obj, "type")->valueint;
                            attr->acces = cJSON_GetObjectItem(attr_obj, "acces")->valueint;
                            attr->size = cJSON_GetObjectItem(attr_obj, "size")->valueint;
                            attr->is_void_pointer = cJSON_GetObjectItem(attr_obj, "is_void_pointer")->valueint;
                            attr->manuf_code = cJSON_GetObjectItem(attr_obj, "manuf_code")->valueint;
                            attr->parent_cluster_id = cJSON_GetObjectItem(attr_obj, "parent_cluster_id")->valueint;

                            // p_value — пока NULL, можно загрузить позже по типу
                            attr->p_value = NULL;

                            dev->server_PowerConfigurationClusterObj->nostandart_attr_array[a] = attr;
                        }
                        if (alloc_failed) {
                            ESP_LOGW(TAG, "Partial failure loading nostandart_attributes for Basic cluster");
                        }
                    }
                }

            }
        }

    // Endpoints
    dev->endpoints_count = cJSON_GetObjectItem(dev_json, "endpointscount")->valueint;
        if (dev->endpoints_count > 0)
        {
            cJSON *eps = cJSON_GetObjectItem(dev_json, "endpoints");
            if (cJSON_IsArray(eps)) {
                //dev->endpoints_count = cJSON_GetArraySize(eps);
                dev->endpoints_array = calloc(dev->endpoints_count, sizeof(endpoint_custom_t*));
                if (!dev->endpoints_array) {
                    ESP_LOGE(TAG, "Failed to allocate endpoints_array for device %02x:%02x...", dev->ieee_addr[7], dev->ieee_addr[6]);
                    //free(dev);
                    return ESP_FAIL;
                }

                for (int j = 0; j < dev->endpoints_count; j++) {
                    cJSON *ep_obj = NULL;
                    ep_obj = cJSON_GetArrayItem(eps, j);
                    if (!ep_obj) {
                        ESP_LOGW(TAG, "Missing endpoint at index %d", j);
                        continue;
                    }

                    endpoint_custom_t *ep = NULL;
                    ep = calloc(1, sizeof(endpoint_custom_t));
                    if (!ep) {
                        ESP_LOGE(TAG, "Failed to allocate memory for endpoint %d", j);
                        continue;
                    }

                    ep->ep_id = cJSON_GetObjectItem(ep_obj, "ep_id")->valueint;
                    ep->is_use_on_device = cJSON_GetObjectItem(ep_obj, "is_use_on_device")->valueint;

                    const char *ep_name = cJSON_GetObjectItem(ep_obj, "friendly_name")->valuestring;
                    strlcpy(ep->friendly_name, ep_name ? ep_name : "", sizeof(ep->friendly_name));

                    ep->deviceId = cJSON_GetObjectItem(ep_obj, "deviceId")->valueint;

                    const char *dev_id_name = cJSON_GetObjectItem(ep_obj, "device_type")->valuestring;
                    strlcpy(ep->device_Id_text, ep_name ? ep_name : "", sizeof(ep->device_Id_text));
                    /*cJSON *dev_type_obj = cJSON_GetObjectItem(ep_obj, "device_type");
                    if (dev_type_obj && dev_type_obj->valuestring) {
                        strlcpy(ep->device_Id_text, dev_type_obj->valuestring, sizeof(ep->device_Id_text));
                    }*/

                    // Identify Cluster
                    cJSON *identify = NULL;
                    identify = cJSON_GetObjectItem(ep_obj, "identify");
                    if (identify) {
                        ep->is_use_identify_cluster = 1;
                        ep->server_IdentifyClusterObj = calloc(1, sizeof(zb_manager_identify_cluster_t));
                        if (ep->server_IdentifyClusterObj) {
                            //ep->server_IdentifyClusterObj->cluster_Id_name
                            ep->server_IdentifyClusterObj->identify_time = cJSON_GetObjectItem(identify, "identify_time")->valueint;
                        }
                    }

                    // Temperature Cluster
                    cJSON *temp = NULL;
                    temp = cJSON_GetObjectItem(ep_obj, "temperature");
                    if (temp) {
                        ep->is_use_temperature_measurement_cluster = 1;
                        ep->server_TemperatureMeasurementClusterObj = calloc(1, sizeof(zb_manager_temperature_measurement_cluster_t));
                        if (ep->server_TemperatureMeasurementClusterObj) {
                            ep->server_TemperatureMeasurementClusterObj->measured_value = cJSON_GetObjectItem(temp, "measured_value")->valueint;
                            ep->server_TemperatureMeasurementClusterObj->min_measured_value = cJSON_GetObjectItem(temp, "min_measured_value")->valueint;
                            ep->server_TemperatureMeasurementClusterObj->max_measured_value = cJSON_GetObjectItem(temp, "max_measured_value")->valueint;
                            ep->server_TemperatureMeasurementClusterObj->tolerance = cJSON_GetObjectItem(temp, "tolerance")->valueint;
                            ep->server_TemperatureMeasurementClusterObj->last_update_ms = cJSON_GetObjectItem(temp, "last_update_ms")->valueint;
                            ep->server_TemperatureMeasurementClusterObj->read_error = cJSON_GetObjectItem(temp, "read_error")->type == cJSON_True;

                            // === Загрузка нестандартных атрибутов ===
                            cJSON *nostandart_attrs = cJSON_GetObjectItem(temp, "nostandart_attributes");
                            if (nostandart_attrs && cJSON_IsArray(nostandart_attrs)) {
                                int count = cJSON_GetArraySize(nostandart_attrs);
                                ep->server_TemperatureMeasurementClusterObj->nostandart_attr_count = count;
                                ep->server_TemperatureMeasurementClusterObj->nostandart_attr_array = calloc(count, sizeof(attribute_custom_t*));
                                if (ep->server_TemperatureMeasurementClusterObj->nostandart_attr_array) {
                                    bool alloc_failed = false;
                                    for (int a = 0; a < count; a++) {
                                        cJSON *attr_obj = cJSON_GetArrayItem(nostandart_attrs, a);
                                        if (!attr_obj) continue;

                                        attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
                                        if (!attr) {
                                            alloc_failed = true;
                                            continue;
                                        }

                                        attr->id = cJSON_GetObjectItem(attr_obj, "id")->valueint;
                                        LOAD_STRING(attr_obj, "attr_id_text", attr->attr_id_text);
                                        attr->type = cJSON_GetObjectItem(attr_obj, "type")->valueint;
                                        attr->acces = cJSON_GetObjectItem(attr_obj, "acces")->valueint;
                                        attr->size = cJSON_GetObjectItem(attr_obj, "size")->valueint;
                                        attr->is_void_pointer = cJSON_GetObjectItem(attr_obj, "is_void_pointer")->valueint;
                                        attr->manuf_code = cJSON_GetObjectItem(attr_obj, "manuf_code")->valueint;
                                        attr->parent_cluster_id = cJSON_GetObjectItem(attr_obj, "parent_cluster_id")->valueint;

                                        // p_value — пока NULL, можно загрузить позже по типу
                                        attr->p_value = NULL;

                                        ep->server_TemperatureMeasurementClusterObj->nostandart_attr_array[a] = attr;
                                    }
                                    if (alloc_failed) {
                                        ESP_LOGW(TAG, "Partial failure loading nostandart_attributes for Basic cluster");
                                    }
                                }
                            }
                        }
                    }

                    // Humidity Cluster
                    cJSON *hum = NULL;
                    hum = cJSON_GetObjectItem(ep_obj, "humidity");
                    if (hum) {
                        ep->is_use_humidity_measurement_cluster = 1;
                        ep->server_HumidityMeasurementClusterObj = calloc(1, sizeof(zb_manager_humidity_measurement_cluster_t));
                        if (ep->server_HumidityMeasurementClusterObj) {
                            ep->server_HumidityMeasurementClusterObj->measured_value = cJSON_GetObjectItem(hum, "measured_value")->valueint;
                            ep->server_HumidityMeasurementClusterObj->min_measured_value = cJSON_GetObjectItem(hum, "min_measured_value")->valueint;
                            ep->server_HumidityMeasurementClusterObj->max_measured_value = cJSON_GetObjectItem(hum, "max_measured_value")->valueint;
                            ep->server_HumidityMeasurementClusterObj->tolerance = cJSON_GetObjectItem(hum, "tolerance")->valueint;
                            ep->server_HumidityMeasurementClusterObj->last_update_ms = cJSON_GetObjectItem(hum, "last_update_ms")->valueint;
                            ep->server_HumidityMeasurementClusterObj->read_error = cJSON_GetObjectItem(hum, "read_error")->type == cJSON_True;

                            // === Загрузка нестандартных атрибутов ===
                            cJSON *nostandart_attrs = cJSON_GetObjectItem(hum, "nostandart_attributes");
                            if (nostandart_attrs && cJSON_IsArray(nostandart_attrs)) {
                                int count = cJSON_GetArraySize(nostandart_attrs);
                                ep->server_HumidityMeasurementClusterObj->nostandart_attr_count = count;
                                ep->server_HumidityMeasurementClusterObj->nostandart_attr_array = calloc(count, sizeof(attribute_custom_t*));
                                if (ep->server_HumidityMeasurementClusterObj->nostandart_attr_array) {
                                    bool alloc_failed = false;
                                    for (int a = 0; a < count; a++) {
                                        cJSON *attr_obj = cJSON_GetArrayItem(nostandart_attrs, a);
                                        if (!attr_obj) continue;

                                        attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
                                        if (!attr) {
                                            alloc_failed = true;
                                            continue;
                                        }

                                        attr->id = cJSON_GetObjectItem(attr_obj, "id")->valueint;
                                        LOAD_STRING(attr_obj, "attr_id_text", attr->attr_id_text);
                                        attr->type = cJSON_GetObjectItem(attr_obj, "type")->valueint;
                                        attr->acces = cJSON_GetObjectItem(attr_obj, "acces")->valueint;
                                        attr->size = cJSON_GetObjectItem(attr_obj, "size")->valueint;
                                        attr->is_void_pointer = cJSON_GetObjectItem(attr_obj, "is_void_pointer")->valueint;
                                        attr->manuf_code = cJSON_GetObjectItem(attr_obj, "manuf_code")->valueint;
                                        attr->parent_cluster_id = cJSON_GetObjectItem(attr_obj, "parent_cluster_id")->valueint;

                                        // p_value — пока NULL, можно загрузить позже по типу
                                        attr->p_value = NULL;

                                        ep->server_HumidityMeasurementClusterObj->nostandart_attr_array[a] = attr;
                                    }
                                    if (alloc_failed) {
                                        ESP_LOGW(TAG, "Partial failure loading nostandart_attributes for Basic cluster");
                                    }
                                }
                            }
                        }
                    }

                    // ON/OFF Cluster
                    cJSON *onoff = NULL;
                    onoff = cJSON_GetObjectItem(ep_obj, "onoff");
                    if (onoff) {
                        ep->is_use_on_off_cluster = 1;
                        ep->server_OnOffClusterObj = calloc(1, sizeof(zb_manager_on_off_cluster_t));
                        if (ep->server_OnOffClusterObj) {
                            ep->server_OnOffClusterObj->on_off = cJSON_GetObjectItem(onoff, "on")->type == cJSON_True;
                            ep->server_OnOffClusterObj->on_time = cJSON_GetObjectItem(onoff, "on_time")->valueint;
                            ep->server_OnOffClusterObj->off_wait_time = cJSON_GetObjectItem(onoff, "off_wait_time")->valueint;
                            ep->server_OnOffClusterObj->start_up_on_off = cJSON_GetObjectItem(onoff, "start_up")->valueint;
                            ep->server_OnOffClusterObj->last_update_ms = cJSON_GetObjectItem(onoff, "last_update_ms")->valueint;
                            // === Загрузка нестандартных атрибутов ===
                            cJSON *nostandart_attrs = cJSON_GetObjectItem(onoff, "nostandart_attributes");
                            if (nostandart_attrs && cJSON_IsArray(nostandart_attrs)) {
                                int count = cJSON_GetArraySize(nostandart_attrs);
                                ep->server_OnOffClusterObj->nostandart_attr_count = count;
                                ep->server_OnOffClusterObj->nostandart_attr_array = calloc(count, sizeof(attribute_custom_t*));
                                if (ep->server_OnOffClusterObj->nostandart_attr_array) {
                                    bool alloc_failed = false;
                                    for (int a = 0; a < count; a++) {
                                        cJSON *attr_obj = cJSON_GetArrayItem(nostandart_attrs, a);
                                        if (!attr_obj) continue;

                                        attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
                                        if (!attr) {
                                            alloc_failed = true;
                                            continue;
                                        }

                                        attr->id = cJSON_GetObjectItem(attr_obj, "id")->valueint;
                                        LOAD_STRING(attr_obj, "attr_id_text", attr->attr_id_text);
                                        attr->type = cJSON_GetObjectItem(attr_obj, "type")->valueint;
                                        attr->acces = cJSON_GetObjectItem(attr_obj, "acces")->valueint;
                                        attr->size = cJSON_GetObjectItem(attr_obj, "size")->valueint;
                                        attr->is_void_pointer = cJSON_GetObjectItem(attr_obj, "is_void_pointer")->valueint;
                                        attr->manuf_code = cJSON_GetObjectItem(attr_obj, "manuf_code")->valueint;
                                        attr->parent_cluster_id = cJSON_GetObjectItem(attr_obj, "parent_cluster_id")->valueint;

                                        // p_value — пока NULL, можно загрузить позже по типу
                                        attr->p_value = NULL;

                                        ep->server_OnOffClusterObj->nostandart_attr_array[a] = attr;
                                    }
                                    if (alloc_failed) {
                                        ESP_LOGW(TAG, "Partial failure loading nostandart_attributes for Basic cluster");
                                    }
                                }
                            }
                        }
                    }

                    

                    // === Загрузка unknown_input_clusters ===
                    ep->UnKnowninputClusterCount = cJSON_GetObjectItem(ep_obj, "unkinpclcount")->valueint;
                    if (ep->UnKnowninputClusterCount > 0)
                    {
                        cJSON *unk_input = NULL;
                        unk_input = cJSON_GetObjectItem(ep_obj, "unknown_input_clusters");
                        if (unk_input && cJSON_IsArray(unk_input)) {
                            //int count = cJSON_GetArraySize(unk_input);
                            //ep->UnKnowninputClusterCount = count;
                            ep->UnKnowninputClusters_array = NULL;
                            ep->UnKnowninputClusters_array = calloc(ep->UnKnowninputClusterCount, sizeof(cluster_custom_t*));
                            if (!ep->UnKnowninputClusters_array) {
                                ESP_LOGE(TAG, "Failed to allocate UnKnowninputClusters_array for EP %d", ep->ep_id);
                            } else {
                                bool alloc_failed = false;
                                for (int k = 0; k < ep->UnKnowninputClusterCount; k++) {
                                    cJSON *cl_obj = NULL;
                                    cl_obj = cJSON_GetArrayItem(unk_input, k);
                                    if (!cl_obj) {
                                        ESP_LOGW(TAG, "Missing unknown_input_cluster at index %d", k);
                                        continue;
                                    }

                                    cluster_custom_t *cl = NULL;
                                    cl = calloc(1, sizeof(cluster_custom_t));
                                    if (!cl) {
                                        ESP_LOGE(TAG, "Failed to allocate cluster for input cluster %d", k);
                                        alloc_failed = true;
                                        continue;
                                    }

                                    cl->id = cJSON_GetObjectItem(cl_obj, "id")->valueint;
                                    LOAD_STRING(cl_obj, "cluster_id_text", cl->cluster_id_text);
                                    cl->role_mask = cJSON_GetObjectItem(cl_obj, "role_mask")->valueint;
                                    cl->manuf_code = cJSON_GetObjectItem(cl_obj, "manuf_code")->valueint;
                                    cl->is_use_on_device = cJSON_GetObjectItem(cl_obj, "is_use_on_device")->valueint;

                                    cJSON *attrs = NULL;
                                    attrs = cJSON_GetObjectItem(cl_obj, "attributes");
                                    if (attrs && cJSON_IsArray(attrs)) {
                                        int attr_count = cJSON_GetArraySize(attrs);
                                        cl->attr_count = attr_count;
                                        if (cl->attr_count > 0)
                                        {
                                            cl->attr_array = calloc(attr_count, sizeof(attribute_custom_t*));
                                            if (!cl->attr_array) {
                                                ESP_LOGE(TAG, "Failed to allocate attr_array for cluster %d", k);
                                                free(cl);
                                                alloc_failed = true;
                                                continue;
                                            }

                                            for (int a = 0; a < attr_count; a++) {
                                                cJSON *attr_obj = cJSON_GetArrayItem(attrs, a);
                                                if (!attr_obj) continue;

                                                attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
                                                if (!attr) continue;

                                                attr->id = cJSON_GetObjectItem(attr_obj, "id")->valueint;
                                                LOAD_STRING(attr_obj, "attr_id_text", attr->attr_id_text);
                                                attr->type = cJSON_GetObjectItem(attr_obj, "type")->valueint;
                                                attr->acces = cJSON_GetObjectItem(attr_obj, "acces")->valueint;
                                                attr->size = cJSON_GetObjectItem(attr_obj, "size")->valueint;
                                                attr->is_void_pointer = cJSON_GetObjectItem(attr_obj, "is_void_pointer")->valueint;
                                                attr->manuf_code = cJSON_GetObjectItem(attr_obj, "manuf_code")->valueint;
                                                attr->parent_cluster_id = cJSON_GetObjectItem(attr_obj, "parent_cluster_id")->valueint;
                                                attr->p_value = NULL;

                                                cl->attr_array[a] = attr;
                                            }
                                        }
                                    }

                                    ep->UnKnowninputClusters_array[k] = cl;
                                }
                                if (alloc_failed) {
                                    ESP_LOGW(TAG, "Partial failure in loading unknown_input_clusters for EP %d", ep->ep_id);
                                }
                            }
                        }
                    } // end if count > 0
                    // Output Clusters
                    ep->output_clusters_count = cJSON_GetObjectItem(ep_obj, "outpclcount")->valueint;
                    if (ep->output_clusters_count > 0)
                    {
                        cJSON *out_clusters = NULL;
                        out_clusters = cJSON_GetObjectItem(ep_obj, "output_clusters");
                        if (out_clusters && cJSON_IsArray(out_clusters)) {
                            //int count = cJSON_GetArraySize(out_clusters);
                            //ep->output_clusters_count = count;
                            ep->output_clusters_array = calloc(ep->output_clusters_count, sizeof(uint16_t));
                            if (ep->output_clusters_array) {
                                for (int k = 0; k < ep->output_clusters_count; k++) {
                                    cJSON *item = cJSON_GetArrayItem(out_clusters, k);
                                    if (cJSON_IsNumber(item)) {
                                        ep->output_clusters_array[k] = (uint16_t)item->valueint;
                                    }
                                }
                            }
                        }
                    }
                    // === Загрузка unknown_output_clusters ===
                    ep->UnKnownoutputClusterCount = cJSON_GetObjectItem(ep_obj, "unkoutpclcount")->valueint;
                    if (ep->UnKnownoutputClusterCount > 0)
                    {
                        cJSON *unk_output = NULL;
                        unk_output = cJSON_GetObjectItem(ep_obj, "unknown_output_clusters");
                        if (unk_output && cJSON_IsArray(unk_output)) {
                            //int count = cJSON_GetArraySize(unk_output);
                            //ep->UnKnownoutputClusterCount = count;
                            ep->UnKnownoutputClusters_array = calloc(ep->UnKnownoutputClusterCount, sizeof(cluster_custom_t*));
                            if (!ep->UnKnownoutputClusters_array) {
                                ESP_LOGE(TAG, "Failed to allocate UnKnownoutputClusters_array for EP %d", ep->ep_id);
                            } else {
                                bool alloc_failed = false;
                                for (int k = 0; k < ep->UnKnownoutputClusterCount; k++) {
                                    cJSON *cl_obj = NULL;
                                    cl_obj = cJSON_GetArrayItem(unk_output, k);
                                    if (!cl_obj) continue;

                                    cluster_custom_t *cl = NULL;
                                    cl = calloc(1, sizeof(cluster_custom_t));
                                    if (!cl) {
                                        ESP_LOGE(TAG, "Failed to allocate cluster for output cluster %d", k);
                                        alloc_failed = true;
                                        continue;
                                    }

                                    cl->id = cJSON_GetObjectItem(cl_obj, "id")->valueint;
                                    LOAD_STRING(cl_obj, "cluster_id_text", cl->cluster_id_text);
                                    cl->role_mask = cJSON_GetObjectItem(cl_obj, "role_mask")->valueint;
                                    cl->manuf_code = cJSON_GetObjectItem(cl_obj, "manuf_code")->valueint;
                                    cl->is_use_on_device = cJSON_GetObjectItem(cl_obj, "is_use_on_device")->valueint;

                                    cJSON *attrs = NULL;
                                    attrs = cJSON_GetObjectItem(cl_obj, "attributes");
                                    if (attrs && cJSON_IsArray(attrs)) {
                                        int attr_count = cJSON_GetArraySize(attrs);
                                        cl->attr_count = attr_count;
                                        cl->attr_array = calloc(attr_count, sizeof(attribute_custom_t*));
                                        if (!cl->attr_array) {
                                            free(cl);
                                            alloc_failed = true;
                                            continue;
                                        }

                                        for (int a = 0; a < attr_count; a++) {
                                            cJSON *attr_obj = cJSON_GetArrayItem(attrs, a);
                                            if (!attr_obj) continue;

                                            attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
                                            if (!attr) continue;

                                            attr->id = cJSON_GetObjectItem(attr_obj, "id")->valueint;
                                            LOAD_STRING(attr_obj, "attr_id_text", attr->attr_id_text);
                                            attr->type = cJSON_GetObjectItem(attr_obj, "type")->valueint;
                                            attr->acces = cJSON_GetObjectItem(attr_obj, "acces")->valueint;
                                            attr->size = cJSON_GetObjectItem(attr_obj, "size")->valueint;
                                            attr->is_void_pointer = cJSON_GetObjectItem(attr_obj, "is_void_pointer")->valueint;
                                            attr->manuf_code = cJSON_GetObjectItem(attr_obj, "manuf_code")->valueint;
                                            attr->parent_cluster_id = cJSON_GetObjectItem(attr_obj, "parent_cluster_id")->valueint;
                                            attr->p_value = NULL;

                                            cl->attr_array[a] = attr;
                                        }
                                    }else alloc_failed = true;

                                    ep->UnKnownoutputClusters_array[k] = cl;
                                }
                                if (alloc_failed) {
                                    ESP_LOGW(TAG, "Partial failure in loading unknown_output_clusters for EP %d", ep->ep_id);
                                }
                            }
                        }
                    }
                    dev->endpoints_array[j] = ep;
                }
            }
    }

    return ESP_OK;
}
//********************************************************************************************************************************

//================================================================================================================================
//================================================== ZBM_DEV_BASE_SAVE_ONE_DEVICE ================================================
//================================================================================================================================
static esp_err_t zbm_dev_save_single_device(device_custom_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;

    cJSON *dev_json = zbm_dev_base_device_to_json(dev);
    if (!dev_json) {
        ESP_LOGE(TAG, "Failed to create JSON for device %02x:%02x...", dev->ieee_addr[7], dev->ieee_addr[6]);
        return ESP_FAIL;
    }

    char filepath[64];
    zbm_dev_get_filename_by_ieee(dev->ieee_addr, filepath, sizeof(filepath));

    char *json_str = cJSON_PrintUnformatted(dev_json);
    cJSON_Delete(dev_json);
    if (!json_str) {
        ESP_LOGE(TAG, "Failed to print JSON for device");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Saving device %04x to file: %s", dev->short_addr, filepath);

    FILE *f = fopen(filepath, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for writing", filepath);
        free(json_str);
        return ESP_FAIL;
    }
    fwrite(json_str, 1, strlen(json_str), f);
    fclose(f);
    free(json_str);

    ESP_LOGD(TAG, "Device saved to %s", filepath);
    return ESP_OK;
}
//********************************************************************************************************************************

//================================================================================================================================
//========================================================= ZBM_DEV_BASE_SAVE ====================================================
//================================================================================================================================
static esp_err_t zbm_dev_base_save_new(const char *index_filepath)
{
    if (xSemaphoreTake(zbm_g_device_array_mutex, pdMS_TO_TICKS(ZBM_BASE_MUTEX_TIMEOUT_LONG_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex in zbm_dev_base_save");
        return ESP_ERR_TIMEOUT;
    }

    cJSON *index_array = cJSON_CreateArray();
    if (!index_array) {
        xSemaphoreGive(zbm_g_device_array_mutex);
        return ESP_ERR_NO_MEM;
    }

    bool save_failed = false;

    for (int i = 0; i < zbm_RemoteDevicesCount; i++) {
        device_custom_t *dev = zbm_RemoteDevicesArray[i];
        if (!dev) continue;

        esp_err_t err = zbm_dev_save_single_device(dev);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to save device %02x:%02x...", dev->ieee_addr[7], dev->ieee_addr[6]);
            save_failed = true;
            continue;
        }

        char ieee_str[24] = {0};
        ieee_to_str(ieee_str, dev->ieee_addr);

        char dev_filepath[64];
        zbm_dev_get_filename_by_ieee(dev->ieee_addr, dev_filepath, sizeof(dev_filepath));

        cJSON *idx_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(idx_obj, "ieee_addr", ieee_str);
        cJSON_AddNumberToObject(idx_obj, "short_addr", dev->short_addr);
        cJSON_AddStringToObject(idx_obj, "friendly_name", dev->friendly_name);
        cJSON_AddStringToObject(idx_obj, "file", dev_filepath);
        cJSON_AddItemToArray(index_array, idx_obj);
    }

    char *index_str = cJSON_PrintUnformatted(index_array);
    cJSON_Delete(index_array);
    if (!index_str) {
        xSemaphoreGive(zbm_g_device_array_mutex);
        return ESP_FAIL;
    }

    FILE *f = fopen(index_filepath, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open index file %s", index_filepath);
        free(index_str);
        xSemaphoreGive(zbm_g_device_array_mutex);
        return ESP_FAIL;
    }
    fwrite(index_str, 1, strlen(index_str), f);
    fclose(f);
    free(index_str);

    xSemaphoreGive(zbm_g_device_array_mutex);

    ESP_LOGI(TAG, "Device index saved to %s", index_filepath);
    return save_failed ? ESP_ERR_NOT_FINISHED : ESP_OK;
}


//================================================================================================================================
//========================================================= ZBM_DEV_BASE_SAVE ====================================================
//================================================================================================================================
static esp_err_t zbm_dev_base_load_new(const char *index_filepath)
{
    ESP_LOGW(TAG, "zbm_dev_base_load: loading from index %s", index_filepath);

    FILE *f = fopen(index_filepath, "r");
    if (!f) {
        ESP_LOGW(TAG, "No index file found: %s", index_filepath);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *json_str = malloc(len + 1);
    if (!json_str) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    fread(json_str, 1, len, f);
    json_str[len] = '\0';
    fclose(f);

    cJSON *index_array = cJSON_Parse(json_str);
    free(json_str);
    if (!index_array) {
        ESP_LOGE(TAG, "JSON parse failed: %s", cJSON_GetErrorPtr());
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(zbm_g_device_array_mutex, pdMS_TO_TICKS(ZBM_BASE_MUTEX_TIMEOUT_LONG_MS)) != pdTRUE) {
        cJSON_Delete(index_array);
        return ESP_ERR_TIMEOUT;
    }

    for (int i = 0; i < zbm_RemoteDevicesCount; i++) {
        if (zbm_RemoteDevicesArray[i]) {
            zbm_dev_base_dev_delete_safe(zbm_RemoteDevicesArray[i]);
            zbm_RemoteDevicesArray[i] = NULL;
        }
    }

    cJSON *idx_item = NULL;
    cJSON_ArrayForEach(idx_item, index_array) {
        const char *ieee_str = cJSON_GetObjectItem(idx_item, "ieee_addr")->valuestring;
        const char *dev_file = cJSON_GetObjectItem(idx_item, "file")->valuestring;
        const char *friendly_name = cJSON_GetObjectItem(idx_item, "friendly_name")->valuestring;
        int short_addr = cJSON_GetObjectItem(idx_item, "short_addr")->valueint;

        if (!ieee_str || !dev_file) continue;

        uint8_t ieee[8];
        if (!str_to_ieee(ieee_str, ieee)) continue;

        FILE *df = fopen(dev_file, "r");
        if (!df) {
            ESP_LOGW(TAG, "Device file not found: %s", dev_file);
            continue;
        }

        fseek(df, 0, SEEK_END);
        long dlen = ftell(df);
        fseek(df, 0, SEEK_SET);
        char *dstr = malloc(dlen + 1);
        if (!dstr) {
            fclose(df);
            continue;
        }
        fread(dstr, 1, dlen, df);
        dstr[dlen] = '\0';
        fclose(df);

        cJSON *dev_json = cJSON_Parse(dstr);
        free(dstr);
        if (!dev_json) continue;

        device_custom_t *dev = calloc(1, sizeof(device_custom_t));
        if (!dev) {
            cJSON_Delete(dev_json);
            continue;
        }

        memcpy(dev->ieee_addr, ieee, 8);
        dev->short_addr = short_addr;
        strlcpy(dev->friendly_name, friendly_name ? friendly_name : "", sizeof(dev->friendly_name));
        dev->friendly_name_len = strlen(dev->friendly_name);

        esp_err_t load_result = zbm_dev_base_load_device_from_json(dev, dev_json);
        cJSON_Delete(dev_json);

        if (load_result != ESP_OK) {
            free(dev);
            continue;
        }

        bool inserted = false;
        for (int idx = 0; idx < zbm_RemoteDevicesCount; idx++) {
            if (zbm_RemoteDevicesArray[idx] == NULL) {
                zbm_RemoteDevicesArray[idx] = dev;
                inserted = true;
                break;
            }
        }
        if (!inserted) {
            ESP_LOGE(TAG, "No free slot for device %s", ieee_str);
            zbm_dev_base_dev_delete(dev);
        }
    }

    for (int i = 0; i < zbm_RemoteDevicesCount; i++) {
        if (zbm_RemoteDevicesArray[i]) {
            zbm_dev_configure_device_timeout(zbm_RemoteDevicesArray[i]);
        }
    }

    xSemaphoreGive(zbm_g_device_array_mutex);
    cJSON_Delete(index_array);

    ESP_LOGI(TAG, "Devices loaded from index and individual files");
    return ESP_OK;
}
//********************************************************************************************************************************

//================================================================================================================================
//========================================================== ZBM_DEV_BASE_DEVICE_TO_JSON =========================================
//================================================================================================================================
cJSON *zbm_dev_base_device_to_json(device_custom_t* dev)
{
if (!dev) {
        ESP_LOGE(TAG, "zb_manager_get_device_json: dev is NULL");
        return NULL;
    }

    ESP_LOGI(TAG, "zb_manager_get_device_json: Building JSON for device IEEE=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, short=0x%04x",
             dev->ieee_addr[7], dev->ieee_addr[6], dev->ieee_addr[5], dev->ieee_addr[4],
             dev->ieee_addr[3], dev->ieee_addr[2], dev->ieee_addr[1], dev->ieee_addr[0],
             dev->short_addr);

    cJSON *dev_obj = NULL;
    dev_obj = cJSON_CreateObject();
    if (!dev_obj) {
        ESP_LOGE(TAG, "zb_manager_get_device_json: Failed to create dev_obj");
        return NULL;
    }

    // === Основные поля устройства ===
    char ieee_str[24] = {0};
    ieee_to_str(ieee_str, dev->ieee_addr);
    cJSON_AddStringToObject(dev_obj, "ieee_addr", ieee_str);
    cJSON_AddNumberToObject(dev_obj, "short_addr", dev->short_addr);
    cJSON_AddNumberToObject(dev_obj, "is_in_build_status", dev->is_in_build_status);
    cJSON_AddNumberToObject(dev_obj, "index_in_array", dev->index_in_array);
    cJSON_AddStringToObject(dev_obj, "friendly_name", dev->friendly_name);
    cJSON_AddNumberToObject(dev_obj, "friendly_name_len", dev->friendly_name_len);
    cJSON_AddNumberToObject(dev_obj, "capability", dev->capability);
    cJSON_AddNumberToObject(dev_obj, "lqi", dev->lqi);
    cJSON_AddNumberToObject(dev_obj, "last_seen_ms", dev->last_seen_ms);
    cJSON_AddNumberToObject(dev_obj, "device_timeout_ms", dev->device_timeout_ms);
    cJSON_AddBoolToObject(dev_obj, "is_online", dev->is_online);

    if (dev->manufacturer_code != 0) {
        cJSON_AddNumberToObject(dev_obj, "manufacturer_code", dev->manufacturer_code);
        cJSON_AddStringToObject(dev_obj, "manufacturer_name_str", zbm_extract_manufacturer_name_by_code(dev->manufacturer_code));
    }

    // Basic Cluster
    if (dev->server_BasicClusterObj) {
        cJSON *basic = NULL;
        basic = cJSON_CreateObject();

        if (basic) {
            cJSON_AddNumberToObject(basic, "cluster_id", 0x0000);
            cJSON_AddNumberToObject(basic, "zcl_version", dev->server_BasicClusterObj->zcl_version);
            cJSON_AddNumberToObject(basic, "app_version", dev->server_BasicClusterObj->application_version);
            cJSON_AddNumberToObject(basic, "stack_version", dev->server_BasicClusterObj->stack_version);
            cJSON_AddNumberToObject(basic, "hw_version", dev->server_BasicClusterObj->hw_version);
            cJSON_AddStringToObject(basic, "manufacturer_name", dev->server_BasicClusterObj->manufacturer_name);
            cJSON_AddStringToObject(basic, "model_id", dev->server_BasicClusterObj->model_identifier);
            cJSON_AddStringToObject(basic, "date_code", dev->server_BasicClusterObj->date_code);
            cJSON_AddNumberToObject(basic, "power_source", dev->server_BasicClusterObj->power_source);
            const char* power_text = get_power_source_string(dev->server_BasicClusterObj->power_source);
            cJSON_AddStringToObject(basic, "power_source_text", power_text);
            cJSON_AddStringToObject(basic, "location", dev->server_BasicClusterObj->location_description);
            cJSON_AddNumberToObject(basic, "env", dev->server_BasicClusterObj->physical_environment);
            cJSON_AddBoolToObject(basic, "enabled", dev->server_BasicClusterObj->device_enabled);
            cJSON_AddNumberToObject(basic, "last_update_ms", dev->server_BasicClusterObj->last_update_ms);
            // === Добавляем нестандартные атрибуты ===
            if (dev->server_BasicClusterObj->nostandart_attr_count > 0 && dev->server_BasicClusterObj->nostandart_attr_array) {
                cJSON *attrs = cJSON_CreateArray();
                if (attrs) {
                    for (int a = 0; a < dev->server_BasicClusterObj->nostandart_attr_count; a++) {
                        attribute_custom_t *attr = dev->server_BasicClusterObj->nostandart_attr_array[a];
                        if (!attr) continue;
                        cJSON *attr_obj = cJSON_CreateObject();
                        if (!attr_obj) continue;

                        cJSON_AddNumberToObject(attr_obj, "id", attr->id);
                        cJSON_AddStringToObject(attr_obj, "attr_id_text", attr->attr_id_text);
                        cJSON_AddNumberToObject(attr_obj, "type", attr->type);
                        cJSON_AddNumberToObject(attr_obj, "acces", attr->acces);
                        cJSON_AddNumberToObject(attr_obj, "size", attr->size);
                        cJSON_AddNumberToObject(attr_obj, "is_void_pointer", attr->is_void_pointer);
                        cJSON_AddNumberToObject(attr_obj, "manuf_code", attr->manuf_code);
                        cJSON_AddNumberToObject(attr_obj, "parent_cluster_id", attr->parent_cluster_id);

                        // p_value — сложнее, зависит от типа. Пока пропустим или добавим как hex строку при необходимости
                        // Можно добавить опционально: если size > 0 && p_value != NULL

                        cJSON_AddItemToArray(attrs, attr_obj);
                    }
                    cJSON_AddItemToObject(basic, "nostandart_attributes", attrs);
                }
            }

            cJSON_AddItemToObject(dev_obj, "device_basic_cluster", basic);
        }
    }

    // Power Configuration Cluster
    if (dev->server_PowerConfigurationClusterObj) {
        cJSON *power_config = NULL;
        power_config = cJSON_CreateObject();
        if (power_config) {
            cJSON_AddNumberToObject(power_config, "cluster_id", 0x0001);
            cJSON_AddNumberToObject(power_config, "battery_voltage", dev->server_PowerConfigurationClusterObj->battery_voltage);
            cJSON_AddNumberToObject(power_config, "battery_percentage", dev->server_PowerConfigurationClusterObj->battery_percentage);
            cJSON_AddStringToObject(power_config, "battery_voltage_str", get_battery_voltage_string(dev->server_PowerConfigurationClusterObj->battery_voltage));
            cJSON_AddStringToObject(power_config, "battery_percentage_str", get_battery_percentage_string(dev->server_PowerConfigurationClusterObj->battery_percentage));
            cJSON_AddStringToObject(power_config, "battery_manufacturer", dev->server_PowerConfigurationClusterObj->battery_manufacturer);
            cJSON_AddNumberToObject(power_config, "battery_size", dev->server_PowerConfigurationClusterObj->battery_size);
            cJSON_AddStringToObject(power_config, "battery_size_str", get_battery_size_string(dev->server_PowerConfigurationClusterObj->battery_size));
            cJSON_AddNumberToObject(power_config, "battery_a_hr_rating", dev->server_PowerConfigurationClusterObj->battery_a_hr_rating);
            cJSON_AddNumberToObject(power_config, "battery_quantity", dev->server_PowerConfigurationClusterObj->battery_quantity);
            cJSON_AddNumberToObject(power_config, "battery_rated_voltage", dev->server_PowerConfigurationClusterObj->battery_rated_voltage);
            cJSON_AddNumberToObject(power_config, "battery_alarm_mask", dev->server_PowerConfigurationClusterObj->battery_alarm_mask);
            cJSON_AddNumberToObject(power_config, "battery_voltage_min_th", dev->server_PowerConfigurationClusterObj->battery_voltage_min_th);
            cJSON_AddNumberToObject(power_config, "battery_voltage_th1", dev->server_PowerConfigurationClusterObj->battery_voltage_th1);
            cJSON_AddNumberToObject(power_config, "battery_voltage_th2", dev->server_PowerConfigurationClusterObj->battery_voltage_th2);
            cJSON_AddNumberToObject(power_config, "battery_voltage_th3", dev->server_PowerConfigurationClusterObj->battery_voltage_th3);
            cJSON_AddNumberToObject(power_config, "battery_percentage_min_th", dev->server_PowerConfigurationClusterObj->battery_percentage_min_th);
            cJSON_AddNumberToObject(power_config, "battery_percentage_th1", dev->server_PowerConfigurationClusterObj->battery_percentage_th1);
            cJSON_AddNumberToObject(power_config, "battery_percentage_th2", dev->server_PowerConfigurationClusterObj->battery_percentage_th2);
            cJSON_AddNumberToObject(power_config, "battery_percentage_th3", dev->server_PowerConfigurationClusterObj->battery_percentage_th3);
            cJSON_AddNumberToObject(power_config, "battery_alarm_state", dev->server_PowerConfigurationClusterObj->battery_alarm_state);
            cJSON_AddNumberToObject(power_config, "mains_voltage", dev->server_PowerConfigurationClusterObj->mains_voltage);
            cJSON_AddNumberToObject(power_config, "mains_frequency", dev->server_PowerConfigurationClusterObj->mains_frequency);
            cJSON_AddNumberToObject(power_config, "mains_alarm_mask", dev->server_PowerConfigurationClusterObj->mains_alarm_mask);
            cJSON_AddNumberToObject(power_config, "mains_voltage_min_th", dev->server_PowerConfigurationClusterObj->mains_voltage_min_th);
            cJSON_AddNumberToObject(power_config, "mains_voltage_max_th", dev->server_PowerConfigurationClusterObj->mains_voltage_max_th);
            cJSON_AddNumberToObject(power_config, "mains_dwell_trip_point", dev->server_PowerConfigurationClusterObj->mains_dwell_trip_point);
            cJSON_AddNumberToObject(power_config, "last_update_ms", dev->server_PowerConfigurationClusterObj->last_update_ms);
            // === Добавляем нестандартные атрибуты ===
            if (dev->server_PowerConfigurationClusterObj->nostandart_attr_count > 0 && dev->server_PowerConfigurationClusterObj->nostandart_attr_array) {
                cJSON *attrs = cJSON_CreateArray();
                if (attrs) {
                    for (int a = 0; a < dev->server_PowerConfigurationClusterObj->nostandart_attr_count; a++) {
                        attribute_custom_t *attr = dev->server_PowerConfigurationClusterObj->nostandart_attr_array[a];
                        if (!attr) continue;
                        cJSON *attr_obj = cJSON_CreateObject();
                        if (!attr_obj) continue;

                        cJSON_AddNumberToObject(attr_obj, "id", attr->id);
                        cJSON_AddStringToObject(attr_obj, "attr_id_text", attr->attr_id_text);
                        cJSON_AddNumberToObject(attr_obj, "type", attr->type);
                        cJSON_AddNumberToObject(attr_obj, "acces", attr->acces);
                        cJSON_AddNumberToObject(attr_obj, "size", attr->size);
                        cJSON_AddNumberToObject(attr_obj, "is_void_pointer", attr->is_void_pointer);
                        cJSON_AddNumberToObject(attr_obj, "manuf_code", attr->manuf_code);
                        cJSON_AddNumberToObject(attr_obj, "parent_cluster_id", attr->parent_cluster_id);

                        // p_value — сложнее, зависит от типа. Пока пропустим или добавим как hex строку при необходимости
                        // Можно добавить опционально: если size > 0 && p_value != NULL

                        cJSON_AddItemToArray(attrs, attr_obj);
                    }
                    cJSON_AddItemToObject(power_config, "nostandart_attributes", attrs);
                }
            }
            cJSON_AddItemToObject(dev_obj, "device_power_config_cluster", power_config);
        }
    }

    // === Endpoints ===
    cJSON *eps = NULL;
    eps = cJSON_CreateArray();
    if (!eps) {
        ESP_LOGE(TAG, "zb_manager_get_device_json: Failed to create eps array");
        cJSON_Delete(dev_obj);
        return NULL;
    }

    ESP_LOGD(TAG, "zb_manager_get_device_json: Found %d endpoints", dev->endpoints_count);

    cJSON_AddNumberToObject(dev_obj, "endpointscount", dev->endpoints_count);

    if(dev->endpoints_count > 0 && dev->endpoints_array){
        for (int j = 0; j < dev->endpoints_count; j++) {
            endpoint_custom_t *ep = NULL;
            ep = dev->endpoints_array[j];
            if (!ep) {
                ESP_LOGE(TAG, "zb_manager_get_device_json: endpoints_array[%d] is NULL", j);
                continue;
            }

            ESP_LOGI(TAG, "zb_manager_get_device_json: Processing EP %d, deviceId=0x%04x, use_on_dev=%d",
                    ep->ep_id, ep->deviceId, ep->is_use_on_device);

            cJSON *ep_obj = NULL;
            ep_obj = cJSON_CreateObject();
            if (!ep_obj) {
                ESP_LOGE(TAG, "zb_manager_get_device_json: Failed to create ep_obj for ep_id=%d", ep->ep_id);
                continue;
            }

            cJSON_AddNumberToObject(ep_obj, "ep_id", ep->ep_id);
            cJSON_AddNumberToObject(ep_obj, "is_use_on_device", ep->is_use_on_device);
            cJSON_AddStringToObject(ep_obj, "friendly_name", ep->friendly_name);
            cJSON_AddNumberToObject(ep_obj, "deviceId", ep->deviceId);
            const char* dev_type_name = zbm_dev_base_extract_device_type_name_from_device_id(ep->deviceId);
            cJSON_AddStringToObject(ep_obj, "device_type", dev_type_name);

            // Identify Cluster
            if (ep->is_use_identify_cluster && ep->server_IdentifyClusterObj) {
                cJSON *identify = NULL;
                identify = cJSON_CreateObject();
                if (identify) {
                    cJSON_AddNumberToObject(identify, "cluster_id", 0x0003);
                    cJSON_AddNumberToObject(identify, "identify_time", ep->server_IdentifyClusterObj->identify_time);
                    cJSON_AddItemToObject(ep_obj, "identify", identify);
                }
            }

            // Temperature Measurement Cluster
            if (ep->is_use_temperature_measurement_cluster && ep->server_TemperatureMeasurementClusterObj) {
                cJSON *temp = NULL;
                temp = cJSON_CreateObject();
                if (temp) {
                    cJSON_AddNumberToObject(temp, "cluster_id", 0x0402);
                    cJSON_AddNumberToObject(temp, "measured_value", ep->server_TemperatureMeasurementClusterObj->measured_value);
                    cJSON_AddNumberToObject(temp, "min_measured_value", ep->server_TemperatureMeasurementClusterObj->min_measured_value);
                    cJSON_AddNumberToObject(temp, "max_measured_value", ep->server_TemperatureMeasurementClusterObj->max_measured_value);
                    cJSON_AddNumberToObject(temp, "tolerance", ep->server_TemperatureMeasurementClusterObj->tolerance);
                    cJSON_AddNumberToObject(temp, "last_update_ms", ep->server_TemperatureMeasurementClusterObj->last_update_ms);
                    cJSON_AddBoolToObject(temp, "read_error", ep->server_TemperatureMeasurementClusterObj->read_error);
                    // === Добавляем нестандартные атрибуты ===
                    if (ep->server_TemperatureMeasurementClusterObj->nostandart_attr_count > 0 && ep->server_TemperatureMeasurementClusterObj->nostandart_attr_array) {
                        cJSON *attrs = cJSON_CreateArray();
                        if (attrs) {
                            for (int a = 0; a < ep->server_TemperatureMeasurementClusterObj->nostandart_attr_count; a++) {
                                attribute_custom_t *attr = ep->server_TemperatureMeasurementClusterObj->nostandart_attr_array[a];
                                if (!attr) continue;
                                cJSON *attr_obj = cJSON_CreateObject();
                                if (!attr_obj) continue;

                                cJSON_AddNumberToObject(attr_obj, "id", attr->id);
                                cJSON_AddStringToObject(attr_obj, "attr_id_text", attr->attr_id_text);
                                cJSON_AddNumberToObject(attr_obj, "type", attr->type);
                                cJSON_AddNumberToObject(attr_obj, "acces", attr->acces);
                                cJSON_AddNumberToObject(attr_obj, "size", attr->size);
                                cJSON_AddNumberToObject(attr_obj, "is_void_pointer", attr->is_void_pointer);
                                cJSON_AddNumberToObject(attr_obj, "manuf_code", attr->manuf_code);
                                cJSON_AddNumberToObject(attr_obj, "parent_cluster_id", attr->parent_cluster_id);

                                // p_value — сложнее, зависит от типа. Пока пропустим или добавим как hex строку при необходимости
                                // Можно добавить опционально: если size > 0 && p_value != NULL

                                cJSON_AddItemToArray(attrs, attr_obj);
                            }
                            cJSON_AddItemToObject(temp, "nostandart_attributes", attrs);
                        }
                    }
                    cJSON_AddItemToObject(ep_obj, "temperature", temp);
                }
            }

            // Humidity Measurement Cluster
            if (ep->is_use_humidity_measurement_cluster && ep->server_HumidityMeasurementClusterObj) {
                cJSON *hum = NULL;
                hum = cJSON_CreateObject();
                if (hum) {
                    cJSON_AddNumberToObject(hum, "cluster_id", 0x0405);
                    cJSON_AddNumberToObject(hum, "measured_value", ep->server_HumidityMeasurementClusterObj->measured_value);
                    cJSON_AddNumberToObject(hum, "min_measured_value", ep->server_HumidityMeasurementClusterObj->min_measured_value);
                    cJSON_AddNumberToObject(hum, "max_measured_value", ep->server_HumidityMeasurementClusterObj->max_measured_value);
                    cJSON_AddNumberToObject(hum, "tolerance", ep->server_HumidityMeasurementClusterObj->tolerance);
                    cJSON_AddNumberToObject(hum, "last_update_ms", ep->server_HumidityMeasurementClusterObj->last_update_ms);
                    cJSON_AddBoolToObject(hum, "read_error", ep->server_HumidityMeasurementClusterObj->read_error);
                    // === Добавляем нестандартные атрибуты ===
                    if (ep->server_HumidityMeasurementClusterObj->nostandart_attr_count > 0 && ep->server_HumidityMeasurementClusterObj->nostandart_attr_array) {
                        cJSON *attrs = cJSON_CreateArray();
                        if (attrs) {
                            for (int a = 0; a < ep->server_HumidityMeasurementClusterObj->nostandart_attr_count; a++) {
                                attribute_custom_t *attr = ep->server_HumidityMeasurementClusterObj->nostandart_attr_array[a];
                                if (!attr) continue;
                                cJSON *attr_obj = cJSON_CreateObject();
                                if (!attr_obj) continue;

                                cJSON_AddNumberToObject(attr_obj, "id", attr->id);
                                cJSON_AddStringToObject(attr_obj, "attr_id_text", attr->attr_id_text);
                                cJSON_AddNumberToObject(attr_obj, "type", attr->type);
                                cJSON_AddNumberToObject(attr_obj, "acces", attr->acces);
                                cJSON_AddNumberToObject(attr_obj, "size", attr->size);
                                cJSON_AddNumberToObject(attr_obj, "is_void_pointer", attr->is_void_pointer);
                                cJSON_AddNumberToObject(attr_obj, "manuf_code", attr->manuf_code);
                                cJSON_AddNumberToObject(attr_obj, "parent_cluster_id", attr->parent_cluster_id);

                                // p_value — сложнее, зависит от типа. Пока пропустим или добавим как hex строку при необходимости
                                // Можно добавить опционально: если size > 0 && p_value != NULL

                                cJSON_AddItemToArray(attrs, attr_obj);
                            }
                            cJSON_AddItemToObject(hum, "nostandart_attributes", attrs);
                        }
                    }
                    cJSON_AddItemToObject(ep_obj, "humidity", hum);
                }
            }

            // OnOff Cluster
            if (ep->is_use_on_off_cluster && ep->server_OnOffClusterObj) {
                cJSON *onoff = NULL;
                onoff = cJSON_CreateObject();
                if (onoff) {
                    cJSON_AddNumberToObject(onoff, "cluster_id", 0x0006);
                    cJSON_AddBoolToObject(onoff, "on", ep->server_OnOffClusterObj->on_off);
                    cJSON_AddNumberToObject(onoff, "on_time", ep->server_OnOffClusterObj->on_time);
                    cJSON_AddNumberToObject(onoff, "off_wait_time", ep->server_OnOffClusterObj->off_wait_time);
                    cJSON_AddNumberToObject(onoff, "start_up", ep->server_OnOffClusterObj->start_up_on_off);
                    cJSON_AddNumberToObject(onoff, "last_update_ms", ep->server_OnOffClusterObj->last_update_ms);
                    if (ep->server_OnOffClusterObj->nostandart_attr_count > 0 && ep->server_OnOffClusterObj->nostandart_attr_array) {
                        cJSON *attrs = cJSON_CreateArray();
                        if (attrs) {
                            for (int a = 0; a < ep->server_OnOffClusterObj->nostandart_attr_count; a++) {
                                attribute_custom_t *attr = ep->server_OnOffClusterObj->nostandart_attr_array[a];
                                if (!attr) continue;
                                cJSON *attr_obj = cJSON_CreateObject();
                                if (!attr_obj) continue;

                                cJSON_AddNumberToObject(attr_obj, "id", attr->id);
                                cJSON_AddStringToObject(attr_obj, "attr_id_text", attr->attr_id_text);
                                cJSON_AddNumberToObject(attr_obj, "type", attr->type);
                                cJSON_AddNumberToObject(attr_obj, "acces", attr->acces);
                                cJSON_AddNumberToObject(attr_obj, "size", attr->size);
                                cJSON_AddNumberToObject(attr_obj, "is_void_pointer", attr->is_void_pointer);
                                cJSON_AddNumberToObject(attr_obj, "manuf_code", attr->manuf_code);
                                cJSON_AddNumberToObject(attr_obj, "parent_cluster_id", attr->parent_cluster_id);

                                // p_value — сложнее, зависит от типа. Пока пропустим или добавим как hex строку при необходимости
                                // Можно добавить опционально: если size > 0 && p_value != NULL

                                cJSON_AddItemToArray(attrs, attr_obj);
                            }
                            cJSON_AddItemToObject(onoff, "nostandart_attributes", attrs);
                        }
                    }
                    cJSON_AddItemToObject(ep_obj, "onoff", onoff);
                }
            }

            
            cJSON_AddNumberToObject(ep_obj, "unkinpclcount", ep->UnKnowninputClusterCount);
            // Unknown Input Clusters
            if (ep->UnKnowninputClusterCount > 0 && ep->UnKnowninputClusters_array) {
                cJSON *unk_input = NULL;
                unk_input = cJSON_CreateArray();
                if (unk_input) {
                    ESP_LOGD(TAG, "zb_manager_get_device_json: EP %d has %d unknown input clusters", ep->ep_id, ep->UnKnowninputClusterCount);
                    for (int k = 0; k < ep->UnKnowninputClusterCount; k++) {
                        cluster_custom_t *cl = NULL;
                        cl = ep->UnKnowninputClusters_array[k];
                        if (!cl) {
                            ESP_LOGE(TAG, "zb_manager_get_device_json: UnKnowninputClusters_array[%d] is NULL", k);
                            continue;
                        }
                        cJSON *cl_obj = NULL;
                        cl_obj = cJSON_CreateObject();
                        if (!cl_obj) 
                        {
                            ESP_LOGE(TAG, "cl_obj = cJSON_CreateObject(); ERROR str 1280");
                            continue;
                        }

                        cJSON_AddNumberToObject(cl_obj, "id", cl->id);
                        cJSON_AddStringToObject(cl_obj, "cluster_id_text", cl->cluster_id_text);
                        cJSON_AddNumberToObject(cl_obj, "role_mask", cl->role_mask);
                        cJSON_AddNumberToObject(cl_obj, "manuf_code", cl->manuf_code);
                        cJSON_AddNumberToObject(cl_obj, "is_use_on_device", cl->is_use_on_device);

                        cJSON *attrs = NULL;
                        attrs = cJSON_CreateArray();
                        if (attrs) {
                            for (int a = 0; a < cl->attr_count; a++) {
                                attribute_custom_t *attr = NULL;
                                attr = cl->attr_array[a];
                                if (!attr) continue;
                                cJSON *attr_obj = NULL;
                                attr_obj = cJSON_CreateObject();
                                if (!attr_obj) continue;

                                cJSON_AddNumberToObject(attr_obj, "id", attr->id);
                                cJSON_AddStringToObject(attr_obj, "attr_id_text", attr->attr_id_text);
                                cJSON_AddNumberToObject(attr_obj, "type", attr->type);
                                cJSON_AddNumberToObject(attr_obj, "acces", attr->acces);
                                cJSON_AddNumberToObject(attr_obj, "size", attr->size);
                                cJSON_AddNumberToObject(attr_obj, "is_void_pointer", attr->is_void_pointer);
                                cJSON_AddNumberToObject(attr_obj, "manuf_code", attr->manuf_code);
                                cJSON_AddNumberToObject(attr_obj, "parent_cluster_id", attr->parent_cluster_id);
                                cJSON_AddItemToArray(attrs, attr_obj);
                            }
                            cJSON_AddItemToObject(cl_obj, "attributes", attrs);
                        }
                        cJSON_AddItemToArray(unk_input, cl_obj);
                    }
                    cJSON_AddItemToObject(ep_obj, "unknown_input_clusters", unk_input);
                }
            } else {
                ESP_LOGD(TAG, "zb_manager_get_device_json: EP %d has NO unknown input clusters (count=%d, ptr=%p)",
                        ep->ep_id, ep->UnKnowninputClusterCount, ep->UnKnowninputClusters_array);
            }

            cJSON_AddNumberToObject(ep_obj, "outpclcount", ep->output_clusters_count);
            // Output Clusters
            if (ep->output_clusters_count > 0 && ep->output_clusters_array) {
                cJSON *out_clusters = NULL;
                out_clusters = cJSON_CreateArray();
                if (out_clusters) {
                    ESP_LOGD(TAG, "zb_manager_get_device_json: EP %d has %d output clusters", ep->ep_id, ep->output_clusters_count);
                    for (int k = 0; k < ep->output_clusters_count; k++) {
                        cJSON_AddItemToArray(out_clusters, cJSON_CreateNumber(ep->output_clusters_array[k]));
                    }
                    cJSON_AddItemToObject(ep_obj, "output_clusters", out_clusters);
                }
            }

            cJSON_AddNumberToObject(ep_obj, "unkoutpclcount", ep->UnKnownoutputClusterCount);
            // Unknown Output Clusters
            if (ep->UnKnownoutputClusterCount > 0 && ep->UnKnownoutputClusters_array) {
                cJSON *unk_output = NULL;
                unk_output = cJSON_CreateArray();
                if (unk_output) {
                    ESP_LOGD(TAG, "zb_manager_get_device_json: EP %d has %d unknown output clusters", ep->ep_id, ep->UnKnownoutputClusterCount);
                    for (int k = 0; k < ep->UnKnownoutputClusterCount; k++) {
                        cluster_custom_t *cl = NULL;
                        cl = ep->UnKnownoutputClusters_array[k];
                        if (!cl) {
                            ESP_LOGE(TAG, "zb_manager_get_device_json: UnKnownoutputClusters_array[%d] is NULL", k);
                            continue;
                        }
                        cJSON *cl_obj = NULL;
                        cl_obj = cJSON_CreateObject();
                        if (!cl_obj) continue;

                        cJSON_AddNumberToObject(cl_obj, "id", cl->id);
                        cJSON_AddStringToObject(cl_obj, "cluster_id_text", cl->cluster_id_text);
                        cJSON_AddNumberToObject(cl_obj, "role_mask", cl->role_mask);
                        cJSON_AddNumberToObject(cl_obj, "manuf_code", cl->manuf_code);
                        cJSON_AddNumberToObject(cl_obj, "is_use_on_device", cl->is_use_on_device);

                        cJSON *attrs = NULL;
                        attrs = cJSON_CreateArray();
                        if (attrs) {
                            for (int a = 0; a < cl->attr_count; a++) {
                                attribute_custom_t *attr = NULL;
                                attr = cl->attr_array[a];
                                if (!attr) continue;
                                cJSON *attr_obj = NULL;
                                attr_obj = cJSON_CreateObject();
                                if (!attr_obj) continue;

                                cJSON_AddNumberToObject(attr_obj, "id", attr->id);
                                cJSON_AddStringToObject(attr_obj, "attr_id_text", attr->attr_id_text);
                                cJSON_AddNumberToObject(attr_obj, "type", attr->type);
                                cJSON_AddNumberToObject(attr_obj, "acces", attr->acces);
                                cJSON_AddNumberToObject(attr_obj, "size", attr->size);
                                cJSON_AddNumberToObject(attr_obj, "is_void_pointer", attr->is_void_pointer);
                                cJSON_AddNumberToObject(attr_obj, "manuf_code", attr->manuf_code);
                                cJSON_AddNumberToObject(attr_obj, "parent_cluster_id", attr->parent_cluster_id);
                                cJSON_AddItemToArray(attrs, attr_obj);
                            }
                            cJSON_AddItemToObject(cl_obj, "attributes", attrs);
                        }
                        cJSON_AddItemToArray(unk_output, cl_obj);
                    }
                    cJSON_AddItemToObject(ep_obj, "unknown_output_clusters", unk_output);
                }
            } else {
                ESP_LOGD(TAG, "zb_manager_get_device_json: EP %d has NO unknown output clusters (count=%d, ptr=%p)",
                        ep->ep_id, ep->UnKnownoutputClusterCount, ep->UnKnownoutputClusters_array);
            }

            cJSON_AddItemToArray(eps, ep_obj);
        }
    } // end if endpoints count > 0
    cJSON_AddItemToObject(dev_obj, "endpoints", eps);
    ESP_LOGD(TAG, "zb_manager_get_device_json: Successfully built JSON for device %s", ieee_str);
    return dev_obj;
}

//********************************************************************************************************************************

//================================================================================================================================
//========================================================== ZBM_DEV_BASE_TO_JSON_SAFE ===========================================
//================================================================================================================================
cJSON *zbm_dev_base_to_json_safe()
{
    cJSON *root = NULL;
    if (xSemaphoreTake(zbm_g_device_array_mutex, pdMS_TO_TICKS(ZBM_BASE_MUTEX_TIMEOUT_LONG_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex in zb_manager_get_full_json_from_remote_devices_array_safe");
        return root;
    }

    root = cJSON_CreateArray();
    if (root)
    {
        for (int i = 0; i < zbm_RemoteDevicesCount; i++) {
            device_custom_t *dev = NULL;
            dev = zbm_RemoteDevicesArray[i];
            if (!dev) continue;
            cJSON *dev_json = NULL;
            dev_json = zbm_dev_base_device_to_json(dev);
            if (!dev_json) continue;
            if (dev_json) {
                cJSON_AddItemToArray(root, dev_json);
            }
        }
    } else {ESP_LOGE(TAG,"root = cJSON_CreateArray(); FAIL");}
    xSemaphoreGive(zbm_g_device_array_mutex);
    return root;
}
//********************************************************************************************************************************

//================================================================================================================================
//========================================================== ZBM_DEV_BASE_SAVE ===================================================
//================================================================================================================================
static esp_err_t zbm_dev_base_save(const char *filepath)
{
    //cJSON *root = zb_manager_get_full_json_from_remote_devices_array_old();
    cJSON *root = zbm_dev_base_to_json_safe();
    if (!root) {
        ESP_LOGE(TAG, "Failed to generate JSON object for saving devices");
        return ESP_FAIL;
    }

    char *json_str = cJSON_PrintUnformatted(root);
    if (!json_str) {
        ESP_LOGE(TAG, "Failed to print JSON");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    FILE *file = fopen(filepath, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open %s for writing", filepath);
        cJSON_Delete(root);
        free(json_str);
        return ESP_FAIL;
    }

    fwrite(json_str, 1, strlen(json_str), file);
    fclose(file);
    free(json_str);
    cJSON_Delete(root);

    ESP_LOGD(TAG, "Devices saved to %s", filepath);
    return ESP_OK;
}
//********************************************************************************************************************************

//================================================================================================================================
//===================================================== ZBM_DEV_BASE_QUEUE_REQ_CMD ===============================================
//================================================================================================================================
esp_err_t  zbm_dev_base_queue_save_req_cmd()
{
    if (zbm_base_dev_save_to_file_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Save queue is NULL!");
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t cmd = SAVE_TASK_CMD_SAVE;
    BaseType_t ret = xQueueSend(zbm_base_dev_save_to_file_cmd_queue, &cmd, 0);
    if (ret == pdPASS) {
        ESP_LOGD(TAG, "Save request queued");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to queue save request (queue full)");
        return ESP_ERR_TIMEOUT;
    }
}
//********************************************************************************************************************************

//================================================================================================================================
//========================================================== ZBM_DEV_BASE_LOAD ===================================================
//================================================================================================================================
static esp_err_t    zbm_dev_base_load(const char *filepath)
{
    ESP_LOGW(TAG, "zbm_dev_base_load:processing");
    FILE *f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGW(TAG, "No JSON file found: %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *json_str = malloc(len + 1);
    if (!json_str) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    fread(json_str, 1, len, f);
    json_str[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) {
        ESP_LOGE(TAG, "JSON parse failed: %s", cJSON_GetErrorPtr());
        return ESP_ERR_INVALID_ARG;
    }

    // Очистка старых устройств
    for (int i = 0; i < zbm_RemoteDevicesCount; i++) {
        if (zbm_RemoteDevicesArray[i]) {
            zbm_dev_base_dev_delete_safe(zbm_RemoteDevicesArray[i]);
            zbm_RemoteDevicesArray[i] = NULL;
        }
    }

    // === БЛОКИРОВКА ПЕРЕД ЗАГРУЗКОЙ ===
    if (xSemaphoreTake(zbm_g_device_array_mutex, pdMS_TO_TICKS(ZBM_BASE_MUTEX_TIMEOUT_LONG_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take device mutex in zb_manager_load_devices_from_json");
        cJSON_Delete(root);
        return ESP_ERR_TIMEOUT;
    }
    cJSON *dev_item = NULL;
    cJSON_ArrayForEach(dev_item, root) {
        device_custom_t *dev = NULL;
        dev = calloc(1, sizeof(device_custom_t));
        if (!dev) {
            ESP_LOGE(TAG, "Failed to allocate memory for device");
            continue;
        }

        // IEEE
        cJSON *ieee_obj = NULL;
        ieee_obj = cJSON_GetObjectItem(dev_item, "ieee_addr");
        if (!ieee_obj || !str_to_ieee(ieee_obj->valuestring, dev->ieee_addr)) {
            ESP_LOGE(TAG, "Invalid or missing IEEE address");
            free(dev);
            continue;
        }

        dev->short_addr = cJSON_GetObjectItem(dev_item, "short_addr")->valueint;
        dev->is_in_build_status = cJSON_GetObjectItem(dev_item, "is_in_build_status")->valueint;
        dev->index_in_array = cJSON_GetObjectItem(dev_item, "index_in_array")->valueint;

        const char *name = cJSON_GetObjectItem(dev_item, "friendly_name")->valuestring;
        strlcpy(dev->friendly_name, name ? name : "", sizeof(dev->friendly_name));

        dev->friendly_name_len = strlen(dev->friendly_name);
        dev->capability = cJSON_GetObjectItem(dev_item, "capability")->valueint;
        dev->lqi = cJSON_GetObjectItem(dev_item, "lqi")->valueint;

        cJSON *last_seen_item = cJSON_GetObjectItem(dev_item, "last_seen_ms");
        if (last_seen_item) {
            dev->last_seen_ms = 0; // Сбрасываем — будет обновлено при активности
            dev->is_online = false;
        }

        cJSON *timeout_item = cJSON_GetObjectItem(dev_item, "device_timeout_ms");
        if (timeout_item && cJSON_IsNumber(timeout_item)) {
            dev->device_timeout_ms = timeout_item->valueint;
        } else {
            dev->device_timeout_ms = ZB_DEVICE_DEFAULT_TIMEOUT_MS;
        }

        // Manufacturer code
        cJSON *manuf_code_obj = cJSON_GetObjectItem(dev_item, "manufacturer_code");
        if (manuf_code_obj) {
            dev->manufacturer_code = (uint16_t)manuf_code_obj->valueint;
        }
        
        // Device-level Basic Cluster
        cJSON *dev_basic = NULL;
        dev_basic = cJSON_GetObjectItem(dev_item, "device_basic_cluster");
        if (dev_basic) {
            dev->server_BasicClusterObj = calloc(1, sizeof(zigbee_manager_basic_cluster_t));
            if (dev->server_BasicClusterObj) {
                dev->server_BasicClusterObj->zcl_version = cJSON_GetObjectItem(dev_basic, "zcl_version")->valueint;
                dev->server_BasicClusterObj->application_version = cJSON_GetObjectItem(dev_basic, "app_version")->valueint;
                dev->server_BasicClusterObj->stack_version = cJSON_GetObjectItem(dev_basic, "stack_version")->valueint;
                dev->server_BasicClusterObj->hw_version = cJSON_GetObjectItem(dev_basic, "hw_version")->valueint;

                LOAD_STRING(dev_basic, "manufacturer_name", dev->server_BasicClusterObj->manufacturer_name);
                LOAD_STRING(dev_basic, "model_id", dev->server_BasicClusterObj->model_identifier);
                LOAD_STRING(dev_basic, "date_code", dev->server_BasicClusterObj->date_code);
                LOAD_STRING(dev_basic, "location", dev->server_BasicClusterObj->location_description);

                cJSON *ps_item = cJSON_GetObjectItem(dev_basic, "power_source");
                if (ps_item && cJSON_IsNumber(ps_item)) {
                    dev->server_BasicClusterObj->power_source = ps_item->valueint;
                }

                dev->server_BasicClusterObj->physical_environment = cJSON_GetObjectItem(dev_basic, "env")->valueint;
                dev->server_BasicClusterObj->device_enabled = cJSON_GetObjectItem(dev_basic, "enabled")->type == cJSON_True;

                cJSON *last_update_item = cJSON_GetObjectItem(dev_basic, "last_update_ms");
                if (last_update_item && cJSON_IsNumber(last_update_item)) {
                    dev->server_BasicClusterObj->last_update_ms = last_update_item->valueint;
                } else {
                    dev->server_BasicClusterObj->last_update_ms = esp_log_timestamp();
                }

                // === Загрузка нестандартных атрибутов ===
                cJSON *nostandart_attrs = cJSON_GetObjectItem(dev_basic, "nostandart_attributes");
                if (nostandart_attrs && cJSON_IsArray(nostandart_attrs)) {
                    int count = cJSON_GetArraySize(nostandart_attrs);
                    dev->server_BasicClusterObj->nostandart_attr_count = count;
                    dev->server_BasicClusterObj->nostandart_attr_array = calloc(count, sizeof(attribute_custom_t*));
                    if (dev->server_BasicClusterObj->nostandart_attr_array) {
                        bool alloc_failed = false;
                        for (int a = 0; a < count; a++) {
                            cJSON *attr_obj = cJSON_GetArrayItem(nostandart_attrs, a);
                            if (!attr_obj) continue;

                            attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
                            if (!attr) {
                                alloc_failed = true;
                                continue;
                            }

                            attr->id = cJSON_GetObjectItem(attr_obj, "id")->valueint;
                            LOAD_STRING(attr_obj, "attr_id_text", attr->attr_id_text);
                            attr->type = cJSON_GetObjectItem(attr_obj, "type")->valueint;
                            attr->acces = cJSON_GetObjectItem(attr_obj, "acces")->valueint;
                            attr->size = cJSON_GetObjectItem(attr_obj, "size")->valueint;
                            attr->is_void_pointer = cJSON_GetObjectItem(attr_obj, "is_void_pointer")->valueint;
                            attr->manuf_code = cJSON_GetObjectItem(attr_obj, "manuf_code")->valueint;
                            attr->parent_cluster_id = cJSON_GetObjectItem(attr_obj, "parent_cluster_id")->valueint;

                            // p_value — пока NULL, можно загрузить позже по типу
                            attr->p_value = NULL;

                            dev->server_BasicClusterObj->nostandart_attr_array[a] = attr;
                        }
                        if (alloc_failed) {
                            ESP_LOGW(TAG, "Partial failure loading nostandart_attributes for Basic cluster");
                        }
                    }
                }

            }
        }

        // Power Configuration Cluster
        cJSON *dev_power_config_item = NULL;
        dev_power_config_item = cJSON_GetObjectItem(dev_item, "device_power_config_cluster");
        if (dev_power_config_item) {
            dev->server_PowerConfigurationClusterObj = calloc(1, sizeof(zb_manager_power_config_cluster_t));
            if (dev->server_PowerConfigurationClusterObj) {
                LOAD_NUMBER(dev_power_config_item, "battery_voltage", dev->server_PowerConfigurationClusterObj->battery_voltage);
                LOAD_NUMBER(dev_power_config_item, "battery_percentage", dev->server_PowerConfigurationClusterObj->battery_percentage);
                LOAD_STRING(dev_power_config_item, "battery_manufacturer", dev->server_PowerConfigurationClusterObj->battery_manufacturer);
                LOAD_NUMBER(dev_power_config_item, "battery_size", dev->server_PowerConfigurationClusterObj->battery_size);
                LOAD_NUMBER(dev_power_config_item, "battery_a_hr_rating", dev->server_PowerConfigurationClusterObj->battery_a_hr_rating);
                LOAD_NUMBER(dev_power_config_item, "battery_quantity", dev->server_PowerConfigurationClusterObj->battery_quantity);
                LOAD_NUMBER(dev_power_config_item, "battery_rated_voltage", dev->server_PowerConfigurationClusterObj->battery_rated_voltage);
                LOAD_NUMBER(dev_power_config_item, "battery_alarm_mask", dev->server_PowerConfigurationClusterObj->battery_alarm_mask);
                LOAD_NUMBER(dev_power_config_item, "battery_voltage_min_th", dev->server_PowerConfigurationClusterObj->battery_voltage_min_th);
                LOAD_NUMBER(dev_power_config_item, "battery_voltage_th1", dev->server_PowerConfigurationClusterObj->battery_voltage_th1);
                LOAD_NUMBER(dev_power_config_item, "battery_voltage_th2", dev->server_PowerConfigurationClusterObj->battery_voltage_th2);
                LOAD_NUMBER(dev_power_config_item, "battery_voltage_th3", dev->server_PowerConfigurationClusterObj->battery_voltage_th3);
                LOAD_NUMBER(dev_power_config_item, "battery_percentage_min_th", dev->server_PowerConfigurationClusterObj->battery_percentage_min_th);
                LOAD_NUMBER(dev_power_config_item, "battery_percentage_th1", dev->server_PowerConfigurationClusterObj->battery_percentage_th1);
                LOAD_NUMBER(dev_power_config_item, "battery_percentage_th2", dev->server_PowerConfigurationClusterObj->battery_percentage_th2);
                LOAD_NUMBER(dev_power_config_item, "battery_percentage_th3", dev->server_PowerConfigurationClusterObj->battery_percentage_th3);
                LOAD_NUMBER(dev_power_config_item, "battery_alarm_state", dev->server_PowerConfigurationClusterObj->battery_alarm_state);
                LOAD_NUMBER(dev_power_config_item, "mains_voltage", dev->server_PowerConfigurationClusterObj->mains_voltage);
                LOAD_NUMBER(dev_power_config_item, "mains_frequency", dev->server_PowerConfigurationClusterObj->mains_frequency);
                LOAD_NUMBER(dev_power_config_item, "mains_alarm_mask", dev->server_PowerConfigurationClusterObj->mains_alarm_mask);
                LOAD_NUMBER(dev_power_config_item, "mains_voltage_min_th", dev->server_PowerConfigurationClusterObj->mains_voltage_min_th);
                LOAD_NUMBER(dev_power_config_item, "mains_voltage_max_th", dev->server_PowerConfigurationClusterObj->mains_voltage_max_th);
                LOAD_NUMBER(dev_power_config_item, "mains_dwell_trip_point", dev->server_PowerConfigurationClusterObj->mains_dwell_trip_point);

                cJSON *last_update_item = cJSON_GetObjectItem(dev_power_config_item, "last_update_ms");
                if (last_update_item && cJSON_IsNumber(last_update_item)) {
                    dev->server_PowerConfigurationClusterObj->last_update_ms = last_update_item->valueint;
                } else {
                    dev->server_PowerConfigurationClusterObj->last_update_ms = esp_log_timestamp();
                }

                // === Загрузка нестандартных атрибутов ===
                cJSON *nostandart_attrs = cJSON_GetObjectItem(dev_power_config_item, "nostandart_attributes");
                if (nostandart_attrs && cJSON_IsArray(nostandart_attrs)) {
                    int count = cJSON_GetArraySize(nostandart_attrs);
                    dev->server_PowerConfigurationClusterObj->nostandart_attr_count = count;
                    dev->server_PowerConfigurationClusterObj->nostandart_attr_array = calloc(count, sizeof(attribute_custom_t*));
                    if (dev->server_PowerConfigurationClusterObj->nostandart_attr_array) {
                        bool alloc_failed = false;
                        for (int a = 0; a < count; a++) {
                            cJSON *attr_obj = cJSON_GetArrayItem(nostandart_attrs, a);
                            if (!attr_obj) continue;

                            attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
                            if (!attr) {
                                alloc_failed = true;
                                continue;
                            }

                            attr->id = cJSON_GetObjectItem(attr_obj, "id")->valueint;
                            LOAD_STRING(attr_obj, "attr_id_text", attr->attr_id_text);
                            attr->type = cJSON_GetObjectItem(attr_obj, "type")->valueint;
                            attr->acces = cJSON_GetObjectItem(attr_obj, "acces")->valueint;
                            attr->size = cJSON_GetObjectItem(attr_obj, "size")->valueint;
                            attr->is_void_pointer = cJSON_GetObjectItem(attr_obj, "is_void_pointer")->valueint;
                            attr->manuf_code = cJSON_GetObjectItem(attr_obj, "manuf_code")->valueint;
                            attr->parent_cluster_id = cJSON_GetObjectItem(attr_obj, "parent_cluster_id")->valueint;

                            // p_value — пока NULL, можно загрузить позже по типу
                            attr->p_value = NULL;

                            dev->server_PowerConfigurationClusterObj->nostandart_attr_array[a] = attr;
                        }
                        if (alloc_failed) {
                            ESP_LOGW(TAG, "Partial failure loading nostandart_attributes for Basic cluster");
                        }
                    }
                }

            }
        }

        // Endpoints
        dev->endpoints_count = cJSON_GetObjectItem(dev_item, "endpointscount")->valueint;
        if (dev->endpoints_count == 0)
        {
            continue;
        }
        cJSON *eps = cJSON_GetObjectItem(dev_item, "endpoints");
        if (cJSON_IsArray(eps)) {
            //dev->endpoints_count = cJSON_GetArraySize(eps);
            dev->endpoints_array = calloc(dev->endpoints_count, sizeof(endpoint_custom_t*));
            if (!dev->endpoints_array) {
                ESP_LOGE(TAG, "Failed to allocate endpoints_array for device %02x:%02x...", dev->ieee_addr[7], dev->ieee_addr[6]);
                free(dev);
                continue;
            }

            for (int j = 0; j < dev->endpoints_count; j++) {
                cJSON *ep_obj = NULL;
                ep_obj = cJSON_GetArrayItem(eps, j);
                if (!ep_obj) {
                    ESP_LOGW(TAG, "Missing endpoint at index %d", j);
                    continue;
                }

                endpoint_custom_t *ep = NULL;
                ep = calloc(1, sizeof(endpoint_custom_t));
                if (!ep) {
                    ESP_LOGE(TAG, "Failed to allocate memory for endpoint %d", j);
                    continue;
                }

                ep->ep_id = cJSON_GetObjectItem(ep_obj, "ep_id")->valueint;
                ep->is_use_on_device = cJSON_GetObjectItem(ep_obj, "is_use_on_device")->valueint;

                const char *ep_name = cJSON_GetObjectItem(ep_obj, "friendly_name")->valuestring;
                strlcpy(ep->friendly_name, ep_name ? ep_name : "", sizeof(ep->friendly_name));

                ep->deviceId = cJSON_GetObjectItem(ep_obj, "deviceId")->valueint;

                const char *dev_id_name = cJSON_GetObjectItem(ep_obj, "device_type")->valuestring;
                strlcpy(ep->device_Id_text, ep_name ? ep_name : "", sizeof(ep->device_Id_text));
                /*cJSON *dev_type_obj = cJSON_GetObjectItem(ep_obj, "device_type");
                if (dev_type_obj && dev_type_obj->valuestring) {
                    strlcpy(ep->device_Id_text, dev_type_obj->valuestring, sizeof(ep->device_Id_text));
                }*/

                // Identify Cluster
                cJSON *identify = NULL;
                identify = cJSON_GetObjectItem(ep_obj, "identify");
                if (identify) {
                    ep->is_use_identify_cluster = 1;
                    ep->server_IdentifyClusterObj = calloc(1, sizeof(zb_manager_identify_cluster_t));
                    if (ep->server_IdentifyClusterObj) {
                        //ep->server_IdentifyClusterObj->cluster_Id_name
                        ep->server_IdentifyClusterObj->identify_time = cJSON_GetObjectItem(identify, "identify_time")->valueint;
                    }
                }

                // Temperature Cluster
                cJSON *temp = NULL;
                temp = cJSON_GetObjectItem(ep_obj, "temperature");
                if (temp) {
                    ep->is_use_temperature_measurement_cluster = 1;
                    ep->server_TemperatureMeasurementClusterObj = calloc(1, sizeof(zb_manager_temperature_measurement_cluster_t));
                    if (ep->server_TemperatureMeasurementClusterObj) {
                        ep->server_TemperatureMeasurementClusterObj->measured_value = cJSON_GetObjectItem(temp, "measured_value")->valueint;
                        ep->server_TemperatureMeasurementClusterObj->min_measured_value = cJSON_GetObjectItem(temp, "min_measured_value")->valueint;
                        ep->server_TemperatureMeasurementClusterObj->max_measured_value = cJSON_GetObjectItem(temp, "max_measured_value")->valueint;
                        ep->server_TemperatureMeasurementClusterObj->tolerance = cJSON_GetObjectItem(temp, "tolerance")->valueint;
                        ep->server_TemperatureMeasurementClusterObj->last_update_ms = cJSON_GetObjectItem(temp, "last_update_ms")->valueint;
                        ep->server_TemperatureMeasurementClusterObj->read_error = cJSON_GetObjectItem(temp, "read_error")->type == cJSON_True;

                        // === Загрузка нестандартных атрибутов ===
                        cJSON *nostandart_attrs = cJSON_GetObjectItem(temp, "nostandart_attributes");
                        if (nostandart_attrs && cJSON_IsArray(nostandart_attrs)) {
                            int count = cJSON_GetArraySize(nostandart_attrs);
                            ep->server_TemperatureMeasurementClusterObj->nostandart_attr_count = count;
                            ep->server_TemperatureMeasurementClusterObj->nostandart_attr_array = calloc(count, sizeof(attribute_custom_t*));
                            if (ep->server_TemperatureMeasurementClusterObj->nostandart_attr_array) {
                                bool alloc_failed = false;
                                for (int a = 0; a < count; a++) {
                                    cJSON *attr_obj = cJSON_GetArrayItem(nostandart_attrs, a);
                                    if (!attr_obj) continue;

                                    attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
                                    if (!attr) {
                                        alloc_failed = true;
                                        continue;
                                    }

                                    attr->id = cJSON_GetObjectItem(attr_obj, "id")->valueint;
                                    LOAD_STRING(attr_obj, "attr_id_text", attr->attr_id_text);
                                    attr->type = cJSON_GetObjectItem(attr_obj, "type")->valueint;
                                    attr->acces = cJSON_GetObjectItem(attr_obj, "acces")->valueint;
                                    attr->size = cJSON_GetObjectItem(attr_obj, "size")->valueint;
                                    attr->is_void_pointer = cJSON_GetObjectItem(attr_obj, "is_void_pointer")->valueint;
                                    attr->manuf_code = cJSON_GetObjectItem(attr_obj, "manuf_code")->valueint;
                                    attr->parent_cluster_id = cJSON_GetObjectItem(attr_obj, "parent_cluster_id")->valueint;

                                    // p_value — пока NULL, можно загрузить позже по типу
                                    attr->p_value = NULL;

                                    ep->server_TemperatureMeasurementClusterObj->nostandart_attr_array[a] = attr;
                                }
                                if (alloc_failed) {
                                    ESP_LOGW(TAG, "Partial failure loading nostandart_attributes for Basic cluster");
                                }
                            }
                        }
                    }
                }

                // Humidity Cluster
                cJSON *hum = NULL;
                hum = cJSON_GetObjectItem(ep_obj, "humidity");
                if (hum) {
                    ep->is_use_humidity_measurement_cluster = 1;
                    ep->server_HumidityMeasurementClusterObj = calloc(1, sizeof(zb_manager_humidity_measurement_cluster_t));
                    if (ep->server_HumidityMeasurementClusterObj) {
                        ep->server_HumidityMeasurementClusterObj->measured_value = cJSON_GetObjectItem(hum, "measured_value")->valueint;
                        ep->server_HumidityMeasurementClusterObj->min_measured_value = cJSON_GetObjectItem(hum, "min_measured_value")->valueint;
                        ep->server_HumidityMeasurementClusterObj->max_measured_value = cJSON_GetObjectItem(hum, "max_measured_value")->valueint;
                        ep->server_HumidityMeasurementClusterObj->tolerance = cJSON_GetObjectItem(hum, "tolerance")->valueint;
                        ep->server_HumidityMeasurementClusterObj->last_update_ms = cJSON_GetObjectItem(hum, "last_update_ms")->valueint;
                        ep->server_HumidityMeasurementClusterObj->read_error = cJSON_GetObjectItem(hum, "read_error")->type == cJSON_True;

                        // === Загрузка нестандартных атрибутов ===
                        cJSON *nostandart_attrs = cJSON_GetObjectItem(hum, "nostandart_attributes");
                        if (nostandart_attrs && cJSON_IsArray(nostandart_attrs)) {
                            int count = cJSON_GetArraySize(nostandart_attrs);
                            ep->server_HumidityMeasurementClusterObj->nostandart_attr_count = count;
                            ep->server_HumidityMeasurementClusterObj->nostandart_attr_array = calloc(count, sizeof(attribute_custom_t*));
                            if (ep->server_HumidityMeasurementClusterObj->nostandart_attr_array) {
                                bool alloc_failed = false;
                                for (int a = 0; a < count; a++) {
                                    cJSON *attr_obj = cJSON_GetArrayItem(nostandart_attrs, a);
                                    if (!attr_obj) continue;

                                    attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
                                    if (!attr) {
                                        alloc_failed = true;
                                        continue;
                                    }

                                    attr->id = cJSON_GetObjectItem(attr_obj, "id")->valueint;
                                    LOAD_STRING(attr_obj, "attr_id_text", attr->attr_id_text);
                                    attr->type = cJSON_GetObjectItem(attr_obj, "type")->valueint;
                                    attr->acces = cJSON_GetObjectItem(attr_obj, "acces")->valueint;
                                    attr->size = cJSON_GetObjectItem(attr_obj, "size")->valueint;
                                    attr->is_void_pointer = cJSON_GetObjectItem(attr_obj, "is_void_pointer")->valueint;
                                    attr->manuf_code = cJSON_GetObjectItem(attr_obj, "manuf_code")->valueint;
                                    attr->parent_cluster_id = cJSON_GetObjectItem(attr_obj, "parent_cluster_id")->valueint;

                                    // p_value — пока NULL, можно загрузить позже по типу
                                    attr->p_value = NULL;

                                    ep->server_HumidityMeasurementClusterObj->nostandart_attr_array[a] = attr;
                                }
                                if (alloc_failed) {
                                    ESP_LOGW(TAG, "Partial failure loading nostandart_attributes for Basic cluster");
                                }
                            }
                        }
                    }
                }

                // ON/OFF Cluster
                cJSON *onoff = NULL;
                onoff = cJSON_GetObjectItem(ep_obj, "onoff");
                if (onoff) {
                    ep->is_use_on_off_cluster = 1;
                    ep->server_OnOffClusterObj = calloc(1, sizeof(zb_manager_on_off_cluster_t));
                    if (ep->server_OnOffClusterObj) {
                        ep->server_OnOffClusterObj->on_off = cJSON_GetObjectItem(onoff, "on")->type == cJSON_True;
                        ep->server_OnOffClusterObj->on_time = cJSON_GetObjectItem(onoff, "on_time")->valueint;
                        ep->server_OnOffClusterObj->off_wait_time = cJSON_GetObjectItem(onoff, "off_wait_time")->valueint;
                        ep->server_OnOffClusterObj->start_up_on_off = cJSON_GetObjectItem(onoff, "start_up")->valueint;
                        ep->server_OnOffClusterObj->last_update_ms = cJSON_GetObjectItem(onoff, "last_update_ms")->valueint;
                        // === Загрузка нестандартных атрибутов ===
                        cJSON *nostandart_attrs = cJSON_GetObjectItem(onoff, "nostandart_attributes");
                        if (nostandart_attrs && cJSON_IsArray(nostandart_attrs)) {
                            int count = cJSON_GetArraySize(nostandart_attrs);
                            ep->server_OnOffClusterObj->nostandart_attr_count = count;
                            ep->server_OnOffClusterObj->nostandart_attr_array = calloc(count, sizeof(attribute_custom_t*));
                            if (ep->server_OnOffClusterObj->nostandart_attr_array) {
                                bool alloc_failed = false;
                                for (int a = 0; a < count; a++) {
                                    cJSON *attr_obj = cJSON_GetArrayItem(nostandart_attrs, a);
                                    if (!attr_obj) continue;

                                    attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
                                    if (!attr) {
                                        alloc_failed = true;
                                        continue;
                                    }

                                    attr->id = cJSON_GetObjectItem(attr_obj, "id")->valueint;
                                    LOAD_STRING(attr_obj, "attr_id_text", attr->attr_id_text);
                                    attr->type = cJSON_GetObjectItem(attr_obj, "type")->valueint;
                                    attr->acces = cJSON_GetObjectItem(attr_obj, "acces")->valueint;
                                    attr->size = cJSON_GetObjectItem(attr_obj, "size")->valueint;
                                    attr->is_void_pointer = cJSON_GetObjectItem(attr_obj, "is_void_pointer")->valueint;
                                    attr->manuf_code = cJSON_GetObjectItem(attr_obj, "manuf_code")->valueint;
                                    attr->parent_cluster_id = cJSON_GetObjectItem(attr_obj, "parent_cluster_id")->valueint;

                                    // p_value — пока NULL, можно загрузить позже по типу
                                    attr->p_value = NULL;

                                    ep->server_OnOffClusterObj->nostandart_attr_array[a] = attr;
                                }
                                if (alloc_failed) {
                                    ESP_LOGW(TAG, "Partial failure loading nostandart_attributes for Basic cluster");
                                }
                            }
                        }
                    }
                }

                

                // === Загрузка unknown_input_clusters ===
                ep->UnKnowninputClusterCount = cJSON_GetObjectItem(ep_obj, "unkinpclcount")->valueint;
                if (ep->UnKnowninputClusterCount > 0)
                {
                    cJSON *unk_input = NULL;
                    unk_input = cJSON_GetObjectItem(ep_obj, "unknown_input_clusters");
                    if (unk_input && cJSON_IsArray(unk_input)) {
                        //int count = cJSON_GetArraySize(unk_input);
                        //ep->UnKnowninputClusterCount = count;
                        ep->UnKnowninputClusters_array = NULL;
                        ep->UnKnowninputClusters_array = calloc(ep->UnKnowninputClusterCount, sizeof(cluster_custom_t*));
                        if (!ep->UnKnowninputClusters_array) {
                            ESP_LOGE(TAG, "Failed to allocate UnKnowninputClusters_array for EP %d", ep->ep_id);
                        } else {
                            bool alloc_failed = false;
                            for (int k = 0; k < ep->UnKnowninputClusterCount; k++) {
                                cJSON *cl_obj = NULL;
                                cl_obj = cJSON_GetArrayItem(unk_input, k);
                                if (!cl_obj) {
                                    ESP_LOGW(TAG, "Missing unknown_input_cluster at index %d", k);
                                    continue;
                                }

                                cluster_custom_t *cl = NULL;
                                cl = calloc(1, sizeof(cluster_custom_t));
                                if (!cl) {
                                    ESP_LOGE(TAG, "Failed to allocate cluster for input cluster %d", k);
                                    alloc_failed = true;
                                    continue;
                                }

                                cl->id = cJSON_GetObjectItem(cl_obj, "id")->valueint;
                                LOAD_STRING(cl_obj, "cluster_id_text", cl->cluster_id_text);
                                cl->role_mask = cJSON_GetObjectItem(cl_obj, "role_mask")->valueint;
                                cl->manuf_code = cJSON_GetObjectItem(cl_obj, "manuf_code")->valueint;
                                cl->is_use_on_device = cJSON_GetObjectItem(cl_obj, "is_use_on_device")->valueint;

                                cJSON *attrs = NULL;
                                attrs = cJSON_GetObjectItem(cl_obj, "attributes");
                                if (attrs && cJSON_IsArray(attrs)) {
                                    int attr_count = cJSON_GetArraySize(attrs);
                                    cl->attr_count = attr_count;
                                    if (cl->attr_count > 0)
                                    {
                                        cl->attr_array = calloc(attr_count, sizeof(attribute_custom_t*));
                                        if (!cl->attr_array) {
                                            ESP_LOGE(TAG, "Failed to allocate attr_array for cluster %d", k);
                                            free(cl);
                                            alloc_failed = true;
                                            continue;
                                        }

                                        for (int a = 0; a < attr_count; a++) {
                                            cJSON *attr_obj = cJSON_GetArrayItem(attrs, a);
                                            if (!attr_obj) continue;

                                            attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
                                            if (!attr) continue;

                                            attr->id = cJSON_GetObjectItem(attr_obj, "id")->valueint;
                                            LOAD_STRING(attr_obj, "attr_id_text", attr->attr_id_text);
                                            attr->type = cJSON_GetObjectItem(attr_obj, "type")->valueint;
                                            attr->acces = cJSON_GetObjectItem(attr_obj, "acces")->valueint;
                                            attr->size = cJSON_GetObjectItem(attr_obj, "size")->valueint;
                                            attr->is_void_pointer = cJSON_GetObjectItem(attr_obj, "is_void_pointer")->valueint;
                                            attr->manuf_code = cJSON_GetObjectItem(attr_obj, "manuf_code")->valueint;
                                            attr->parent_cluster_id = cJSON_GetObjectItem(attr_obj, "parent_cluster_id")->valueint;
                                            attr->p_value = NULL;

                                            cl->attr_array[a] = attr;
                                        }
                                    }
                                }

                                ep->UnKnowninputClusters_array[k] = cl;
                            }
                            if (alloc_failed) {
                                ESP_LOGW(TAG, "Partial failure in loading unknown_input_clusters for EP %d", ep->ep_id);
                            }
                        }
                    }
                } // end if count > 0
                // Output Clusters
                ep->output_clusters_count = cJSON_GetObjectItem(ep_obj, "outpclcount")->valueint;
                if (ep->output_clusters_count > 0)
                {
                    cJSON *out_clusters = NULL;
                    out_clusters = cJSON_GetObjectItem(ep_obj, "output_clusters");
                    if (out_clusters && cJSON_IsArray(out_clusters)) {
                        //int count = cJSON_GetArraySize(out_clusters);
                        //ep->output_clusters_count = count;
                        ep->output_clusters_array = calloc(ep->output_clusters_count, sizeof(uint16_t));
                        if (ep->output_clusters_array) {
                            for (int k = 0; k < ep->output_clusters_count; k++) {
                                cJSON *item = cJSON_GetArrayItem(out_clusters, k);
                                if (cJSON_IsNumber(item)) {
                                    ep->output_clusters_array[k] = (uint16_t)item->valueint;
                                }
                            }
                        }
                    }
                }
                // === Загрузка unknown_output_clusters ===
                ep->UnKnownoutputClusterCount = cJSON_GetObjectItem(ep_obj, "unkoutpclcount")->valueint;
                if (ep->UnKnownoutputClusterCount > 0)
                {
                    cJSON *unk_output = NULL;
                    unk_output = cJSON_GetObjectItem(ep_obj, "unknown_output_clusters");
                    if (unk_output && cJSON_IsArray(unk_output)) {
                        //int count = cJSON_GetArraySize(unk_output);
                        //ep->UnKnownoutputClusterCount = count;
                        ep->UnKnownoutputClusters_array = calloc(ep->UnKnownoutputClusterCount, sizeof(cluster_custom_t*));
                        if (!ep->UnKnownoutputClusters_array) {
                            ESP_LOGE(TAG, "Failed to allocate UnKnownoutputClusters_array for EP %d", ep->ep_id);
                        } else {
                            bool alloc_failed = false;
                            for (int k = 0; k < ep->UnKnownoutputClusterCount; k++) {
                                cJSON *cl_obj = NULL;
                                cl_obj = cJSON_GetArrayItem(unk_output, k);
                                if (!cl_obj) continue;

                                cluster_custom_t *cl = NULL;
                                cl = calloc(1, sizeof(cluster_custom_t));
                                if (!cl) {
                                    ESP_LOGE(TAG, "Failed to allocate cluster for output cluster %d", k);
                                    alloc_failed = true;
                                    continue;
                                }

                                cl->id = cJSON_GetObjectItem(cl_obj, "id")->valueint;
                                LOAD_STRING(cl_obj, "cluster_id_text", cl->cluster_id_text);
                                cl->role_mask = cJSON_GetObjectItem(cl_obj, "role_mask")->valueint;
                                cl->manuf_code = cJSON_GetObjectItem(cl_obj, "manuf_code")->valueint;
                                cl->is_use_on_device = cJSON_GetObjectItem(cl_obj, "is_use_on_device")->valueint;

                                cJSON *attrs = NULL;
                                attrs = cJSON_GetObjectItem(cl_obj, "attributes");
                                if (attrs && cJSON_IsArray(attrs)) {
                                    int attr_count = cJSON_GetArraySize(attrs);
                                    cl->attr_count = attr_count;
                                    cl->attr_array = calloc(attr_count, sizeof(attribute_custom_t*));
                                    if (!cl->attr_array) {
                                        free(cl);
                                        alloc_failed = true;
                                        continue;
                                    }

                                    for (int a = 0; a < attr_count; a++) {
                                        cJSON *attr_obj = cJSON_GetArrayItem(attrs, a);
                                        if (!attr_obj) continue;

                                        attribute_custom_t *attr = calloc(1, sizeof(attribute_custom_t));
                                        if (!attr) continue;

                                        attr->id = cJSON_GetObjectItem(attr_obj, "id")->valueint;
                                        LOAD_STRING(attr_obj, "attr_id_text", attr->attr_id_text);
                                        attr->type = cJSON_GetObjectItem(attr_obj, "type")->valueint;
                                        attr->acces = cJSON_GetObjectItem(attr_obj, "acces")->valueint;
                                        attr->size = cJSON_GetObjectItem(attr_obj, "size")->valueint;
                                        attr->is_void_pointer = cJSON_GetObjectItem(attr_obj, "is_void_pointer")->valueint;
                                        attr->manuf_code = cJSON_GetObjectItem(attr_obj, "manuf_code")->valueint;
                                        attr->parent_cluster_id = cJSON_GetObjectItem(attr_obj, "parent_cluster_id")->valueint;
                                        attr->p_value = NULL;

                                        cl->attr_array[a] = attr;
                                    }
                                }else alloc_failed = true;

                                ep->UnKnownoutputClusters_array[k] = cl;
                            }
                            if (alloc_failed) {
                                ESP_LOGW(TAG, "Partial failure in loading unknown_output_clusters for EP %d", ep->ep_id);
                            }
                        }
                    }
                }
                dev->endpoints_array[j] = ep;
            }
        }

        // Найти свободный слот
        bool inserted = false;
        for (int idx = 0; idx < zbm_RemoteDevicesCount; idx++) {
            if (zbm_RemoteDevicesArray[idx] == NULL) {
                zbm_RemoteDevicesArray[idx] = dev;
                inserted = true;
                break;
            }
        }
        if (!inserted) {
            ESP_LOGE(TAG, "No free slot for device %02x:%02x...", dev->ieee_addr[7], dev->ieee_addr[6]);
            zbm_dev_base_dev_delete(dev);
        }
    }

    // Настройка таймаутов
    for (int i = 0; i < zbm_RemoteDevicesCount; i++) {
        if (zbm_RemoteDevicesArray[i]) {
            zbm_dev_configure_device_timeout(zbm_RemoteDevicesArray[i]);
        }
    }

    xSemaphoreGive(zbm_g_device_array_mutex);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Devices loaded from JSON: %s", filepath);
    return ESP_OK;
}

//********************************************************************************************************************************

//================================================================================================================================
//===================================================== ZBM_DEV_BASE_CREATE_NEW_DEVICE_OBJ =======================================
//================================================================================================================================
device_custom_t* zbm_dev_base_create_new_device_obj(esp_zb_ieee_addr_t ieee_addr)
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
    new_dev->lqi = 0;                    // ← Инициализация
    new_dev->last_seen_ms = 0;           // ← Можно обновлять при получении пакетов
    new_dev->device_timeout_ms = ZB_DEVICE_DEFAULT_TIMEOUT_MS; // ← По умолчанию
    new_dev->is_online = false;
    new_dev->last_bind_attempt_ms = 0;
    new_dev->last_pending_read_ms = 0;
    new_dev->last_status_print_log_time = 0;
    new_dev->server_BasicClusterObj = NULL;
    new_dev->server_PowerConfigurationClusterObj = NULL;
    return new_dev;
}
//********************************************************************************************************************************

//================================================================================================================================
//===================================================== ZBM_DEV_BASE_CREATE_NEW_ENDPOINT_OBJ =====================================
//================================================================================================================================
endpoint_custom_t* zbm_dev_base_create_new_endpoint_obj(uint8_t ep_id)
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
    new_ep->is_use_identify_cluster = 0;
    new_ep->server_IdentifyClusterObj = NULL;
    new_ep->is_use_temperature_measurement_cluster = 0;
    new_ep->server_TemperatureMeasurementClusterObj = NULL;
    new_ep->is_use_humidity_measurement_cluster = 0;
    new_ep->server_HumidityMeasurementClusterObj = NULL;
    new_ep->is_use_on_off_cluster = 0;
    new_ep->server_OnOffClusterObj = NULL;
    new_ep->output_clusters_count = 0;
    new_ep->output_clusters_array = NULL;

    return new_ep;
}
//********************************************************************************************************************************

//================================================================================================================================
//===================================================== ZBM_DEV_BASE_FIND_DEV_BY_SHORT ===========================================
//================================================================================================================================
static uint16_t s_last_short = 0xFFFF;
static device_custom_t* s_last_dev = NULL;

device_custom_t*    zbm_dev_base_find_device_by_short_safe(uint16_t short_addr)
{
    // Проверяем кэш
    if (s_last_dev && s_last_short == short_addr) {
        return s_last_dev;
    }

    if (xSemaphoreTake(zbm_g_device_array_mutex, pdMS_TO_TICKS(ZBM_BASE_MUTEX_TIMEOUT_SHORT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take device mutex in find_by_short");
        return NULL;
    }

    device_custom_t* dev = NULL;
    for (int i = 0; i < zbm_RemoteDevicesCount; i++) {
        if (zbm_RemoteDevicesArray[i] != NULL &&
            zbm_RemoteDevicesArray[i]->short_addr == short_addr) {
            dev = zbm_RemoteDevicesArray[i];
            break;
        }
    }

    // Обновляем кэш
    s_last_short = short_addr;
    s_last_dev = dev;

    xSemaphoreGive(zbm_g_device_array_mutex);
    return dev;
}
//********************************************************************************************************************************

//================================================================================================================================
//===================================================== ZBM_DEV_BASE_FIND_EP_IN_DEV_OBJ ==========================================
//================================================================================================================================
static device_custom_t*     s_last_dev_obj  = NULL;
static uint8_t              s_last_ep_id    = 0xFF;
static endpoint_custom_t*   s_last_ep_obj   = NULL;
endpoint_custom_t* zbm_dev_base_find_endpoint_by_id_safe(device_custom_t* dev, uint8_t ep_id)
{
    // 🔹 Кэш: проверяем, не тот ли это же эндпоинт
    if (s_last_ep_obj && s_last_dev_obj == dev && s_last_ep_id == ep_id) {
        return s_last_ep_obj;
    }
    return NULL;
}
//********************************************************************************************************************************

//================================================================================================================================
//===================================================== ZBM_DEV_BASE_UPDATE_DEV_FROM_READ_RESP ===================================
//================================================================================================================================
esp_err_t zbm_dev_base_dev_update_from_read_response_safe(zb_manager_cmd_read_attr_resp_message_t* read_resp)
{
    esp_err_t result = ESP_FAIL;
    device_custom_t* dev = NULL;
    dev = zbm_dev_base_find_device_by_short_safe(read_resp->info.src_address.u.short_addr);
    if (!dev)
    {
        ESP_LOGE(TAG,"zbm_dev_base_dev_update_from_read_response_safe dev == NULL");
        return result;
    }

    // 🔹 Блокировка
    if (xSemaphoreTake(zbm_g_device_array_mutex, pdMS_TO_TICKS(ZBM_BASE_MUTEX_TIMEOUT_SHORT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take device mutex in find_endpoint");
        return result;
    }

    endpoint_custom_t* ep = NULL;
    for (int j = 0; j < dev->endpoints_count; j++) {
        if (dev->endpoints_array[j] != NULL && ((endpoint_custom_t*)dev->endpoints_array[j])->ep_id == read_resp->info.src_endpoint) 
        {
            ep = (endpoint_custom_t*)dev->endpoints_array[j];
            break;
        }
    }
    if (ep)
    {
        result = zbm_dev_base_dev_update_from_read_response(dev, ep, read_resp);
    }
    xSemaphoreGive(zbm_g_device_array_mutex);
    return result;
}
//********************************************************************************************************************************

//================================================================================================================================
//===================================================== ZBM_DEV_BASE_UPDATE_DEV_FROM_REPORT ======================================
//================================================================================================================================
esp_err_t zbm_dev_base_dev_update_from_report_notify_safe(zb_manager_cmd_report_attr_resp_message_t *rep)
{
    esp_err_t result = ESP_FAIL;
    device_custom_t* dev = NULL;
    dev = zbm_dev_base_find_device_by_short_safe(rep->src_address.u.short_addr);
    if (!dev)
    {
        ESP_LOGE(TAG,"zbm_dev_base_dev_update_from_read_response_safe dev == NULL");
        return result;
    }

    // 🔹 Блокировка
    if (xSemaphoreTake(zbm_g_device_array_mutex, pdMS_TO_TICKS(ZBM_BASE_MUTEX_TIMEOUT_SHORT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take device mutex in find_endpoint");
        return result;
    }
    endpoint_custom_t* ep = NULL;
    for (int j = 0; j < dev->endpoints_count; j++) {
        if (dev->endpoints_array[j] != NULL && ((endpoint_custom_t*)dev->endpoints_array[j])->ep_id == rep->src_endpoint) 
        {
            ep = (endpoint_custom_t*)dev->endpoints_array[j];
            break;
        }
    }
    if (ep)
    {
        result = zbm_dev_base_dev_update_from_report(dev, ep, rep);
    }
    xSemaphoreGive(zbm_g_device_array_mutex);
    return result;
}


cJSON *zbm_base_dev_short_list_for_webserver(void)
{
    cJSON *list = NULL;
    list = cJSON_CreateArray();
    if (!list) {
        ESP_LOGE(TAG, "zbm_base_dev_short_list_for_webserver, list == NULL");
        return NULL;
    }
    ESP_LOGI(TAG, "zbm_base_dev_short_list_for_webserver");
    if (xSemaphoreTake(zbm_g_device_array_mutex, pdMS_TO_TICKS(ZBM_BASE_MUTEX_TIMEOUT_LONG_MS)) == pdTRUE) {
        for (int i = 0; i < zbm_RemoteDevicesCount; i++) {
            device_custom_t *dev = zbm_RemoteDevicesArray[i];
            if (!dev) 
            {
                ESP_LOGW(TAG, "zbm_base_dev_short_list_for_webserver, dev == NULL");
                continue;
            }

            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "short", dev->short_addr);
            cJSON_AddStringToObject(item, "name", dev->friendly_name[0] ? dev->friendly_name : "unknown");
            cJSON_AddBoolToObject(item, "online", dev->is_online);

            cJSON_AddItemToArray(list, item);
        }
        xSemaphoreGive(zbm_g_device_array_mutex);
    }
    return list;
}
//********************************************************************************************************************************

//================================================================================================================================
//===================================================== ZBM_DEV_BASE_UPDATE_DEV_FRIENDLY_NAME ====================================
//================================================================================================================================
esp_err_t zbm_dev_base_dev_update_friendly_name(uint16_t short_addr, const char* name)
{
    esp_err_t result = ESP_FAIL;
    if (xSemaphoreTake(zbm_g_device_array_mutex, pdMS_TO_TICKS(ZBM_BASE_MUTEX_TIMEOUT_SHORT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take device mutex in find_by_short");
        return result;
    }
    device_custom_t* dev = NULL;
    for (int i = 0; i < zbm_RemoteDevicesCount; i++) {
        if (zbm_RemoteDevicesArray[i] != NULL && zbm_RemoteDevicesArray[i]->short_addr == short_addr) 
        {
            dev = zbm_RemoteDevicesArray[i];
            break;
        }
    }
    if(dev)
    {
        strncpy(dev->friendly_name, name, sizeof(dev->friendly_name) - 1);
        dev->friendly_name[sizeof(dev->friendly_name) - 1] = '\0';
        dev->friendly_name_len = strlen(name);
        result = ESP_OK;
    }
    xSemaphoreGive(zbm_g_device_array_mutex);
    return result;
}
//********************************************************************************************************************************

//================================================================================================================================
//===================================================== ZBM_DEV_BASE_UPDATE_EP_FRIENDLY_NAME =====================================
//================================================================================================================================
esp_err_t zbm_dev_base_endpoint_update_friendly_name(uint16_t short_addr, uint8_t ep_id, const char* name)
{
    esp_err_t result = ESP_FAIL;

    if (xSemaphoreTake(zbm_g_device_array_mutex, pdMS_TO_TICKS(ZBM_BASE_MUTEX_TIMEOUT_SHORT_MS)) != pdTRUE) {
        ESP_LOGI(TAG, "Failed to take device mutex in find_by_short");
        return result;
    }
    device_custom_t* dev = NULL;
    for (int i = 0; i < zbm_RemoteDevicesCount; i++) {
        if (zbm_RemoteDevicesArray[i] != NULL && zbm_RemoteDevicesArray[i]->short_addr == short_addr) 
        {
            dev = zbm_RemoteDevicesArray[i];
            break;
        }
    }
    if(dev)
    {
        endpoint_custom_t* ep = NULL;
        for (int j = 0; j < dev->endpoints_count; j++) {
            if (dev->endpoints_array[j] != NULL && dev->endpoints_array[j]->ep_id == ep_id) 
            {
                ep = dev->endpoints_array[j];
                break;
            }
        } 
        if (ep)
        {
            strncpy(ep->friendly_name, name, sizeof(ep->friendly_name) - 1);
            ep->friendly_name[sizeof(ep->friendly_name) - 1] = '\0';
            result = ESP_OK;
        }
    }
    xSemaphoreGive(zbm_g_device_array_mutex);
    ESP_LOGI(TAG,"zbm_dev_base_endpoint_update_friendly_name result %d",result);
    return result;
}
//********************************************************************************************************************************

//================================================================================================================================
//===================================================== ZBM_DEV_BASE_DEV_FIND_BY_LONG ============================================
//================================================================================================================================
device_custom_t* zbm_dev_base_find_device_by_long_safe(esp_zb_ieee_addr_t *ieee)
{
    device_custom_t* dev = NULL;
    if (xSemaphoreTake(zbm_g_device_array_mutex, pdMS_TO_TICKS(ZBM_BASE_MUTEX_TIMEOUT_SHORT_MS)) != pdTRUE) {
        ESP_LOGI(TAG, "Failed to take device mutex in find_by_short");
        return NULL;
    }
    
    for (int i = 0; i < zbm_RemoteDevicesCount; i++) {
        if (zbm_RemoteDevicesArray[i] != NULL && ieee_addr_compare(&zbm_RemoteDevicesArray[i]->ieee_addr, ieee) == 0) 
        {
            dev = zbm_RemoteDevicesArray[i];
            break;
        }
    }
    xSemaphoreGive(zbm_g_device_array_mutex);
    return dev;
}
//********************************************************************************************************************************

//================================================================================================================================
//===================================================== ZBM_DEV_BASE_DEV_OBJ_APPEND ==============================================
//================================================================================================================================
esp_err_t zbm_dev_base_dev_obj_append_safe(device_custom_t* dev_object)
{
    esp_err_t result = ESP_FAIL;
    if (xSemaphoreTake(zbm_g_device_array_mutex, pdMS_TO_TICKS(ZBM_BASE_MUTEX_TIMEOUT_LONG_MS)) != pdTRUE) {
        ESP_LOGI(TAG, "Failed to take device mutex in find_by_short");
        return result;
    }
    uint8_t free_index = REMOTE_DEVICES_COUNT + 1; // несуществующий индекс
    for(int i = 0; i < REMOTE_DEVICES_COUNT; i++)
    {  
        if (zbm_RemoteDevicesArray[i] == NULL)
        {
            free_index = i;
            break;
        }
    }
    if (free_index < REMOTE_DEVICES_COUNT + 1)
    {
        dev_object->index_in_array = free_index;
        zbm_RemoteDevicesArray[free_index] = dev_object;
        result = ESP_OK;                     
    }
    xSemaphoreGive(zbm_g_device_array_mutex);
    return result;
}