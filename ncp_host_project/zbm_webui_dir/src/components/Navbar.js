// src/components/Navbar.js
import { useState, useEffect } from 'react';
import { useNotification } from '../context/NotificationContext';
import { useNetworkStatus } from '../hooks/useNetworkStatus';

const tabs = [
  { name: 'Устройства', path: '/', icon: '🔌' },
  { name: 'Связи', path: '/links', icon: '🔗' },
  { name: 'Сценарии', path: '/scenes', icon: '🎬' },
  { name: 'Настройки', path: '/settings', icon: '⚙️' },
  { name: 'Мониторинг', path: '/monitor', icon: '📊' },
];

export default function Navbar() {
  const { addToast } = useNotification();
  const { isJoining } = useNetworkStatus();
  const [duration, setDuration] = useState(60);
  const [currentPath, setCurrentPath] = useState(window.location.hash || '#/');

  useEffect(() => {
    const handler = () => setCurrentPath(window.location.hash || '#/');
    window.addEventListener('hashchange', handler);
    return () => window.removeEventListener('hashchange', handler);
  }, []);

  const isActive = (path) => currentPath === `#${path}`;

  const navigate = (path) => {
    window.location.hash = path;
  };

  const togglePermitJoin = async () => {
    try {
      const newJoiningState = !isJoining;

      console.log(`📡 Sending toggle_permit_join: ${newJoiningState ? 'OPEN' : 'CLOSE'}`);

      const res = await fetch('/api/post/zbnetwork/open_close', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          cmd: 'toggle_permit_join',
          duration: newJoiningState ? duration : 0
        })
      });

      if (!res.ok) {
        const text = await res.text();
        throw new Error(`HTTP ${res.status}: ${text}`);
      }

      console.log('✅ Command sent successfully. Waiting for state update via WebSocket...');
    } catch (err) {
      console.error('❌ Error in togglePermitJoin:', err);
      addToast(`❌ Ошибка: ${err.message}`, 5000);
    }
  };

  // Пока статус сети неизвестен
  if (isJoining === null) {
    return (
      <nav className="navbar">
        <div className="nav-link disabled text-gray-500">🌀 Загрузка...</div>
      </nav>
    );
  }

  return (
    <nav className="navbar">
      {/* Вкладки слева */}
      <div className="flex items-center">
        {tabs.map((tab) => (
          <a
            key={tab.path}
            href={`#${tab.path}`}
            className={`nav-link ${isActive(tab.path) ? 'active' : ''}`}
            onClick={(e) => {
              e.preventDefault();
              navigate(tab.path);
            }}
          >
            {tab.icon} {tab.name}
          </a>
        ))}
      </div>

      {/* Кнопка + поле ввода — справа */}
      <div className="navbar-controls">
        {/* Поле ввода только когда сеть закрыта */}
        {!isJoining && (
          <input
            type="number"
            min="1"
            max="254"
            value={duration}
            onChange={(e) => setDuration(Number(e.target.value))}
            placeholder="60"
            title="Длительность открытия сети (секунды)"
          />
        )}

        <button
          onClick={togglePermitJoin}
          className={`join-button ${isJoining ? 'joining' : ''}`}
          title={isJoining ? 'Закрыть сеть zigbee' : 'Открыть сеть zigbee для подключения'}
        >
          {isJoining ? 'Закрыть zigbee' : 'Открыть zigbee'}
        </button>
      </div>
    </nav>
  );
}