#include "wifi_manager.h"
#include "dns_hijack.h"
#include "ssdp_server.h"
//#include "ssdp.h"
#include "esp_log.h"
#include "mdns.h"
#include "web_server.h"
#include "ha_integration.h"

static const char *TAG = "wifi_manager";
static const char *TAG_AP = "WiFi SoftAP";
static const char *TAG_STA = "WiFi Sta";

TaskHandle_t WifiTaskH = NULL;
bool s_is_in_ap_only_mode = true;
/*DHCP server option*/
#define DHCPS_OFFER_DNS             0x02

/* STA Configuration */
#define EXAMPLE_ESP_WIFI_STA_SSID           CONFIG_ESP_WIFI_REMOTE_AP_SSID
#define EXAMPLE_ESP_WIFI_STA_PASSWD         CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY           CONFIG_ESP_MAXIMUM_STA_RETRY

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WAPI_PSK
#endif

/* AP Configuration */
#define EXAMPLE_ESP_WIFI_AP_SSID            CONFIG_ESP_WIFI_AP_SSID
#define EXAMPLE_ESP_WIFI_AP_PASSWD          CONFIG_ESP_WIFI_AP_PASSWORD
//#define EXAMPLE_ESP_WIFI_CHANNEL            CONFIG_ESP_WIFI_AP_CHANNEL
#define EXAMPLE_ESP_WIFI_CHANNEL            6
#define EXAMPLE_MAX_STA_CONN                CONFIG_ESP_MAX_STA_CONN_AP

/* MDNS*/
#define MDNS_INSTANCE_NAME "Zigbee Gateway"
#define MDNS_HOSTNAME                      CONFIG_MDNS_HOST_NAME
char s_last_ssid[32] = {0};
char s_last_password[64] = {0};

esp_netif_t *esp_netif_ap = NULL;
esp_netif_t *esp_netif_sta = NULL;

static bool STA_connect_enable = false;
/* FreeRTOS event group to signal when we are connected/disconnected */
static EventGroupHandle_t s_wifi_event_group;

/************** MDNS  **************/
static bool mdns_is_started = false;
static void start_mdns_service(esp_netif_t *actual_netif)
{
    if (!actual_netif) {
        ESP_LOGI(TAG, "No netif available");
        return;
    }

    // Останавливаем, если уже запущен
    if (mdns_is_started == true) {
        mdns_free();
        mdns_is_started = false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Инициализируем mDNS
    if (mdns_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init mDNS");
        return;
    }

    // Устанавливаем hostname и instance name
    mdns_hostname_set(CONFIG_MDNS_HOST_NAME);
    mdns_instance_name_set("ESP32 Zigbee Gateway");

    mdns_service_add("web", "_http", "_tcp", 80, NULL, 0);
    mdns_service_add("ws", "_ws", "_tcp", 81, NULL, 0);
    // 1. Добавляем HTTP сервис
    const mdns_txt_item_t http_txt_data[] = {
        {"path", "/"}
    };
    mdns_service_add("ESP32 Zigbee Gateway", "_http", "_tcp", 80, http_txt_data, 1);

    // 2. Добавляем _home-assistant._tcp — КЛЮЧ к автообнаружению в HA
    char config_url[96];
    snprintf(config_url, sizeof(config_url), "http://%s.local/homeassistant.json", CONFIG_MDNS_HOST_NAME);

    mdns_txt_item_t ha_txt_data[] = {
        {"version", "1.0"},
        {"features", "configuration_url"},
        {"config", NULL}
    };
    ha_txt_data[2].value = config_url;

    mdns_service_add("ESP32 Zigbee Gateway", "_home-assistant", "_tcp", 80, ha_txt_data, 3);

    mdns_is_started = true;

    ESP_LOGI(TAG, "mDNS service started: http://%s.local", CONFIG_MDNS_HOST_NAME);
}


static void stop_mdns_service(void)
{
    mdns_free();  // Освобождает все ресурсы mDNS
    mdns_is_started = false;
    ESP_LOGI(TAG, "mDNS service stopped");
    
   return;
}

esp_err_t load_wifi_config_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    size_t ssid_len = 32, pass_len = 64;
    err = nvs_get_str(handle, "wifi_ssid", s_last_ssid, &ssid_len);
    if (err == ESP_OK) {
        err = nvs_get_str(handle, "wifi_pass", s_last_password, &pass_len);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Loaded Wi-Fi config: SSID='%s'", s_last_ssid);
            nvs_close(handle);
            return ESP_OK;
        }
    }
    nvs_close(handle);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t save_wifi_config_to_nvs(const char *ssid, const char *password)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(handle, "wifi_ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(handle, "wifi_pass", password);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_netif_t *wifi_init_softap(void)
{
    esp_netif_ap = esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_AP_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_AP_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_AP_PASSWD,
            //.password = "",
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            //.authmode = WIFI_AUTH_OPEN,
            .pmf_cfg = {
                .required = true,
                //.capable = false,
            },
        },
    };

    if (strlen(EXAMPLE_ESP_WIFI_AP_PASSWD) == 0) {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));

    ESP_LOGI(TAG_AP, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_AP_SSID, EXAMPLE_ESP_WIFI_AP_PASSWD, EXAMPLE_ESP_WIFI_CHANNEL);

    return esp_netif_ap;
}

/* Initialize wifi station from config*/
esp_netif_t *wifi_init_sta_from_config(bool connect_enable)
{
    // если уже создан, то ставим флаг на переподключение
    bool need_reconnect = false;
    if (esp_netif_sta != NULL)
    {
        need_reconnect = true;
        STA_connect_enable = false;
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(500)); // Wait 
    }
    if (esp_netif_sta == NULL)
    {
        esp_netif_sta = esp_netif_create_default_wifi_sta();
    }

    if(connect_enable == true)
    {
        wifi_config_t wifi_sta_config = {
            .sta = {
                .ssid = EXAMPLE_ESP_WIFI_STA_SSID,
                .password = EXAMPLE_ESP_WIFI_STA_PASSWD,
                .scan_method = WIFI_ALL_CHANNEL_SCAN,
                //.failure_retry_cnt = EXAMPLE_ESP_MAXIMUM_RETRY,
                /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
                * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
                * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
                * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
                */
                .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
                .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            },
        };
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config) );
        memcpy (s_last_ssid, wifi_sta_config.sta.ssid, 32);
        ESP_LOGI(TAG_STA, "wifi_init_sta from config finished. SSID %s", s_last_ssid);
    }else ESP_LOGI(TAG_STA, "wifi_init_sta from config finished. Only Scan Enable");

    if ((need_reconnect == true) && (connect_enable == true)) 
    {
        ESP_LOGI(TAG_STA, "wifi_init_sta REINIT from config Compliete and START Reconnect with new config");
        esp_wifi_connect();
    }
    STA_connect_enable = connect_enable;
    return esp_netif_sta;
}
/* Initialize wifi station */
esp_netif_t *wifi_init_sta(bool connect_enable)
{
    // если уже создан, то ставим флаг на переподключение
    bool need_reconnect = false;
    if (esp_netif_sta != NULL)
    {
        need_reconnect = true;
        STA_connect_enable = false; // отключаем иначе при disconnected будет переподключение
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(500)); // Wait 
    }

    // если первый запуск, то создаем
    if (esp_netif_sta == NULL)
    {
        esp_netif_sta = esp_netif_create_default_wifi_sta();
    }

    if(connect_enable == true)
    {
        wifi_config_t wifi_sta_config = {
        .sta = {
            //.ssid = EXAMPLE_ESP_WIFI_STA_SSID,
            //.password = EXAMPLE_ESP_WIFI_STA_PASSWD,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            //.failure_retry_cnt = EXAMPLE_ESP_MAXIMUM_RETRY,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
            * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    strncpy((char*)wifi_sta_config.sta.ssid, s_last_ssid, 32);
    if (strlen(s_last_password) > 0) {
        strncpy((char*)wifi_sta_config.sta.password, s_last_password, 64);
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config) );

    ESP_LOGI(TAG_STA, "wifi_init_sta finished.");
    }else ESP_LOGI(TAG_STA, "wifi_init_sta finished. Only Scan Enable");

    if ((need_reconnect == true) && (connect_enable == true)) 
    {
        ESP_LOGI(TAG_STA, "wifi_init_sta REINIT Compliete and START Reconnect with new config");
        esp_wifi_connect();
    }
    STA_connect_enable = connect_enable;
    return esp_netif_sta;
}

void softap_set_dns_addr(esp_netif_t *esp_netif_ap,esp_netif_t *esp_netif_sta)
{
    esp_netif_dns_info_t dns;
    esp_netif_get_dns_info(esp_netif_sta,ESP_NETIF_DNS_MAIN,&dns);
    uint8_t dhcps_offer_option = DHCPS_OFFER_DNS;
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(esp_netif_ap));
    ESP_ERROR_CHECK(esp_netif_dhcps_option(esp_netif_ap, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_offer_option, sizeof(dhcps_offer_option)));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(esp_netif_ap, ESP_NETIF_DNS_MAIN, &dns));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(esp_netif_ap));
}


static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station "MACSTR" left, AID=%d, reason:%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (STA_connect_enable == true)
        {
            esp_wifi_connect();
            ESP_LOGI(TAG_STA, "Station started");
        }else {
            ESP_LOGI(TAG_STA, "SSID для подключения не определена, START отменён, доступно только сканирование");
            start_mdns_service(esp_netif_sta);
            vTaskDelay(pdMS_TO_TICKS(100)); // Wait before retrying
            //start_webserver();
            //wifi_scan();
        }
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED Stopping Serverы");
         stop_mdns_service();
        stop_webserver();       // ✅ Останавливаем
        stop_ssdp_server();
        s_is_in_ap_only_mode = true;
        esp_netif_set_default_netif(esp_netif_ap);
        //ESP_LOGI(TAG, "Stopping SSDP Server"); // если был уже запущен, не страшно
        //stop_ssdp_server();
        if (STA_connect_enable == true)
        {
            esp_wifi_connect();
            ESP_LOGI(TAG_STA, "Disconnected. Connecting to the AP again...");
            vTaskDelay(pdMS_TO_TICKS(5000)); // Wait before retrying
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG_STA, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_is_in_ap_only_mode = false;
        
        //s_retry_num = 0;
        //xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        softap_set_dns_addr(esp_netif_ap,esp_netif_sta);
        esp_netif_set_default_netif(esp_netif_sta);
        start_mdns_service(esp_netif_sta);
        vTaskDelay(pdMS_TO_TICKS(200)); // Wait before retrying
        // обновляем IP адреса
        ssdp_update_ip_from_event(event->ip_info.ip.addr);
        ha_integration_update_ip_from_event(event->ip_info.ip.addr);

        ESP_LOGI(TAG, "Starting SSDP Server"); // если был уже запущен, не страшно
        stop_ssdp_server();                    // ← Обязательно!
        vTaskDelay(pdMS_TO_TICKS(10));
        start_ssdp_server();
        vTaskDelay(pdMS_TO_TICKS(100)); // Wait before retrying
        //update_ip_from_event_extern_call_and_send_notify(event->ip_info.ip.addr);
        ha_integration_init();

        // Получаем время из интернета
        sntp_initialize();
        
        //start_webserver();
        
    }
    
}

esp_err_t wifi_manager_init()
{
    ESP_ERROR_CHECK(esp_netif_init());

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Register Event handler */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID,
                    &wifi_event_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP,
                    &wifi_event_handler,
                    NULL,
                    NULL));

    /*Initialize WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    /* Initialize AP */
    ESP_LOGI(TAG_AP, "ESP_WIFI_MODE_AP");
    esp_netif_ap = wifi_init_softap();
    STA_connect_enable = false;
    esp_netif_set_default_netif(esp_netif_ap);
    // Загружаем SSID/пароль
    if (load_wifi_config_from_nvs() == ESP_OK && strlen(s_last_ssid) > 0) {
        ESP_LOGI(TAG, "Credentials found in NVS. Starting STA...");
        // Запускаем STA
        /* Initialize STA */
        ESP_LOGI(TAG_STA, "ESP_WIFI_MODE_STA");
        STA_connect_enable = true;
        esp_netif_sta = wifi_init_sta(STA_connect_enable);
    } else
    {
        /*char* config_ssid = EXAMPLE_ESP_WIFI_STA_SSID;
        char* config_password = EXAMPLE_ESP_WIFI_STA_PASSWD;
        if (strlen(config_ssid) > 0) {
            ESP_LOGI(TAG, "Credentials found in config. Starting STA...");
            // временно false!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            //STA_connect_enable = true;
            STA_connect_enable = false;
            //wifi_init_sta_from_config(STA_connect_enable);
        }else */
        {
            esp_netif_set_default_netif(esp_netif_ap);
            s_is_in_ap_only_mode = true;
            ESP_LOGI(TAG_STA, "ESP_WIFI_MODE_STA WITHOUT CONNECT, ONLY SCAN Enable");
            STA_connect_enable = false;
            //esp_netif_sta = wifi_init_sta(STA_connect_enable);
        }
    }
    
    /* Start WiFi */
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "Wi-Fi Manager starting...");
    /* Enable napt on the AP netif */
    if (esp_netif_napt_enable(esp_netif_ap) != ESP_OK) {
        ESP_LOGE(TAG_STA, "NAPT not enabled on the netif: %p", esp_netif_ap);
    }

    ESP_LOGI(TAG, "Starting webserver...");
    start_webserver();              // Start the webserver
    ESP_LOGI(TAG, "Webserver started successfully.");

    ESP_LOGI(TAG, "Starting DNS Server");
    init_dns_hijack();


    //ESP_LOGI(TAG, "Starting SSDP Server"); // если был уже запущен, не страшно
    //init _ssdp_server();
    //init_ssdp_load_boot_id();

    ESP_LOGI(TAG_STA, "Test scan START");
    //wifi_scan();

    return ESP_OK;
}

esp_err_t wifi_manager_reinit_sta(void)
{
    if (load_wifi_config_from_nvs() == ESP_OK && strlen(s_last_ssid) > 0) {
        ESP_LOGI(TAG, "Credentials found in NVS. Starting STA...");
        // Запускаем STA
        /* Initialize STA */
        ESP_LOGI(TAG_STA, "ESP_WIFI_MODE_STA");
        STA_connect_enable = true;
        esp_netif_sta = wifi_init_sta(STA_connect_enable);
    } else
    {
        char* config_ssid = EXAMPLE_ESP_WIFI_STA_SSID;
        char* config_password = EXAMPLE_ESP_WIFI_STA_PASSWD;
        if (strlen(config_ssid) > 0) {
            ESP_LOGI(TAG, "Credentials found in config. Starting STA...");
            STA_connect_enable = true;
            wifi_init_sta_from_config(STA_connect_enable);
        }else 
        {
            //esp_netif_set_default_netif(esp_netif_ap);
            ESP_LOGI(TAG_STA, "ESP_WIFI_MODE_STA WITHOUT CONNECT, ONLY SCAN Enable");
            STA_connect_enable = false;
            esp_netif_sta = wifi_init_sta(STA_connect_enable);
        }
    }
    return ESP_OK;
}

void wifi_manager_scan_network(void)
{
    bool need_reconnect = STA_connect_enable;
    if (esp_netif_sta != NULL)
    {
        STA_connect_enable = false; // отключаем иначе при disconnected будет переподключение
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(500)); // Wait 
    }
    wifi_scan();
    wifi_manager_reinit_sta();
}

esp_err_t wifi_manager_deinit_sta_for_setup(void)
{
    if (esp_netif_sta != NULL)
    {
        STA_connect_enable = false; // отключаем иначе при disconnected будет переподключение
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(100)); // Wait 
    }
    return ESP_OK;
}

/**
 * @brief Переключает устройство в режим только AP (для настройки Wi-Fi)
 */
// запуск из wifi_manager_switch_to_ap_mode_task
void wifi_manager_switch_to_ap_mode(void)
{
    ESP_LOGI(TAG, "Switching to AP-only mode...");

    // 1. Останавливаем подключение к роутеру
    if (esp_netif_sta != NULL) {
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_wifi_set_mode(WIFI_MODE_AP);
        ESP_LOGI(TAG, "Wi-Fi mode switched to AP only");
    }

    // 2. Останавливаем сервисы, зависящие от подключения к роутеру
    stop_mdns_service();
    stop_webserver();
    stop_ssdp_server();
    //ha_integration_deinit(); // если есть
    //sntp_stop();             // останавливаем NTP

    // 3. Меняем default netif на AP
    esp_netif_set_default_netif(esp_netif_ap);
    s_is_in_ap_only_mode = true;

    // 4. Запускаем веб-сервер и DNS-перехват
    start_webserver();
    init_dns_hijack(); // чтобы все запросы вели на captive portal

    ESP_LOGI(TAG, "Device is now in AP-only mode. Ready for Wi-Fi setup.");
}

void wifi_manager_switch_to_ap_mode_task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(100)); // дадим завершиться ISR
    wifi_manager_switch_to_ap_mode();
    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelete(NULL); // завершаем задачу
}

void wifi_manager_switch_to_ap_mode_safe(void)
{
    xTaskCreate(wifi_manager_switch_to_ap_mode_task, "wifi_ap_mode_task", 4096, NULL, 10, NULL);
}