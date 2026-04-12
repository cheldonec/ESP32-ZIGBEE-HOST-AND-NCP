🔧 Формат сообщения
JSON
{
  "event": "attribute_updated",
  "guid": "string",
  "type": number,
  "value_bytes": [number, ...]
}
Поле	Тип	Описание
event	string	Тип события — всегда "attribute_updated"
guid	string	Уникальный идентификатор устройства или конкретного endpoint'а (например, 0x1234_ep1_onoff)
type	number	Тип данных ZCL (см. таблицу ниже)
value_bytes	array of numbers	Значение атрибута в виде массива байтов (uint8_t[])
🧩 Поддерживаемые типы данных ZCL (частичный список)
Тип (число)	Имя типа	Описание	Пример использования
0x10	BOOLEAN	Логическое значение	onOff — включено/выключено
0x20	UINT8	8-битное беззнаковое число	Уровень яркости, режим датчика
0x21	UINT16	16-битное беззнаковое число	Температура x100, мощность
0x22	UINT24	24-битное беззнаковое число	Редко используется
0x23	UINT32	32-битное беззнаковое число	Время, энергия, счётчики
0x30	ENUM8	8-битное перечисление	Состояние устройства, режим
0x31	ENUM16	16-битное перечисление	Более сложные режимы
0x42	CHAR_STR	Строка (в байтах)	Текстовое описание, модель
0x50	BITMAP8	8-битная маска	Флаги состояния
0x51	BITMAP16	16-битная маска	Несколько флагов
0x52	BITMAP32	32-битная маска	Расширенные флаги

⚠️ Полный список: см. ZCL Spec раздел 2.5.2.


✅ Примеры сообщений
1. Лампочка включилась / выключилась
JSON
{
  "event": "attribute_updated",
  "guid": "0x1234_ep1_onoff",
  "type": 16,
  "value_bytes": [1]
}
guid: Устройство 0x1234, endpoint 1, кластер OnOff
type: BOOLEAN (0x10 = 16)
value_bytes[0]: 1 = включено, 0 = выключено
2. Яркость лампы изменена до 128 (из 255)
JSON
{
  "event": "attribute_updated",
  "guid": "0x1234_ep1_level",
  "type": 32,
  "value_bytes": [128]
}
type: UINT8 (0x20 = 32)
value_bytes[0]: текущая яркость
3. Температура с датчика (в сотых градуса)
JSON
{
  "event": "attribute_updated",
  "guid": "0x5678_ep1_temperature",
  "type": 33,
  "value_bytes": [0x1F, 0x90]  // 8080 → 80.80°C
}
type: UINT16 (0x21 = 33)
Значение: little-endian → 0x901F = 36895 → делится на 100 → 368.95°C (или зависит от кластера)

💡 Для кластера Temperature Measurement: значение в десятых/сотых градуса Цельсия.


4. Датчик движения сработал
JSON
{
  "event": "attribute_updated",
  "guid": "0xABCD_ep1_iaszone",
  "type": 16,
  "value_bytes": [1]
}
1 = тревога (motion detected), 0 = всё спокойно
5. Название устройства (строка)
JSON
{
  "event": "attribute_updated",
  "guid": "0x1234_ep1_devname",
  "type": 66,
  "value_bytes": [72, 101, 108, 108, 111]  // "Hello"
}
type: CHAR_STR (0x42 = 66)
Преобразуй байты в строку: String.fromCharCode(...value_bytes) → "Hello"
🛠️ Пример обработки в UI (JavaScript)
JavaScript
function handleWebSocketMessage(data) {
  if (data.event === 'attribute_updated') {
    const { guid, type, value_bytes } = data;

    // Преобразуем байты в значение
    let value;
    switch (type) {
      case 16: // BOOLEAN
        value = !!value_bytes[0];
        break;

      case 32: // UINT8
        value = value_bytes[0];
        break;

      case 33: // UINT16 (little-endian)
        value = value_bytes[0] + (value_bytes[1] << 8);
        break;

      case 66: // CHAR_STR
        value = String.fromCharCode(...value_bytes.filter(b => b !== 0)); // убираем null-байты
        break;

      default:
        console.log(`Unsupported type ${type}`, value_bytes);
        return;
    }

    // Обновляем состояние устройства
    updateDeviceState(guid, value);
  }
}

function updateDeviceState(guid, value) {
  const el = document.getElementById(guid);
  if (el) {
    if (typeof value === 'boolean') {
      el.textContent = value ? 'ON' : 'OFF';
      el.className = value ? 'on' : 'off';
    } else {
      el.textContent = value;
    }
  }
}