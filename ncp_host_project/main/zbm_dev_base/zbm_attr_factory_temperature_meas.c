// File: main/zbm_dev_base/zbm_attr_factory_temperature_meas.c
#include "zbm_attr_factory_temperature_meas.h"
#include "zbm_clusters_type.h"
#include "zbm_attr_types.h"
#include "zbm_dev_simple_func.h"

#include <string.h>
#include <stdlib.h>

static zbm_cluster_attribute_t* create_attr_measured_value(void) {
    return create_attr(0x0000, "Measured Value", ZBM_ATTR_TYPE_S16, sizeof(int16_t));
}

static zbm_cluster_attribute_t* create_attr_min_measured_value(void) {
    return create_attr(0x0001, "Min Measured Value", ZBM_ATTR_TYPE_S16, sizeof(int16_t));
}

static zbm_cluster_attribute_t* create_attr_max_measured_value(void) {
    return create_attr(0x0002, "Max Measured Value", ZBM_ATTR_TYPE_S16, sizeof(int16_t));
}

static zbm_cluster_attribute_t* create_attr_tolerance(void) {
    return create_attr(0x0003, "Tolerance", ZBM_ATTR_TYPE_U16, sizeof(uint16_t));
}

zbm_cluster_attribute_t** zbm_create_temperature_meas_attr_array(zbm_cluster_role_t role_mask, uint8_t* count)
{
    if (!count) return NULL;
    *count = 0;

    // Только сервер имеет атрибуты
    if (!(role_mask & ZBM_CLUSTER_ROLE_SERVER)) {
        return NULL;
    }
    // Всего 4 атрибута
    zbm_cluster_attribute_t** attr_array = (zbm_cluster_attribute_t**)calloc(4, sizeof(zbm_cluster_attribute_t*));
    if (!attr_array) return NULL;

    zbm_cluster_attribute_t* attrs[4] = {
        create_attr_measured_value(),
        create_attr_min_measured_value(),
        create_attr_max_measured_value(),
        create_attr_tolerance()
    };

    int valid_count = 0;
    for (int i = 0; i < 4; i++) {
        if (attrs[i]) {
            attr_array[valid_count++] = attrs[i];
        }
    }

    *count = valid_count;
    return attr_array;
}