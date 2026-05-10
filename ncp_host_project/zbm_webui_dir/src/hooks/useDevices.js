// src/hooks/useDevices.js

import { useState, useEffect, useRef, useCallback } from 'react';

import { api } from '../api/httpClient';
import { subscribeToWebSocket } from '../api/websocket';
import { fromZigbeeType } from '../utils/zigbeeTypes';

// 🟩 Глобальный кэш и флаг
let devicesCache = [];
let hasLoadedDevices = false;

export const useDevices = ({ onAttributeUpdate, onSystemNotify, enabled = true } = {}) => {
  const [devices, setDevices] = useState(devicesCache);
  const [loading, setLoading] = useState(() => !hasLoadedDevices);
  const [error, setError] = useState(null);

  const onAttributeUpdateRef = useRef();
  const onSystemNotifyRef = useRef();

  useEffect(() => {
    onAttributeUpdateRef.current = onAttributeUpdate;
  }, [onAttributeUpdate]);

  useEffect(() => {
    onSystemNotifyRef.current = onSystemNotify;
  }, [onSystemNotify]);

  const devicesRef = useRef(devices);
  useEffect(() => {
    devicesRef.current = devices;
  }, [devices]);

  // --- Обработчики ---
  const refreshDeviceByShort = async (shortAddr) => {
    try {
      const updatedDevice = await api.getDeviceByShort(shortAddr);
      setDevices(prev => {
        const updated = prev.map(d => d.short_addr === shortAddr ? updatedDevice : d);
        devicesCache = updated; // обновляем кэш
        return updated;
      });
    } catch (err) {
      console.warn(`❌ Не удалось обновить устройство: ${shortAddr}`, err);
    }
  };

  const updateDeviceAttribute = useCallback((update) => {
    const { guid, type, value_bytes } = update;
    if (!guid || !Array.isArray(value_bytes)) {
      console.warn('⚠️ Некорректные данные атрибута:', update);
      return;
    }

    let formattedValue = '—';
    try {
      const buffer = new Uint8Array(value_bytes);
      formattedValue = String(fromZigbeeType(type, buffer));
    } catch (err) {
      formattedValue = 'parse error';
    }

    const currentDevices = devicesRef.current;
    let foundAttr = null, foundCluster = null, foundEp = null, foundDevice = null, isCustomReport = false;

    for (const device of currentDevices) {
      for (const ep of device.endpoints || []) {
        for (const cluster of [...(ep.standard_clusters || []), ...(ep.custom_clusters || [])]) {
          foundAttr = cluster.attributes?.find(a => a.guid === guid) ||
                      cluster.custom_reports?.find(r => r.guid === guid);
          if (foundAttr) {
            foundCluster = cluster;
            foundEp = ep;
            foundDevice = device;
            isCustomReport = !!cluster.custom_reports?.find(r => r.guid === guid);
            break;
          }
        }
        if (foundAttr) break;
      }
      if (foundAttr) break;
    }

    if (!foundAttr) {
      console.warn(`❌ Атрибут с GUID ${guid} не найден в устройствах`);
      return;
    }

    console.group(`⚡ Обновление атрибута: ${foundDevice.name || foundDevice.ieee_addr}`);
    console.log('🔹 Устройство:', foundDevice.name || foundDevice.ieee_addr);
    console.log('🔹 Endpoint:', foundEp.id);
    console.log('🔹 Кластер:', `0x${foundCluster.id.toString(16).padStart(4, '0')}`);
    console.log('🔹 Атрибут:', foundAttr.name || `0x${foundAttr.id.toString(16).padStart(4, '0')}`);
    console.log('🔹 GUID:', guid);
    console.log('🔹 Тип:', type);
    console.log('🔹 Raw value (bytes):', value_bytes);
    console.log('🔹 Форматированное значение:', formattedValue);
    console.groupEnd();

    setDevices(prev => {
      const updated = JSON.parse(JSON.stringify(prev));
      updated.forEach(device => {
        device.endpoints?.forEach(ep => {
          [...(ep.standard_clusters || []), ...(ep.custom_clusters || [])].forEach(cluster => {
            [...(cluster.attributes || []), ...(cluster.custom_reports || [])].forEach(item => {
              if (item.guid === guid) {
                item.value_bytes = value_bytes;
                try {
                  const buffer = new Uint8Array(value_bytes);
                  item.value = String(fromZigbeeType(type, buffer));
                } catch (err) {
                  item.value = `<parse error>`;
                }
              }
            });
          });
        });
      });
      devicesCache = updated; // ⬅️ Обновляем кэш
      return updated;
    });

    if (onAttributeUpdateRef.current) {
      onAttributeUpdateRef.current({
        guid,
        value: formattedValue,
        rawValue: value_bytes,
        type,
        device: foundDevice,
        endpoint: foundEp,
        cluster: foundCluster,
        attribute: foundAttr,
        short: foundDevice.short_addr.replace('0x', '').toUpperCase(),
        ep: foundEp.id,
        clusterId: `0x${foundCluster.id.toString(16).padStart(4, '0')}`,
        attrId: `0x${foundAttr.id.toString(16).padStart(4, '0')}`,
        isCustomReport
      });
    }
  }, []);

  const handleSystemNotify = useCallback((data) => {
    const { type, message } = data;
    let emoji = 'ℹ️';
    if (type === 'zigbee_permit_join_started') emoji = '🔓';
    if (type === 'zigbee_permit_join_stopped') emoji = '🔒';

    console.group(`🔔 Системное уведомление: ${emoji} ${type}`);
    console.log('Полные данные:', data);
    console.groupEnd();

    if (type === 'var_updated') {
      window.dispatchEvent(new CustomEvent('variables_changed'));
    }

    if (type === 'rule_updated' || type === 'rule_deleted') {
      window.dispatchEvent(new CustomEvent('rules_changed'));
    }

    if (onSystemNotifyRef.current) {
      onSystemNotifyRef.current({ type, message, emoji, data });
    }

    window.dispatchEvent(new CustomEvent('system_notify', { detail: { type, message, emoji, data } }));

    if (type === 'device_updated') {
      const { short_addr } = data.data || {};
      if (short_addr) {
        console.log(`🔧 Обновление устройства: short_addr=${short_addr}`);
        refreshDeviceByShort(short_addr);
      }
    }

    if (type === 'device_renamed') {
      const { ieee_addr, friendly_name } = data.data || {};
      if (ieee_addr) {
        console.log(`✏️ Переименование устройства: ${friendly_name} (${ieee_addr})`);
        setDevices(prev => {
          const updated = prev.map(d =>
            d.ieee_addr === ieee_addr
              ? { ...d, name: friendly_name, friendly_name }
              : d
          );
          devicesCache = updated; // ⬅️ Обновляем кэш
          return updated;
        });
      }
    }
  }, []);

  // Основной эффект: загрузка устройств
  useEffect(() => {
    if (!enabled) return;
    if (hasLoadedDevices) {
      console.log('🟡 [useDevices] Загрузка пропущена — устройства уже загружены');
      return;
    }

    const fetchWithRetry = async (addr, maxRetries = 2, delayMs = 300) => {
      for (let i = 0; i <= maxRetries; i++) {
        try {
          return await api.getDeviceByShort(addr);
        } catch (err) {
          if (i === maxRetries) throw err;
          console.warn(`🔁 Повтор запроса к ${addr} (попытка ${i + 1})`);
          await new Promise(resolve => setTimeout(resolve, delayMs));
        }
      }
    };

    const loadDevices = async () => {
      try {
        setLoading(true);
        const briefList = await api.getDevices();

        const fullDevices = [];

        for (const dev of briefList) {
          const addrNum = typeof dev.short === 'string' ? parseInt(dev.short, 16) : dev.short;
          const addrHex = addrNum.toString(16).toUpperCase().padStart(4, '0');
          const addr = `0x${addrHex}`;

          try {
            const fullDevice = await fetchWithRetry(addr);
            console.log(`✅ Успешно загружено устройство:`, fullDevice.name || addr);
            fullDevices.push(fullDevice);
          } catch (err) {
            console.warn(`❌ Не удалось загрузить устройство после повторов: ${addr}`, err);
          }

          await new Promise(resolve => setTimeout(resolve, 300));
        }

        setDevices(fullDevices);
        devicesCache = fullDevices;
        hasLoadedDevices = true;
      } catch (err) {
        setError(err.message);
      } finally {
        setLoading(false);
      }
    };

    const handleWebSocketMessage = (data) => {
      if (data.event === 'attribute_updated') {
        updateDeviceAttribute(data);
      } else if (data.event === 'system_notify') {
        handleSystemNotify(data);
      }
    };

    loadDevices();
    const unsubscribe = subscribeToWebSocket(handleWebSocketMessage);

    return () => {
      unsubscribe();
    };
  }, [updateDeviceAttribute, handleSystemNotify, enabled]);

  return { devices, loading, error };
};