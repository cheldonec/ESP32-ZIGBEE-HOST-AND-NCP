// src/components/DeviceCard.js
import React, { useState } from 'react';

const DeviceCard = ({
  device,
  onToggleDevice,
  onUpdateFriendlyName,
  onUpdateEndpointName,
  onOpenCommandModal,
}) => {
  const [editingEndpoint, setEditingEndpoint] = useState(null);
  const [editingFriendlyName, setEditingFriendlyName] = useState(false); // ← Новое состояние
  const dev = device;

  

  const handleFriendlyNameBlur = (e) => {
    const newName = e.target.value.trim();
    if (newName && newName !== dev.name) {
      onUpdateFriendlyName(dev.short, newName);
    }
  };

  const handleEndpointNameClick = () => {
    const firstCluster = dev.clusters.find(c => c.endpoint_id === Number(editingEndpoint?.endpoint_id));
    setEditingEndpoint({
      short: dev.short,
      endpoint_id: Number(editingEndpoint?.endpoint_id),
      originalName: firstCluster?.endpoint_name || '',
    });
  };

  const handleEndpointNameSave = (e) => {
    const newName = e.target.value.trim();
    if (newName) {
      onUpdateEndpointName(dev.short, editingEndpoint.endpoint_id, newName);
    }
    setEditingEndpoint(null);
  };

  const renderClusterValue = (cluster) => {
    const icon = cluster.type === 'on_off' ? (cluster.value ? '🟢' : '🔴') :
               cluster.type === 'temperature' ? '🌡️' :
               cluster.type === 'humidity' ? '💧' : '🔧';

    if (!dev.online) {
      return (
        <div className="state-indicator offline">
          {icon} ⚠️ Offline
        </div>
      );
    }
    
    if (cluster.type === 'on_off') {
      return (
        <div className={`state-indicator ${cluster.value ? 'on' : 'off'}`}>
          {cluster.value ? '🟢 ВКЛ' : '🔴 ВЫКЛ'}
          <button
            className="toggle-btn"
            onClick={() => onToggleDevice(dev.short, cluster.endpoint_id)}
          >
            {cluster.value ? 'Выключить' : 'Включить'}
          </button>
        </div>
      );
    }

    if (cluster.type === 'temperature') {
      return (
        <div className="sensor-value">
          <span>🌡️</span>
          <span>Температура: <strong>{cluster.display}</strong></span>
        </div>
      );
    }

    if (cluster.type === 'humidity') {
      return (
        <div className="sensor-value">
          <span>💧</span>
          <span>Влажность: <strong>{cluster.display}</strong></span>
        </div>
      );
    }

    return <div className="unknown">⚠️ {cluster.display}</div>;
  };

  // Группировка кластеров по endpoint
  const clustersByEp = dev.clusters.reduce((acc, cluster) => {
    if (!acc[cluster.endpoint_id]) acc[cluster.endpoint_id] = [];
    acc[cluster.endpoint_id].push(cluster);
    return acc;
  }, {});

  return (
    <div className="device-card">
      {/* Заголовок устройства */}
      <div className="device-header">
        <div className="device-name-edit">
          {editingFriendlyName ? (
            <input
                type="text"
                className="friendly-name-input"
                defaultValue={dev.name}
                onBlur={(e) => {
                const newName = e.target.value.trim();
                if (newName && newName !== dev.name) {
                    onUpdateFriendlyName(dev.short, newName);
                }
                setEditingFriendlyName(false);
                }}
                onKeyDown={(e) => {
                if (e.key === 'Enter') {
                    const newName = e.target.value.trim();
                    if (newName && newName !== dev.name) {
                    onUpdateFriendlyName(dev.short, newName);
                    }
                    setEditingFriendlyName(false);
                } else if (e.key === 'Escape') {
                    setEditingFriendlyName(false);
                }
                }}
                autoFocus
                onClick={(e) => e.target.select()}
            />
            ) : (
            <span
                className="friendly-name-display"
                onClick={() => setEditingFriendlyName(true)}
                title="Нажмите, чтобы изменить имя"
            >
                {dev.name || `0x${dev.short.toString(16).padStart(4, '0').toUpperCase()}`}
            </span>
            )}
        </div>
        <span className={`status-dot ${dev.online ? 'online' : 'offline'}`}></span>
      </div>

      {/* Информация об устройстве */}
      <div className="device-info">
        <small>
          🔗 {`0x${dev.short.toString(16).padStart(4, '0').toUpperCase()}`}
          {dev.model_id && (
            <>
              {' | '}
              <span title="Model ID" style={{ color: '#aaa', fontSize: '0.9em' }}>
                {dev.model_id}
              </span>
            </>
          )}
          {dev.manufacturer_name && (
            <>
              {' | '}
              <span title="Manufacturer" style={{ color: '#888', fontSize: '0.8em' }}>
                {dev.manufacturer_name}
              </span>
            </>
          )}
          {dev.battery && (
            <>
              {' | '}
              <span
                title={`Батарея: ${dev.battery.display}${dev.battery.percent_display ? ' (' + dev.battery.percent_display + ')' : ''}`}
              >
                🔋 {dev.battery.display}
                {dev.battery.percent_display && <> ({dev.battery.percent_display})</>}
              </span>
            </>
          )}
        </small>
      </div>

      {/* Кластеры по endpoint */}
      <div className="device-content">
        {Object.entries(clustersByEp).map(([epId, clusters]) => {
          const firstCluster = clusters[0];

          // Проверяем, есть ли среди кластеров этого endpoint'а тип 'on_off'
          const hasOnOffCluster = clusters.some(c => c.type === 'on_off');

          const isEditing =
            editingEndpoint &&
            editingEndpoint.short === dev.short &&
            editingEndpoint.endpoint_id === Number(epId);

          return (
            <div key={epId} className="cluster-group">
              <div className="endpoint-name-edit">
                {isEditing ? (
                  <input
                    autoFocus
                    defaultValue={editingEndpoint.originalName}
                    onBlur={handleEndpointNameSave}
                    onKeyDown={(e) => {
                      if (e.key === 'Enter') {
                        handleEndpointNameSave(e);
                      } else if (e.key === 'Escape') {
                        setEditingEndpoint(null);
                      }
                    }}
                    className="endpoint-edit-input"
                  />
                ) : (
                  <div className="endpoint-header">
                    <span
                      onClick={() =>
                        setEditingEndpoint({
                          short: dev.short,
                          endpoint_id: Number(epId),
                          originalName: firstCluster.endpoint_name,
                        })
                      }
                      className="endpoint-name-label"
                      title="Нажмите, чтобы переименовать endpoint"
                    >
                      🏷️ {firstCluster.endpoint_name}
                    </span>

                    {/* Показываем кнопку команд только если есть кластер on_off */}
                    {hasOnOffCluster && (
                    <button
                        className="action-menu-btn"
                        title="Тестовые команды"
                        onClick={(e) => {
                        e.stopPropagation();
                        onOpenCommandModal(dev, epId);
                        }}
                    >
                        ⋮
                    </button>
                    )}
                  </div>
                )}
              </div>

              {clusters.map((cluster, idx) => (
                <div key={idx} className="cluster-item">
                  {renderClusterValue(cluster)}
                </div>
              ))}
            </div>
          );
        })}
      </div>
    </div>
  );
};

export default DeviceCard;