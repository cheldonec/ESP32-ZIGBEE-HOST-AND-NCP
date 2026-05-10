import { useState, useEffect, useRef } from 'react';

import { api } from '../api/httpClient';
import { subscribeToWebSocket } from '../api/websocket';

export const useCoordinator = ({ enabled = true } = {}) => {
  const [coordinator, setCoordinator] = useState(null);
  const [variables, setVariables] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  const variablesRef = useRef([]);

  const reloadVariables = async () => {
    try {
      const data = await api.getVariables();
      const mapped = data.map(v => ({
        idx: v.idx,
        guid: `var_${v.idx}`,
        name: v.name || `var_${v.idx}`,
        type: v.type,
        value: v.value ?? '',
        init_value: v.init_value ?? ''
      }));
      setVariables(mapped);
      variablesRef.current = mapped;
    } catch (err) {
      console.error('💥 Failed to load variables:', err);
    }
  };

  const loadCoordinator = async () => {
    try {
      const data = await api.getCoordinator();
      setCoordinator(data);
    } catch (err) {
      console.error('❌ Failed to load coordinator:', err);
      setError(err.message);
    }
  };

  useEffect(() => {
    if (!enabled) return;

    const init = async () => {
      const unsubscribe = subscribeToWebSocket(async (data) => {
        if (data.event === 'system_notify') {
          const { type } = data;

          if (type === 'var_updated') {
            console.log('🔁 Переменные обновлены → перезагружаем...');
            await reloadVariables();
          }

          if (type === 'device_renamed' && coordinator?.ieee_addr === data.data?.ieee_addr) {
            setCoordinator(prev => ({
              ...prev,
              friendly_name: data.data.friendly_name
            }));
          }
        }
      });

      // Ждём WebSocket
      const checkWs = setInterval(() => {
        if (window.ws?.readyState === WebSocket.OPEN) {
          clearInterval(checkWs);
          Promise.all([
            loadCoordinator(),
            reloadVariables()
          ]).finally(() => {
            setLoading(false);
          });
        }
      }, 100);

      return () => {
        clearInterval(checkWs);
        unsubscribe();
      };
    };

    init();
  }, [enabled]);

  return { coordinator, variables, loading, error, reloadVariables };
};