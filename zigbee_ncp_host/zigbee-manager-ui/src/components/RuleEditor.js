// src/components/RuleEditor.js
import React, { useState } from 'react';

// Полифил для генерации UUID v4
const generateUUID = () => {
  if (typeof crypto !== 'undefined' && crypto.randomUUID) {
    return crypto.randomUUID();
  }
  // Fallback: UUID v4 вручную
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
    setTriggers([...triggers, {
      type: 'device_state',
      short: '',
      endpoint_id: 1,
      cluster_type: 'on_off',
      condition: 'eq',
      value: 1
    }]);
  };

  const handleAddAction = () => {
    setActions([...actions, {
      type: 'device_command',
      short: '',
      endpoint: 1,
      cmd_id: 1
    }]);
  };

  const saveRule = () => {
    const newRule = {
      id: rule?.id || generateUUID(), // 
      name,
      module,
      priority,
      enabled,
      triggers: JSON.parse(JSON.stringify(triggers)),
      actions: JSON.parse(JSON.stringify(actions))
    };
    onSave(newRule);
  };

  return (
    <div className="rule-editor">
      <h3>{rule ? '✏️ Редактировать правило' : '➕ Новое правило'}</h3>

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

      <div className="form-row">
        <div className="form-group" style={{ flex: 1 }}>
          <label>Модуль</label>
          <select
            value={module}
            onChange={(e) => setModule(e.target.value)}
            className="form-select"
          >
            {modules.map(m => (
              <option key={m.id} value={m.id}>{m.name}</option>
            ))}
          </select>
        </div>
        <div className="form-group" style={{ flex: 1, marginLeft: '12px' }}>
          <label>Приоритет</label>
          <select
            value={priority}
            onChange={(e) => setPriority(Number(e.target.value))}
            className="form-select"
          >
            {[1,2,3,4,5].map(p => (
              <option key={p} value={p}>
                {p} {p === 1 ? '(высший)' : p === 5 ? '(низший)' : ''}
              </option>
            ))}
          </select>
        </div>
      </div>

      <div className="form-group">
        <label>
          <input
            type="checkbox"
            checked={enabled}
            onChange={(e) => setEnabled(e.target.checked)}
          /> Включено
        </label>
      </div>

      {/* Триггеры */}
      <div className="section">
        <h4>📌 Триггеры</h4>
        {triggers.map((t, idx) => (
          <div key={idx} className="trigger-item">
            <select
              value={t.type}
              onChange={(e) => {
                const newTriggers = [...triggers];
                newTriggers[idx].type = e.target.value;
                setTriggers(newTriggers);
              }}
            >
              <option value="device_state">Состояние устройства</option>
              <option value="time_range">Временной интервал</option>
            </select>

            {t.type === 'device_state' && (
              <>
                <select
                  value={t.short}
                  onChange={(e) => {
                    const newTriggers = [...triggers];
                    newTriggers[idx].short = Number(e.target.value);
                    setTriggers(newTriggers);
                  }}
                >
                  <option>Выберите устройство</option>
                  {devices.map(d => (
                    <option key={d.short} value={d.short}>
                      {d.name} (0x{d.short.toString(16)})
                    </option>
                  ))}
                </select>

                <select
                  value={t.cluster_type}
                  onChange={(e) => {
                    const newTriggers = [...triggers];
                    newTriggers[idx].cluster_type = e.target.value;
                    setTriggers(newTriggers);
                  }}
                >
                  <option value="on_off">On/Off</option>
                  <option value="illuminance">Освещённость</option>
                  <option value="temperature">Температура</option>
                </select>

                <select
                  value={t.condition}
                  onChange={(e) => {
                    const newTriggers = [...triggers];
                    newTriggers[idx].condition = e.target.value;
                    setTriggers(newTriggers);
                  }}
                >
                  <option value="eq">=</option>
                  <option value="ne">≠</option>
                  <option value="gt">&gt;</option>
                  <option value="lt">&lt;</option>
                  <option value="gte">≥</option>
                  <option value="lte">≤</option>
                </select>

                <input
                  type="number"
                  value={t.value}
                  onChange={(e) => {
                    const newTriggers = [...triggers];
                    newTriggers[idx].value = Number(e.target.value);
                    setTriggers(newTriggers);
                  }}
                  placeholder="Значение"
                />
              </>
            )}
          </div>
        ))}
        <button onClick={handleAddTrigger} className="btn-add">+ Добавить триггер</button>
      </div>

      {/* Действия */}
      <div className="section">
        <h4>⚡ Действия</h4>
        {actions.map((a, idx) => (
          <div key={idx} className="action-item">
            <select
              value={a.type}
              onChange={(e) => {
                const newActions = [...actions];
                newActions[idx].type = e.target.value;
                setActions(newActions);
              }}
            >
              <option value="device_command">Команда устройству</option>
            </select>

            {a.type === 'device_command' && (
              <>
                <select
                  value={a.short}
                  onChange={(e) => {
                    const newActions = [...actions];
                    newActions[idx].short = Number(e.target.value);
                    setActions(newActions);
                  }}
                >
                  <option>Устройство</option>
                  {devices.map(d => (
                    <option key={d.short} value={d.short}>
                      {d.name} (0x{d.short.toString(16)})
                    </option>
                  ))}
                </select>

                <input
                  type="number"
                  value={a.endpoint}
                  onChange={(e) => {
                    const newActions = [...actions];
                    newActions[idx].endpoint = Number(e.target.value);
                    setActions(newActions);
                  }}
                  placeholder="EP"
                  style={{ width: 60 }}
                />

                <select
                  value={a.cmd_id}
                  onChange={(e) => {
                    const newActions = [...actions];
                    newActions[idx].cmd_id = Number(e.target.value);
                    setActions(newActions);
                  }}
                >
                  <option value={1}>ON</option>
                  <option value={0}>OFF</option>
                  <option value={2}>TOGGLE</option>
                </select>
              </>
            )}
          </div>
        ))}
        <button onClick={handleAddAction} className="btn-add">+ Добавить действие</button>
      </div>

      <div className="modal-buttons">
        <button onClick={saveRule} className="btn-primary">💾 Сохранить</button>
        <button onClick={onCancel} className="btn-danger">❌ Отмена</button>
      </div>
    </div>
  );
};

export default RuleEditor;
