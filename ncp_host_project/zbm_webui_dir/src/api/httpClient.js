// src/api/httpClient.js

const API_BASE = '/api';

// Универсальный HTTP-клиент
const request = async (endpoint, options = {}) => {
  const url = `${API_BASE}${endpoint}`;
  const config = {
    headers: { 'Content-Type': 'application/json' },
    ...options
  };

  try {
    const res = await fetch(url, config);
    if (!res.ok) {
      const text = await res.text();
      throw new Error(`HTTP ${res.status}: ${text}`);
    }
    return await res.json();
  } catch (err) {
    console.error(`❌ Ошибка запроса к ${url}:`, err);
    throw err;
  }
};

// Экспортируем конкретные методы
export const api = {
  // Devices
  getDevices: () => request('/devices'),
  getDeviceByShort: (addr) => request(`/device/by_short?addr=${addr}`),
  updateFriendlyName: (ieee_addr, friendly_name) =>
    request('/device/update_friendly_name', {
      method: 'POST',
      body: JSON.stringify({ ieee_addr, friendly_name })
    }),

  // ZDO
  activeEndpointRequest: (short_addr) =>
    request('/zdo/active_endpoint_req', {
      method: 'POST',
      body: JSON.stringify({ short_addr })
    }),
  simpleDescriptorRequest: (short_addr, endpoint_id) =>
    request('/zdo/simple_desc', {
      method: 'POST',
      body: JSON.stringify({ short_addr, endpoint_id })
    }),

  // Network
  getZigbeeNetworkStatus: () =>
    request('/get/zigbee_network/status'),

  togglePermitJoin: (duration) =>
    request('/post/zbnetwork/open_close', {
      method: 'POST',
      body: JSON.stringify({ cmd: 'toggle_permit_join', duration })
    }),

  // Coordinator
  getCoordinator: () => request('/get/coordinator'),

  // Coordinator update
  updateCoordinator: (payload) =>
    request('/post/coordinator', {
      method: 'POST',
      body: JSON.stringify(payload)
    }),

  // Получение переменных
  getVariables: () => request('/get/vars'),

  // Обновление переменной
  updateVariable: (idx, payload) =>
    request(`/post/var/${idx}`, {
      method: 'POST',
      body: JSON.stringify(payload)
    }),
  
  // Получение списка правил
  getRulesList: () => request('/rules'),
  // Загрузка одного правила по ID
  getRuleById: (id) => request(`/rule/${id}`),

  // Создание нового правила
  createRule: (payload) =>
    request('/rule', {
      method: 'POST',
      body: JSON.stringify(payload)
    }),

  // Обновление правила
  updateRule: (id, payload) =>
    request(`/rule`, {
      method: 'POST',
      body: JSON.stringify({ ...payload, id })
    }),

  // Удаление правила
  deleteRule: (id) =>
    request(`/rule/${id}`, {
      method: 'DELETE'
    }),
    
  // Server status (health-check)
  getServerStatus: () =>
    request(`/get_server_status?t=${Date.now()}`, { method: 'GET', cache: 'no-cache' })
};