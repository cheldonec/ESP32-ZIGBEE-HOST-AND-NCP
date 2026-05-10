import { useState, useEffect } from 'react';

import { api } from '../api/httpClient';
import { subscribeToWebSocket } from '../api/websocket';

export const useNetworkStatus = () => {
  const [isJoining, setIsJoining] = useState(null); // null = загрузка

  // Загрузка начального состояния
  useEffect(() => {
    const fetchStatus = async () => {
      try {
        const data = await api.getZigbeeNetworkStatus(); // ← лучше так, чем fetch напрямую
        console.log('✅ [useNetworkStatus] Initial status loaded:', data);
        setIsJoining(data.is_open);
      } catch (err) {
        console.warn('❌ Failed to fetch network status on init:', err);
        // Попробуем дефолтное значение
        setIsJoining(false);
      }
    };

    fetchStatus();
  }, []);

  // Подписка на WebSocket для обновлений
  useEffect(() => {
    const unsubscribe = subscribeToWebSocket((data) => {
      if (data.event === 'system_notify') {
        const { type } = data;
        console.log('📡 [useNetworkStatus] Received system_notify:', type);

        if (type === 'zigbee_permit_join_started') {
          console.log('🟢 Setting isJoining = true');
          setIsJoining(true);
        } else if (type === 'zigbee_permit_join_stopped') {
          console.log('🔴 Setting isJoining = false');
          setIsJoining(false);
        }
      }
    });

    return () => {
      unsubscribe(); // отписка при unmount
    };
  }, []);

  return { isJoining, setIsJoining };
};