#ifndef ZBM_CLUSTERS_H

#define ZBM_CLUSTERS_H
#include "stdint.h"
#include "esp_err.h"
#include "cJSON.h"

typedef struct attribute_custom_s{
    uint16_t                    id;
    char                        attr_id_text[64];
    uint8_t                     type;       /*!< Attribute type see zcl_attr_type */
    uint8_t                     acces;      /*!< Attribute access options according to esp_zb_zcl_attr_access_t */
    uint16_t                    size;
    uint8_t                     is_void_pointer; /*!if (attr_type < 0x41U && attr_type > 0x51U) is_void_pointer = 0 // 0x41U - 0x51U размер в себе имеют и поэтому они будут через указатель*/
    uint16_t                    manuf_code;
    uint16_t                    parent_cluster_id;
    uint64_t last_update_ms;
    void*                       p_value;
}attribute_custom_t;


typedef struct cluster_custom_s{
    uint16_t                    id;
    char                        cluster_id_text[64];
    uint8_t                     is_use_on_device;         // 0 no 1-yes
    uint8_t                     role_mask; // esp_zb_zcl_cluster_role_t;
    uint16_t                    manuf_code;
    uint16_t                    attr_count;
    attribute_custom_t**        attr_array;
    // Надо добавить специфичные команды
}cluster_custom_t;

esp_err_t zbm_cluster_add_custom_attribute(cluster_custom_t *cluster,uint16_t attr_id, uint8_t attr_type);

esp_err_t zbm_cluster_remove_custom_attribute(cluster_custom_t *cluster, uint16_t attr_id);
void zbm_cluster_free_all_attributes(cluster_custom_t *cluster);

void zbm_update_custom_attribute_value(attribute_custom_t *attr, const uint8_t *new_value, uint16_t value_len);

attribute_custom_t* zbm_cluster_find_custom_attribute(cluster_custom_t *cluster, uint16_t attr_id);

void  zbm_json_add_attribute_value(cJSON *attr_obj, attribute_custom_t *attr);

void  zbm_json_load_attribute_value(attribute_custom_t *attr, cJSON *attr_obj);

/**
 * @brief Convert unknown/custom cluster to cJSON
 * @param cluster Pointer to cluster_custom_t
 * @return cJSON* - new JSON object, or NULL
 */
cJSON* zbm_unknown_cluster_to_json(cluster_custom_t *cluster);

#endif // ZBM_CLUSTERS_H