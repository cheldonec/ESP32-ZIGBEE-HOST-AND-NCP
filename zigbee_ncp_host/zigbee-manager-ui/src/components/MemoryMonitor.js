// src/components/MemoryMonitor.js
import React, { useState, useEffect } from 'react';
import './MemoryMonitor.css';

const MemoryMonitor = () => {
  const [memory, setMemory] = useState(null);
  const [error, setError] = useState(false);

  useEffect(() => {
    const fetchMemory = () => {
      fetch('/api/memory')
        .then((res) => {
          if (!res.ok) throw new Error('Network error');
          return res.json();
        })
        .then((data) => {
          setMemory(data);
          setError(false);
        })
        .catch((err) => {
          console.warn('❌ Ошибка загрузки памяти:', err.message);
          setError(true);
        });
    };

    // Первое обновление
    fetchMemory();

    // Обновляем каждые 5 сек
    const interval = setInterval(fetchMemory, 5000);

    return () => clearInterval(interval);
  }, []);

  if (!memory && !error) return null;

  const freeKb = memory ? (memory.free / 1024).toFixed(1) : '?';
  const minKb = memory ? (memory.min / 1024).toFixed(1) : '?';
  const largestKb = memory ? (memory.largest / 1024).toFixed(1) : '?';
  const fragPct = memory?.frag_pct ?? '?';

  const isLowMemory = freeKb < 30;
  const isHighFrag = fragPct > 70;

  return (
    <div className={`memory-monitor ${isLowMemory || isHighFrag ? 'warning' : ''}`}>
      <div className="memory-content">
        <span title="Свободно">
          📊 <strong>{freeKb} КБ</strong> | 
        </span>
        <span title="Минимум свободно">
          Min: {minKb} КБ | 
        </span>
        <span title="Наибольший блок">
          Largest: {largestKb} КБ | 
        </span>
        <span title="Фрагментация">
          Фрагментация: {fragPct}%
        </span>
        {error && <span className="error"> ❌ ошибка</span>}
      </div>
    </div>
  );
};

export default MemoryMonitor;