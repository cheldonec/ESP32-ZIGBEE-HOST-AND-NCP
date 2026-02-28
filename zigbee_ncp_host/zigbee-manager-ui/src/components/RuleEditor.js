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
  const [triggers, setTriggers] = useState(rule?.triggers || []);
  const [actions, setActions] = useState(rule?.actions || []);

  const modules = [
    { id: 'light', name: 'Свет' },
    { id: 'relay', name: 'Реле' },
    { id: 'security', name: 'Безопасность' },
  ];

  const handleAddTrigger = () => {
    setTriggers([
      ...triggers,
      {
        type: 'device_state',
        short: '',
        endpoint_id: 1,
        cluster_type: 'on_off',
        condition: 'eq',
        value: 1,
      },
    ]);
  };

  const handleAddAction = () => {
    setActions([
      ...actions,
      {
        type: 'device_command',
        short: '',
        endpoint: 1,
        cmd_id: 1,
      },
    ]);
  };

  const saveRule = () => {
    const newRule = {
      id: rule?.id || generateUUID(),
      name,
      module,
      priority,
      enabled,
      triggers: JSON.parse(JSON.stringify(triggers)),
      actions: JSON.parse(JSON.stringify(actions)),
    };
    onSave(newRule);
  };

  return (
    <div className="rule-editor">
      <h3>{rule ? '✏️ Редактировать правило' : '➕ Новое правило'}</h3>

      {/* Прокручиваемая область */}
      <div style={{ maxHeight: 'calc(70vh - 120px)', overflowY: 'auto', paddingRight: '8px' }}>
        {/* Стиль скролла (опционально) */}
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
          {triggers.length === 0 ? (
            <p style={{ color: 'var(--text-secondary)', fontSize: '14px' }}>Нет триггеров</p>
          ) : (
            triggers.map((t, idx) => (
              <div
                key={idx}
                className="trigger-item"
                style={{
                  marginBottom: '12px',
                  padding: '12px',
                  background: 'var(--bg-primary)',
                  borderRadius: '8px',
                }}
              >
                <div style={{ marginBottom: '8px' }}>
                  <select
                    value={t.type}
                    onChange={(e) => {
                      const newTriggers = [...triggers];
                      newTriggers[idx].type = e.target.value;
                      setTriggers(newTriggers);
                    }}
                    className="form-select"
                  >
                    <option value="device_state">Состояние устройства</option>
                    <option value="time_range">Временной интервал</option>
                  </select>
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
                key={idx}
                className="action-item"
                style={{
                  marginBottom: '12px',
                  padding: '12px',
                  background: 'var(--bg-primary)',
                  borderRadius: '8px',
                }}
              >
                <div style={{ marginBottom: '8px' }}>
                  <select
                    value={a.type}
                    onChange={(e) => {
                      const newActions = [...actions];
                      newActions[idx].type = e.target.value;
                      setActions(newActions);
                    }}
                    className="form-select"
                  >
                    <option value="device_command">Команда устройству</option>
                  </select>
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

      {/* Кнопки сохранения — фиксированы внизу */}
      <div className="modal-buttons" style={{ marginTop: '16px', paddingTop: '8px', borderTop: '1px solid var(--border-color)' }}>
        <button onClick={saveRule} className="btn-primary">💾 Сохранить</button>
        <button onClick={onCancel} className="btn-danger">❌ Отмена</button>
      </div>
    </div>
  );
};

export default RuleEditor;