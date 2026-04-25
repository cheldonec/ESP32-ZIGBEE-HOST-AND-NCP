// src/hooks/useVariables.js
import { useState, useEffect } from 'react';

export const useVariables = () => {
  const [variables, setVariables] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  const reload = async () => {
    try {
      const res = await fetch('/api/get/vars');
      if (!res.ok) throw new Error('Failed to fetch variables');
      const data = await res.json();

      const mapped = data.map(v => ({
        idx: v.idx,
        guid: `var_${v.idx}`,
        name: v.name || `var_${v.idx}`,
        type: v.type,
        value: v.value ?? '',       // текущее значение (runtime)
        init_value: v.init_value ?? ''  // начальное значение (config)
      }));

      setVariables(mapped);
    } catch (err) {
      console.error('💥 Failed to load variables:', err);
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    reload();

    // Подписка на обновления (если где-то будет триггер)
    // Например, после сохранения в Settings
    const handleVarsChange = () => {
      console.log('🔁 Variables changed — reloading...');
      reload();
    };

    window.addEventListener('variables_changed', handleVarsChange);
    return () => {
      window.removeEventListener('variables_changed', handleVarsChange);
    };
  }, []);

  return { variables, loading, error, reload };
};