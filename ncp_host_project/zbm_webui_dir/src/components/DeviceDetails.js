// src/components/DeviceDetails.js

import { useState } from 'react';
import { fromZigbeeType, ZigbeeTypeNames } from '../utils/zigbeeTypes';
import { api } from '../api/httpClient';
//import { subscribeToWebSocket, initWebSocket } from '../api/websocket';

export default function DeviceDetails({ device }) {
  // ✅ Перенесены внутрь компонента
  const [inputValues, setInputValues] = useState({});

  // Функция обновления значения параметра
  const handleParamChange = (cmdId, paramIndex, value) => {
    setInputValues(prev => ({
      ...prev,
      [`${cmdId}-${paramIndex}`]: value
    }));
  };

  // === Функция отправки команды ===
  const handleSendCommand = (cmd, ep, cluster) => {
    const cmdGuid = cmd.guid;
    const clusterId = cluster.id;
    const endpointId = ep.id;

    // Собираем параметры
    const params = (cmd.params || []).map((param, i) => {
      const key = `${cmd.id}-${i}`;
      const rawValue = inputValues[key] || '';
      let value;

      // Пробуем распознать тип
      switch (param.type) {
        case 16: // bool
          value = rawValue.toLowerCase() === 'true' || rawValue === '1';
          break;
        case 32: // uint8
        case 33: // uint16
        case 35: // uint32
          value = parseInt(rawValue, 10);
          if (isNaN(value)) value = 0;
          break;
        case 66: // string
          value = String(rawValue);
          break;
        case 48: // enum8
          value = parseInt(rawValue, 10);
          if (isNaN(value)) value = 0;
          break;
        default:
          value = rawValue;
      }

      return {
        name: param.name,
        type: param.type,
        value
      };
    });

    // Формируем сообщение
    const message = {
      cmd: 'send_zcl_command',
      guid: cmdGuid,
      cluster_id: clusterId,
      endpoint_id: endpointId,
      params
    };

    // Отправляем через WebSocket
    if (window.ws && window.ws.readyState === WebSocket.OPEN) {
      window.ws.send(JSON.stringify(message));
      console.log('Command sent:', message);
    } else {
      alert('WebSocket не подключён');
    }
  };

  // Остальная часть остаётся без изменений
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
              <tr>
                <td>IEEE</td>
                <td><code className="text-blue-300">{device.ieee_addr}</code></td>
              </tr>
              <tr>
                <td>Short Addr</td>
                <td><code className="text-green-300">{device.short_addr}</code></td>
              </tr>
              <tr>
                <td>LQI</td>
                <td>{device.lqi || '—'}</td>
              </tr>
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

          {/* Кнопка: Active Endpoint Request */}
          <div className="mt-3 pt-3 border-t border-gray-700 flex justify-center">
            <button
              className="btn-primary text-xs px-3 py-1.5"
              onClick={async () => {
                const short = parseInt(device.short_addr.replace('0x', ''), 16);
                try {
                  const data = await api.activeEndpointRequest(short);
                  if (data.status === 'success') {
                    alert(`✅ Запрос отправлен: Active Endpoint для 0x${short.toString(16).toUpperCase()}`);
                  } else {
                    alert(`❌ Ошибка: ${data.message}`);
                  }
                } catch (err) {
                  alert(`Ошибка сети: ${err.message}`);
                }
              }}
            >
              📡 Запросить эндпоинты (Active EP)
            </button>
          </div>
        </div>
      </div>

      {/* Эндпоинты */}
      {device.endpoints && device.endpoints.length > 0 ? (
        device.endpoints.map((ep, idx) => (
          <div className="panel" key={ep.id}>
            <div className="panel-header flex items-center justify-between">
              <div className="flex items-center gap-2">
                📡 <strong>EP {ep.id}</strong> | {ep.device_type || 'Unknown'} 
                {ep.profile_id && (
                  <span className="text-xs text-gray-400">
                    (0x{ep.profile_id.toString(16).padStart(4, '0')})
                  </span>
                )}
              </div>
              <button
                className="btn-primary text-xs px-3 py-1.5 mt-1"
                onClick={async (e) => {
                  e.stopPropagation();
                  const short = parseInt(device.short_addr.replace('0x', ''), 16);
                  try {
                    const data = await api.simpleDescriptorRequest(short, ep.id);
                    if (data.status === 'success') {
                      alert(`✅ Запрос отправлен: Simple Descriptor для EP ${ep.id}, 0x${short.toString(16).toUpperCase()}`);
                    } else {
                      alert(`❌ Ошибка: ${data.message}`);
                    }
                  } catch (err) {
                    alert(`Ошибка сети: ${err.message}`);
                  }
                  /*try {
                    const res = await fetch('/api/zdo/simple_desc', {
                      method: 'POST',
                      headers: { 'Content-Type': 'application/json' },
                      body: JSON.stringify({
                        short_addr: short,
                        endpoint_id: ep.id
                      })
                    });

                    const data = await res.json();
                    if (data.status === 'success') {
                      alert(`✅ Запрос отправлен: Simple Descriptor для EP ${ep.id}, 0x${short.toString(16).toUpperCase()}`);
                    } else {
                      alert(`❌ Ошибка: ${data.message}`);
                    }
                  } catch (err) {
                    alert(`Ошибка сети: ${err.message}`);
                  }*/
                }}
                title={`Запросить Simple Descriptor для EP ${ep.id}`}
              >
                📥 Запросить кластеры (Simple Desc)
              </button>
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
                                              onChange={(e) => handleParamChange(cmd.id, i, e.target.value)}
                                              defaultValue=""
                                            />
                                          </td>
                                        </tr>
                                      ))}
                                    </tbody>
                                  </table>
                                ) : (
                                  <p className="text-gray-500 text-xs italic">Нет параметров</p>
                                )}
                                <button
                                  className="btn-primary mt-1 text-xs px-2 py-0.5"
                                  onClick={() => handleSendCommand(cmd, ep, cluster)}
                                >
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

              {/* Кастомные кластеры */}
              {ep.custom_clusters?.length > 0 && (
                <div className="mt-3 pt-3 border-t border-gray-700">
                  {ep.custom_clusters.map(cluster => (
                    <div key={cluster.id} className="panel mb-3 bg-gray-800/40 border border-gray-700">
                      <div className="panel-header bg-gray-800 text-orange-200">
                        🧩 {cluster.name} (0x{cluster.id.toString(16).padStart(4, '0')}) | EP {ep.id}
                      </div>
                      <div className="panel-body flex flex-col gap-3">
                        {/* Атрибуты */}
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
                                      (0x{attr.id.toString(16).padStart(4, '0')}) {attr.name || 'UnknownAttr'}
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

                        {/* Команды */}
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
                                                onChange={(e) => handleParamChange(cmd.id, i, e.target.value)}
                                                defaultValue=""
                                              />
                                            </td>
                                          </tr>
                                        ))}
                                      </tbody>
                                    </table>
                                  ) : (
                                    <p className="text-gray-500 text-xs italic">Нет параметров</p>
                                  )}
                                  <button
                                    className="btn-primary mt-1 text-xs px-2 py-0.5"
                                    onClick={() => handleSendCommand(cmd, ep, cluster)}
                                  >
                                    Отправить
                                  </button>
                                </div>
                              ))}
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