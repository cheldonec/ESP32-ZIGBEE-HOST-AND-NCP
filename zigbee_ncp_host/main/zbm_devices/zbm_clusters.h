#ifndef ZBM_CLUSTERS_H

#define ZBM_CLUSTERS_H
#include "stdint.h"

typedef struct attribute_custom_s{
    uint16_t                    id;
    char                        attr_id_text[64];
    uint8_t                     type;       /*!< Attribute type see zcl_attr_type */
    uint8_t                     acces;      /*!< Attribute access options according to esp_zb_zcl_attr_access_t */
    uint16_t                    size;
    uint8_t                     is_void_pointer; /*!if (attr_type < 0x41U && attr_type > 0x51U) is_void_pointer = 0 // 0x41U - 0x51U размер в себе имеют и поэтому они будут через указатель*/
    uint16_t                    manuf_code;
    uint16_t                    parent_cluster_id;
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

#endif // ZBM_CLUSTERS_H