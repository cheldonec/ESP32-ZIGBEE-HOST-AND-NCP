// main/zbm_dev_base/zbm_core_sync.c
#include "zbm_core_sync.h"

#include "zbm_device_db.h"
#include "zbm_guid_db.h"
#include "zbm_dev_simple_func.h"
#include "esp_log.h"

// === Заменяем pthread на FreeRTOS ===
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char* TAG = "ZBM_CORE_SYNC";
static SemaphoreHandle_t g_zbm_core_mutex = NULL;
static StaticSemaphore_t g_zbm_core_mutex_buffer;

void zbm_core_sync_init(void) {
    if (g_zbm_core_mutex == NULL) {
        // Создаём РЕКУРСИВНЫЙ мьютекс
        g_zbm_core_mutex = xSemaphoreCreateRecursiveMutexStatic(&g_zbm_core_mutex_buffer);
        if (g_zbm_core_mutex == NULL) {
            ESP_LOGE("SYNC", "Failed to create recursive mutex!");
        } else {
            ESP_LOGI("SYNC", "Core synchronization initialized (recursive)");
        }
    }
}

void zbm_core_sync_lock(void) {
    if (g_zbm_core_mutex != NULL) {
        if (xSemaphoreTakeRecursive(g_zbm_core_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGW("SYNC", "Lock timeout! Possible deadlock.");
        }
    }
}

void zbm_core_sync_unlock(void) {
    if (g_zbm_core_mutex != NULL) {
        xSemaphoreGiveRecursive(g_zbm_core_mutex);
    }
}

// ===================================================================
// === Device DB Safe Wrappers =======================================
// ===================================================================


// ===================================================================
// === zbm_device_manager_add_by_ieee_safe ===========================
// ===================================================================

zbm_dev_t* zbm_dev_create_and_add_to_devdb_by_ieee_safe(const uint8_t* ieee_addr)
{
    if (!ieee_addr) return NULL;

    zbm_core_sync_lock();

    // Проверяем, нет ли уже устройства с таким IEEE
    zbm_dev_t* existing = zbm_find_device_in_devdb_by_ieee(ieee_addr);
    if (existing) {
        zbm_core_sync_unlock();
        return existing;
    }

    // Создаём новое устройство
    zbm_dev_t* dev = zbm_create_device_obj_by_ieee(ieee_addr);
    if (!dev) {
        zbm_core_sync_unlock();
        return NULL;
    }

    // Добавляем в базу
    if (!zbm_device_add_to_devdb(dev)) {
        zbm_core_sync_unlock();
        zbm_free_dev_t(dev);
        return NULL;
    }

    zbm_core_sync_unlock();
    return dev;
}

// ===================================================================
// === zbm_device_manager_update_short_addr_safe =====================
// ===================================================================
bool zbm_dev_update_short_addr_safe(zbm_dev_t* dev, uint16_t new_short_addr, const uint8_t* ieee_addr)
{
    if (!dev) return false;

    zbm_core_sync_lock();

    // Если уже известен и совпадает — ок
    if (dev->short_addr == new_short_addr) {
        zbm_core_sync_unlock();
        return true;
    }

    // Проверим, нет ли другого устройства с таким short_addr
    zbm_dev_t* conflict = zbm_find_device_in_devdb_by_short(new_short_addr);
    if (conflict && conflict != dev) {
        ESP_LOGW(TAG, "Conflict: short address 0x%04X already used by another device, removing conflicting device", new_short_addr);
        zbm_guid_db_unregister_by_short_addr(new_short_addr);
        zbm_remove_device_from_devdb_by_short(new_short_addr);
        zbm_free_dev_t(conflict);
    }

    // Найти по IEEE (должно быть)
    zbm_dev_t* dev_in_base = zbm_find_device_in_devdb_by_ieee(ieee_addr);
    if (!dev_in_base || dev_in_base != dev) {
        ESP_LOGE(TAG, "Device not found in devdb by IEEE");
        zbm_core_sync_unlock();
        return false;
    }

    // === УДАЛЯЕМ ТОЛЬКО ИЗ ХЭША, НО НЕ УДАЛЯЕМ САМО УСТРОЙСТВО ===
    if (!zbm_remove_device_from_devdb_by_short(dev->short_addr)) {
        ESP_LOGD(TAG, "No old hash entry for short addr 0x%04X", dev->short_addr);
        // Это не ошибка — может быть первый раз
    }

    // === ОБНОВЛЯЕМ short_addr ===
    uint16_t old_addr = dev->short_addr;
    dev->short_addr = new_short_addr;

    // === ДОБАВЛЯЕМ В HASH С НОВЫМ АДРЕСОМ ===
    if (!zbm_device_add_to_devdb(dev)) {
        ESP_LOGE(TAG, "Failed to re-add device to devdb with new short address 0x%04X", new_short_addr);
        dev->short_addr = old_addr;  // откат
        zbm_core_sync_unlock();
        return false;
    }

    // === ОБНОВЛЯЕМ GUID DB ===
    zbm_guid_db_update_device_guids(dev);

    ESP_LOGI(TAG, "Successfully updated short address: 0x%04X -> 0x%04X", old_addr, new_short_addr);

    zbm_core_sync_unlock();
    return true;
}



// ===================================================================
// === zbm_device_manager_remove_by_ieee_safe ========================
// ===================================================================

bool zbm_device_manager_remove_by_ieee_safe(const uint8_t* ieee_addr)
{
    if (!ieee_addr) return false;

    zbm_core_sync_lock();

   bool result = zbm_remove_device_from_devdb_by_ieee(ieee_addr);
    zbm_core_sync_unlock();
    return result; // true если удалено, false если не найдено
}

// ===================================================================
// === zbm_device_manager_add_endpoint_safe ==========================
// ===================================================================

bool zbm_device_manager_add_endpoint_safe(zbm_dev_t* dev, uint8_t endpoint_id,
                                          zbm_device_type_t device_id,
                                          const char* friendlyname)
{
    if (!dev) return false;

    zbm_core_sync_lock();

    // Проверка на дубликат по ID
    for (uint8_t i = 0; i < dev->endpoints_count; i++) {
        if (dev->endpoints_array[i] && dev->endpoints_array[i]->id == endpoint_id) {
            zbm_core_sync_unlock();
            return false; // Эндпоинт с таким ID уже есть
        }
    }

    // Создаём новый эндпоинт
    zbm_dev_endpoint_t* ep = zbm_create_empty_endpoint();
    if (!ep) {
        zbm_core_sync_unlock();
        return false;
    }

    ep->id = endpoint_id;
    ep->device_id = device_id;

    if (friendlyname) {
        ep->friendlyname = strdup(friendlyname);
        if (!ep->friendlyname) {
            free(ep);
            zbm_core_sync_unlock();
            return false;
        }
    }

    // Увеличиваем массив
    zbm_dev_endpoint_t** new_array = realloc(dev->endpoints_array,
                                             (dev->endpoints_count + 1) * sizeof(zbm_dev_endpoint_t*));
    if (!new_array) {
        zbm_free_dev_endpoint(ep); // освободит friendlyname и т.д.
        zbm_core_sync_unlock();
        return false;
    }

    dev->endpoints_array = new_array;
    dev->endpoints_array[dev->endpoints_count] = ep;
    dev->endpoints_count++;

    zbm_core_sync_unlock();
    return true;
}


zbm_dev_t* zbm_find_device_in_devdb_by_short_safe(uint16_t short_addr) {
    zbm_core_sync_lock();
    zbm_dev_t* dev = zbm_find_device_in_devdb_by_short(short_addr);
    zbm_core_sync_unlock();
    return dev;
}

zbm_dev_t* zbm_find_device_in_devdb_by_ieee_safe(const uint8_t* ieee_addr) {
    zbm_core_sync_lock();
    zbm_dev_t* dev = zbm_find_device_in_devdb_by_ieee(ieee_addr);
    zbm_core_sync_unlock();
    return dev;
}

bool zbm_remove_device_from_devdb_and_guiddb_by_short_self(uint16_t short_addr) {
    bool result = false;
    zbm_core_sync_lock();
    // Сначала удаляем все GUID, связанные с устройством
    zbm_guid_db_unregister_by_short_addr(short_addr);
    // Потом удаляем само устройство
    result = zbm_remove_device_from_devdb_by_short(short_addr);
    zbm_core_sync_unlock();
    return result;
}

void zbm_device_db_foreach_safe(void (*visitor)(zbm_dev_t*, void*), void* ctx) {
    zbm_core_sync_lock();
    zbm_device_db_foreach(visitor, ctx);
    zbm_core_sync_unlock();
}

void zbm_guid_db_update_device_guids_safe(zbm_dev_t* dev) {
    if (!dev) return;

    zbm_core_sync_lock();
        zbm_guid_db_update_device_guids(dev);
    zbm_core_sync_unlock();
}

// ===================================================================
// === GUID DB Safe Wrappers =========================================
// ===================================================================

zbm_standart_cluster_t* zbm_create_standard_cluster_safe(uint16_t cluster_id, zbm_cluster_role_t role_mask)
{
    zbm_core_sync_lock();
    zbm_standart_cluster_t* cluster = zbm_create_standard_cluster(cluster_id, role_mask);
    zbm_core_sync_unlock();
    return cluster;
}

bool zbm_guid_db_register_safe(zbm_cluster_attribute_t** pp_attr,
                               uint16_t short_addr,
                               uint8_t endpoint,
                               uint16_t cluster_id,
                               uint16_t attr_id,
                               const char* custom_guid) {
    zbm_core_sync_lock();
    bool result = zbm_guid_db_register(pp_attr, short_addr, endpoint, cluster_id, attr_id, custom_guid);
    zbm_core_sync_unlock();
    return result;
}

zbm_cluster_attribute_t* zbm_find_attr_by_guid_safe(const char* guid) {
    zbm_core_sync_lock();
    zbm_cluster_attribute_t* attr = zbm_find_attr_by_guid(guid);
    zbm_core_sync_unlock();
    return attr;
}

zbm_cluster_attribute_t* zbm_find_attr_by_key_safe(uint16_t short_addr,
                                                   uint8_t endpoint,
                                                   uint16_t cluster_id,
                                                   uint16_t attr_id) {
    zbm_core_sync_lock();
    zbm_cluster_attribute_t* attr = zbm_find_attr_by_key(short_addr, endpoint, cluster_id, attr_id);
    zbm_core_sync_unlock();
    return attr;
}

void zbm_guid_db_unregister_by_guid_safe(const char* guid) {
    zbm_core_sync_lock();
    zbm_guid_db_unregister_by_guid(guid);
    zbm_core_sync_unlock();
}

void zbm_guid_db_unregister_by_attr_ptr_safe(zbm_cluster_attribute_t* attr) {
    zbm_core_sync_lock();
    zbm_guid_db_unregister_by_attr_ptr(attr);
    zbm_core_sync_unlock();
}

void zbm_guid_db_refresh_all_attr_ptrs_safe(void) {
    zbm_core_sync_lock();
    zbm_guid_db_refresh_all_attr_ptrs();
    zbm_core_sync_unlock();
}

// ===================================================================
// === Update Functions ==============================================
// ===================================================================



uint8_t zbm_device_apply_reported_value_safe(zbm_dev_t* dev_obj,
                                          uint8_t endpoint_id,
                                          uint16_t cluster_id,
                                          zbm_cluster_role_t role_mask,
                                          uint16_t attr_id,
                                          const char* attr_friendlyname,
                                          uint8_t acces,
                                          zbm_attr_data_types_t data_type,
                                          uint16_t data_size,
                                          const void* new_value) {
                    
    zbm_core_sync_lock();
    uint8_t result = zbm_device_apply_reported_value(dev_obj, endpoint_id, cluster_id,
                                                  role_mask,
                                                  attr_id, attr_friendlyname, acces,
                                                  data_type, data_size, new_value);
    zbm_core_sync_unlock();
    return result;
}

uint8_t zbm_update_cluster_custom_report_safe(zbm_dev_t* dev_obj,
                                              uint8_t endpoint_id,
                                              uint16_t cluster_id,
                                              zbm_cluster_role_t role_mask,
                                              uint8_t cmd_id,
                                              const char* cmd_friendlyname,
                                              zbm_cmd_data_types_t data_type,
                                              uint16_t data_size,
                                              const void* new_value) {
    zbm_core_sync_lock();
    uint8_t result = zbm_update_cluster_custom_report(dev_obj, endpoint_id, cluster_id,
                                                      role_mask,
                                                      cmd_id, cmd_friendlyname,
                                                      data_type, data_size, new_value);
    zbm_core_sync_unlock();
    return result;
}


// ===================================================================
// === zbm_device_apply_simple_descriptor_safe =======================
// ===================================================================

// ===================================================================
// === zbm_device_apply_simple_descriptor_safe =======================
// ===================================================================

void zbm_device_apply_simple_descriptor_safe(zbm_dev_t* dev,
                                             uint8_t endpoint_id,
                                             uint16_t device_id,
                                             uint16_t* input_clusters, uint8_t in_count,
                                             uint16_t* output_clusters, uint8_t out_count)
{
    if (!dev || !input_clusters || in_count == 0) return;

    zbm_core_sync_lock();

    // --- Шаг 1: Найти или создать эндпоинт ---
    zbm_dev_endpoint_t* ep = zbm_find_endpoint_by_id(dev, endpoint_id);
    bool new_ep = false;

    if (!ep) {
        ep = zbm_create_empty_endpoint();
        if (!ep) {
            ESP_LOGE("SD", "Failed to allocate endpoint %d", endpoint_id);
            zbm_core_sync_unlock();
            return;
        }

        ep->id = endpoint_id;
        ep->device_id = device_id;

        char ep_name[32];
        snprintf(ep_name, sizeof(ep_name), "EP %d", endpoint_id);
        ep->friendlyname = strdup(ep_name);
        if (!ep->friendlyname) {
            free(ep);
            ESP_LOGE("SD", "Failed to allocate endpoint name");
            zbm_core_sync_unlock();
            return;
        }

        // Увеличить массив эндпоинтов
        zbm_dev_endpoint_t** new_array = realloc(dev->endpoints_array,
            (dev->endpoints_count + 1) * sizeof(zbm_dev_endpoint_t*));
        if (!new_array) {
            free(ep->friendlyname);
            free(ep);
            ESP_LOGE("SD", "Failed to grow endpoints array");
            zbm_core_sync_unlock();
            return;
        }

        dev->endpoints_array = new_array;
        dev->endpoints_array[dev->endpoints_count] = ep;
        dev->endpoints_count++;
        new_ep = true;

        ESP_LOGI("SD", "Created new endpoint %d", endpoint_id);
    } else {
        // Если эндпоинт уже есть — обновим device_id, если нужно
        if (ep->device_id == 0) {
            ep->device_id = device_id;
        }
        ESP_LOGD("SD", "Endpoint %d already exists", endpoint_id);
    }

    // --- Шаг 2: Добавляем server-кластеры ---
    for (int i = 0; i < in_count; i++) {
        uint16_t cluster_id = input_clusters[i];

        // Проверяем, нет ли уже такого кластера
        zbm_standart_cluster_t* cluster = zbm_find_standard_cluster_by_id(ep, cluster_id);
        if (cluster) {
            // Если роль не SERVER — добавим флаг
            if (cluster->role_mask & ZBM_CLUSTER_ROLE_SERVER) {
                continue;
            } else {
                cluster->role_mask |= ZBM_CLUSTER_ROLE_SERVER;
                ESP_LOGD("SD", "Updated role of cluster 0x%04X to include SERVER", cluster_id);
            }
        } else {
            // Создаём новый кластер как SERVER
            cluster = zbm_create_standard_cluster(cluster_id, ZBM_CLUSTER_ROLE_SERVER);
            if (!cluster) {
                ESP_LOGW("SD", "Failed to create standard cluster 0x%04X (server)", cluster_id);
                continue;
            }

            // Добавляем в массив
            zbm_standart_cluster_t** new_array = realloc(ep->standart_cluster_array,
                (ep->standart_cluster_count + 1) * sizeof(zbm_standart_cluster_t*));
            if (!new_array) {
                zbm_free_standart_cluster(cluster);
                ESP_LOGW("SD", "Failed to grow cluster array for server cluster 0x%04X", cluster_id);
                continue;
            }

            ep->standart_cluster_array = new_array;
            ep->standart_cluster_array[ep->standart_cluster_count] = cluster;
            ep->standart_cluster_count++;

            ESP_LOGI("SD", "Added server cluster 0x%04X to endpoint %d", cluster_id, endpoint_id);
             // === РЕГИСТРИРУЕМ АТРИБУТЫ И КОМАНДЫ, если short_addr известен ===
             // 0xFFFE — неизвестный адрес   0xFFFF — broadcast
            if (dev->short_addr != ZBM_ADDR_UNKNOWN) {                                                    
                for (int j = 0; j < cluster->attr_count; j++) {
                    zbm_cluster_attribute_t* attr = cluster->attr_array[j];
                    if (attr && attr->guid[0] == '\0') {  // ✅ проверка: GUID ещё не установлен
                        bool registered = zbm_guid_db_register(
                            &cluster->attr_array[j],
                            dev->short_addr,
                            ep->id,
                            cluster_id,
                            attr->id,
                            NULL
                        );
                        if (!registered) {
                            ESP_LOGD("SD", "Failed to register attr GUID: cluster 0x%04X attr 0x%04X", cluster_id, attr->id);
                        }
                    }
                }

                if (cluster->role_mask & ZBM_CLUSTER_ROLE_SERVER) {
                    for (int j = 0; j < cluster->standart_cmd_count; j++) {
                        zbm_cluster_standart_cmd_t* cmd = cluster->standart_cmd_array[j];
                        if (cmd && cmd->guid[0] == '\0') {  // ✅
                            bool registered = zbm_guid_db_register_cmd(
                                &cluster->standart_cmd_array[j],
                                dev->short_addr,
                                ep->id,
                                cluster_id,
                                cmd->id,
                                NULL
                            );
                            if (!registered) {
                                ESP_LOGD("SD", "Failed to register cmd GUID: cluster 0x%04X cmd 0x%02X", cluster_id, cmd->id);
                            }
                        }
                    }
                }

                // Кастомные репорты
                for (int j = 0; j < cluster->custom_report_cmd_count; j++) {
                    zbm_cluster_custom_report_cmd_t* report = cluster->custom_report_cmd_array[j];
                    if (report && report->guid[0] == '\0') {  // ✅
                        bool registered = zbm_guid_db_register_custom_report(
                            &cluster->custom_report_cmd_array[j],
                            dev->short_addr,
                            ep->id,
                            cluster_id,
                            report->id,
                            NULL
                        );
                        if (!registered) {
                            ESP_LOGD("SD", "Failed to register custom report GUID: cluster 0x%04X rep 0x%02X", cluster_id, report->id);
                        }
                    }
                }
            }
        }
    }

    // --- Шаг 3: Добавляем client-кластеры ---
    for (int i = 0; i < out_count; i++) {
        uint16_t cluster_id = output_clusters[i];

        zbm_standart_cluster_t* cluster = zbm_find_standard_cluster_by_id(ep, cluster_id);
        if (cluster) {
            if (cluster->role_mask & ZBM_CLUSTER_ROLE_CLIENT) {
                continue;
            } else {
                cluster->role_mask |= ZBM_CLUSTER_ROLE_CLIENT;
                ESP_LOGD("SD", "Updated role of cluster 0x%04X to include CLIENT", cluster_id);
            }
        } else {
            cluster = zbm_create_standard_cluster(cluster_id, ZBM_CLUSTER_ROLE_CLIENT);
            if (!cluster) {
                ESP_LOGW("SD", "Failed to create standard cluster 0x%04X (client)", cluster_id);
                continue;
            }

            zbm_standart_cluster_t** new_array = realloc(ep->standart_cluster_array,
                (ep->standart_cluster_count + 1) * sizeof(zbm_standart_cluster_t*));
            if (!new_array) {
                zbm_free_standart_cluster(cluster);
                ESP_LOGW("SD", "Failed to grow cluster array for client cluster 0x%04X", cluster_id);
                continue;
            }

            ep->standart_cluster_array = new_array;
            ep->standart_cluster_array[ep->standart_cluster_count] = cluster;
            ep->standart_cluster_count++;

            ESP_LOGI("SD", "Added client cluster 0x%04X to endpoint %d", cluster_id, endpoint_id);

             // === РЕГИСТРИРУЕМ АТРИБУТЫ И КОМАНДЫ, если short_addr известен ===
              // 0xFFFE — неизвестный адрес   0xFFFF — broadcast
            if (dev->short_addr != ZBM_ADDR_UNKNOWN) {
                for (int j = 0; j < cluster->attr_count; j++) {
                    zbm_cluster_attribute_t* attr = cluster->attr_array[j];
                    if (attr && attr->guid[0] == '\0') {  // ✅ проверка: GUID ещё не установлен
                        bool registered = zbm_guid_db_register(
                            &cluster->attr_array[j],
                            dev->short_addr,
                            ep->id,
                            cluster_id,
                            attr->id,
                            NULL
                        );
                        if (!registered) {
                            ESP_LOGD("SD", "Failed to register attr GUID: cluster 0x%04X attr 0x%04X", cluster_id, attr->id);
                        }
                    }
                }

                if (cluster->role_mask & ZBM_CLUSTER_ROLE_SERVER) {
                    for (int j = 0; j < cluster->standart_cmd_count; j++) {
                        zbm_cluster_standart_cmd_t* cmd = cluster->standart_cmd_array[j];
                        if (cmd && cmd->guid[0] == '\0') {  // ✅
                            bool registered = zbm_guid_db_register_cmd(
                                &cluster->standart_cmd_array[j],
                                dev->short_addr,
                                ep->id,
                                cluster_id,
                                cmd->id,
                                NULL
                            );
                            if (!registered) {
                                ESP_LOGD("SD", "Failed to register cmd GUID: cluster 0x%04X cmd 0x%02X", cluster_id, cmd->id);
                            }
                        }
                    }
                }

                // Кастомные репорты
                for (int j = 0; j < cluster->custom_report_cmd_count; j++) {
                    zbm_cluster_custom_report_cmd_t* report = cluster->custom_report_cmd_array[j];
                    if (report && report->guid[0] == '\0') {  // ✅
                        bool registered = zbm_guid_db_register_custom_report(
                            &cluster->custom_report_cmd_array[j],
                            dev->short_addr,
                            ep->id,
                            cluster_id,
                            report->id,
                            NULL
                        );
                        if (!registered) {
                            ESP_LOGD("SD", "Failed to register custom report GUID: cluster 0x%04X rep 0x%02X", cluster_id, report->id);
                        }
                    }
                }
            }
        }
    }
   

    zbm_core_sync_unlock();
     // 0xFFFE — неизвестный адрес   0xFFFF — broadcast
    if (dev->short_addr != ZBM_ADDR_UNKNOWN) {
        zbm_guid_db_update_device_guids_safe(dev);
    }
}

