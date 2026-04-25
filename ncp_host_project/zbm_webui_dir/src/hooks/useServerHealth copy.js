// src/hooks/useServerHealth.js
import { useEffect } from 'react';

export const useServerHealth = () => {
  useEffect(() => {
    let interval = null;

    const checkServer = async () => {
      try {
        const res = await fetch('/api/get_server_status?t=' + Date.now(), {
          method: 'GET',
          cache: 'no-cache'
        });

        if (res.ok) {
          const data = await res.json();
          const savedToken = localStorage.getItem('server_session_token');
          const currentToken = data.session_token;

          // 🔁 Если токены есть и отличаются — это перезагрузка ESP
          if (savedToken && currentToken && savedToken !== currentToken) {
            console.log('🔄 Server restarted. New token:', currentToken);
            console.log('Old token:', savedToken);

            // ✅ Сохраняем НОВЫЙ токен перед перезагрузкой
            localStorage.setItem('server_session_token', currentToken);

            // ✅ Перезагружаем UI
            console.log('🔁 Reloading UI...');
            window.location.reload();
          }

          // 🆕 Если токена ещё не было — сохраняем (первый запуск)
          else if (!savedToken && currentToken) {
            console.log('✅ First run. Saving session token:', currentToken);
            localStorage.setItem('server_session_token', currentToken);
          }

          // ❌ Если есть saved, но нет current → сервер сломался? пропускаем
        }
      } catch (err) {
        // Сервер не отвечает — ничего не делаем
        console.debug('[Health] Fetch failed:', err.message);
      }
    };

    // Проверяем каждые  секунды
    interval = setInterval(checkServer, 10000);

    return () => {
      if (interval) clearInterval(interval);
    };
  }, []);

  return null;
};