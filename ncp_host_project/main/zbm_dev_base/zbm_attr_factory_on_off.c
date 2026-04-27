// File: main/zbm_dev_base/zbm_attr_factory_on_off.c
#include "zbm_attr_factory_on_off.h"
#include "zbm_clusters_type.h"
#include <string.h>
#include <stdlib.h>
#include "zbm_dev_simple_func.h"
#include "zbm_cmd_types.h"
#include "ps_ram_utils.h"

/**
 * @brief Создаёт атрибут OnOff для On/Off кластера
 */
static zbm_cluster_attribute_t* create_attr_onoff(void) {
    return create_attr(0x0000, "OnOff", ZBM_ATTR_TYPE_BOOL, sizeof(bool));
}

/**
 * @brief Создаёт атрибут GlobalSceneControl
 */
static zbm_cluster_attribute_t* create_attr_global_scene_control(void) {
    return create_attr(0x4000, "Global Scene Control", ZBM_ATTR_TYPE_BOOL, sizeof(bool));
}

/**
 * @brief Создаёт атрибут OnTime
 */
static zbm_cluster_attribute_t* create_attr_on_time(void) {
    return create_attr(0x4001, "On Time", ZBM_ATTR_TYPE_U16, sizeof(uint16_t));
}

/**
 * @brief Создаёт атрибут OffWaitTime
 */
static zbm_cluster_attribute_t* create_attr_off_wait_time(void) {
    return create_attr(0x4002, "Off Wait Time", ZBM_ATTR_TYPE_U16, sizeof(uint16_t));
}

/**
 * @brief Создаёт атрибут StartUpOnOff
 */
static zbm_cluster_attribute_t* create_attr_startup_onoff(void) {
    return create_attr(0x4003, "StartUp OnOff", ZBM_ATTR_TYPE_T8BIT_ENUM, sizeof(uint8_t));
}

/**
 * @brief Создаёт массив стандартных атрибутов для On/Off кластера
 */
zbm_cluster_attribute_t** zbm_create_on_off_attr_array(zbm_cluster_role_t role_mask, uint8_t* count) {
    if (!count) return NULL;
    *count = 0;

    // Только сервер имеет атрибуты
    if (!(role_mask & ZBM_CLUSTER_ROLE_SERVER)) {
        return NULL;
    }

    zbm_cluster_attribute_t** attr_array = calloc(5, sizeof(zbm_cluster_attribute_t*));
    if (!attr_array) return NULL;

    zbm_cluster_attribute_t* attrs[5] = {
        create_attr_onoff(),
        create_attr_global_scene_control(),
        create_attr_on_time(),
        create_attr_off_wait_time(),
        create_attr_startup_onoff()
    };

    for (int i = 0; i < 5; i++) {
        if (!attrs[i]) {
            for (int j = 0; j < i; j++) zbm_free_cluster_attribute(attrs[j]);
            free(attr_array);
            return NULL;
        }
        attr_array[i] = attrs[i];
    }

    *count = 5;
    return attr_array;
}

//============================= ON/OFF Cluster CMD ================================
/**
 * @brief Создаёт команду "Off" для On/Off кластера
 * @return Указатель на zbm_cluster_standart_cmd_t или NULL при ошибке
 */
static zbm_cluster_standart_cmd_t* zbm_create_on_off_cmd_off(void)
{
    zbm_cluster_standart_cmd_t* cmd = (zbm_cluster_standart_cmd_t*)calloc(1, sizeof(zbm_cluster_standart_cmd_t));
    if (!cmd) {
        return NULL;
    }

    cmd->id = 0x00;
    cmd->friendlyname = psram_strdup("Off");
    if (!cmd->friendlyname) {
        free(cmd);
        return NULL;
    }

    cmd->param_count = 0;
    cmd->params = NULL;

    return cmd;
}

/**
 * @brief Создаёт команду "On" для On/Off кластера
 * @return Указатель на zbm_cluster_standart_cmd_t или NULL при ошибке
 */
static zbm_cluster_standart_cmd_t* zbm_create_on_off_cmd_on(void)
{
    zbm_cluster_standart_cmd_t* cmd = (zbm_cluster_standart_cmd_t*)calloc(1, sizeof(zbm_cluster_standart_cmd_t));
    if (!cmd) {
        return NULL;
    }

    cmd->id = 0x01;
    cmd->friendlyname = psram_strdup("On");
    if (!cmd->friendlyname) {
        free(cmd);
        return NULL;
    }

    cmd->param_count = 0;
    cmd->params = NULL;

    return cmd;
}

/**
 * @brief Создаёт команду "Toggle" для On/Off кластера
 * @return Указатель на zbm_cluster_standart_cmd_t или NULL при ошибке
 */
static zbm_cluster_standart_cmd_t* zbm_create_on_off_cmd_toggle(void)
{
    zbm_cluster_standart_cmd_t* cmd = (zbm_cluster_standart_cmd_t*)calloc(1, sizeof(zbm_cluster_standart_cmd_t));
    if (!cmd) {
        return NULL;
    }

    cmd->id = 0x02;
    cmd->friendlyname = psram_strdup("Toggle");
    if (!cmd->friendlyname) {
        free(cmd);
        return NULL;
    }

    cmd->param_count = 0;
    cmd->params = NULL;

    return cmd;
}

/**
 * @brief Создаёт команду "On With Timed Off" для On/Off кластера
 * Параметры:
 * - OnOffControl: uint8_t
 * - OnTime: uint16_t (длительность включения, в десятых долях секунды)
 * - OffWaitTime: uint16_t (ожидание перед выключением, в десятых долях секунды)
 * @return Указатель на zbm_cluster_standart_cmd_t или NULL при ошибке
 */
static zbm_cluster_standart_cmd_t* zbm_create_on_off_cmd_on_with_timed_off(void)
{
    zbm_cluster_standart_cmd_t* cmd = (zbm_cluster_standart_cmd_t*)calloc(1, sizeof(zbm_cluster_standart_cmd_t));
    if (!cmd) {
        return NULL;
    }

    cmd->id = 0x42;
    cmd->friendlyname = psram_strdup("On With Timed Off");
    if (!cmd->friendlyname) {
        free(cmd);
        return NULL;
    }

    cmd->param_count = 3;
    cmd->params = (zbm_cluster_cmd_param_t**)calloc(3, sizeof(zbm_cluster_cmd_param_t*));
    if (!cmd->params) {
        free(cmd->friendlyname);
        free(cmd);
        return NULL;
    }

    // Параметр 1: OnOffControl (uint8_t)
    zbm_cluster_cmd_param_t* param1 = (zbm_cluster_cmd_param_t*)calloc(1, sizeof(zbm_cluster_cmd_param_t));
    if (!param1) goto error;
    param1->friendlyname = psram_strdup("OnOff Control");
    param1->data_type = ZBM_CMD_DATA_TYPE_U8;
    param1->data_size = sizeof(uint8_t);
    param1->p_value = calloc(1, sizeof(uint8_t));
    if (!param1->friendlyname || !param1->p_value) {
        zbm_free_cluster_cmd_param(param1);
        goto error;
    }
    cmd->params[0] = param1;

    // Параметр 2: OnTime (uint16_t)
    zbm_cluster_cmd_param_t* param2 = (zbm_cluster_cmd_param_t*)calloc(1, sizeof(zbm_cluster_cmd_param_t));
    if (!param2) goto error;
    param2->friendlyname = psram_strdup("On Time (0.1s)");
    param2->data_type = ZBM_CMD_DATA_TYPE_U16;
    param2->data_size = sizeof(uint16_t);
    param2->p_value = calloc(1, sizeof(uint16_t));
    if (!param2->friendlyname || !param2->p_value) {
        zbm_free_cluster_cmd_param(param2);
        goto error;
    }
    cmd->params[1] = param2;

    // Параметр 3: OffWaitTime (uint16_t)
    zbm_cluster_cmd_param_t* param3 = (zbm_cluster_cmd_param_t*)calloc(1, sizeof(zbm_cluster_cmd_param_t));
    if (!param3) goto error;
    param3->friendlyname = psram_strdup("Off Wait Time (0.1s)");
    param3->data_type = ZBM_CMD_DATA_TYPE_U16;
    param3->data_size = sizeof(uint16_t);
    param3->p_value = calloc(1, sizeof(uint16_t));
    if (!param3->friendlyname || !param3->p_value) {
        zbm_free_cluster_cmd_param(param3);
        goto error;
    }
    cmd->params[2] = param3;

    return cmd;

error:
    // Очистка при ошибке
    if (cmd->params) {
        for (int i = 0; i < 3; i++) {
            if (cmd->params[i]) {
                zbm_free_cluster_cmd_param(cmd->params[i]);
            }
        }
        free(cmd->params);
    }
    if (cmd->friendlyname) free(cmd->friendlyname);
    free(cmd);
    return NULL;
}

zbm_cluster_standart_cmd_t** zbm_create_on_off_cmd_array(zbm_cluster_role_t role_mask, uint8_t* count)
{
    if (!count) {
        return NULL;
    }

    *count = 0;

    // Только сервер имеет команды
    if (!(role_mask & ZBM_CLUSTER_ROLE_SERVER)) {
        return NULL;
    }
    // Теперь 4 команды
    zbm_cluster_standart_cmd_t** cmd_array = (zbm_cluster_standart_cmd_t**)calloc(4, sizeof(zbm_cluster_standart_cmd_t*));
    if (!cmd_array) {
        return NULL;
    }

    zbm_cluster_standart_cmd_t* off_cmd = zbm_create_on_off_cmd_off();
    zbm_cluster_standart_cmd_t* on_cmd = zbm_create_on_off_cmd_on();
    zbm_cluster_standart_cmd_t* toggle_cmd = zbm_create_on_off_cmd_toggle();
    zbm_cluster_standart_cmd_t* timed_on_cmd = zbm_create_on_off_cmd_on_with_timed_off();

    if (!off_cmd || !on_cmd || !toggle_cmd || !timed_on_cmd) {
        // Очистка
        if (off_cmd) free(off_cmd->friendlyname), free(off_cmd);
        if (on_cmd) free(on_cmd->friendlyname), free(on_cmd);
        if (toggle_cmd) free(toggle_cmd->friendlyname), free(toggle_cmd);
        if (timed_on_cmd) {
            for (int i = 0; i < 3; i++) {
                if (timed_on_cmd->params && timed_on_cmd->params[i]) {
                    zbm_free_cluster_cmd_param(timed_on_cmd->params[i]);
                }
            }
            if (timed_on_cmd->params) free(timed_on_cmd->params);
            if (timed_on_cmd->friendlyname) free(timed_on_cmd->friendlyname);
            free(timed_on_cmd);
        }
        free(cmd_array);
        return NULL;
    }

    cmd_array[0] = off_cmd;
    cmd_array[1] = on_cmd;
    cmd_array[2] = toggle_cmd;
    cmd_array[3] = timed_on_cmd;
    *count = 4;

    return cmd_array;
}
