// src/hooks/useNetworkStatus.js

import { useState, useEffect } from 'react';

export const useNetworkStatus = () => {
  const [isJoining, setIsJoining] = useState(null); // ← null = загрузка

  // Загрузка начального состояния
  useEffect(() => {
    const fetchStatus = async () => {
      try {
        const res = await fetch('/api/get/zigbee_network/status');
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        
        const data = await res.json();
        console.log('✅ [useNetworkStatus] Initial status loaded:', data);
        setIsJoining(data.is_open);
      } catch (err) {
        console.warn('❌ Failed to fetch network status on init:', err);
        // Если не удалось — попробуем через WebSocket
        setIsJoining(false); // дефолт
      }
    };

    fetchStatus();
  }, []);

  // Обработка событий из WebSocket
  useEffect(() => {
    const handleSystemNotify = (e) => {
      const { type } = e.detail;
      console.log('📡 [useNetworkStatus] Received system_notify:', type);

      if (type === 'zigbee_permit_join_started') {
        console.log('🟢 Setting isJoining = true');
        setIsJoining(true);
      } else if (type === 'zigbee_permit_join_stopped') {
        console.log('🔴 Setting isJoining = false');
        setIsJoining(false);
      }
    };

    window.addEventListener('system_notify', handleSystemNotify);
    return () => window.removeEventListener('system_notify', handleSystemNotify);
  }, []);

  return { isJoining, setIsJoining };
};