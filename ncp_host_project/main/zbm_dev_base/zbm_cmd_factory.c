#include "zbm_cmd_types.h"
#include <stdlib.h>
#include <stddef.h> 
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "zbm_clusters_type.h"
#include "zbm_attr_factory_basic.h"
#include "zbm_attr_factory_on_off.h"


bool zbm_free_cluster_cmd_param(zbm_cluster_cmd_param_t* param)
{
    if (!param) return false;

    if (param->friendlyname) {
        free(param->friendlyname);
        param->friendlyname = NULL;
    }

    if (param->p_value) {
        free(param->p_value);
        param->p_value = NULL;
    }

    free(param);
    return true;
}

bool zbm_free_cluster_standart_cmd(zbm_cluster_standart_cmd_t* cmd)
{
    if (!cmd) return false;

    if (cmd->friendlyname) {
        free(cmd->friendlyname);
        cmd->friendlyname = NULL;
    }

    if (cmd->params && cmd->param_count > 0) {
        for (int i = 0; i < cmd->param_count; i++) {
            zbm_free_cluster_cmd_param(cmd->params[i]);
            cmd->params[i] = NULL;
        }
        free(cmd->params);
        cmd->params = NULL;
    }

    free(cmd);
    return true;
}

bool zbm_free_cluster_custom_report_cmd(zbm_cluster_custom_report_cmd_t* report_cmd)
{
    if (!report_cmd) return false;

    if (report_cmd->friendlyname) {
        free(report_cmd->friendlyname);
        report_cmd->friendlyname = NULL;
    }

    if (report_cmd->p_value) {
        free(report_cmd->p_value);
        report_cmd->p_value = NULL;
    }

    free(report_cmd);
    return true;
}

// в самом конце должна быть
//=========================== Общая для стандартных команд ====================
zbm_cluster_standart_cmd_t** zbm_create_standard_command_array(uint16_t cluster_id, zbm_cluster_role_t role_mask, uint8_t* count)
{
    if (!count) {
        return NULL;
    }

    *count = 0;

    switch (cluster_id) {
        case ZBM_CLUSTER_ID_BASIC:
            return zbm_create_basic_cmd_array(role_mask, count);

        case ZBM_CLUSTER_ID_ON_OFF:
            return zbm_create_on_off_cmd_array(role_mask, count);

        // Другие стандартные кластеры можно будет добавить позже
        // case ZBM_CLUSTER_ID_LEVEL_CONTROL:
        //     return zbm_create_level_control_cmd_array(count);

        default:
            // Для неизвестных кластеров — 0 команд
            *count = 0;
            return NULL;
    }
}