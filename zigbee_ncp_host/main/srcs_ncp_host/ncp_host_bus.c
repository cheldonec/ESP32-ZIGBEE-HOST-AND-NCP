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

// Реализация чтения из буфера (если потребуется)
static esp_err_t host_bus_read_hdl(void *buffer, uint16_t size)
{
    esp_host_bus_t *bus = s_host_bus;
    if (!bus || !bus->input_buf || !buffer) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t received = xStreamBufferReceive(bus->input_buf, buffer, size, pdMS_TO_TICKS(10));
    return (received == size) ? ESP_OK : ESP_ERR_TIMEOUT;
}

static esp_err_t host_bus_write_hdl(void *buffer, uint16_t size)
{
    return (uart_write_bytes(CONFIG_HOST_BUS_UART_NUM, (const char*) buffer, size) == size) ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

static esp_err_t host_bus_deinit_hdl(void)
{
    ESP_LOGI(TAG, "🛑 host_bus_deinit_hdl->uart_driver_delete");
    esp_err_t ret = uart_driver_delete(CONFIG_HOST_BUS_UART_NUM);
    uart0_queue = NULL;
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

    if (!dtmp) {
        ESP_LOGE(TAG, "Failed to allocate dtmp buffer");
        vTaskDelete(NULL);
        return;
    }

    esp_host_bus_t *bus = (esp_host_bus_t *)pvParameter;
    bus->state = BUS_INIT_START;
    esp_host_ctx_t host_event = {
        .event = HOST_EVENT_INPUT,
    };

    bool ncp_foult = false;

    while (bus->state == BUS_INIT_START) {
        if (xQueueReceive(uart0_queue, (void *)&event, (TickType_t)portMAX_DELAY)) {
            bzero(dtmp, HOST_BUS_BUF_SIZE);
            switch(event.type) {
                case UART_DATA:
                    host_event.size = uart_read_bytes(CONFIG_HOST_BUS_UART_NUM, dtmp, event.size, portMAX_DELAY);
                    if (host_event.size > 0) {
                        xStreamBufferSend(bus->input_buf, dtmp, host_event.size, 0);
                        ESP_LOGW(TAG, "UART_DATA received, sent to input_buf: %d bytes", host_event.size);
                        esp_host_send_event(&host_event);
                    }
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
                    if (zigbee_ncp_module_state == WORKING) {
                        ncp_foult = true;
                    }
                    break;
                default:
                    ESP_LOGI(TAG, "uart event type: %d", event.type);
                    break;
            }
        }

        if (ncp_foult) {
            zigbee_ncp_module_state = FOULTED;
            if (xZB_TaskHandle != NULL) {
                xTaskNotifyGive(xZB_TaskHandle);
            } else {
                ESP_LOGE(TAG, "❌ xZB_TaskHandle is NULL");
            }
            break;
        }
    }

    // Освобождаем память перед завершением задачи
    if (dtmp) {
        free(dtmp);
        dtmp = NULL;
    }

    // Ожидание уничтожения извне
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Этот код недостижим, но для ясности:
    // vTaskDelete(NULL); // Выполняется автоматически при выходе
}

esp_err_t esp_host_bus_output(const void *buffer, uint16_t len)
{
    if (buffer == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_host_bus_t *bus = s_host_bus;
    if (!bus || !bus->output_buf) {
        return ESP_FAIL;
    }

    esp_host_ctx_t host_event = {
        .event = HOST_EVENT_OUTPUT,
        .size = len
    };

    // Отправляем данные с таймаутом — без циклов ожидания
    xSemaphoreTake(bus->input_sem, portMAX_DELAY);
    size_t sent = xStreamBufferSend(bus->output_buf, buffer, len, pdMS_TO_TICKS(50));
    xSemaphoreGive(bus->input_sem);

    if (sent != len) {
        ESP_LOGE(TAG, "Failed to send to output_buf: %d of %d", sent, len);
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGW(TAG, "esp_host_bus_output: %d bytes queued", len);
    return esp_host_send_event(&host_event);
}

esp_err_t esp_host_bus_input(const void *buffer, uint16_t len)
{
    return esp_host_frame_input(buffer, len);
}

esp_err_t esp_host_bus_init(esp_host_bus_t **bus)
{
    ESP_LOGI(TAG, "start esp_host_bus_init()");
    if (s_host_bus) {
        ESP_LOGW(TAG, "s_host_bus already exists! Cleaning up...");
        esp_host_bus_deinit(s_host_bus);
        s_host_bus = NULL;
    }

    esp_host_bus_t *bus_handle = calloc(1, sizeof(esp_host_bus_t));
    if (!bus_handle) {
        return ESP_ERR_NO_MEM;
    }

    bus_handle->input_buf = xStreamBufferCreate(HOST_BUS_RINGBUF_SIZE, 8);
    if (!bus_handle->input_buf) {
        ESP_LOGE(TAG, "Input buffer create error");
        esp_host_bus_deinit(bus_handle);
        return ESP_ERR_NO_MEM;
    }

    bus_handle->output_buf = xStreamBufferCreate(HOST_BUS_RINGBUF_SIZE, 8);
    if (!bus_handle->output_buf) {
        ESP_LOGE(TAG, "Out buffer create error");
        esp_host_bus_deinit(bus_handle);
        return ESP_ERR_NO_MEM;
    }

    bus_handle->input_sem = xSemaphoreCreateMutex();
    if (!bus_handle->input_sem) {
        ESP_LOGE(TAG, "Input semaphore create error");
        esp_host_bus_deinit(bus_handle);
        return ESP_ERR_NO_MEM;
    }

    bus_handle->init = host_bus_init_hdl;
    bus_handle->deinit = host_bus_deinit_hdl;
    bus_handle->read = host_bus_read_hdl;      // Теперь реализован
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

    return (xTaskCreate(esp_host_bus_task, "host_bus_task", HOST_BUS_TASK_STACK, bus, HOST_BUS_TASK_PRIORITY, NULL) == pdTRUE)
           ? ESP_OK : ESP_FAIL;
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
        ESP_LOGI(TAG, "🛑 vStreamBufferDelete(output_buf)");
    }

    if (bus->input_buf) {
        vStreamBufferDelete(bus->input_buf);
        bus->input_buf = NULL;
        ESP_LOGI(TAG, "🛑 vStreamBufferDelete(input_buf)");
    }

    if (bus->input_sem) {
        vSemaphoreDelete(bus->input_sem);
        bus->input_sem = NULL;
        ESP_LOGI(TAG, "🛑 vSemaphoreDelete(input_sem)");
    }

    bus->read = NULL;
    bus->write = NULL;
    free(bus);
    s_host_bus = NULL;

    return ESP_OK;
}