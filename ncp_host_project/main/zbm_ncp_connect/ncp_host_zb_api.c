#include "ncp_host_zb_api.h"
#include "string.h"
#include "esp_random.h"
#include "zbm_ncp_connect.h"
#include "ncp_host_zb_api_from_ncp.h"
#include "esp_log.h"

static const char* TAG = "NCP_HOST_ZB_API";


void zbm_zigbee_stack_main_loop(void)
{
    esp_host_zb_ctx_t host_ctx;
    while (1) {
        // Проверяем, не пришло ли уведомление о рестарте NCP
        
        if (zigbee_ncp_module_state == WORKING)
        {
            uint32_t notify_value = ulTaskNotifyTake(pdTRUE, 100); // ждём 100 мс
            if (notify_value > 0 || g_zigbee_restarting) {
                ESP_LOGW(TAG, "🔄 Zigbee stack main loop interrupted for restart");
                zbm_host_reinit_on_ncp_foulture();
                if (host_ctx.data) {
                free(host_ctx.data);
                host_ctx.data = NULL;
                }
                continue; // выходим из цикла
            }
        }

       if (xQueueReceive(ncp_host_notify_queue, &host_ctx, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
       }

       for (int i = 0; i <= host_zb_api_from_ncp_func_table_size; i ++) {
            if (host_ctx.id != host_zb_api_from_ncp_func_table[i].id) {
                continue;
            }

            host_zb_api_from_ncp_func_table[i].set_func(host_ctx.data, host_ctx.size);
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
    zbm_zigbee_stack_main_loop();
}

esp_err_t esp_host_zb_input(esp_host_header_t *host_header, const void *buffer, uint16_t len)
{
    QueueHandle_t queue = (host_header->flags.type == ESP_ZNSP_TYPE_NOTIFY) ? ncp_host_notify_queue : ncp_host_output_queue;
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

        xSemaphoreTakeRecursive(ncp_host_lock_semaphore, portMAX_DELAY);
        esp_host_frame_output(&data_header, buffer, len);

        esp_host_zb_ctx_t host_ctx;
        xQueueReceive(ncp_host_output_queue, &host_ctx, portMAX_DELAY);
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
        xSemaphoreGiveRecursive(ncp_host_lock_semaphore);
    
    return  ESP_OK;
}