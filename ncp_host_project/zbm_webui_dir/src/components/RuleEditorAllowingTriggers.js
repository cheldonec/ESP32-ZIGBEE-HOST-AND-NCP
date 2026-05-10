// src/components/RuleEditorAllowingTriggers.js
import { useState } from 'react';
import { formatDataType } from '../utils/variables';

const conditionTypes = [
  { value: 'eq', label: 'равно' },
  { value: 'ne', label: 'не равно' },
  { value: 'gt', label: 'больше' },
  { value: 'lt', label: 'меньше' },
  { value: 'gte', label: 'больше или равно' },
  { value: 'lte', label: 'меньше или равно' },
];

const sourceTypes = [
  { value: 'attr_rep', label: 'Значение атрибута/репорта' },
  { value: 'variable', label: 'Значение переменной' },
];

function getClusterOptions(device, epId) {
  if (!device || !epId) return [];
  const ep = device.endpoints?.find(e => e.id === parseInt(epId));
  if (!ep) return [];
  return [...(ep.standard_clusters?.filter(c => c.role === 'server') || []), ...(ep.custom_clusters || [])];
}

function getAttrRepOptions(cluster) {
  if (!cluster) return [];

  const attrs = cluster.attributes?.map(a => ({
    value: a.guid,
    label: `${a.name} (0x${a.id.toString(16).padStart(4, '0')})`,
    type: a.type,
  })) || [];

  const reps = cluster.custom_reports?.map(r => ({
    value: r.guid,
    label: `${r.name} (0x${r.id.toString(16).padStart(2, '0')})`,
    type: r.type,
  })) || [];

  return [...attrs, ...reps];
}

export default function RuleEditorAllowingTriggers({ devices, variables, triggers, onAdd, onChange, onRemove }) {
  const updateTrigger = (id, updates) => {
    const trigger = triggers.find(t => t.id === id);
    onChange(id, { ...trigger, ...updates });
  };

  return (
    <div className="panel mb-6">
      <div className="panel-header">🎯 Разрешающие условия</div>
      <div className="panel-body space-y-4">
        {triggers.length === 0 ? (
          <p className="text-gray-500 text-sm">Нет разрешающих условий</p>
        ) : (
          triggers.map((trigger) => {
            const sourceType = trigger.sourceType || 'attr_rep';
            const isVarMode = sourceType === 'variable';

            // Переменная
            const varData = isVarMode ? variables.find(v => v.guid === trigger.var) : null;

            // Атрибут/репорт
            const device = devices.find(d => d.ieee_addr === trigger.device);
            //const ep = device?.endpoints.find(e => e.id === parseInt(trigger.ep));
            const cluster = getClusterOptions(device, trigger.ep).find(c => c.id === parseInt(trigger.cluster));
            const attrOrRep = !isVarMode && trigger.attrOrRep
              ? getAttrRepOptions(cluster)?.find(a => a.value === trigger.attrOrRep)
              : null;

            return (
              <div
                key={trigger.id}
                className="bg-gray-800/40 p-4 rounded border border-gray-700 relative"
              >
                <label className="absolute -top-2 left-3 px-1 text-xs font-medium bg-gray-900 text-gray-300">
                  Условие #{triggers.findIndex(t => t.id === trigger.id) + 1}
                </label>

                {/* Тип источника */}
                <div className="form-row mb-3">
                  <label className="form-label">Источник</label>
                  <select
                    value={sourceType}
                    onChange={(e) => {
                      const newType = e.target.value;
                      if (newType === 'variable') {
                        updateTrigger(trigger.id, {
                          sourceType: newType,
                          device: '',
                          ep: '',
                          cluster: '',
                          attrOrRep: '',
                          cond: 'eq',
                        });
                      } else {
                        updateTrigger(trigger.id, {
                          sourceType: newType,
                          var: '',
                          cond: 'eq',
                        });
                      }
                    }}
                    className="form-input"
                  >
                    {sourceTypes.map(t => (
                      <option key={t.value} value={t.value}>{t.label}</option>
                    ))}
                  </select>
                </div>

                {/* Блок: Переменная */}
                {isVarMode && (
                  <div className="space-y-3 ml-4 border-l-2 border-gray-700 pl-4 mb-4">
                    <div className="form-row">
                      <label className="form-label">Переменная</label>
                      <select
                        value={trigger.var || ''}
                        onChange={(e) => {
                          const guid = e.target.value;
                          const variable = variables.find(v => v.guid === guid);
                          updateTrigger(trigger.id, {
                            var: guid,
                            guid,
                            dataType: variable?.type || 0x20,
                            cond: 'eq',
                            value: trigger.value || '0'
                          });
                        }}
                        className="form-input"
                      >
                        <option value="">Выберите</option>
                        {variables.map(v => (
                          <option key={v.guid} value={v.guid}>{v.name} ({v.guid})</option>
                        ))}
                      </select>
                    </div>

                    {varData && (
                      <div className="form-row">
                        <label className="form-label">Тип</label>
                        <div className="form-static-text font-mono text-green-400">
                          {formatDataType(varData.type)}
                        </div>
                      </div>
                    )}
                  </div>
                )}

                {/* Блок: Атрибут/Репорт */}
                {!isVarMode && (
                  <div className="space-y-3 ml-4 border-l-2 border-gray-700 pl-4 mb-4">
                    <div className="grid grid-cols-1 md:grid-cols-4 gap-4 text-sm">

                      {/* Устройство */}
                      <div className="form-row">
                        <label className="form-label">Устройство</label>
                        <select
                          value={trigger.device || ''}
                          onChange={(e) => {
                            updateTrigger(trigger.id, {
                              device: e.target.value,
                              ep: '',
                              cluster: '',
                              attrOrRep: ''
                            });
                          }}
                          className="form-input"
                        >
                          <option value="">Выберите</option>
                          {devices.map(d => (
                            <option key={d.ieee_addr} value={d.ieee_addr}>
                              {d.name || d.friendly_name || d.ieee_addr}
                            </option>
                          ))}
                        </select>
                      </div>

                      {/* Эндпоинт */}
                      <div className="form-row">
                        <label className="form-label">Эндпоинт</label>
                        <select
                          value={trigger.ep || ''}
                          onChange={(e) => {
                            updateTrigger(trigger.id, {
                              ep: e.target.value,
                              cluster: '',
                              attrOrRep: ''
                            });
                          }}
                          disabled={!trigger.device}
                          className="form-input"
                        >
                          <option value="">EP</option>
                          {device?.endpoints.map(ep => (
                            <option key={ep.id} value={ep.id}>EP {ep.id}</option>
                          ))}
                        </select>
                      </div>

                      {/* Кластер */}
                      <div className="form-row">
                        <label className="form-label">Кластер</label>
                        <select
                          value={trigger.cluster || ''}
                          onChange={(e) => {
                            updateTrigger(trigger.id, {
                              cluster: e.target.value,
                              attrOrRep: ''
                            });
                          }}
                          disabled={!trigger.ep}
                          className="form-input"
                        >
                          <option value="">Кластер</option>
                          {getClusterOptions(device, trigger.ep).map(c => (
                            <option key={c.id} value={c.id}>
                              {c.name} (0x{c.id.toString(16).padStart(4, '0')})
                            </option>
                          ))}
                        </select>
                      </div>

                      {/* Атрибут/Репорт */}
                      <div className="form-row">
                        <label className="form-label">Атрибут/Репорт</label>
                        <select
                          value={trigger.attrOrRep || ''}
                          onChange={(e) => {
                            const guid = e.target.value;
                            const option = getAttrRepOptions(cluster)?.find(a => a.value === guid);
                            updateTrigger(trigger.id, {
                              attrOrRep: guid,
                              guid, // реальный guid атрибута/репорта
                              dataType: option?.type || 0x20,
                            });
                          }}
                          disabled={!trigger.cluster}
                          className="form-input"
                        >
                          <option value="">Выберите</option>
                          {getAttrRepOptions(cluster).map(opt => (
                            <option key={opt.value} value={opt.value}>{opt.label}</option>
                          ))}
                        </select>
                      </div>
                    </div>

                    {/* Тип атрибута */}
                    {attrOrRep && (
                      <div className="form-row">
                        <label className="form-label">Тип</label>
                        <div className="form-static-text font-mono text-green-400">
                          {formatDataType(attrOrRep.type)}
                        </div>
                      </div>
                    )}
                  </div>
                )}

                {/* Условие и значение */}
                <div className="flex flex-wrap gap-4 ml-4">
                  <div className="form-row flex-1 min-w-[150px]">
                    <label className="form-label">Условие</label>
                    <select
                      value={trigger.cond}
                      onChange={(e) => updateTrigger(trigger.id, { cond: e.target.value })}
                      className="form-input"
                    >
                      {conditionTypes.map(c => (
                        <option key={c.value} value={c.value}>{c.label}</option>
                      ))}
                    </select>
                  </div>

                  <div className="form-row flex-1 min-w-[150px]">
                    <label className="form-label">Значение</label>
                    <input
                      type="text"
                      value={trigger.value || ''}
                      onChange={(e) => updateTrigger(trigger.id, { value: e.target.value })}
                      placeholder="1, true..."
                      className="form-input"
                    />
                  </div>
                </div>

                {/* Кнопка удаления */}
                
                <button
                  type="button"
                  onClick={() => onRemove(trigger.id)}
                  aria-label="Удалить элемент"
                  className="delete-button"
                >
                  ×
                </button>
              </div>
            );
          })
        )}

        <button onClick={onAdd} className="btn-primary text-xs px-3 py-1">
          + Добавить условие
        </button>
      </div>
    </div>
  );
}