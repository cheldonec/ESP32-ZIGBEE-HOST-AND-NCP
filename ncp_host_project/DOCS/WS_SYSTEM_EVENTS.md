# 📡 WebSocket: Системные события (System Notifications)

> Все системные уведомления отправляются через WebSocket (`/ws`) с типом события `"event": "system_notify"`.

## 🔧 Формат сообщения

```json
{
  "event": "system_notify",
  "type": "string",
  "message": "string",
  "data": { ... } // опционально
}
Поле	Тип	Описание
event	string	Тип события — всегда "system_notify"
type	string	Кодовое имя события
message	string	Человекочитаемое описание
data	object/null	Дополнительные данные (если есть)
✅ Доступные события
1. zigbee_network_up — Zigbee-сеть успешно создана
Когда отправляется:

После успешного выполнения ESP_ZB_BDB_SIGNAL_FORMATION.

Пример:
JSON
{
  "event": "system_notify",
  "type": "zigbee_network_up",
  "message": "Zigbee network formed successfully",
  "data": {
    "channel": 15,
    "pan_id": 6789,
    "ieee": "00124B0009ABCD12"
  }
}
Поле	Тип	Описание
channel	number	Радиоканал Zigbee (например, 11, 15, 20, 25)
pan_id	number	PAN ID сети в десятичном виде
ieee	string	IEEE-адрес координатора (без двоеточий, 16 символов)
2. zigbee_network_error — Ошибка формирования сети
Когда отправляется:

Если ESP_ZB_BDB_SIGNAL_FORMATION завершился с ошибкой.

Пример:
JSON
{
  "event": "system_notify",
  "type": "zigbee_network_error",
  "message": "Failed to form Zigbee network",
  "data": {
    "error": "ESP_ERR_TIMEOUT"
  }
}
Поле	Тип	Описание
error	string	Код ошибки ESP-IDF (например, ESP_ERR_TIMEOUT, ESP_ERR_NO_MEM)
3. zigbee_permit_join_started — Сеть открыта для подключения устройств
Когда отправляется:

При получении ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS с duration > 0.

Пример:
JSON
{
  "event": "system_notify",
  "type": "zigbee_permit_join_started",
  "message": "Zigbee network is now open for device joining",
  "data": {
    "duration": 60,
    "pan_id": 6789
  }
}
Поле	Тип	Описание
duration	number	Время, на которое сеть открыта (в секундах)
pan_id	number	PAN ID текущей сети
4. zigbee_permit_join_stopped — Сеть закрыта для подключения
Когда отправляется:

При получении ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS с duration == 0.

Пример:
JSON
{
  "event": "system_notify",
  "type": "zigbee_permit_join_stopped",
  "message": "Zigbee network closed for new devices",
  "data": {
    "pan_id": 6789
  }
}
Поле	Тип	Описание
pan_id	number	PAN ID текущей сети
🛠️ Пример обработки в UI (React / JavaScript)
JavaScript
function handleWebSocketMessage(data) {
  if (data.event === 'system_notify') {
    switch (data.type) {
      case 'zigbee_network_up':
        showToast(`✅ Сеть запущена: канал ${data.data.channel}`);
        break;

      case 'zigbee_network_error':
        showToast(`❌ Ошибка: ${data.data.error}`, 'error');
        break;

      case 'zigbee_permit_join_started':
        showToast(`🔓 Устройства могут подключаться (${data.data.duration} сек)`);
        break;

      case 'zigbee_permit_join_stopped':
        showToast('🔒 Подключение закрыто');
        break;

      default:
        console.warn('Неизвестное системное событие:', data.type);
    }
  }
}