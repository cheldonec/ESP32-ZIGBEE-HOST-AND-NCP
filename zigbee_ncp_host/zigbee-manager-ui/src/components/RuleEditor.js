// src/components/RuleEditor.js
import React, { useState } from 'react';

// Полифил для генерации UUID v4
const generateUUID = () => {
  if (typeof crypto !== 'undefined' && crypto.randomUUID) {
    return crypto.randomUUID();
  }
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (c) => {
    const r = (Math.random() * 16) | 0;
    const v = c === 'x' ? r : (r & 0x3) | 0x8;
    return v.toString(16);
  });
};

const RuleEditor = ({ rule, devices, onSave, onCancel }) => {
  const [name, setName] = useState(rule?.name || '');
  const [module, setModule] = useState(rule?.module || 'light');
  const [priority, setPriority] = useState(rule?.priority || 3);
  const [enabled, setEnabled] = useState(rule?.enabled !== false);
  const [triggers, setTriggers] = useState((rule?.triggers || []).map(t => ({...t,id: t.id || generateUUID()})));
  const [actions, setActions] = useState((rule?.actions || []).map(a => ({...a,id: a.id || generateUUID()})));
  const [triggerLogic, setTriggerLogic] = useState(rule?.trigger_logic || 'any');
  const modules = [
    { id: 'light', name: 'Свет' },
    { id: 'relay', name: 'Реле' },
    { id: 'security', name: 'Безопасность' },
    { id: 'climate', name: 'Климат' },
  ];

  // --- Триггеры ---
  const handleAddTrigger = () => {
    setTriggers([
      ...triggers,
      {
        id: generateUUID(),
        type: 'device_state',
        short: '',
        endpoint_id: 1,
        cluster_type: 'on_off',
        condition: 'eq',
        value: 1,
      },
    ]);
  };

  const removeTrigger = (index) => {
    setTriggers(triggers.filter((_, i) => i !== index));
  };

  // --- Действия ---
  const handleAddAction = () => {
    setActions([
      ...actions,
      {
        id: generateUUID(), // ← добавляем ID
        type: 'device_command',
        short: '',
        endpoint: 1,
        cmd_id: 1,
      },
    ]);
  };

  const removeAction = (id) => {
    setActions(actions.filter(a => a.id !== id));
  };

  const saveRule = () => {
    console.log('🔍 Триггеры перед сохранением:', JSON.stringify(triggers, null, 2));
    const newRule = {
      id: rule?.id || generateUUID(),
      name,
      module,
      priority,
      enabled,
      trigger_logic: triggerLogic,
      triggers: JSON.parse(JSON.stringify(triggers)),
      actions: JSON.parse(JSON.stringify(actions)),
    };
    console.log('📤 Отправляем в onSave:', newRule); // 🔥 Добавь эту строку
    onSave(newRule);
  };

  return (
    <div className="rule-editor">
      <h3>{rule ? '✏️ Редактировать правило' : '➕ Новое правило'}</h3>

      {/* Прокручиваемая область */}
      <div style={{ maxHeight: 'calc(70vh - 120px)', overflowY: 'auto', paddingRight: '8px' }}>
        <style jsx>{`
          div[style*='overflowY']::-webkit-scrollbar {
            width: 6px;
          }
          div[style*='overflowY']::-webkit-scrollbar-track {
            background: var(--bg-secondary);
            border-radius: 3px;
          }
          div[style*='overflowY']::-webkit-scrollbar-thumb {
            background: var(--border-color);
            border-radius: 3px;
          }
          div[style*='overflowY']:hover::-webkit-scrollbar-thumb {
            background: #555;
          }
        `}</style>

        {/* Название */}
        <div className="form-group">
          <label>Название</label>
          <input
            type="text"
            value={name}
            onChange={(e) => setName(e.target.value)}
            placeholder="Например: Включить свет вечером"
            className="form-input"
          />
        </div>

        {/* Модуль и Приоритет */}
        <div className="form-row" style={{ display: 'flex', gap: '12px' }}>
          <div className="form-group" style={{ flex: 1 }}>
            <label>Модуль</label>
            <select
              value={module}
              onChange={(e) => setModule(e.target.value)}
              className="form-select"
            >
              {modules.map((m) => (
                <option key={m.id} value={m.id}>
                  {m.name}
                </option>
              ))}
            </select>
          </div>
          <div className="form-group" style={{ flex: 1 }}>
            <label>Приоритет</label>
            <select
              value={priority}
              onChange={(e) => setPriority(Number(e.target.value))}
              className="form-select"
            >
              {[1, 2, 3, 4, 5].map((p) => (
                <option key={p} value={p}>
                  {p} {p === 1 ? '(высший)' : p === 5 ? '(низший)' : ''}
                </option>
              ))}
            </select>
          </div>
        </div>

        {/* Включено */}
        <div className="form-group">
          <label>
            <input
              type="checkbox"
              checked={enabled}
              onChange={(e) => setEnabled(e.target.checked)}
            />{' '}
            Включено
          </label>
        </div>

        {/* Триггеры */}
        <div className="section">
          <h4>📌 Триггеры</h4>

          {/* Логика объединения триггеров — теперь всегда сверху */}
          <div className="form-group" style={{ marginTop: '8px', marginBottom: '16px' }}>
            <label>Логика триггеров</label>
            <select
              value={triggerLogic}
              onChange={(e) => setTriggerLogic(e.target.value)}
              className="form-select"
            >
              <option value="any">Любой (ИЛИ) — сработает при любом условии</option>
              <option value="all">Все (И) — сработает только при всех условиях</option>
            </select>
            <p style={{ fontSize: '12px', color: '#666', marginTop: '4px' }}>
              Например: «время 20:00 И освещённость &lt; 10»
            </p>
          </div>

          {triggers.length === 0 ? (
            <p style={{ color: 'var(--text-secondary)', fontSize: '14px' }}>Нет триггеров</p>
          ) : (
            triggers.map((t, idx) => (
              <div
                key={t.id}
                className="trigger-item"
                style={{
                  marginBottom: '12px',
                  padding: '12px',
                  background: 'var(--bg-primary)',
                  borderRadius: '8px',
                  position: 'relative',
                }}
              >
                {/* Тип триггера + кнопка удаления справа */}
                <div style={{ marginBottom: '8px', position: 'relative' }}>
                  <select
                    value={t.type}
                    onChange={(e) => {
                      const newTriggers = [...triggers];
                      newTriggers[idx].type = e.target.value;
                      setTriggers(newTriggers);
                    }}
                    className="form-select"
                    style={{ paddingRight: '40px' }}
                  >
                    <option value="device_state">Состояние устройства</option>
                    <option value="device_unavailable">Устройство недоступно</option>
                    <option value="time_range">Временной интервал</option>
                    <option value="virtual_var">Виртуальная переменная</option>
                  </select>
                  <button
                    onClick={() => removeTrigger(idx)}
                    className="delete-btn"
                    title="Удалить триггер"
                    style={{
                      position: 'absolute',
                      top: '6px',
                      right: '8px',
                      padding: '0 6px',
                      height: '20px',
                      minWidth: '24px',
                      fontSize: '14px',
                      fontWeight: 'normal',
                    }}
                  >
                    ×
                  </button>
                </div>

                {t.type === 'device_state' && (
                  <>
                    <div style={{ marginBottom: '8px' }}>
                      <label>Устройство</label>
                      <select
                        value={t.short || ''}
                        onChange={(e) => {
                          const newTriggers = [...triggers];
                          newTriggers[idx].short = Number(e.target.value);
                          setTriggers(newTriggers);
                        }}
                        className="form-select"
                      >
                        <option value="">Выберите устройство...</option>
                        {devices.map((d) => (
                          <option key={d.short} value={d.short}>
                            {d.name} (0x{d.short.toString(16).padStart(4, '0').toUpperCase()})
                          </option>
                        ))}
                      </select>
                    </div>

                    <div style={{ marginBottom: '8px', display: 'flex', gap: '8px' }}>
                      <div style={{ flex: 1 }}>
                        <label>Кластер</label>
                        <select
                          value={t.cluster_type}
                          onChange={(e) => {
                            const newTriggers = [...triggers];
                            newTriggers[idx].cluster_type = e.target.value;
                            setTriggers(newTriggers);
                          }}
                          className="form-select"
                        >
                          <option value="on_off">On/Off</option>
                          <option value="illuminance">Освещённость</option>
                          <option value="temperature">Температура</option>
                          <option value="humidity">Влажность</option>
                          <option value="battery">Батарея</option>
                        </select>
                      </div>
                      <div style={{ flex: 1 }}>
                        <label>Условие</label>
                        <select
                          value={t.condition}
                          onChange={(e) => {
                            const newTriggers = [...triggers];
                            newTriggers[idx].condition = e.target.value;
                            setTriggers(newTriggers);
                          }}
                          className="form-select"
                        >
                          <option value="eq">=</option>
                          <option value="ne">≠</option>
                          <option value="gt">&gt;</option>
                          <option value="lt">&lt;</option>
                          <option value="gte">≥</option>
                          <option value="lte">≤</option>
                        </select>
                      </div>
                    </div>

                    <div>
                      <label>Значение</label>
                      <input
                        type="number"
                        value={t.value}
                        onChange={(e) => {
                          const newTriggers = [...triggers];
                          newTriggers[idx].value = Number(e.target.value);
                          setTriggers(newTriggers);
                        }}
                        className="form-input"
                        placeholder="Например: 1"
                      />
                    </div>
                  </>
                )}
                {t.type === 'time_range' && (
                  <>
                    {/* Время: с ... до ... */}
                    <div style={{ display: 'flex', gap: '8px', marginBottom: '12px' }}>
                      <div style={{ flex: 1 }}>
                        <label>С</label>
                        <input
                          type="time"
                          value={t.from || '08:00'}
                          onChange={(e) => {
                            const newTriggers = [...triggers];
                            newTriggers[idx].from = e.target.value;
                            setTriggers(newTriggers);
                          }}
                          className="form-input"
                        />
                      </div>
                      <div style={{ flex: 1 }}>
                        <label>До</label>
                        <input
                          type="time"
                          value={t.to || '20:00'}
                          onChange={(e) => {
                            const newTriggers = [...triggers];
                            newTriggers[idx].to = e.target.value;
                            setTriggers(newTriggers);
                          }}
                          className="form-input"
                        />
                      </div>
                    </div>

                    {/* Дни недели */}
                    <div style={{ marginBottom: '12px' }}>
                      <label>Дни недели</label>
                      <div style={{
                        display: 'grid',
                        gridTemplateColumns: 'repeat(7, 1fr)',
                        gap: '4px',
                        marginTop: '6px',
                        fontSize: '14px'
                      }}>
                        {['Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб', 'Вс'].map((day, i) => (
                          <label key={i} style={{
                            textAlign: 'center',
                            padding: '4px',
                            background: (t.days_of_week & (1 << i)) ? 'var(--accent-color)' : 'var(--bg-secondary)',
                            color: (t.days_of_week & (1 << i)) ? 'white' : 'var(--text-primary)',
                            borderRadius: '4px',
                            cursor: 'pointer'
                          }}>
                            <input
                              type="checkbox"
                              checked={!!(t.days_of_week & (1 << i))}
                              onChange={(e) => {
                                const newTriggers = [...triggers];
                                if (e.target.checked) {
                                  newTriggers[idx].days_of_week |= (1 << i);
                                } else {
                                  newTriggers[idx].days_of_week &= ~(1 << i);
                                }
                                setTriggers(newTriggers);
                              }}
                              style={{ display: 'none' }}
                            />
                            {day}
                          </label>
                        ))}
                      </div>
                    </div>

                    {/* Задержка */}
                    <div>
                      <label>Задержка (сек)</label>
                      <input
                        type="number"
                        min="0"
                        max="3600"
                        step="15"
                        value={t.delay_sec || 0}
                        onChange={(e) => {
                          const newTriggers = [...triggers];
                          newTriggers[idx].delay_sec = Number(e.target.value);
                          setTriggers(newTriggers);
                        }}
                        placeholder="0"
                        className="form-input"
                        style={{ width: '100%' }}
                      />
                      <p style={{ fontSize: '12px', color: '#666', marginTop: '4px' }}>
                        Например: 300 = 5 минут задержки после начала интервала
                      </p>
                    </div>
                  </>
                )}
                {t.type === 'virtual_var' && (
                  <>
                    <div style={{ marginBottom: '8px' }}>
                      <label>Переменная</label>
                      <select
                        value={t.var_index || 0}
                        onChange={(e) => {
                          const newTriggers = [...triggers];
                          newTriggers[idx].var_index = Number(e.target.value);
                          setTriggers(newTriggers);
                        }}
                        className="form-select"
                      >
                        <option value={0}>Переменная 1</option>
                        <option value={1}>Переменная 2</option>
                        <option value={2}>Переменная 3</option>
                      </select>
                    </div>

                    <div style={{ marginBottom: '8px', display: 'flex', gap: '8px' }}>
                      <div style={{ flex: 1 }}>
                        <label>Условие</label>
                        <select
                          value={t.condition}
                          onChange={(e) => {
                            const newTriggers = [...triggers];
                            newTriggers[idx].condition = e.target.value;
                            setTriggers(newTriggers);
                          }}
                          className="form-select"
                        >
                          <option value="eq">=</option>
                          <option value="ne">≠</option>
                          <option value="gt">&gt;</option>
                          <option value="lt">&lt;</option>
                          <option value="gte">≥</option>
                          <option value="lte">≤</option>
                        </select>
                      </div>
                      <div style={{ flex: 1 }}>
                        <label>Значение</label>
                        <input
                          type="number"
                          min="0"
                          max="255"
                          value={t.value}
                          onChange={(e) => {
                            const newTriggers = [...triggers];
                            newTriggers[idx].value = Number(e.target.value);
                            setTriggers(newTriggers);
                          }}
                          className="form-input"
                        />
                      </div>
                    </div>
                  </>
                )}
                {t.type === 'device_unavailable' && (
                  <>
                    <div style={{ marginBottom: '8px' }}>
                      <label>Устройство</label>
                      <select
                        value={t.short || ''}
                        onChange={(e) => {
                          const newTriggers = [...triggers];
                          newTriggers[idx].short = Number(e.target.value);
                          setTriggers(newTriggers);
                        }}
                        className="form-select"
                      >
                        <option value="">Выберите устройство...</option>
                        {devices.map((d) => (
                          <option key={d.short} value={d.short}>
                            {d.name} (0x{d.short.toString(16).padStart(4, '0').toUpperCase()})
                          </option>
                        ))}
                      </select>
                    </div>

                    <div style={{ marginBottom: '8px' }}>
                      <label>Кластер</label>
                      <select
                        value={t.cluster_type || 'temperature'}
                        onChange={(e) => {
                          const newTriggers = [...triggers];
                          newTriggers[idx].cluster_type = e.target.value;
                          setTriggers(newTriggers);
                        }}
                        className="form-select"
                      >
                        <option value="temperature">Температура</option>
                        <option value="humidity">Влажность</option>
                        <option value="on_off">Свет</option>
                        <option value="illuminance">Освещённость</option>
                        <option value="battery">Батарея</option>
                      </select>
                    </div>
                  </>
                )}
              </div>
            ))
          )}

          <button
            onClick={handleAddTrigger}
            className="btn-primary"
            style={{ padding: '8px 12px', fontSize: '14px' }}
          >
            ➕ Добавить триггер
          </button>
        </div>

        {/* Действия */}
        <div className="section" style={{ marginTop: '20px' }}>
          <h4>⚡ Действия</h4>

          {actions.length === 0 ? (
            <p style={{ color: 'var(--text-secondary)', fontSize: '14px' }}>Нет действий</p>
          ) : (
            actions.map((a, idx) => (
              <div
                key={a.id}
                className="action-item"
                style={{
                  marginBottom: '12px',
                  padding: '12px',
                  background: 'var(--bg-primary)',
                  borderRadius: '8px',
                  position: 'relative',
                }}
              >
                {/* Тип действия + кнопка удаления справа */}
                <div style={{ marginBottom: '8px', position: 'relative' }}>
                  <select
                    value={a.type}
                    onChange={(e) => {
                      const newActions = [...actions];
                      newActions[idx].type = e.target.value;
                      // Сбрасываем лишние поля при смене типа
                      if (e.target.value !== 'set_virtual_var') {
                        delete newActions[idx].value;
                      }
                      setActions(newActions);
                    }}
                    className="form-select"
                    style={{ paddingRight: '40px' }}
                  >
                    <option value="device_command">Команда устройству</option>
                    <option value="set_virtual_var">Установить переменную</option>
                    <option value="var_inc">Увеличить счётчик (+1)</option>
                    <option value="var_dec">Уменьшить счётчик (-1)</option>
                    <option value="var_toggle">Переключить флаг (0↔1)</option>
                    {/* <option value="http_request">Отправить уведомление (Telegram)</option> */}
                  </select>
                  <button
                    onClick={() => removeAction(a.id)}
                    className="delete-btn"
                    title="Удалить действие"
                    style={{
                      position: 'absolute',
                      top: '6px',
                      right: '8px',
                      padding: '0 6px',
                      height: '20px',
                      minWidth: '24px',
                      fontSize: '14px',
                      fontWeight: 'normal',
                    }}
                  >
                    ×
                  </button>
                </div>

                {a.type === 'device_command' && (
                  <>
                    <div style={{ marginBottom: '8px', display: 'flex', gap: '8px' }}>
                      <div style={{ flex: 1 }}>
                        <label>Устройство</label>
                        <select
                          value={a.short || ''}
                          onChange={(e) => {
                            const newActions = [...actions];
                            newActions[idx].short = Number(e.target.value);
                            setActions(newActions);
                          }}
                          className="form-select"
                        >
                          <option value="">Выберите устройство...</option>
                          {devices.map((d) => (
                            <option key={d.short} value={d.short}>
                              {d.name} (0x{d.short.toString(16).padStart(4, '0').toUpperCase()})
                            </option>
                          ))}
                        </select>
                      </div>
                      <div style={{ width: '80px' }}>
                        <label>EP</label>
                        <input
                          type="number"
                          value={a.endpoint}
                          onChange={(e) => {
                            const newActions = [...actions];
                            newActions[idx].endpoint = Number(e.target.value);
                            setActions(newActions);
                          }}
                          className="form-input"
                          placeholder="1"
                        />
                      </div>
                    </div>

                    <div>
                      <label>Команда</label>
                      <select
                        value={a.cmd_id}
                        onChange={(e) => {
                          const newActions = [...actions];
                          newActions[idx].cmd_id = Number(e.target.value);
                          setActions(newActions);
                        }}
                        className="form-select"
                      >
                        <option value={1}>ON</option>
                        <option value={0}>OFF</option>
                        <option value={2}>TOGGLE</option>
                      </select>
                    </div>
                  </>
                )}
                {a.type === 'set_virtual_var' && (
                  <>
                    <div style={{ marginBottom: '8px' }}>
                      <label>Переменная</label>
                      <select
                        value={a.var_index || 0}
                        onChange={(e) => {
                          const newActions = [...actions];
                          newActions[idx].var_index = Number(e.target.value);
                          setActions(newActions);
                        }}
                        className="form-select"
                      >
                        {/* === Системные переменные (0–15) === */}
                        <option value={0}>🌅 Утро началось</option>
                        <option value={1}>🏠 Кто-то дома</option>
                        <option value={2}>🚫 Дома никого</option>
                        <option value={3}>🎉 Праздник / Выходной</option>
                        <option value={4}>🌙 Ночь активна</option>
                        <option value={5}>🚗🚪 Гараж открыт</option>
                        <option value={6}>🔔 Сигнализация включена</option>
                        <option value={7}>🔢 Счётчик кликов кнопки</option>
                        <option value={8}>✋ Ручной режим света</option>
                        <option value={9}>✅ Система готова (всё онлайн)</option>
                        <option value={10}>🪟 Окно открыто</option>
                        <option value={11}>🔥 Котёл работает</option>
                        <option value={12}>🌇 Закат прошёл</option>
                        <option value={13}>📺 Режим просмотра ТВ</option>
                        <option value={14}>🚪 Дверь открывалась сегодня</option>
                        <option value={15}>🔄 Последнее сработавшее правило</option>

                        {/* === Пользовательские переменные (16–31) === */}
                        <optgroup label="🔧 Пользовательские переменные">
                          <option value={16}>📦 Переменная USER_VAR[1] (пользовательская)</option>
                          <option value={17}>📦 Переменная USER_VAR[2]  (пользовательская)</option>
                          <option value={18}>📦 Переменная USER_VAR[3]  (пользовательская)</option>
                          <option value={19}>📦 Переменная USER_VAR[4]  (пользовательская)</option>
                          <option value={20}>📦 Переменная USER_VAR[5]  (пользовательская)</option>
                          <option value={21}>📦 Переменная USER_VAR[6]  (пользовательская)</option>
                          <option value={22}>📦 Переменная USER_VAR[7]  (пользовательская)</option>
                          <option value={23}>📦 Переменная USER_VAR[8]  (пользовательская)</option>
                          <option value={24}>📦 Переменная USER_VAR[9]  (пользовательская)</option>
                          <option value={25}>📦 Переменная USER_VAR[10]  (пользовательская)</option>
                          <option value={26}>📦 Переменная USER_VAR[11]  (пользовательская)</option>
                          <option value={27}>📦 Переменная USER_VAR[12]  (пользовательская)</option>
                          <option value={28}>📦 Переменная USER_VAR[13]  (пользовательская)</option>
                          <option value={29}>📦 Переменная USER_VAR[14]  (пользовательская)</option>
                          <option value={30}>📦 Переменная USER_VAR[15]  (пользовательская)</option>
                          <option value={31}>📦 Переменная USER_VAR[16]  (пользовательская)</option>
                        </optgroup>
                      </select>
                    </div>
                    <div>
                      <label>Значение</label>
                      <input
                        type="number"
                        min="0"
                        max="255"
                        value={a.value}
                        onChange={(e) => {
                          const newActions = [...actions];
                          newActions[idx].value = Number(e.target.value);
                          setActions(newActions);
                        }}
                        className="form-input"
                      />
                    </div>
                  </>
                )}

                {(a.type === 'var_inc' || a.type === 'var_dec' || a.type === 'var_toggle') && (
                  <>
                    <div style={{ marginBottom: '8px' }}>
                      <label>Переменная</label>
                      <select
                        value={a.var_index || 0}
                        onChange={(e) => {
                          const newActions = [...actions];
                          newActions[idx].var_index = Number(e.target.value);
                          setActions(newActions);
                        }}
                        className="form-select"
                      >
                        <option value={0}>🌅 Утро началось</option>
                        <option value={1}>🏠 Кто-то дома</option>
                        <option value={2}>🚫 Дома никого</option>
                        <option value={3}>🎉 Праздник</option>
                        <option value={4}>🌙 Ночь</option>
                        <option value={5}>🚪 Гараж открыт</option>
                        <option value={6}>🔔 Сигнализация</option>
                        <option value={7}>📊 Счётчик кликов</option>
                        <option value={8}>💡 Ручной режим света</option>
                        <option value={9}>✅ Система готова</option>
                      </select>
                    </div>
                    {a.type === 'var_inc' && (
                      <p style={{ fontSize: '13px', color: '#2e8b57', margin: '4px 0' }}>
                        ➕ Переменная будет увеличена на 1
                      </p>
                    )}
                    {a.type === 'var_dec' && (
                      <p style={{ fontSize: '13px', color: '#b22222', margin: '4px 0' }}>
                        ➖ Переменная будет уменьшена на 1 (не ниже 0)
                      </p>
                    )}
                    {a.type === 'var_toggle' && (
                      <p style={{ fontSize: '13px', color: '#4682b4', margin: '4px 0' }}>
                        🔁 Переменная будет переключена: 0 → 1, 1 → 0
                      </p>
                    )}
                  </>
                )}
                {a.type === 'http_request' && (
                  <div>
                    <p style={{ color: '#888', fontSize: '13px', margin: '8px 0' }}>
                      Отправит сообщение в Telegram. API ключ настраивается в разделе «Настройки».
                    </p>
                    <div>
                      <label>Сообщение</label>
                      <textarea
                        value={a.body || 'Сработало правило: {{name}}'}
                        onChange={(e) => {
                          const newActions = [...actions];
                          newActions[idx].body = e.target.value;
                          setActions(newActions);
                        }}
                        className="form-input"
                        rows="2"
                        placeholder="Текст уведомления"
                      />
                    </div>
                    <div style={{ marginTop: '8px', fontSize: '12px', color: '#666' }}>
                      Поддерживается подстановка: <code>{{name}}</code>, <code>{'{{time}}'}</code>
                    </div>
                  </div>
                )}
              </div>
            ))
          )}

          <button
            onClick={handleAddAction}
            className="btn-primary"
            style={{ padding: '8px 12px', fontSize: '14px' }}
          >
            ➕ Добавить действие
          </button>
        </div>
      </div>

      {/* Кнопки сохранения */}
      <div className="modal-buttons" style={{ marginTop: '16px', paddingTop: '8px', borderTop: '1px solid var(--border-color)' }}>
        <button onClick={saveRule} className="btn-primary">💾 Сохранить</button>
        <button onClick={onCancel} className="btn-danger">❌ Отмена</button>
      </div>
    </div>
  );
};

export default RuleEditor;