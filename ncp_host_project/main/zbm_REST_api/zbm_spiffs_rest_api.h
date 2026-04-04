#ifndef ZBM_SPIFFS_REST_API_H

#define ZBM_SPIFFS_REST_API_H

#include "esp_err.h"
#include "esp_http_server.h"

//GET /api/spiffs/<partition>/ls?path=/
//http://192.168.4.1/api/spiffs/config/ls
//http://192.168.4.1/api/spiffs/quirks/ls
//http://192.168.4.1/api/spiffs/certs/ls
esp_err_t spiffs_api_ls_handler(httpd_req_t* req);

//GET /api/spiffs/<partition>/get/<filename>
//http://192.168.4.1/api/spiffs/config/get/dev_0x1234.json
esp_err_t spiffs_api_get_file_handler(httpd_req_t* req);

//POST /api/spiffs/<partition>/save/<filename>
esp_err_t spiffs_api_save_file_handler(httpd_req_t* req);

//Удаление файла (для certs, quirks)  /api/spiffs/*/delete/*
esp_err_t spiffs_api_delete_file_handler(httpd_req_t* req);

///api/backup — выгрузка всех конфигурационных файлов Возвращает JSON с содержимым всех важных файлов:
//http://192.168.4.1/api/backup
esp_err_t spiffs_api_backup_handler(httpd_req_t* req);

// zbm_spiffs_rest_api.h
/*
Принимает POST /api/restore
Ожидает JSON в формате, как из /api/backup
Восстанавливает:
config/_.json → в SPIFFS_ZBM_CONF_MOUNT_POINT
quirks/_.json → в SPIFFS_ZBM_QUIRKS_MOUNT_POINT
certs/- → в SPIFFS_ZBM_CERTS_MOUNT_POINT

JavaScript
fetch('/api/restore', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(backupData)
})
.then(r => r.json())
.then(console.log);
*/
esp_err_t spiffs_api_restore_handler(httpd_req_t* req);

#endif