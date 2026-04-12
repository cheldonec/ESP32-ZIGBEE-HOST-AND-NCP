// main/zbm_core/zbm_guid_db.c
#include "zbm_guid_db.h"
#include "zbm_device_db.h"
#include "zbm_dev_types.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char* TAG = "GUID_DB";

#define ZBM_GUID_HASH_SIZE 1024

// === NEW: Обобщённая структура узла ================================
typedef struct guid_node {
    char guid[64];
    void** pp_ptr;           // Указывает на указатель (attr**, cmd**, report**)
    struct guid_node* next;
} guid_node_t;

static guid_node_t* hash_table[ZBM_GUID_HASH_SIZE] = {0};

static void refresh_attrs_in_device(zbm_dev_t* dev, void* ctx);
static void* zbm_find_by_generic_guid(const char* guid);
static void zbm_guid_db_unregister_by_guid_generic(const char* guid);
static void zbm_guid_db_unregister_by_ptr_generic(void* ptr);

// Простой FNV-1a хеш
static uint8_t hash_guid(const char* str) {
    uint32_t h = 2166136261;
    while (*str) {
        h ^= *str++;
        h *= 16777619;
    }
    return h % ZBM_GUID_HASH_SIZE;
}

void zbm_guid_db_destroy(void) {
    for (int i = 0; i < ZBM_GUID_HASH_SIZE; i++) {
        guid_node_t* curr = hash_table[i];
        while (curr) {
            guid_node_t* next = curr->next;
            ESP_LOGD(TAG, "Freeing GUID node: %s", curr->guid);
            free(curr);
            curr = next;
        }
        hash_table[i] = NULL;
    }
}

void zbm_guid_db_init(void) {
    zbm_guid_db_destroy();
    for (int i = 0; i < ZBM_GUID_HASH_SIZE; i++) {
        hash_table[i] = NULL;
    }
    ESP_LOGI(TAG, "GUID database initialized");
}

// === Проверка наличия GUID ========================================
bool zbm_guid_db_contains_guid(const char* guid) {
    if (!guid) return false;
    uint8_t idx = hash_guid(guid);
    guid_node_t* node = hash_table[idx];
    while (node) {
        if (strcmp(node->guid, guid) == 0) {
            return true;
        }
        node = node->next;
    }
    return false;
}

// === NEW: Общая регистрация ========================================

static bool zbm_guid_db_register_generic(void** pp_ptr,
                                         uint16_t short_addr,
                                         uint8_t endpoint,
                                         uint16_t cluster_id,
                                         uint32_t item_id,
                                         const char* type_prefix,
                                         const char* custom_guid)
{
    //ESP_LOGI(TAG, "TRY Registering GUID for %s", type_prefix); // ← Добавляем тип префикса в лог
    if (!pp_ptr || !*pp_ptr) {
        ESP_LOGE(TAG, "Invalid pointer for registration");
        return false;
    }

    char guid[64];
    if (custom_guid && strlen(custom_guid) > 0) {
        strncpy(guid, custom_guid, sizeof(guid) - 1);
        guid[sizeof(guid) - 1] = '\0';
    } else {
        snprintf(guid, sizeof(guid), "0x%04X:%d:%s:%04X:%02X",
                 short_addr, endpoint, type_prefix, cluster_id, (uint8_t)item_id);
    }

    // === Проверяем, не зарегистрирован ли уже этот GUID ===
    if (zbm_guid_db_contains_guid(guid)) {
        ESP_LOGI(TAG, "GUID already exists, skipping: %s", guid);
        return true;  // ← ВАЖНО: возвращаем true, чтобы продолжить работу
    }

    guid_node_t* node = calloc(1, sizeof(guid_node_t));
    if (!node) {
        ESP_LOGE(TAG, "Failed to allocate guid node");
        return false;
    }

    strcpy(node->guid, guid);
    node->pp_ptr = pp_ptr;

    uint8_t idx = hash_guid(guid);
    node->next = hash_table[idx];
    hash_table[idx] = node;
    //ESP_LOGI(TAG, "✅ ADDED GUID: %s (bucket %d)", guid, idx);
    return true;
}

// === ATTRIBUTE: Original functions, updated ========================
bool zbm_guid_db_register(zbm_cluster_attribute_t** pp_attr,
                          uint16_t short_addr,
                          uint8_t endpoint,
                          uint16_t cluster_id,
                          uint16_t attr_id,
                          const char* custom_guid)
{
    // === Сначала сформируем GUID ===
    char guid[64];
    if (custom_guid && strlen(custom_guid) > 0) {
        strncpy(guid, custom_guid, sizeof(guid) - 1);
        guid[sizeof(guid) - 1] = '\0';
    } else {
        snprintf(guid, sizeof(guid), "0x%04X:%d:attr:%04X:%04X",
                 short_addr, endpoint, cluster_id, attr_id);
    }

    // === Запишем в структуру ВСЕГДА ===
    zbm_cluster_attribute_t* attr = *pp_attr;
    if (attr) {
        strlcpy(attr->guid, guid, sizeof(attr->guid));
    }

    // === Теперь попробуем зарегистрировать ===
    bool result = zbm_guid_db_register_generic(
        (void**)pp_attr, short_addr, endpoint, cluster_id, attr_id,
        "attr", custom_guid
    );

    return result; // можно вернуть false, если дубликат, но guid уже заполнен
}


zbm_cluster_attribute_t* zbm_find_attr_by_guid(const char* guid) {
    if (!guid) return NULL;
    uint8_t idx = hash_guid(guid);
    guid_node_t* node = hash_table[idx];
    while (node) {
        if (strcmp(node->guid, guid) == 0) {
            zbm_cluster_attribute_t* attr = *(zbm_cluster_attribute_t**)node->pp_ptr;
            if (attr) {
                ESP_LOGD(TAG, "Found attr by GUID: %s → %p", guid, attr);
            }
            return attr;
        }
        node = node->next;
    }
    return NULL;
}

zbm_cluster_attribute_t* zbm_find_attr_by_key(uint16_t short_addr, uint8_t endpoint,
                                              uint16_t cluster_id, uint16_t attr_id)
{
    char guid[64];
    snprintf(guid, sizeof(guid), "0x%04X:%d:attr:%04X:%04X",
             short_addr, endpoint, cluster_id, attr_id);
    return zbm_find_attr_by_guid(guid);
}

void zbm_guid_db_unregister_by_guid(const char* guid) {
    if (!guid) return;
    uint8_t idx = hash_guid(guid);
    guid_node_t* prev = NULL;
    guid_node_t* curr = hash_table[idx];
    while (curr) {
        if (strcmp(curr->guid, guid) == 0) {
            if (prev) prev->next = curr->next;
            else hash_table[idx] = curr->next;
            ESP_LOGD(TAG, "Unregistered GUID: %s", guid);
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

void zbm_guid_db_unregister_by_attr_ptr(zbm_cluster_attribute_t* attr) {
    if (!attr) return;
    for (int i = 0; i < ZBM_GUID_HASH_SIZE; i++) {
        guid_node_t* prev = NULL;
        guid_node_t* curr = hash_table[i];
        while (curr) {
            if (*(curr->pp_ptr) == attr) {
                if (prev) prev->next = curr->next;
                else hash_table[i] = curr->next;
                ESP_LOGD(TAG, "Unregistered attr ptr: %p (GUID: %s)", attr, curr->guid);
                free(curr);
                return;
            }
            prev = curr;
            curr = curr->next;
        }
    }
}

bool zbm_guid_db_unregister_by_short_addr(uint16_t short_addr) {
    bool result = false;
    char prefix[16];
    snprintf(prefix, sizeof(prefix), "0x%04X:", short_addr);
    for (int i = 0; i < ZBM_GUID_HASH_SIZE; i++) {
        guid_node_t* prev = NULL;
        guid_node_t* curr = hash_table[i];
        while (curr) {
            if (strncmp(curr->guid, prefix, strlen(prefix)) == 0) {
                guid_node_t* to_free = curr;
                if (prev) prev->next = curr->next;
                else hash_table[i] = curr->next;
                curr = curr->next;
                ESP_LOGD(TAG, "Unregistered GUID: %s", to_free->guid);
                result = true;
                free(to_free);
            } else {
                prev = curr;
                curr = curr->next;
            }
        }
    }
    return result;
}

// === GENERIC HELPERS ===============================================

static void* zbm_find_by_generic_guid(const char* guid) {
    ESP_LOGI(TAG,"try find CMD By GUID %s",guid);
    if (!guid) return NULL;
    uint8_t idx = hash_guid(guid);
    guid_node_t* node = hash_table[idx];
    while (node) {
        if (strcmp(node->guid, guid) == 0) {
            return *(node->pp_ptr);
        }
        node = node->next;
    }
    return NULL;
}

static void zbm_guid_db_unregister_by_guid_generic(const char* guid) {
    if (!guid) return;
    uint8_t idx = hash_guid(guid);
    guid_node_t* prev = NULL;
    guid_node_t* curr = hash_table[idx];
    while (curr) {
        if (strcmp(curr->guid, guid) == 0) {
            if (prev) prev->next = curr->next;
            else hash_table[idx] = curr->next;
            ESP_LOGD(TAG, "Unregistered GUID: %s", guid);
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

static void zbm_guid_db_unregister_by_ptr_generic(void* ptr) {
    if (!ptr) return;
    for (int i = 0; i < ZBM_GUID_HASH_SIZE; i++) {
        guid_node_t* prev = NULL;
        guid_node_t* curr = hash_table[i];
        while (curr) {
            if (*(curr->pp_ptr) == ptr) {
                if (prev) prev->next = curr->next;
                else hash_table[i] = curr->next;
                ESP_LOGD(TAG, "Unregistered ptr: %p (GUID: %s)", ptr, curr->guid);
                free(curr);
                return;
            }
            prev = curr;
            curr = curr->next;
        }
    }
}

// === COMMANDS ======================================================
bool zbm_guid_db_register_cmd(
    zbm_cluster_standart_cmd_t** cmd_ptr,
    uint16_t short_addr,
    uint8_t endpoint_id,
    uint16_t cluster_id,
    uint8_t cmd_id,
    const char* custom_suffix)
{
    zbm_cluster_standart_cmd_t* cmd = *cmd_ptr;
    if (!cmd) return false;

    // Генерация GUID
    char guid[64];
    if (custom_suffix) {
        snprintf(guid, sizeof(guid), "0x%04X:%d:cmd:%04X:%s", short_addr, endpoint_id, cluster_id, custom_suffix);
    } else {
        snprintf(guid, sizeof(guid), "0x%04X:%d:cmd:%04X:%02X", short_addr, endpoint_id, cluster_id, cmd_id);
    }

    // === Записываем ВСЕГДА ===
    strlcpy(cmd->guid, guid, sizeof(cmd->guid));

    ESP_LOGI(TAG, "🔧 Registering CMD: %s → %p", guid, *cmd_ptr);

    return zbm_guid_db_register_generic(
        (void**)cmd_ptr,
        short_addr,
        endpoint_id,
        cluster_id,
        cmd_id,
        "cmd",
        custom_suffix
    );
}


zbm_cluster_standart_cmd_t* zbm_find_cmd_by_guid(const char* guid) {
    return (zbm_cluster_standart_cmd_t*)zbm_find_by_generic_guid(guid);
    
}

zbm_cluster_standart_cmd_t* zbm_find_cmd_by_key(uint16_t short_addr,
                                                uint8_t endpoint,
                                                uint16_t cluster_id,
                                                uint8_t cmd_id)
{
    char guid[64];
    snprintf(guid, sizeof(guid), "0x%04X:%d:cmd:%04X:%02X",
             short_addr, endpoint, cluster_id, cmd_id);
    return zbm_find_cmd_by_guid(guid);
}

void zbm_guid_db_unregister_cmd_by_guid(const char* guid) {
    zbm_guid_db_unregister_by_guid_generic(guid);
}

void zbm_guid_db_unregister_cmd_by_ptr(zbm_cluster_standart_cmd_t* cmd) {
    zbm_guid_db_unregister_by_ptr_generic(cmd);
}

// === CUSTOM REPORTS ================================================
bool zbm_guid_db_register_custom_report(
    zbm_cluster_custom_report_cmd_t** pp_report,
    uint16_t short_addr,
    uint8_t endpoint,
    uint16_t cluster_id,
    uint8_t cmd_id,
    const char* custom_guid)
{
    if (!pp_report || !*pp_report) {
        ESP_LOGE(TAG, "Invalid report pointer");
        return false;
    }

    zbm_cluster_custom_report_cmd_t* report = *pp_report;

    // === Генерация GUID строки ===
    char guid[64];
    if (custom_guid && strlen(custom_guid) > 0) {
        // Используем кастомный GUID
        strlcpy(guid, custom_guid, sizeof(guid));
    } else {
        // Формат: 0x1234:1:rep:0006:FD
        snprintf(guid, sizeof(guid), "0x%04X:%d:rep:%04X:%02X",
                 short_addr, endpoint, cluster_id, cmd_id);
    }

    // === ВСЕГДА записываем в структуру ===
    strlcpy(report->guid, guid, sizeof(report->guid));

    // === Регистрируем в хэше (может вернуть false при дубликате — нормально) ===
    bool result = zbm_guid_db_register_generic(
        (void**)pp_report,
        short_addr,
        endpoint,
        cluster_id,
        cmd_id,
        "rep",              // тип: "rep"
        custom_guid         // передаём дальше, если нужен кастомный
    );

    if (!result) {
        ESP_LOGW(TAG, "GUID already registered (or error): %s", guid);
        // Но! report->guid уже заполнен — это главное
    }

    return result;
}


zbm_cluster_custom_report_cmd_t* zbm_find_custom_report_by_guid(const char* guid) {
    return (zbm_cluster_custom_report_cmd_t*)zbm_find_by_generic_guid(guid);
}

zbm_cluster_custom_report_cmd_t* zbm_find_custom_report_by_key(uint16_t short_addr,
                                                               uint8_t endpoint,
                                                               uint16_t cluster_id,
                                                               uint8_t cmd_id)
{
    char guid[64];
    snprintf(guid, sizeof(guid), "0x%04X:%d:rep:%04X:%02X",
             short_addr, endpoint, cluster_id, cmd_id);
    return zbm_find_custom_report_by_guid(guid);
}

void zbm_guid_db_unregister_custom_report_by_guid(const char* guid) {
    zbm_guid_db_unregister_by_guid_generic(guid);
}

void zbm_guid_db_unregister_custom_report_by_ptr(zbm_cluster_custom_report_cmd_t* report) {
    zbm_guid_db_unregister_by_ptr_generic(report);
}



// === REMOVE ALL CMD & REPORTS BY SHORT_ADDR ========================

/*void zbm_guid_db_unregister_cmds_and_reports_by_short_addr(uint16_t short_addr) {
    char prefix[16];
    snprintf(prefix, sizeof(prefix), "0x%04X:", short_addr);
    for (int i = 0; i < ZBM_GUID_HASH_SIZE; i++) {
        guid_node_t* prev = NULL;
        guid_node_t* curr = hash_table[i];
        while (curr) {
            if (strncmp(curr->guid, prefix, strlen(prefix)) == 0 &&
                (strstr(curr->guid, ":cmd:") || strstr(curr->guid, ":rep:"))) {
                guid_node_t* to_free = curr;
                if (prev) prev->next = curr->next;
                else hash_table[i] = curr->next;
                curr = curr->next;
                ESP_LOGD(TAG, "Unregistered cmd/report: %s", to_free->guid);
                free(to_free);
            } else {
                prev = curr;
                curr = curr->next;
            }
        }
    }
}*/

// === REFRESH & INVALIDATE ==========================================

static void refresh_attrs_in_device(zbm_dev_t* dev, void* ctx);

void zbm_guid_db_refresh_all_attr_ptrs(void) {
    zbm_device_db_foreach(refresh_attrs_in_device, NULL);
}

static void refresh_attrs_in_device(zbm_dev_t* dev, void* ctx) {
    if (!dev || !dev->endpoints_array) return;
    for (uint8_t ep_idx = 0; ep_idx < dev->endpoints_count; ep_idx++) {
        zbm_dev_endpoint_t* ep = dev->endpoints_array[ep_idx];
        if (!ep) continue;

        if (ep->standart_cluster_array) {
            for (int cl_idx = 0; cl_idx < ep->standart_cluster_count; cl_idx++) {
                zbm_standart_cluster_t* cluster = ep->standart_cluster_array[cl_idx];
                if (!cluster || !cluster->attr_array) continue;
                for (int attr_idx = 0; attr_idx < cluster->attr_count; attr_idx++) {
                    zbm_cluster_attribute_t* attr = cluster->attr_array[attr_idx];
                    if (!attr) continue;
                    zbm_guid_db_register(&cluster->attr_array[attr_idx],
                        dev->short_addr, ep->id, cluster->id, attr->id, NULL);
                }
            }
        }
        if (ep->custom_cluster_array) {
            for (int cl_idx = 0; cl_idx < ep->custom_cluster_count; cl_idx++) {
                zbm_custom_cluster_t* cluster = ep->custom_cluster_array[cl_idx];
                if (!cluster || !cluster->attr_array) continue;
                for (int attr_idx = 0; attr_idx < cluster->attr_count; attr_idx++) {
                    zbm_cluster_attribute_t* attr = cluster->attr_array[attr_idx];
                    if (!attr) continue;
                    zbm_guid_db_register(&cluster->attr_array[attr_idx],
                        dev->short_addr, ep->id, cluster->id, attr->id, NULL);
                }
            }
        }
    }
}

void zbm_guid_db_update_cluster_attr_ptrs(uint16_t short_addr,
                                              uint8_t endpoint_id,
                                              uint16_t cluster_id,
                                              zbm_cluster_attribute_t** attr_array,
                                              uint8_t attr_count)
{
    if (!attr_array || attr_count == 0) return;
    for (int i = 0; i < attr_count; i++) {
        char guid[64];
        snprintf(guid, sizeof(guid), "0x%04X:%d:attr:%04X:%04X",
                 short_addr, endpoint_id, cluster_id, attr_array[i]->id);
        uint8_t idx = hash_guid(guid);
        guid_node_t* node = hash_table[idx];
        while (node) {
            if (strcmp(node->guid, guid) == 0) {
                node->pp_ptr = (void**)&attr_array[i];
                break;
            }
            node = node->next;
        }
    }
}

void zbm_guid_db_update_custom_cluster_attr_ptrs(uint16_t short_addr,
                                                     uint8_t endpoint_id,
                                                     uint16_t cluster_id,
                                                     zbm_cluster_attribute_t** attr_array,
                                                     uint8_t attr_count)
{
    if (!attr_array || attr_count == 0) return;
    for (int i = 0; i < attr_count; i++) {
        char guid[64];
        snprintf(guid, sizeof(guid), "0x%04X:%d:attr:%04X:%04X",
                 short_addr, endpoint_id, cluster_id, attr_array[i]->id);
        uint8_t idx = hash_guid(guid);
        guid_node_t* node = hash_table[idx];
        while (node) {
            if (strcmp(node->guid, guid) == 0) {
                node->pp_ptr = (void**)&attr_array[i];
                break;
            }
            node = node->next;
        }
    }
}

// === INVALIDATE FOR COMMANDS AND REPORTS ===========================

void zbm_guid_db_update_cluster_cmd_ptrs(uint16_t short_addr,
                                             uint8_t endpoint_id,
                                             uint16_t cluster_id,
                                             zbm_cluster_standart_cmd_t** cmd_array,
                                             uint8_t cmd_count)
{
    if (!cmd_array || cmd_count == 0) return;
    for (int i = 0; i < cmd_count; i++) {
        char guid[64];
        snprintf(guid, sizeof(guid), "0x%04X:%d:cmd:%04X:%02X",
                 short_addr, endpoint_id, cluster_id, cmd_array[i]->id);
        uint8_t idx = hash_guid(guid);
        guid_node_t* node = hash_table[idx];
        while (node) {
            if (strcmp(node->guid, guid) == 0) {
                node->pp_ptr = (void**)&cmd_array[i];
                break;
            }
            node = node->next;
        }
    }
}

void zbm_guid_db_update_custom_report_ptrs(uint16_t short_addr,
                                               uint8_t endpoint_id,
                                               uint16_t cluster_id,
                                               zbm_cluster_custom_report_cmd_t** report_array,
                                               uint8_t report_count)
{
    if (!report_array || report_count == 0) return;
    for (int i = 0; i < report_count; i++) {
        char guid[64];
        snprintf(guid, sizeof(guid), "0x%04X:%d:rep:%04X:%02X",
                 short_addr, endpoint_id, cluster_id, report_array[i]->id);
        uint8_t idx = hash_guid(guid);
        guid_node_t* node = hash_table[idx];
        while (node) {
            if (strcmp(node->guid, guid) == 0) {
                node->pp_ptr = (void**)&report_array[i];
                break;
            }
            node = node->next;
        }
    }
}

bool zbm_guid_db_update_device_guids(zbm_dev_t* dev)
{
    ESP_LOGI("GUID_UPDATE", "Updating GUIDs for dev %p: last=%04X, current=%04X",
         dev, dev->last_guid_update_short_addr, dev->short_addr);
    bool result = false;
    if (!dev) return result;

    

    // предотвращает множественный апдэйт хэш записей
    /*if (dev->last_guid_update_short_addr == dev->short_addr) {
        return true; // Уже актуально — ничего не делаем
    }*/

    // Удаляем все старые записи по short_addr
    result = zbm_guid_db_unregister_by_short_addr(dev->short_addr);

    for (uint8_t ep_idx = 0; ep_idx < dev->endpoints_count; ep_idx++) {
        zbm_dev_endpoint_t* ep = dev->endpoints_array[ep_idx];
        if (!ep) continue;

        // === Стандартные кластеры ===
        for (int cl_idx = 0; cl_idx < ep->standart_cluster_count; cl_idx++) {
            zbm_standart_cluster_t* cl = ep->standart_cluster_array[cl_idx];
            if (!cl) continue;

            // Перерегистрация команд
            for (int i = 0; i < cl->standart_cmd_count; i++) {
                zbm_cluster_standart_cmd_t* cmd = cl->standart_cmd_array[i];
                if (cmd) {
                    zbm_guid_db_register_cmd(
                        &cl->standart_cmd_array[i],
                        dev->short_addr,
                        ep->id,
                        cl->id,
                        cmd->id,
                        NULL
                    );
                }
            }

            // Перерегистрация атрибутов
            for (int i = 0; i < cl->attr_count; i++) {
                zbm_cluster_attribute_t* attr = cl->attr_array[i];
                if (attr) {
                    zbm_guid_db_register(
                        &cl->attr_array[i],
                        dev->short_addr,
                        ep->id,
                        cl->id,
                        attr->id,
                        NULL
                    );
                }
            }

            // ✅ Перерегистрация кастомных репортов (например, Tuya 0xFD)
            for (int i = 0; i < cl->custom_report_cmd_count; i++) {
                zbm_cluster_custom_report_cmd_t* rep = cl->custom_report_cmd_array[i];
                if (rep) {
                    zbm_guid_db_register_custom_report(
                        &cl->custom_report_cmd_array[i],
                        dev->short_addr,
                        ep->id,
                        cl->id,
                        rep->id,
                        NULL
                    );
                }
            }
        }

        // === Кастомные кластеры ===
        for (int cl_idx = 0; cl_idx < ep->custom_cluster_count; cl_idx++) {
            zbm_custom_cluster_t* cl = ep->custom_cluster_array[cl_idx];
            if (!cl) continue;

            // Атрибуты
            for (int i = 0; i < cl->attr_count; i++) {
                zbm_cluster_attribute_t* attr = cl->attr_array[i];
                if (attr) {
                    zbm_guid_db_register(
                        &cl->attr_array[i],
                        dev->short_addr,
                        ep->id,
                        cl->id,
                        attr->id,
                        NULL
                    );
                }
            }

            // Кастомные репорты
            for (int i = 0; i < cl->custom_report_cmd_count; i++) {
                zbm_cluster_custom_report_cmd_t* rep = cl->custom_report_cmd_array[i];
                if (rep) {
                    zbm_guid_db_register_custom_report(
                        &cl->custom_report_cmd_array[i],
                        dev->short_addr,
                        ep->id,
                        cl->id,
                        rep->id,
                        NULL
                    );
                }
            }
        }
    }
    dev->last_guid_update_short_addr = dev->short_addr;
    result = true;
    return result;
}

// === DEBUG: Вывод всех зарегистрированных GUID ===
void zbm_guid_db_dump_all(void)
{
    ESP_LOGI(TAG, "🔍 DUMPING ALL GUID ENTRIES (%d buckets)", ZBM_GUID_HASH_SIZE);

    int total_count = 0;
    for (int i = 0; i < ZBM_GUID_HASH_SIZE; i++) {
        guid_node_t* node = hash_table[i];
        if (!node) continue;

        ESP_LOGI(TAG, "  Bucket %d:", i);
        while (node) {
            // Попробуем определить тип по префиксу GUID
            const char* type = "unknown";
            if (strstr(node->guid, ":attr:")) type = "ATTR";
            else if (strstr(node->guid, ":cmd:")) type = "CMD ";
            else if (strstr(node->guid, ":rep:")) type = "REP ";

            void* ptr = *(node->pp_ptr);
            ESP_LOGI(TAG, "    [%s] %s → %p", type, node->guid, ptr);

            total_count++;
            node = node->next;
        }
    }

    ESP_LOGI(TAG, "🔍 TOTAL GUIDs REGISTERED: %d", total_count);
}