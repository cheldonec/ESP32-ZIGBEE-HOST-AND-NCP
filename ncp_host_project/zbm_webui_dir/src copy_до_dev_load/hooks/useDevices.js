// src/hooks/useDevices.js
import { useState, useEffect } from 'react';

// Утилита: конвертация short_addr в hex строку
const toHexAddr = (short) => short.toString(16).toUpperCase().padStart(4, '0');

export const useDevices = () => {
  const [devices, setDevices] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  useEffect(() => {
    const loadDevices = async () => {
      try {
        // Шаг 1: Получаем краткий список
        const res = await fetch('/api/devices');
        if (!res.ok) throw new Error('Failed to fetch device list');
        const briefList = await res.json();

        // Шаг 2: Параллельно загружаем полные данные
        const fullDevicesPromises = briefList.map(async (dev) => {
          const addrHex = toHexAddr(dev.short);
          const detailRes = await fetch(`/api/device/by_short?addr=0x${addrHex}`);
          if (!detailRes.ok) {
            console.warn(`Failed to load details for ${addrHex}`);
            return null;
          }
          const fullData = await detailRes.json();
          return fullData;
        });

        const fullDevices = await Promise.allSettled(fullDevicesPromises);
        const validDevices = fullDevices
          .filter(p => p.status === 'fulfilled' && p.value !== null)
          .map(p => p.value);

        setDevices(validDevices);
      } catch (err) {
        console.error('Error loading devices:', err);
        setError(err.message);
      } finally {
        setLoading(false);
      }
    };

    loadDevices();
  }, []);

  return { devices, loading, error };
};