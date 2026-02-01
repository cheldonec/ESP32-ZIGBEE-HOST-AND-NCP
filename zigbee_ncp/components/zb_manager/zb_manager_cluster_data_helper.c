#include "zb_manager_cluster_data_helper.h"
#include "zb_manager_cluster_data_helper.h"
#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_power_config.h"


typedef struct {
    uint16_t cluster_id;
    const char *cluster_name;
} zb_cluster_entry_t;

static const zb_cluster_entry_t cluster_table[] = {
    { ESP_ZB_ZCL_CLUSTER_ID_BASIC, "Basic" },
    { ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, "Power Config" },
    { ESP_ZB_ZCL_CLUSTER_ID_DEVICE_TEMP_CONFIG, "Device Temp Config" },
    { ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY, "Identify" },
    { ESP_ZB_ZCL_CLUSTER_ID_GROUPS, "Groups" },
    { ESP_ZB_ZCL_CLUSTER_ID_SCENES, "Scenes" },
    { ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, "On/Off" },
    { ESP_ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG, "On/Off Switch Config" },
    { ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, "Level Control" },
    { ESP_ZB_ZCL_CLUSTER_ID_ALARMS, "Alarms" },
    { ESP_ZB_ZCL_CLUSTER_ID_TIME, "Time" },
    { ESP_ZB_ZCL_CLUSTER_ID_RSSI_LOCATION, "RSSI Location" },
    { ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, "Analog Input" },
    { ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, "Analog Output" },
    { ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE, "Analog Value" },
    { ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT, "Binary Input" },
    { ESP_ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT, "Binary Output" },
    { ESP_ZB_ZCL_CLUSTER_ID_BINARY_VALUE, "Binary Value" },
    { ESP_ZB_ZCL_CLUSTER_ID_MULTI_INPUT, "Multi Input" },
    { ESP_ZB_ZCL_CLUSTER_ID_MULTI_OUTPUT, "Multi Output" },
    { ESP_ZB_ZCL_CLUSTER_ID_MULTI_VALUE, "Multi Value" },
    { ESP_ZB_ZCL_CLUSTER_ID_COMMISSIONING, "Commissioning" },
    { ESP_ZB_ZCL_CLUSTER_ID_OTA_UPGRADE, "OTA Upgrade" },
    { ESP_ZB_ZCL_CLUSTER_ID_POLL_CONTROL, "Poll Control" },
    { ESP_ZB_ZCL_CLUSTER_ID_GREEN_POWER, "Green Power" },
    { ESP_ZB_ZCL_CLUSTER_ID_KEEP_ALIVE, "Keep Alive" },
    { ESP_ZB_ZCL_CLUSTER_ID_SHADE_CONFIG, "Shade Config" },
    { ESP_ZB_ZCL_CLUSTER_ID_DOOR_LOCK, "Door Lock" },
    { ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, "Window Covering" },
    { ESP_ZB_ZCL_CLUSTER_ID_PUMP_CONFIG_CONTROL, "Pump Config Control" },
    { ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, "Thermostat" },
    { ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL, "Fan Control" },
    { ESP_ZB_ZCL_CLUSTER_ID_DEHUMIDIFICATION_CONTROL, "Dehumidification Control" },
    { ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT_UI_CONFIG, "Thermostat UI Config" },
    { ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, "Color Control" },
    { ESP_ZB_ZCL_CLUSTER_ID_BALLAST_CONFIG, "Ballast Config" },
    { ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT, "Illuminance Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, "Temperature Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, "Pressure Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT, "Flow Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, "Relative Humidity Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, "Occupancy Sensing" },
    { ESP_ZB_ZCL_CLUSTER_ID_PH_MEASUREMENT, "pH Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_EC_MEASUREMENT, "Electrical Conductivity Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_WIND_SPEED_MEASUREMENT, "Wind Speed Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, "Carbon Dioxide Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_PM2_5_MEASUREMENT, "PM2.5 Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, "IAS Zone" },
    { ESP_ZB_ZCL_CLUSTER_ID_IAS_ACE, "IAS ACE" },
    { ESP_ZB_ZCL_CLUSTER_ID_IAS_WD, "IAS WD" },
    { ESP_ZB_ZCL_CLUSTER_ID_PRICE, "Price" },
    { ESP_ZB_ZCL_CLUSTER_ID_DRLC, "DRLC" },
    { ESP_ZB_ZCL_CLUSTER_ID_METERING, "Metering" },
    { ESP_ZB_ZCL_CLUSTER_ID_METER_IDENTIFICATION, "Meter Identification" },
    { ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, "Electrical Measurement" },
    { ESP_ZB_ZCL_CLUSTER_ID_DIAGNOSTICS, "Diagnostics" },
};

/*****************************************************
uint16_t cluster_id = 0x0006; // On/Off cluster
const char* name = zb_manager_get_cluster_name(cluster_id);
printf("Cluster Name: %s\n", name); // Output: "On/Off"
 */
const char* zb_manager_get_cluster_name(uint16_t clusterID) {
    for (size_t i = 0; i < sizeof(cluster_table) / sizeof(cluster_table[0]); i++) {
        if (cluster_table[i].cluster_id == clusterID) {
            return cluster_table[i].cluster_name;
        }
    }
    return "Unknown Cluster";
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID;
const char* name = zb_manager_get_basic_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Power Source"*/
const char* zb_manager_get_basic_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID:
            return "ZCL Version";
        case ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID:
            return "Application Version";
        case ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID:
            return "Stack Version";
        case ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID:
            return "Hardware Version";
        case ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID:
            return "Manufacturer Name";
        case ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID:
            return "Model Identifier";
        case ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID:
            return "Date Code";
        case ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID:
            return "Power Source";
        case ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_CLASS_ID:
            return "Generic Device Class";
        case ESP_ZB_ZCL_ATTR_BASIC_GENERIC_DEVICE_TYPE_ID:
            return "Generic Device Type";
        case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_CODE_ID:
            return "Product Code";
        case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_URL_ID:
            return "Product URL";
        case ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_VERSION_DETAILS_ID:
            return "Manufacturer Version Details";
        case ESP_ZB_ZCL_ATTR_BASIC_SERIAL_NUMBER_ID:
            return "Serial Number";
        case ESP_ZB_ZCL_ATTR_BASIC_PRODUCT_LABEL_ID:
            return "Product Label";
        case ESP_ZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID:
            return "Location Description";
        case ESP_ZB_ZCL_ATTR_BASIC_PHYSICAL_ENVIRONMENT_ID:
            return "Physical Environment";
        case ESP_ZB_ZCL_ATTR_BASIC_DEVICE_ENABLED_ID:
            return "Device Enabled";
        case ESP_ZB_ZCL_ATTR_BASIC_ALARM_MASK_ID:
            return "Alarm Mask";
        case ESP_ZB_ZCL_ATTR_BASIC_DISABLE_LOCAL_CONFIG_ID:
            return "Disable Local Config";
        case ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID:
            return "SW Build";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_ALARM_MASK_ID;
const char* name = zb_manager_get_power_config_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Battery Alarm Mask"
*/
const char* zb_manager_get_power_config_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        // Mains Attributes
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_VOLTAGE_ID:
            return "Mains Voltage";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_FREQUENCY_ID:
            return "Mains Frequency";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_ALARM_MASK_ID:
            return "Mains Alarm Mask";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_VOLTAGE_MIN_THRESHOLD:
            return "Mains Voltage Min Threshold";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_VOLTAGE_MAX_THRESHOLD:
            return "Mains Voltage Max Threshold";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_MAINS_DWELL_TRIP_POINT:
            return "Mains Dwell Trip Point";

        // Battery 1 Attributes
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID:
            return "Battery Voltage";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID:
            return "Battery Percentage Remaining";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_MANUFACTURER_ID:
            return "Battery Manufacturer";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_SIZE_ID:
            return "Battery Size";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_A_HR_RATING_ID:
            return "Battery AH Rating";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_QUANTITY_ID:
            return "Battery Quantity";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_RATED_VOLTAGE_ID:
            return "Battery Rated Voltage";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_ALARM_MASK_ID:
            return "Battery Alarm Mask";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_MIN_THRESHOLD_ID:
            return "Battery Voltage Min Threshold";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD1_ID:
            return "Battery Voltage Threshold 1";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD2_ID:
            return "Battery Voltage Threshold 2";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_THRESHOLD3_ID:
            return "Battery Voltage Threshold 3";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_MIN_THRESHOLD_ID:
            return "Battery Percentage Min Threshold";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD1_ID:
            return "Battery Percentage Threshold 1";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD2_ID:
            return "Battery Percentage Threshold 2";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_THRESHOLD3_ID:
            return "Battery Percentage Threshold 3";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_ALARM_STATE_ID:
            return "Battery Alarm State";

        // Battery 2 Attributes
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_ID:
            return "Battery 2 Voltage";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_REMAINING_ID:
            return "Battery 2 Percentage Remaining";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_MANUFACTURER_ID:
            return "Battery 2 Manufacturer";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_SIZE_ID:
            return "Battery 2 Size";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_A_HR_RATING_ID:
            return "Battery 2 AH Rating";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_QUANTITY_ID:
            return "Battery 2 Quantity";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_RATED_VOLTAGE_ID:
            return "Battery 2 Rated Voltage";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_ALARM_MASK_ID:
            return "Battery 2 Alarm Mask";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_MIN_THRESHOLD_ID:
            return "Battery 2 Voltage Min Threshold";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_THRESHOLD1_ID:
            return "Battery 2 Voltage Threshold 1";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_THRESHOLD2_ID:
            return "Battery 2 Voltage Threshold 2";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_VOLTAGE_THRESHOLD3_ID:
            return "Battery 2 Voltage Threshold 3";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_MIN_THRESHOLD_ID:
            return "Battery 2 Percentage Min Threshold";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_THRESHOLD1_ID:
            return "Battery 2 Percentage Threshold 1";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_THRESHOLD2_ID:
            return "Battery 2 Percentage Threshold 2";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_PERCENTAGE_THRESHOLD3_ID:
            return "Battery 2 Percentage Threshold 3";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY2_ALARM_STATE_ID:
            return "Battery 2 Alarm State";

        // Battery 3 Attributes
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_ID:
            return "Battery 3 Voltage";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_REMAINING_ID:
            return "Battery 3 Percentage Remaining";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_MANUFACTURER_ID:
            return "Battery 3 Manufacturer";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_SIZE_ID:
            return "Battery 3 Size";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_A_HR_RATING_ID:
            return "Battery 3 AH Rating";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_QUANTITY_ID:
            return "Battery 3 Quantity";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_RATED_VOLTAGE_ID:
            return "Battery 3 Rated Voltage";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_ALARM_MASK_ID:
            return "Battery 3 Alarm Mask";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_MIN_THRESHOLD_ID:
            return "Battery 3 Voltage Min Threshold";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_THRESHOLD1_ID:
            return "Battery 3 Voltage Threshold 1";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_THRESHOLD2_ID:
            return "Battery 3 Voltage Threshold 2";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_VOLTAGE_THRESHOLD3_ID:
            return "Battery 3 Voltage Threshold 3";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_MIN_THRESHOLD_ID:
            return "Battery 3 Percentage Min Threshold";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_THRESHOLD1_ID:
            return "Battery 3 Percentage Threshold 1";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_THRESHOLD2_ID:
            return "Battery 3 Percentage Threshold 2";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_PERCENTAGE_THRESHOLD3_ID:
            return "Battery 3 Percentage Threshold 3";
        case ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY3_ALARM_STATE_ID:
            return "Battery 3 Alarm State";

        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_DEVICE_TEMP_CONFIG_HIGH_TEMP_THRESHOLD_ID;
const char* name = zb_manager_get_device_temp_config_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "High Temperature Threshold"
*/
const char* zb_manager_get_device_temp_config_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_DEVICE_TEMP_CONFIG_CURRENT_TEMP_ID:
            return "Current Temperature";
        case ESP_ZB_ZCL_ATTR_DEVICE_TEMP_CONFIG_MIN_TEMP_EXPERIENCED_ID:
            return "Minimum Temperature Experienced";
        case ESP_ZB_ZCL_ATTR_DEVICE_TEMP_CONFIG_MAX_TEMP_EXPERIENCED_ID:
            return "Maximum Temperature Experienced";
        case ESP_ZB_ZCL_ATTR_DEVICE_TEMP_CONFIG_OVER_TEMP_TOTAL_DWELL_ID:
            return "Over Temperature Total Dwell";
        case ESP_ZB_ZCL_ATTR_DEVICE_TEMP_CONFIG_DEVICE_TEMP_ALARM_MASK_ID:
            return "Device Temperature Alarm Mask";
        case ESP_ZB_ZCL_ATTR_DEVICE_TEMP_CONFIG_LOW_TEMP_THRESHOLD_ID:
            return "Low Temperature Threshold";
        case ESP_ZB_ZCL_ATTR_DEVICE_TEMP_CONFIG_HIGH_TEMP_THRESHOLD_ID:
            return "High Temperature Threshold";
        case ESP_ZB_ZCL_ATTR_DEVICE_TEMP_CONFIG_LOW_TEMP_DWELL_TRIP_POINT_ID:
            return "Low Temperature Dwell Trip Point";
        case ESP_ZB_ZCL_ATTR_DEVICE_TEMP_CONFIG_HIGH_TEMP_DWELL_TRIP_POINT_ID:
            return "High Temperature Dwell Trip Point";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID;
const char* attr_name = zb_manager_get_identify_cluster_attr_name(attr_id);
printf("Attribute: %s\n", attr_name); // Output: "Identify Time"
*/
const char* zb_manager_get_identify_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID:
            return "Identify Time";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_GROUPS_NAME_SUPPORT_ID;
const char* name = zb_manager_get_groups_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Name Support"
*/
const char* zb_manager_get_groups_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_GROUPS_NAME_SUPPORT_ID:
            return "Name Support";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_SCENES_CURRENT_GROUP_ID;
const char* name = zb_manager_get_scenes_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Current Group"
*/
const char* zb_manager_get_scenes_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_SCENES_SCENE_COUNT_ID:
            return "Scene Count";
        case ESP_ZB_ZCL_ATTR_SCENES_CURRENT_SCENE_ID:
            return "Current Scene";
        case ESP_ZB_ZCL_ATTR_SCENES_CURRENT_GROUP_ID:
            return "Current Group";
        case ESP_ZB_ZCL_ATTR_SCENES_SCENE_VALID_ID:
            return "Scene Valid";
        case ESP_ZB_ZCL_ATTR_SCENES_NAME_SUPPORT_ID:
            return "Name Support";
        case ESP_ZB_ZCL_ATTR_SCENES_LAST_CONFIGURED_BY_ID:
            return "Last Configured By";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF;
const char* name = zb_manager_get_on_off_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Start Up On/Off"
*/
const char* zb_manager_get_on_off_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID:
            return "On/Off";
        case ESP_ZB_ZCL_ATTR_ON_OFF_GLOBAL_SCENE_CONTROL:
            return "Global Scene Control";
        case ESP_ZB_ZCL_ATTR_ON_OFF_ON_TIME:
            return "On Time";
        case ESP_ZB_ZCL_ATTR_ON_OFF_OFF_WAIT_TIME:
            return "Off Wait Time";
        case ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF:
            return "Start Up On/Off";
        default:
            return "Unknown Attribute";
    }
}

const char* zb_manager_get_on_off_switch_type_attr_name(uint16_t attrID) {
    switch (attrID) {
        //case ESP_ZB_ZCL_ON_OFF_SWITCH_CONFIGURATION_SWITCH_TYPE_TOGGLE:
        case 0x0000:
            return "Type Toggle Switch";
        //case ESP_ZB_ZCL_ON_OFF_SWITCH_CONFIGURATION_SWITCH_TYPE_MOMENTARY:
        case 0x0001:
            return "Type Momentary Switch";
        //case ESP_ZB_ZCL_ON_OFF_SWITCH_CONFIGURATION_SWITCH_TYPE_MULTIFUNCTION:
        case 0x0002:
            return "Type Multifunction Switch";
        //case ESP_ZB_ZCL_ON_OFF_SWITCH_CONFIGURATION_SWITCH_ACTIONS_TYPE1:
        case 0x0010:
            return "Actions Type 1 Command Pattern";
        //case ESP_ZB_ZCL_ON_OFF_SWITCH_CONFIGURATION_SWITCH_ACTIONS_TYPE2:
        case 0x0011:
            return "Actions Type 2 Command Pattern";
        //case ESP_ZB_ZCL_ON_OFF_SWITCH_CONFIGURATION_SWITCH_ACTIONS_TOGGLE:
        case 0x0012:
            return "Actions Toggle Command Pattern";
        default:
            return "Unknown Switch Type";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_ON_LEVEL_ID;
const char* name = zb_manager_get_level_control_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "On Level"
*/
const char* zb_manager_get_level_control_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID:
            return "Current Level";
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_REMAINING_TIME_ID:
            return "Remaining Time";
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_MIN_LEVEL_ID:
            return "Min Level";
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_MAX_LEVEL_ID:
            return "Max Level";
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_FREQUENCY_ID:
            return "Current Frequency";
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_MIN_FREQUENCY_ID:
            return "Min Frequency";
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_MAX_FREQUENCY_ID:
            return "Max Frequency";
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_ON_OFF_TRANSITION_TIME_ID:
            return "On/Off Transition Time";
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_ON_LEVEL_ID:
            return "On Level";
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_ON_TRANSITION_TIME_ID:
            return "On Transition Time";
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_OFF_TRANSITION_TIME_ID:
            return "Off Transition Time";
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_DEFAULT_MOVE_RATE_ID:
            return "Default Move Rate";
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_OPTIONS_ID:
            return "Options";
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_START_UP_CURRENT_LEVEL_ID:
            return "Start Up Current Level";
        case ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_MOVE_STATUS_ID:
            return "Move Status (Internal)";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_ALARMS_ALARM_COUNT_ID;
const char* name = zb_manager_get_alarms_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Alarm Count"
*/
const char* zb_manager_get_alarms_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_ALARMS_ALARM_COUNT_ID:
            return "Alarm Count";
        case ESP_ZB_ZCL_ATTR_ALARMS_ALARM_TABLE_SIZE_ID:
            return "Alarm Table Size (Internal)";
        case ESP_ZB_ZCL_ATTR_ALARMS_ALARM_TABLE_ID:
            return "Alarm Table (Internal)";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_TIME_LOCAL_TIME_ID;
const char* name = zb_manager_get_time_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Local Time"
*/
const char* zb_manager_get_time_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_TIME_TIME_ID:
            return "Time";
        case ESP_ZB_ZCL_ATTR_TIME_TIME_STATUS_ID:
            return "Time Status";
        case ESP_ZB_ZCL_ATTR_TIME_TIME_ZONE_ID:
            return "Time Zone";
        case ESP_ZB_ZCL_ATTR_TIME_DST_START_ID:
            return "DST Start";
        case ESP_ZB_ZCL_ATTR_TIME_DST_END_ID:
            return "DST End";
        case ESP_ZB_ZCL_ATTR_TIME_DST_SHIFT_ID:
            return "DST Shift";
        case ESP_ZB_ZCL_ATTR_TIME_STANDARD_TIME_ID:
            return "Standard Time";
        case ESP_ZB_ZCL_ATTR_TIME_LOCAL_TIME_ID:
            return "Local Time";
        case ESP_ZB_ZCL_ATTR_TIME_LAST_SET_TIME_ID:
            return "Last Set Time";
        case ESP_ZB_ZCL_ATTR_TIME_VALID_UNTIL_TIME_ID:
            return "Valid Until Time";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID;
const char* name = zb_manager_get_analog_input_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Present Value"
*/
const char* zb_manager_get_analog_input_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_ANALOG_INPUT_DESCRIPTION_ID:
            return "Description";
        case ESP_ZB_ZCL_ATTR_ANALOG_INPUT_MAX_PRESENT_VALUE_ID:
            return "Max Present Value";
        case ESP_ZB_ZCL_ATTR_ANALOG_INPUT_MIN_PRESENT_VALUE_ID:
            return "Min Present Value";
        case ESP_ZB_ZCL_ATTR_ANALOG_INPUT_OUT_OF_SERVICE_ID:
            return "Out Of Service";
        case ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID:
            return "Present Value";
        case ESP_ZB_ZCL_ATTR_ANALOG_INPUT_RELIABILITY_ID:
            return "Reliability";
        case ESP_ZB_ZCL_ATTR_ANALOG_INPUT_RESOLUTION_ID:
            return "Resolution";
        case ESP_ZB_ZCL_ATTR_ANALOG_INPUT_STATUS_FLAGS_ID:
            return "Status Flags";
        case ESP_ZB_ZCL_ATTR_ANALOG_INPUT_ENGINEERING_UNITS_ID:
            return "Engineering Units";
        case ESP_ZB_ZCL_ATTR_ANALOG_INPUT_APPLICATION_TYPE_ID:
            return "Application Type";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_PRIORITY_ARRAY_ID;
const char* name = zb_manager_get_analog_output_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Priority Array"
*/
const char* zb_manager_get_analog_output_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_DESCRIPTION_ID:
            return "Description";
        case ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_MAX_PRESENT_VALUE_ID:
            return "Max Present Value";
        case ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_MIN_PRESENT_VALUE_ID:
            return "Min Present Value";
        case ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_OUT_OF_SERVICE_ID:
            return "Out Of Service";
        case ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID:
            return "Present Value";
        case ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_PRIORITY_ARRAY_ID:
            return "Priority Array";
        case ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_RELIABILITY_ID:
            return "Reliability";
        case ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_RELINQUISH_DEFAULT_ID:
            return "Relinquish Default";
        case ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_RESOLUTION_ID:
            return "Resolution";
        case ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_STATUS_FLAGS_ID:
            return "Status Flags";
        case ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_ENGINEERING_UNITS_ID:
            return "Engineering Units";
        case ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_APPLICATION_TYPE_ID:
            return "Application Type";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_BINARY_INPUT_POLARITY_ID;
const char* name = zb_manager_get_binary_input_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Polarity"
*/
const char* zb_manager_get_binary_input_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_BINARY_INPUT_ACTIVE_TEXT_ID:
            return "Active Text";
        case ESP_ZB_ZCL_ATTR_BINARY_INPUT_DESCRIPTION_ID:
            return "Description";
        case ESP_ZB_ZCL_ATTR_BINARY_INPUT_INACTIVE_TEXT_ID:
            return "Inactive Text";
        case ESP_ZB_ZCL_ATTR_BINARY_INPUT_OUT_OF_SERVICE_ID:
            return "Out Of Service";
        case ESP_ZB_ZCL_ATTR_BINARY_INPUT_POLARITY_ID:
            return "Polarity";
        case ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID:
            return "Present Value";
        case ESP_ZB_ZCL_ATTR_BINARY_INPUT_RELIABILITY_ID:
            return "Reliability";
        case ESP_ZB_ZCL_ATTR_BINARY_INPUT_STATUS_FLAGS_ID:
            return "Status Flags";
        case ESP_ZB_ZCL_ATTR_BINARY_INPUT_APPLICATION_TYPE_ID:
            return "Application Type";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_BINARY_OUTPUT_RELINQUISH_DEFAULT_ID;
const char* name = zb_manager_get_binary_output_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Relinquish Default"
*/
const char* zb_manager_get_binary_output_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_BINARY_OUTPUT_ACTIVE_TEXT_ID:
            return "Active Text";
        case ESP_ZB_ZCL_ATTR_BINARY_OUTPUT_DESCRIPTION_ID:
            return "Description";
        case ESP_ZB_ZCL_ATTR_BINARY_OUTPUT_INACTIVE_TEXT_ID:
            return "Inactive Text";
        case ESP_ZB_ZCL_ATTR_BINARY_OUTPUT_MIN_OFF_TIME_ID:
            return "Min Off Time";
        case ESP_ZB_ZCL_ATTR_BINARY_OUTPUT_MAX_ON_TIME_ID:
            return "Max On Time";
        case ESP_ZB_ZCL_ATTR_BINARY_OUTPUT_OUT_OF_SERVICE_ID:
            return "Out Of Service";
        case ESP_ZB_ZCL_ATTR_BINARY_OUTPUT_POLARITY_ID:
            return "Polarity";
        case ESP_ZB_ZCL_ATTR_BINARY_OUTPUT_PRESENT_VALUE_ID:
            return "Present Value";
        case ESP_ZB_ZCL_ATTR_BINARY_OUTPUT_RELIABILITY_ID:
            return "Reliability";
        case ESP_ZB_ZCL_ATTR_BINARY_OUTPUT_RELINQUISH_DEFAULT_ID:
            return "Relinquish Default";
        case ESP_ZB_ZCL_ATTR_BINARY_OUTPUT_STATUS_FLAGS_ID:
            return "Status Flags";
        case ESP_ZB_ZCL_ATTR_BINARY_OUTPUT_APPLICATION_TYPE_ID:
            return "Application Type";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_BINARY_VALUE_MIN_ON_TIME_ID;
const char* name = zb_manager_get_binary_value_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Min On Time"
*/
const char* zb_manager_get_binary_value_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_BINARY_VALUE_ACTIVE_TEXT_ID:
            return "Active Text";
        case ESP_ZB_ZCL_ATTR_BINARY_VALUE_DESCRIPTION_ID:
            return "Description";
        case ESP_ZB_ZCL_ATTR_BINARY_VALUE_INACTIVE_TEXT_ID:
            return "Inactive Text";
        case ESP_ZB_ZCL_ATTR_BINARY_VALUE_MIN_OFF_TIME_ID:
            return "Min Off Time";
        case ESP_ZB_ZCL_ATTR_BINARY_VALUE_MIN_ON_TIME_ID:
            return "Min On Time";
        case ESP_ZB_ZCL_ATTR_BINARY_VALUE_OUT_OF_SERVICE_ID:
            return "Out Of Service";
        case ESP_ZB_ZCL_ATTR_BINARY_VALUE_PRESENT_VALUE_ID:
            return "Present Value";
        case ESP_ZB_ZCL_ATTR_BINARY_VALUE_RELIABILITY_ID:
            return "Reliability";
        case ESP_ZB_ZCL_ATTR_BINARY_VALUE_RELINQUISH_DEFAULT_ID:
            return "Relinquish Default";
        case ESP_ZB_ZCL_ATTR_BINARY_VALUE_STATUS_FLAGS_ID:
            return "Status Flags";
        case ESP_ZB_ZCL_ATTR_BINARY_VALUE_APPLICATION_TYPE_ID:
            return "Application Type";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_MULTI_INPUT_NUMBER_OF_STATES_ID;
const char* name = zb_manager_get_multi_input_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Number of States"
*/
const char* zb_manager_get_multi_input_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_MULTI_INPUT_STATE_TEXT_ID:
            return "State Text";
        case ESP_ZB_ZCL_ATTR_MULTI_INPUT_DESCRIPTION_ID:
            return "Description";
        case ESP_ZB_ZCL_ATTR_MULTI_INPUT_NUMBER_OF_STATES_ID:
            return "Number of States";
        case ESP_ZB_ZCL_ATTR_MULTI_INPUT_OUT_OF_SERVICE_ID:
            return "Out Of Service";
        case ESP_ZB_ZCL_ATTR_MULTI_INPUT_PRESENT_VALUE_ID:
            return "Present Value";
        case ESP_ZB_ZCL_ATTR_MULTI_INPUT_RELIABILITY_ID:
            return "Reliability";
        case ESP_ZB_ZCL_ATTR_MULTI_INPUT_STATUS_FLAGS_ID:
            return "Status Flags";
        case ESP_ZB_ZCL_ATTR_MULTI_INPUT_APPLICATION_TYPE_ID:
            return "Application Type";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_RELINQUISH_DEFAULT_ID;
const char* name = zb_manager_get_multi_output_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Relinquish Default"
*/
const char* zb_manager_get_multi_output_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_STATE_TEXT_ID:
            return "State Text";
        case ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_DESCRIPTION_ID:
            return "Description";
        case ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_NUMBER_OF_STATES_ID:
            return "Number of States";
        case ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_OUT_OF_SERVICE_ID:
            return "Out Of Service";
        case ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_PRESENT_VALUE_ID:
            return "Present Value";
        case ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_RELIABILITY_ID:
            return "Reliability";
        case ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_RELINQUISH_DEFAULT_ID:
            return "Relinquish Default";
        case ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_STATUS_FLAGS_ID:
            return "Status Flags";
        case ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_APPLICATION_TYPE_ID:
            return "Application Type";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_MULTI_VALUE_RELINQUISH_DEFAULT_ID;
const char* name = zb_manager_get_multi_value_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Relinquish Default"
*/
const char* zb_manager_get_multi_value_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_MULTI_VALUE_STATE_TEXT_ID:
            return "State Text";
        case ESP_ZB_ZCL_ATTR_MULTI_VALUE_DESCRIPTION_ID:
            return "Description";
        case ESP_ZB_ZCL_ATTR_MULTI_VALUE_NUMBER_OF_STATES_ID:
            return "Number of States";
        case ESP_ZB_ZCL_ATTR_MULTI_VALUE_OUT_OF_SERVICE_ID:
            return "Out Of Service";
        case ESP_ZB_ZCL_ATTR_MULTI_VALUE_PRESENT_VALUE_ID:
            return "Present Value";
        case ESP_ZB_ZCL_ATTR_MULTI_VALUE_RELIABILITY_ID:
            return "Reliability";
        case ESP_ZB_ZCL_ATTR_MULTI_VALUE_RELINQUISH_DEFAULT_ID:
            return "Relinquish Default";
        case ESP_ZB_ZCL_ATTR_MULTI_VALUE_STATUS_FLAGS_ID:
            return "Status Flags";
        case ESP_ZB_ZCL_ATTR_MULTI_VALUE_APPLICATION_TYPE_ID:
            return "Application Type";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_COMMISSIONING_PROTOCOL_VERSION_ID;
const char* name = zb_manager_get_commissioning_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Protocol Version"
*/
const char* zb_manager_get_commissioning_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_SHORT_ADDRESS_ID:
            return "Short Address";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_EXTENDED_PANID_ID:
            return "Extended PAN ID";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_PANID_ID:
            return "PAN ID";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_CHANNEL_MASK_ID:
            return "Channel Mask";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_PROTOCOL_VERSION_ID:
            return "Protocol Version";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_STACK_PROFILE_ID:
            return "Stack Profile";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_STARTUP_CONTROL_ID:
            return "Startup Control";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_TRUST_CENTER_ADDRESS_ID:
            return "Trust Center Address";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_TRUST_CENTER_MASTER_KEY_ID:
            return "Trust Center Master Key";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_NETWORK_KEY_ID:
            return "Network Key";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_USE_INSECURE_JOIN_ID:
            return "Use Insecure Join";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_PRECONFIGURED_LINK_KEY_ID:
            return "Preconfigured Link Key";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_NETWORK_KEY_SEQ_NUM_ID:
            return "Network Key Seq Num";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_NETWORK_KEY_TYPE_ID:
            return "Network Key Type";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_NETWORK_MANAGER_ADDRESS_ID:
            return "Network Manager Address";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_SCAN_ATTEMPTS_ID:
            return "Scan Attempts";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_TIME_BETWEEN_SCANS_ID:
            return "Time Between Scans";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_REJOIN_INTERVAL_ID:
            return "Rejoin Interval";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_MAX_REJOIN_INTERVAL_ID:
            return "Max Rejoin Interval";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_INDIRECT_POLL_RATE_ID:
            return "Indirect Poll Rate";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_PARENT_RETRY_THRESHOLD_ID:
            return "Parent Retry Threshold";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_CONCENTRATOR_FLAG_ID:
            return "Concentrator Flag";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_CONCENTRATOR_RADIUS_ID:
            return "Concentrator Radius";
        case ESP_ZB_ZCL_ATTR_COMMISSIONING_CONCENTRATOR_DISCOVERY_TIME_ID:
            return "Concentrator Discovery Time";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_OTA_UPGRADE_IMAGE_TYPE_ID;
const char* name = zb_manager_get_ota_upgrade_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Image Type"
*/
const char* zb_manager_get_ota_upgrade_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ID:
            return "Upgrade Server ID";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_FILE_OFFSET_ID:
            return "File Offset";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_FILE_VERSION_ID:
            return "Current File Version";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_STACK_VERSION_ID:
            return "Current Zigbee Stack Version";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_DOWNLOADED_FILE_VERSION_ID:
            return "Downloaded File Version";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_DOWNLOADED_STACK_VERSION_ID:
            return "Downloaded Zigbee Stack Version";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_IMAGE_STATUS_ID:
            return "Image Upgrade Status";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_MANUFACTURE_ID:
            return "Manufacturer Code";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_IMAGE_TYPE_ID:
            return "Image Type";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_MIN_BLOCK_REQUE_ID:
            return "Minimum Block Request Period";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_IMAGE_STAMP_ID:
            return "Image Stamp";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_UPGRADE_ACTIVATION_POLICY_ID:
            return "Upgrade Activation Policy";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_UPGRADE_TIMEOUT_POLICY_ID:
            return "Upgrade Timeout Policy";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ENDPOINT_ID:
            return "OTA Server Endpoint (Custom)";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ADDR_ID:
            return "OTA Server Address (Custom)";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_CLIENT_DATA_ID:
            return "OTA Client Data (Custom)";
        case ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_DATA_ID:
            return "OTA Server Data (Custom)";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_POLL_CONTROL_FAST_POLL_TIMEOUT_ID;
const char* name = zb_manager_get_poll_control_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Fast Poll Timeout"
*/
const char* zb_manager_get_poll_control_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_POLL_CONTROL_CHECK_IN_INTERVAL_ID:
            return "Check-in Interval";
        case ESP_ZB_ZCL_ATTR_POLL_CONTROL_LONG_POLL_INTERVAL_ID:
            return "Long Poll Interval";
        case ESP_ZB_ZCL_ATTR_POLL_CONTROL_SHORT_POLL_INTERVAL_ID:
            return "Short Poll Interval";
        case ESP_ZB_ZCL_ATTR_POLL_CONTROL_FAST_POLL_TIMEOUT_ID:
            return "Fast Poll Timeout";
        case ESP_ZB_ZCL_ATTR_POLL_CONTROL_MIN_CHECK_IN_INTERVAL_ID:
            return "Minimum Check-in Interval";
        case ESP_ZB_ZCL_ATTR_POLL_CONTROL_LONG_POLL_MIN_INTERVAL_ID:
            return "Minimum Long Poll Interval";
        case ESP_ZB_ZCL_ATTR_POLL_CONTROL_FAST_POLL_MAX_TIMEOUT_ID:
            return "Maximum Fast Poll Timeout";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_GPS_SECURITY_LEVEL_ID;
const char* name = zb_manager_get_green_power_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Security Level"
*/
const char* zb_manager_get_green_power_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_GPS_MAX_SINK_TABLE_ENTRIES_ID:
            return "Max Sink Table Entries";
        case ESP_ZB_ZCL_ATTR_GPS_SINK_TABLE_ID:
            return "Sink Table";
        case ESP_ZB_ZCL_ATTR_GPS_COMMUNICATION_MODE_ID:
            return "Communication Mode";
        case ESP_ZB_ZCL_ATTR_GPS_COMMISSIONING_EXIT_MODE_ID:
            return "Commissioning Exit Mode";
        case ESP_ZB_ZCL_ATTR_GPS_COMMISSIONING_WINDOW_ID:
            return "Commissioning Window Duration";
        case ESP_ZB_ZCL_ATTR_GPS_SECURITY_LEVEL_ID:
            return "Security Level";
        case ESP_ZB_ZCL_ATTR_GPS_FUNCTIONALITY_ID:
            return "Functionality";
        case ESP_ZB_ZCL_ATTR_GPS_ACTIVE_FUNCTIONALITY_ID:
            return "Active Functionality";
        case ESP_ZB_ZCL_ATTR_GPP_MAX_PROXY_TABLE_ENTRIES_ID:
            return "Max Proxy Table Entries";
        case ESP_ZB_ZCL_ATTR_GPP_PROXY_TABLE_ID:
            return "Proxy Table";
        case ESP_ZB_ZCL_ATTR_GPP_NOTIFICATION_RETRY_NUMBER_ID:
            return "Notification Retry Number";
        case ESP_ZB_ZCL_ATTR_GPP_NOTIFICATION_RETRY_TIMER_ID:
            return "Notification Retry Timer";
        case ESP_ZB_ZCL_ATTR_GPP_MAX_SEARCH_COUNTER_ID:
            return "Max Search Counter";
        case ESP_ZB_ZCL_ATTR_GPP_BLOCKED_GPDID_ID:
            return "Blocked GPD ID List";
        case ESP_ZB_ZCL_ATTR_GPP_FUNCTIONALITY_ID:
            return "Proxy Functionality";
        case ESP_ZB_ZCL_ATTR_GPP_ACTIVE_FUNCTIONALITY_ID:
            return "Proxy Active Functionality";
        case ESP_ZB_ZCL_ATTR_GP_SHARED_SECURITY_KEY_TYPE_ID:
            return "Shared Security Key Type";
        case ESP_ZB_ZCL_ATTR_GP_SHARED_SECURITY_KEY_ID:
            return "Shared Security Key";
        case ESP_ZB_ZCL_ATTR_GP_LINK_KEY_ID:
            return "Link Key";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_SHADE_CONFIG_MOTOR_STEP_SIZE_ID;
const char* name = zb_manager_get_shade_config_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Motor Step Size"
*/
const char* zb_manager_get_shade_config_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_SHADE_CONFIG_PHYSICAL_CLOSED_LIMIT_ID:
            return "Physical Closed Limit";
        case ESP_ZB_ZCL_ATTR_SHADE_CONFIG_MOTOR_STEP_SIZE_ID:
            return "Motor Step Size";
        case ESP_ZB_ZCL_ATTR_SHADE_CONFIG_STATUS_ID:
            return "Status";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_DOOR_LOCK_SECURITY_LEVEL_ID;
const char* name = zb_manager_get_door_lock_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Security Level"
*/
const char* zb_manager_get_door_lock_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_ID:
            return "Lock State";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_LOCK_TYPE_ID:
            return "Lock Type";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_ACTUATOR_ENABLED_ID:
            return "Actuator Enabled";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_DOOR_STATE_ID:
            return "Door State";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_NUM_OF_DOOR_OPEN_EVENTS_ID:
            return "Number of Door Open Events";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_NUM_OF_DOOR_CLOSED_EVENTS_ID:
            return "Number of Door Closed Events";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_OPEN_PERIOD_ID:
            return "Open Period";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_NUMBER_OF_LOG_RECORDS_SUPPORTED_ID:
            return "Number of Log Records Supported";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_NUM_TOTAL_USERS_ID:
            return "Number of Total Users Supported";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_NUM_PIN_USERS_ID:
            return "Number of PIN Users Supported";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_NUMBER_OF_RFID_USERS_SUPPORTED_ID:
            return "Number of RFID Users Supported";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_NUM_WEEK_DAY_SCHEDULE_PER_USER_ID:
            return "Number of Week Day Schedules Per User";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_NUM_YEAR_DAY_SCHEDULE_PER_USER_ID:
            return "Number of Year Day Schedules Per User";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_NUM_HOLIDAY_SCHEDULE_ID:
            return "Number of Holiday Schedules";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_MAX_PIN_LEN_ID:
            return "Max PIN Length";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_MIN_PIN_LEN_ID:
            return "Min PIN Length";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_MAX_RFID_CODE_LENGTH_ID:
            return "Max RFID Code Length";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_MIN_RFID_CODE_LENGTH_ID:
            return "Min RFID Code Length";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_ENABLE_LOGGING_ID:
            return "Enable Logging";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_LANGUAGE_ID:
            return "Language";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_LED_SETTINGS_ID:
            return "LED Settings";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_AUTO_RELOCK_TIME_ID:
            return "Auto Relock Time";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_SOUND_VOLUME_ID:
            return "Sound Volume";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_OPERATING_MODE_ID:
            return "Operating Mode";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_OPERATION_MODES_SUPPORTED_ID:
            return "Supported Operating Modes";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_DEFAULT_CONFIGURATION_REGISTER_ID:
            return "Default Configuration Register";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_ENABLE_LOCAL_PROGRAMMING_ID:
            return "Enable Local Programming";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_ENABLE_ONE_TOUCH_LOCKING_ID:
            return "Enable One-Touch Locking";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_ENABLE_INSIDE_STATUS_LED_ID:
            return "Enable Inside Status LED";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_ENABLE_PRIVACY_MODE_BUTTON_ID:
            return "Enable Privacy Mode Button";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_WRONG_CODE_ENTRY_LIMIT_ID:
            return "Wrong Code Entry Limit";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_USER_CODE_TEMPORARY_DISABLE_TIME_ID:
            return "User Code Temporary Disable Time";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_SEND_PIN_OVER_THE_AIR_ID:
            return "Send PIN Over the Air";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_REQUIRE_PIN_RF_ID:
            return "Require PIN for RF Operation";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_SECURITY_LEVEL_ID:
            return "Security Level";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_ALARM_MASK_ID:
            return "Alarm Mask";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_KEYPAD_OPERATION_EVENT_MASK_ID:
            return "Keypad Operation Event Mask";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_RF_OPERATION_EVENT_MASK_ID:
            return "RF Operation Event Mask";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_MANUAL_OPERATION_EVENT_MASK_ID:
            return "Manual Operation Event Mask";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_RFID_OPERATION_EVENT_MASK_ID:
            return "RFID Operation Event Mask";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_KEYPAD_PROGRAMMING_EVENT_MASK_ID:
            return "Keypad Programming Event Mask";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_RF_PROGRAMMING_EVENT_MASK_ID:
            return "RF Programming Event Mask";
        case ESP_ZB_ZCL_ATTR_DOOR_LOCK_RFID_PROGRAMMING_EVENT_MASK_ID:
            return "RFID Programming Event Mask";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_VELOCITY_ID;
const char* name = zb_manager_get_window_covering_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Velocity (Lift)"
*/
const char* zb_manager_get_window_covering_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        // Information Attributes
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_WINDOW_COVERING_TYPE_ID:
            return "Window Covering Type";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_PHYSICAL_CLOSED_LIMIT_LIFT_ID:
            return "Physical Closed Limit (Lift)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_PHY_CLOSED_LIMIT_TILT_ID:
            return "Physical Closed Limit (Tilt)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_ID:
            return "Current Position (Lift)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_ID:
            return "Current Position (Tilt)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_NUMBER_OF_ACTUATIONS_LIFT_ID:
            return "Number of Actuations (Lift)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_NUMBER_OF_ACTUATIONS_TILT_ID:
            return "Number of Actuations (Tilt)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_STATUS_ID:
            return "Config/Status";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID:
            return "Current Position (Lift Percentage)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_ID:
            return "Current Position (Tilt Percentage)";

        // Settings Attributes
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_LIFT_ID:
            return "Installed Open Limit (Lift)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT_ID:
            return "Installed Closed Limit (Lift)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_TILT_ID:
            return "Installed Open Limit (Tilt)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_TILT_ID:
            return "Installed Closed Limit (Tilt)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_VELOCITY_ID:
            return "Velocity (Lift)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_ACCELERATION_TIME_ID:
            return "Acceleration Time (Lift)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_DECELERATION_TIME_ID:
            return "Deceleration Time (Lift)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_MODE_ID:
            return "Mode";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INTERMEDIATE_SETPOINTS_LIFT_ID:
            return "Intermediate Setpoints (Lift)";
        case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INTERMEDIATE_SETPOINTS_TILT_ID:
            return "Intermediate Setpoints (Tilt)";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_THERMOSTAT_AC_ERROR_CODE_ID;
const char* name = zb_manager_get_thermostat_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "AC Error Code"
*/
const char* zb_manager_get_thermostat_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        // Core Temperature & Control Attributes
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_ID:
            return "Local Temperature";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_OUTDOOR_TEMPERATURE_ID:
            return "Outdoor Temperature";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPANCY_ID:
            return "Occupancy";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_ABS_MIN_HEAT_SETPOINT_LIMIT_ID:
            return "Abs Min Heat Setpoint Limit";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_ABS_MAX_HEAT_SETPOINT_LIMIT_ID:
            return "Abs Max Heat Setpoint Limit";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_ABS_MIN_COOL_SETPOINT_LIMIT_ID:
            return "Abs Min Cool Setpoint Limit";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_ABS_MAX_COOL_SETPOINT_LIMIT_ID:
            return "Abs Max Cool Setpoint Limit";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_PI_COOLING_DEMAND_ID:
            return "PI Cooling Demand";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_PI_HEATING_DEMAND_ID:
            return "PI Heating Demand";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_HVAC_SYSTEM_TYPE_CONFIGURATION_ID:
            return "HVAC System Type Configuration";

        // Setpoint Attributes
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_CALIBRATION_ID:
            return "Local Temperature Calibration";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ID:
            return "Occupied Cooling Setpoint";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID:
            return "Occupied Heating Setpoint";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_UNOCCUPIED_COOLING_SETPOINT_ID:
            return "Unoccupied Cooling Setpoint";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_UNOCCUPIED_HEATING_SETPOINT_ID:
            return "Unoccupied Heating Setpoint";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_MIN_HEAT_SETPOINT_LIMIT_ID:
            return "Min Heat Setpoint Limit";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_MAX_HEAT_SETPOINT_LIMIT_ID:
            return "Max Heat Setpoint Limit";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_MIN_COOL_SETPOINT_LIMIT_ID:
            return "Min Cool Setpoint Limit";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_MAX_COOL_SETPOINT_LIMIT_ID:
            return "Max Cool Setpoint Limit";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_MIN_SETPOINT_DEAD_BAND_ID:
            return "Min Setpoint Dead Band";

        // System Control Attributes
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_REMOTE_SENSING_ID:
            return "Remote Sensing";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_CONTROL_SEQUENCE_OF_OPERATION_ID:
            return "Control Sequence Of Operation";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID:
            return "System Mode";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_ALARM_MASK_ID:
            return "Alarm Mask";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_RUNNING_MODE_ID:
            return "Running Mode";

        // Schedule & Programming Attributes
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_START_OF_WEEK_ID:
            return "Start of Week";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_NUMBER_OF_WEEKLY_TRANSITIONS_ID:
            return "Number of Weekly Transitions";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_NUMBER_OF_DAILY_TRANSITIONS_ID:
            return "Number of Daily Transitions";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_TEMPERATURE_SETPOINT_HOLD_ID:
            return "Temperature Setpoint Hold";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_TEMPERATURE_SETPOINT_HOLD_DURATION_ID:
            return "Temperature Setpoint Hold Duration";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_THERMOSTAT_PROGRAMMING_OPERATION_MODE_ID:
            return "Thermostat Programming Operation Mode";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_THERMOSTAT_RUNNING_STATE_ID:
            return "Thermostat Running State";

        // Event & Logging Attributes
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_SETPOINT_CHANGE_SOURCE_ID:
            return "Setpoint Change Source";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_SETPOINT_CHANGE_AMOUNT_ID:
            return "Setpoint Change Amount";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_SETPOINT_CHANGE_SOURCE_TIMESTAMP_ID:
            return "Setpoint Change Source Timestamp";

        // Setback & Emergency Attributes
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_SETBACK_ID:
            return "Occupied Setback";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_SETBACK_MIN_ID:
            return "Occupied Setback Min";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_SETBACK_MAX_ID:
            return "Occupied Setback Max";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_UNOCCUPIED_SETBACK_ID:
            return "Unoccupied Setback";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_UNOCCUPIED_SETBACK_MIN_ID:
            return "Unoccupied Setback Min";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_UNOCCUPIED_SETBACK_MAX_ID:
            return "Unoccupied Setback Max";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_EMERGENCY_HEAT_DELTA_ID:
            return "Emergency Heat Delta";

        // Mini Split AC Specific Attributes
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_AC_TYPE_ID:
            return "AC Type";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_AC_CAPACITY_ID:
            return "AC Capacity";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_AC_REFRIGERANT_TYPE_ID:
            return "AC Refrigerant Type";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_AC_COMPRESSOR_TYPE_ID:
            return "AC Compressor Type";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_AC_ERROR_CODE_ID:
            return "AC Error Code";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_AC_LOUVER_POSITION_ID:
            return "AC Louver Position";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_AC_COIL_TEMPERATURE_ID:
            return "AC Coil Temperature";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_AC_CAPACITY_FORMAT_ID:
            return "AC Capacity Format";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_THERMOSTAT_UI_CONFIG_KEYPAD_LOCKOUT_ID;
const char* name = zb_manager_get_thermostat_ui_config_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Keypad Lockout"
*/
const char* zb_manager_get_thermostat_ui_config_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_UI_CONFIG_TEMPERATURE_DISPLAY_MODE_ID:
            return "Temperature Display Mode";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_UI_CONFIG_KEYPAD_LOCKOUT_ID:
            return "Keypad Lockout";
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_UI_CONFIG_SCHEDULE_PROGRAMMING_VISIBILITY_ID:
            return "Schedule Programming Visibility";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_SEQUENCE_ID;
const char* name = zb_manager_get_fan_control_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Fan Mode Sequence"
*/
const char* zb_manager_get_fan_control_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_ID:
            return "Fan Mode";
        case ESP_ZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_SEQUENCE_ID:
            return "Fan Mode Sequence";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_DEHUMIDIFICATION_CONTROL_DEHUMIDIFICATION_HYSTERESIS_ID;
const char* name = zb_manager_get_dehumidification_control_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Dehumidification Hysteresis"
*/
const char* zb_manager_get_dehumidification_control_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_DEHUMIDIFICATION_CONTROL_RELATIVE_HUMIDITY_ID:
            return "Relative Humidity";
        case ESP_ZB_ZCL_ATTR_DEHUMIDIFICATION_CONTROL_DEHUMIDIFICATION_COOLING_ID:
            return "Dehumidification Cooling";
        case ESP_ZB_ZCL_ATTR_DEHUMIDIFICATION_CONTROL_RHDEHUMIDIFICATION_SETPOINT_ID:
            return "RH Dehumidification Setpoint";
        case ESP_ZB_ZCL_ATTR_DEHUMIDIFICATION_CONTROL_RELATIVE_HUMIDITY_MODE_ID:
            return "Relative Humidity Mode";
        case ESP_ZB_ZCL_ATTR_DEHUMIDIFICATION_CONTROL_DEHUMIDIFICATION_LOCKOUT_ID:
            return "Dehumidification Lockout";
        case ESP_ZB_ZCL_ATTR_DEHUMIDIFICATION_CONTROL_DEHUMIDIFICATION_HYSTERESIS_ID:
            return "Dehumidification Hysteresis";
        case ESP_ZB_ZCL_ATTR_DEHUMIDIFICATION_CONTROL_DEHUMIDIFICATION_MAX_COOL_ID:
            return "Dehumidification Max Cool";
        case ESP_ZB_ZCL_ATTR_DEHUMIDIFICATION_CONTROL_RELATIVE_HUMIDITY_DISPLAY_ID:
            return "Relative Humidity Display";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID;
const char* name = zb_manager_get_color_control_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Color Temperature (Mireds)"
*/
const char* zb_manager_get_color_control_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        // Core Color Attributes
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID:
            return "Current Hue";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID:
            return "Current Saturation";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_REMAINING_TIME_ID:
            return "Remaining Time";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID:
            return "Current X";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID:
            return "Current Y";

        // Advanced Color Attributes
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_DRIFT_COMPENSATION_ID:
            return "Drift Compensation";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COMPENSATION_TEXT_ID:
            return "Compensation Text";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID:
            return "Color Temperature (Mireds)";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_MODE_ID:
            return "Color Mode";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_OPTIONS_ID:
            return "Options";

        // Enhanced Color Attributes
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_ENHANCED_CURRENT_HUE_ID:
            return "Enhanced Current Hue";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_ENHANCED_COLOR_MODE_ID:
            return "Enhanced Color Mode";

        // Color Loop Attributes
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_LOOP_ACTIVE_ID:
            return "Color Loop Active";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_LOOP_DIRECTION_ID:
            return "Color Loop Direction";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_LOOP_TIME_ID:
            return "Color Loop Time";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_LOOP_START_ENHANCED_HUE_ID:
            return "Color Loop Start Enhanced Hue";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_LOOP_STORED_ENHANCED_HUE_ID:
            return "Color Loop Stored Enhanced Hue";

        // Hardware & Configuration Attributes
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_CAPABILITIES_ID:
            return "Color Capabilities";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_MIREDS_ID:
            return "Color Temp Physical Min (Mireds)";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_MIREDS_ID:
            return "Color Temp Physical Max (Mireds)";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COUPLE_COLOR_TEMP_TO_LEVEL_MIN_MIREDS_ID:
            return "Couple Color Temp to Level Min (Mireds)";
        case ESP_ZB_ZCL_ATTR_COLOR_CONTROL_START_UP_COLOR_TEMPERATURE_MIREDS_ID:
            return "Start Up Color Temperature (Mireds)";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_TOLERANCE_ID;
const char* name = zb_manager_get_illuminance_measurement_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Tolerance"
*/
const char* zb_manager_get_illuminance_measurement_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MEASURED_VALUE_ID:
            return "Measured Value";
        case ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MIN_MEASURED_VALUE_ID:
            return "Min Measured Value";
        case ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MAX_MEASURED_VALUE_ID:
            return "Max Measured Value";
        case ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_TOLERANCE_ID:
            return "Tolerance";
        case ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_LIGHT_SENSOR_TYPE_ID:
            return "Light Sensor Type";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID;
const char* name = zb_manager_get_temperature_measurement_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Tolerance"
*/
const char* zb_manager_get_temperature_measurement_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID:
            return "Measured Value";
        case ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID:
            return "Min Measured Value";
        case ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID:
            return "Max Measured Value";
        case ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID:
            return "Tolerance";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_SCALE_ID;
const char* name = zb_manager_get_pressure_measurement_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Scale (Exponent)"
*/
const char* zb_manager_get_pressure_measurement_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        // Core Measurement Attributes
        case ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_ID:
            return "Measured Value";
        case ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_MIN_VALUE_ID:
            return "Min Measured Value";
        case ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_MAX_VALUE_ID:
            return "Max Measured Value";
        case ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_TOLERANCE_ID:
            return "Tolerance";

        // Scaled Measurement Attributes
        case ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_SCALED_VALUE_ID:
            return "Scaled Value";
        case ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_MIN_SCALED_VALUE_ID:
            return "Min Scaled Value";
        case ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_MAX_SCALED_VALUE_ID:
            return "Max Scaled Value";
        case ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_SCALED_TOLERANCE_ID:
            return "Scaled Tolerance";
        case ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_SCALE_ID:
            return "Scale (Exponent)";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_FLOW_MEASUREMENT_TOLERANCE_ID;
const char* name = zb_manager_get_flow_measurement_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Tolerance"
*/
const char* zb_manager_get_flow_measurement_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_FLOW_MEASUREMENT_VALUE_ID:
            return "Measured Value";
        case ESP_ZB_ZCL_ATTR_FLOW_MEASUREMENT_MIN_VALUE_ID:
            return "Min Measured Value";
        case ESP_ZB_ZCL_ATTR_FLOW_MEASUREMENT_MAX_VALUE_ID:
            return "Max Measured Value";
        case ESP_ZB_ZCL_ATTR_FLOW_MEASUREMENT_TOLERANCE_ID:
            return "Tolerance";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_REL_HUMIDITY_TOLERANCE_ID;
const char* name = zb_manager_get_humidity_measurement_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Tolerance"
*/
const char* zb_manager_get_humidity_measurement_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID:
            return "Measured Value";
        case ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MIN_VALUE_ID:
            return "Min Measured Value";
        case ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MAX_VALUE_ID:
            return "Max Measured Value";
        case ESP_ZB_ZCL_ATTR_REL_HUMIDITY_TOLERANCE_ID:
            return "Tolerance";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PIR_UNOCC_TO_OCC_THRESHOLD_ID;
const char* name = zb_manager_get_occupancy_sensing_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "PIR Unoccupied to Occupied Threshold"
*/
const char* zb_manager_get_occupancy_sensing_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        // Core Attributes
        case ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID:
            return "Occupancy";
        case ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_ID:
            return "Occupancy Sensor Type";
        case ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_BITMAP_ID:
            return "Occupancy Sensor Type Bitmap";

        // PIR Sensor Configuration
        case ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PIR_OCC_TO_UNOCC_DELAY_ID:
            return "PIR Occupied to Unoccupied Delay";
        case ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PIR_UNOCC_TO_OCC_DELAY_ID:
            return "PIR Unoccupied to Occupied Delay";
        case ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PIR_UNOCC_TO_OCC_THRESHOLD_ID:
            return "PIR Unoccupied to Occupied Threshold";

        // Ultrasonic Sensor Configuration
        case ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_ULTRASONIC_OCCUPIED_TO_UNOCCUPIED_DELAY_ID:
            return "Ultrasonic Occupied to Unoccupied Delay";
        case ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_ULTRASONIC_UNOCCUPIED_TO_OCCUPIED_DELAY_ID:
            return "Ultrasonic Unoccupied to Occupied Delay";
        case ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_ULTRASONIC_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_ID:
            return "Ultrasonic Unoccupied to Occupied Threshold";

        // Physical Contact Sensor Configuration
        case ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PHYSICAL_CONTACT_OCCUPIED_TO_UNOCCUPIED_DELAY_ID:
            return "Physical Contact Occupied to Unoccupied Delay";
        case ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PHYSICAL_CONTACT_UNOCCUPIED_TO_OCCUPIED_DELAY_ID:
            return "Physical Contact Unoccupied to Occupied Delay";
        case ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_PHYSICAL_CONTACT_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_ID:
            return "Physical Contact Unoccupied to Occupied Threshold";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_PH_MEASUREMENT_TOLERANCE_ID;
const char* name = zb_manager_get_ph_measurement_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Tolerance"
*/
const char* zb_manager_get_ph_measurement_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_PH_MEASUREMENT_MEASURED_VALUE_ID:
            return "Measured Value";
        case ESP_ZB_ZCL_ATTR_PH_MEASUREMENT_MIN_MEASURED_VALUE_ID:
            return "Min Measured Value";
        case ESP_ZB_ZCL_ATTR_PH_MEASUREMENT_MAX_MEASURED_VALUE_ID:
            return "Max Measured Value";
        case ESP_ZB_ZCL_ATTR_PH_MEASUREMENT_TOLERANCE_ID:
            return "Tolerance";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_EC_MEASUREMENT_TOLERANCE_ID;
const char* name = zb_manager_get_ec_measurement_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Tolerance"
*/
const char* zb_manager_get_ec_measurement_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_EC_MEASUREMENT_MEASURED_VALUE_ID:
            return "Measured Value";
        case ESP_ZB_ZCL_ATTR_EC_MEASUREMENT_MIN_MEASURED_VALUE_ID:
            return "Min Measured Value";
        case ESP_ZB_ZCL_ATTR_EC_MEASUREMENT_MAX_MEASURED_VALUE_ID:
            return "Max Measured Value";
        case ESP_ZB_ZCL_ATTR_EC_MEASUREMENT_TOLERANCE_ID:
            return "Tolerance";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_TOLERANCE_ID;
const char* name = zb_manager_get_wind_speed_measurement_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Tolerance"
*/
const char* zb_manager_get_wind_speed_measurement_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_MEASURED_VALUE_ID:
            return "Measured Value";
        case ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_MIN_MEASURED_VALUE_ID:
            return "Min Measured Value";
        case ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_MAX_MEASURED_VALUE_ID:
            return "Max Measured Value";
        case ESP_ZB_ZCL_ATTR_WIND_SPEED_MEASUREMENT_TOLERANCE_ID:
            return "Tolerance";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_TOLERANCE_ID;
const char* name = zb_manager_get_carbon_dioxide_measurement_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Tolerance"*/

const char* zb_manager_get_carbon_dioxide_measurement_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID:
            return "Measured Value";
        case ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MIN_MEASURED_VALUE_ID:
            return "Min Measured Value";
        case ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MAX_MEASURED_VALUE_ID:
            return "Max Measured Value";
        case ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_TOLERANCE_ID:
            return "Tolerance";
        default:
            return "Unknown Attribute";
    }
}


/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_PM2_5_MEASUREMENT_TOLERANCE_ID;
const char* name = zb_manager_get_pm2_5_measurement_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Tolerance"
*/
const char* zb_manager_get_pm2_5_measurement_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        case ESP_ZB_ZCL_ATTR_PM2_5_MEASUREMENT_MEASURED_VALUE_ID:
            return "Measured Value";
        case ESP_ZB_ZCL_ATTR_PM2_5_MEASUREMENT_MIN_MEASURED_VALUE_ID:
            return "Min Measured Value";
        case ESP_ZB_ZCL_ATTR_PM2_5_MEASUREMENT_MAX_MEASURED_VALUE_ID:
            return "Max Measured Value";
        case ESP_ZB_ZCL_ATTR_PM2_5_MEASUREMENT_TOLERANCE_ID:
            return "Tolerance";
        default:
            return "Unknown Attribute";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONETYPE_ID;
const char* name = zb_manager_get_ias_zone_cluster_attr_name(attr_id);
printf("Attribute Name: %s\n", name); // Output: "Zone Type"
*/
const char* zb_manager_get_ias_zone_cluster_attr_name(uint16_t attrID) {
    switch (attrID) {
        // Core Zone Attributes
        case ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONESTATE_ID:
            return "Zone State";
        case ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONETYPE_ID:
            return "Zone Type";
        case ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONESTATUS_ID:
            return "Zone Status";

        // Device Configuration Attributes
        case ESP_ZB_ZCL_ATTR_IAS_ZONE_IAS_CIE_ADDRESS_ID:
            return "IAS CIE Address";
        case ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONEID_ID:
            return "Zone ID";
        case ESP_ZB_ZCL_ATTR_IAS_ZONE_NUMBER_OF_ZONE_SENSITIVITY_LEVELS_SUPPORTED_ID:
            return "Number of Sensitivity Levels Supported";
        case ESP_ZB_ZCL_ATTR_IAS_ZONE_CURRENT_ZONE_SENSITIVITY_LEVEL_ID:
            return "Current Sensitivity Level";

        // Internal/Custom Attributes
        case ESP_ZB_ZCL_ATTR_CUSTOM_CIE_ADDR_IS_SET:
            return "CIE Address Set Flag";
        case ESP_ZB_ZCL_ATTR_CUSTOM_CIE_EP:
            return "CIE Endpoint";
        case ESP_ZB_ZCL_ATTR_CUSTOM_CIE_SHORT_ADDR:
            return "CIE Short Address";
        case ESP_ZB_ZCL_ATTR_IAS_ZONE_INT_CTX_ID:
            return "Internal Context";
        default:
            return "Unknown Attribute";
    }
}

/*
esp_zb_zcl_drlc_cli_attr_t attr_id = ESP_ZB_ZCL_ATTR_DRLC_UTILITY_ENROLLMENT_GROUP;
printf("Attribute name: %s\n", esp_zb_zcl_drlc_get_attr_name(attr_id));
// Вывод: Attribute name: UTILITY_ENROLLMENT_GROUP
*/
const char *esp_zb_zcl_drlc_get_attr_name(uint16_t attr_id)
{
    switch (attr_id) {
        case ESP_ZB_ZCL_ATTR_DRLC_UTILITY_ENROLLMENT_GROUP:
            return "UTILITY_ENROLLMENT_GROUP";
        case ESP_ZB_ZCL_ATTR_DRLC_START_RANDOMIZATION_MINUTES:
            return "START_RANDOMIZATION_MINUTES";
        case ESP_ZB_ZCL_ATTR_DRLC_DURATION_RANDOMIZATION_MINUTES:
            return "DURATION_RANDOMIZATION_MINUTES";
        case ESP_ZB_ZCL_ATTR_DRLC_DEVICE_CLASS_VALUE:
            return "DEVICE_CLASS_VALUE";
        default:
            return "UNKNOWN_ATTRIBUTE";
    }
}

/*
uint16_t attr_id = ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_ID;
    printf("Attribute Name: %s\n", get_electrical_measurement_attr_name(attr_id));*/
const char* get_electrical_measurement_attr_name(uint16_t attr_id) {
    switch (attr_id) {
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_ID: return "Measurement Type";

        // DC Measurement
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_ID: return "DC Voltage";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_MIN_ID: return "DC Voltage Min";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_VOLTAGE_MAX_ID: return "DC Voltage Max";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_ID: return "DC Current";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_MIN_ID: return "DC Current Min";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_CURRENT_MAX_ID: return "DC Current Max";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DCPOWER_ID: return "DC Power";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_MIN_ID: return "DC Power Min";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_DC_POWER_MAX_ID: return "DC Power Max";

        // AC Measurement (Non Phase)
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_ID: return "AC Frequency";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_MIN_ID: return "AC Frequency Min";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_FREQUENCY_MAX_ID: return "AC Frequency Max";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_NEUTRAL_CURRENT_ID: return "Neutral Current";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_TOTAL_ACTIVE_POWER_ID: return "Total Active Power";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_TOTAL_REACTIVE_POWER_ID: return "Total Reactive Power";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_TOTAL_APPARENT_POWER_ID: return "Total Apparent Power";

        // AC Measurement (Phase A)
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_LINE_CURRENT_ID: return "Line Current (Phase A)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_CURRENT_ID: return "Active Current (Phase A)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_REACTIVE_CURRENT_ID: return "Reactive Current (Phase A)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_ID: return "RMS Voltage (Phase A)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_ID: return "RMS Voltage Min (Phase A)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_ID: return "RMS Voltage Max (Phase A)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_ID: return "RMS Current (Phase A)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_ID: return "RMS Current Min (Phase A)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_ID: return "RMS Current Max (Phase A)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID: return "Active Power (Phase A)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_REACTIVE_POWER_ID: return "Reactive Power (Phase A)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_APPARENT_POWER_ID: return "Apparent Power (Phase A)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_ID: return "Power Factor (Phase A)";

        // AC Measurement (Phase B)
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_LINE_CURRENT_PH_B_ID: return "Line Current (Phase B)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_CURRENT_PH_B_ID: return "Active Current (Phase B)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_REACTIVE_CURRENT_PH_B_ID: return "Reactive Current (Phase B)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_PHB_ID: return "RMS Voltage (Phase B)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_PH_B_ID: return "RMS Voltage Min (Phase B)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_PH_B_ID: return "RMS Voltage Max (Phase B)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_PHB_ID: return "RMS Current (Phase B)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_PH_B_ID: return "RMS Current Min (Phase B)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_PH_B_ID: return "RMS Current Max (Phase B)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PHB_ID: return "Active Power (Phase B)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_REACTIVE_POWER_PH_B_ID: return "Reactive Power (Phase B)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_APPARENT_POWER_PHB_ID: return "Apparent Power (Phase B)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_B_ID: return "Power Factor (Phase B)";

        // AC Measurement (Phase C)
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_LINE_CURRENT_PH_C_ID: return "Line Current (Phase C)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_CURRENT_PH_C_ID: return "Active Current (Phase C)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_REACTIVE_CURRENT_PH_C_ID: return "Reactive Current (Phase C)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSVOLTAGE_PHC_ID: return "RMS Voltage (Phase C)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MIN_PH_C_ID: return "RMS Voltage Min (Phase C)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_MAX_PH_C_ID: return "RMS Voltage Max (Phase C)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_PHC_ID: return "RMS Current (Phase C)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MIN_PH_C_ID: return "RMS Current Min (Phase C)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_PH_C_ID: return "RMS Current Max (Phase C)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_PHC_ID: return "Active Power (Phase C)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_REACTIVE_POWER_PH_C_ID: return "Reactive Power (Phase C)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_APPARENT_POWER_PHC_ID: return "Apparent Power (Phase C)";
        case ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_POWER_FACTOR_PH_C_ID: return "Power Factor (Phase C)";

        default: return "Unknown Attribute";
    }
}



// Define function pointer type for attribute name functions
typedef const char* (*attr_func_t)(uint16_t attr_id);

// Mapping of cluster IDs to their respective attribute naming functions
static const struct {
    uint16_t cluster_id;
    attr_func_t func;
} cluster_attr_funcs[] = {
    { ESP_ZB_ZCL_CLUSTER_ID_BASIC, zb_manager_get_basic_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, zb_manager_get_power_config_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_DEVICE_TEMP_CONFIG, zb_manager_get_device_temp_config_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY, zb_manager_get_identify_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_GROUPS, zb_manager_get_groups_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_SCENES, zb_manager_get_scenes_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, zb_manager_get_on_off_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG, zb_manager_get_on_off_switch_type_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, zb_manager_get_level_control_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_ALARMS, zb_manager_get_alarms_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_TIME, zb_manager_get_time_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, zb_manager_get_analog_input_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, zb_manager_get_analog_output_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT, zb_manager_get_binary_input_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT, zb_manager_get_binary_output_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_BINARY_VALUE, zb_manager_get_binary_value_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_MULTI_INPUT, zb_manager_get_multi_input_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_MULTI_OUTPUT, zb_manager_get_multi_output_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_MULTI_VALUE, zb_manager_get_multi_value_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_COMMISSIONING, zb_manager_get_commissioning_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_OTA_UPGRADE, zb_manager_get_ota_upgrade_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_POLL_CONTROL, zb_manager_get_poll_control_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_GREEN_POWER, zb_manager_get_green_power_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_SHADE_CONFIG, zb_manager_get_shade_config_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_DOOR_LOCK, zb_manager_get_door_lock_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, zb_manager_get_window_covering_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, zb_manager_get_thermostat_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL, zb_manager_get_fan_control_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_DEHUMIDIFICATION_CONTROL, zb_manager_get_dehumidification_control_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT_UI_CONFIG, zb_manager_get_thermostat_ui_config_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, zb_manager_get_color_control_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT, zb_manager_get_illuminance_measurement_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, zb_manager_get_temperature_measurement_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, zb_manager_get_pressure_measurement_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT, zb_manager_get_flow_measurement_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, zb_manager_get_humidity_measurement_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, zb_manager_get_occupancy_sensing_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_PH_MEASUREMENT, zb_manager_get_ph_measurement_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_EC_MEASUREMENT, zb_manager_get_ec_measurement_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_WIND_SPEED_MEASUREMENT, zb_manager_get_wind_speed_measurement_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, zb_manager_get_carbon_dioxide_measurement_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_PM2_5_MEASUREMENT, zb_manager_get_pm2_5_measurement_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, zb_manager_get_ias_zone_cluster_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_DRLC, esp_zb_zcl_drlc_get_attr_name },
    { ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, get_electrical_measurement_attr_name },
};

const char* zb_manager_get_attr_name(uint16_t clusterID, uint16_t attr_id) {
    for (size_t i = 0; i < sizeof(cluster_attr_funcs) / sizeof(cluster_attr_funcs[0]); i++) {
        if (cluster_attr_funcs[i].cluster_id == clusterID) {
            return cluster_attr_funcs[i].func(attr_id);
        }
    }
    return "Unknown Cluster";
}

/**
 * @brief Converts esp_zb_zcl_attr_type_t enum value to a human-readable string.
 *
 * @param attr_type The attribute type to convert.
 * @return A string representation of the attribute type.
 */
const char* zb_manager_attr_type_to_string(uint8_t attr_type) {
    switch (attr_type) {
        case ESP_ZB_ZCL_ATTR_TYPE_NULL: return "ESP_ZB_ZCL_ATTR_TYPE_NULL";
        case ESP_ZB_ZCL_ATTR_TYPE_8BIT: return "ESP_ZB_ZCL_ATTR_TYPE_8BIT";
        case ESP_ZB_ZCL_ATTR_TYPE_16BIT: return "ESP_ZB_ZCL_ATTR_TYPE_16BIT";
        case ESP_ZB_ZCL_ATTR_TYPE_24BIT: return "ESP_ZB_ZCL_ATTR_TYPE_24BIT";
        case ESP_ZB_ZCL_ATTR_TYPE_32BIT: return "ESP_ZB_ZCL_ATTR_TYPE_32BIT";
        case ESP_ZB_ZCL_ATTR_TYPE_40BIT: return "ESP_ZB_ZCL_ATTR_TYPE_40BIT";
        case ESP_ZB_ZCL_ATTR_TYPE_48BIT: return "ESP_ZB_ZCL_ATTR_TYPE_48BIT";
        case ESP_ZB_ZCL_ATTR_TYPE_56BIT: return "ESP_ZB_ZCL_ATTR_TYPE_56BIT";
        case ESP_ZB_ZCL_ATTR_TYPE_64BIT: return "ESP_ZB_ZCL_ATTR_TYPE_64BIT";
        case ESP_ZB_ZCL_ATTR_TYPE_BOOL: return "ESP_ZB_ZCL_ATTR_TYPE_BOOL";
        case ESP_ZB_ZCL_ATTR_TYPE_8BITMAP: return "ESP_ZB_ZCL_ATTR_TYPE_8BITMAP";
        case ESP_ZB_ZCL_ATTR_TYPE_16BITMAP: return "ESP_ZB_ZCL_ATTR_TYPE_16BITMAP";
        case ESP_ZB_ZCL_ATTR_TYPE_24BITMAP: return "ESP_ZB_ZCL_ATTR_TYPE_24BITMAP";
        case ESP_ZB_ZCL_ATTR_TYPE_32BITMAP: return "ESP_ZB_ZCL_ATTR_TYPE_32BITMAP";
        case ESP_ZB_ZCL_ATTR_TYPE_40BITMAP: return "ESP_ZB_ZCL_ATTR_TYPE_40BITMAP";
        case ESP_ZB_ZCL_ATTR_TYPE_48BITMAP: return "ESP_ZB_ZCL_ATTR_TYPE_48BITMAP";
        case ESP_ZB_ZCL_ATTR_TYPE_56BITMAP: return "ESP_ZB_ZCL_ATTR_TYPE_56BITMAP";
        case ESP_ZB_ZCL_ATTR_TYPE_64BITMAP: return "ESP_ZB_ZCL_ATTR_TYPE_64BITMAP";
        case ESP_ZB_ZCL_ATTR_TYPE_U8: return "ESP_ZB_ZCL_ATTR_TYPE_U8";
        case ESP_ZB_ZCL_ATTR_TYPE_U16: return "ESP_ZB_ZCL_ATTR_TYPE_U16";
        case ESP_ZB_ZCL_ATTR_TYPE_U24: return "ESP_ZB_ZCL_ATTR_TYPE_U24";
        case ESP_ZB_ZCL_ATTR_TYPE_U32: return "ESP_ZB_ZCL_ATTR_TYPE_U32";
        case ESP_ZB_ZCL_ATTR_TYPE_U40: return "ESP_ZB_ZCL_ATTR_TYPE_U40";
        case ESP_ZB_ZCL_ATTR_TYPE_U48: return "ESP_ZB_ZCL_ATTR_TYPE_U48";
        case ESP_ZB_ZCL_ATTR_TYPE_U56: return "ESP_ZB_ZCL_ATTR_TYPE_U56";
        case ESP_ZB_ZCL_ATTR_TYPE_U64: return "ESP_ZB_ZCL_ATTR_TYPE_U64";
        case ESP_ZB_ZCL_ATTR_TYPE_S8: return "ESP_ZB_ZCL_ATTR_TYPE_S8";
        case ESP_ZB_ZCL_ATTR_TYPE_S16: return "ESP_ZB_ZCL_ATTR_TYPE_S16";
        case ESP_ZB_ZCL_ATTR_TYPE_S24: return "ESP_ZB_ZCL_ATTR_TYPE_S24";
        case ESP_ZB_ZCL_ATTR_TYPE_S32: return "ESP_ZB_ZCL_ATTR_TYPE_S32";
        case ESP_ZB_ZCL_ATTR_TYPE_S40: return "ESP_ZB_ZCL_ATTR_TYPE_S40";
        case ESP_ZB_ZCL_ATTR_TYPE_S48: return "ESP_ZB_ZCL_ATTR_TYPE_S48";
        case ESP_ZB_ZCL_ATTR_TYPE_S56: return "ESP_ZB_ZCL_ATTR_TYPE_S56";
        case ESP_ZB_ZCL_ATTR_TYPE_S64: return "ESP_ZB_ZCL_ATTR_TYPE_S64";
        case ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM: return "ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM";
        case ESP_ZB_ZCL_ATTR_TYPE_16BIT_ENUM: return "ESP_ZB_ZCL_ATTR_TYPE_16BIT_ENUM";
        case ESP_ZB_ZCL_ATTR_TYPE_SEMI: return "ESP_ZB_ZCL_ATTR_TYPE_SEMI";
        case ESP_ZB_ZCL_ATTR_TYPE_SINGLE: return "ESP_ZB_ZCL_ATTR_TYPE_SINGLE";
        case ESP_ZB_ZCL_ATTR_TYPE_DOUBLE: return "ESP_ZB_ZCL_ATTR_TYPE_DOUBLE";
        case ESP_ZB_ZCL_ATTR_TYPE_OCTET_STRING: return "ESP_ZB_ZCL_ATTR_TYPE_OCTET_STRING";
        case ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING: return "ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING";
        case ESP_ZB_ZCL_ATTR_TYPE_LONG_OCTET_STRING: return "ESP_ZB_ZCL_ATTR_TYPE_LONG_OCTET_STRING";
        case ESP_ZB_ZCL_ATTR_TYPE_LONG_CHAR_STRING: return "ESP_ZB_ZCL_ATTR_TYPE_LONG_CHAR_STRING";
        case ESP_ZB_ZCL_ATTR_TYPE_ARRAY: return "ESP_ZB_ZCL_ATTR_TYPE_ARRAY";
        case ESP_ZB_ZCL_ATTR_TYPE_16BIT_ARRAY: return "ESP_ZB_ZCL_ATTR_TYPE_16BIT_ARRAY";
        case ESP_ZB_ZCL_ATTR_TYPE_32BIT_ARRAY: return "ESP_ZB_ZCL_ATTR_TYPE_32BIT_ARRAY";
        case ESP_ZB_ZCL_ATTR_TYPE_STRUCTURE: return "ESP_ZB_ZCL_ATTR_TYPE_STRUCTURE";
        case ESP_ZB_ZCL_ATTR_TYPE_SET: return "ESP_ZB_ZCL_ATTR_TYPE_SET";
        case ESP_ZB_ZCL_ATTR_TYPE_BAG: return "ESP_ZB_ZCL_ATTR_TYPE_BAG";
        case ESP_ZB_ZCL_ATTR_TYPE_TIME_OF_DAY: return "ESP_ZB_ZCL_ATTR_TYPE_TIME_OF_DAY";
        case ESP_ZB_ZCL_ATTR_TYPE_DATE: return "ESP_ZB_ZCL_ATTR_TYPE_DATE";
        case ESP_ZB_ZCL_ATTR_TYPE_UTC_TIME: return "ESP_ZB_ZCL_ATTR_TYPE_UTC_TIME";
        case ESP_ZB_ZCL_ATTR_TYPE_CLUSTER_ID: return "ESP_ZB_ZCL_ATTR_TYPE_CLUSTER_ID";
        case ESP_ZB_ZCL_ATTR_TYPE_ATTRIBUTE_ID: return "ESP_ZB_ZCL_ATTR_TYPE_ATTRIBUTE_ID";
        case ESP_ZB_ZCL_ATTR_TYPE_BACNET_OID: return "ESP_ZB_ZCL_ATTR_TYPE_BACNET_OID";
        case ESP_ZB_ZCL_ATTR_TYPE_IEEE_ADDR: return "ESP_ZB_ZCL_ATTR_TYPE_IEEE_ADDR";
        case ESP_ZB_ZCL_ATTR_TYPE_128_BIT_KEY: return "ESP_ZB_ZCL_ATTR_TYPE_128_BIT_KEY";
        case ESP_ZB_ZCL_ATTR_TYPE_INVALID: return "ESP_ZB_ZCL_ATTR_TYPE_INVALID";
        default: return "Unknown";
    }
}

uint16_t zb_manager_get_attr_data_size(esp_zb_zcl_attr_type_t attr_type, void* attr_data) {
    if((attr_type == ESP_ZB_ZCL_ATTR_TYPE_OCTET_STRING) || (attr_type == ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING) || 
        (attr_type == ESP_ZB_ZCL_ATTR_TYPE_LONG_OCTET_STRING) || (attr_type == ESP_ZB_ZCL_ATTR_TYPE_LONG_CHAR_STRING) ||
        (attr_type == ESP_ZB_ZCL_ATTR_TYPE_LONG_OCTET_STRING) ) 
    {
         zbstring_t *zbstr = (zbstring_t *)attr_data;
         return(zbstr->len + 1);
    }else
    if((attr_type == ESP_ZB_ZCL_ATTR_TYPE_ARRAY))
    {

    }else
    if((attr_type == ESP_ZB_ZCL_ATTR_TYPE_16BIT_ARRAY))
    {

    }else
    if((attr_type == ESP_ZB_ZCL_ATTR_TYPE_32BIT_ARRAY))
    {

    }else
    if((attr_type == ESP_ZB_ZCL_ATTR_TYPE_STRUCTURE))
    {

    }else
    if((attr_type == ESP_ZB_ZCL_ATTR_TYPE_SET))
    {

    }else
    if((attr_type == ESP_ZB_ZCL_ATTR_TYPE_BAG))
    {

    }
    switch (attr_type) {
        case ESP_ZB_ZCL_ATTR_TYPE_NULL:             return 0;
        case ESP_ZB_ZCL_ATTR_TYPE_8BIT:             return 1;
        case ESP_ZB_ZCL_ATTR_TYPE_16BIT:            return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_24BIT:            return 3;
        case ESP_ZB_ZCL_ATTR_TYPE_32BIT:            return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_40BIT:            return 5;
        case ESP_ZB_ZCL_ATTR_TYPE_48BIT:            return 6;
        case ESP_ZB_ZCL_ATTR_TYPE_56BIT:            return 7;
        case ESP_ZB_ZCL_ATTR_TYPE_64BIT:            return 8;
        case ESP_ZB_ZCL_ATTR_TYPE_BOOL:             return 1;
        case ESP_ZB_ZCL_ATTR_TYPE_8BITMAP:          return 1;
        case ESP_ZB_ZCL_ATTR_TYPE_16BITMAP:         return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_24BITMAP:         return 3;
        case ESP_ZB_ZCL_ATTR_TYPE_32BITMAP:         return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_40BITMAP:         return 5;
        case ESP_ZB_ZCL_ATTR_TYPE_48BITMAP:         return 6;
        case ESP_ZB_ZCL_ATTR_TYPE_56BITMAP:         return 7;
        case ESP_ZB_ZCL_ATTR_TYPE_64BITMAP:         return 8;
        case ESP_ZB_ZCL_ATTR_TYPE_U8:               return 1;
        case ESP_ZB_ZCL_ATTR_TYPE_U16:              return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_U24:              return 3;
        case ESP_ZB_ZCL_ATTR_TYPE_U32:              return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_U40:              return 5;
        case ESP_ZB_ZCL_ATTR_TYPE_U48:              return 6;
        case ESP_ZB_ZCL_ATTR_TYPE_U56:              return 7;
        case ESP_ZB_ZCL_ATTR_TYPE_U64:              return 8;
        case ESP_ZB_ZCL_ATTR_TYPE_S8:               return 1;
        case ESP_ZB_ZCL_ATTR_TYPE_S16:              return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_S24:              return 3;
        case ESP_ZB_ZCL_ATTR_TYPE_S32:              return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_S40:              return 5;
        case ESP_ZB_ZCL_ATTR_TYPE_S48:              return 6;
        case ESP_ZB_ZCL_ATTR_TYPE_S56:              return 7;
        case ESP_ZB_ZCL_ATTR_TYPE_S64:              return 8;
        case ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM:        return 1;
        case ESP_ZB_ZCL_ATTR_TYPE_16BIT_ENUM:       return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_SEMI:             return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_SINGLE:           return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_DOUBLE:           return 8;
        case ESP_ZB_ZCL_ATTR_TYPE_OCTET_STRING:     return 0xFFFF; // Variable size
        case ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING:      return 0xFFFF; // Variable size
        case ESP_ZB_ZCL_ATTR_TYPE_LONG_OCTET_STRING: return 0xFFFF; // Variable size
        case ESP_ZB_ZCL_ATTR_TYPE_LONG_CHAR_STRING: return 0xFFFF; // Variable size
        case ESP_ZB_ZCL_ATTR_TYPE_ARRAY:            return 0xFFFF; // Variable size
        case ESP_ZB_ZCL_ATTR_TYPE_16BIT_ARRAY:      return 0xFFFF; // Variable size
        case ESP_ZB_ZCL_ATTR_TYPE_32BIT_ARRAY:      return 0xFFFF; // Variable size
        case ESP_ZB_ZCL_ATTR_TYPE_STRUCTURE:        return 0xFFFF; // Variable size
        case ESP_ZB_ZCL_ATTR_TYPE_SET:              return 0xFFFF; // Variable size
        case ESP_ZB_ZCL_ATTR_TYPE_BAG:              return 0xFFFF; // Variable size
        case ESP_ZB_ZCL_ATTR_TYPE_TIME_OF_DAY:      return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_DATE:             return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_UTC_TIME:         return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_CLUSTER_ID:       return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_ATTRIBUTE_ID:     return 2;
        case ESP_ZB_ZCL_ATTR_TYPE_BACNET_OID:       return 4;
        case ESP_ZB_ZCL_ATTR_TYPE_IEEE_ADDR:        return 8;
        case ESP_ZB_ZCL_ATTR_TYPE_128_BIT_KEY:      return 16;
        case ESP_ZB_ZCL_ATTR_TYPE_INVALID:          return 0xFFFF;
        default:                                    return 0xFFFF;
    }
}