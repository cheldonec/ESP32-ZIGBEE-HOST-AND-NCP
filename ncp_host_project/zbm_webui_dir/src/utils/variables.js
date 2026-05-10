/**
 * Форматирует тип переменной в читаемое имя
 * @param {number} type - Тип из прошивки (например, 0x20, 0x42)
 * @returns {string} Человекочитаемое название типа
 */
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

/**
 * Находит переменную по GUID
 * @param {Array} variables - Список переменных из useCoordinator
 * @param {string} guid - Например, "var_5"
 * @returns {Object|null}
 */
export const getVariableByGuid = (variables, guid) => {
  return variables.find(v => v.guid === guid) || null;
};

/**
 * Находит переменную по индексу
 * @param {Array} variables
 * @param {number} idx
 * @returns {Object|null}
 */
export const getVariableByIndex = (variables, idx) => {
  return variables.find(v => v.idx === idx) || null;
};