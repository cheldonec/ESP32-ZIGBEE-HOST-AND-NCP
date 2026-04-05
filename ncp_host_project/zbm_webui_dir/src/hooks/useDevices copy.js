// src/hooks/useDevices.js
import { useState, useEffect } from 'react';
import { fromZigbeeType } from '../utils/fromZigbee';

const toHexAddr = (short) => short.toString(16).toUpperCase().padStart(4, '0');

export const useDevices = () => {
  const [devices, setDevices] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [wsRetry, setWsRetry] = useState(0); // для переподключения

  useEffect(() => {
    let ws = null;
    console.log('🔁 useDevices: effect started, wsRetry =', wsRetry);
    const loadDevices = async () => {
      try {
        const res = await fetch('/api/devices');
        if (!res.ok) throw new Error('Failed to fetch device list');
        const briefList = await res.json();

        const fullDevicesPromises = briefList.map(async (dev) => {
          const addrHex = toHexAddr(dev.short);
          const detailRes = await fetch(`/api/device/by_short?addr=0x${addrHex}`);
          if (!detailRes.ok) return null;
          return await detailRes.json();
        });

        const fullDevices = await Promise.allSettled(fullDevicesPromises);
        const validDevices = fullDevices
          .filter(p => p.status === 'fulfilled' && p.value !== null)
          .map(p => p.value);

        setDevices(validDevices);
      } catch (err) {
        setError(err.message);
      } finally {
        setLoading(false);
      }
    };

    loadDevices();

    ws = new WebSocket(`ws://${window.location.host}/ws`);
    console.log('🚀 Trying to connect WebSocket:', `ws://${window.location.host}/ws`);
    ws.onopen = () => {
      console.log('✅ WebSocket: connected');
      setWsRetry(0);
    };

    ws.onmessage = (e) => {
      console.log('📩 WS received:', e.data);
      try {
        const data = JSON.parse(e.data);
        if (data.event === 'attribute_updated') {
          updateDeviceAttribute(data);
        }
      } catch (err) {
        console.error('❌ WS parse error:', e.data);
      }
    };

    ws.onerror = (err) => {
      console.error('⚠️ WebSocket error:', err);
    };

    ws.onclose = () => {
      console.log('🔁 WebSocket closed, reconnecting...');
      setTimeout(() => setWsRetry(r => r + 1), 3000);
    };

    let addToastFn = null;
    
    const updateDeviceAttribute = (update) => {
      const { guid, type, value_bytes } = update;
      console.log('🔄 Update requested:', { guid, type, value_bytes });

      if (!guid || !Array.isArray(value_bytes)) return;

      setDevices(prev => {
        const updated = JSON.parse(JSON.stringify(prev));
        let found = false;

        updated.forEach(device => {
          device.endpoints?.forEach(ep => {
            [...(ep.standard_clusters || []), ...(ep.custom_clusters || [])].forEach(cluster => {
              [...(cluster.attributes || []), ...(cluster.custom_reports || [])].forEach(attr => {
                if (attr.guid === guid) {
                  console.log('🎯 Updated:', attr.name, '→', value_bytes);
                  attr.value_bytes = value_bytes;
                  try {
                    const buffer = new Uint8Array(value_bytes);
                    attr.value = fromZigbeeType(type, buffer);
                  } catch (err) {
                    attr.value = `<parse error: ${err.message}>`;
                  }
                  found = true;
                }
              });
            });
          });
        });

        return found ? updated : prev;
      });
    };

    return () => {
      if (ws) ws.close();
    };
  }, [wsRetry]);

  return { devices, loading, error };
};