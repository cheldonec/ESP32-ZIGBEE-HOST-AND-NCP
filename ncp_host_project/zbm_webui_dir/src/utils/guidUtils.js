// src/utils/guidUtils.js
//import { fromZigbeeType } from './zigbeeTypes';

/**
 * Парсит GUID в структуру
 * Пример: "0xAB2D:1:rep:0006:FD" → { short: '0xAB2D', epId: 1, type: 'rep', clusterId: 0x0006, attrId: 0xFD }
 * ВАЖНО: первая часть — это short_addr, а не ieee_addr!
 */
export const parseGuid = (guid) => {
  if (!guid || typeof guid !== 'string') return null;

  const parts = guid.split(':');
  if (parts.length !== 5) return null;

  const [short, epStr, type, clusterStr, attrStr] = parts;
  const epId = parseInt(epStr, 10);
  const clusterId = parseInt(clusterStr, 16);
  const attrId = parseInt(attrStr, 16);

  // Проверяем валидность
  if (isNaN(epId) || isNaN(clusterId) || isNaN(attrId)) return null;

  return { short, epId, type, clusterId, attrId };
};

/**
 * Находит устройство по short_addr (например, "0x7169")
 */
export const findDeviceByShort = (devices, shortAddr) => {
  if (!devices || !shortAddr) return null;
  const normalized = shortAddr.toLowerCase();
  return devices.find(d => d.short_addr?.toLowerCase() === normalized);
};

/**
 * Находит устройство по IEEE (полный 64-битный адрес)
 */
export const findDeviceByIeee = (devices, ieeeAddr) => {
  if (!devices || !ieeeAddr) return null;
  return devices.find(d => d.ieee_addr === ieeeAddr);
};

/**
 * Находит endpoint по ID
 */
export const findEndpointById = (device, epId) => {
  return device?.endpoints?.find(ep => ep.id === epId);
};

/**
 * Находит кластер по ID
 * ВАЖНО: для custom_report ищем во всех кластерах (включая standard), где есть custom_reports
 */
export const findClusterById = (endpoint, clusterId, isCustom = false) => {
  if (!endpoint) return null;

  // Для custom report ищем среди всех кластеров, где может быть custom_reports
  if (isCustom) {
    const allClusters = [
      ...(endpoint.standard_clusters || []),
      ...(endpoint.custom_clusters || [])
    ];
    return allClusters.find(c => c.id === clusterId);
  }

  // Обычные кластеры
  const list = [
    ...(endpoint.standard_clusters || []),
    ...(endpoint.in_clusters || [])
  ];
  return list?.find(c => c.id === clusterId);
};

/**
 * Находит атрибут по ID
 */
export const findAttributeById = (cluster, attrId) => {
  return cluster?.attributes?.find(a => a.id === attrId);
};

/**
 * Находит custom report по ID
 */
export const findCustomReportById = (cluster, attrId) => {
  return cluster?.custom_reports?.find(r => r.id === attrId) || null;
};

/**
 * Получает читаемое имя из любого GUID
 */
export const getLabelFromGuid = (guid, devices) => {
  const parsed = parseGuid(guid);
  if (!parsed) return guid;

  const { short, epId, type, clusterId, attrId } = parsed;
  const device = findDeviceByShort(devices, short);
  if (!device) return `${short} (устройство не найдено)`; // short, не IEEE

  const ep = findEndpointById(device, epId);
  if (!ep) return `EP ${epId}`;

  const isCustomReport = type === 'rep';
  const cluster = findClusterById(ep, clusterId, isCustomReport);
  if (!cluster) return `Cl 0x${clusterId.toString(16)}`;

  const item = isCustomReport
    ? findCustomReportById(cluster, attrId)
    : findAttributeById(cluster, attrId);

  if (item) {
    return item.name || `Attr 0x${attrId.toString(16)}`;
  }

  return `0x${attrId.toString(16)}`;
};

/**
 * Получает полную цепочку: устройство > эндпоинт > кластер > атрибут
 * @returns {Object|null} { device, ep, cluster, attr, isCustomReport } или null
 */
export const resolveGuidPath = (guid, devices) => {
  const parsed = parseGuid(guid);
  if (!parsed) return null;

  const { short, epId, type, clusterId, attrId } = parsed;

  const device = findDeviceByShort(devices, short);
  if (!device) return null;

  const ep = findEndpointById(device, epId);
  if (!ep) return null;

  const isCustomReport = type === 'rep';
  const cluster = findClusterById(ep, clusterId, isCustomReport);
  if (!cluster) return null;

  const attr = isCustomReport
    ? findCustomReportById(cluster, attrId)
    : findAttributeById(cluster, attrId);

  if (!attr) return null;

  return { device, ep, cluster, attr, isCustomReport };
};

/**
 * Находит тип данных (data_type) по GUID, используя devices
 * ВАЖНО: первая часть GUID — это short_addr (например, "0x97EC"), а не ieee_addr!
 */
export function findDataTypeByGuid(guid, devices) {
  if (!guid || !devices) {
    console.warn('❌ findDataTypeByGuid: no guid or devices', { guid, devices });
    return null;
  }

  const parts = guid.split(':');
  if (parts.length < 5) {
    console.warn('❌ Invalid GUID format', guid);
    return null;
  }

  const shortAddr = parts[0];
  const epId = parseInt(parts[1], 10);
  const typePart = parts[2];
  const clusterId = parseInt(parts[3], 16);
  const attrId = parseInt(parts[4], 16);

  // Ищем устройство
  const device = devices.find(d => d.short_addr?.toLowerCase() === shortAddr.toLowerCase());
  if (!device) {
    console.warn(`Device not found for short_addr: ${shortAddr}`);
    return null;
  }

  const ep = device.endpoints.find(e => e.id === epId);
  if (!ep) {
    console.warn(`Endpoint ${epId} not found on device ${shortAddr}`);
    return null;
  }

  if (typePart === 'rep') {
    const cluster = [
      ...(ep.standard_clusters || []),
      ...(ep.custom_clusters || [])
    ].find(c => c.id === clusterId);

    if (!cluster) {
      console.warn(`Cluster 0x${clusterId.toString(16)} not found on EP ${epId}`);
      return null;
    }

    const report = cluster.custom_reports?.find(r => r.id === attrId);
    if (report) {
      console.log(`✅ Found custom_report type: ${report.type} for ${guid}`);
      return report.type;
    }

    console.warn(`Custom report not found: id=${attrId} in cluster 0x${clusterId.toString(16)}`);
    return null;
  }

  if (typePart === 'attr') {
    const cluster = [
      ...(ep.standard_clusters || []),
      ...(ep.custom_clusters || [])
    ].find(c => c.id === clusterId);

    const attr = cluster?.attributes?.find(a => a.id === attrId);
    if (attr) {
      console.log(`✅ Found attribute data_type: ${attr.data_type} for ${guid}`);
      return attr.data_type;
    }
    console.warn(`Attribute not found: cluster=${clusterId}, attr=${attrId}`);
    return null;
  }

  console.warn(`Unknown typePart: ${typePart}`);
  return null;
}