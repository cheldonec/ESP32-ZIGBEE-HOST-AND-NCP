// src/utils/clusterNames.js

// Карта ID → имя (можно обновлять из zb_manager_clusters.c)
const KNOWN_CLUSTERS = {
  0x0000: 'Basic',
  0x0001: 'Power Config',
  0x0003: 'Identify',
  0x0004: 'Groups',
  0x0005: 'Scenes',
  0x0006: 'On/Off',
  0x0008: 'Level Control',
  0x000a: 'Time',
  0x0019: 'OTA Upgrade',
  0x0300: 'Color Control',
  0x0402: 'Temperature Measurement',
  0x0405: 'Relative Humidity Measurement',
  0x0500: 'IAS Zone',
  0x0702: 'Metering',
  0x0b04: 'Electrical Measurement',
  // Добавьте остальные по мере поддержки
};

export const getClusterName = (id) => {
  const hex = typeof id === 'string' ? parseInt(id, 16) : id;
  return KNOWN_CLUSTERS[hex] || 'Unknown Cluster';
};