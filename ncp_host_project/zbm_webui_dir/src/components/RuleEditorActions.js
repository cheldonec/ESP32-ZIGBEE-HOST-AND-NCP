// src/components/RuleEditorActions.js
import { useEffect } from 'react';
import { formatDataType } from '../utils/variables';

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
  //const ep = device?.endpoints.find(e => e.id === parseInt(epId));
  const cluster = getClusterOptions(device, epId).find(c => c.id === parseInt(clusterId));
  return cluster?.commands?.find(c => c.guid === cmdGuid) || null;
}

export default function RuleEditorActions({ devices, variables, actions, onChange, onParamChange, onRemove, onAdd }) {
  // Сброс ep при смене устройства
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
      <div className="panel-body">
        {actions.map((action) => {
          const device = devices.find(d => d.ieee_addr === action.device);
          //const ep = device?.endpoints.find(e => e.id === parseInt(action.ep));
          const cluster = getClusterOptions(device, action.ep).find(c => c.id === parseInt(action.cluster));
          const cmd = getCommandById(device, action.ep, cluster?.id, action.cmd);

          return (
            <div key={action.id} className="rule-action-item">
              {/* Тип действия */}
              <div className="rule-action-header">
                <label>Тип действия:</label>
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
                >
                  {actionTypes.map(act => (
                    <option key={act.value} value={act.value}>{act.label}</option>
                  ))}
                </select>
              </div>

              {/* Блок: Изменить переменную */}
              {action.type === 'set_var' && (
                <div style={{ marginLeft: '20px', paddingLeft: '20px', borderLeft: '2px solid #333' }}>
                  <div className="form-row">
                    <label>Переменная</label>
                    <select
                      value={action.target || ''}
                      onChange={(e) => {
                        const guid = e.target.value;
                        onChange(action.id, 'target', guid);
                        onChange(action.id, 'value', '');
                      }}
                    >
                      <option value="">Выберите</option>
                      {variables.map(v => (
                        <option key={v.guid} value={v.guid}>{v.name} ({v.guid})</option>
                      ))}
                    </select>
                  </div>

                  {action.target && (() => {
                    const dataType = action.dataType ?? variables.find(v => v.guid === action.target)?.type ?? 0x20;
                    return (
                      <>
                        <div className="form-row">
                          <label>Тип</label>
                          <span style={{ fontFamily: 'monospace', color: '#4ade80' }}>
                            {formatDataType(dataType)}
                          </span>
                        </div>
                        <div className="form-row">
                          <label>Значение</label>
                          {dataType === 0x42 || dataType === 0x43 ? (
                            <input
                              type="text"
                              value={action.value || ''}
                              onChange={(e) => onChange(action.id, 'value', e.target.value)}
                              placeholder="Введите строку"
                            />
                          ) : (
                            <input
                              type="number"
                              value={action.value || ''}
                              onChange={(e) => onChange(action.id, 'value', e.target.value)}
                              placeholder="1, 255..."
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

              {/* Блок: Отправить команду */}
              {action.type === 'send_cmd_device' && (
                <div style={{ marginLeft: '20px' }}>
                  <div className="form-row">
                    <label>Устройство</label>
                    <select
                      value={action.device || ''}
                      onChange={(e) => {
                        onChange(action.id, 'device', e.target.value);
                        onChange(action.id, 'ep', '');
                        onChange(action.id, 'cluster', '');
                        onChange(action.id, 'cmd', '');
                      }}
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
                    <label>Эндпоинт</label>
                    <select
                      value={action.ep || ''}
                      disabled={!action.device}
                      onChange={(e) => {
                        onChange(action.id, 'ep', e.target.value);
                        onChange(action.id, 'cluster', '');
                        onChange(action.id, 'cmd', '');
                      }}
                    >
                      <option value="">EP</option>
                      {device?.endpoints.map(ep => (
                        <option key={ep.id} value={ep.id}>EP {ep.id}</option>
                      ))}
                    </select>
                  </div>

                  <div className="form-row">
                    <label>Кластер</label>
                    <select
                      value={action.cluster || ''}
                      disabled={!action.ep}
                      onChange={(e) => {
                        onChange(action.id, 'cluster', e.target.value);
                        onChange(action.id, 'cmd', '');
                      }}
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
                    <label>Команда</label>
                    <select
                      value={action.cmd || ''}
                      disabled={!action.cluster}
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
                    >
                      <option value="">Выберите команду</option>
                      {cluster && getCommandOptions(cluster).map(cmd => (
                        <option key={cmd.guid} value={cmd.guid}>{cmd.name}</option>
                      ))}
                    </select>
                  </div>

                  {/* Параметры */}
                  {action.cmd && cmd?.params?.length > 0 && (
                    <table style={{ width: '100%', marginTop: '8px', fontSize: '12px' }}>
                      <thead>
                        <tr style={{ color: '#999', textAlign: 'left' }}>
                          <th>Параметр</th>
                          <th>Тип</th>
                          <th>Ввод</th>
                        </tr>
                      </thead>
                      <tbody>
                        {cmd.params.map((param, i) => (
                          <tr key={i}>
                            <td style={{ padding: '4px 0', color: '#ccc' }}>{param.name}</td>
                            <td style={{ color: '#777' }}>{formatDataType(param.type)}</td>
                            <td>
                              <input
                                type="text"
                                value={action.params[param.name] || ''}
                                onChange={(e) => onParamChange(action.id, param.name, e.target.value)}
                                style={{ width: '100%', padding: '4px', fontSize: '12px' }}
                              />
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  )}
                </div>
              )}

              {/* Кнопка удаления */}
              <button
                type="button"
                onClick={() => onRemove(action.id)}
                aria-label="Удалить действие"
                className="rule-action-remove"
              >
                ×
              </button>
            </div>
          );
        })}

        <button onClick={onAdd} className="btn-add-action">
          + Добавить действие
        </button>
      </div>
    </div>
  );
}