// src/components/DeviceDetails.js
import { fromZigbeeType, ZigbeeTypes, ZigbeeTypeNames } from '../utils/zigbeeTypes';
import { useState } from 'react';

export default function DeviceDetails({ device }) {
  if (!device) {
    return (
      <div className="flex-1 p-8 text-gray-500 flex items-center justify-center">
        <div className="text-center">
          <div className="text-6xl mb-4">🔌</div>
          <div className="text-lg font-medium">Выберите устройство</div>
          <div className="text-sm text-gray-600 mt-1">Чтобы посмотреть свойства</div>
        </div>
      </div>
    );
  }

  const ep = device.endpoints?.[0];

  const formatValue = (attr) => {
    if (attr.value !== undefined && attr.value !== null) {
      return String(attr.value);
    }
    if (attr.value_bytes && Array.isArray(attr.value_bytes)) {
      const buffer = new Uint8Array(attr.value_bytes);
      try {
        const parsed = fromZigbeeType(attr.type, buffer);
        return String(parsed);
      } catch (err) {
        return `<parse error: ${err.message}>`;
      }
    }
    return '—';
  };

  // Форматируем тип данных команды
  const formatDataType = (type) => {
    const types = {
      16: 'bool',
      32: 'uint8',
      33: 'uint16',
      35: 'uint32',
      48: 'enum8',
      66: 'string',
      255: 'unknown'
    };
    return types[type] || `0x${type.toString(16).padStart(2, '0')}`;
  };

  return (
    <div className="flex-1 p-6 overflow-y-auto bg-gradient-to-br from-gray-900/30 to-gray-850/20 flex flex-col gap-4">
      {/* Информация */}
      <div className="panel">
        <div className="panel-header">ℹ️ Основная информация</div>
        <div className="panel-body">
          <table>
            <tbody>
              <tr><td>IEEE</td><td><code className="text-blue-300">{device.ieee_addr}</code></td></tr>
              <tr><td>Short Addr</td><td><code className="text-green-300">{device.short_addr}</code></td></tr>
              <tr><td>LQI</td><td>{device.lqi || '—'}</td></tr>
              <tr>
                <td>Онлайн</td>
                <td>
                  <span className={`device-status ${device.is_online ? 'status-online' : 'status-offline'}`}>
                    {device.is_online ? 'Online' : 'Offline'}
                  </span>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>

       {/* Эндпоинты */}
      {device.endpoints && device.endpoints.length > 0 ? (
        device.endpoints.map((ep, idx) => (
          <div className="panel" key={ep.id}>
            <div className="panel-header">
              📡 EP {ep.id} | {ep.device_type || 'Unknown'} ({ep.profile_id ? `0x${ep.profile_id.toString(16).padStart(4, '0')}` : '—'})
            </div>
            <div className="panel-body flex flex-col gap-4">
              {/* Стандартные кластеры */}
              {ep.standard_clusters?.length > 0 ? (
                ep.standard_clusters.map(cluster => (
                  <div className="panel" key={cluster.id}>
                    <div className="panel-header">
                      ⚙️ {cluster.name} (0x{cluster.id.toString(16).padStart(4, '0')}) | EP {ep.id}
                    </div>
                    <div className="panel-body flex flex-col gap-4">
                      {/* Атрибуты */}
                      {cluster.attributes?.length > 0 && (
                        <div className="table-container mb-3">
                          <h5 className="text-xs font-medium text-gray-400 mb-1">Атрибуты</h5>
                          <table className="attribute-table">
                            <thead>
                              <tr>
                                <th>Атрибут</th>
                                <th>Тип</th>
                                <th>Значение</th>
                              </tr>
                            </thead>
                            <tbody>
                              {cluster.attributes.map(attr => (
                                <tr key={attr.guid || attr.id}>
                                  <td className="text-gray-300">
                                    <span>
                                      (0x{attr.id.toString(16).padStart(4, '0')}) {attr.name || 'UnknownAttr'}
                                    </span>
                                  </td>
                                  <td className="text-xs text-gray-500">
                                    {ZigbeeTypeNames[Number(attr.type)] || 'unknown'}
                                  </td>
                                  <td>
                                    <code className="text-yellow-300 bg-gray-800 px-2 py-0.5 rounded text-xs font-mono">
                                      {formatValue(attr)}
                                    </code>
                                  </td>
                                </tr>
                              ))}
                            </tbody>
                          </table>
                        </div>
                      )}

                      {/* Команды */}
                      {cluster.commands?.length > 0 && (
                        <div className="mb-3">
                          <h5 className="text-xs font-medium text-gray-400 mb-1">Команды</h5>
                          <div className="space-y-2">
                            {cluster.commands.map(cmd => (
                              <div key={cmd.guid || cmd.id} className="command-item bg-gray-800/50 p-2 rounded border border-gray-700">
                                <div className="flex justify-between items-start mb-1">
                                  <strong className="text-white text-xs">
                                    {cmd.name || `Cmd 0x${cmd.id.toString(16).padStart(2, '0')}`}
                                  </strong>
                                </div>
                                {cmd.params?.length > 0 ? (
                                  <table className="mt-1 w-full text-xs border-collapse">
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
                                          <td className="py-1 text-gray-300">{param.name || `Param ${i}`}</td>
                                          <td className="py-1 text-gray-500">{formatDataType(param.type)}</td>
                                          <td className="py-1">
                                            <input
                                              type="text"
                                              placeholder="значение"
                                              className="form-input text-xs px-2 py-1 h-6"
                                              style={{ fontSize: '11px', padding: '1px 4px' }}
                                            />
                                          </td>
                                        </tr>
                                      ))}
                                    </tbody>
                                  </table>
                                ) : (
                                  <p className="text-gray-500 text-xs italic">Нет параметров</p>
                                )}
                                <button className="btn-primary mt-1 text-xs px-2 py-0.5">
                                  Отправить
                                </button>
                              </div>
                            ))}
                          </div>
                        </div>
                      )}

                      {/* Custom Reports */}
                      {cluster.custom_reports?.length > 0 && (
                        <div className="mb-3">
                          <h5 className="text-xs font-medium text-gray-400 mb-1">Custom Reports</h5>
                          <div className="table-container">
                            <table className="attribute-table">
                              <thead>
                                <tr>
                                  <th>Репорт</th>
                                  <th>Тип</th>
                                  <th>Значение</th>
                                </tr>
                              </thead>
                              <tbody>
                                {cluster.custom_reports.map(report => (
                                  <tr key={report.guid || report.id}>
                                    <td className="text-gray-300">
                                      {report.name || `Report 0x${report.id.toString(16).padStart(2, '0')}`}
                                    </td>
                                    <td className="text-xs text-gray-500">
                                      {ZigbeeTypeNames[Number(report.type)] || `0x${report.type.toString(16).padStart(2, '0')}`}
                                    </td>
                                    <td>
                                      <code className="text-yellow-300 bg-gray-800 px-2 py-0.5 rounded text-xs font-mono">
                                        {formatValue(report)}
                                      </code>
                                    </td>
                                  </tr>
                                ))}
                              </tbody>
                            </table>
                          </div>
                        </div>
                      )}
                    </div>
                  </div>
                ))
              ) : (
                <div className="panel">
                  <div className="panel-header">⚠️ Нет стандартных кластеров</div>
                  <div className="panel-body text-gray-400">
                    На этом эндпоинте нет стандартных кластеров.
                  </div>
                </div>
              )}

                            {/* Кастомные кластеры (если есть) */}
              {ep.custom_clusters?.length > 0 && (
                <div className="mt-3 pt-3 border-t border-gray-700">
                  {/*<h4 className="text-sm font-semibold text-orange-300 mb-2">🔧 Кастомные кластеры</h4>*/}
                  {ep.custom_clusters.map(cluster => (
                    <div key={cluster.id} className="panel mb-3 bg-gray-800/40 border border-gray-700">
                      <div className="panel-header bg-gray-800 text-orange-200">
                        🧩 {cluster.name} (0x{cluster.id.toString(16).padStart(4, '0')}) | EP {ep.id}
                      </div>
                      <div className="panel-body flex flex-col gap-3">

                        {/* Атрибуты кастомного кластера */}
                        {cluster.attributes?.length > 0 && (
                          <div className="table-container">
                            <h5 className="text-xs font-medium text-gray-400 mb-1">Атрибуты</h5>
                            <table className="attribute-table">
                              <thead>
                                <tr>
                                  <th>Атрибут</th>
                                  <th>Тип</th>
                                  <th>Значение</th>
                                </tr>
                              </thead>
                              <tbody>
                                {cluster.attributes.map(attr => (
                                  <tr key={attr.guid || attr.id}>
                                    <td className="text-gray-300">
                                      <span>
                                        (0x{attr.id.toString(16).padStart(4, '0')}) {attr.name || 'UnknownAttr'}
                                      </span>
                                    </td>
                                    <td className="text-xs text-gray-500">
                                      {ZigbeeTypeNames[Number(attr.type)] || `0x${attr.type.toString(16).padStart(2, '0')}`}
                                    </td>
                                    <td>
                                      <code className="text-yellow-300 bg-gray-800 px-2 py-0.5 rounded text-xs font-mono">
                                        {formatValue(attr)}
                                      </code>
                                    </td>
                                  </tr>
                                ))}
                              </tbody>
                            </table>
                          </div>
                        )}

                        {/* Команды кастомного кластера */}
                        {cluster.commands?.length > 0 && (
                          <div>
                            <h5 className="text-xs font-medium text-gray-400 mb-1">Команды</h5>
                            <div className="space-y-2">
                              {cluster.commands.map(cmd => (
                                <div key={cmd.guid || cmd.id} className="command-item bg-gray-800/50 p-2 rounded border border-gray-700">
                                  <div className="flex justify-between items-start mb-1">
                                    <strong className="text-white text-xs">
                                      {cmd.name || `Cmd 0x${cmd.id.toString(16).padStart(2, '0')}`}
                                    </strong>
                                  </div>
                                  {cmd.params?.length > 0 ? (
                                    <table className="mt-1 w-full text-xs border-collapse">
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
                                            <td className="py-1 text-gray-300">{param.name || `Param ${i}`}</td>
                                            <td className="py-1 text-gray-500">{formatDataType(param.type)}</td>
                                            <td className="py-1">
                                              <input
                                                type="text"
                                                placeholder="значение"
                                                className="form-input text-xs px-2 py-1 h-6"
                                                style={{ fontSize: '11px', padding: '1px 4px' }}
                                              />
                                            </td>
                                          </tr>
                                        ))}
                                      </tbody>
                                    </table>
                                  ) : (
                                    <p className="text-gray-500 text-xs italic">Нет параметров</p>
                                  )}
                                  <button className="btn-primary mt-1 text-xs px-2 py-0.5">
                                    Отправить
                                  </button>
                                </div>
                              ))}
                            </div>
                          </div>
                        )}

                        {/* Custom Reports в кастомных кластерах */}
                        {cluster.custom_reports?.length > 0 && (
                          <div>
                            <h5 className="text-xs font-medium text-gray-400 mb-1">Custom Reports</h5>
                            <div className="table-container">
                              <table className="attribute-table">
                                <thead>
                                  <tr>
                                    <th>Репорт</th>
                                    <th>Тип</th>
                                    <th>Значение</th>
                                  </tr>
                                </thead>
                                <tbody>
                                  {cluster.custom_reports.map(report => (
                                    <tr key={report.guid || report.id}>
                                      <td className="text-gray-300">
                                        {report.name || `Report 0x${report.id.toString(16).padStart(2, '0')}`}
                                      </td>
                                      <td className="text-xs text-gray-500">
                                        {ZigbeeTypeNames[Number(report.type)] || `0x${report.type.toString(16).padStart(2, '0')}`}
                                      </td>
                                      <td>
                                        <code className="text-yellow-300 bg-gray-800 px-2 py-0.5 rounded text-xs font-mono">
                                          {formatValue(report)}
                                        </code>
                                      </td>
                                    </tr>
                                  ))}
                                </tbody>
                              </table>
                            </div>
                          </div>
                        )}
                      </div>
                    </div>
                  ))}
                </div>
              )}

            </div>
          </div>
        ))
      ) : (
        <div className="panel">
          <div className="panel-header">⚠️ Нет эндпоинтов</div>
          <div className="panel-body text-gray-400">
            Устройство не содержит доступных эндпоинтов.
          </div>
        </div>
      )}
    </div>
  );
}