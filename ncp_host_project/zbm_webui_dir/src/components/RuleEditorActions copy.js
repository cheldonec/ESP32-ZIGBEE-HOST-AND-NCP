// src/components/RuleEditorActions.js
import { useState } from 'react';
import { virtualVariables, getVariable, formatDataType } from './variables';

const actionTypes = [
  { value: 'send_cmd_device', label: 'Отправить команду устройству' },
  { value: 'set_var', label: 'Изменить переменную' },
];

function getClusterOptions(device, epId) {
  if (!device || !epId) return [];
  const ep = device.endpoints?.find(e => e.id === parseInt(epId));
  if (!ep) return [];
  return [...(ep.standard_clusters?.filter(c => c.role === 'server') || []), ...(ep.custom_clusters || [])];
}

function getCommandOptions(cluster) {
  return cluster?.commands || [];
}

function getCommandById(device, epId, clusterId, cmdGuid) {
  const ep = device?.endpoints.find(e => e.id === parseInt(epId));
  const cluster = getClusterOptions(device, epId).find(c => c.id === parseInt(clusterId));
  return cluster?.commands?.find(c => c.guid === cmdGuid) || null;
}

/*function formatDataType(type) {
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
}*/

export default function RuleEditorActions({ devices, actions, onChange, onParamChange, onRemove, onAdd }) {
  return (
    <div className="panel mb-6">
      <div className="panel-header">✅ Действия</div>
      <div className="panel-body space-y-4">
        {actions.map((action) => {
          const device = devices.find(d => d.ieee_addr === action.device);
          const ep = device?.endpoints.find(e => e.id === parseInt(action.ep));
          const cluster = getClusterOptions(device, action.ep).find(c => c.id === parseInt(action.cluster));
          const cmd = getCommandById(device, action.ep, cluster?.id, action.cmd);

          return (
            <div key={action.id} className="bg-gray-800/40 p-4 rounded border border-gray-700 relative">
              {/* Тип действия */}
              <div className="form-row mb-3">
                <label className="form-label">Тип</label>
                <select
                  value={action.type}
                  onChange={(e) => onChange(action.id, 'type', e.target.value)}
                  className="form-input"
                >
                  {actionTypes.map(act => (
                    <option key={act.value} value={act.value}>{act.label}</option>
                  ))}
                </select>
              </div>

              {/* Блок "Изменить переменную" */}
              {action.type === 'set_var' && (
                <div className="space-y-3 ml-4">
                    <div className="form-row">
                    <label className="form-label">Переменная</label>
                    <select
                        value={action.target || ''}
                        onChange={(e) => onChange(action.id, 'target', e.target.value)}
                        className="form-input"
                    >
                        <option value="">Выберите</option>
                        {virtualVariables.map(v => (
                        <option key={v.guid} value={v.guid}>{v.name} ({v.guid})</option>
                        ))}
                    </select>
                    </div>
                    <div className="form-row">
                    <label className="form-label">Тип</label>
                    <div className="form-static-text font-mono text-green-400">
                        {action.target ? formatDataType(getVariable(action.target)?.type) : '—'}
                    </div>
                    </div>
                    <div className="form-row">
                    <label className="form-label">Значение</label>
                    <input
                        type="text"
                        value={action.value || ''}
                        onChange={(e) => onChange(action.id, 'value', e.target.value)}
                        placeholder="1, true..."
                        className="form-input"
                    />
                    </div>
                </div>
                )}

              {/* Блок "Отправить команду устройству" */}
              {action.type === 'send_cmd_device' && (
                <div className="space-y-3 ml-4">
                  <div className="form-row">
                    <label className="form-label">Устройство</label>
                    <select
                      value={action.device}
                      onChange={(e) => onChange(action.id, 'device', e.target.value)}
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

                  <div className="form-row">
                    <label className="form-label">Эндпоинт</label>
                    <select
                      value={action.ep}
                      onChange={(e) => onChange(action.id, 'ep', e.target.value)}
                      disabled={!action.device}
                      className="form-input"
                    >
                      <option value="">EP</option>
                      {device?.endpoints.map(ep => (
                        <option key={ep.id} value={ep.id}>EP {ep.id}</option>
                      ))}
                    </select>
                  </div>

                  <div className="form-row">
                    <label className="form-label">Кластер</label>
                    <select
                      value={action.cluster}
                      onChange={(e) => onChange(action.id, 'cluster', e.target.value)}
                      disabled={!action.ep}
                      className="form-input"
                    >
                      <option value="">Кластер</option>
                      {getClusterOptions(device, action.ep).map(c => (
                        <option key={c.id} value={c.id}>
                          {c.name} (0x{c.id.toString(16).padStart(4, '0')})
                        </option>
                      ))}
                    </select>
                  </div>

                  <div className="form-row">
                    <label className="form-label">Команда</label>
                    <select
                      value={action.cmd}
                      onChange={(e) => {
                        const newCmdGuid = e.target.value;
                        onChange(action.id, 'cmd', newCmdGuid);
                        const newCmd = getCommandById(device, action.ep, action.cluster, newCmdGuid);
                        if (newCmd?.params) {
                          const emptyParams = {};
                          newCmd.params.forEach(p => { emptyParams[p.name] = ''; });
                          onChange(action.id, 'params', emptyParams);
                        }
                      }}
                      disabled={!action.cluster}
                      className="form-input"
                    >
                      <option value="">Выберите команду</option>
                      {cluster && getCommandOptions(cluster).map(cmd => (
                        <option key={cmd.guid} value={cmd.guid}>{cmd.name}</option>
                      ))}
                    </select>
                  </div>

                  {/* Параметры команды — НЕ ТРОГАЕМ, оставляем идеальной таблицей */}
                  {action.cmd && cmd?.params?.length > 0 && (
                    <div className="mt-2 ml-2">
                      <table className="w-full text-xs border-collapse">
                        <thead>
                          <tr className="text-left text-gray-500">
                            <th>Параметр</th>
                            <th>Тип</th>
                            <th>Ввод</th>
                          </tr>
                        </thead>
                        <tbody>
                          {cmd.params.map((param, i) => (
                            <tr key={i}>
                              <td className="py-1 text-gray-300">{param.name}</td>
                              <td className="py-1 text-gray-500">{formatDataType(param.type)}</td>
                              <td className="py-1">
                                <input
                                  type="text"
                                  value={action.params[param.name] || ''}
                                  onChange={(e) => onParamChange(action.id, param.name, e.target.value)}
                                  className="form-input text-xs px-2 py-1 h-6"
                                  style={{ fontSize: '11px' }}
                                />
                              </td>
                            </tr>
                          ))}
                        </tbody>
                      </table>
                    </div>
                  )}
                </div>
              )}

              <button
                onClick={() => onRemove(action.id)}
                className="absolute top-2 right-2 text-red-400 hover:text-red-300 text-lg"
              >
                ✕
              </button>
            </div>
          );
        })}

        <button onClick={onAdd} className="btn-primary text-xs px-3 py-1">
          + Добавить действие
        </button>
      </div>
    </div>
  );
}