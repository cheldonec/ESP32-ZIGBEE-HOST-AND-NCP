// src/components/DeviceExplorerUnknownCluster.js
import React from 'react';
import { ClusterItem } from './DeviceExplorerClusterItem';

/**
 * Отображает неизвестный input-кластер как исследуемый
 */
export const UnknownCluster = ({
  cluster,
  deviceShort,
  endpointId,
  discoveryForm,
  setDiscoveryForm,
  onDiscover,
  onReadAttribute,
  onWriteAttribute,
}) => {
  // Если мы знаем имя кластера — используем его
  const clusterName = cluster.cluster_id_text || 'Unknown Input Cluster';
  const clusterId = cluster.id;

  // Определяем иконку
  const icon = [
    0x0000, 0x0001, 0x0003, 0x0004, 0x0005, 0x0006, 0x0008, 0x0300, 0x0402, 0x0405, 0x0500, 0x0702, 0x0b04
  ].includes(clusterId) ? '🔗' : '❓';

  return (
    <ClusterItem
      key={clusterId}
      icon={icon}
      title={`${clusterName} (0x${clusterId.toString(16).padStart(4, '0')})`}
      clusterId={clusterId}
      standardAttrs={[]}
      customAttrs={[]}
      deviceShort={deviceShort}
      endpointId={endpointId}
      discoveryForm={{
        ...discoveryForm,
        short_addr: deviceShort,
        endpoint: endpointId,
        cluster: clusterId,
      }}
      setDiscoveryForm={setDiscoveryForm}
      onDiscover={onDiscover}
      onReadAttribute={onReadAttribute}
      onWriteAttribute={onWriteAttribute}
    />
  );
};