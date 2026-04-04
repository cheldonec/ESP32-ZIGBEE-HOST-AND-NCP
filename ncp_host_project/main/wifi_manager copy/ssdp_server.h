/*
  ssdp_server.h - SSDP Server Header
*/

#ifndef SSDP_SERVER_H
#define SSDP_SERVER_H

#include "esp_http_server.h"

/**
 * @brief Запускает SSDP сервер
 */
void start_ssdp_server(void);

/**
 * @brief Останавливает SSDP сервер
 */
void stop_ssdp_server(void);

/**
 * @brief Обработчик для /description.xml
 */
esp_err_t description_xml_handler(httpd_req_t *req);

/**
 * @brief Обновляет IP-адрес в SSDP при получении IP
 */
void ssdp_update_ip_from_event(uint32_t newIP);

#endif // SSDP_SERVER_H