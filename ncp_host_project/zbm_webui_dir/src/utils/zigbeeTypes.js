// utils/zigbeeTypes.js

const ZigbeeTypes = {
  // Простые типы
  BOOL: 16,
  U8: 32,
  S8: 40,
  U16: 33,
  S16: 41,
  U24: 34,
  U32: 35,
  S24: 42,
  S32: 43,
  U40: 36,
  U48: 37,
  U56: 38,
  U64: 39,
  S40: 44,
  S48: 45,
  S56: 46,
  S64: 47,
  E8: 48,
  E16: 49,

  // Плавающая точка
  SEMI: 56,    // 2-byte float
  SINGLE: 57,  // float
  DOUBLE: 58,  // double

  // Строки
  CHAR_STRING: 66,
  LONG_CHAR_STRING: 67,
  OCTET_STRING: 65,
  LONG_OCTET_STRING: 68,

  // Адреса
  IEEE_ADDR: 72,

  // Идентификаторы
  CLUSTER_ID: 85,
  ATTRIBUTE_ID: 86,

  // Время и дата
  UTC_TIME: 108,
  TIME_OF_DAY: 109,
  DATE: 110,

  // Сложные типы (не поддерживаются напрямую)
  ARRAY: 48,
  STRUCTURE: 51,
  SET: 52,
  BAG: 53,
};

export const ZigbeeTypeNames = {
  16: 'bool',
  32: 'uint8',
  40: 'int8',
  33: 'uint16',
  41: 'int16',
  34: 'uint24',
  35: 'uint32',
  42: 'int24',
  43: 'int32',
  36: 'uint40',
  37: 'uint48',
  38: 'uint56',
  39: 'uint64',
  44: 'int40',
  45: 'int48',
  46: 'int56',
  47: 'int64',

  56: 'semi',
  57: 'float',
  58: 'double',

  66: 'char_str',
  67: 'long_char_str',
  65: 'octet_str',
  68: 'long_octet_str',

  72: 'ieee_addr',

  85: 'cluster_id',
  86: 'attr_id',

  108: 'utc_time',
  109: 'time_of_day',
  110: 'date',

  48: 'enum8',   // ← важно!
  51: 'struct',
  52: 'set',
  53: 'bag',

  default: 'unknown'
};

/**
 * Преобразует значение по типу Zigbee в читаемый JS-объект
 * @param {number} type - тип атрибута (Zigbee ZCL type)
 * @param {Buffer|Uint8Array} buffer - бинарные данные
 * @returns {*} читаемое значение
 */
function fromZigbeeType(type, buffer) {
  if (!buffer || buffer.length === 0) return null;

  switch (type) {
    case ZigbeeTypes.BOOL:
      return !!buffer[0];

    case ZigbeeTypes.U8:
      return buffer[0];

    case ZigbeeTypes.S8:
      return buffer[0] >= 128 ? buffer[0] - 256 : buffer[0];

    case ZigbeeTypes.U16:
      return buffer[0] + (buffer[1] << 8);

    case ZigbeeTypes.S16:
      const val16 = buffer[0] + (buffer[1] << 8);
      return val16 >= 32768 ? val16 - 65536 : val16;

    case ZigbeeTypes.U24:
    case ZigbeeTypes.U32:
      return (
        buffer[0] +
        (buffer[1] << 8) +
        (buffer[2] << 16) +
        ((buffer[3] || 0) << 24)
      );

    case ZigbeeTypes.S24:
    case ZigbeeTypes.S32:
      let val32 =
        buffer[0] +
        (buffer[1] << 8) +
        (buffer[2] << 16) +
        ((buffer[3] || 0) << 24);
      if (val32 >= 2147483648) val32 -= 4294967296;
      return val32;

    case ZigbeeTypes.U40:
    case ZigbeeTypes.U48:
    case ZigbeeTypes.U56:
    case ZigbeeTypes.U64:
      return '0x' + Array.from(buffer.slice(0, 8))
        .reverse()
        .map(b => b.toString(16).padStart(2, '0'))
        .join('');

    case ZigbeeTypes.S40:
    case ZigbeeTypes.S48:
    case ZigbeeTypes.S56:
    case ZigbeeTypes.S64:
      const isNegative = buffer.length > 7 && (buffer[7] & 0x80);
      const abs = Array.from(buffer.slice(0, 8))
        .reverse()
        .map(b => b.toString(16).padStart(2, '0'))
        .join('');
      return isNegative ? '-0x' + abs : '0x' + abs;

    case ZigbeeTypes.SEMI:
    case ZigbeeTypes.SINGLE:
      const buf32 = new Uint8Array(4);
      buf32.set(buffer.slice(0, 4));
      const floatView = new DataView(buf32.buffer);
      return floatView.getFloat32(0, true); // little-endian

    case ZigbeeTypes.DOUBLE:
      const buf64 = new Uint8Array(8);
      buf64.set(buffer.slice(0, 8));
      const doubleView = new DataView(buf64.buffer);
      return doubleView.getFloat64(0, true);

    case ZigbeeTypes.CHAR_STRING:
    case ZigbeeTypes.LONG_CHAR_STRING:
      const len = buffer[0];
      const strBytes = buffer.slice(1, 1 + len);
      return new TextDecoder().decode(strBytes);

    case ZigbeeTypes.OCTET_STRING:
    case ZigbeeTypes.LONG_OCTET_STRING:
      return Array.from(buffer)
        .map((b) => b.toString(16).padStart(2, '0').toUpperCase())
        .join('');

    case ZigbeeTypes.IEEE_ADDR:
      if (buffer.length >= 8) {
        return Array.from(buffer)
          .reverse() // Уточнить порядок! В Zigbee — little-endian
          .map((b) => b.toString(16).padStart(2, '0'))
          .join(':');
      }
      return '<invalid>';

    case ZigbeeTypes.CLUSTER_ID:
    case ZigbeeTypes.ATTRIBUTE_ID:
      return `0x${(buffer[0] + (buffer[1] << 8)).toString(16).padStart(4, '0').toUpperCase()}`;

    case ZigbeeTypes.UTC_TIME:
      return buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);

    case ZigbeeTypes.TIME_OF_DAY:
      const h = buffer[0], m = buffer[1], s = buffer[2], hun = buffer[3];
      return `${h}:${m}:${s}.${hun}`;

    case ZigbeeTypes.DATE:
      const year = 2000 + buffer[0];
      const month = buffer[1];
      const day = buffer[2];
      const dow = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'][buffer[3]];
      return `${year}-${month}-${day} (${dow})`;

    case ZigbeeTypes.ARRAY:
    case ZigbeeTypes.STRUCTURE:
    case ZigbeeTypes.SET:
    case ZigbeeTypes.BAG:
      return '<complex>';

    default:
      return `<raw:${type}>`;
  }
}

/**
 * Обратная функция: преобразует JS-значение в буфер по типу Zigbee
 * @param {number} type - тип атрибута
 * @param {*} value - значение (строка, число и т.д.)
 * @returns {Uint8Array|null}
 */
function toZigbeeType(type, value) {
  if (value == null) return null;

  switch (type) {
    case ZigbeeTypes.BOOL:
      return new Uint8Array([value ? 1 : 0]);

    case ZigbeeTypes.U8:
      return new Uint8Array([Number(value) & 0xFF]);

    case ZigbeeTypes.S8:
      const s8 = Number(value);
      return new Uint8Array([s8 < 0 ? s8 + 256 : s8]);

    case ZigbeeTypes.U16:
      const u16 = Number(value) & 0xFFFF;
      return new Uint8Array([u16 & 0xFF, (u16 >> 8) & 0xFF]);

    case ZigbeeTypes.S16:
      const n = Number(value);
      const v16 = n < 0 ? n + 65536 : n & 0xFFFF;
      return new Uint8Array([v16 & 0xFF, (v16 >> 8) & 0xFF]);

    case ZigbeeTypes.U24:
    case ZigbeeTypes.U32:
      const u32 = Number(value) >>> 0;
      return new Uint8Array([
        u32 & 0xFF,
        (u32 >> 8) & 0xFF,
        (u32 >> 16) & 0xFF,
        (u32 >> 24) & 0xFF,
      ]);

    case ZigbeeTypes.S32:
      const s32 = Number(value);
      const v32 = s32 < 0 ? s32 + 4294967296 : s32 >>> 0;
      return new Uint8Array([
        v32 & 0xFF,
        (v32 >> 8) & 0xFF,
        (v32 >> 16) & 0xFF,
        (v32 >> 24) & 0xFF,
      ]);

    case ZigbeeTypes.U64:
    case ZigbeeTypes.S64:
      console.warn('U64/S64 not implemented without BigInt');
      return null;

    case ZigbeeTypes.SINGLE:
      const f32 = new Float32Array([Number(value)]);
      const u32arr = new Uint8Array(f32.buffer);
      return u32arr;

    case ZigbeeTypes.DOUBLE:
      const f64 = new Float64Array([Number(value)]);
      return new Uint8Array(f64.buffer);

    case ZigbeeTypes.CHAR_STRING:
    case ZigbeeTypes.LONG_CHAR_STRING:
      const encoder = new TextEncoder();
      const strBytes = encoder.encode(String(value));
      const lenBuf = new Uint8Array([strBytes.length]);
      return concatTypedArrays(lenBuf, strBytes);

    case ZigbeeTypes.OCTET_STRING:
    case ZigbeeTypes.LONG_OCTET_STRING:
      const hex = String(value).replace(/[^a-fA-F0-9]/g, '');
      const bytes = [];
      for (let i = 0; i < hex.length; i += 2) {
        bytes.push(parseInt(hex.substr(i, 2), 16));
      }
      return new Uint8Array(bytes);

    case ZigbeeTypes.IEEE_ADDR:
      const parts = String(value).split(':').map((x) => parseInt(x, 16));
      if (parts.length !== 8) return null;
      return new Uint8Array(parts.reverse()); // little-endian

    case ZigbeeTypes.CLUSTER_ID:
    case ZigbeeTypes.ATTRIBUTE_ID:
      const id = parseInt(String(value).replace('0x', ''), 16);
      return new Uint8Array([id & 0xFF, (id >> 8) & 0xFF]);

    case ZigbeeTypes.UTC_TIME:
      const utc = Number(value);
      return new Uint8Array([
        utc & 0xFF,
        (utc >> 8) & 0xFF,
        (utc >> 16) & 0xFF,
        (utc >> 24) & 0xFF,
      ]);

    case ZigbeeTypes.TIME_OF_DAY:
      const [h, m, s, hun] = String(value).split(/[:.]/).map(Number);
      return new Uint8Array([h || 0, m || 0, s || 0, hun || 0]);

    case ZigbeeTypes.DATE:
      const match = /(\d{4})-(\d{1,2})-(\d{1,2})/.exec(String(value));
      if (!match) return null;
      const y = Number(match[1]) - 2000;
      const mo = Number(match[2]);
      const d = Number(match[3]);
      return new Uint8Array([y, mo, d, 0]); // DOW пока 0

    default:
      return null;
  }
}

// Вспомогательная: склеивает два TypedArray
function concatTypedArrays(a, b) {
  const c = new Uint8Array(a.length + b.length);
  c.set(a, 0);
  c.set(b, a.length);
  return c;
}

export { ZigbeeTypes, fromZigbeeType, toZigbeeType };