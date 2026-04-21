// src/components/RuleEditorActions.js
import { useEffect } from 'react';
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

export default function RuleEditorActions({ devices, variables, actions, onChange, onParamChange, onRemove, onAdd }) {
  // === Сброс ep при смене device ===
  useEffect(() => {
    actions.forEach(action => {
      if (action.type !== 'send_cmd_device') return;
      const deviceObj = devices.find(d => d.ieee_addr === action.device);
      if (deviceObj && action.ep && !deviceObj.endpoints.some(ep => ep.id === parseInt(action.ep))) {
        onChange(action.id, 'ep', '');
        onChange(action.id, 'cluster', '');
        onChange(action.id, 'cmd', '');
      }
    });
  }, [devices, actions, onChange]);

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
                  onChange={(e) => {
                    const newType = e.target.value;
                    if (newType === 'set_var') {
                      onChange(action.id, 'device', '');
                      onChange(action.id, 'ep', '');
                      onChange(action.id, 'cluster', '');
                      onChange(action.id, 'cmd', '');
                    }
                    onChange(action.id, 'type', newType);
                  }}
                  className="form-input"
                >
                  {actionTypes.map(act => (
                    <option key={act.value} value={act.value}>{act.label}</option>
                  ))}
                </select>
              </div>

              {/* Блок "Изменить переменную" */}
              {/* Блок "Изменить переменную" */}
              {action.type === 'set_var' && (
                <div className="space-y-3 ml-4 border-l-2 border-gray-700 pl-4">
                  <div className="form-row">
                    <label className="form-label">Переменная</label>
                    <select
                      value={action.target || ''}
                      onChange={(e) => {
                        const guid = e.target.value;
                        const variable = variables.find(v => v.guid === guid);
                        onChange(action.id, 'target', guid);
                        // Автоматически сбрасываем значение при смене переменной
                        onChange(action.id, 'value', '');
                      }}
                      className="form-input"
                    >
                      <option value="">Выберите</option>
                      {variables.map(v => (
                        <option key={v.guid} value={v.guid}>{v.name} ({v.guid})</option>
                      ))}
                    </select>
                  </div>

                  {action.target && (() => {
                  // Используем тип из действия (сохранённый), а не из текущей переменной
                  const dataType = action.dataType !== undefined 
                    ? action.dataType 
                    : (variables.find(v => v.guid === action.target)?.type ?? 0x20);

                  return (
                    <>
                      <div className="form-row">
                        <label className="form-label">Тип</label>
                        <div className="form-static-text font-mono text-green-400">
                          {formatDataType(dataType)}
                        </div>
                      </div>

                      <div className="form-row">
                        <label className="form-label">Значение</label>
                        {dataType === 0x42 || dataType === 0x43 ? (
                          <input
                            type="text"
                            value={action.value || ''}
                            onChange={(e) => onChange(action.id, 'value', e.target.value)}
                            placeholder="Введите строку"
                            className="form-input"
                          />
                        ) : (
                          <input
                            type="number"
                            value={action.value || ''}
                            onChange={(e) => onChange(action.id, 'value', e.target.value)}
                            placeholder="1, 255..."
                            className="form-input"
                            min="0"
                            max="65535"
                          />
                        )}
                      </div>
                    </>
                  );
                })()}
                </div>
              )}

              {/* Блок "Отправить команду устройству" */}
              {action.type === 'send_cmd_device' && (
                <div className="space-y-3 ml-4">
                  <div className="form-row">
                    <label className="form-label">Устройство</label>
                    <select
                      value={action.device}
                      onChange={(e) => {
                        onChange(action.id, 'device', e.target.value);
                        onChange(action.id, 'ep', '');
                        onChange(action.id, 'cluster', '');
                        onChange(action.id, 'cmd', '');
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

                  <div className="form-row">
                    <label className="form-label">Эндпоинт</label>
                    <select
                      value={action.ep || ''}
                      onChange={(e) => {
                        onChange(action.id, 'ep', e.target.value);
                        onChange(action.id, 'cluster', '');
                        onChange(action.id, 'cmd', '');
                      }}
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
                      value={action.cluster || ''}
                      onChange={(e) => {
                        onChange(action.id, 'cluster', e.target.value);
                        onChange(action.id, 'cmd', '');
                      }}
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
                      value={action.cmd || ''}
                      onChange={(e) => {
                        const newCmdGuid = e.target.value;
                        onChange(action.id, 'cmd', newCmdGuid);
                        const newCmd = getCommandById(device, action.ep, action.cluster, newCmdGuid);
                        if (newCmd?.params) {
                          const emptyParams = {};
                          newCmd.params.forEach(p => {
                            emptyParams[p.name] = '';
                          });
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

                  {/* Параметры команды */}
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