// src/hooks/useDevices.js

import { useState, useEffect, useRef } from 'react';
// ✅ Используем fromZigbeeType из zigbeeTypes.js (единая точка)
import { fromZigbeeType } from '../utils/zigbeeTypes';

const toHexAddr = (short) => short.toString(16).toUpperCase().padStart(4, '0');

export const useDevices = ({ onAttributeUpdate, onSystemNotify } = {}) => {
  const [devices, setDevices] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [wsRetry, setWsRetry] = useState(0);

  // ✅ Сохраняем последнюю версию колбэка
  const onAttributeUpdateRef = useRef();
  const onSystemNotifyRef = useRef();
  useEffect(() => {
    onAttributeUpdateRef.current = onAttributeUpdate;
  }, [onAttributeUpdate]);

  useEffect(() => {
    onSystemNotifyRef.current = onSystemNotify;
  }, [onSystemNotify]);

  // ✅ Реф для актуального списка устройств
  const devicesRef = useRef(devices);
  useEffect(() => {
    devicesRef.current = devices;
  }, [devices]);

  useEffect(() => {
    let ws = null;

    console.log('🔁 useDevices: effect started, wsRetry =', wsRetry);

    const loadDevices = async () => {
      try {
        const res = await fetch('/api/devices');
        if (!res.ok) throw new Error('Failed to fetch device list');
        const briefList = await res.json();
        console.log('✅ [useDevices] Got brief device list:', briefList);

        const fullDevicesPromises = briefList.map(async (dev) => {
          const addrNum = typeof dev.short === 'string' ? parseInt(dev.short, 16) : dev.short;
          const addrHex = addrNum.toString(16).toUpperCase().padStart(4, '0');
          const detailRes = await fetch(`/api/device/by_short?addr=0x${addrHex}`);

          if (!detailRes.ok) {
            const text = await detailRes.text();
            console.warn(`❌ Failed to load device 0x${addrHex}:`, detailRes.status, text);
            return null;
          }

          const data = await detailRes.json();
          console.log(`✅ Loaded device 0x${addrHex}:`, data.ieee_addr);
          return data;
        });

        const fullDevices = await Promise.allSettled(fullDevicesPromises);
        const validDevices = fullDevices
          .filter(p => p.status === 'fulfilled' && p.value !== null)
          .map(p => p.value);

        console.log('✅ [useDevices] Final devices loaded:', validDevices.length, validDevices);
        setDevices(validDevices);
      } catch (err) {
        console.error('💥 Failed to load devices:', err);
        setError(err.message);
      } finally {
        setLoading(false);
      }
    };

    loadDevices();
    // обработка сигнала по обновлению устройства
    const refreshDeviceByShort = async (shortAddr) => {
    try {
      const res = await fetch(`/api/device/by_short?addr=${shortAddr}`);
      if (!res.ok) {
        console.warn(`❌ Failed to refresh device ${shortAddr}:`, res.status);
        return;
      }

      const updatedDevice = await res.json();
      console.log('✅ Device refreshed:', updatedDevice);

      setDevices(prev => {
        return prev.map(device =>
          device.short_addr === shortAddr ? updatedDevice : device
        );
      });
    } catch (err) {
      console.error('💥 Failed to refresh device:', err);
    }
  };


    //

    ws = new WebSocket(`ws://${window.location.host}/ws`);
    console.log('🚀 Trying to connect WebSocket:', `ws://${window.location.host}/ws`);

    ws.onopen = () => {
      console.log('✅ WebSocket: connected');
      setWsRetry(0);
      window.ws = ws;
    };

    ws.onmessage = (e) => {
      console.log('📩 WS received:', e.data);
      try {
        const data = JSON.parse(e.data);
        if (data.event === 'attribute_updated') {
          updateDeviceAttribute(data);
        }

      // ✅ Обработка обновления структуры устройства
      else if (data.event === 'system_notify' && data.type === 'device_updated') {
        const { short_addr } = data.data || {};
        if (short_addr) {
          console.log('🔁 Device structure changed, refreshing:', short_addr);
          refreshDeviceByShort(short_addr);
        }
      }

      // ✅ Обработка переименования устройства
    else if (data.event === 'system_notify' && data.type === 'device_renamed') {
      const { ieee_addr, friendly_name } = data.data || {};
      if (!ieee_addr) return;

      console.log('🔄 Device renamed:', ieee_addr, '→', friendly_name);

      // Обновляем только имя в состоянии
      setDevices(prev =>
        prev.map(device =>
          device.ieee_addr === ieee_addr
            ? { ...device, name: friendly_name, friendly_name } // поддержка обоих полей
            : device
        )
      );

      // Необязательно: триггер onSystemNotify
      if (onSystemNotifyRef.current) {
        onSystemNotifyRef.current(data);
      }
    }

        // ✅ Обработка системных уведомлений
      else if (data.event === 'system_notify') {
        const { type, message } = data;

        let emoji = 'ℹ️';
        if (type === 'zigbee_permit_join_started') emoji = '🔓';
        if (type === 'zigbee_permit_join_stopped') emoji = '🔒';

        // Вызываем внешний колбэк
        if (onSystemNotifyRef.current) {
          onSystemNotifyRef.current({
            type,
            message,
            emoji,
            data
          });
        }
        // ✅ Дополнительно: триггерим DOM-событие для других подписчиков
        const event = new CustomEvent('system_notify', {
          detail: { type, message, emoji, data }
        });
        window.dispatchEvent(event);
      }

      } catch (err) {
        console.error('❌ WS parse error:', e.data);
      }
    };

    ws.onerror = (err) => {
      console.error('⚠️ WebSocket error:', err);
      window.ws = null;
    };

    ws.onclose = () => {
      console.log('🔁 WebSocket closed, reconnecting...');
      window.ws = null;
      setTimeout(() => setWsRetry(r => r + 1), 3000);
    };

    // 🔁 Обновление атрибута (видит актуальные устройства через ref)
    const updateDeviceAttribute = (update) => {
      const { guid, type, value_bytes } = update;

      if (!guid || !Array.isArray(value_bytes)) {
        console.warn('Invalid update data:', update);
        return;
      }

      let formattedValue = '—';
      try {
        const buffer = new Uint8Array(value_bytes);
        formattedValue = String(fromZigbeeType(type, buffer));
      } catch (err) {
        formattedValue = 'parse error';
        console.error('Failed to parse value:', err);
      }

      // ✅ Используем актуальный список устройств
      const currentDevices = devicesRef.current;

      console.log('🔍 Looking for GUID:', guid);
      console.log('💾 Current devices count:', currentDevices.length);

      if (currentDevices.length === 0) {
        console.warn('🕒 Devices not loaded yet, but event received. Buffering not implemented.');
        return;
      }

      let foundAttr = null;
      let foundCluster = null;
      let foundEp = null;
      let foundDevice = null;
      let isCustomReport = false;

      for (const device of currentDevices) {
        for (const ep of device.endpoints || []) {
          for (const cluster of [...(ep.standard_clusters || []), ...(ep.custom_clusters || [])]) {
            // Поиск в attributes
            foundAttr = cluster.attributes?.find(a => a.guid === guid);
            if (foundAttr) {
              foundCluster = cluster;
              foundEp = ep;
              foundDevice = device;
              isCustomReport = false;
              break;
            }

            // Поиск в custom_reports
            foundAttr = cluster.custom_reports?.find(r => r.guid === guid);
            if (foundAttr) {
              foundCluster = cluster;
              foundEp = ep;
              foundDevice = device;
              isCustomReport = true;
              break;
            }
          }
          if (foundAttr) break;
        }
        if (foundAttr) break;
      }

      if (!foundAttr || !foundCluster || !foundEp || !foundDevice) {
        console.warn('❌ Attribute not found by GUID:', guid);
        return;
      }

      // Удобные строки
      const short = foundDevice.short_addr.replace('0x', '').toUpperCase();
      const clusterId = `0x${foundCluster.id.toString(16).padStart(4, '0')}`;
      const attrId = `0x${foundAttr.id.toString(16).padStart(4, '0')}`;

      // 🔁 Обновляем состояние
      setDevices(prev => {
        const updated = JSON.parse(JSON.stringify(prev));
        let found = false;

        updated.forEach(device => {
          device.endpoints?.forEach(ep => {
            [...(ep.standard_clusters || []), ...(ep.custom_clusters || [])].forEach(cluster => {
              (cluster.attributes || []).forEach(attr => {
                if (attr.guid === guid) {
                  attr.value_bytes = value_bytes;
                  try {
                    const buffer = new Uint8Array(value_bytes);
                    attr.value = String(fromZigbeeType(type, buffer));
                  } catch (err) {
                    attr.value = `<parse error: ${err.message}>`;
                  }
                  found = true;
                }
              });

              (cluster.custom_reports || []).forEach(report => {
                if (report.guid === guid) {
                  report.value_bytes = value_bytes;
                  try {
                    const buffer = new Uint8Array(value_bytes);
                    report.value = String(fromZigbeeType(type, buffer));
                  } catch (err) {
                    report.value = `<parse error: ${err.message}>`;
                  }
                  found = true;
                }
              });
            });
          });
        });

        return found ? updated : prev;
      });

      // ✅ Вызываем внешний колбэк (например, для тостов)
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
          short,
          ep: foundEp.id,
          clusterId,
          attrId,
          isCustomReport
        });
      }
    };

    return () => {
      if (ws) ws.close();
    };
  }, [wsRetry]); // Не добавляй devices или onAttributeUpdate сюда!

  return { devices, loading, error };
};