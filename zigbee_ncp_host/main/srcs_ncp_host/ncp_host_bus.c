/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <sys/select.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"

#include "esp_log.h"
#include "driver/uart.h"

#include "ncp_host_bus.h"
#include "ncp_host_frame.h"
#include "ncp_host.h"
#include "zb_manager_ncp_host.h"

static const char* TAG = "ncp_host_bus.c";

static QueueHandle_t uart0_queue;
static esp_host_bus_t *s_host_bus;


static esp_err_t host_bus_read_hdl(void *buffer, uint16_t size)
{
    return ESP_FAIL;
}

static esp_err_t host_bus_write_hdl(void *buffer, uint16_t size)
{
    return (uart_write_bytes(CONFIG_HOST_BUS_UART_NUM, (const char*) buffer, size) == size) ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

static esp_err_t host_bus_deinit_hdl(void)
{
    ESP_LOGI(TAG, "🛑 host_bus_deinit_hdl->uart_driver_delete");
    esp_err_t ret = uart_driver_delete(CONFIG_HOST_BUS_UART_NUM);
    uart0_queue = NULL;  // 
    return ret;
}

static esp_err_t host_bus_init_hdl(uint8_t transport)
{
    
    ESP_LOGI(TAG, "start host_bus_init_hdl()");
    uart_config_t uart_config = {
        .baud_rate = CONFIG_HOST_BUS_UART_BAUD_RATE,
        .data_bits = CONFIG_HOST_BUS_UART_BYTE_SIZE,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = CONFIG_HOST_BUS_UART_STOP_BITS,
        .flow_ctrl = CONFIG_HOST_BUS_UART_FLOW_CONTROL,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_LOGI(TAG, "start uart_driver_install()");
    uart_driver_install(CONFIG_HOST_BUS_UART_NUM, HOST_BUS_BUF_SIZE * 2, HOST_BUS_BUF_SIZE * 2, 80, &uart0_queue, 0);
    ESP_LOGI(TAG, "start uart_param_config()");
    uart_param_config(CONFIG_HOST_BUS_UART_NUM, &uart_config);
    ESP_LOGI(TAG, "start uart_set_pin()");
    uart_set_pin(CONFIG_HOST_BUS_UART_NUM, CONFIG_HOST_BUS_UART_TX_PIN, CONFIG_HOST_BUS_UART_RX_PIN, CONFIG_HOST_BUS_UART_RTS_PIN, CONFIG_HOST_BUS_UART_CTS_PIN);

    return ESP_OK;
}

static void esp_host_bus_task(void *pvParameter)
{
    ESP_LOGI(TAG, "start esp_host_bus_task STARTED");
    uart_event_t event;
    uint8_t *dtmp = (uint8_t*)malloc(HOST_BUS_BUF_SIZE);

    esp_host_bus_t *bus = (esp_host_bus_t *)pvParameter;
    bus->state = BUS_INIT_START;
    esp_host_ctx_t host_event = {
        .event = HOST_EVENT_INPUT,
    };

    bool ncp_foult = false;
    ESP_LOGI(TAG, "start esp_host_bus_task STARTED and working while while (bus->state == BUS_INIT_START)");
        while (bus->state == BUS_INIT_START) {
       
        if (xQueueReceive(uart0_queue, (void *)&event, (TickType_t)portMAX_DELAY)) {
            bzero(dtmp, HOST_BUS_BUF_SIZE);
            switch(event.type) {
                case UART_DATA:
                    host_event.size = uart_read_bytes(CONFIG_HOST_BUS_UART_NUM, dtmp, event.size, portMAX_DELAY);
                    xStreamBufferSend(bus->input_buf, dtmp, host_event.size, 0);
                    ESP_LOGW(TAG, "esp_host_bus_task->UART_DATA SEND EVENT");
                    esp_host_send_event(&host_event);
                    break;
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow");
                    uart_flush_input(CONFIG_HOST_BUS_UART_NUM);
                    xQueueReset(uart0_queue);
                    break;
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full");
                    uart_flush_input(CONFIG_HOST_BUS_UART_NUM);
                    xQueueReset(uart0_queue);
                    break;
                case UART_BREAK:
                    ESP_LOGW(TAG, "UART BREAK DETECTED - scheduling Zigbee restart");

                    uart_flush_input(CONFIG_HOST_BUS_UART_NUM);
                    xQueueReset(uart0_queue);
                    if (zigbee_ncp_module_state == WORKING)
                    {
                        // ставим флаг выхода из цикла
                        ncp_foult = true;
                    }
                    break;
                default:
                    ESP_LOGI(TAG, "uart event type: %d", event.type);
                    break;
            }
        }
        // если падение - выходим из цикла
            if (ncp_foult) {
                zigbee_ncp_module_state = FOULTED;
                if (xZB_TaskHandle != NULL) {
                    //освобождаем ресурсы
                    if (dtmp) {
                        free(dtmp);
                        dtmp = NULL;
                    }
                    xTaskNotifyGive(xZB_TaskHandle); // отправляем уведомление в esp_zb_stack_main_loop (ncp_host_zb_api), который крутится в esp_zb_task (xb_manager_ncp_host)
                    //vTaskDelay(pdMS_TO_TICKS(100));
                    // выходим в режим ожидания уничтожения процесса извне
                    break;
                } else {
                        ESP_LOGE(TAG, "❌ xZB_TaskHandle is NULL");
                }
            } 
    }
    // Цикл ожидания уничтожения данного процесса извне
        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
           
    if (dtmp) {
        free(dtmp);
        dtmp = NULL;
    }

    vTaskDelete(NULL);
}

esp_err_t esp_host_bus_output(const void *buffer, uint16_t len)
{
    if (buffer == NULL) {
        return ESP_FAIL;
    }

    esp_host_bus_t *bus = s_host_bus;
    esp_host_ctx_t host_event = {
        .event = HOST_EVENT_OUTPUT,
    };
    int count = HOST_BUS_RINGBUF_TIMEOUT_MS / 10;
    size_t ret_size = 0;

    if (bus->output_buf == NULL) {
        return ESP_FAIL;
    }

    while (xStreamBufferSpacesAvailable(bus->output_buf) < len) {
        if(count == 0) 
        {
            ESP_LOGW(TAG, "output_buf not enough line 175");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        count --;
    }

    if (count == 0) {
        ESP_LOGE(TAG, "output_buf not enough");
        return ESP_FAIL;
    }

    xSemaphoreTake(bus->input_sem, portMAX_DELAY);
    ret_size = xStreamBufferSend(bus->output_buf, buffer, len, 0);
    xSemaphoreGive(bus->input_sem);
    if (ret_size != len) {
        ESP_LOGE(TAG, "output_buf send error: size %d expect %d", ret_size, len);
        return ESP_FAIL;
    } else {
        host_event.size = len;
        esp_err_t host_send_event_res = ESP_FAIL;
        ESP_LOGW(TAG, "esp_host_bus_output()");
        host_send_event_res = esp_host_send_event(&host_event);
        return host_send_event_res;
    }
}

esp_err_t esp_host_bus_input(const void *buffer, uint16_t len)
{
    return esp_host_frame_input(buffer, len);
}

esp_err_t esp_host_bus_init(esp_host_bus_t **bus)
{
    ESP_LOGI(TAG, "start esp_host_bus_init()");
    /*if (s_host_bus) {
        ESP_LOGW(TAG, "s_host_bus already exists! Cleaning up...");
        esp_host_bus_deinit(s_host_bus);
        s_host_bus = NULL;
    }*/
    esp_host_bus_t *bus_handle = calloc(1, sizeof(esp_host_bus_t));

    if (!bus_handle) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "creating bus_handle->input_buf = xStreamBufferCreate");
    bus_handle->input_buf = xStreamBufferCreate(HOST_BUS_RINGBUF_SIZE, 8);
    if (bus_handle->input_buf == NULL) {
        ESP_LOGE(TAG, "Input buffer create error");
        esp_host_bus_deinit(bus_handle);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "creating bus_handle->output_buf = xStreamBufferCreate");
    bus_handle->output_buf = xStreamBufferCreate(HOST_BUS_RINGBUF_SIZE, 8);
    if (bus_handle->output_buf == NULL) {
        ESP_LOGE(TAG, "Out buffer create error");
        esp_host_bus_deinit(bus_handle);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "creating bus_handle->input_sem = xSemaphoreCreateMutex()");
    bus_handle->input_sem = xSemaphoreCreateMutex();
    if (bus_handle->input_sem == NULL) {
        ESP_LOGE(TAG, "Input semaphore create error");
        esp_host_bus_deinit(bus_handle);
        return ESP_ERR_NO_MEM;
    }

    bus_handle->init = host_bus_init_hdl;
    bus_handle->deinit = host_bus_deinit_hdl;
    bus_handle->read = host_bus_read_hdl;
    bus_handle->write = host_bus_write_hdl;

    *bus = bus_handle;
    s_host_bus = bus_handle;

    return ESP_OK;
}

esp_err_t esp_host_bus_start(esp_host_bus_t *bus)
{
    ESP_LOGI(TAG, "start esp_host_bus_start");
    if (!bus) {
        ESP_LOGE(TAG, "Invalid handle when start bus");
        return ESP_ERR_INVALID_ARG;
    }

    if (bus->state == BUS_INIT_START) {
        ESP_LOGE(TAG, "Invalid state %d when start bus", bus->state);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "start esp_host_bus_task");
    return (xTaskCreate(esp_host_bus_task, "host_bus_task", HOST_BUS_TASK_STACK, bus, HOST_BUS_TASK_PRIORITY, NULL) == pdTRUE) ? ESP_OK : ESP_FAIL;
}

esp_err_t esp_host_bus_stop(esp_host_bus_t *bus)
{
    if (!bus) {
        ESP_LOGE(TAG, "Invalid handle when stop bus");
        return ESP_OK;
    }

    if (bus->state != BUS_INIT_START) {
        ESP_LOGE(TAG, "Invalid state %d when stop bus", bus->state);
        return ESP_FAIL;
    }

    bus->state = BUS_INIT_STOP;

    return ESP_OK;
}

esp_err_t esp_host_bus_deinit(esp_host_bus_t *bus)
{
    ESP_LOGI(TAG, "🛑 esp_host_bus_deinit");
    if (!bus) {
        ESP_LOGE(TAG, "Invalid handle when deinit");
        return ESP_ERR_INVALID_ARG;
    }

    if (bus->output_buf) {
        vStreamBufferDelete(bus->output_buf);
        bus->output_buf = NULL;
        ESP_LOGI(TAG, "🛑 esp_host_bus_deinit->vStreamBufferDelete(bus->output_buf)");

    }

    if (bus->input_buf) {
        vStreamBufferDelete(bus->input_buf);
        bus->input_buf = NULL;
        ESP_LOGI(TAG, "🛑 esp_host_bus_deinit->vStreamBufferDelete(bus->input_buf)");
    }

    if (bus->input_sem) {
        vSemaphoreDelete(bus->input_sem);
        bus->input_sem = NULL;
        ESP_LOGI(TAG, "🛑 esp_host_bus_deinit->vSemaphoreDelete(bus->input_sem)");
    }

    if(bus->read)
    {
        bus->read = NULL;
    }
    if(bus->write)
    {
        bus->write = NULL;
    }
    free(bus);
    s_host_bus = NULL;

    //ESP_LOGI(TAG, "🛑 esp_host_bus_deinit->uart_driver_delete");
    //return uart_driver_delete(CONFIG_HOST_BUS_UART_NUM);
    return ESP_OK;
}


