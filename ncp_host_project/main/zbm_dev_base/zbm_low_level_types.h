#ifndef ZBM_LOW_LEVEL_TYPES_H

#define ZBM_LOW_LEVEL_TYPES_H


#define ZBM_ADDR_UNKNOWN     0xFFFE
#define ZBM_ADDR_BROADCAST   0xFFFF

/**
 * @brief Роль кластера (сервер или клиент)
 */
typedef enum {
    ZBM_CLUSTER_ROLE_SERVER = 0x01U,         /*!< Серверная роль кластера */
    ZBM_CLUSTER_ROLE_CLIENT = 0x02U,         /*!< Клиентская роль кластера */
} zbm_cluster_role_t;

#endif