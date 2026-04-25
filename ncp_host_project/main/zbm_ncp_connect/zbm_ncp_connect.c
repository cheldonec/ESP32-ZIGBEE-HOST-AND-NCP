#include "zbm_ncp_connect.h"
#include "ncp_host_zb_api.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ncp_host_zb_api_to_ncp.h"
#include "zbm_core_sync.h"
#include "zbm_coordinator.h"

static const char* TAG = "ZBM_NCP_CONNECT";

QueueHandle_t               ncp_host_output_queue = NULL;           /*!< The queue handler for wait response */
QueueHandle_t               ncp_host_notify_queue = NULL;           /*!< The queue handler for wait notification */
SemaphoreHandle_t           ncp_host_lock_semaphore = NULL;
esp_zb_ieee_addr_t          CoordinatorIeeeAdr;
bool                        g_zigbee_restarting = false; 
bool                        isZigbeeNetworkOpened = false;
zigbee_ncp_module_state_e   zigbee_ncp_module_state = NOT_INIT;



// Буфер под служебные данные задачи
    static StaticTask_t xZB_TaskBuffer;
    // Буфер под стек задачи
    static StackType_t xZB_Stack[ZIGBEE_STACK_SIZE];

    static TaskHandle_t xZB_Handle;
    TaskHandle_t xZB_TaskHandle = NULL; // для управления xZB_Handle

static void main_zigbee_task(void *pvParameters)
{
    
    ESP_LOGI(TAG, "Zigbee main Task started");

    zbm_zigbee_stack_main_loop();
    vTaskDelete(NULL);
}

static esp_err_t zbm_start_main_zigbee_task(uint8_t core)
{
        //zb_manager_register_event_action_handler(&zb_manager_event_action_handler, NULL);

        xZB_Handle = xTaskCreateStaticPinnedToCore(main_zigbee_task, "esp_zb_task", ZIGBEE_STACK_SIZE, NULL, ZIGBEE_TASK_PRIORITY, xZB_Stack, &xZB_TaskBuffer, core);
        if (xZB_Handle == NULL)
        {
            ESP_LOGE(TAG, "Failed to create Zigbee task");
            return ESP_FAIL;
        }else
        {
            xZB_TaskHandle = xZB_Handle; // сохраняем указатель для управления
            return ESP_OK;
        }
}

esp_err_t zbm_ncp_connect_start(void)
{
    esp_err_t result = ESP_OK;
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };

    
    // Здесь надо перезапустить NCP через пин 3сек + 2 сек ожидание на случай если Хост ребутнулся

    ncp_host_output_queue = xQueueCreate(HOST_EVENT_QUEUE_LEN, sizeof(esp_host_zb_ctx_t)); // output_queue init in "ncp_host_zb_api.h"
    ncp_host_notify_queue = xQueueCreate(HOST_EVENT_QUEUE_LEN, sizeof(esp_host_zb_ctx_t)); // notify_queue init in "ncp_host_zb_api.h"
    ncp_host_lock_semaphore = xSemaphoreCreateRecursiveMutex();                            // lock_semaphore init in "ncp_host_zb_api.h"

    ESP_ERROR_CHECK(zbm_start_main_zigbee_task(1)); //in "ncp_host_zb_api.h"

    
    ESP_ERROR_CHECK(esp_host_init(config.host_config.host_mode));
    ESP_ERROR_CHECK(esp_host_start());

    // Здесь надо 3 сек ожидание так как NCP ребутнётся при подключении порта

    

   // vTaskDelay(500 / portTICK_PERIOD_MS); // задержка 500 мс
    vTaskDelay(3000 / portTICK_PERIOD_MS); // задержка 3 sec
    // Команда инициализации Zigbee Stack (2 попытки)
    result = zbm_to_ncp_cmd_init_zigbee_stack();
    if (result == ESP_OK) ESP_LOGI(TAG, "Zigbee stack init zbm_to_ncp_cmd_init_zigbee_stack"); else
    {
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        ESP_ERROR_CHECK(zbm_to_ncp_cmd_init_zigbee_stack()); // иначе на ребут
        result = ESP_OK;
        ESP_LOGI(TAG, "Zigbee stack init zbm_to_ncp_cmd_init_zigbee_stack");
    }
    ESP_LOGI(TAG, "Zigbee stack init OK");

    // Регистрируем Эндпоинт
    ESP_ERROR_CHECK(zbm_to_ncp_cmd_register_endpoint(&zbm_coordinator.zb_endpoint));

    // Инициализируем IEEE адрес координатора
    memset(zbm_coordinator.zb_ieee_addr,0,8);

    // Команда на запуск Zigbee Stack
    ESP_ERROR_CHECK(zbm_to_ncp_cmd_start_zigbee_stack());
    ESP_LOGI(TAG, "Zigbee stack start OK");
    zigbee_ncp_module_state = STARTED;

    // Получаем Extended PAN ID
    esp_zb_ieee_addr_t extended_pan_id;
    zbm_to_ncp_req_get_extended_pan_id(extended_pan_id);
    // Копируем Extended PAN ID
    memcpy(zbm_coordinator.zb_extended_pan_id, extended_pan_id, 8);

    ESP_LOGI(TAG,  "zbm_coordinator.zb_extended_pan_id: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", zbm_coordinator.zb_extended_pan_id[0], 
        zbm_coordinator.zb_extended_pan_id[1], zbm_coordinator.zb_extended_pan_id[2], zbm_coordinator.zb_extended_pan_id[3], 
        zbm_coordinator.zb_extended_pan_id[4], zbm_coordinator.zb_extended_pan_id[5], zbm_coordinator.zb_extended_pan_id[6], zbm_coordinator.zb_extended_pan_id[7]);

    // Получаем PAN ID и Radio Channel
    zbm_coordinator.zb_pan_id = zbm_to_ncp_req_get_pan_id();
    zbm_coordinator.zb_radio_channel = zbm_to_ncp_req_get_current_channel();
    ESP_LOGI(TAG,  "zbm_coordinator.zb_pan_id: %d", zbm_coordinator.zb_pan_id);
    ESP_LOGI(TAG,  "zbm_coordinator.zb_radio_channel: %d", zbm_coordinator.zb_radio_channel);

    // Получаем Long IEEE Address
    zbm_to_ncp_req_get_coord_long_addr(zbm_coordinator.zb_ieee_addr);
    ESP_LOGI(TAG,  "zbm_coordinator.zb_ieee_addr: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", zbm_coordinator.zb_ieee_addr[0], 
        zbm_coordinator.zb_ieee_addr[1], zbm_coordinator.zb_ieee_addr[2], zbm_coordinator.zb_ieee_addr[3], 
        zbm_coordinator.zb_ieee_addr[4], zbm_coordinator.zb_ieee_addr[5], zbm_coordinator.zb_ieee_addr[6], zbm_coordinator.zb_ieee_addr[7]);

    zigbee_ncp_module_state = WORKING;

    return result;
}

esp_err_t zbm_host_reinit_on_ncp_foulture(void)
{
    // на всякий случай сохраняем состояние устройств
    zbm_save_all_devices_to_spiffs_safe();
    ESP_LOGW(TAG, "🔄 Full Zigbee NCP restart initiated");
    uint32_t start_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Heap before restart: %u bytes", start_free);

    zigbee_ncp_module_state = RESTARTING;

    // === 1. Останавливаем Zigbee стек ===
    ESP_LOGW(TAG, "🛑 Stopping Zigbee host...");
    if (esp_host_bus_stop(s_host_dev.bus) == ESP_OK) {
        ESP_LOGI(TAG, "✅ esp_host_bus_stop OK");
    }

    if (esp_host_stop() == ESP_OK) {
        ESP_LOGI(TAG, "✅ esp_host_stop OK");
    }

    if (esp_host_deinit() == ESP_OK) {
        ESP_LOGI(TAG, "✅ esp_host_deinit OK");
    }

    // Удаляем очереди и семафор
    if (ncp_host_output_queue) {
        vQueueDelete(ncp_host_output_queue);
        ncp_host_output_queue = NULL;
        ESP_LOGD(TAG, "✅ Deleted output_queue");
    }

    if (ncp_host_notify_queue) {
        vQueueDelete(ncp_host_notify_queue);
        ncp_host_notify_queue = NULL;
        ESP_LOGD(TAG, "✅ Deleted notify_queue");
    }

    if (ncp_host_lock_semaphore) {
        vSemaphoreDelete(ncp_host_lock_semaphore);
        ncp_host_lock_semaphore = NULL;
        ESP_LOGD(TAG, "✅ Deleted lock_semaphore");
    }

    // Удаляем оставшиеся задачи
    TaskHandle_t host_task = xTaskGetHandle("host_task");
    if (host_task != NULL) {
        ESP_LOGW(TAG, "Found orphaned 'host_task' - deleting...");
        vTaskDelete(host_task);
    }

    TaskHandle_t bus_task = xTaskGetHandle("host_bus_task");
    if (bus_task != NULL) {
        ESP_LOGW(TAG, "Found orphaned 'host_bus_task' - deleting...");
        vTaskDelete(bus_task);
    }

    vTaskDelay(pdMS_TO_TICKS(3000));

    // === 2. Пересоздаём платформу ===
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };

    ncp_host_output_queue = xQueueCreate(HOST_EVENT_QUEUE_LEN, sizeof(esp_host_zb_ctx_t));
    ncp_host_notify_queue = xQueueCreate(HOST_EVENT_QUEUE_LEN, sizeof(esp_host_zb_ctx_t));
    ncp_host_lock_semaphore = xSemaphoreCreateRecursiveMutex();

    ESP_ERROR_CHECK(esp_host_init(config.host_config.host_mode));
    ESP_ERROR_CHECK(esp_host_start());

    vTaskDelay(pdMS_TO_TICKS(3000)); // Ждём перезагрузки NCP

    // === 3. Инициализация стека ===
    esp_err_t result = zbm_to_ncp_cmd_init_zigbee_stack();
    if (result != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        ESP_ERROR_CHECK(zbm_to_ncp_cmd_init_zigbee_stack());
    }
    ESP_LOGI(TAG, "✅ Zigbee stack init OK");

    // Регистрируем endpoint из координатора
    ESP_ERROR_CHECK(zbm_to_ncp_cmd_register_endpoint(&zbm_coordinator.zb_endpoint));
    ESP_LOGI(TAG, "✅ Endpoint registered from zbm_coordinator");

    // Запускаем стек
    ESP_ERROR_CHECK(zbm_to_ncp_cmd_start_zigbee_stack());
    ESP_LOGI(TAG, "✅ Zigbee stack started");

    zigbee_ncp_module_state = STARTED;

    // === 4. ВОССТАНАВЛИВАЕМ ДАННЫЕ КООРДИНАТОРА ИЗ NCP ===
    esp_zb_ieee_addr_t extended_pan_id;

    // PAN ID и канал
    zbm_coordinator.zb_pan_id = zbm_to_ncp_req_get_pan_id();
    zbm_coordinator.zb_radio_channel = zbm_to_ncp_req_get_current_channel();

    // Extended PAN ID
    zbm_to_ncp_req_get_extended_pan_id(extended_pan_id);
    memcpy(zbm_coordinator.zb_extended_pan_id, extended_pan_id, 8);

    // IEEE адрес координатора
    zbm_to_ncp_cmd_get_local_long_addr(zbm_coordinator.zb_ieee_addr);

    // Short address всегда 0x0000
    zbm_coordinator.zb_short_address = zbm_to_ncp_req_get_network_short_addr();

    // === 5. Логируем ===
    ESP_LOGI(TAG, "🔁 Zigbee restarted successfully. Current network:");
    ESP_LOGI(TAG, "  PAN ID: 0x%04hx", zbm_coordinator.zb_pan_id);
    ESP_LOGI(TAG, "  Channel: %d", zbm_coordinator.zb_radio_channel);
    ESP_LOGI(TAG, "  Short Addr: 0x%04hx", zbm_coordinator.zb_short_address);
    ESP_LOGI(TAG, "  IEEE: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             zbm_coordinator.zb_ieee_addr[0], zbm_coordinator.zb_ieee_addr[1],
             zbm_coordinator.zb_ieee_addr[2], zbm_coordinator.zb_ieee_addr[3],
             zbm_coordinator.zb_ieee_addr[4], zbm_coordinator.zb_ieee_addr[5],
             zbm_coordinator.zb_ieee_addr[6], zbm_coordinator.zb_ieee_addr[7]);
    ESP_LOGI(TAG, "  Ext PAN ID: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             zbm_coordinator.zb_extended_pan_id[0], zbm_coordinator.zb_extended_pan_id[1],
             zbm_coordinator.zb_extended_pan_id[2], zbm_coordinator.zb_extended_pan_id[3],
             zbm_coordinator.zb_extended_pan_id[4], zbm_coordinator.zb_extended_pan_id[5],
             zbm_coordinator.zb_extended_pan_id[6], zbm_coordinator.zb_extended_pan_id[7]);

    // === 6. Сохраняем в SPIFFS (на всякий случай) ===
    if (zbm_save_coordinator_to_spiffs(&zbm_coordinator)) {
        ESP_LOGI(TAG, "✅ Coordinator saved after restart");
    } else {
        ESP_LOGE(TAG, "❌ Failed to save coordinator after restart");
    }

    zigbee_ncp_module_state = WORKING;

    vTaskDelay(pdMS_TO_TICKS(1000));
    uint32_t end_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Heap after restart: %u bytes, delta: %d", end_free, (int)(end_free - start_free));

    return ESP_OK;
}