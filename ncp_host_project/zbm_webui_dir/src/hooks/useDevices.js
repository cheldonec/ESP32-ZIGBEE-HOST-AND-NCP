// src/hooks/useDevices.js

import { useState, useEffect, useRef } from 'react';
import { fromZigbeeType } from '../utils/zigbeeTypes';

export const useDevices = ({ onAttributeUpdate, onSystemNotify } = {}) => {
  const [devices, setDevices] = useState([]);
  const [loading, setLoading] = useState(true);
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

  // --- 1. Загрузка устройств ---
  const loadDevices = async () => {
    try {
      console.log('🔍 Начало загрузки устройств...');
      const res = await fetch('/api/devices');
      if (!res.ok) throw new Error('Failed to fetch device list');
      const briefList = await res.json();
      console.log('📋 Получен краткий список устройств:', briefList);

      const fullDevicesPromises = briefList.map(async (dev) => {
        const addrNum = typeof dev.short === 'string' ? parseInt(dev.short, 16) : dev.short;
        const addrHex = addrNum.toString(16).toUpperCase().padStart(4, '0');
        const detailRes = await fetch(`/api/device/by_short?addr=0x${addrHex}`);
        if (!detailRes.ok) {
          console.warn(`⚠️ Не удалось загрузить детали для устройства 0x${addrHex}`);
          return null;
        }
        const fullDevice = await detailRes.json();
        console.log(`✅ Загружено устройство: ${fullDevice.name || fullDevice.ieee_addr}`, fullDevice);
        return fullDevice;
      });

      const fullDevices = await Promise.allSettled(fullDevicesPromises);
      const validDevices = fullDevices
        .filter(p => p.status === 'fulfilled' && p.value !== null)
        .map(p => p.value);

      console.log('🎉 Все устройства загружены:', validDevices);
      setDevices(validDevices);
    } catch (err) {
      console.error('💥 Failed to load devices:', err);
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  // --- 2. WebSocket ---
  useEffect(() => {
    let ws = null;
    let isReconnecting = false;

    const connectWebSocket = () => {
      if (isReconnecting) return;
      isReconnecting = true;

      console.log('🚀 Подключение WebSocket...');
      ws = new WebSocket(`ws://${window.location.host}/ws`);

      ws.onopen = () => {
        console.log('✅ WebSocket соединение установлено');
        isReconnecting = false;
        window.ws = ws;
      };

      ws.onmessage = (e) => {
        try {
          const data = JSON.parse(e.data);
          console.log('📩 Получено сообщение через WebSocket:', data); // ← ключевой вывод

          if (data.event === 'attribute_updated') {
            console.log('⚡ АТРИБУТ ОБНОВЛЁН:', data); // ← очень важно!
            updateDeviceAttribute(data);
          } else if (data.event === 'system_notify') {
            console.log('🔔 СИСТЕМНОЕ УВЕДОМЛЕНИЕ:', data);
            handleSystemNotify(data);
          } else {
            console.log('❓ Неизвестное событие:', data.event);
          }
        } catch (err) {
          console.error('❌ Ошибка парсинга WebSocket:', e.data, err);
        }
      };

      ws.onerror = (err) => {
        console.error('⚠️ Ошибка WebSocket:', err);
      };

      ws.onclose = () => {
        console.log('🔁 WebSocket закрыт, переподключение через 3 сек...');
        window.ws = null;
        setTimeout(connectWebSocket, 3000);
      };
    };

    // Запуск
    loadDevices();        // Загрузка один раз
    connectWebSocket();   // Подключение WS

    // Очистка
    return () => {
      if (ws) ws.close();
    };
  }, []); // ← один раз при монтировании

  // --- Обработчики ---
  const refreshDeviceByShort = async (shortAddr) => {
    try {
      console.log(`🔄 Обновление устройства по short_addr: ${shortAddr}`);
      const res = await fetch(`/api/device/by_short?addr=${shortAddr}`);
      if (!res.ok) {
        console.warn(`❌ Не удалось обновить устройство: ${shortAddr}`);
        return;
      }
      const updatedDevice = await res.json();
      console.log(`✅ Устройство обновлено:`, updatedDevice);
      setDevices(prev => prev.map(d => d.short_addr === shortAddr ? updatedDevice : d));
    } catch (err) {
      console.error('💥 Ошибка при обновлении устройства:', err);
    }
  };

  const updateDeviceAttribute = (update) => {
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

    // 🔍 Выводим подробности обновления атрибута
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
  };

  const handleSystemNotify = (data) => {
    const { type, message } = data;
    let emoji = 'ℹ️';
    if (type === 'zigbee_permit_join_started') emoji = '🔓';
    if (type === 'zigbee_permit_join_stopped') emoji = '🔒';

    console.group(`🔔 Системное уведомление: ${emoji} ${type}`);
    console.log('Полные данные:', data);
    console.groupEnd();

    if (type === 'var_updated') {
      console.log('🔔 Передача события: variables_changed');
      window.dispatchEvent(new CustomEvent('variables_changed'));
    }

    if (type === 'rule_updated' || type === 'rule_deleted') {
      console.log('🔔 Передача события: rules_changed');
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
        setDevices(prev => prev.map(d =>
          d.ieee_addr === ieee_addr
            ? { ...d, name: friendly_name, friendly_name }
            : d
        ));
      }
    }
  };

  return { devices, loading, error };
};