#include "zb_manager_endpoints_gen.h"
#include "stdbool.h"
#include "esp_log.h"
#include "zb_manager.h"
#include "string.h"
static const char *TAG = "zb_manager_cluster_gen_module";
// список локальных эндпоинтов
//esp_zb_ep_list_t *zb_manager_ep_gen_list = NULL; 



static char* set_zcl_string(char *value)
{
    char* result = calloc(1,strlen(value)+1);
    result[0] = (char) strlen(value);
    memcpy(result + 1, value, result[0]);
    return result;
}
/**************************************************** BASIC CLUSTER 0x0000 ****************************************************/
static esp_err_t zb_cluster_list_add_basic_cluster_zcl8_fn(esp_zb_cluster_list_t *cluster_list, esp_zb_attribute_list_t *attr_list, uint8_t role)
{
    //ВСЁ создаётся вручную
    esp_err_t ret = ESP_FAIL;
    //static char manufacturer[64], model[64], firmware_version[64];
    if ((cluster_list == NULL)||(attr_list == NULL)) return ESP_ERR_INVALID_ARG;
    esp_zb_attribute_list_t *esp_zb_basic_cluster = (esp_zb_attribute_list_t *)attr_list;
    esp_zb_cluster_list_t *cl_list = (esp_zb_cluster_list_t *)cluster_list;
    //esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    //esp_zb_basic_cluster_cfg_t basic_cfg;
    //esp_zb_basic_cluster = esp_zb_basic_cluster_create(&basic_cfg);

    uint8_t *zcl_ver = calloc(1,sizeof(uint8_t)); *zcl_ver = 0x08;
        //esp_zb_cluster_update_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, zcl_ver); // создаётся из esp_zb_basic_cluster_cfg_t
        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, zcl_ver); // создаётся из esp_zb_basic_cluster_cfg_t
        uint8_t *app_ver = calloc(1,sizeof(uint8_t)); *app_ver = ESP_ZB_ZCL_BASIC_APPLICATION_VERSION_DEFAULT_VALUE;
        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID, app_ver);

        uint8_t *stack_ver = calloc(1,sizeof(uint8_t)); *stack_ver = ZB_STACK_SPEC_VERSION;
        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID, stack_ver);

        uint8_t *hw_ver = calloc(1,sizeof(uint8_t)); *hw_ver = CONFIG_MODEL_HW_VER_ID;
        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID, hw_ver);

        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, set_zcl_string(CONFIG_MANUFACTURER_NAME_ID));

        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, set_zcl_string(CONFIG_MODEL_NAME_ID));
       
        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID, set_zcl_string(CONFIG_MODEL_DATE_CODE));

        uint8_t *power_source = calloc(1,sizeof(uint8_t));
        #ifdef POWER_SOURCE_TYPE_UNKNOWN
            *power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_TYPE_UNKNOWN;
        #elif CONFIG_POWER_SOURCE_MAINS_SINGLE_PHASE
            *power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_MAINS_SINGLE_PHASE;
        #elif CONFIG_POWER_SOURCE_MAINS_THREE_PHASE
            *power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_MAINS_THREE_PHASE;
        #elif CONFIG_POWER_SOURCE_BATTERY
            *power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_BATTERY;
        #elif CONFIG_POWER_SOURCE_DC_SOURCE
            *power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE;
        #elif CONFIG_EMERGENCY_MAINS_CONST
            *power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_EMERGENCY_MAINS_CONST;
        #elif CONFIG_POWER_SOURCE_EMERGENCY_MAINS_TRANSF
            *power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_EMERGENCY_MAINS_TRANSF;
        
        #endif
        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, power_source);
        //esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_VERSION_DETAILS_ID, ESP_MANUFACTURER_VERSION_DETAILS);
        //esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_SERIAL_NUMBER_ID, ESP_DEVICE_SN);
        //esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_LABEL_ID, ESP_DEVICE_PRODUCT_LABEL);
        //esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID, ESP_DEVICE_LOCAL_DESCRIPTION);
        //esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_PHYSICAL_ENVIRONMENT_ID, ESP_ZB_ZCL_BASIC_PHYSICAL_ENVIRONMENT_DEFAULT_VALUE);
        bool *dev_enabled = calloc(1,sizeof(bool)); *dev_enabled = true;
        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_DEVICE_ENABLED_ID, dev_enabled);
        //esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ALARM_MASK_ID, ESP_ZB_ZCL_BASIC_ALARM_MASK_DEFAULT_VALUE);

        uint8_t *dis_local_config = calloc(1,sizeof(uint8_t)); *dis_local_config = CONFIG_DISABLE_LOCAL_CONFIG;
        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_DISABLE_LOCAL_CONFIG_ID, dis_local_config);
    
        esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, set_zcl_string(CONFIG_MODEL_SW_VER_ID));
        ret = esp_zb_cluster_list_add_basic_cluster(cl_list, esp_zb_basic_cluster, role);
        if (ret != ESP_OK) {ESP_LOGW(TAG, "esp_zb_cluster_list_add_basic_cluster failed");}
        return ret;
}

/********************************************************* ON/OFF CLUSTER 0x0006 ****************************************************/
static esp_err_t generate_on_off_cluster_zcl8_fn(esp_zb_cluster_list_t *cluster_list, esp_zb_attribute_list_t *attr_list, uint8_t role)
{
    ESP_LOGW(TAG, "server generate_on_off_cluster_zcl8 and role = %d", role);
    //ВСЁ создаётся вручную
    esp_err_t ret = ESP_FAIL;
    esp_zb_attribute_list_t *esp_zb_on_off_cluster = (esp_zb_attribute_list_t *)attr_list;
    esp_zb_cluster_list_t *cl_list = (esp_zb_cluster_list_t *)cluster_list;
    if ((cluster_list == NULL)||(attr_list == NULL)) return ESP_ERR_INVALID_ARG;

        bool *on_off_value = calloc(1,sizeof(bool)); *on_off_value = ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE;
        esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, on_off_value);
        bool *global_scene_control = calloc(1,sizeof(bool)); *global_scene_control = ESP_ZB_ZCL_ON_OFF_GLOBAL_SCENE_CONTROL_DEFAULT_VALUE;
        esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_GLOBAL_SCENE_CONTROL, global_scene_control);
        uint16_t *on_time = calloc(1,sizeof(uint16_t)); *on_time = ESP_ZB_ZCL_ON_OFF_ON_TIME_DEFAULT_VALUE;
        esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_ON_TIME, on_time);
        uint16_t *off_wait_time = calloc(1,sizeof(uint16_t)); *off_wait_time = ESP_ZB_ZCL_ON_OFF_OFF_WAIT_TIME_DEFAULT_VALUE;
        esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_OFF_WAIT_TIME, off_wait_time);
        uint8_t *start_up_on_off = calloc(1,sizeof(uint8_t)); *start_up_on_off = 0x00;
        esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF, start_up_on_off);
        ret = esp_zb_cluster_list_add_basic_cluster(cl_list, esp_zb_on_off_cluster, role);
        if (ret != ESP_OK) {ESP_LOGW(TAG, "esp_zb_cluster_list_add_on/off_cluster failed");}
        return ret;
}

typedef struct {
    uint16_t  cluster_id;
    esp_err_t (*add_cluster_fn)(esp_zb_cluster_list_t *cluster_list, esp_zb_attribute_list_t *attr_list, uint8_t role_mask);
    esp_err_t (*del_cluster_fn)(esp_zb_cluster_list_t *cluster_list, esp_zb_attribute_list_t *attr_list, uint8_t role_mask);
} esp_zb_cluster_fn_t;

static esp_zb_cluster_fn_t cluster_list_fn_table[] = {
    { ESP_ZB_ZCL_CLUSTER_ID_BASIC                      , zb_cluster_list_add_basic_cluster_zcl8_fn , NULL },
    //{ ESP_ZB_ZCL_CLUSTER_ID_BASIC                      , esp_zb_cluster_list_add_basic_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG               , esp_zb_cluster_list_add_power_config_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_DEVICE_TEMP_CONFIG         , esp_zb_cluster_list_add_custom_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY                   , esp_zb_cluster_list_add_identify_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_GROUPS                     , esp_zb_cluster_list_add_groups_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_SCENES                     , esp_zb_cluster_list_add_scenes_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_ON_OFF                     , esp_zb_cluster_list_add_on_off_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG       , esp_zb_cluster_list_add_on_off_switch_config_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL              , esp_zb_cluster_list_add_level_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_ALARMS                     , esp_zb_cluster_list_add_custom_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_TIME                       , esp_zb_cluster_list_add_time_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_RSSI_LOCATION              , esp_zb_cluster_list_add_custom_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT               , esp_zb_cluster_list_add_analog_input_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT              , esp_zb_cluster_list_add_analog_output_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE               , esp_zb_cluster_list_add_analog_value_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT               , esp_zb_cluster_list_add_binary_input_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT              , esp_zb_cluster_list_add_custom_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_BINARY_VALUE               , esp_zb_cluster_list_add_custom_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_MULTI_INPUT                , esp_zb_cluster_list_add_custom_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_MULTI_OUTPUT               , esp_zb_cluster_list_add_custom_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_MULTI_VALUE                , esp_zb_cluster_list_add_multistate_value_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_COMMISSIONING              , esp_zb_cluster_list_add_touchlink_commissioning_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_OTA_UPGRADE                , esp_zb_cluster_list_add_ota_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_POLL_CONTROL               , esp_zb_cluster_list_add_custom_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_GREEN_POWER                , esp_zb_cluster_list_add_custom_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_KEEP_ALIVE                 , esp_zb_cluster_list_add_custom_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_SHADE_CONFIG               , esp_zb_cluster_list_add_shade_config_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_DOOR_LOCK                  , esp_zb_cluster_list_add_door_lock_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING            , esp_zb_cluster_list_add_window_covering_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_PUMP_CONFIG_CONTROL        , esp_zb_cluster_list_add_custom_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT                 , esp_zb_cluster_list_add_thermostat_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL                , esp_zb_cluster_list_add_fan_control_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_DEHUMIDIFICATION_CONTROL   , esp_zb_cluster_list_add_dehumidification_control_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT_UI_CONFIG       , esp_zb_cluster_list_add_thermostat_ui_config_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL              , esp_zb_cluster_list_add_color_control_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_BALLAST_CONFIG             , esp_zb_cluster_list_add_custom_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT    , esp_zb_cluster_list_add_illuminance_meas_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT           , esp_zb_cluster_list_add_temperature_meas_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT       , esp_zb_cluster_list_add_pressure_meas_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT   , esp_zb_cluster_list_add_humidity_meas_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING          , esp_zb_cluster_list_add_occupancy_sensing_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT , esp_zb_cluster_list_add_carbon_dioxide_measurement_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_PM2_5_MEASUREMENT          , esp_zb_cluster_list_add_pm2_5_measurement_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE                   , esp_zb_cluster_list_add_ias_zone_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT     , esp_zb_cluster_list_add_electrical_meas_cluster , NULL },
    { ESP_ZB_ZCL_CLUSTER_ID_METERING                   , esp_zb_cluster_list_add_metering_cluster , NULL },
    { 0xe000                                           , esp_zb_cluster_list_add_custom_cluster , NULL },
    { 0xe001                                           , esp_zb_cluster_list_add_custom_cluster , NULL },
    { 0xef00                                           , esp_zb_cluster_list_add_custom_cluster , NULL },
};

/****************************************** zb_manager_generate_endpoint *************************************/
esp_err_t zb_manager_generate_endpoint(custom_zb_endpoint_add_str_t *ep_str, esp_zb_endpoint_config_t endpoint_config)
{

    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();

    if (ep_str->inputClusterCount > 0) {
        if(ep_str->inputClusterList){
            for (int i = 0; i < ep_str->inputClusterCount; i ++) {
            esp_zb_attribute_list_t *esp_zb_cluster = esp_zb_zcl_attr_list_create(ep_str->inputClusterList[i]);
            ESP_LOGI(TAG, "inputClusterList %02x", ep_str->inputClusterList[i]);
                for (int j = 0; j < sizeof(cluster_list_fn_table) / sizeof(cluster_list_fn_table[0]); j ++) {
                    if (ep_str->inputClusterList[i] == cluster_list_fn_table[j].cluster_id) {
                        esp_err_t func_status;
                        func_status = cluster_list_fn_table[j].add_cluster_fn(esp_zb_cluster_list, esp_zb_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
                       
                        break;
                    }
                }
            } 
        }   
    }

    if (ep_str->outputClusterCount > 0) {
        if(ep_str->outputClusterList){
            for (int i = 0; i < ep_str->outputClusterCount; i ++) {
            esp_zb_attribute_list_t *esp_zb_cluster = esp_zb_zcl_attr_list_create(ep_str->outputClusterList[i]);
            ESP_LOGI(TAG, "outputClusterList %02x", ep_str->outputClusterList[i]);
                for (int j = 0; j < sizeof(cluster_list_fn_table) / sizeof(cluster_list_fn_table[0]); j ++) {
                    if (ep_str->outputClusterList[i] == cluster_list_fn_table[j].cluster_id) {
                        esp_err_t func_status;
                        func_status = cluster_list_fn_table[j].add_cluster_fn(esp_zb_cluster_list, esp_zb_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
                        
                        break;
                    }
                }
            } 
        }   
    }
    /*if(zb_manager_ep_gen_list == NULL){
        return ESP_FAIL;
    }*/
    
    if (esp_zb_ep_list_add_ep(zb_manager_obj.esp_local_zb_ep_list, esp_zb_cluster_list, endpoint_config)!=ESP_OK){ESP_LOGW(TAG,"esp_zb_ep_list_add_ep fail");}
    return ESP_OK;
}


//***************** генерация identify кластера 0x0003 ************** ZCL8/
esp_zb_attribute_list_t *generate_identify_cluster_zcl8(esp_zb_zcl_cluster_role_t role)
{
    /* identify cluster create with fully customized */
    esp_zb_attribute_list_t *esp_zb_identify_cluster = NULL;
    esp_zb_identify_cluster_cfg_t identify_cfg;
    esp_zb_identify_cluster = esp_zb_identify_cluster_create(&identify_cfg);
    if (esp_zb_identify_cluster == NULL) return NULL;
    if (role == ESP_ZB_ZCL_CLUSTER_SERVER_ROLE)
    {
        uint16_t *identify_time = calloc(1,sizeof(uint16_t));
        if (identify_time == NULL) return NULL;
        *identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;
        esp_zb_cluster_update_attr(esp_zb_identify_cluster, ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID, identify_time);
    }
    return esp_zb_identify_cluster;
}

//***************** генерация on/off кластера 0x0006 ************** ZCL8/
esp_zb_attribute_list_t *generate_on_off_cluster_zcl8(esp_zb_zcl_cluster_role_t role)
{
    ESP_LOGW(TAG, "server generate_on_off_cluster_zcl8 and role = %d", role);
    esp_zb_attribute_list_t *esp_zb_on_off_cluster = NULL;
    esp_zb_on_off_cluster_cfg_t on_off_cfg;
    on_off_cfg.on_off = ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE;
    //esp_zb_on_off_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF);
    esp_zb_on_off_cluster = esp_zb_on_off_cluster_create(&on_off_cfg);
    if (esp_zb_on_off_cluster == NULL) return NULL;
    if (role == ESP_ZB_ZCL_CLUSTER_SERVER_ROLE)
    {
        ESP_LOGI(TAG, "server role");
        bool on_off_value = ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE;
        bool global_scene_control = ESP_ZB_ZCL_ON_OFF_GLOBAL_SCENE_CONTROL_DEFAULT_VALUE;
        //esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &on_off_value);
        //esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_GLOBAL_SCENE_CONTROL, &global_scene_control);
        //esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_ON_TIME, ESP_ZB_ZCL_ON_OFF_ON_TIME_DEFAULT_VALUE);
        //esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_OFF_WAIT_TIME, ESP_ZB_ZCL_ON_OFF_OFF_WAIT_TIME_DEFAULT_VALUE);
        //esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF, (uint8_t)0x00);
    }
    return esp_zb_on_off_cluster;
}

esp_zb_ep_list_t *generate_local_endpoints_desc_for_sample(void)
{

    /* basic cluster create with fully customized */
    esp_zb_attribute_list_t *esp_zb_basic_cluster = generate_basic_cluster_zcl8(ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_attribute_list_t *esp_zb_identify_server_cluster = generate_identify_cluster_zcl8(ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    //esp_zb_attribute_list_t *esp_zb_on_off_client_cluster = generate_on_off_cluster_zcl8(ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
    esp_zb_attribute_list_t *esp_zb_on_off_server_cluster = generate_on_off_cluster_zcl8(ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_attribute_list_t *esp_zb_identify_client_cluster = generate_identify_cluster_zcl8(ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    if (esp_zb_cluster_list == NULL) return NULL;
     esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_server_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    //esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list, esp_zb_on_off_client_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list, esp_zb_on_off_server_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_client_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
    //zb_manager_ep_gen_list = esp_zb_ep_list_create();
    //if (zb_manager_ep_gen_list == NULL) return NULL;
    //esp_zb_endpoint_config_t endpoint_config = LOCAL_REMOTE_ENDPOINT1_CONFIG();
    //esp_zb_ep_list_add_ep(zb_manager_ep_gen_list, esp_zb_cluster_list, endpoint_config);
    return zb_manager_ep_gen_list;
}