// src/hooks/useServerStatus.js
import { useState, useEffect } from 'react';
import { api } from '../api/httpClient';

export const useServerStatus = () => {
  const [isReady, setIsReady] = useState(false);
  const [error, setError] = useState(null);
  const [attempt, setAttempt] = useState(0);
  const [memory, setMemory] = useState(null);

  useEffect(() => {
    let isActive = true;
    let timeoutId = null;
    let intervalId = null; // ← для регулярных опросов

    const checkStatus = async () => {
      if (!isActive) return;

      try {
        const data = await api.getServerStatus();

        const savedToken = localStorage.getItem('server_session_token');
        const currentToken = data.session_token;

        if (!savedToken) {
          localStorage.setItem('server_session_token', currentToken);
        }

        if (data.memory) {
          setMemory(data.memory);
        }

        if (isActive) {
          setIsReady(true);
          setError(null);
          setAttempt(0); // сбрасываем счётчик

          // 🔁 После успешного подключения — запускаем опрос каждые 10 сек
          if (!intervalId) {
            intervalId = setInterval(() => {
              console.debug('🔁 Регулярный опрос /get_server_status');
              checkStatus();
            }, 10000);
          }
        }
      } catch (err) {
        if (!isActive) return;

        console.debug(`🔁 Попытка ${attempt + 1}: сервер не готов`);
        setError(err.message);
        setAttempt(prev => prev + 1);

        const delay = Math.min(1000 + attempt * 1000, 5000);
        timeoutId = setTimeout(checkStatus, delay);
      }
    };

    checkStatus();

    return () => {
      isActive = false;
      if (timeoutId) clearTimeout(timeoutId);
      if (intervalId) clearInterval(intervalId); // очистка
    };
  }, []);

  return { isReady, error, attempt, memory };
};