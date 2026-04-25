// src/hooks/useRules.js
import { useState, useEffect } from 'react';

export const useRules = () => {
  const [rules, setRules] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  const reload = async () => {
    try {
      const res = await fetch('/api/rules');
      if (!res.ok) throw new Error('Failed to fetch rules list');
      const briefList = await res.json();

      const fullRulesPromises = briefList.map(async (brief) => {
        try {
          const detailRes = await fetch(`/api/rule/${brief.id}`);
          if (!detailRes.ok) return null;
          return await detailRes.json();
        } catch (err) {
          console.warn(`Failed to load rule ${brief.id}:`, err);
          return null;
        }
      });

      const fullRules = (await Promise.allSettled(fullRulesPromises))
        .filter(p => p.status === 'fulfilled' && p.value !== null)
        .map(p => p.value);

      setRules(fullRules);
    } catch (err) {
      console.error('💥 Failed to load rules:', err);
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    // Первая загрузка
    reload();
    
    // Подписка на внешние изменения (например, от useDevices через WebSocket)
    const handleRulesChange = () => {
      console.log('🔁 External event: rules changed — reloading...');
      reload();
    };

    window.addEventListener('rules_changed', handleRulesChange);

    // Очистка — убираем слушатель
    return () => {
      window.removeEventListener('rules_changed', handleRulesChange);
    };
  }, []); // ← пустой массив: запускается один раз при монтировании

  return { rules, loading, error, reload };
};