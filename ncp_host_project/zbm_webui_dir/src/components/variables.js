// src/components/variables.js
import { useVariables } from '../hooks/useVariables';

// Экспортируем только интерфейс — теперь переменные динамические
export const virtualVariables = []; // будет переопределено при рендере

export const getVariable = (guid) => {
  const idx = parseInt(guid.replace('var_', ''), 10);
  if (isNaN(idx)) return null;
  // Мы не можем использовать useVariables здесь напрямую — это хук
  // Поэтому используем косвенный способ
  return null; // будет передано через пропсы или контекст
};

export const formatDataType = (type) => {
  const types = {
    0x20: 'uint8',
    0x28: 'int8',
    0x21: 'uint16',
    0x29: 'int16',
    0x42: 'char_string',
    0x44: 'long_char_string'
  };
  return types[type] || `unknown(0x${type.toString(16)})`;
};