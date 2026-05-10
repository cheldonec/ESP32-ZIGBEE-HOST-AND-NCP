// src/hooks/useServerStatus.js
import { useState, useEffect } from 'react';
import { api } from '../api/httpClient';

export const useServerStatus = () => {
  const [isReady, setIsReady] = useState(false);
  const [error, setError] = useState(null);
  const [attempt, setAttempt] = useState(0);

  useEffect(() => {
    let isActive = true;
    let timeoutId;

    const checkStatus = async () => {
      if (!isActive) return;

      try {
        const data = await api.getServerStatus();
        const savedToken = localStorage.getItem('server_session_token');
        const currentToken = data.session_token;

        if (!savedToken) {
          localStorage.setItem('server_session_token', currentToken);
        }

        if (isActive) {
          console.log('✅ Сервер готов, токен совпадает');
          setIsReady(true);
        }
      } catch (err) {
        if (!isActive) return;

        console.debug(`🔁 Попытка ${attempt + 1}: сервер не готов — пробуем снова...`);
        setError(err.message);
        setAttempt(prev => prev + 1);

        // Экспоненциальная задержка: 1s → 2s → 3s → 4s...
        const delay = Math.min(1000 + attempt * 1000, 5000);
        timeoutId = setTimeout(checkStatus, delay);
      }
    };

    checkStatus();

    return () => {
      isActive = false;
      if (timeoutId) clearTimeout(timeoutId);
    };
  }, [attempt]);

  return { isReady, error, attempt };
};