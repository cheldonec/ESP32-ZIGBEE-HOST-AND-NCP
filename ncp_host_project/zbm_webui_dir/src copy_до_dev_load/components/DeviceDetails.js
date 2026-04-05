// src/components/DeviceDetails.js
import { fromZigbeeType, ZigbeeTypes } from '../utils/zigbeeTypes';

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
    // Если есть value — используем его
    if (attr.value !== undefined && attr.value !== null) {
      return String(attr.value);
    }
    // Если есть байты — парсим через fromZigbeeType
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

  return (
    <div className="flex-1 p-6 overflow-y-auto bg-gradient-to-br from-gray-900/30 to-gray-850/20">
      <h2 className="text-2xl font-bold text-white mb-1">{device.name || 'Устройство'}</h2>
      <p className="text-gray-500 text-sm mb-6">
        IEEE: <code className="text-gray-400 font-mono">{device.ieee_addr}</code>
      </p>

      <div className="space-y-4">
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
          </div>
        </div>

        {/* Кластеры */}
        {ep && ep.standard_clusters?.length > 0 ? (
          ep.standard_clusters.map(cluster => (
            <div className="panel" key={cluster.id}>
              <div className="panel-header">
                ⚙️ {cluster.name} (0x{cluster.id.toString(16).padStart(4, '0')})
              </div>
              <div className="panel-body">
                <table className="attribute-table">
                  <thead>
                    <tr>
                      <th>Атрибут</th>
                      <th>Тип</th>
                      <th>Значение</th>
                    </tr>
                  </thead>
                  <tbody>
                    {cluster.attributes?.map(attr => (
                      <tr key={attr.guid || attr.id}>
                        <td className="text-gray-300">
                          {attr.name || `Attr 0x${attr.id.toString(16).padStart(4, '0')}`}
                        </td>
                        <td className="text-xs text-gray-500">
                          {attr.type} ({ZigbeeTypes[attr.type]?.name || 'unknown'})
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
            </div>
          ))
        ) : (
          <div className="panel">
            <div className="panel-header">⚠️ Нет данных</div>
            <div className="panel-body text-gray-400">
              Устройство не содержит доступных кластеров.
            </div>
          </div>
        )}
      </div>
    </div>
  );
}