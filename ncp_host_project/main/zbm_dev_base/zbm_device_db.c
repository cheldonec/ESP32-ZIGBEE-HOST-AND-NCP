#include "zbm_device_db.h"
#include "zbm_dev_types.h"
#include "zbm_dev_simple_func.h"
#include <string.h>
#include <stdlib.h>
#include <esp_log.h>
#include "zbm_core_sync.h"

static dev_node_t* dev_hash[ZBM_DEVICE_HASH_SIZE] = {0};

static uint8_t hash_short(uint16_t addr) {
    return addr % ZBM_DEVICE_HASH_SIZE;
}

void zbm_device_db_init(void) {
    for (int i = 0; i < ZBM_DEVICE_HASH_SIZE; i++) {
        dev_hash[i] = NULL;
    }
}

bool zbm_device_add_to_devdb(zbm_dev_t* dev) {
    if (!dev) return false;

    uint8_t idx = hash_short(dev->short_addr);
    dev_node_t* curr = dev_hash[idx];

    // Проверка: нет ли уже этого УСТРОЙСТВА
    while (curr) {
        if (curr->dev == dev) {
            ESP_LOGW("DB", "Device already in DB: ptr=%p, short=0x%04X", dev, dev->short_addr);
            return false;
        }
        curr = curr->next;
    }

    dev_node_t* node = calloc(1, sizeof(dev_node_t));
    if (!node) return false;

    node->dev = dev;
    node->next = dev_hash[idx];
    dev_hash[idx] = node;

    ESP_LOGI("DB", "✅ Added device to DB: ptr=%p, short=0x%04X, friendly=%s", dev, dev->short_addr, dev->friendly_name);

    zbm_guid_db_update_device_guids(dev);
    return true;
}

zbm_dev_t* zbm_find_device_in_devdb_by_short_old(uint16_t short_addr) {
    uint8_t idx = hash_short(short_addr);
    dev_node_t* node = dev_hash[idx];
    while (node) {
        if (node->dev->short_addr == short_addr) {
            return node->dev;
        }
        node = node->next;
    }
    return NULL;
}

zbm_dev_t* zbm_find_device_in_devdb_by_short(uint16_t short_addr) {
    
    for (int i = 0; i < ZBM_DEVICE_HASH_SIZE; i++) {
        dev_node_t* node = dev_hash[i];
        while (node) {
            if (node->dev->short_addr == short_addr) {
                return node->dev;
            }
            node = node->next;
        }
    }
    return NULL;
}

zbm_dev_t* zbm_find_device_in_devdb_by_ieee(const uint8_t* ieee_addr) {
    if (!ieee_addr) return NULL;
    for (int i = 0; i < ZBM_DEVICE_HASH_SIZE; i++) {
        dev_node_t* node = dev_hash[i];
        while (node) {
            if (memcmp(node->dev->ieee_addr, ieee_addr, 8) == 0) {
                return node->dev;
            }
            node = node->next;
        }
    }
    return NULL;
}

/**
 * @brief Удаляет узел устройства из базы по указателю (без удаления самого устройства)
 * @param dev Указатель на устройство
 * @return true если найдено и удалено, false если не найдено
 */
bool zbm_remove_device_from_devdb_by_dev(zbm_dev_t* dev) {
    if (!dev) return false;

    for (int i = 0; i < ZBM_DEVICE_HASH_SIZE; i++) {
        dev_node_t* curr = dev_hash[i];
        dev_node_t* prev = NULL;

        while (curr) {
            if (curr->dev == dev) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    dev_hash[i] = curr->next;
                }
                ESP_LOGI("DB", "🗑️ Removing node for device: ptr=%p, short was 0x%04X, friendly=%s", dev, dev->short_addr, dev->friendly_name);
                // Освобождаем само устройство
                //zbm_free_dev_t(curr->dev);
                free(curr);  // освобождаем ТОЛЬКО узел
                
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
    }

    ESP_LOGW("DB", "⚠️ Device node not found for removal: %s", dev->friendly_name);
    return false;
}

bool zbm_remove_device_from_devdb_by_short(uint16_t short_addr)
{
    for (int i = 0; i < ZBM_DEVICE_HASH_SIZE; i++) {
        dev_node_t* curr = dev_hash[i];
        dev_node_t* prev = NULL;

        while (curr) {
            if (curr->dev->short_addr == short_addr) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    dev_hash[i] = curr->next;
                }
                //ESP_LOGI("DB", "🗑️ Removing node for device: ptr=%p, short was 0x%04X, friendly=%s", dev, dev->short_addr, dev->friendly_name);
                // Освобождаем само устройство
                //zbm_free_dev_t(curr->dev);
                free(curr);  // освобождаем ТОЛЬКО узел
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
    }
    return false;

}

bool zbm_remove_device_from_devdb_by_ieee(const uint8_t* ieee_addr) {
    if (!ieee_addr) return false;

    bool removed = false;
    for (int i = 0; i < ZBM_DEVICE_HASH_SIZE; i++) {
        dev_node_t* curr = dev_hash[i];
        dev_node_t* prev = NULL;

        while (curr) {
            if (memcmp(curr->dev->ieee_addr, ieee_addr, 8) == 0) {
                // Удаляем узел
                if (prev) {
                    prev->next = curr->next;
                } else {
                    dev_hash[i] = curr->next;
                }

                // Освобождаем само устройство
                //zbm_free_dev_t(curr->dev);

                free(curr);
                removed = true;
                break; // одно устройство с таким IEEE
            }
            prev = curr;
            curr = curr->next;
        }
    }
    return removed;
}


void zbm_device_db_foreach(zbm_device_visitor_t visitor, void* ctx) {
    if (!visitor) return;
    for (int i = 0; i < ZBM_DEVICE_HASH_SIZE; i++) {
        dev_node_t* curr = dev_hash[i];
        while (curr) {
            ESP_LOGI("DB_FOREACH", "Visiting dev %p, short=0x%04X, ieee=%02X:%02X..%02X",
                     curr->dev,
                     curr->dev->short_addr,
                     curr->dev->ieee_addr[0], curr->dev->ieee_addr[1], curr->dev->ieee_addr[7]);
            visitor(curr->dev, ctx);
            curr = curr->next;
        }
    }
}

/**
 * @brief Считает количество устройств в базе
 * @return Количество активных устройств
 */
size_t zbm_device_db_count(void) {
    size_t count = 0;
    for (int i = 0; i < ZBM_DEVICE_HASH_SIZE; i++) {
        dev_node_t* curr = dev_hash[i];
        while (curr) {
            count++;
            curr = curr->next;
        }
    }
    return count;
}

size_t zbm_device_db_count_safe(void)
{
    size_t count = 0;
    zbm_core_sync_lock();  // Предполагается, что это мьютекс/критическая секция
    for (int i = 0; i < ZBM_DEVICE_HASH_SIZE; i++) {
        dev_node_t* curr = dev_hash[i];
        while (curr) {
            count++;
            curr = curr->next;
        }
    }
    zbm_core_sync_unlock();
    return count;
}

