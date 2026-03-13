// src/components/GlobalVariablesSection.js
import React, { useState } from 'react';

const VAR_CONFIG = {
  0: { name: 'Утро', emoji: '🌅' },
  1: { name: 'Кто-то дома', emoji: '👨‍👩‍👧‍👦' },
  2: { name: 'Режим "Нет дома"', emoji: '🏠❌' },
  3: { name: 'Праздник/отпуск', emoji: '🎉' },
  4: { name: 'Ночной режим', emoji: '🌙' },
  5: { name: 'Ворота открыты', emoji: '🚗🚪' },
  6: { name: 'Сигнализация активна', emoji: '🔔' },
  7: { name: 'Счётчик кликов', emoji: '🔢' },
  8: { name: 'Ручное управление светом', emoji: '✋💡' },
  9: { name: 'Система готова', emoji: '✅' },
  10: { name: 'Окно открыто', emoji: '🪟' },
  11: { name: 'Котёл работает', emoji: '🔥' },
  12: { name: 'Закат прошёл', emoji: '🌇' },
  13: { name: 'TV режим', emoji: '📺' },
  14: { name: 'Дверь открывалась сегодня', emoji: '🚪📅' },
  15: { name: 'ID последнего правила', emoji: '🔄' },
  // Пользовательские переменные (16–31)
  16: { name: 'USER_VAR[1]', emoji: '📦' },
  17: { name: 'USER_VAR[2]', emoji: '📦' },
  18: { name: 'USER_VAR[3]', emoji: '📦' },
  19: { name: 'USER_VAR[4]', emoji: '📦' },
  20: { name: 'USER_VAR[5]', emoji: '📦' },
  21: { name: 'USER_VAR[6]', emoji: '📦' },
  22: { name: 'USER_VAR[7]', emoji: '📦' },
  23: { name: 'USER_VAR[8]', emoji: '📦' },
  24: { name: 'USER_VAR[9]', emoji: '📦' },
  25: { name: 'USER_VAR[10]', emoji: '📦' },
  26: { name: 'USER_VAR[11]', emoji: '📦' },
  27: { name: 'USER_VAR[12]', emoji: '📦' },
  28: { name: 'USER_VAR[13]', emoji: '📦' },
  29: { name: 'USER_VAR[14]', emoji: '📦' },
  30: { name: 'USER_VAR[15]', emoji: '📦' },
  31: { name: 'USER_VAR[16]', emoji: '📦' },
};

export const GlobalVariablesSection = ({ variables }) => {
  const [expanded, setExpanded] = useState(false);

  // Хранит значения полей ввода для каждой переменной (по индексу)
  const [inputValues, setInputValues] = useState({});

  const sendWsCommand = (cmd, varIndex, value = null) => {
    try {
      const ws = new WebSocket(`ws://${window.location.host}/ws`);
      ws.onopen = () => {
        const msg = { cmd, var_index: varIndex };
        if (value !== null) msg.value = value;
        ws.send(JSON.stringify(msg));
        setTimeout(() => ws.close(), 100);
      };
    } catch (err) {
      console.error('Ошибка WebSocket:', err);
    }
  };

  // Обработчик изменения поля ввода
  const handleInputChange = (idx, value) => {
    setInputValues((prev) => ({
      ...prev,
      [idx]: value,
    }));
  };

  // Обработчик кнопки SET
  const handleSet = (idx) => {
    const valStr = inputValues[idx];
    const val = parseInt(valStr, 10);
    if (!isNaN(val) && val >= 0 && val <= 255) {
      sendWsCommand('set_virtual_var', idx, val);
      // Опционально: очистить поле после отправки
      setInputValues((prev) => ({ ...prev, [idx]: '' }));
    } else {
      alert(`Введите число от 0 до 255`);
    }
  };

  return (
    <div style={{ marginBottom: '12px' }}>
      <div
        style={{
          fontWeight: 'bold',
          cursor: 'pointer',
          padding: '8px',
          background: '#2d2d2d',
          borderRadius: '6px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
        }}
        onClick={() => setExpanded(!expanded)}
      >
        <span>🌐 Глобальные переменные</span>
        <span>{expanded ? '▼' : '▶'}</span>
      </div>

      {expanded && (
        <div style={{ marginLeft: '20px', marginTop: '8px', paddingLeft: '12px' }}>
          {variables && variables.length > 0 ? (
            variables.map((value, idx) => {
              const config = VAR_CONFIG[idx] || { name: `VAR[${idx}]`, emoji: '🔧' };
              const inputValue = inputValues[idx] || '';

              return (
                <div
                    key={idx}
                    style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    padding: '6px 0',
                    fontSize: '14px',
                    color: '#e0e0e0',
                    borderBottom: '1px solid #444',
                    }}
                >
                    {/* Левая часть: [индекс] 🎉 Название */}
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                    <strong style={{ color: '#9fcbff', minWidth: '40px', textAlign: 'center' }}>[{idx}]</strong>
                    <span style={{ fontSize: '16px' }}>{config.emoji}</span>
                    <span style={{ minWidth: '180px' }}>{config.name}</span>
                    </div>

                    {/* Правая часть: значение и кнопки */}
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                    <span style={{ width: '40px', textAlign: 'center', fontWeight: 'bold' }}>
                        {value}
                    </span>

                    {/* Кнопка INC */}
                    <button
                        className="btn-small"
                        title="Увеличить на 1"
                        style={{ padding: '2px 6px', fontSize: '12px' }}
                        onClick={(e) => {
                        e.stopPropagation();
                        sendWsCommand('inc_virtual_var', idx);
                        }}
                    >
                        ➕
                    </button>

                    {/* Кнопка DEC */}
                    <button
                        className="btn-small"
                        title="Уменьшить на 1"
                        style={{ padding: '2px 6px', fontSize: '12px' }}
                        onClick={(e) => {
                        e.stopPropagation();
                        sendWsCommand('dec_virtual_var', idx);
                        }}
                    >
                        ➖
                    </button>

                    {/* Кнопка TOGGLE */}
                    <button
                        className="btn-small"
                        title="Переключить (0 ↔ 1)"
                        style={{ padding: '2px 6px', fontSize: '12px' }}
                        onClick={(e) => {
                        e.stopPropagation();
                        sendWsCommand('toggle_virtual_var', idx);
                        }}
                    >
                        🔄
                    </button>

                    {/* Поле ввода SET */}
                    <input
                        type="number"
                        min="0"
                        max="255"
                        placeholder="set"
                        value={inputValue}
                        onChange={(e) => handleInputChange(idx, e.target.value)}
                        onKeyDown={(e) => {
                        if (e.key === 'Enter') {
                            handleSet(idx);
                        }
                        }}
                        style={{
                        width: '60px',
                        padding: '2px 4px',
                        fontSize: '12px',
                        background: '#333',
                        border: '1px solid #555',
                        color: '#fff',
                        }}
                    />

                    {/* Кнопка SET */}
                    <button
                        className="btn-primary"
                        title="Установить значение"
                        style={{ padding: '2px 6px', fontSize: '12px' }}
                        onClick={() => handleSet(idx)}
                    >
                        ✏️
                    </button>
                    </div>
                </div>
                );
            })
          ) : (
            <div style={{ color: '#666', fontSize: '13px', fontStyle: 'italic' }}>Нет данных</div>
          )}
        </div>
      )}
    </div>
  );
};