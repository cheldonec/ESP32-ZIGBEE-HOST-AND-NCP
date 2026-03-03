// src/components/DeviceExplorerEndpointItem.js
import React, { useState } from 'react';
import { ClusterItem } from './DeviceExplorerClusterItem';
import { getStandardAttrsForCluster } from '../utils/zclAttributes';
import { getClusterName } from '../utils/clusterNames';

export const EndpointItem = ({
  ep,
  epObj,
  deviceShort,
  discoveryForm,
  setDiscoveryForm,
  onDiscover,
  onReadAttribute,
  onWriteAttribute,
}) => {
  const [expanded, setExpanded] = useState(false);

  // Известные кластеры
  const knownClusters = [];
  if (ep.device_basic_cluster) {
    knownClusters.push({ id: 0, name: 'Basic', data: ep.device_basic_cluster });
  }
  if (ep.device_power_config_cluster) {
    knownClusters.push({ id: 1, name: 'Power Config', data: ep.device_power_config_cluster });
  }
  if (ep.temperature) {
    knownClusters.push({ id: 1026, name: 'Temperature', data: ep.temperature });
  }
  if (ep.humidity) {
    knownClusters.push({ id: 1029, name: 'Humidity', data: ep.humidity });
  }
  if (ep.onoff) {
    knownClusters.push({ id: 6, name: 'On/Off', data: ep.onoff });
  }

  // Неизвестные input-кластеры
  const unknownInputClusters = ep.unknown_input_clusters || [];

  return (
    <div key={ep.ep_id}>
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          padding: '6px 0',
          cursor: 'pointer',
        }}
        onClick={() => setExpanded(!expanded)}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <input
            type="checkbox"
            checked={!!ep.is_use_on_device}
            onChange={() => {}}
            onClick={(e) => e.stopPropagation()}
          />
          <span style={{ fontWeight: 500 }}>
            EP {ep.ep_id} — {ep.friendly_name || 'Unknown'}
          </span>
        </div>
        <span>{expanded ? '▼' : '▶'}</span>
      </div>

      {expanded && (
        <div style={{ marginLeft: '24px', marginTop: '6px' }}>
          {/* === ИЗВЕСТНЫЕ КЛАСТЕРЫ === */}
          {knownClusters.map((cl) => (
            <ClusterItem
              key={cl.id}
              icon="🔗"
              title={cl.name}
              clusterId={cl.id}
              standardAttrs={getStandardAttrsForCluster(cl.id, cl.data)}
              customAttrs={
                (() => {
                  if (cl.id === 0) return ep?.device_basic_cluster?.nostandart_attributes || [];
                  if (cl.id === 1) return ep?.device_power_config_cluster?.nostandart_attributes || [];
                  if (cl.id === 1026) return ep?.temperature?.nostandart_attributes || [];
                  if (cl.id === 1029) return ep?.humidity?.nostandart_attributes || [];
                  if (cl.id === 6) return ep?.onoff?.nostandart_attributes || [];
                  return [];
                })()
              }
              deviceShort={deviceShort}
              endpointId={ep.ep_id}
              discoveryForm={{
                ...discoveryForm,
                short_addr: deviceShort,
                endpoint: ep.ep_id,
                cluster: cl.id,
              }}
              setDiscoveryForm={setDiscoveryForm}
              onDiscover={onDiscover}
              onReadAttribute={onReadAttribute}
              onWriteAttribute={onWriteAttribute}
            />
          ))}

          {/* === НЕИЗВЕСТНЫЕ INPUT-КЛАСТЕРЫ === */}
          {unknownInputClusters.length > 0 &&
            unknownInputClusters.map((cl) => {
              const clusterName = cl.cluster_id_text || 'Unknown Input Cluster';
              const clusterId = cl.id;
              const icon = [0x0000, 0x0001, 0x0003, 0x0004, 0x0005, 0x0006, 0x0402, 0x0405, 0x0702, 0x0b04].includes(clusterId)
                ? '🔗'
                : '❓';

              return (
                <ClusterItem
                  key={`unk-${clusterId}`}
                  icon={icon}
                  title={`${clusterName} (0x${clusterId.toString(16).padStart(4, '0')})`}
                  clusterId={clusterId}
                  standardAttrs={[]}
                  customAttrs={cl.attributes || []}  // ← Здесь теперь передаются атрибуты
                  deviceShort={deviceShort}
                  endpointId={ep.ep_id}
                  discoveryForm={{
                    ...discoveryForm,
                    short_addr: deviceShort,
                    endpoint: ep.ep_id,
                    cluster: clusterId,
                  }}
                  setDiscoveryForm={setDiscoveryForm}
                  onDiscover={onDiscover}
                  onReadAttribute={onReadAttribute}
                  onWriteAttribute={onWriteAttribute}
                />
              );
            })}

          {/* === OUTPUT-КЛАСТЕРЫ === */}
          {Array.isArray(ep?.output_clusters) && ep.output_clusters.length > 0 && (
            <div style={{ marginLeft: '10px', marginTop: '6px', fontSize: '13px', color: '#888' }}>
              <strong>📤 Output Clusters:</strong>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px', marginTop: '4px' }}>
                {ep.output_clusters.map((id) => {
                  const hexId = typeof id === 'number' ? id.toString(16).padStart(4, '0') : id;
                  const name = getClusterName(id);
                  return (
                    <span
                      key={`out-${id}`}
                      title={name}
                      style={{
                        background: '#555',
                        padding: '2px 6px',
                        borderRadius: '4px',
                        fontSize: '12px',
                        cursor: 'help',
                      }}
                    >
                      0x{hexId} ({name})
                    </span>
                  );
                })}
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  );
};