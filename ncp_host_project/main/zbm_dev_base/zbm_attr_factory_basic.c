// File: main/zbm_dev_base/zbm_attr_factory_basic.c
#include "zbm_attr_factory_basic.h"
#include "zbm_clusters_type.h"
#include "zbm_attr_types.h"
#include "zbm_dev_simple_func.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static zbm_cluster_attribute_t* create_attr_zcl_version(void) {
    return create_attr(0x0000, "ZCL Version", ZBM_ATTR_TYPE_U8, sizeof(uint8_t));
}

static zbm_cluster_attribute_t* create_attr_app_version(void) {
    return create_attr(0x0001, "Application Version", ZBM_ATTR_TYPE_U8, sizeof(uint8_t));
}

static zbm_cluster_attribute_t* create_attr_stack_version(void) {
    return create_attr(0x0002, "Stack Version", ZBM_ATTR_TYPE_U8, sizeof(uint8_t));
}

static zbm_cluster_attribute_t* create_attr_hw_version(void) {
    return create_attr(0x0003, "HW Version", ZBM_ATTR_TYPE_U8, sizeof(uint8_t));
}

static zbm_cluster_attribute_t* create_attr_manufacturer_name(void) {
    return create_attr(0x0004, "Manufacturer Name", ZBM_ATTR_TYPE_CHAR_STRING, 33);
}

static zbm_cluster_attribute_t* create_attr_model_id(void) {
    return create_attr(0x0005, "Model Identifier", ZBM_ATTR_TYPE_CHAR_STRING, 33);
}

static zbm_cluster_attribute_t* create_attr_date_code(void) {
    return create_attr(0x0006, "Date Code", ZBM_ATTR_TYPE_CHAR_STRING, 17);
}

static zbm_cluster_attribute_t* create_attr_power_source(void) {
    return create_attr(0x0007, "Power Source", ZBM_ATTR_TYPE_U8, sizeof(uint8_t));
}

static zbm_cluster_attribute_t* create_attr_generic_device_class(void) {
    return create_attr(0x0008, "Generic Device Class", ZBM_ATTR_TYPE_U8, sizeof(uint8_t));
}

static zbm_cluster_attribute_t* create_attr_generic_device_type(void) {
    return create_attr(0x0009, "Generic Device Type", ZBM_ATTR_TYPE_U8, sizeof(uint8_t));
}

static zbm_cluster_attribute_t* create_attr_product_code(void) {
    return create_attr(0x000A, "Product Code", ZBM_ATTR_TYPE_OCTET_STRING, 19);
}

static zbm_cluster_attribute_t* create_attr_product_url(void) {
    return create_attr(0x000B, "Product URL", ZBM_ATTR_TYPE_CHAR_STRING, 256);
}

static zbm_cluster_attribute_t* create_attr_manufacturer_version_details(void) {
    return create_attr(0x000C, "Manufacturer Version Details", ZBM_ATTR_TYPE_CHAR_STRING, 65);
}

static zbm_cluster_attribute_t* create_attr_serial_number(void) {
    return create_attr(0x000D, "Serial Number", ZBM_ATTR_TYPE_CHAR_STRING, 33);
}

static zbm_cluster_attribute_t* create_attr_product_label(void) {
    return create_attr(0x000E, "Product Label", ZBM_ATTR_TYPE_CHAR_STRING, 33);
}

static zbm_cluster_attribute_t* create_attr_location_description(void) {
    return create_attr(0x0010, "Location Description", ZBM_ATTR_TYPE_CHAR_STRING, 17);
}

static zbm_cluster_attribute_t* create_attr_physical_environment(void) {
    return create_attr(0x0011, "Physical Environment", ZBM_ATTR_TYPE_U8, sizeof(uint8_t));
}

static zbm_cluster_attribute_t* create_attr_device_enabled(void) {
    return create_attr(0x0012, "Device Enabled", ZBM_ATTR_TYPE_BOOL, sizeof(bool));
}

static zbm_cluster_attribute_t* create_attr_alarm_mask(void) {
    return create_attr(0x0013, "Alarm Mask", ZBM_ATTR_TYPE_U8, sizeof(uint8_t));
}

static zbm_cluster_attribute_t* create_attr_disable_local_config(void) {
    return create_attr(0x0014, "Disable Local Config", ZBM_ATTR_TYPE_BOOL, sizeof(bool));
}

static zbm_cluster_attribute_t* create_attr_sw_build_id(void) {
    return create_attr(0x4000, "SW Build ID", ZBM_ATTR_TYPE_CHAR_STRING, 17);
}

zbm_cluster_attribute_t** zbm_create_basic_attr_array(zbm_cluster_role_t role_mask, uint8_t* count) {
    if (!count) return NULL;
    *count = 0;

    // Только сервер имеет атрибуты
    if (!(role_mask & ZBM_CLUSTER_ROLE_SERVER)) {
        return NULL;
    }
    
    // Всего 21 стандартных атрибутов
    zbm_cluster_attribute_t** attr_array = (zbm_cluster_attribute_t**)calloc(21, sizeof(zbm_cluster_attribute_t*));
    if (!attr_array) return NULL;

    zbm_cluster_attribute_t* attrs[21] = {
        create_attr_zcl_version(),
        create_attr_app_version(),
        create_attr_stack_version(),
        create_attr_hw_version(),
        create_attr_manufacturer_name(),
        create_attr_model_id(),
        create_attr_date_code(),
        create_attr_power_source(),
        create_attr_generic_device_class(),
        create_attr_generic_device_type(),
        create_attr_product_code(),
        create_attr_product_url(),
        create_attr_manufacturer_version_details(),
        create_attr_serial_number(),
        create_attr_product_label(),
        create_attr_location_description(),
        create_attr_physical_environment(),
        create_attr_device_enabled(),
        create_attr_alarm_mask(),
        create_attr_disable_local_config(),
        create_attr_sw_build_id()
    };

    for (int i = 0; i < 21; i++) {
        if (!attrs[i]) {
            for (int j = 0; j < i; j++) {
                zbm_free_cluster_attribute(attrs[j]);
            }
            free(attr_array);
            return NULL;
        }
        attr_array[i] = attrs[i];
    }

    *count = 21;
    return attr_array;
}

//============================= Basic Cluster ================================
/**
 * @brief "Reset to Factory Defaults" для Basic кластера
 * @return Указатель на zbm_cluster_standart_cmd_t или NULL при ошибке
 */
static zbm_cluster_standart_cmd_t* zbm_create_basic_cmd_reset(void)
{
    zbm_cluster_standart_cmd_t* cmd = (zbm_cluster_standart_cmd_t*)calloc(1, sizeof(zbm_cluster_standart_cmd_t));
    if (!cmd) {
        return NULL;
    }

    cmd->id = 0x00;
    cmd->friendlyname = strdup("Reset to Factory Defaults");
    if (!cmd->friendlyname) {
        free(cmd);
        return NULL;
    }

    cmd->param_count = 0;
    cmd->params = NULL;

    return cmd;
}

zbm_cluster_standart_cmd_t** zbm_create_basic_cmd_array(zbm_cluster_role_t role_mask, uint8_t* count)
{
    if (!count) {
        return NULL;
    }

    *count = 0;

    // Только сервер имеет команды
    if (!(role_mask & ZBM_CLUSTER_ROLE_SERVER)) {
        return NULL;
    }
    // Создаём массив из одной команды
    zbm_cluster_standart_cmd_t** cmd_array = (zbm_cluster_standart_cmd_t**)calloc(1, sizeof(zbm_cluster_standart_cmd_t*));
    if (!cmd_array) {
        return NULL;
    }

    zbm_cluster_standart_cmd_t* reset_cmd = zbm_create_basic_cmd_reset();
    if (!reset_cmd) {
        free(cmd_array);
        return NULL;
    }

    cmd_array[0] = reset_cmd;
    *count = 1;

    return cmd_array;
}