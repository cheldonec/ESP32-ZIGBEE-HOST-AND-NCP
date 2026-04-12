// File: main/zbm_dev_base/zbm_dev_simple_func.c
// Основные функции для динамического управления Zigbee устройствами:
// - Создание/обновление атрибутов, кластеров, команд
// - Автоматическая регистрация в GUID DB
// - Поддержка стандартных и кастомных кластеров

#include "zbm_dev_simple_func.h"
#include "zbm_core_sync.h"
#include "zbm_attr_types.h"
#include <string.h>
#include <stdbool.h>
#include <stddef.h>  // Для offsetof
#include <stdlib.h>
#include "esp_log.h"
#include "zbm_low_level_types.h"
#include "zbm_spiffs_helper.h"
#include "zbm_dev_storage_spiffs.h"
#include "zbm_web_server.h"

static const char* TAG = "zbm_dev_simple_func";

// Используем esp_log_timestamp() из ESP-IDF
#ifndef get_ms
#define get_ms() ((uint64_t)esp_log_timestamp())
#endif

// === Очистка ресурсов ===


// === Обновление атрибута ===
uint8_t zbm_device_apply_reported_value(
    zbm_dev_t* dev_obj,
    uint8_t endpoint_id,
    uint16_t cluster_id,
    zbm_cluster_role_t role_mask,
    uint16_t attr_id,
    const char* attr_friendlyname,
    uint8_t acces,
    zbm_attr_data_types_t data_type,
    uint16_t data_size,
    const void* new_value)
{
    if (!dev_obj || !new_value) return 0xFF;
    if (!zbm_is_valid_data_size(data_type, data_size)) return 0xFF;

    dev_obj->last_seen_ms = get_ms();
    dev_obj->is_online = true;
    bool created = false;
    // Поиск или создание эндпоинта
    zbm_dev_endpoint_t* endpoint = NULL;
    for (uint8_t i = 0; i < dev_obj->endpoints_count; i++) {
        if (dev_obj->endpoints_array[i] && dev_obj->endpoints_array[i]->id == endpoint_id) {
            endpoint = dev_obj->endpoints_array[i];
            break;
        }
    }

    if (!endpoint) {
        endpoint = calloc(1, sizeof(zbm_dev_endpoint_t));
        if (!endpoint) return 0xFF;
        endpoint->id = endpoint_id;
        endpoint->is_use_on_device = 1;

        zbm_dev_endpoint_t** new_array = realloc(dev_obj->endpoints_array,
            (dev_obj->endpoints_count + 1) * sizeof(zbm_dev_endpoint_t*));
        if (!new_array) {
            free(endpoint);
            return 0xFF;
        }
        dev_obj->endpoints_array = new_array;
        dev_obj->endpoints_array[dev_obj->endpoints_count++] = endpoint;
        created = true;
    }

    // Поиск кластера
    zbm_standart_cluster_t* std_cluster = NULL;
    for (int i = 0; i < endpoint->standart_cluster_count; i++) {
        zbm_standart_cluster_t* c = endpoint->standart_cluster_array[i];
        if (c && c->id == cluster_id && (c->role_mask & role_mask)) {
            std_cluster = c;
            break;
        }
    }

    zbm_custom_cluster_t* custom_cluster = NULL;
    if (!std_cluster) {
        for (int i = 0; i < endpoint->custom_cluster_count; i++) {
            zbm_custom_cluster_t* c = endpoint->custom_cluster_array[i];
            if (c && c->id == cluster_id && (c->role_mask & role_mask)) {
                custom_cluster = c;
                break;
            }
        }
    }

    // Создание нового кластера
    if (!std_cluster && !custom_cluster) {
        bool known_standard = zbm_is_standard_cluster_id(cluster_id);

        if (known_standard) {
            std_cluster = calloc(1, sizeof(zbm_standart_cluster_t));
            if (!std_cluster) return 0xFF;

            std_cluster->id = cluster_id;
            std_cluster->role_mask = role_mask;
            std_cluster->friendlyname = generate_cluster_name(cluster_id, false);
            if (!std_cluster->friendlyname) {
                free(std_cluster);
                return 0xFF;
            }

            // Атрибуты
            uint8_t attr_count = 0;
            zbm_cluster_attribute_t** attr_array = zbm_create_standard_attribute_array(cluster_id, role_mask, &attr_count);
            std_cluster->attr_array = attr_array;
            std_cluster->attr_count = attr_count;

            // Команды (SERVER)
            uint8_t cmd_count = 0;
            zbm_cluster_standart_cmd_t** cmd_array = NULL;
            if (role_mask & ZBM_CLUSTER_ROLE_SERVER) {
                cmd_array = zbm_create_standard_command_array(cluster_id, role_mask, &cmd_count);
                std_cluster->standart_cmd_array = cmd_array;
                std_cluster->standart_cmd_count = cmd_count;
            }

            // Добавляем кластер
            zbm_standart_cluster_t** new_array = realloc(endpoint->standart_cluster_array,
                (endpoint->standart_cluster_count + 1) * sizeof(zbm_standart_cluster_t*));
            if (!new_array) {
                // Очистка
                if (cmd_array) {
                    for (int i = 0; i < cmd_count; i++) {
                        if (cmd_array[i]) zbm_free_cluster_standart_cmd(cmd_array[i]);
                    }
                    free(cmd_array);
                }
                free(std_cluster->friendlyname);
                free(std_cluster);
                return 0xFF;
            }
            endpoint->standart_cluster_array = new_array;
            endpoint->standart_cluster_array[endpoint->standart_cluster_count++] = std_cluster;

            // Регистрация атрибутов и команд ТОЛЬКО если short_addr известен
            if (dev_obj->short_addr != 0xFFFE) {
                for (int i = 0; i < std_cluster->attr_count; i++) {
                    zbm_cluster_attribute_t* attr = std_cluster->attr_array[i];
                    if (attr) {
                        bool registered = zbm_guid_db_register(
                            &std_cluster->attr_array[i],
                            dev_obj->short_addr,
                            endpoint->id,
                            cluster_id,
                            attr->id,
                            NULL
                        );
                        if (!registered) {
                            ESP_LOGD(TAG, "Duplicate or failed to register attr GUID: %s", attr->guid);
                        }
                    }
                }

                for (int i = 0; i < std_cluster->standart_cmd_count; i++) {
                    zbm_cluster_standart_cmd_t* cmd = std_cluster->standart_cmd_array[i];
                    if (cmd) {
                        bool registered = zbm_guid_db_register_cmd(
                            &std_cluster->standart_cmd_array[i],
                            dev_obj->short_addr,
                            endpoint->id,
                            cluster_id,
                            cmd->id,
                            NULL
                        );
                        if (!registered) {
                            ESP_LOGD(TAG, "Duplicate or failed to register cmd GUID: %s", cmd->guid);
                        }
                    }
                }
            }

            // Обновляем указатели в GUID DB (нужно всегда, даже если short_addr=0xFFFE)
            zbm_guid_db_update_cluster_attr_ptrs(
                dev_obj->short_addr,
                endpoint->id,
                cluster_id,
                std_cluster->attr_array,
                std_cluster->attr_count
            );

            zbm_guid_db_update_cluster_cmd_ptrs(
                dev_obj->short_addr,
                endpoint->id,
                cluster_id,
                std_cluster->standart_cmd_array,
                std_cluster->standart_cmd_count
            );
        } else {
            custom_cluster = calloc(1, sizeof(zbm_custom_cluster_t));
            if (!custom_cluster) return 0xFF;

            custom_cluster->id = cluster_id;
            custom_cluster->role_mask = role_mask;
            custom_cluster->friendlyname = generate_cluster_name(cluster_id, true);
            if (!custom_cluster->friendlyname) {
                free(custom_cluster);
                return 0xFF;
            }

            zbm_custom_cluster_t** new_array = realloc(endpoint->custom_cluster_array,
                (endpoint->custom_cluster_count + 1) * sizeof(zbm_custom_cluster_t*));
            if (!new_array) {
                free(custom_cluster->friendlyname);
                free(custom_cluster);
                return 0xFF;
            }
            endpoint->custom_cluster_array = new_array;
            endpoint->custom_cluster_array[endpoint->custom_cluster_count++] = custom_cluster;
        }
        created = true;
    }

    // Работа с атрибутом
    zbm_cluster_attribute_t** attr_array = NULL;
    uint8_t* attr_count_ptr = NULL;

    if (std_cluster) {
        attr_array = std_cluster->attr_array;
        attr_count_ptr = &std_cluster->attr_count;
    } else {
        attr_array = custom_cluster->attr_array;
        attr_count_ptr = &custom_cluster->attr_count;
    }

    zbm_cluster_attribute_t* attr = NULL;
    for (int i = 0; i < *attr_count_ptr; i++) {
        if (attr_array[i] && attr_array[i]->id == attr_id) {
            attr = attr_array[i];
            break;
        }
    }

    //bool created = false;
    if (!attr) {
        attr = calloc(1, sizeof(zbm_cluster_attribute_t));
        if (!attr) return 0xFF;

        attr->id = attr_id;
        attr->acces = acces;
        attr->data_type = data_type;
        attr->data_size = data_size;
        attr->friendlyname = attr_friendlyname ? strdup(attr_friendlyname) : NULL;
        if (attr_friendlyname && !attr->friendlyname) {
            free(attr);
            return 0xFF;
        }

        attr->p_value = calloc(1, data_size);
        if (!attr->p_value) {
            if (attr->friendlyname) free(attr->friendlyname);
            free(attr);
            return 0xFF;
        }

        zbm_cluster_attribute_t** new_array = realloc(attr_array, (*attr_count_ptr + 1) * sizeof(zbm_cluster_attribute_t*));
        if (!new_array) {
            zbm_free_cluster_attribute(attr);
            return 0xFF;
        }

        if (std_cluster) {
            std_cluster->attr_array = new_array;
            std_cluster->attr_array[*attr_count_ptr] = attr;
        } else {
            custom_cluster->attr_array = new_array;
            custom_cluster->attr_array[*attr_count_ptr] = attr;
        }

        (*attr_count_ptr)++;
        created = true;

        // Регистрация нового атрибута ТОЛЬКО если short_addr известен
        if (dev_obj->short_addr != 0xFFFE) {
            bool registered = zbm_guid_db_register(
                &attr,
                dev_obj->short_addr,
                endpoint->id,
                cluster_id,
                attr_id,
                NULL
            );
            if (!registered) {
                ESP_LOGD(TAG, "Failed to register new attr GUID (duplicate?): %s", attr->guid);
            }
        }

        // Обновляем указатели в GUID DB
        if (std_cluster) {
            zbm_guid_db_update_cluster_attr_ptrs(
                dev_obj->short_addr,
                endpoint->id,
                cluster_id,
                std_cluster->attr_array,
                std_cluster->attr_count
            );
        } else {
            zbm_guid_db_update_custom_cluster_attr_ptrs(
                dev_obj->short_addr,
                endpoint->id,
                cluster_id,
                custom_cluster->attr_array,
                custom_cluster->attr_count
            );
        }
    } else {
        if (attr_friendlyname && (!attr->friendlyname ||
            strcmp(attr->friendlyname, attr_friendlyname) != 0)) {
            free(attr->friendlyname);
            attr->friendlyname = strdup(attr_friendlyname);
            if (!attr->friendlyname) return 0xFF;
        }
        attr->acces = acces;
        attr->data_type = data_type;
    }

    // Обновление значения
    if (attr->data_size != data_size) {
        void* new_p_value = realloc(attr->p_value, data_size);
        if (!new_p_value) return 0xFF;
        attr->p_value = new_p_value;
        attr->data_size = data_size;
        memset(attr->p_value, 0, data_size);
    }

    memcpy(attr->p_value, new_value, data_size);
    attr->last_update_ms = get_ms();

    //отправляем в web socet
    zbm_ws_send_data_update_notify(attr->guid, attr->data_type, new_value, data_size);
    return created ? 1 : 0;
}

// === Обновление кастомного репорта (TUYA 0xFD и др.) ===
uint8_t zbm_update_cluster_custom_report(
    zbm_dev_t* dev_obj,
    uint8_t endpoint_id,
    uint16_t cluster_id,
    zbm_cluster_role_t role_mask,
    uint8_t cmd_id,
    const char* cmd_friendlyname,
    zbm_cmd_data_types_t data_type,
    uint16_t data_size,
    const void* new_value)
{
    if (!dev_obj || !new_value || data_size == 0 || data_size > 256) return 0xFF;

    // Поиск/создание эндпоинта
    zbm_dev_endpoint_t* endpoint = NULL;
    for (uint8_t i = 0; i < dev_obj->endpoints_count; i++) {
        if (dev_obj->endpoints_array[i] && dev_obj->endpoints_array[i]->id == endpoint_id) {
            endpoint = dev_obj->endpoints_array[i];
            break;
        }
    }

    if (!endpoint) {
        endpoint = calloc(1, sizeof(zbm_dev_endpoint_t));
        if (!endpoint) return 0xFF;
        endpoint->id = endpoint_id;
        endpoint->is_use_on_device = 1;

        zbm_dev_endpoint_t** new_array = realloc(dev_obj->endpoints_array,
            (dev_obj->endpoints_count + 1) * sizeof(zbm_dev_endpoint_t*));
        if (!new_array) {
            free(endpoint);
            return 0xFF;
        }
        dev_obj->endpoints_array = new_array;
        dev_obj->endpoints_array[dev_obj->endpoints_count++] = endpoint;
    }

    // Поиск кластера
    zbm_standart_cluster_t* std_cluster = NULL;
    zbm_custom_cluster_t* custom_cluster = NULL;
    bool is_standard = zbm_is_standard_cluster_id(cluster_id);

    if (is_standard) {
        for (int i = 0; i < endpoint->standart_cluster_count; i++) {
            zbm_standart_cluster_t* c = endpoint->standart_cluster_array[i];
            if (c && c->id == cluster_id && (c->role_mask & role_mask)) {
                std_cluster = c;
                break;
            }
        }
    } else {
        for (int i = 0; i < endpoint->custom_cluster_count; i++) {
            zbm_custom_cluster_t* c = endpoint->custom_cluster_array[i];
            if (c && c->id == cluster_id && (c->role_mask & role_mask)) {
                custom_cluster = c;
                break;
            }
        }
    }

    // Создание кластера
    if (!std_cluster && !custom_cluster) {
        if (is_standard) {
            std_cluster = calloc(1, sizeof(zbm_standart_cluster_t));
            if (!std_cluster) return 0xFF;
            std_cluster->id = cluster_id;
            std_cluster->role_mask = role_mask;
            std_cluster->friendlyname = generate_cluster_name(cluster_id, false);
            if (!std_cluster->friendlyname) {
                free(std_cluster);
                return 0xFF;
            }
            std_cluster->standart_cmd_array = NULL;
            std_cluster->standart_cmd_count = 0;
            std_cluster->attr_array = NULL;
            std_cluster->attr_count = 0;
            std_cluster->custom_report_cmd_array = NULL;
            std_cluster->custom_report_cmd_count = 0;

            zbm_standart_cluster_t** new_array = realloc(endpoint->standart_cluster_array,
                (endpoint->standart_cluster_count + 1) * sizeof(zbm_standart_cluster_t*));
            if (!new_array) {
                free(std_cluster->friendlyname);
                free(std_cluster);
                return 0xFF;
            }
            endpoint->standart_cluster_array = new_array;
            endpoint->standart_cluster_array[endpoint->standart_cluster_count++] = std_cluster;
        } else {
            custom_cluster = calloc(1, sizeof(zbm_custom_cluster_t));
            if (!custom_cluster) return 0xFF;
            custom_cluster->id = cluster_id;
            custom_cluster->role_mask = role_mask;
            custom_cluster->friendlyname = generate_cluster_name(cluster_id, true);
            if (!custom_cluster->friendlyname) {
                free(custom_cluster);
                return 0xFF;
            }
            custom_cluster->attr_array = NULL;
            custom_cluster->attr_count = 0;
            custom_cluster->standart_cmd_array = NULL;
            custom_cluster->standart_cmd_count = 0;
            custom_cluster->custom_report_cmd_array = NULL;
            custom_cluster->custom_report_cmd_count = 0;

            zbm_custom_cluster_t** new_array = realloc(endpoint->custom_cluster_array,
                (endpoint->custom_cluster_count + 1) * sizeof(zbm_custom_cluster_t*));
            if (!new_array) {
                free(custom_cluster->friendlyname);
                free(custom_cluster);
                return 0xFF;
            }
            endpoint->custom_cluster_array = new_array;
            endpoint->custom_cluster_array[endpoint->custom_cluster_count++] = custom_cluster;
        }
    }

    // Массив репортов
    zbm_cluster_custom_report_cmd_t** report_array = NULL;
    uint8_t* report_count = NULL;

    if (std_cluster) {
        report_array = std_cluster->custom_report_cmd_array;
        report_count = &std_cluster->custom_report_cmd_count;
    } else {
        report_array = custom_cluster->custom_report_cmd_array;
        report_count = &custom_cluster->custom_report_cmd_count;
    }

    // Поиск репорта
    zbm_cluster_custom_report_cmd_t* report_cmd = NULL;
    for (int i = 0; i < *report_count; i++) {
        if (report_array[i] && report_array[i]->id == cmd_id) {
            report_cmd = report_array[i];
            break;
        }
    }

    bool created = false;
    if (!report_cmd) {
        report_cmd = calloc(1, sizeof(zbm_cluster_custom_report_cmd_t));
        if (!report_cmd) return 0xFF;

        report_cmd->id = cmd_id;
        report_cmd->data_type = data_type;
        report_cmd->data_size = data_size;

        if (cmd_friendlyname) {
            report_cmd->friendlyname = strdup(cmd_friendlyname);
        } else {
            char buf[64];
            const char* name = zbm_get_cluster_friendlyname(cluster_id);
            snprintf(buf, sizeof(buf), "%s_Report_0x%02X", name ? name : "Cluster", cmd_id);
            report_cmd->friendlyname = strdup(buf);
        }
        if (!report_cmd->friendlyname) {
            free(report_cmd);
            return 0xFF;
        }
                report_cmd->p_value = calloc(1, data_size);
        if (!report_cmd->p_value) {
            zbm_free_cluster_custom_report_cmd(report_cmd);
            return 0xFF;
        }

        zbm_cluster_custom_report_cmd_t** new_array = realloc(report_array,
            (*report_count + 1) * sizeof(zbm_cluster_custom_report_cmd_t*));
        if (!new_array) {
            zbm_free_cluster_custom_report_cmd(report_cmd);
            return 0xFF;
        }

        // Присваиваем новый массив ДО регистрации
        if (std_cluster) {
            std_cluster->custom_report_cmd_array = new_array;
            std_cluster->custom_report_cmd_array[*report_count] = report_cmd;
        } else {
            custom_cluster->custom_report_cmd_array = new_array;
            custom_cluster->custom_report_cmd_array[*report_count] = report_cmd;
        }

        (*report_count)++;
        created = true;

        // ✅ Регистрируем через правильную функцию — ПОСЛЕ присвоения
        zbm_cluster_custom_report_cmd_t** target_array = 
            std_cluster ? std_cluster->custom_report_cmd_array : custom_cluster->custom_report_cmd_array;

        bool registered = zbm_guid_db_register_custom_report(
            &target_array[*report_count - 1],
            dev_obj->short_addr,
            endpoint->id,
            cluster_id,
            report_cmd->id,
            NULL
        );
        if (!registered) {
            ESP_LOGW(TAG, "Failed to register custom report GUID (duplicate?): %s", report_cmd->friendlyname);
        }

        // Обновляем указатели в GUID DB
        zbm_guid_db_update_custom_report_ptrs(
            dev_obj->short_addr,
            endpoint->id,
            cluster_id,
            target_array,
            *report_count
        );
    }

    // === Обновление значения ===
    if (report_cmd->data_size != data_size) {
        void* new_p_value = realloc(report_cmd->p_value, data_size);
        if (!new_p_value) return 0xFF;
        report_cmd->p_value = new_p_value;
        report_cmd->data_size = data_size;
        memset(report_cmd->p_value, 0, data_size);
    }

    memcpy(report_cmd->p_value, new_value, data_size);
    //отправляем в web socet
    zbm_ws_send_data_update_notify(report_cmd->guid, report_cmd->data_type, new_value, data_size);
    return created ? 1 : 0;
}

// === Обработка ответа Active Endpoint ===
// result = 0 (update), result = 1 (update with create), result = 0xff (update error)
// Вызывающий отвечает за сохранение и уведомления
uint8_t zbm_process_active_endpoint_response(
    zbm_dev_t* dev,
    uint8_t zdo_status,
    uint8_t ep_count,
    uint8_t* ep_list)
{
    if (!dev || !ep_list) {
        ESP_LOGW(TAG, "Invalid args: dev=%p, ep_list=%p", dev, ep_list);
        return 0xFF;
    }

    if (zdo_status != 0x00) {
        ESP_LOGW(TAG, "ZDO status error: 0x%02x", zdo_status);
        return 0xFF;
    }

    dev->last_seen_ms = get_ms();
    dev->is_online = true;

    bool created_any = false;
    bool updated_any = false; // всегда true, если есть эндпоинты

    for (int i = 0; i < ep_count; i++) {
        uint8_t endpoint_id = ep_list[i];

        // Проверим, есть ли такой эндпоинт
        zbm_dev_endpoint_t* ep = NULL;
        for (int j = 0; j < dev->endpoints_count; j++) {
            if (dev->endpoints_array[j] && dev->endpoints_array[j]->id == endpoint_id) {
                ep = dev->endpoints_array[j];
                updated_any = true;
                break;
            }
        }

        if (ep) {
            ESP_LOGD(TAG, "Endpoint 0x%02x already exists", endpoint_id);
            continue;
        }

        // Создаём новый эндпоинт
        ep = calloc(1, sizeof(zbm_dev_endpoint_t));
        if (!ep) {
            ESP_LOGE(TAG, "Failed to allocate endpoint %d", endpoint_id);
            continue;
        }
        ep->id = endpoint_id;
        ep->is_use_on_device = 1;

        // Расширяем массив
        zbm_dev_endpoint_t** new_array = realloc(dev->endpoints_array,
            (dev->endpoints_count + 1) * sizeof(zbm_dev_endpoint_t*));
        if (!new_array) {
            free(ep);
            ESP_LOGE(TAG, "Failed to realloc endpoints array");
            continue;
        }
        dev->endpoints_array = new_array;
        dev->endpoints_array[dev->endpoints_count++] = ep;
        created_any = true;

        ESP_LOGI(TAG, "Added new endpoint %d to device 0x%04x", endpoint_id, dev->short_addr);
    }

    // === Определяем результат ===
    if (created_any) {
        return 1; // что-то создано
    } else if (updated_any || ep_count > 0) {
        return 0; // только обновлено (или дубли)
    } else {
        return 0xFF; // не должно быть, но на всякий случай
    }
}


// === Генерация имени кластера ===
char* generate_cluster_name(uint16_t cluster_id, bool is_custom)
{
    const char* base_name = zbm_get_cluster_friendlyname(cluster_id);
    if (base_name) {
        char* result = strdup(base_name);
        if (!result) {
            ESP_LOGE(TAG, "Failed to strdup cluster name: %s", base_name);
        }
        return result;
    }

    const char* prefix = is_custom ? "Custom Cluster" : "Cluster";
    char buf[64];
    snprintf(buf, sizeof(buf), "%s 0x%04X", prefix, cluster_id);

    char* result = strdup(buf);
    if (!result) {
        ESP_LOGE(TAG, "Failed to strdup generated cluster name: %s", buf);
    }
    return result;
}





