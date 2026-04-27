set(PRIV_REQ_var
    esp_timer esp_wifi esp_http_server nvs_flash spiffs vfs driver esp_psram
)

set(SRCS_var main.c)

file(GLOB_RECURSE SRCS_dir
    "${CMAKE_CURRENT_LIST_DIR}/zbm_dev_base/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/zbm_automation/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/wifi_manager/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/zbm_web_server/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/zbm_spiffs_manager/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/zbm_REST_api/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/zbm_ncp_connect/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/SNTP_TIME/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/PS_RAM_UTILS/*.c"
    
)

list(APPEND SRCS_var ${SRCS_dir})

set(INC_DIR_var
    "." "zbm_dev_base" "zbm_automation" "wifi_manager" 
    "zbm_web_server" "zbm_spiffs_manager" "zbm_REST_api" "zbm_ncp_connect" "SNTP_TIME" "PS_RAM_UTILS"
)

# =============================================================== #
#           ГЕНЕРАЦИЯ SPIFFS ОБРАЗОВ — НАЧАЛО                      #
# =============================================================== #

# Переменные окружения и пути
set(WEB_STATIC_DIR ${CMAKE_SOURCE_DIR}/zbm_webui_dir/build)
set(QUIRKS_JSON_DIR ${CMAKE_SOURCE_DIR}/zbm_quirks_dir)
set(CONF_JSON_DIR ${CMAKE_SOURCE_DIR}/zbm_conf_dir)
set(CERTS_DIR ${CMAKE_SOURCE_DIR}/zbm_certs_dir)

set(SPIFFS_TEMP_DIR ${CMAKE_BINARY_DIR}/spiffs_temp)
set(SPIFFS_QUIRKS_TEMP_DIR ${CMAKE_BINARY_DIR}/spiffs_quirks_temp)
set(SPIFFS_CONF_TEMP_DIR ${CMAKE_BINARY_DIR}/spiffs_conf_temp)
set(SPIFFS_CERTS_TEMP_DIR ${CMAKE_BINARY_DIR}/spiffs_certs_temp)

set(SPIFFS_UI_IMAGE ${CMAKE_BINARY_DIR}/spiffs_zbm_webui.bin)
set(SPIFFS_QUIRKS_IMAGE ${CMAKE_BINARY_DIR}/spiffs_zbm_quirks.bin)
set(SPIFFS_CONF_IMAGE ${CMAKE_BINARY_DIR}/spiffs_zbm_conf.bin)
set(SPIFFS_CERTS_IMAGE ${CMAKE_BINARY_DIR}/spiffs_zbm_certs.bin)

# Python и IDF инструменты
# Найти интерпретатор Python
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(PYTHON3 ${Python3_EXECUTABLE})

# Диагностика
message(STATUS "✅ Using Python: ${PYTHON3}")
execute_process(COMMAND ${PYTHON3} --version OUTPUT_VARIABLE PY_VER OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "📦 Python version: ${PY_VER}")

# === Сборка spiffs_zbm_ui.bin (размер из partitions.csv: 4M = 0x400000) ===
if(EXISTS ${WEB_STATIC_DIR})
    add_custom_command(
        OUTPUT ${SPIFFS_UI_IMAGE}  # ← исправлено: было ${SPIFFS_ZBM_UI_IMAGE}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${SPIFFS_TEMP_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIFFS_TEMP_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${WEB_STATIC_DIR} ${SPIFFS_TEMP_DIR}
        # Удаляем ненужные файлы
        COMMAND ${CMAKE_COMMAND} -E remove
            ${SPIFFS_TEMP_DIR}/*.bak
            ${SPIFFS_TEMP_DIR}/*~
            ${SPIFFS_TEMP_DIR}/static/js/*.map
            ${SPIFFS_TEMP_DIR}/static/css/*.map
            ${SPIFFS_TEMP_DIR}/asset-manifest.json
            ${SPIFFS_TEMP_DIR}/robots.txt
            ${SPIFFS_TEMP_DIR}/LICENSE
        COMMAND ${PYTHON3} ${IDF_PATH}/components/spiffs/spiffsgen.py
            --page-size 256
            --block-size 4096
            --obj-name-len 64
            --meta-len 4
            --use-magic
            --use-magic-len
            0x400000  # 4M, как в partitions.csv: zbm_ui
            ${SPIFFS_TEMP_DIR}
            ${SPIFFS_UI_IMAGE}
        DEPENDS ${SRCS_var}
        COMMENT "🔨 Building SPIFFS image for ZBM UI (4M)..."
    )

    add_custom_target(spiffs_zbm_ui ALL DEPENDS ${SPIFFS_UI_IMAGE})

    add_custom_command(TARGET spiffs_zbm_ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "💡 Run 'make flash-ui' to flash UI partition"
    )
else()
    message(WARNING "⚠️ WEB_STATIC_DIR not found: ${WEB_STATIC_DIR}")
endif()

# === Сборка spiffs_zbm_quirks.bin (размер: 0x50000 = 320KB) ===
if(EXISTS ${QUIRKS_JSON_DIR}/index.json)
    add_custom_command(
        OUTPUT ${SPIFFS_QUIRKS_IMAGE}  # ← исправлено: было ${SPIFFS_ZBM_QUIRKS_IMAGE}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${SPIFFS_QUIRKS_TEMP_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIFFS_QUIRKS_TEMP_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${QUIRKS_JSON_DIR} ${SPIFFS_QUIRKS_TEMP_DIR}
        COMMAND ${PYTHON3} ${IDF_PATH}/components/spiffs/spiffsgen.py
            --page-size 256
            --block-size 4096
            --obj-name-len 64
            --meta-len 4
            --use-magic
            --use-magic-len
            0x50000  # 320KB, как в partitions.csv: zbm_quirks
            ${SPIFFS_QUIRKS_TEMP_DIR}
            ${SPIFFS_QUIRKS_IMAGE}
        DEPENDS ${SRCS_var}
        COMMENT "🔧 Building SPIFFS image for quirks..."
    )

    add_custom_target(spiffs_zbm_quirks ALL DEPENDS ${SPIFFS_QUIRKS_IMAGE})

    add_custom_command(TARGET spiffs_zbm_quirks POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "🔧 Run 'make flash-quirks' to flash quirks partition"
    )
else()
    message(WARNING "⚠️ index.json not found in: ${QUIRKS_JSON_DIR}/index.json")
endif()

# === Сборка spiffs_zbm_conf.bin (размер из partitions.csv: 2M = 0x200000) ===
if(EXISTS ${CONF_JSON_DIR})
    add_custom_command(
        OUTPUT ${SPIFFS_CONF_IMAGE}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${SPIFFS_CONF_TEMP_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIFFS_CONF_TEMP_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CONF_JSON_DIR} ${SPIFFS_CONF_TEMP_DIR}
        # Можно удалить временные файлы, если нужно
        COMMAND ${PYTHON3} ${IDF_PATH}/components/spiffs/spiffsgen.py
            --page-size 256
            --block-size 4096
            --obj-name-len 64
            --meta-len 4
            --use-magic
            --use-magic-len
            0x200000  # 2M, как в partitions.csv: zbm_conf
            ${SPIFFS_CONF_TEMP_DIR}
            ${SPIFFS_CONF_IMAGE}
        DEPENDS ${SRCS_var}
        COMMENT "🔧 Building SPIFFS image for config..."
    )

    add_custom_target(spiffs_zbm_conf ALL DEPENDS ${SPIFFS_CONF_IMAGE})

    add_custom_command(TARGET spiffs_zbm_conf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "⚙️ Run 'make flash-conf' to flash config partition"
    )
else()
    message(WARNING "⚠️ CONF_JSON_DIR not found: ${CONF_JSON_DIR}")
endif()

# === Сборка spiffs_zbm_certs.bin (размер из partitions.csv: 0x20000 = 128KB) ===
if(EXISTS ${CERTS_DIR})
    add_custom_command(
        OUTPUT ${SPIFFS_CERTS_IMAGE}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${SPIFFS_CERTS_TEMP_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIFFS_CERTS_TEMP_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CERTS_DIR} ${SPIFFS_CERTS_TEMP_DIR}
        COMMAND ${PYTHON3} ${IDF_PATH}/components/spiffs/spiffsgen.py
            --page-size 256
            --block-size 4096
            --obj-name-len 64
            --meta-len 4
            --use-magic
            --use-magic-len
            0x20000  # 128KB, как в partitions.csv: zbm_certs
            ${SPIFFS_CERTS_TEMP_DIR}
            ${SPIFFS_CERTS_IMAGE}
        DEPENDS ${SRCS_var}
        COMMENT "🔐 Building SPIFFS image for certificates..."
    )

    add_custom_target(spiffs_zbm_certs ALL DEPENDS ${SPIFFS_CERTS_IMAGE})

    add_custom_command(TARGET spiffs_zbm_certs POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "🔐 Run 'make flash-certs' to flash certs partition"
    )
else()
    message(WARNING "⚠️ CERTS_DIR not found: ${CERTS_DIR}")
endif()
# =============================================================== #
#           РЕГИСТРАЦИЯ КОМПОНЕНТА — ВАЖНО: В КОНЦЕ!                #
# =============================================================== #

idf_component_register(
    SRCS ${SRCS_var}
    INCLUDE_DIRS ${INC_DIR_var}
    PRIV_REQUIRES ${PRIV_REQ_var}
)

# =============================================================== #
#                   АЛИАСЫ ДЛЯ ПРОШИВКИ                            #
# =============================================================== #

if(TARGET spiffs_zbm_ui)
    add_custom_target(flash-ui
        COMMAND parttool.py --port $ENV{ESPPORT} write_partition --partition-name=zbm_ui --input ${SPIFFS_ZBM_UI_IMAGE}
        DEPENDS spiffs_zbm_ui
    )
    message(STATUS "✅ make flash-ui — прошивает раздел zbm_ui")
endif()

if(TARGET spiffs_zbm_quirks)
    add_custom_target(flash-quirks
        COMMAND parttool.py --port $ENV{ESPPORT} write_partition --partition-name=zbm_quirks --input ${SPIFFS_ZBM_QUIRKS_IMAGE}
        DEPENDS spiffs_zbm_quirks
    )
    message(STATUS "✅ make flash-quirks — прошивает раздел zbm_quirks")
endif()

if(TARGET spiffs_zbm_conf)
    add_custom_target(flash-conf
        COMMAND parttool.py --port $ENV{ESPPORT} write_partition --partition-name=zbm_conf --input ${SPIFFS_CONF_IMAGE}
        DEPENDS spiffs_zbm_conf
    )
    message(STATUS "✅ make flash-conf — прошивает раздел zbm_conf")
endif()

if(TARGET spiffs_zbm_certs)
    add_custom_target(flash-certs
        COMMAND parttool.py --port $ENV{ESPPORT} write_partition --partition-name=zbm_certs --input ${SPIFFS_CERTS_IMAGE}
        DEPENDS spiffs_zbm_certs
    )
    message(STATUS "✅ make flash-certs — прошивает раздел zbm_certs")
endif()
