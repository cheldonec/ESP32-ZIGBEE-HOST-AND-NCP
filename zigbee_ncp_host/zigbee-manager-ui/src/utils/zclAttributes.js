// src/utils/zclAttributes.js

const ZCL_ATTR_TYPES = {
  0x00: 'null',
  0x08: '8bit', 0x09: '16bit', 0x0a: '24bit', 0x0b: '32bit',
  0x0c: '40bit', 0x0d: '48bit', 0x0e: '56bit', 0x0f: '64bit',
  0x10: 'bool',
  0x18: '8bitmap', 0x19: '16bitmap', 0x1a: '24bitmap', 0x1b: '32bitmap',
  0x20: 'u8', 0x21: 'u16', 0x22: 'u24', 0x23: 'u32',
  0x28: 's8', 0x29: 's16', 0x2a: 's24', 0x2b: 's32',
  0x30: 'enum8', 0x31: 'enum16',
  0x38: 'semi', 0x39: 'single', 0x3a: 'double',
  0x41: 'octet str', 0x42: 'char str', 0x43: 'long octet str', 0x44: 'long char str',
  0x48: 'array', 0x4c: 'struct',
  0xe0: 'time of day', 0xe1: 'date', 0xe2: 'utc time',
  0xf0: 'IEEE addr', 0xf1: '128-bit key',
  0xff: 'invalid',
  0x83: 's24',   // signed 24-bit integer
  0x84: 's32',   // дубль? убрать или переименовать
  0x85: 'float',
};

export const getAttrTypeName = (type) => ZCL_ATTR_TYPES[type] || `0x${type.toString(16)}`;

/**
 * Попробует преобразовать hex-строку в ASCII
 * @param {string} hexStr - hex без '0x'
 * @returns {string|null}
 */
function tryHexToAscii(hexStr) {
  if (hexStr.length % 2 !== 0) return null;

  const bytes = [];
  for (let i = 0; i < hexStr.length; i += 2) {
    const byte = parseInt(hexStr.substr(i, 2), 16);
    if (isNaN(byte)) return null;
    bytes.push(byte);
  }

  // Только печатные ASCII символы (32–126)
  if (!bytes.every(b => b >= 32 && b <= 126)) return null;

  return String.fromCharCode(...bytes);
}

/**
 * Форматирует значение атрибута для отображения
 * Учитывает value_hex, is_void_pointer, p_value и тип
 * @param {Object} attr - объект атрибута
 * @param {boolean} isCustom - является ли атрибут пользовательским
 * @returns {React.ReactNode|string}
 */
export const formatAttributeValue = (attr, isCustom = false) => {
  // 1. Сырые данные (приоритет)
  if (attr.is_void_pointer && attr.value_hex) {
    const hexStr = attr.value_hex.replace(/^0x/i, '');
    const ascii = tryHexToAscii(hexStr);
    return (
      <span>
        <strong>{attr.value_hex}</strong>
        {ascii && <span style={{ color: '#5c9', marginLeft: '8px' }}>("{ascii}")</span>}
      </span>
    );
  }

  // 2. p_value (если есть)
  if (attr.p_value !== undefined && attr.p_value !== null) {
    return String(attr.p_value);
  }

  // 3. value (включая null/undefined)
  if (attr.value !== undefined && attr.value !== null) {
    return String(attr.value);
  }

  // 4. value === null → явно покажем "null"
  if (attr.value === null) {
    return <span style={{ color: '#888', fontStyle: 'italic' }}>null</span>;
  }

  // 5. значение неизвестно
  return <span style={{ color: '#666' }}>—</span>;
};

/**
 * Возвращает массив стандартных атрибутов для заданного кластера
 */
export const getStandardAttrsForCluster = (clusterId, clusterData) => {
  if (!clusterData) return [];

  const attrs = [];

  if (clusterId === 0) {
    attrs.push(
      { id: 0x0000, name: 'ZCL Version', value: clusterData.zcl_version, type: 0x20 },
      { id: 0x0001, name: 'App Version', value: clusterData.app_version, type: 0x20 },
      { id: 0x0002, name: 'Stack Version', value: clusterData.stack_version, type: 0x20 },
      { id: 0x0003, name: 'HW Version', value: clusterData.hw_version, type: 0x20 },
      { id: 0x0004, name: 'Manufacturer', value: clusterData.manufacturer_name, type: 0x42 },
      { id: 0x0005, name: 'Model ID', value: clusterData.model_id, type: 0x42 },
      { id: 0x0006, name: 'Date Code', value: clusterData.date_code, type: 0x42 },
      { id: 0x0007, name: 'Power Source', value: clusterData.power_source_text, type: 0x20 },
      { id: 0x0010, name: 'Location', value: clusterData.location || 'N/A', type: 0x42 },
      { id: 0x4000, name: 'SW Build ID', value: 'N/A', type: 0x42 }
    );
  }

  if (clusterId === 1) {
    attrs.push(
      { id: 0x0020, name: 'Battery Voltage', value: `${(clusterData.battery_voltage * 0.1).toFixed(1)} V`, type: 0x20 },
      { id: 0x0021, name: 'Battery %', value: clusterData.battery_percentage === 0xFF ? 'Unknown' : `${(clusterData.battery_percentage / 2).toFixed(1)}%`, type: 0x20 },
      { id: 0x0031, name: 'Battery Size', value: clusterData.battery_size_str, type: 0x30 },
      { id: 0x0033, name: 'Battery Quantity', value: clusterData.battery_quantity, type: 0x20 },
      { id: 0x0035, name: 'Battery Rated Voltage', value: clusterData.battery_rated_voltage, type: 0x20 }
    );
  }

  if (clusterId === 1026) {
    const val = clusterData.measured_value;
    attrs.push(
      { id: 0x0000, name: 'Measured Value', value: val === 0x8000 ? 'Unknown' : `${(val / 100).toFixed(2)} °C`, type: 0x29 },
      { id: 0x0001, name: 'Min Measured Value', value: clusterData.min_measured_value === 0x8000 ? 'Unknown' : `${(clusterData.min_measured_value / 100).toFixed(2)} °C`, type: 0x29 },
      { id: 0x0002, name: 'Max Measured Value', value: clusterData.max_measured_value === 0x8000 ? 'Unknown' : `${(clusterData.max_measured_value / 100).toFixed(2)} °C`, type: 0x29 },
      { id: 0x0003, name: 'Tolerance', value: clusterData.tolerance, type: 0x21 }
    );
  }

  if (clusterId === 1029) {
    const val = clusterData.measured_value;
    attrs.push(
      { id: 0x0000, name: 'Measured Value', value: val === 0xFFFF ? 'Unknown' : `${(val / 100).toFixed(2)} %`, type: 0x21 },
      { id: 0x0001, name: 'Min Measured Value', value: clusterData.min_measured_value === 0xFFFF ? 'Unknown' : `${(clusterData.min_measured_value / 100).toFixed(2)} %`, type: 0x21 },
      { id: 0x0002, name: 'Max Measured Value', value: clusterData.max_measured_value === 0xFFFF ? 'Unknown' : `${(clusterData.max_measured_value / 100).toFixed(2)} %`, type: 0x21 },
      { id: 0x0003, name: 'Tolerance', value: clusterData.tolerance, type: 0x21 }
    );
  }

  if (clusterId === 6) {
    attrs.push(
      { id: 0x0000, name: 'On/Off', value: clusterData.on ? 'ON' : 'OFF', type: 0x10 },
      { id: 0x4000, name: 'Global Scene Control', value: 'true', type: 0x10 },
      { id: 0x4001, name: 'On Time', value: 0, type: 0x21 },
      { id: 0x4002, name: 'Off Wait Time', value: 0, type: 0x21 },
      { id: 0x4003, name: 'Start Up On/Off', value: 'Previous', type: 0x30 }
    );
  }

  return attrs;
};