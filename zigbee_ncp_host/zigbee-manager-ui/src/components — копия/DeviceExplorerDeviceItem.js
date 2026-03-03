// src/components/DeviceExplorerDeviceItem.js
import React, { useState } from 'react';
import { ClusterItem } from './DeviceExplorerClusterItem';
import { EndpointItem } from './DeviceExplorerEndpointItem';
import { getStandardAttrsForCluster } from '../utils/zclAttributes';

export const DeviceItem = ({
  dev,
  fullDev, // ✅ принимаем как проп
  discoveryForm,
  setDiscoveryForm,
  onDiscover,
  onReadAttribute,
  onWriteAttribute,
}) => {
  const [expanded, setExpanded] = useState(false);

  if (!fullDev) return null;

  const addrHex = dev.short.toString(16).padStart(4, '0').toUpperCase();

  return (
    <div key={dev.short} style={{ marginBottom: '12px' }}>
      <div
        style={{
          fontWeight: 'bold',
          cursor: 'pointer',
          padding: '8px',
          background: '#2d2d2d',
          borderRadius: '6px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
        }}
        onClick={() => setExpanded(!expanded)}
      >
        <span>📦 {dev.name} (0x{addrHex})</span>
        <span>{expanded ? '▼' : '▶'}</span>
      </div>

      {expanded && (
        <div style={{ marginLeft: '20px', marginTop: '8px', borderLeft: '2px solid #444', paddingLeft: '12px' }}>
          {/* Basic Cluster */}
          {fullDev.device_basic_cluster && (
            <ClusterItem
              icon="⚙️"
              title="Basic Cluster"
              clusterId={0}
              standardAttrs={getStandardAttrsForCluster(0, fullDev.device_basic_cluster)}
              customAttrs={fullDev.device_basic_cluster?.nostandart_attributes || []}
              deviceShort={dev.short}
              endpointId=""
              discoveryForm={{
                ...discoveryForm,
                short_addr: dev.short,
                endpoint: 1,
                cluster: 0,
              }}
              setDiscoveryForm={setDiscoveryForm}
              onDiscover={onDiscover}
              onReadAttribute={onReadAttribute}
              onWriteAttribute={onWriteAttribute}
            />
          )}

          {/* Power Config */}
          {fullDev.device_power_config_cluster && (
            <ClusterItem
              icon="🔋"
              title="Power Config"
              clusterId={1}
              standardAttrs={getStandardAttrsForCluster(1, fullDev.device_power_config_cluster)}
              customAttrs={fullDev.device_power_config_cluster?.nostandart_attributes || []}
              deviceShort={dev.short}
              endpointId=""
              discoveryForm={{
                ...discoveryForm,
                short_addr: dev.short,
                endpoint: 1,
                cluster: 1,
              }}
              setDiscoveryForm={setDiscoveryForm}
              onDiscover={onDiscover}
              onReadAttribute={onReadAttribute}
              onWriteAttribute={onWriteAttribute}
            />
          )}

          {/* Эндпоинты */}
          {fullDev.endpoints?.map((ep) => (
            <EndpointItem
              key={ep.ep_id}
              ep={ep}
              epObj={fullDev.endpoints_array?.find((e) => e.ep_id === ep.ep_id)}
              deviceShort={dev.short}
              discoveryForm={discoveryForm}
              setDiscoveryForm={setDiscoveryForm}
              onDiscover={onDiscover}
              onReadAttribute={onReadAttribute}
              onWriteAttribute={onWriteAttribute}
            />
          ))}
        </div>
      )}
    </div>
  );
};