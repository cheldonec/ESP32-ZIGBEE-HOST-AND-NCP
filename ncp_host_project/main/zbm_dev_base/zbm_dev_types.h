#ifndef ZBM_DEV_TYPES_H

#define ZBM_DEV_TYPES_H
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
//#include "zbm_attr_types.h"
#include "zbm_clusters_type.h"
//#include "zbm_cmd_types.h"

typedef enum {
    ZBM_DEVICE_TYPE_ON_OFF_SWITCH                 = 0x0000U,  /*!< Обычный выключатель (On/Off Switch) */
    ZBM_DEVICE_TYPE_LEVEL_CONTROL_SWITCH          = 0x0001U,  /*!< Выключатель с регулировкой яркости */
    ZBM_DEVICE_TYPE_ON_OFF_OUTPUT                 = 0x0002U,  /*!< Выход (реле, розетка) */
    ZBM_DEVICE_TYPE_LEVEL_CONTROLLABLE_OUTPUT     = 0x0003U,  /*!< Выход с регулировкой уровня */
    ZBM_DEVICE_TYPE_SCENE_SELECTOR                = 0x0004U,  /*!< Селектор сцен */
    ZBM_DEVICE_TYPE_CONFIGURATION_TOOL            = 0x0005U,  /*!< Инструмент настройки */
    ZBM_DEVICE_TYPE_REMOTE_CONTROL                = 0x0006U,  /*!< Пульт дистанционного управления */
    ZBM_DEVICE_TYPE_COMBINED_INTERFACE            = 0x0007U,  /*!< Комбинированный интерфейс */
    ZBM_DEVICE_TYPE_RANGE_EXTENDER                = 0x0008U,  /*!< Удлинитель диапазона сигнала */
    ZBM_DEVICE_TYPE_MAINS_POWER_OUTLET            = 0x0009U,  /*!< Розетка с питанием от сети */
    ZBM_DEVICE_TYPE_DOOR_LOCK_CLIENT              = 0x000AU,  /*!< Клиент замка двери */
    ZBM_DEVICE_TYPE_DOOR_LOCK_CONTROLLER          = 0x000BU,  /*!< Контроллер замка двери */
    ZBM_DEVICE_TYPE_SIMPLE_SENSOR                 = 0x000CU,  /*!< Простой датчик (без питания от сети) */
    ZBM_DEVICE_TYPE_CONSUMPTION_AWARENESS         = 0x000DU,  /*!< Устройство контроля потребления */
    ZBM_DEVICE_TYPE_HOME_GATEWAY                  = 0x0050U,  /*!< Шлюз умного дома */
    ZBM_DEVICE_TYPE_SMART_PLUG                    = 0x0051U,  /*!< Умная розетка */
    ZBM_DEVICE_TYPE_WHITE_GOODS                   = 0x0052U,  /*!< Бытовая техника (стиралка, холодильник и т.д.) */
    ZBM_DEVICE_TYPE_METER_INTERFACE               = 0x0053U,  /*!< Интерфейс счётчика */
    ZBM_DEVICE_TYPE_ON_OFF_LIGHT                  = 0x0100U,  /*!< Лампа On/Off */
    ZBM_DEVICE_TYPE_DIMMABLE_LIGHT                = 0x0101U,  /*!< Лампа с регулировкой яркости */
    ZBM_DEVICE_TYPE_COLOR_DIMMABLE_LIGHT          = 0x0102U,  /*!< Цветная лампа с регулировкой яркости */
    ZBM_DEVICE_TYPE_DIMMER_SWITCH                 = 0x0104U,  /*!< Переключатель с диммером */
    ZBM_DEVICE_TYPE_COLOR_DIMMER_SWITCH           = 0x0105U,  /*!< Цветной диммер-переключатель */
    ZBM_DEVICE_TYPE_LIGHT_SENSOR                  = 0x0106U,  /*!< Датчик освещённости */
    ZBM_DEVICE_TYPE_SHADE                         = 0x0200U,  /*!< Жалюзи / шторы */
    ZBM_DEVICE_TYPE_SHADE_CONTROLLER              = 0x0201U,  /*!< Контроллер жалюзи */
    ZBM_DEVICE_TYPE_WINDOW_COVERING               = 0x0202U,  /*!< Устройство управления оконными покрытиями */
    ZBM_DEVICE_TYPE_WINDOW_COVERING_CONTROLLER    = 0x0203U,  /*!< Контроллер оконных покрытий */
    ZBM_DEVICE_TYPE_HEATING_COOLING_UNIT          = 0x0300U,  /*!< Устройство обогрева/охлаждения */
    ZBM_DEVICE_TYPE_THERMOSTAT                    = 0x0301U,  /*!< Термостат */
    ZBM_DEVICE_TYPE_TEMPERATURE_SENSOR            = 0x0302U,  /*!< Датчик температуры */
    ZBM_DEVICE_TYPE_IAS_CONTROL_INDICATING_EQUIP  = 0x0400U,  /*!< IAS: Контроль и индикация */
    ZBM_DEVICE_TYPE_IAS_ANCILLARY_CONTROL_EQUIP   = 0x0401U,  /*!< IAS: Вспомогательное оборудование */
    ZBM_DEVICE_TYPE_IAS_ZONE                      = 0x0402U,  /*!< IAS: Зона (датчик движения, открытия и т.д.) */
    ZBM_DEVICE_TYPE_IAS_WARNING_DEVICE            = 0x0403U,  /*!< IAS: Предупреждающее устройство (сирена) */
    ZBM_DEVICE_TYPE_TEST                          = 0xFFF0U,  /*!< Тестовое устройство */
    ZBM_DEVICE_TYPE_CUSTOM_TUNNEL                 = 0xFFF1U,  /*!< Кастомный туннель (приватный профиль) */
    ZBM_DEVICE_TYPE_CUSTOM_ATTRIBUTES             = 0xFFF2U   /*!< Устройство с кастомными атрибутами */
} zbm_device_type_t;

typedef struct zbm_dev_endpoint_s {
    uint8_t                                 is_use_on_device;
    uint8_t                                 id;
    char*                                   friendlyname;
    zbm_device_type_t                       device_id;
    uint8_t                                 standart_cluster_count; // стандартные кластеры по ZCL
    zbm_standart_cluster_t**                standart_cluster_array;
    uint8_t                                 custom_cluster_count; // нестандартные кластеры (например TUYA)
    zbm_custom_cluster_t**                  custom_cluster_array;
} zbm_dev_endpoint_t;

typedef struct zbm_dev_s {
    uint8_t                                 index_in_array;
    char*                                   friendly_name;
    uint16_t                                short_addr;             //
    uint16_t                                parent_short_addr;                                   
    uint8_t                                 ieee_addr[8];
    uint8_t                                 capability;
    uint8_t                                 lqi;                // уровень качества связи
    uint32_t                                last_seen_ms;         // время последнего контакт
    uint64_t                                device_timeout_ms;
    bool                                    is_online;
    uint16_t                                manufacturer_code;
    bool                                    has_pending_read;         // флаг, была ли команда на чтение атрибутов, исп-я при старте ESP для online статуса небатареечных устройств
    bool                                    has_pending_response;     // флаг, было ли получение ответа на запросы, исп-я при старте ESP для online статуса небатареечных устройств
    uint32_t                                last_pending_read_ms;
    uint8_t                                 endpoints_count;
    zbm_dev_endpoint_t**                    endpoints_array;
    uint16_t                                last_guid_update_short_addr; // для оптимизации при регистрации в хэш, например при смене адреса
} zbm_dev_t;

/**
 * @brief Создаёт пустой эндпоинт (не привязан к устройству)
 * @return Указатель на zbm_dev_endpoint_t или NULL при ошибке
 */
zbm_dev_endpoint_t* zbm_create_empty_endpoint(void);

/**
 * @brief Найти эндпоинт по ID
 */
zbm_dev_endpoint_t* zbm_find_endpoint_by_id(zbm_dev_t* dev, uint8_t endpoint_id);

/**
 * @brief Найти стандартный кластер по ID
 */
zbm_standart_cluster_t* zbm_find_standard_cluster_by_id(zbm_dev_endpoint_t* ep, uint16_t cluster_id);

/**
 * @brief Создаёт новое устройство только по IEEE-адресу (short_addr пока неизвестен)
 * @param ieee_addr Указатель на массив из 8 байт
 * @return Указатель на zbm_dev_t или NULL при ошибке
 */
zbm_dev_t* zbm_create_device_obj_by_ieee(const uint8_t* ieee_addr);


//простая очистка эндпоинта
bool zbm_free_dev_endpoint(zbm_dev_endpoint_t* endpoint);

//простая очистка устройства
bool zbm_free_dev_t(zbm_dev_t* dev);

const char* get_device_type_name(zbm_device_type_t type);

zbm_device_type_t get_device_type_by_name(const char* name);


#endif