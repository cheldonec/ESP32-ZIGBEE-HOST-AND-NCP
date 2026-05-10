/*MIT License

Copyright (c) 2025 Lyxt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.*/

#include "dns_hijack.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include <string.h>
#include <arpa/inet.h>

#define DNS_PORT 53
#define DNS_RESPONSE_IP "192.168.4.1"

static const char *TAG = "dns_hijack";

typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) dns_header_t;

static TaskHandle_t dns_task_handle = NULL;

static void dns_server_task(void *pvParameters) {
    int sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[512];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create DNS socket");
        vTaskDelete(NULL);
        dns_task_handle = NULL;
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DNS_PORT);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "DNS socket bind failed");
        close(sock);
        vTaskDelete(NULL);
        dns_task_handle = NULL;
        return;
    }

    ESP_LOGI(TAG, "DNS Hijack server started on port %d", DNS_PORT);

    while (1) {
        int len = recvfrom(sock, buffer, sizeof(buffer), 0,
                           (struct sockaddr *)&client_addr, &client_addr_len);
        if (len < sizeof(dns_header_t)) continue;

        dns_header_t *dns = (dns_header_t *)buffer;
        ESP_LOGD(TAG, "DNS query ID: 0x%04X", ntohs(dns->id));

        // Prepare response
        dns->flags = htons(0x8180);
        dns->ancount = htons(1);
        dns->nscount = 0;
        dns->arcount = 0;

        int query_end = sizeof(dns_header_t);
        while (buffer[query_end] != 0 && query_end < len) {
            query_end++;
        }
        query_end += 5;

        char *answer = buffer + query_end;
        answer[0] = 0xC0; answer[1] = 0x0C;
        answer[2] = 0x00; answer[3] = 0x01; // A record
        answer[4] = 0x00; answer[5] = 0x01; // IN class
        answer[6] = 0x00; answer[7] = 0x00; answer[8] = 0x00; answer[9] = 0x3C; // TTL 60s
        answer[10] = 0x00; answer[11] = 0x04; // Data len 4
        inet_pton(AF_INET, DNS_RESPONSE_IP, answer + 12);

        int response_len = query_end + 16;
        sendto(sock, buffer, response_len, 0, (struct sockaddr *)&client_addr, client_addr_len);
    }

    close(sock);
    vTaskDelete(NULL);
    dns_task_handle = NULL;
}

void init_dns_hijack() {
    if (dns_task_handle) {
        vTaskDelete(dns_task_handle);
        dns_task_handle = NULL;
    }
    ESP_LOGI(TAG, "Starting DNS hijack task...");
    xTaskCreatePinnedToCore(dns_server_task, "dns_server_task", 4096, NULL, 5, &dns_task_handle, 1);
}

void deinit_dns_hijack() {
    if (dns_task_handle) {
        vTaskDelete(dns_task_handle);
        dns_task_handle = NULL;
        ESP_LOGI(TAG, "DNS hijack stopped");
    }
}