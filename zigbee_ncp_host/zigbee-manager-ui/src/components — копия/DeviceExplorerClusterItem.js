// src/components/DeviceExplorerClusterItem.js
import React from 'react';
import { DiscoveryControls } from './DeviceExplorerDiscoveryControls';
import { AttributeList } from './DeviceExplorerAttributeList';

export const ClusterItem = ({
  icon,
  title, // уже содержит "(0x...)"
  clusterId, // не используется в разметке
  standardAttrs,
  customAttrs,
  deviceShort,
  endpointId,
  discoveryForm,
  setDiscoveryForm,
  onDiscover,
  onReadAttribute,
  onWriteAttribute,
}) => {
  const handleDiscover = () => {
    const short_addr = Number(discoveryForm.short_addr);
    const cluster = Number(discoveryForm.cluster);

    if (isNaN(short_addr) || short_addr <= 0) {
      alert('Некорректный адрес устройства');
      return;
    }

    if (isNaN(cluster) || cluster < 0) {
      alert('Некорректный ID кластера');
      return;
    }

    console.log('Отправляем:', { short_addr, cluster });
      onDiscover(discoveryForm);
    };

    const handleChange = (updates) => {
      setDiscoveryForm((prev) => ({ ...prev, ...updates }));
    };

  return (
    <div style={{ marginBottom: '10px' }}>
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          padding: '6px',
          background: '#333',
          borderRadius: '6px',
          fontSize: '14px',
        }}
      >
        {/* ❌ Было: {icon} {title} (0x...) */}
        {/* ✅ Стало: {icon} {title} — title уже содержит ID */}
        <span>{icon} {title}</span>
        <DiscoveryControls
          form={discoveryForm}
          onChange={handleChange}
          onDiscover={handleDiscover}
        />
      </div>
      <AttributeList
        clusterId={clusterId}
        standardAttrs={standardAttrs}
        customAttrs={customAttrs}
        deviceShort={deviceShort}
        endpointId={endpointId}
        onReadAttribute={onReadAttribute}
        onWriteAttribute={onWriteAttribute}
      />
    </div>
  );
};