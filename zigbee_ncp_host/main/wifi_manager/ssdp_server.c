/*
  ssdp_server.c - Final SSDP server for ESP32
  UPnP-compliant, Windows-compatible, simple and reliable
*/

#include "ssdp_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "nvs_flash.h"
#include "nvs.h"

static const char* TAG = "ssdp_server";

static uint32_t boot_id = 1;

#define SSDP_PORT             1900
#define SSDP_MCAST_ADDR       "239.255.255.250"
#define SSDP_NOTIFY_INTERVAL  pdMS_TO_TICKS(30000)

static bool s_running = false;
static TaskHandle_t s_task_handle = NULL;

// Настройки
static const char* DEVICE_TYPE     = "upnp:rootdevice";
//static const char* DEVICE_TYPE = "urn:schemas-upnp-org:device:Basic:1";
static const char* FRIENDLY_NAME   = "ESP32 Zigbee Gateway";
static const char* MANUFACTURER    = "CheldonecCo";
static const char* MODEL_NAME      = "Zigbee NCP Host";
static const char* MODEL_NUMBER    = "1.0";
static const char* SERIAL_NUMBER   = "00000001";
static const char* SERVER_NAME     = "Linux/ESP32 UPnP/1.1 ESP32-Zigbee-Gateway/1.0";
static const char* SCHEMA_URL      = "description.xml";
static const char* PRESENTATION_URL = "/";

static char s_uuid[64] = "uuid:12345678-1234-5678-9abc-000000000000";
static char current_ip[16] = "192.168.4.1";

// Прототипы
static void ssdp_task(void* pvParameters);
static void generate_uuid_from_mac(void);
static void send_notify(bool alive, bool update);
static void handle_search(const char* req, int len, struct sockaddr_in* client_addr);

void init_ssdp_load_boot_id(void) {
    nvs_handle_t nvs;
    if (nvs_open("ssdp", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_get_u32(nvs, "boot_id", &boot_id);
        boot_id++;
        nvs_set_u32(nvs, "boot_id", boot_id);
        nvs_commit(nvs);
        nvs_close(nvs);
    } else {
        boot_id = 1;
    }
    ESP_LOGI(TAG, "Current BOOTID.UPNP.ORG: %"PRIu32, boot_id);
}

static void generate_uuid_from_mac(void) {
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        ESP_LOGI(TAG, "Using STA MAC: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else if (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) == ESP_OK) {
        ESP_LOGI(TAG, "Using AP MAC: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else if (esp_efuse_mac_get_default(mac) == ESP_OK) {
        ESP_LOGI(TAG, "Using EFUSE MAC: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        strcpy(s_uuid, "uuid:11223344-5566-7788-99aa-bbccddeeff00");
        return;
    }
    snprintf(s_uuid, sizeof(s_uuid), "uuid:12345678-1234-5678-9abc-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Generated UUID from MAC: %s", s_uuid);
}

void ssdp_update_ip_from_event(uint32_t newIP) {
    const char* new_ip = inet_ntoa(newIP);
    if (strcmp(current_ip, new_ip) != 0) {
        strlcpy(current_ip, new_ip, sizeof(current_ip));
        ESP_LOGI(TAG, "SSDP: IP updated to %s", current_ip);
    }
}

void update_ip_from_event_extern_call_and_send_notify(uint32_t newIP) {
    const char* new_ip = inet_ntoa(newIP);
    if (strcmp(current_ip, new_ip) != 0) {
        strlcpy(current_ip, new_ip, sizeof(current_ip));
        ESP_LOGI(TAG, "SSDP: IP updated to %s", current_ip);
        send_notify(true, true);
    }
}

static void send_packet(const char* msg, size_t len, bool multicast, struct sockaddr_in* dest_addr) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;

    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(SSDP_PORT)};
    if (multicast) {
        addr.sin_addr.s_addr = inet_addr(SSDP_MCAST_ADDR);
    } else {
        if (dest_addr == NULL) { close(sock); return; }
        addr.sin_addr.s_addr = dest_addr->sin_addr.s_addr;
    }

    if (sendto(sock, msg, len, 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "sendto failed");
    } else {
        ESP_LOGD(TAG, "Sent %s to %s:%d", multicast ? "multicast" : "unicast", inet_ntoa(addr.sin_addr), SSDP_PORT);
    }
    close(sock);
}

static void send_notify(bool alive, bool update) {
    char buffer[512];
    const char* nts = update ? "ssdp:update" : "ssdp:alive";
    const char* action = update ? "update" : "alive";

    int len = snprintf(buffer, sizeof(buffer),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:80/%s\r\n"
        "NT: %s\r\n"
        "USN: %s::%s\r\n"
        "SERVER: Linux/1.0 UPnP/1.1 Basic/1.0\r\n"
        "X-User-Agent: redsonic\r\n"
        "BOOTID.UPNP.ORG: %"PRIu32"\r\n"
        "CONFIGID.UPNP.ORG: 12345\r\n"
        "NTS: %s\r\n"
        "\r\n",
        current_ip, SCHEMA_URL,           // LOCATION
        DEVICE_TYPE,                      // NT
        s_uuid, DEVICE_TYPE,              // USN
        boot_id,                          // BOOTID.UPNP.ORG
        nts);                             // NTS

    if (len > 0 && len < sizeof(buffer)) {
        send_packet(buffer, len, true, NULL);
        ESP_LOGI(TAG, "SSDP: Sent notify (%s)", action);
    } else {
        ESP_LOGE(TAG, "SSDP: Failed to format NOTIFY message");
    }
}

static void send_notify_old(bool alive, bool update) {
    char buffer[512];
    const char* nts = update ? "ssdp:update" : "ssdp:alive";
    const char* action = update ? "update" : "alive";

    int len = snprintf(buffer, sizeof(buffer),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:80/%s\r\n"
        "SERVER: %s\r\n"
        "BOOTID.UPNP.ORG: %"PRIu32"\r\n"
        "CONFIGID.UPNP.ORG: 12345\r\n"
        "NT: %s\r\n"
        "USN: %s::%s\r\n"
        "NTS: %s\r\n"
        "\r\n",
        current_ip, SCHEMA_URL, SERVER_NAME, boot_id, DEVICE_TYPE, s_uuid, DEVICE_TYPE, nts);

    if (len > 0) {
        send_packet(buffer, len, true, NULL);
        ESP_LOGI(TAG, "SSDP: Sent notify (%s)", action);
    }
}

static void handle_search_not_work(const char* req, int len, struct sockaddr_in* client_addr) {
    if (len < 17 || strncmp(req, "M-SEARCH * HTTP/1.1", 17) != 0) return;

    ESP_LOGI(TAG, "SSDP M-SEARCH from %s:%d",
             inet_ntoa(client_addr->sin_addr),
             ntohs(client_addr->sin_port));

    const char* pos = req;
    const char* end = req + len;

    while (pos < end) {
        const char* line_start = pos;
        const char* line_end = line_start;
        while (line_end < end && *line_end != '\r' && *line_end != '\n') line_end++;
        if (line_end == line_start) { pos = line_end + 1; continue; }
        pos = line_end;
        while (pos < end && (*pos == '\r' || *pos == '\n')) pos++;

        const char* colon = NULL;
        for (const char* p = line_start; p < line_end; p++) {
            if (*p == ':') { colon = p; break; }
        }
        if (!colon) continue;

        int name_len = colon - line_start;
        if (name_len == 0) continue;

        char name[16];
        int copy_len = (name_len < 15) ? name_len : 15;
        memcpy(name, line_start, copy_len);
        name[copy_len] = '\0';

        for (int i = copy_len - 1; i >= 0 && (name[i] == ' ' || name[i] == '\t'); i--) {
            name[i] = '\0';
        }
        char* name_start = name;
        while (*name_start == ' ' || *name_start == '\t') name_start++;

        const char* value = colon + 1;
        while (value < line_end && (*value == ' ' || *value == '\t')) value++;
        int value_len = line_end - value;
        if (value_len <= 0) continue;

        if (strcasecmp(name_start, "ST") == 0) {
            char st_copy[64];
            int vlen = (value_len < 63) ? value_len : 63;
            memcpy(st_copy, value, vlen);
            st_copy[vlen] = '\0';

            for (int i = strlen(st_copy) - 1; i >= 0 && strchr(" \r\n\t", st_copy[i]); i--) {
                st_copy[i] = '\0';
            }

            ESP_LOGI(TAG, "Parsed ST: '%s'", st_copy);

            // Обрабатываем только нужные ST
            if (strcasecmp(st_copy, "ssdp:all") == 0 ||
                strcasecmp(st_copy, "upnp:rootdevice") == 0 ||
                strcasecmp(st_copy, "urn:schemas-upnp-org:device:Basic:1") == 0) {

                char resp[512];
                int resp_len = snprintf(resp, sizeof(resp),
                    "HTTP/1.1 200 OK\r\n"
                    "EXT:\r\n"
                    "CACHE-CONTROL: max-age=1800\r\n"
                    "LOCATION: http://%s:80/%s\r\n"
                    "ST: urn:schemas-upnp-org:device:Basic:1\r\n"
                    "USN: %s::urn:schemas-upnp-org:device:Basic:1\r\n"
                    "SERVER: Linux/1.0 UPnP/1.1 Basic/1.0\r\n"
                    "X-User-Agent: redsonic\r\n"
                    "BOOTID.UPNP.ORG: %"PRIu32"\r\n"
                    "CONFIGID.UPNP.ORG: 12345\r\n"
                    "\r\n",
                    current_ip, SCHEMA_URL,
                    s_uuid,
                    boot_id);

                if (resp_len > 0 && resp_len < sizeof(resp)) {
                    send_packet(resp, resp_len, false, client_addr);
                    ESP_LOGI(TAG, "SSDP: Responded to %s with ST: urn:schemas-upnp-org:device:Basic:1", inet_ntoa(client_addr->sin_addr));
                } else {
                    ESP_LOGE(TAG, "SSDP: Failed to format response");
                }
            }
            return;
        }
    }
    ESP_LOGW(TAG, "No ST header found");
}

static void handle_search(const char* req, int len, struct sockaddr_in* client_addr) {
    if (len < 17 || strncmp(req, "M-SEARCH * HTTP/1.1", 17) != 0) return;

    ESP_LOGI(TAG, "SSDP M-SEARCH from %s:%d",
             inet_ntoa(client_addr->sin_addr),
             ntohs(client_addr->sin_port));

    const char* pos = req;
    const char* end = req + len;

    while (pos < end) {
        const char* line_start = pos;
        const char* line_end = line_start;
        while (line_end < end && *line_end != '\r' && *line_end != '\n') line_end++;
        if (line_end == line_start) { pos = line_end + 1; continue; }
        pos = line_end;
        while (pos < end && (*pos == '\r' || *pos == '\n')) pos++;

        const char* colon = NULL;
        for (const char* p = line_start; p < line_end; p++) {
            if (*p == ':') { colon = p; break; }
        }
        if (!colon) continue;

        int name_len = colon - line_start;
        if (name_len == 0) continue;

        char name[16];
        int copy_len = (name_len < 15) ? name_len : 15;
        memcpy(name, line_start, copy_len);
        name[copy_len] = '\0';

        for (int i = copy_len - 1; i >= 0 && (name[i] == ' ' || name[i] == '\t'); i--) {
            name[i] = '\0';
        }
        char* name_start = name;
        while (*name_start == ' ' || *name_start == '\t') name_start++;

        const char* value = colon + 1;
        while (value < line_end && (*value == ' ' || *value == '\t')) value++;
        int value_len = line_end - value;
        if (value_len <= 0) continue;

        if (strcasecmp(name_start, "ST") == 0) {
            char st_copy[64];
            int vlen = (value_len < 63) ? value_len : 63;
            memcpy(st_copy, value, vlen);
            st_copy[vlen] = '\0';

            for (int i = strlen(st_copy) - 1; i >= 0 && strchr(" \r\n\t", st_copy[i]); i--) {
                st_copy[i] = '\0';
            }

            ESP_LOGI(TAG, "Parsed ST: '%s'", st_copy);

            if (strcasecmp(st_copy, "ssdp:all") == 0 ||
                strcasecmp(st_copy, "upnp:rootdevice") == 0 ||
                strcasecmp(st_copy, DEVICE_TYPE) == 0) {

                char resp[512];
                int resp_len = snprintf(resp, sizeof(resp),
                    "HTTP/1.1 200 OK\r\n"
                    "EXT:\r\n"
                    "CACHE-CONTROL: max-age=1800\r\n"
                    "LOCATION: http://%s:80/%s\r\n"
                    "SERVER: %s\r\n"
                    "BOOTID.UPNP.ORG: %"PRIu32"\r\n"
                    "CONFIGID.UPNP.ORG: 12345\r\n"
                    "ST: %s\r\n"
                    "USN: %s::%s\r\n"
                    "\r\n",
                    current_ip, SCHEMA_URL, SERVER_NAME, boot_id, st_copy, s_uuid, DEVICE_TYPE);

                if (resp_len > 0) {
                    send_packet(resp, resp_len, false, client_addr);
                    ESP_LOGI(TAG, "SSDP: Responded to %s with ST: %s", inet_ntoa(client_addr->sin_addr), st_copy);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    send_notify(true, false);
                }
            }
            return;
        }
    }
    ESP_LOGW(TAG, "No ST header found");
}

static void ssdp_task(void* pvParameters) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { vTaskDelete(NULL); return; }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SSDP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock); vTaskDelete(NULL); return;
    }

    struct ip_mreq mreq = {
        .imr_multiaddr.s_addr = inet_addr(SSDP_MCAST_ADDR),
        .imr_interface.s_addr = htonl(INADDR_ANY),
    };
    setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    ESP_LOGI(TAG, "SSDP server started on %s:%d", SSDP_MCAST_ADDR, SSDP_PORT);
    send_notify(true, true);
    vTaskDelay(pdMS_TO_TICKS(100));
    send_notify(true, false);

    TickType_t last_notify = xTaskGetTickCount();
    while (s_running) {
        TickType_t now = xTaskGetTickCount();
        if ((now - last_notify) >= SSDP_NOTIFY_INTERVAL) {
            send_notify(true, false);
            last_notify = now;
        }

        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        char buf[512];
        int len = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&client_addr, &addr_len);
        if (len > 0) {
            buf[len] = 0;
            handle_search(buf, len, &client_addr);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    send_notify(false, false);
    close(sock);
    vTaskDelete(NULL);
}

void start_ssdp_server(void) {
    if (s_running) return;
    s_running = true;
    init_ssdp_load_boot_id();
    generate_uuid_from_mac();
    xTaskCreatePinnedToCore(ssdp_task, "ssdp_server", 4096, NULL, 5, &s_task_handle, 0);
    ESP_LOGI(TAG, "SSDP server started");
}

void stop_ssdp_server(void) {
    if (!s_running) return;
    s_running = false;
    if (s_task_handle) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }
    ESP_LOGI(TAG, "SSDP server stopped");
}

esp_err_t description_xml_handler(httpd_req_t *req) {
    const char *desc = 
        "<?xml version=\"1.0\"?>\r\n"
        "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\r\n"
        "  <specVersion><major>1</major><minor>0</minor></specVersion>\r\n"
        "  <device>\r\n"
        "    <deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>\r\n"
        "    <friendlyName>%s</friendlyName>\r\n"
        "    <manufacturer>%s</manufacturer>\r\n"
        "    <manufacturerURL>https://github.com/cheldonec</manufacturerURL>\r\n"
        "    <modelDescription>ESP32 Zigbee NCP Host</modelDescription>\r\n"
        "    <modelName>%s</modelName>\r\n"
        "    <modelNumber>%s</modelNumber>\r\n"
        "    <modelURL>https://github.com/cheldonec</modelURL>\r\n"
        "    <serialNumber>%s</serialNumber>\r\n"
        "    <UDN>%s</UDN>\r\n"
        "    <presentationURL>http://%s</presentationURL>\r\n"
        "  </device>\r\n"
        "</root>\r\n";

    char response[1024];
    int len = snprintf(response, sizeof(response), desc,
                       FRIENDLY_NAME, MANUFACTURER, MODEL_NAME,
                       MODEL_NUMBER, SERIAL_NUMBER, s_uuid, current_ip);

    httpd_resp_set_type(req, "text/xml");
    httpd_resp_set_hdr(req, "Server", SERVER_NAME);
    httpd_resp_send(req, response, len);
    return ESP_OK;
}

