// src/components/variables.js

export const virtualVariables = [
  { guid: 'var_0', name: 'Ночное время', type: 16 },   // bool
  { guid: 'var_1', name: 'Освещённость', type: 32 },  // uint8_t
];

export function getVariable(guid) {
  return virtualVariables.find(v => v.guid === guid);
}

export function formatDataType(type) {
  const types = {
    16: 'bool',
    32: 'uint8_t',
    33: 'uint16_t',
    35: 'uint32_t',
    48: 'enum',
    66: 'char_str',
    72: 'long_char_str',
    65: 'octet_str',
    71: 'long_octet_str'
  };
  return types[type] || `unknown (${type})`;
}