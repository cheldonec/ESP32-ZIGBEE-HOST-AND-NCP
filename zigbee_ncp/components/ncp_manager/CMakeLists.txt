idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       #PRIV_INCLUDE_DIRS "src/priv"
                       PRIV_REQUIRES esp-zigbee-lib nvs_flash driver zb_manager)