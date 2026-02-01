idf_component_register(
        INCLUDE_DIRS "include" "devices" "devices/include"
        SRCS    "devices/zb_manager_devices.c" "devices/zb_manager_clusters.c"
                "devices/zb_manager_basic_cluster.c"
                "devices/zb_manager_identify_cluster.c"
                "zb_manager_utility_node_desc.c" "zb_manager_utility_ieee_addr.c" "zb_manager_utility_simple_desc.c" "zb_manager.c" "zb_manager_endpoints_gen.c" 
                            "zb_manager_utility_match_desc.c"
                            "zb_raw_action_handle.c" "zb_aps_handler.c" 
                            "zb_manager_endpoints_gen.c" "zb_aps_handler.c" "zb_raw_action_handle.c"
                            "zb_handlers.c" "zb_manager_cluster_data_helper.c" "zb_manager_tuya_helper.c" "zb_manager_lqi_rssi_cache.c"
                    #INCLUDE_DIRS "include" "devices"
                    REQUIRES "nvs_flash" "esp_timer" "ncp_manager")

