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
  0xff: 'invalid'
};

export const getAttrTypeName = (type) => ZCL_ATTR_TYPES[type] || `0x${type.toString(16)}`;

/**
 * Возвращает массив стандартных атрибутов для заданного кластера
 */
export const getStandardAttrsForCluster = (clusterId, clusterData) => {
  if (!clusterData) return [];

  const attrs = [];

  if (clusterId === 0) {
    attrs.push(
      { id: 0x0000, name: 'ZCL Version', value: clusterData.zcl_version, type: 0x20 },
      { id: 0x0001, name: 'App Version', value: clusterData.application_version, type: 0x20 },
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