#include "zb_manager_devices.h"
#include "string.h"
#include "zb_manager_devices_manufactory_table.h"
#include "web_server.h"
#include "zb_manager_action_handler_worker.h"
#include "esp_log.h"
#include "ha_integration.h"


static const char *TAG = "zb_manager_devices_module";

SemaphoreHandle_t g_device_array_mutex = NULL; // мьютекс для защиты массива устройств


QueueHandle_t zb_manager_save_JSon_cmd_queue = NULL;
/**
 * @brief Функция сравнения двух IEEE-адресов для использования с lfind/bsearch
 * 
 * @param a Указатель на первый IEEE-адрес (const void*)
 * @param b Указатель на второй IEEE-адрес (const void*)
 * @return int 0 если адреса равны, не ноль если различаются
 */
int ieee_addr_compare(esp_zb_ieee_addr_t *a, esp_zb_ieee_addr_t *b)
{
    if (!a || !b) return -1;
    return memcmp((const uint8_t *)a, (const uint8_t *)b, sizeof(esp_zb_ieee_addr_t));
}

uint8_t RemoteDevicesCount = 0;
device_custom_t** RemoteDevicesArray = NULL;

uint8_t DeviceAppendShedulerCount = 0;
device_appending_sheduler_t** DeviceAppendShedulerArray = NULL;

static esp_timer_handle_t online_status_timer; // таймер для обновления online статуса

static uint32_t last_status_print_ms = 0;

// Файл: components/zb_manager/devices/zb_manager_devices.c

// Файл: components/zb_manager/devices/zb_manager_devices.c

static void check_all_devices_status(void *arg)
{
    
    const uint32_t POLLING_INTERVAL_MS = 20 * 1000;    // Опрос OnOff каждые 20 сек
    const uint32_t BIND_RETRY_DELAY_MS = 30 * 1000;    // Повтор Bind не чаще, чем раз в 30 сек

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(POLLING_INTERVAL_MS));

        uint32_t now = esp_log_timestamp();
        DEVICE_ARRAY_LOCK20();

        for (int i = 0; i < RemoteDevicesCount; i++) {
            device_custom_t *dev = RemoteDevicesArray[i];
            if (!dev) continue;
            // === 1. Обновление ONLINE статуса ===
            if(dev->is_in_build_status != 2) continue;
                bool was_online = dev->is_online;
                bool now_online = false;
                if (zb_manager_is_device_always_powered(dev)) {
                    if (dev->has_pending_read) {
                        if (dev->has_pending_response) {
                            now_online = true;
                        } else if (now - dev->last_pending_read_ms > 10000) {
                            // Таймаут: устройство НЕ ответило на ReadAttr
                            now_online = false;
                            dev->has_pending_read = false;

                            // 🔁 Попробовать Bind, если прошло достаточно времени
                            /*if (now - dev->last_bind_attempt_ms > BIND_RETRY_DELAY_MS) {
                                // Ищем первый endpoint с OnOff кластером
                                bool sent_bind = false;
                                for (int ep_idx = 0; ep_idx < dev->endpoints_count; ep_idx++) {
                                    endpoint_custom_t *ep = dev->endpoints_array[ep_idx];
                                    if (ep && ep->is_use_on_off_cluster) {
                                        // биндинг только на координатор
                                        delayed_bind_req_t *delayed_bind = calloc(1, sizeof(delayed_bind_req_t));
                                        if (delayed_bind) {
                                            delayed_bind->short_addr = dev->short_addr;
                                            delayed_bind->src_endpoint = ep->ep_id;
                                            delayed_bind->cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF; // 0x0006

                                            ESP_LOGI(TAG, "📤 Queuing BIND request via action worker: short=0x%04x, ep=%d, cluster=0x%04x",
                                                    delayed_bind->short_addr, delayed_bind->src_endpoint, delayed_bind->cluster_id);

                                            if (zb_manager_post_to_action_worker(ZB_ACTION_DELAYED_BIND_REQ, delayed_bind, sizeof(delayed_bind_req_t))) {
                                                ESP_LOGI(TAG, "✅ BIND request successfully posted to action worker");
                                                dev->last_bind_attempt_ms = now; // Обновляем время попытки
                                                sent_bind = true;
                                            } else {
                                                ESP_LOGE(TAG, "❌ Failed to post BIND request to action worker");
                                                free(delayed_bind); // Освобождаем — данные скопированы или ошибка
                                            }
                                            free(delayed_bind); // Освобождаем — данные скопированы или ошибка
                                        } else {
                                            ESP_LOGE(TAG, "❌ Failed to allocate memory for delayed_bind");
                                        }
                                        break; // Отправляем Bind только для первого OnOff EP
                                    }
                                }

                                if (!sent_bind) {
                                    ESP_LOGW(TAG, "⚠️ No OnOff endpoint found for BIND on device %s (0x%04x)", dev->friendly_name, dev->short_addr);
                                }
                            } else {
                                ESP_LOGD(TAG, "⏳ BIND already attempted recently for %s (0x%04x)", dev->friendly_name, dev->short_addr);
                            }*/
                        } else {
                            now_online = false; // Ещё ждём ответа
                        }
                    } else {
                        now_online = true; // Готов к следующему опросу
                    }
                } else {
                    // Батарейное устройство — по таймеру
                    now_online = (now - dev->last_seen_ms) < dev->device_timeout_ms;
                }
        

                dev->is_online = now_online;
                if (was_online != now_online) {
                    ESP_LOGI(TAG, "Device %s (0x%04x) → %s",
                            dev->friendly_name,
                            dev->short_addr,
                            now_online ? "ONLINE" : "OFFLINE");
                    ws_notify_device_update_unlocked(dev);
                    
                }

                // === 2. Периодический опрос OnOff (если нет pending read) ===
                if (zb_manager_is_device_always_powered(dev) && !dev->has_pending_read) {
                    bool sent_poll = false;
                    for (int ep_idx = 0; ep_idx < dev->endpoints_count; ep_idx++) {
                        endpoint_custom_t *ep = dev->endpoints_array[ep_idx];
                        if (!ep || !ep->is_use_on_off_cluster) continue;

                        uint32_t last_read_ms = ep->server_OnOffClusterObj ? ep->server_OnOffClusterObj->last_update_ms : 0;
                        if (now - last_read_ms > POLLING_INTERVAL_MS) {
                            uint8_t tsn = zb_manager_read_on_off_attribute(dev->short_addr, ep->ep_id);
                            
                            if (tsn != 0xff) {
                                dev->has_pending_read = true;
                                dev->has_pending_response = false;
                                dev->last_pending_read_ms = now;
                                ESP_LOGI(TAG, "🔁 Polling OnOff: %s (0x%04x, EP: %d)", dev->friendly_name, dev->short_addr, ep->ep_id);
                                sent_poll = true;
                            } else {
                                ESP_LOGW(TAG, "❌ Failed to poll OnOff: %s (0x%04x, EP: %d)", dev->friendly_name, dev->short_addr, ep->ep_id);
                            }
                            break;
                        }
                    }
                    if (!sent_poll) {
                        ESP_LOGD(TAG, "No OnOff clusters to poll for %s (0x%04x)", dev->friendly_name, dev->short_addr);
                    }
                }

                // === 3. Лог раз в минуту ===
                if (now - dev->last_status_print_log_time >= 60000) {
                    ESP_LOGI(TAG, "Device %s (0x%04x) is NOW %s",
                            dev->friendly_name,
                            dev->short_addr,
                            dev->is_online ? "ONLINE" : "OFFLINE");
                    dev->last_status_print_log_time = now;
                }
            
        }
        DEVICE_ARRAY_UNLOCK();
    }
}




static void check_all_devices_status_original(void *arg)
{
    uint32_t now = esp_log_timestamp();
    DEVICE_ARRAY_LOCK20();
    for (int i = 0; i < RemoteDevicesCount; i++) {
        device_custom_t *dev = RemoteDevicesArray[i];
        if (dev) {
            bool was_online = dev->is_online;
            bool now_online = zb_manager_update_device_online_status(dev);

            // Логируем изменение
            if (was_online != now_online) {
                ESP_LOGI(TAG, "Device %s (0x%04x) is change online status %s",
                         dev->friendly_name,
                         dev->short_addr,
                         now_online ? "ONLINE" : "OFFLINE");
                // Можно отправить событие: eventLoopPost(DEVICE_ONLINE_STATUS_CHANGED, dev, ...)
            }
            if (now - last_status_print_ms >= 60000) {
                ESP_LOGI(TAG, "Device %s (0x%04x) is NOW %s",
                         dev->friendly_name,
                         dev->short_addr,
                         now_online ? "ONLINE" : "OFFLINE");
            }
        }
    }
    last_status_print_ms = now;
    DEVICE_ARRAY_UNLOCK();
    //json_load_and_print(ZB_MANAGER_JSON_DEVICES_FILE);
    //zb_manager_print_RemoteDevicesArray();

    // Каждые 5 минут — печатаем общий статус 300000
    // каждую  минуту — печатаем статус каждого устройства 60000
}

static esp_err_t zb_manager_save_dev_to_json(const char *filepath);
static void zb_save_task(void *pvParameters)
{
    uint8_t cmd;
    const char *file_path = (const char *)pvParameters;

    ESP_LOGI(TAG, "Zigbee Save Task started");

    while (1) {
        // Ожидаем команду
        if (xQueueReceive(zb_manager_save_JSon_cmd_queue, &cmd, portMAX_DELAY) == pdPASS) {
            if (cmd == ZB_SAVE_CMD_SAVE) {
                ESP_LOGD(TAG, "Executing deferred save to JSON");
                esp_err_t err = zb_manager_save_dev_to_json(file_path);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Devices successfully saved to %s", file_path);
                } else {
                    ESP_LOGE(TAG, "Failed to save devices to %s", file_path);
                }
            }
        }
    }
}

esp_err_t zb_manager_devices_init(void)
{
    RemoteDevicesCount = REMOTE_DEVICES_COUNT;
    RemoteDevicesArray = calloc(RemoteDevicesCount, sizeof(device_custom_t*));
    if (RemoteDevicesArray == NULL) return ESP_FAIL;
    
    for (int i = 0; i < RemoteDevicesCount; i++) RemoteDevicesArray[i] = NULL;

    DeviceAppendShedulerCount = DEVICE_APPEND_SHEDULER_COUNT;
    DeviceAppendShedulerArray = calloc(DeviceAppendShedulerCount, sizeof(device_appending_sheduler_t*));
    if(DeviceAppendShedulerArray == NULL) return ESP_FAIL;
    for (int i = 0; i < DeviceAppendShedulerCount; i++) DeviceAppendShedulerArray[i] = NULL;
    
    // Создаём мьютекс для доступа к массиву устройств
    g_device_array_mutex = xSemaphoreCreateMutex();
    if (g_device_array_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create device array mutex");
        return ESP_FAIL;
    }
    // создаём очередь для команд сохранения
    if (zb_manager_save_JSon_cmd_queue == NULL) {
        zb_manager_save_JSon_cmd_queue = xQueueCreate(ZB_SAVE_CMD_QUEUE_SIZE, sizeof(uint8_t));
        if (zb_manager_save_JSon_cmd_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create save command queue");
            return ESP_ERR_NO_MEM;
        }else{
            ESP_LOGW(TAG, "Save command queue created");
        }
        // Создаём задачу сохранения
        xTaskCreate(zb_save_task, "zb_save_task", 8192, (void*)ZB_MANAGER_JSON_DEVICES_FILE, 5, NULL);
    }
    

    if (zb_manager_load_devices_from_json(ZB_MANAGER_JSON_DEVICES_FILE) != ESP_OK) 
    {
        ESP_LOGW(TAG, "zb_manager_load_devices_from_json не удалось загрузить данные из файла, файл /spiffs/zb_manager_devices.json отсутствует, будем создавать");
        esp_err_t create_file_err = ESP_FAIL;
        create_file_err = zb_manager_save_dev_to_json(ZB_MANAGER_JSON_DEVICES_FILE);
        if (create_file_err == ESP_OK)
        {
            ESP_LOGW(TAG, "/spiffs/zb_manager_devices.json файл создан");
            return ESP_OK;
        }else 
        {
            ESP_LOGW(TAG, "/spiffs/zb_manager_devices.json файл не создан");
            return ESP_FAIL;
        }
    }

    // ✅ Применяем фиксы ко всем устройствам после загрузки 
    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_SHORT_MS)) == pdTRUE) {
    for (int i = 0; i < RemoteDevicesCount; i++) {
        device_custom_t *dev = RemoteDevicesArray[i];
        if (dev) {
            zb_manager_apply_device_fixups(dev);
        }
    }
    DEVICE_ARRAY_UNLOCK();
    } else {
                ESP_LOGE(TAG, "Failed to take mutex for zb_manager_apply_device_fixups in zb_manager_devices_init");
                return ESP_FAIL;
    }

    // запускаем таймер для проверки статуса устройств
    esp_timer_create_args_t status_timer_args = {
    .callback = check_all_devices_status,
    .name = "online_status_checker"
    };
    ESP_ERROR_CHECK(esp_timer_create(&status_timer_args, &online_status_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(online_status_timer, 20000000)); // каждые 60 сек
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

esp_err_t RemoteDeviceEndpointDelete(endpoint_custom_t* ep_object)
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
    free(ep_object);
    ep_object = NULL;
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
    new_dev->lqi = 0;                    // ← Инициализация
    new_dev->last_seen_ms = 0;           // ← Можно обновлять при получении пакетов
    new_dev->device_timeout_ms = ZB_DEVICE_DEFAULT_TIMEOUT_MS; // ← По умолчанию
    new_dev->is_online = false;
    new_dev->last_bind_attempt_ms = 0;
    new_dev->last_pending_read_ms = 0;
    new_dev->last_status_print_log_time = 0;
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

/****** Save Load Functions **************/
void ieee_to_str(char* out, const esp_zb_ieee_addr_t addr) {
    sprintf(out, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
            addr[7], addr[6], addr[5], addr[4],
            addr[3], addr[2], addr[1], addr[0]);
}

bool str_to_ieee(const char* str, esp_zb_ieee_addr_t addr) {
    uint8_t tmp[8];
    if (sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &tmp[7], &tmp[6], &tmp[5], &tmp[4],
               &tmp[3], &tmp[2], &tmp[1], &tmp[0]) != 8) {
        return false;
    }
    memcpy(addr, tmp, 8);
    return true;
}

void zb_manager_update_device_lqi(uint16_t short_addr, uint8_t lqi)
{
      for (int i = 0; i < RemoteDevicesCount; i++) {
        device_custom_t *dev = RemoteDevicesArray[i];
        if (dev && dev->short_addr == short_addr) {
            dev->lqi = lqi;
            dev->last_seen_ms = esp_log_timestamp();
            ESP_LOGD(TAG, "LQI updated for 0x%04x: %d", short_addr, lqi);
            break;
        }
    }
}

uint32_t get_timeout_for_device_id(uint16_t device_id)
{
     switch (device_id) {
        // Датчики — редко отвечают
        case ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID:
        case ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID:
        case ESP_ZB_HA_IAS_ZONE_ID:
        case ESP_ZB_HA_LIGHT_SENSOR_DEVICE_ID:
            return ZB_DEVICE_SENSOR_TIMEOUT_MS; 
        //case ESP_ZB_HA_OCCUPANCY_SENSOR_DEVICE_ID:
            return ZB_DEVICE_SENSOR_TIMEOUT_MS;          // 5 мин

        // Выключатели, кнопки — могут быть "спящими", но лучше проверять чаще
        case ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID:
        case ESP_ZB_HA_DIMMER_SWITCH_DEVICE_ID:
        case ESP_ZB_HA_COLOR_DIMMER_SWITCH_DEVICE_ID:
            return ZB_DEVICE_SWITCH_TIMEOUT_MS;          // 2 мин

        // Устройства с питанием — должны быть всегда онлайн
        case ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID:
        case ESP_ZB_HA_DIMMABLE_LIGHT_DEVICE_ID:
        case ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID:
        case ESP_ZB_HA_MAINS_POWER_OUTLET_DEVICE_ID:
        case ESP_ZB_HA_SMART_PLUG_DEVICE_ID:
        case ESP_ZB_HA_RANGE_EXTENDER_DEVICE_ID:
            return ZB_DEVICE_DEFAULT_TIMEOUT_MS;         // 60 сек
            //return ZB_DEVICE_TUYA_COIL_TIMEOUT_MS;         // 3 минуты

        // Роутеры, шлюзы
        case ESP_ZB_HA_HOME_GATEWAY_DEVICE_ID:
            return ZB_DEVICE_ROUTER_TIMEOUT_MS;          // 60 сек
        //case ESP_ZB_HA_RANGE_EXTENDER_DEVICE_ID:
            //return ZB_DEVICE_ROUTER_TIMEOUT_MS;          // 60 сек

        // По умолчанию
        default:
            return ZB_DEVICE_DEFAULT_TIMEOUT_MS;
    }
}
void zb_manager_configure_device_timeout(device_custom_t* dev)
{
    if (!dev) return;

    uint32_t timeout = ZB_DEVICE_DEFAULT_TIMEOUT_MS;

    // Если Aqara b2lc04 → 60 сек
    if (dev->manufacturer_code == 0x115F) {
        const char *model = dev->server_BasicClusterObj ? dev->server_BasicClusterObj->model_identifier : NULL;
        if (model && strcmp(model, "lumi.switch.b2lc04") == 0) {
            dev->device_timeout_ms = 60000;
            return;
        }
    }

    // По умолчанию: выбираем максимальный таймаут
    for (int i = 0; i < dev->endpoints_count; i++) {
        endpoint_custom_t* ep = dev->endpoints_array[i];
        if (!ep) continue;
        if (ep->is_use_on_device == true)
        {
            uint32_t ep_timeout = get_timeout_for_device_id(ep->deviceId);
            if (ep_timeout > timeout) {
                timeout = ep_timeout;
            }
        }
    }
    dev->device_timeout_ms = timeout;
}

bool zb_manager_update_device_online_status(device_custom_t* dev)
{
     if (!dev || dev->last_seen_ms == 0 || dev->device_timeout_ms == 0) {
        dev->is_online = false;
        return false;
    }

    uint32_t now = esp_log_timestamp();
    uint32_t elapsed = now - dev->last_seen_ms;

    // Если произошёл переполнение (now < last_seen), elapsed будет огромным
    // → устройство точно offline
    if (now < dev->last_seen_ms) {
        dev->is_online = false;
        return false;
    }

    bool online = (elapsed < dev->device_timeout_ms);
    dev->is_online = online;
    // отправляем в веб сервер
    return online;
}

// components/zb_manager/devices/zb_manager_devices.c
void zb_manager_update_device_activity(uint16_t short_addr, uint8_t lqi)
{
    DEVICE_ARRAY_LOCK20();
    device_custom_t *dev = zb_manager_find_device_by_short(short_addr);
    if (!dev) {
        ESP_LOGD(TAG, "Device 0x%04x not found", short_addr);
        DEVICE_ARRAY_UNLOCK();
        return;
    }

    dev->lqi = lqi;
    dev->last_seen_ms = esp_log_timestamp();

    // Для БАТАРЕЙНЫХ устройств — активность = online
    if (!zb_manager_is_device_always_powered(dev)) {
        dev->is_online = true;
        ESP_LOGD(TAG, "✅ Battery-powered: 0x%04x → ONLINE via activity", short_addr);
    }
    // Для питаемых — НЕ меняем is_online здесь!
    // Пусть check_all_devices_status решает по ответу на ReadAttr
    else {
        ESP_LOGD(TAG, "💡 Mains-powered: 0x%04x → activity updated, but is_online unchanged", short_addr);
    }

    // Всё равно уведомляем веб-сервер (если нужно)
    // ws_notify_device_update(dev->short_addr); // ← не всегда нужно здесь

    ESP_LOGD(TAG, "Device 0x%04x: LQI=%d, Online=%s",
             short_addr,
             dev->lqi,
             dev->is_online ? "YES" : "NO");

    DEVICE_ARRAY_UNLOCK();
}


static uint16_t s_last_short = 0xFFFF;
static device_custom_t* s_last_dev = NULL;

device_custom_t* zb_manager_find_device_by_short_safe(uint16_t short_addr)
{
    // Проверяем кэш
    if (s_last_dev && s_last_short == short_addr) {
        return s_last_dev;
    }

    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_SHORT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take device mutex in find_by_short");
        return NULL;
    }

    device_custom_t* dev = NULL;
    for (int i = 0; i < RemoteDevicesCount; i++) {
        if (RemoteDevicesArray[i] != NULL &&
            RemoteDevicesArray[i]->short_addr == short_addr) {
            dev = RemoteDevicesArray[i];
            break;
        }
    }

    // Обновляем кэш
    s_last_short = short_addr;
    s_last_dev = dev;

    DEVICE_ARRAY_UNLOCK();
    return dev;
}

device_custom_t* zb_manager_find_device_by_short(uint16_t short_addr)
{
    // Проверяем кэш
    if (s_last_dev && s_last_short == short_addr) {
        return s_last_dev;
    }

    /*if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_SHORT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take device mutex in find_by_short");
        return NULL;
    }*/

    device_custom_t* dev = NULL;
    for (int i = 0; i < RemoteDevicesCount; i++) {
        if (RemoteDevicesArray[i] != NULL &&
            RemoteDevicesArray[i]->short_addr == short_addr) {
            dev = RemoteDevicesArray[i];
            break;
        }
    }

    // Обновляем кэш
    s_last_short = short_addr;
    s_last_dev = dev;

    //DEVICE_ARRAY_UNLOCK();
    return dev;
}

endpoint_custom_t* zb_manager_find_endpoint_safe(uint16_t short_addr, uint8_t endpoint_id)
{
    static uint16_t s_last_short = 0xFFFF;
    static uint8_t  s_last_ep    = 0xFF;
    static endpoint_custom_t* s_last_ep_ptr = NULL;

    // 🔹 Кэш: проверяем, не тот ли это же эндпоинт
    if (s_last_ep_ptr && s_last_short == short_addr && s_last_ep == endpoint_id) {
        return s_last_ep_ptr;
    }

    // 🔹 Блокировка
    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_SHORT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take device mutex in find_endpoint");
        return NULL;
    }

    endpoint_custom_t* ep = NULL;
    for (int i = 0; i < RemoteDevicesCount; i++) {
        if (RemoteDevicesArray[i] != NULL &&
            RemoteDevicesArray[i]->short_addr == short_addr) {

            // Нашли устройство — ищем эндпоинт
            for (int j = 0; j < RemoteDevicesArray[i]->endpoints_count; j++) {
                if (RemoteDevicesArray[i]->endpoints_array[j] != NULL &&
                    ((endpoint_custom_t*)RemoteDevicesArray[i]->endpoints_array[j])->ep_id == endpoint_id) {
                    ep = (endpoint_custom_t*)RemoteDevicesArray[i]->endpoints_array[j];
                    break;
                }
            }
            break;
        }
    }

    // 🔹 Обновляем кэш
    s_last_short = short_addr;
    s_last_ep    = endpoint_id;
    s_last_ep_ptr = ep;

    DEVICE_ARRAY_UNLOCK();
    return ep;
}

endpoint_custom_t* zb_manager_find_endpoint(uint16_t short_addr, uint8_t endpoint_id)
{
    static uint16_t s_last_short = 0xFFFF;
    static uint8_t  s_last_ep    = 0xFF;
    static endpoint_custom_t* s_last_ep_ptr = NULL;

    // 🔹 Кэш: проверяем, не тот ли это же эндпоинт
    if (s_last_ep_ptr && s_last_short == short_addr && s_last_ep == endpoint_id) {
        return s_last_ep_ptr;
    }

    

    endpoint_custom_t* ep = NULL;
    for (int i = 0; i < RemoteDevicesCount; i++) {
        if (RemoteDevicesArray[i] != NULL &&
            RemoteDevicesArray[i]->short_addr == short_addr) {

            // Нашли устройство — ищем эндпоинт
            for (int j = 0; j < RemoteDevicesArray[i]->endpoints_count; j++) {
                if (RemoteDevicesArray[i]->endpoints_array[j] != NULL &&
                    ((endpoint_custom_t*)RemoteDevicesArray[i]->endpoints_array[j])->ep_id == endpoint_id) {
                    ep = (endpoint_custom_t*)RemoteDevicesArray[i]->endpoints_array[j];
                    break;
                }
            }
            break;
        }
    }

    // 🔹 Обновляем кэш
    s_last_short = short_addr;
    s_last_ep    = endpoint_id;
    s_last_ep_ptr = ep;

    
    return ep;
}

const char* zb_manager_get_ha_device_type_name(uint16_t device_id)
{
    switch (device_id) {
        case ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID:                 return "on_off_switch";
        case ESP_ZB_HA_LEVEL_CONTROL_SWITCH_DEVICE_ID:          return "level_control_switch";
        case ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID:                 return "on_off_output";
        case ESP_ZB_HA_LEVEL_CONTROLLABLE_OUTPUT_DEVICE_ID:     return "level_controllable_output";
        case ESP_ZB_HA_SCENE_SELECTOR_DEVICE_ID:                return "scene_selector";
        case ESP_ZB_HA_CONFIGURATION_TOOL_DEVICE_ID:            return "configuration_tool";
        case ESP_ZB_HA_REMOTE_CONTROL_DEVICE_ID:                return "remote_control";
        case ESP_ZB_HA_COMBINED_INTERFACE_DEVICE_ID:            return "combined_interface";
        case ESP_ZB_HA_RANGE_EXTENDER_DEVICE_ID:                return "range_extender";
        case ESP_ZB_HA_MAINS_POWER_OUTLET_DEVICE_ID:            return "mains_power_outlet";
        case ESP_ZB_HA_DOOR_LOCK_DEVICE_ID:                     return "door_lock";
        case ESP_ZB_HA_DOOR_LOCK_CONTROLLER_DEVICE_ID:          return "door_lock_controller";
        case ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID:                 return "simple_sensor";
        case ESP_ZB_HA_CONSUMPTION_AWARENESS_DEVICE_ID:         return "consumption_awareness";
        case ESP_ZB_HA_HOME_GATEWAY_DEVICE_ID:                  return "home_gateway";
        case ESP_ZB_HA_SMART_PLUG_DEVICE_ID:                    return "smart_plug";
        case ESP_ZB_HA_WHITE_GOODS_DEVICE_ID:                   return "white_goods";
        case ESP_ZB_HA_METER_INTERFACE_DEVICE_ID:               return "meter_interface";
        case ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID:                  return "on_off_light";
        case ESP_ZB_HA_DIMMABLE_LIGHT_DEVICE_ID:                return "dimmable_light";
        case ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID:          return "color_dimmable_light";
        case ESP_ZB_HA_DIMMER_SWITCH_DEVICE_ID:                 return "dimmer_switch";
        case ESP_ZB_HA_COLOR_DIMMER_SWITCH_DEVICE_ID:           return "color_dimmer_switch";
        case ESP_ZB_HA_LIGHT_SENSOR_DEVICE_ID:                  return "light_sensor";
        case ESP_ZB_HA_SHADE_DEVICE_ID:                         return "shade";
        case ESP_ZB_HA_SHADE_CONTROLLER_DEVICE_ID:              return "shade_controller";
        case ESP_ZB_HA_WINDOW_COVERING_DEVICE_ID:               return "window_covering";
        case ESP_ZB_HA_WINDOW_COVERING_CONTROLLER_DEVICE_ID:    return "window_covering_controller";
        case ESP_ZB_HA_HEATING_COOLING_UNIT_DEVICE_ID:          return "heating_cooling_unit";
        case ESP_ZB_HA_THERMOSTAT_DEVICE_ID:                    return "thermostat";
        case ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID:            return "temperature_sensor";
        case ESP_ZB_HA_IAS_CONTROL_INDICATING_EQUIPMENT_ID:     return "ias_control_indicator";
        case ESP_ZB_HA_IAS_ANCILLARY_CONTROL_EQUIPMENT_ID:      return "ias_ancillary_control";
        case ESP_ZB_HA_IAS_ZONE_ID:                             return "ias_zone";
        case ESP_ZB_HA_IAS_WARNING_DEVICE_ID:                   return "ias_warning";
        case ESP_ZB_HA_TEST_DEVICE_ID:                          return "test_device";
        case ESP_ZB_HA_CUSTOM_TUNNEL_DEVICE_ID:                return "custom_tunnel";
        case ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID:                  return "custom_attr";
        default: return "unknown_device_type";
    }
}

static esp_err_t zb_manager_save_dev_to_json(const char *filepath) {
    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take device mutex in zb_manager_save_devices_to_json");
        return ESP_ERR_TIMEOUT;
    }

    cJSON *root = cJSON_CreateArray();
    if (!root) 
    {
       DEVICE_ARRAY_UNLOCK();
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < RemoteDevicesCount; i++) {
        device_custom_t *dev = RemoteDevicesArray[i];
        if (!dev) continue;

        cJSON *dev_obj = cJSON_CreateObject();
        if (!dev_obj) continue;

        // === device_custom_t поля ===
        char ieee_str[24];
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
        // После last_seen_ms и device_timeout_ms
        cJSON_AddBoolToObject(dev_obj, "is_online", dev->is_online);
        if (dev->manufacturer_code != 0) {
            cJSON_AddNumberToObject(dev_obj, "manufacturer_code", dev->manufacturer_code);
            cJSON_AddStringToObject(dev_obj, "manufacturer_name_str",
            zb_manager_get_manufacturer_name_by_code(dev->manufacturer_code));
        }

        // BasicCluster на уровне устройства
        if (dev->server_BasicClusterObj) {
            cJSON *basic = cJSON_CreateObject();
            cJSON_AddNumberToObject(basic, "cluster_id", 0x0000); // ESP_ZB_ZCL_CLUSTER_ID_BASIC
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
            cJSON_AddItemToObject(dev_obj, "device_basic_cluster", basic);
        }

        // === Уровень устройства: Power Configuration Cluster (если есть) ===
        if (dev->server_PowerConfigurationClusterObj) {
            cJSON *power_config = cJSON_CreateObject();
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

            cJSON_AddItemToObject(dev_obj, "device_power_config_cluster", power_config);
        }
        // === Endpoints ===
        cJSON *eps = cJSON_CreateArray();
        for (int j = 0; j < dev->endpoints_count; j++) {
            endpoint_custom_t *ep = dev->endpoints_array[j];
            if (!ep) continue;

            cJSON *ep_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(ep_obj, "ep_id", ep->ep_id);
            cJSON_AddNumberToObject(ep_obj, "is_use_on_device", ep->is_use_on_device);
            cJSON_AddStringToObject(ep_obj, "friendly_name", ep->friendly_name);
            cJSON_AddNumberToObject(ep_obj, "deviceId", ep->deviceId);
            const char* dev_type_name = zb_manager_get_ha_device_type_name(ep->deviceId);
            cJSON_AddStringToObject(ep_obj, "device_type", dev_type_name);

            // Basic Cluster (на уровне endpoint)
            /*if (ep->is_use_basic_cluster && ep->server_BasicClusterObj) {
                cJSON *basic = cJSON_CreateObject();
                cJSON_AddNumberToObject(basic, "cluster_id", 0x0000); // ESP_ZB_ZCL_CLUSTER_ID_BASIC
                cJSON_AddNumberToObject(basic, "zcl_version", ep->server_BasicClusterObj->zcl_version);
                cJSON_AddNumberToObject(basic, "app_version", ep->server_BasicClusterObj->application_version);
                cJSON_AddNumberToObject(basic, "stack_version", ep->server_BasicClusterObj->stack_version);
                cJSON_AddNumberToObject(basic, "hw_version", ep->server_BasicClusterObj->hw_version);
                cJSON_AddStringToObject(basic, "manufacturer_name", ep->server_BasicClusterObj->manufacturer_name);
                cJSON_AddStringToObject(basic, "model_id", ep->server_BasicClusterObj->model_identifier);
                cJSON_AddStringToObject(basic, "date_code", ep->server_BasicClusterObj->date_code);
                cJSON_AddNumberToObject(basic, "power_source", ep->server_BasicClusterObj->power_source);
                const char* power_text = get_power_source_string(ep->server_BasicClusterObj->power_source);
                cJSON_AddStringToObject(basic, "power_source_text", power_text);
                cJSON_AddStringToObject(basic, "location", ep->server_BasicClusterObj->location_description);
                cJSON_AddNumberToObject(basic, "env", ep->server_BasicClusterObj->physical_environment);
                cJSON_AddBoolToObject(basic, "enabled", ep->server_BasicClusterObj->device_enabled);
                 cJSON_AddNumberToObject(basic, "last_update_ms", ep->server_BasicClusterObj->last_update_ms);
                cJSON_AddItemToObject(ep_obj, "basic", basic);
            }*/


            // Identify Cluster
            if (ep->is_use_identify_cluster && ep->server_IdentifyClusterObj) {
                cJSON *identify = cJSON_CreateObject();
                cJSON_AddNumberToObject(identify, "cluster_id", 0x0003); // ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY
                cJSON_AddNumberToObject(identify, "identify_time", ep->server_IdentifyClusterObj->identify_time);
                cJSON_AddItemToObject(ep_obj, "identify", identify);
            }

            // Temperature Measurement Cluster
            if (ep->is_use_temperature_measurement_cluster && ep->server_TemperatureMeasurementClusterObj) {
                cJSON *temp = cJSON_CreateObject();
                cJSON_AddNumberToObject(temp, "cluster_id", 0x0402);
                cJSON_AddNumberToObject(temp, "measured_value", ep->server_TemperatureMeasurementClusterObj->measured_value);
                cJSON_AddNumberToObject(temp, "min_measured_value", ep->server_TemperatureMeasurementClusterObj->min_measured_value);
                cJSON_AddNumberToObject(temp, "max_measured_value", ep->server_TemperatureMeasurementClusterObj->max_measured_value);
                cJSON_AddNumberToObject(temp, "tolerance", ep->server_TemperatureMeasurementClusterObj->tolerance);
                cJSON_AddNumberToObject(temp, "last_update_ms", ep->server_TemperatureMeasurementClusterObj->last_update_ms);
                cJSON_AddBoolToObject(temp, "read_error", ep->server_TemperatureMeasurementClusterObj->read_error);
                cJSON_AddItemToObject(ep_obj, "temperature", temp);
            }

            // Humidity Measurement Cluster
            if (ep->is_use_humidity_measurement_cluster && ep->server_HumidityMeasurementClusterObj) {
                cJSON *hum = cJSON_CreateObject();
                cJSON_AddNumberToObject(hum, "cluster_id", 0x0405);
                cJSON_AddNumberToObject(hum, "measured_value", ep->server_HumidityMeasurementClusterObj->measured_value);
                cJSON_AddNumberToObject(hum, "min_measured_value", ep->server_HumidityMeasurementClusterObj->min_measured_value);
                cJSON_AddNumberToObject(hum, "max_measured_value", ep->server_HumidityMeasurementClusterObj->max_measured_value);
                cJSON_AddNumberToObject(hum, "tolerance", ep->server_HumidityMeasurementClusterObj->tolerance);
                cJSON_AddNumberToObject(hum, "last_update_ms", ep->server_HumidityMeasurementClusterObj->last_update_ms);
                cJSON_AddBoolToObject(hum, "read_error", ep->server_HumidityMeasurementClusterObj->read_error);
                cJSON_AddItemToObject(ep_obj, "humidity", hum);
            }

            // OnOff Cluster
            if (ep->is_use_on_off_cluster && ep->server_OnOffClusterObj) {
                cJSON *onoff = cJSON_CreateObject();
                cJSON_AddNumberToObject(onoff, "cluster_id", 0x0006);
                cJSON_AddBoolToObject(onoff, "on", ep->server_OnOffClusterObj->on_off);
                cJSON_AddNumberToObject(onoff, "on_time", ep->server_OnOffClusterObj->on_time);
                cJSON_AddNumberToObject(onoff, "off_wait_time", ep->server_OnOffClusterObj->off_wait_time);
                cJSON_AddNumberToObject(onoff, "start_up", ep->server_OnOffClusterObj->start_up_on_off);
                cJSON_AddNumberToObject(onoff, "last_update_ms", ep->server_OnOffClusterObj->last_update_ms);
                cJSON_AddItemToObject(ep_obj, "onoff", onoff);
           }

           if (ep->output_clusters_count > 0 && ep->output_clusters_array) {
                cJSON *out_clusters = cJSON_CreateArray();
                for (int i = 0; i < ep->output_clusters_count; i++) {
                    cJSON_AddItemToArray(out_clusters, cJSON_CreateNumber(ep->output_clusters_array[i]));
                }
                cJSON_AddItemToObject(ep_obj, "output_clusters", out_clusters);
            }

           // Power Configuration Cluster
            /*if (ep->is_use_power_configuration_cluster && ep->server_PowerConfigurationClusterObj) {
                cJSON *power_config = cJSON_CreateObject();
                cJSON_AddNumberToObject(power_config, "cluster_id", 0x0001);

                // Battery 1
                cJSON_AddNumberToObject(power_config, "battery_voltage", ep->server_PowerConfigurationClusterObj->battery_voltage);
                cJSON_AddNumberToObject(power_config, "battery_percentage", ep->server_PowerConfigurationClusterObj->battery_percentage);
                cJSON_AddStringToObject(power_config, "battery_voltage_str", get_battery_voltage_string(ep->server_PowerConfigurationClusterObj->battery_voltage));
                cJSON_AddStringToObject(power_config, "battery_percentage_str", get_battery_percentage_string(ep->server_PowerConfigurationClusterObj->battery_percentage));

                // Battery Info
                cJSON_AddStringToObject(power_config, "battery_manufacturer", ep->server_PowerConfigurationClusterObj->battery_manufacturer);
                cJSON_AddNumberToObject(power_config, "battery_size", ep->server_PowerConfigurationClusterObj->battery_size);
                cJSON_AddStringToObject(power_config, "battery_size_str", get_battery_size_string(ep->server_PowerConfigurationClusterObj->battery_size));
                cJSON_AddNumberToObject(power_config, "battery_a_hr_rating", ep->server_PowerConfigurationClusterObj->battery_a_hr_rating);
                cJSON_AddNumberToObject(power_config, "battery_quantity", ep->server_PowerConfigurationClusterObj->battery_quantity);
                cJSON_AddNumberToObject(power_config, "battery_rated_voltage", ep->server_PowerConfigurationClusterObj->battery_rated_voltage);
                cJSON_AddNumberToObject(power_config, "battery_alarm_mask", ep->server_PowerConfigurationClusterObj->battery_alarm_mask);
                cJSON_AddNumberToObject(power_config, "battery_voltage_min_th", ep->server_PowerConfigurationClusterObj->battery_voltage_min_th);
                cJSON_AddNumberToObject(power_config, "battery_voltage_th1", ep->server_PowerConfigurationClusterObj->battery_voltage_th1);
                cJSON_AddNumberToObject(power_config, "battery_voltage_th2", ep->server_PowerConfigurationClusterObj->battery_voltage_th2);
                cJSON_AddNumberToObject(power_config, "battery_voltage_th3", ep->server_PowerConfigurationClusterObj->battery_voltage_th3);
                cJSON_AddNumberToObject(power_config, "battery_percentage_min_th", ep->server_PowerConfigurationClusterObj->battery_percentage_min_th);
                cJSON_AddNumberToObject(power_config, "battery_percentage_th1", ep->server_PowerConfigurationClusterObj->battery_percentage_th1);
                cJSON_AddNumberToObject(power_config, "battery_percentage_th2", ep->server_PowerConfigurationClusterObj->battery_percentage_th2);
                cJSON_AddNumberToObject(power_config, "battery_percentage_th3", ep->server_PowerConfigurationClusterObj->battery_percentage_th3);
                cJSON_AddNumberToObject(power_config, "battery_alarm_state", ep->server_PowerConfigurationClusterObj->battery_alarm_state);

                // Mains Power (опционально)
                cJSON_AddNumberToObject(power_config, "mains_voltage", ep->server_PowerConfigurationClusterObj->mains_voltage);
                cJSON_AddNumberToObject(power_config, "mains_frequency", ep->server_PowerConfigurationClusterObj->mains_frequency);
                cJSON_AddNumberToObject(power_config, "mains_alarm_mask", ep->server_PowerConfigurationClusterObj->mains_alarm_mask);
                cJSON_AddNumberToObject(power_config, "mains_voltage_min_th", ep->server_PowerConfigurationClusterObj->mains_voltage_min_th);
                cJSON_AddNumberToObject(power_config, "mains_voltage_max_th", ep->server_PowerConfigurationClusterObj->mains_voltage_max_th);
                cJSON_AddNumberToObject(power_config, "mains_dwell_trip_point", ep->server_PowerConfigurationClusterObj->mains_dwell_trip_point);

                cJSON_AddNumberToObject(power_config, "last_update_ms", ep->server_PowerConfigurationClusterObj->last_update_ms);

                cJSON_AddItemToObject(ep_obj, "power_config", power_config);
            }*/

            cJSON_AddItemToArray(eps, ep_obj);
        }

        cJSON_AddItemToObject(dev_obj, "endpoints", eps);
        cJSON_AddItemToArray(root, dev_obj);
    }

    // Сериализация
    const char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        FILE *file = fopen(filepath, "w");
        if (file) {
            fwrite(json_str, 1, strlen(json_str), file);
            fclose(file);
            ESP_LOGI(TAG, "Devices saved to %s", filepath);
        } else {
            ESP_LOGE(TAG, "Failed to open %s for writing", filepath);
           xSemaphoreGive(g_device_array_mutex);
            cJSON_Delete(root);
            free((void*)json_str);
            return ESP_FAIL;
        }
        free((void*)json_str);
    } else {
        ESP_LOGE(TAG, "Failed to print JSON");
        xSemaphoreGive(g_device_array_mutex);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON_Delete(root);
    xSemaphoreGive(g_device_array_mutex);
    return ESP_OK;
}

esp_err_t zb_manager_queue_save_request(void)
{
    if (zb_manager_save_JSon_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Save queue is NULL! Did you call zb_manager_devices_init()?");
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t cmd = ZB_SAVE_CMD_SAVE;
    BaseType_t ret = xQueueSend(zb_manager_save_JSon_cmd_queue, &cmd, 0);
    if (ret == pdPASS) {
        ESP_LOGD(TAG, "Save request queued");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Failed to queue save request (queue full)");
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t zb_manager_print_RemoteDevicesArray (void)
{
    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take device mutex in zb_manager_print_RemoteDevicesArray");
        return ESP_ERR_TIMEOUT;
    }

    cJSON *root = cJSON_CreateArray();
    if (!root) {
        DEVICE_ARRAY_UNLOCK();
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < RemoteDevicesCount; i++) {
        device_custom_t *dev = RemoteDevicesArray[i];
        if (!dev) continue;

        cJSON *dev_obj = cJSON_CreateObject();
        if (!dev_obj) continue;

        // === device_custom_t поля ===
        char ieee_str[24];
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
            cJSON_AddStringToObject(dev_obj, "manufacturer_name_str",
                zb_manager_get_manufacturer_name_by_code(dev->manufacturer_code));
        }

        // Device-level Basic Cluster
        if (dev->server_BasicClusterObj) {
            cJSON *basic = cJSON_CreateObject();
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
            cJSON_AddItemToObject(dev_obj, "device_basic_cluster", basic);
        }

        // Power Configuration Cluster
            if (dev->server_PowerConfigurationClusterObj) {
                cJSON *power_config = cJSON_CreateObject();
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
                cJSON_AddItemToObject(dev_obj, "power_config", power_config);
            }

        // === Endpoints ===
        cJSON *eps = cJSON_CreateArray();
        for (int j = 0; j < dev->endpoints_count; j++) {
            endpoint_custom_t *ep = dev->endpoints_array[j];
            if (!ep) continue;

            cJSON *ep_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(ep_obj, "ep_id", ep->ep_id);
            cJSON_AddNumberToObject(ep_obj, "is_use_on_device", ep->is_use_on_device);
            cJSON_AddStringToObject(ep_obj, "friendly_name", ep->friendly_name);
            cJSON_AddNumberToObject(ep_obj, "deviceId", ep->deviceId);
            const char* dev_type_name = zb_manager_get_ha_device_type_name(ep->deviceId);
            cJSON_AddStringToObject(ep_obj, "device_type", dev_type_name);

           

            // Identify Cluster
            if (ep->is_use_identify_cluster && ep->server_IdentifyClusterObj) {
                cJSON *identify = cJSON_CreateObject();
                cJSON_AddNumberToObject(identify, "cluster_id", 0x0003);
                cJSON_AddNumberToObject(identify, "identify_time", ep->server_IdentifyClusterObj->identify_time);
                cJSON_AddItemToObject(ep_obj, "identify", identify);
            }

            // Temperature Measurement Cluster
            if (ep->is_use_temperature_measurement_cluster && ep->server_TemperatureMeasurementClusterObj) {
                cJSON *temp = cJSON_CreateObject();
                cJSON_AddNumberToObject(temp, "cluster_id", 0x0402);
                cJSON_AddNumberToObject(temp, "measured_value", ep->server_TemperatureMeasurementClusterObj->measured_value);
                cJSON_AddNumberToObject(temp, "min_measured_value", ep->server_TemperatureMeasurementClusterObj->min_measured_value);
                cJSON_AddNumberToObject(temp, "max_measured_value", ep->server_TemperatureMeasurementClusterObj->max_measured_value);
                cJSON_AddNumberToObject(temp, "tolerance", ep->server_TemperatureMeasurementClusterObj->tolerance);
                cJSON_AddNumberToObject(temp, "last_update_ms", ep->server_TemperatureMeasurementClusterObj->last_update_ms);
                cJSON_AddBoolToObject(temp, "read_error", ep->server_TemperatureMeasurementClusterObj->read_error);
                cJSON_AddItemToObject(ep_obj, "temperature", temp);
            }

            // Humidity Measurement Cluster
            if (ep->is_use_humidity_measurement_cluster && ep->server_HumidityMeasurementClusterObj) {
                cJSON *hum = cJSON_CreateObject();
                cJSON_AddNumberToObject(hum, "cluster_id", 0x0405);
                cJSON_AddNumberToObject(hum, "measured_value", ep->server_HumidityMeasurementClusterObj->measured_value);
                cJSON_AddNumberToObject(hum, "min_measured_value", ep->server_HumidityMeasurementClusterObj->min_measured_value);
                cJSON_AddNumberToObject(hum, "max_measured_value", ep->server_HumidityMeasurementClusterObj->max_measured_value);
                cJSON_AddNumberToObject(hum, "tolerance", ep->server_HumidityMeasurementClusterObj->tolerance);
                cJSON_AddNumberToObject(hum, "last_update_ms", ep->server_HumidityMeasurementClusterObj->last_update_ms);
                cJSON_AddBoolToObject(hum, "read_error", ep->server_HumidityMeasurementClusterObj->read_error);
                cJSON_AddItemToObject(ep_obj, "humidity", hum);
            }

            // OnOff Cluster
            if (ep->is_use_on_off_cluster && ep->server_OnOffClusterObj) {
                cJSON *onoff = cJSON_CreateObject();
                cJSON_AddNumberToObject(onoff, "cluster_id", 0x0006);
                cJSON_AddBoolToObject(onoff, "on", ep->server_OnOffClusterObj->on_off);
                cJSON_AddNumberToObject(onoff, "on_time", ep->server_OnOffClusterObj->on_time);
                cJSON_AddNumberToObject(onoff, "off_wait_time", ep->server_OnOffClusterObj->off_wait_time);
                cJSON_AddNumberToObject(onoff, "start_up", ep->server_OnOffClusterObj->start_up_on_off);
                cJSON_AddNumberToObject(onoff, "last_update_ms", ep->server_OnOffClusterObj->last_update_ms);
                cJSON_AddItemToObject(ep_obj, "onoff", onoff);
            }

            if (ep->output_clusters_count > 0 && ep->output_clusters_array) {
                ESP_LOGI(TAG, "  Output Clusters: ");
                for (int k = 0; k < ep->output_clusters_count; k++) {
                    ESP_LOGI(TAG, "    - 0x%04x (%s)", ep->output_clusters_array[k],
                    zb_manager_get_cluster_name(ep->output_clusters_array[k]) ?: "Unknown");
                }
            }

            cJSON_AddItemToArray(eps, ep_obj);
        }
        cJSON_AddItemToObject(dev_obj, "endpoints", eps);
        cJSON_AddItemToArray(root, dev_obj);
    }

    // Выводим JSON
    char *print_buffer = cJSON_Print(root);
    if (print_buffer) {
        ESP_LOGI(TAG, "Devices State JSON:\n%s", print_buffer);
        free(print_buffer);
    } else {
        ESP_LOGE(TAG, "Failed to print JSON");
    }

    // === Человекочитаемый вывод статуса ===
    ESP_LOGI(TAG, "=== Zigbee Devices Online Status ===");
    uint32_t now = esp_log_timestamp();
    for (int i = 0; i < RemoteDevicesCount; i++) {
        device_custom_t *dev = RemoteDevicesArray[i];
        if (!dev) continue;

        char ieee_str[24] = {0};
        ieee_to_str(ieee_str, dev->ieee_addr);
        const char *name = dev->friendly_name[0] ? dev->friendly_name : "<unknown>";

        // 🔁 Пересчитываем online статус
        bool is_currently_online = false;
        if (dev->last_seen_ms != 0 && dev->device_timeout_ms != 0) {
            uint32_t elapsed = now - dev->last_seen_ms;
            if (now < dev->last_seen_ms) elapsed = UINT32_MAX;
            is_currently_online = (elapsed < dev->device_timeout_ms);
        }

        if (is_currently_online) {
            ESP_LOGI(TAG, "Device: 0x%04x | IEEE: %s | Name: %-20s | Online: YES", 
                    dev->short_addr, ieee_str, name);
        } else {
            if (dev->last_seen_ms == 0) {
                ESP_LOGI(TAG, "Device: 0x%04x | IEEE: %s | Name: %-20s | Online: NO (never seen)", 
                        dev->short_addr, ieee_str, name);
            } else {
                uint32_t secs_ago = (now - dev->last_seen_ms) / 1000;
                ESP_LOGI(TAG, "Device: 0x%04x | IEEE: %s | Name: %-20s | Online: NO (last seen %"PRIu32"s ago)", 
                        dev->short_addr, ieee_str, name, secs_ago);
            }
        }
    }
    ESP_LOGI(TAG, "=== End of Online Status ===");

    cJSON_Delete(root);
    DEVICE_ARRAY_UNLOCK();
    return ESP_OK;
}

esp_err_t zb_manager_load_devices_from_json(const char *filepath) {
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

     // === БЛОКИРОВКА ПЕРЕД ЗАГРУЗКОЙ ===
    if (xSemaphoreTake(g_device_array_mutex, pdMS_TO_TICKS(ZB_DEVICE_MUTEX_TIMEOUT_LONG_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take device mutex in zb_manager_load_devices_from_json");
        free(json_str);
        cJSON_Delete(root);
        return ESP_ERR_TIMEOUT;
    }

    // Очистка
    for (int i = 0; i < RemoteDevicesCount; i++) {
        if (RemoteDevicesArray[i]) {
            free(RemoteDevicesArray[i]);
            RemoteDevicesArray[i] = NULL;
        }
    }

    cJSON *dev_item = NULL;
    cJSON_ArrayForEach(dev_item, root) {
        device_custom_t *dev = calloc(1, sizeof(device_custom_t));
        if (!dev) continue;

        // IEEE
        cJSON *ieee_obj = cJSON_GetObjectItem(dev_item, "ieee_addr");
        if (!ieee_obj || !str_to_ieee(ieee_obj->valuestring, dev->ieee_addr)) {
            free(dev);
            continue;
        }

        dev->short_addr = cJSON_GetObjectItem(dev_item, "short_addr")->valueint;
        dev->is_in_build_status = cJSON_GetObjectItem(dev_item, "is_in_build_status")->valueint;
        dev->index_in_array = cJSON_GetObjectItem(dev_item, "index_in_array")->valueint;
        const char *name = cJSON_GetObjectItem(dev_item, "friendly_name")->valuestring;
        strncpy(dev->friendly_name, name, sizeof(dev->friendly_name) - 1);
        dev->friendly_name_len = strlen(dev->friendly_name);
        dev->capability = cJSON_GetObjectItem(dev_item, "capability")->valueint;
        dev->lqi = cJSON_GetObjectItem(dev_item, "lqi")->valueint;

        cJSON *last_seen_item = cJSON_GetObjectItem(dev_item, "last_seen_ms");
        if (last_seen_item) {
            // При загрузке last_seen_ms обнуляется, потому что esp_log_timestamp() сбрасывается при перезагрузке.
            // Устройство получит актуальный last_seen_ms только после первого ответа.
            //dev->last_seen_ms = last_seen_item->valueint;
            dev->last_seen_ms = 0;
            dev->is_online = false;
        }

        cJSON *timeout_item = cJSON_GetObjectItem(dev_item, "device_timeout_ms");
        if (timeout_item && cJSON_IsNumber(timeout_item)) {
            dev->device_timeout_ms = timeout_item->valueint;
        } else {
            dev->device_timeout_ms = ZB_DEVICE_DEFAULT_TIMEOUT_MS;
        }
        // Обновляем статус
        zb_manager_update_device_online_status(dev);

        cJSON *manuf_code_obj = cJSON_GetObjectItem(dev_item, "manufacturer_code");
        if (manuf_code_obj) {
            dev->manufacturer_code = (uint16_t)manuf_code_obj->valueint;
            ESP_LOGI(TAG, "Loaded manufacturer_code=0x%04x for 0x%04x", dev->manufacturer_code, dev->short_addr);
        }
        // Device-level Basic Cluster
        cJSON *dev_basic = cJSON_GetObjectItem(dev_item, "device_basic_cluster");
        if (dev_basic) {
            dev->server_BasicClusterObj = calloc(1, sizeof(zigbee_manager_basic_cluster_t));
            if (dev->server_BasicClusterObj) {

                cJSON *cluster_id_item = cJSON_GetObjectItem(dev_basic, "cluster_id");
                if (cluster_id_item && cluster_id_item->valueint != 0x0000) {
                    ESP_LOGW(TAG, "Invalid cluster_id for 'basic': expected 0x0000, got 0x%04x", cluster_id_item->valueint);
                    // Можно пропустить или обработать ошибку
                }
                dev->server_BasicClusterObj->zcl_version = cJSON_GetObjectItem(dev_basic, "zcl_version")->valueint;
                dev->server_BasicClusterObj->application_version = cJSON_GetObjectItem(dev_basic, "app_version")->valueint;
                dev->server_BasicClusterObj->stack_version = cJSON_GetObjectItem(dev_basic, "stack_version")->valueint;
                dev->server_BasicClusterObj->hw_version = cJSON_GetObjectItem(dev_basic, "hw_version")->valueint;
                strncpy(dev->server_BasicClusterObj->manufacturer_name,
                        cJSON_GetObjectItem(dev_basic, "manufacturer_name")->valuestring, 64);
                strncpy(dev->server_BasicClusterObj->model_identifier,
                        cJSON_GetObjectItem(dev_basic, "model_id")->valuestring, 33);
                strncpy(dev->server_BasicClusterObj->date_code,
                        cJSON_GetObjectItem(dev_basic, "date_code")->valuestring, 17);
                cJSON *ps_item = cJSON_GetObjectItem(dev_basic, "power_source");
                if (ps_item && cJSON_IsNumber(ps_item)) {
                    dev->server_BasicClusterObj->power_source = ps_item->valueint;
                }
                strncpy(dev->server_BasicClusterObj->location_description,
                        cJSON_GetObjectItem(dev_basic, "location")->valuestring, 17);
                dev->server_BasicClusterObj->physical_environment = cJSON_GetObjectItem(dev_basic, "env")->valueint;
                dev->server_BasicClusterObj->device_enabled = cJSON_GetObjectItem(dev_basic, "enabled")->type == cJSON_True;
                cJSON *last_update_item = cJSON_GetObjectItem(dev_basic, "last_update_ms");
                        if (last_update_item && cJSON_IsNumber(last_update_item)) {
                            dev->server_BasicClusterObj->last_update_ms = last_update_item->valueint;
                        } else {
                            dev->server_BasicClusterObj->last_update_ms = esp_log_timestamp(); // или 0
                        }
            }
        }

        // === Загрузка Power Configuration Cluster (уровень устройства) ===
        cJSON *dev_power_config_item = cJSON_GetObjectItem(dev_item, "device_power_config_cluster");
        if (dev_power_config_item) {
            dev->server_PowerConfigurationClusterObj = calloc(1, sizeof(zb_manager_power_config_cluster_t));
            if (dev->server_PowerConfigurationClusterObj) {

                cJSON *cluster_id_item = cJSON_GetObjectItem(dev_power_config_item, "cluster_id");
                    if (cluster_id_item && cluster_id_item->valueint != 0x0001) {
                        ESP_LOGW(TAG, "Invalid cluster_id for 'power_config': expected 0x0001, got 0x%04x", cluster_id_item->valueint);
                        // Можно пропустить или игнорировать
                    }
                //dev->server_PowerConfigurationClusterObj->cluster_id = 0x0001;

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
                //LOAD_NUMBER(dev_power_config_item, "last_update_ms", dev->server_PowerConfigurationClusterObj->last_update_ms);
                cJSON *last_update_item = cJSON_GetObjectItem(dev_power_config_item, "last_update_ms");
                        if (last_update_item && cJSON_IsNumber(last_update_item)) {
                            dev->server_PowerConfigurationClusterObj->last_update_ms = last_update_item->valueint;
                        } else {
                            dev->server_PowerConfigurationClusterObj->last_update_ms = esp_log_timestamp(); // или 0
                        }
            }
        }


        // Endpoints
        cJSON *eps = cJSON_GetObjectItem(dev_item, "endpoints");
        if (cJSON_IsArray(eps)) {
            dev->endpoints_count = cJSON_GetArraySize(eps);
            dev->endpoints_array = calloc(dev->endpoints_count, sizeof(endpoint_custom_t*));

            for (int j = 0; j < dev->endpoints_count; j++) {
                cJSON *ep_obj = cJSON_GetArrayItem(eps, j);
                if (!ep_obj) continue;

                endpoint_custom_t *ep = calloc(1, sizeof(endpoint_custom_t));
                if (!ep) continue;

                ep->ep_id = cJSON_GetObjectItem(ep_obj, "ep_id")->valueint;
                ep->is_use_on_device = cJSON_GetObjectItem(ep_obj, "is_use_on_device")->valueint;
                const char *ep_name = cJSON_GetObjectItem(ep_obj, "friendly_name")->valuestring;
                strncpy(ep->friendly_name, ep_name, sizeof(ep->friendly_name) - 1);
                ep->deviceId = cJSON_GetObjectItem(ep_obj, "deviceId")->valueint;
                // Device Type (textual)
                cJSON *dev_type_obj = cJSON_GetObjectItem(ep_obj, "device_type");
                if (dev_type_obj && dev_type_obj->valuestring) {
                    strncpy(ep->device_Id_text, dev_type_obj->valuestring, sizeof(ep->device_Id_text) - 1);
                    ep->device_Id_text[sizeof(ep->device_Id_text) - 1] = '\0';
                } else {
                    ep->device_Id_text[0] = '\0';
                }

                // Basic Cluster
                /*cJSON *basic = cJSON_GetObjectItem(ep_obj, "basic");
                if (basic) {
                    // Проверим cluster_id
                        cJSON *cluster_id_item = cJSON_GetObjectItem(basic, "cluster_id");
                        if (cluster_id_item && cluster_id_item->valueint != 0x0000) {
                            ESP_LOGW(TAG, "Invalid cluster_id for 'basic': expected 0x0000, got 0x%04x", cluster_id_item->valueint);
                            // Можно пропустить или обработать ошибку
                        }
                        ep->is_use_basic_cluster = 1;
                        ep->server_BasicClusterObj = calloc(1, sizeof(zigbee_manager_basic_cluster_t));
                    if (ep->server_BasicClusterObj) {
                        ep->server_BasicClusterObj->zcl_version = cJSON_GetObjectItem(basic, "zcl_version")->valueint;
                        ep->server_BasicClusterObj->application_version = cJSON_GetObjectItem(basic, "app_version")->valueint;
                        ep->server_BasicClusterObj->stack_version = cJSON_GetObjectItem(basic, "stack_version")->valueint;
                        ep->server_BasicClusterObj->hw_version = cJSON_GetObjectItem(basic, "hw_version")->valueint;
                        strncpy(ep->server_BasicClusterObj->manufacturer_name,
                                cJSON_GetObjectItem(basic, "manufacturer_name")->valuestring, 64);
                        strncpy(ep->server_BasicClusterObj->model_identifier,
                                cJSON_GetObjectItem(basic, "model_id")->valuestring, 33);
                        strncpy(ep->server_BasicClusterObj->date_code,
                                cJSON_GetObjectItem(basic, "date_code")->valuestring, 17);
                        cJSON *ps_item = cJSON_GetObjectItem(basic, "power_source");
                        if (ps_item && cJSON_IsNumber(ps_item)) {
                            ep->server_BasicClusterObj->power_source = ps_item->valueint;
                        }
                        strncpy(ep->server_BasicClusterObj->location_description,
                                cJSON_GetObjectItem(basic, "location")->valuestring, 17);
                        ep->server_BasicClusterObj->physical_environment = cJSON_GetObjectItem(basic, "env")->valueint;
                        ep->server_BasicClusterObj->device_enabled = cJSON_GetObjectItem(basic, "enabled")->type == cJSON_True;

                        cJSON *last_update_item = cJSON_GetObjectItem(basic, "last_update_ms");
                        if (last_update_item && cJSON_IsNumber(last_update_item)) {
                            ep->server_BasicClusterObj->last_update_ms = last_update_item->valueint;
                        } else {
                            ep->server_BasicClusterObj->last_update_ms = esp_log_timestamp(); // или 0
                        }

                    }
                }*/

                // Identify Cluster
                cJSON *identify = cJSON_GetObjectItem(ep_obj, "identify");
                if (identify) {
                    // Проверим cluster_id
                    cJSON *cluster_id_item = cJSON_GetObjectItem(identify, "cluster_id");
                    if (cluster_id_item && cluster_id_item->valueint != 0x0003) {
                        ESP_LOGW(TAG, "Invalid cluster_id for 'identify': expected 0x0000, got 0x%04x", cluster_id_item->valueint);
                        // Можно пропустить или обработать ошибку
                    }
                    ep->is_use_identify_cluster = 1;
                    ep->server_IdentifyClusterObj = calloc(1, sizeof(zb_manager_identify_cluster_t));
                    if (ep->server_IdentifyClusterObj) {
                        ep->server_IdentifyClusterObj->identify_time = cJSON_GetObjectItem(identify, "identify_time")->valueint;
                    }
                }

                // Temperature Cluster
                cJSON *temp = cJSON_GetObjectItem(ep_obj, "temperature");
                if (temp) {
                    ep->is_use_temperature_measurement_cluster = 1;
                    // Проверим cluster_id
                    cJSON *cluster_id_item = cJSON_GetObjectItem(temp, "cluster_id");
                    if (cluster_id_item && cluster_id_item->valueint != 0x0402) {
                        ESP_LOGW(TAG, "Invalid cluster_id for 'temperature': expected 0x0000, got 0x%04x", cluster_id_item->valueint);
                        // Можно пропустить или обработать ошибку
                    }
                    ep->server_TemperatureMeasurementClusterObj = calloc(1, sizeof(zb_manager_temperature_measurement_cluster_t));
                    if (ep->server_TemperatureMeasurementClusterObj) {
                        ep->server_TemperatureMeasurementClusterObj->measured_value = cJSON_GetObjectItem(temp, "measured_value")->valueint;
                        ep->server_TemperatureMeasurementClusterObj->min_measured_value = cJSON_GetObjectItem(temp, "min_measured_value")->valueint;
                        ep->server_TemperatureMeasurementClusterObj->max_measured_value = cJSON_GetObjectItem(temp, "max_measured_value")->valueint;
                        ep->server_TemperatureMeasurementClusterObj->tolerance = cJSON_GetObjectItem(temp, "tolerance")->valueint;
                        ep->server_TemperatureMeasurementClusterObj->last_update_ms = cJSON_GetObjectItem(temp, "last_update_ms")->valueint;
                        ep->server_TemperatureMeasurementClusterObj->read_error = cJSON_GetObjectItem(temp, "read_error")->type == cJSON_True;
                    }
                }

                // Humidity Cluster
                cJSON *hum = cJSON_GetObjectItem(ep_obj, "humidity");
                if (hum) {
                    cJSON *cluster_id_item = cJSON_GetObjectItem(hum, "cluster_id");
                    if (cluster_id_item && cluster_id_item->valueint != 0x0405) {
                        ESP_LOGW(TAG, "Invalid cluster_id for 'humidity': expected 0x0000, got 0x%04x", cluster_id_item->valueint);
                        // Можно пропустить или обработать ошибку
                    }
                    ep->is_use_humidity_measurement_cluster = 1;
                    ep->server_HumidityMeasurementClusterObj = calloc(1, sizeof(zb_manager_humidity_measurement_cluster_t));
                    if (ep->server_HumidityMeasurementClusterObj) {
                        ep->server_HumidityMeasurementClusterObj->measured_value = cJSON_GetObjectItem(hum, "measured_value")->valueint;
                        ep->server_HumidityMeasurementClusterObj->min_measured_value = cJSON_GetObjectItem(hum, "min_measured_value")->valueint;
                        ep->server_HumidityMeasurementClusterObj->max_measured_value = cJSON_GetObjectItem(hum, "max_measured_value")->valueint;
                        ep->server_HumidityMeasurementClusterObj->tolerance = cJSON_GetObjectItem(hum, "tolerance")->valueint;
                        ep->server_HumidityMeasurementClusterObj->last_update_ms = cJSON_GetObjectItem(hum, "last_update_ms")->valueint;
                        ep->server_HumidityMeasurementClusterObj->read_error = cJSON_GetObjectItem(hum, "read_error")->type == cJSON_True;
                    }
                }

                // ON/OFF Cluster
                cJSON *onoff = cJSON_GetObjectItem(ep_obj, "onoff");
                if (onoff) {
                        cJSON *cluster_id_item = cJSON_GetObjectItem(onoff, "cluster_id");
                        if (cluster_id_item && cluster_id_item->valueint != 0x0006) {
                            ESP_LOGW(TAG, "Invalid cluster_id for 'onoff': expected 0x0006, got 0x%04x", cluster_id_item->valueint);
                            continue; // или игнорировать
                        }
                    ep->is_use_on_off_cluster = 1;
                    ep->server_OnOffClusterObj = calloc(1, sizeof(zb_manager_on_off_cluster_t));
                    if (ep->server_OnOffClusterObj) {
                        ep->server_OnOffClusterObj->on_off = cJSON_GetObjectItem(onoff, "on")->type == cJSON_True;
                        ep->server_OnOffClusterObj->on_time = cJSON_GetObjectItem(onoff, "on_time")->valueint;
                        ep->server_OnOffClusterObj->off_wait_time = cJSON_GetObjectItem(onoff, "off_wait_time")->valueint;
                        ep->server_OnOffClusterObj->start_up_on_off = cJSON_GetObjectItem(onoff, "start_up")->valueint;
                        ep->server_OnOffClusterObj->last_update_ms = cJSON_GetObjectItem(onoff, "last_update_ms")->valueint;
                    }
                }

                // output clusters
                cJSON *out_clusters = cJSON_GetObjectItem(ep_obj, "output_clusters");
                if (out_clusters && cJSON_IsArray(out_clusters)) {
                    int count = cJSON_GetArraySize(out_clusters);
                    ep->output_clusters_count = count;
                    ep->output_clusters_array = calloc(count, sizeof(uint16_t));
                    if (ep->output_clusters_array) {
                        for (int k = 0; k < count; k++) {
                            cJSON *item = cJSON_GetArrayItem(out_clusters, k);
                            if (cJSON_IsNumber(item)) {
                                ep->output_clusters_array[k] = (uint16_t)item->valueint;
                            }
                        }
                    }
                }
                

                dev->endpoints_array[j] = ep;
            }
        }

        // Найти свободный слот
        for (int idx = 0; idx < RemoteDevicesCount; idx++) {
            if (RemoteDevicesArray[idx] == NULL) {
                RemoteDevicesArray[idx] = dev;
                break;
            }
        }
    }
    for (int i = 0; i < RemoteDevicesCount; i++) {
        if (RemoteDevicesArray[i]) {
            zb_manager_configure_device_timeout(RemoteDevicesArray[i]);
        }
    }
    DEVICE_ARRAY_UNLOCK();
    //free(json_str);
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Devices loaded from JSON: %s", filepath);
    return ESP_OK;
}


esp_err_t json_load_and_print(const char *filepath)
{
    char *file_buffer = NULL;
    FILE *f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        return ESP_ERR_NOT_FOUND;
    }

    // Определяем размер файла
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size == 0) {
        ESP_LOGE(TAG, "File is empty: %s", filepath);
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }

    // Читаем содержимое файла
    file_buffer = calloc(1, file_size + 1);
    if (!file_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for file buffer");
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t read_size = fread(file_buffer, 1, file_size, f);
    if (read_size != file_size) {
        ESP_LOGE(TAG, "Failed to read full file");
        free(file_buffer);
        fclose(f);
        return ESP_FAIL;
    }
    file_buffer[file_size] = '\0'; // Убедимся, что строка завершена

    fclose(f);

    // Парсим JSON
    cJSON *json = cJSON_Parse(file_buffer);
    if (!json) {
        ESP_LOGE(TAG, "JSON parse failed: %s", cJSON_GetErrorPtr());
        if (file_buffer) free(file_buffer);
        return ESP_ERR_INVALID_ARG;
    }

    // Печатаем JSON в лог (красиво)
    char *print_buffer = cJSON_Print(json);
    if (print_buffer) {
        ESP_LOGI(TAG, "Loaded JSON for Print to Log from %s:\n%s", filepath, print_buffer);
        free(print_buffer);
    } else {
        ESP_LOGE(TAG, "Failed to print JSON");
    }

    // Освобождаем
    cJSON_Delete(json);
    free(file_buffer);

    return ESP_OK;
}

bool zb_manager_is_device_always_powered(device_custom_t *dev)
{
    if (!dev || !dev->server_BasicClusterObj) {
        ESP_LOGW(TAG, "zb_manager_is_device_always_powered Invalid device or Basic Cluster");
        return false;
    }

    uint8_t ps = dev->server_BasicClusterObj->power_source & 0x7F;

    return (ps == ESP_ZB_ZCL_BASIC_POWER_SOURCE_MAINS_SINGLE_PHASE ||
            ps == ESP_ZB_ZCL_BASIC_POWER_SOURCE_MAINS_THREE_PHASE);
}

void zb_manager_apply_device_fixups(device_custom_t *dev) {
    if (!dev || !dev->server_BasicClusterObj) return;

    uint8_t ps = dev->server_BasicClusterObj->power_source & 0x7F;
    if (ps != ESP_ZB_ZCL_BASIC_POWER_SOURCE_BATTERY) return;

    const char *model = dev->server_BasicClusterObj->model_identifier;
    if (!model || strlen(model) == 0) return;

    bool should_be_mains = false;
    const char *reason = NULL;

    // Проверка: это датчик? → не трогаем
    bool is_sensor = false;
    for (int j = 0; j < dev->endpoints_count; j++) {
        endpoint_custom_t *ep = dev->endpoints_array[j];
        if (!ep) continue;
        uint16_t dev_id = ep->deviceId;
        if (dev_id == ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID ||
            //dev_id == ESP_ZB_HA_HUMIDITY_SENSOR_DEVICE_ID ||
            dev_id == ESP_ZB_HA_IAS_ZONE_ID ||
            dev_id == ESP_ZB_HA_LIGHT_SENSOR_DEVICE_ID){
            //dev_id == ESP_ZB_HA_OCCUPANCY_SENSOR_DEVICE_ID) {
            is_sensor = true;
            break;
        }
    }

    if (is_sensor) {
        return; // ❌ Это датчик — не исправляем
    }

    // ========== TUYA ==========
    if (dev->manufacturer_code == 0x1002 || 
        (dev->ieee_addr[7] == 0xa4 && dev->ieee_addr[6] == 0xc1 && dev->ieee_addr[5] == 0x38)) {
        
        // Обычные выключатели/розетки
        if (strstr(model, "TS00") ||
            strstr(model, "TS01") ||
            strstr(model, "TS02") ||
            strstr(model, "TS0601") ||
            strstr(model, "TS0501") ||
            strstr(model, "TS011F") ||
            strstr(model, "TS0001")) {
            should_be_mains = true;
            reason = "Tuya switch/plug";
        }
        // Moes, _TZ3000_xxxxxx
        else if (strncmp(model, "_TZ3000_", 9) == 0 ||
                 strncmp(model, "_TZ3210_", 9) == 0) {
            should_be_mains = true;
            reason = "Tuya _TZ3000/_TZ3210";
        }
        // TZE200 — реле, шторы
        else if (strncmp(model, "TZE200", 6) == 0) {
            should_be_mains = true;
            reason = "Tuya TZE200";
        }
        // Lonsonho, Teckin, Smart9
        else if (strstr(model, "LS") ||
                 strstr(model, "SNZB") ||
                 strstr(model, "S9")) {
            should_be_mains = true;
            reason = "Tuya-based switch";
        }
        // Moes термостаты
        else if (strstr(model, "BHT-") ||
                 strstr(model, "BAC-002") ||
                 strstr(model, "Moes-TRV")) {
            should_be_mains = true;
            reason = "Moes thermostat";
        }
    }

    if (should_be_mains) {
        ESP_LOGI("DEVICE_FIXUP", "🔧 Fixing power_source: %s (0x%04x) '%s' → Mains [%s]",
                 dev->server_BasicClusterObj->manufacturer_name[0] ? dev->server_BasicClusterObj->manufacturer_name : "Unknown",
                 dev->short_addr, model, reason);
        dev->server_BasicClusterObj->power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_MAINS_SINGLE_PHASE;
    }
}

