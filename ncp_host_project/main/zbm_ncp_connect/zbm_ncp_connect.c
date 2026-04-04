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

    ESP_ERROR_CHECK(zbm_start_main_zigbee_task(0)); //in "ncp_host_zb_api.h"

    
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
    memset(CoordinatorIeeeAdr,0,8);

    // Команда на запуск Zigbee Stack
    ESP_ERROR_CHECK(zbm_to_ncp_cmd_start_zigbee_stack());
    ESP_LOGI(TAG, "Zigbee stack start OK");
    zigbee_ncp_module_state = STARTED;

    zbm_to_ncp_req_get_coord_long_addr(CoordinatorIeeeAdr);

    ESP_LOGI(TAG,  "CoordinatorIeeeAdr: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", CoordinatorIeeeAdr[0], CoordinatorIeeeAdr[1], CoordinatorIeeeAdr[2],
                CoordinatorIeeeAdr[3], CoordinatorIeeeAdr[4], CoordinatorIeeeAdr[5], CoordinatorIeeeAdr[6], CoordinatorIeeeAdr[7]);
    
    zigbee_ncp_module_state = WORKING;

    return result;
}

esp_err_t zbm_host_reinit_on_ncp_foulture(void)
{
    // на всякий случай сохраняем состояние устройств
    zbm_save_all_devices_to_spiffs_safe();
    ESP_LOGW(TAG, "🔄 zb_manager_ncp_host_restart_on_ncp_foulture Full Zigbee NCP restart initiated");
    uint32_t start_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Heap before restart: %u bytes", start_free);

    zigbee_ncp_module_state = RESTARTING;

    // === 1. Останавливаем Zigbee стек (очереди, семафор) ===
    ESP_LOGW(TAG, "🔄 zb_manager_ncp_host_restart_on_ncp_foulture -> esp_zb_stack_shutdown()");
    // Останавливаем шину
    if (esp_host_bus_stop(s_host_dev.bus)== ESP_OK){
            ESP_LOGI(TAG, "🛑 esp_host_bus_stop OK");
        }

    if (esp_host_stop() == ESP_OK){
        ESP_LOGI(TAG, "🛑 esp_host_stop OK");
    }
    //vTaskDelay(pdMS_TO_TICKS(300));

    if (esp_host_deinit() == ESP_OK){
        ESP_LOGI(TAG, "🛑 esp_host_deinit OK");
    }

    //vTaskDelay(pdMS_TO_TICKS(300));
    //удаляем таск и очередь
    if (ncp_host_output_queue) {
        vQueueDelete(ncp_host_output_queue);
        ncp_host_output_queue = NULL;
        ESP_LOGD(TAG, "✅ Deleted output_queue");
        ESP_LOGI(TAG, "✅ esp_zb_stack_shutdown Deleted output_queue.");
    }

    if (ncp_host_notify_queue) {
        vQueueDelete(ncp_host_notify_queue);
        ncp_host_notify_queue = NULL;
        ESP_LOGD(TAG, "✅ Deleted notify_queue");
        ESP_LOGI(TAG, "✅ esp_zb_stack_shutdown Deleted notify_queue.");
    }

    if (ncp_host_lock_semaphore) {
        vSemaphoreDelete(ncp_host_lock_semaphore);
        ncp_host_lock_semaphore = NULL;
        ESP_LOGD(TAG, "✅ Deleted lock_semaphore");
        ESP_LOGI(TAG, "✅ esp_zb_stack_shutdown Deleted lock_semaphore.");
    }
    
    // ✅ Принудительно удаляем, если осталась
    TaskHandle_t host_task = xTaskGetHandle("host_task");
    if (host_task != NULL) {
        ESP_LOGW(TAG, "Found orphaned 'host_task' - deleting...");
        vTaskDelete(host_task);
        host_task = NULL;
    }

    //vTaskDelay(pdMS_TO_TICKS(300));

    TaskHandle_t bus_task = xTaskGetHandle("host_bus_task");
    if (bus_task != NULL) {
        ESP_LOGW(TAG, "Found orphaned 'host_bus_task' - deleting...");
        vTaskDelete(bus_task);
        bus_task = NULL;
    }

    vTaskDelay(pdMS_TO_TICKS(3000));

    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };

    
    // Пересоздаём платформу Zigbee
    ncp_host_output_queue = xQueueCreate(HOST_EVENT_QUEUE_LEN, sizeof(esp_host_zb_ctx_t)); // output_queue init in "ncp_host_zb_api.h"
    ncp_host_notify_queue = xQueueCreate(HOST_EVENT_QUEUE_LEN, sizeof(esp_host_zb_ctx_t)); // notify_queue init in "ncp_host_zb_api.h"
    ncp_host_lock_semaphore = xSemaphoreCreateRecursiveMutex();                            // lock_semaphore init in "ncp_host_zb_api.h"

    ESP_ERROR_CHECK(esp_host_init(config.host_config.host_mode));
    ESP_ERROR_CHECK(esp_host_start());

    // Здесь надо 3 сек ожидание так как NCP ребутнётся при подключении порта

    vTaskDelay(3000 / portTICK_PERIOD_MS); // задержка 3 sec

    // Инициализируем IEEE адрес координатора
    memset(CoordinatorIeeeAdr,0,8);
    //vTaskDelay(500 / portTICK_PERIOD_MS); // задержка 500 мс

    // Команда инициализации Zigbee Stack (2 попытки)
    esp_err_t result;
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
    memset(CoordinatorIeeeAdr,0,8);

    // Команда на запуск Zigbee Stack
    ESP_ERROR_CHECK(zbm_to_ncp_cmd_start_zigbee_stack());
    ESP_LOGI(TAG, "Zigbee stack start OK");
    zigbee_ncp_module_state = STARTED;

    result = zbm_to_ncp_req_get_coord_long_addr(CoordinatorIeeeAdr);

    ESP_LOGI(TAG,  "CoordinatorIeeeAdr: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", CoordinatorIeeeAdr[0], CoordinatorIeeeAdr[1], CoordinatorIeeeAdr[2],
                CoordinatorIeeeAdr[3], CoordinatorIeeeAdr[4], CoordinatorIeeeAdr[5], CoordinatorIeeeAdr[6], CoordinatorIeeeAdr[7]);
    
    if (result == ESP_OK)
    {
        zigbee_ncp_module_state = WORKING;
    }else 
    {
        zigbee_ncp_module_state = FOULTED;
    }

    vTaskDelay(3000 / portTICK_PERIOD_MS); // задержка 3 sec
    uint32_t end_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Heap after restart: %u bytes, delta: %d", end_free, (int)(end_free - start_free));

    return result;
}