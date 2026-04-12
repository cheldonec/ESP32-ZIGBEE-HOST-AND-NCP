// src/App.js
import { useState, useEffect } from 'react';
import './App.css';

// Компоненты
import DeviceSidebar from './components/DeviceSidebar';
import DeviceDetails from './components/DeviceDetails';
import Settings from './components/Settings';
import NotificationProvider from './components/NotificationProvider';
// ✅ Импортируем useDevices с WebSocket
import { useDevices } from './hooks/useDevices';

// Хук для координатора
const useCoordinator = () => {
  const [coordinator, setCoordinator] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const load = async () => {
      try {
        const res = await fetch('/api/get/coordinator');
        if (res.ok) {
          const data = await res.json();
          setCoordinator(data);
        }
      } catch (err) {
        console.error('Failed to load coordinator:', err);
      } finally {
        setLoading(false);
      }
    };
    load();
  }, []);

  return { coordinator, loading };
};



function App() {
  const [currentPath, setCurrentPath] = useState(window.location.hash || '#/');
  const { coordinator } = useCoordinator();

  // ✅ Сначала получаем устройства
  const { devices: fullDevices } = useDevices({
  onAttributeUpdate: ({ short, ep, clusterId, attrId, attributeName, value }) => {
  // Время
  const now = new Date();
  const timeStr = now.toLocaleTimeString('ru-RU', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit'
  });

  // Иконка
  const icon = {
    OnOff: value === '1' || value === 'true' ? '💡' : '⚫️',
    Temperature: '🌡️',
    Humidity: '💧',
    Voltage: '🔋',
    Pressure: '📊',
    Battery: '⚡',
    LinkQuality: '📶',
    Default: '📡'
  };

  const emoji = icon[attributeName] || icon.Default;

  // Читаемое значение
  let readableValue = value;
  if (attributeName === 'OnOff') {
    readableValue = value === '1' || value === 'true' ? 'включено' : 'выключено';
  } else if (attributeName === 'Temperature') {
    readableValue = `${parseFloat(value).toFixed(1)}°C`;
  } else if (attributeName === 'Humidity') {
    readableValue = `${value}%`;
  } else if (attributeName === 'Voltage') {
    readableValue = `${parseFloat(value).toFixed(2)} В`;
  } else if (attributeName === 'Battery') {
    readableValue = `${value}%`;
  }

  // Формируем подпись с техническими данными
  const technical = `[${short}/${ep}] Cl: ${clusterId}, Attr: ${attrId}`;

  // Полное сообщение
  const message = `${emoji} ${attributeName} → ${readableValue}\n${technical} (${timeStr})`;

  addToast(message);
}
});

  // 🔁 Храним только IEEE выбранного устройства
  const [selectedIEEE, setSelectedIEEE] = useState(null);

  // ✅ Всегда получаем актуальное устройство из свежего списка
  const selectedDevice = fullDevices.find(d => d.ieee_addr === selectedIEEE) || null;

  // Краткий список для сайдбара
  const briefList = fullDevices.map(dev => ({
    ieee: dev.ieee_addr,
    short: parseInt(dev.short_addr.replace('0x', ''), 16),
    friendly_name: dev.name || dev.friendly_name,
    online: dev.is_online,
    linkquality: dev.lqi
  }));

  // Обработчик выбора устройства
  const handleSelectDevice = (briefDev) => {
    setSelectedIEEE(briefDev.ieee);
  };

  // Состояние для тостов
  const [toasts, setToasts] = useState([]);

  // Функция добавления уведомления
  const addToast = (message) => {
    const id = Date.now() + Math.random();
    setToasts(prev => [...prev, { id, message }]);
    setTimeout(() => {
      setToasts(prev => prev.filter(t => t.id !== id));
    }, 5000);
  };

  // Удаление тоста
  const removeToast = (id) => {
    setToasts(prev => prev.filter(t => t.id !== id));
  };

  // Следим за хэшем URL
  useEffect(() => {
    const onHashChange = () => {
      setCurrentPath(window.location.hash || '#/');
    };
    window.addEventListener('hashchange', onHashChange);
    return () => window.removeEventListener('hashchange', onHashChange);
  }, []);

  // Пока нет координатора
  if (!coordinator) {
    return (
      <div className="app-container">
        <header className="header">
          <div className="left">🌀 Загрузка координатора...</div>
        </header>
        <div className="main-layout">
          <div className="device-list">
            <p>Загрузка данных...</p>
          </div>
          <div className="device-details">
            <p>Ожидание подключения к шлюзу...</p>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="app-container">
      {/* Шапка */}
      <header className="header">
        <div className="left">
          <div>
            <span>🌀</span> <strong>Zigbee:</strong> PAN {coordinator.pan_id} | CH {coordinator.radio_channel}
          </div>
          <div>
            <span>📶</span> <strong>AP:</strong> {coordinator.wifi_ap_ssid || 'N/A'}
          </div>
        </div>
        <div className="ieee">{coordinator.ieee_addr}</div>
      </header>

      {/* Навигация */}
      <nav className="navbar">
        <a href="#/" className={`nav-link ${currentPath === '#/' ? 'active' : ''}`}>
          🔌 Устройства
        </a>
        <a href="#/links" className={`nav-link ${currentPath === '#/links' ? 'active' : ''}`}>
          🔗 Связи
        </a>
        <a href="#/scenes" className={`nav-link ${currentPath === '#/scenes' ? 'active' : ''}`}>
          🎬 Сценарии
        </a>
        <a href="#/settings" className={`nav-link ${currentPath === '#/settings' ? 'active' : ''}`}>
          ⚙️ Настройки
        </a>
        <a href="#/monitor" className={`nav-link ${currentPath === '#/monitor' ? 'active' : ''}`}>
          📊 Мониторинг
        </a>
      </nav>

      {/* Основной макет */}
      <div className="main-layout">
        <DeviceSidebar
          devices={briefList}
          selectedIEEE={selectedIEEE}
          onSelect={handleSelectDevice}
        />

        <div className="content-area">
          {currentPath === '#/settings' && <Settings />}
          {currentPath === '#/' && <DeviceDetails key={selectedIEEE} device={selectedDevice} />}
          {currentPath !== '#/' && currentPath !== '#/settings' && (
            <div className="p-8 text-gray-500">
              <h2 className="text-xl font-semibold">🚧 Страница в разработке</h2>
              <p>{currentPath}</p>
            </div>
          )}
        </div>
      </div>

      <footer className="footer">
        <span>🧠 RAM: 48%</span>
        <span>💾 Heap: 28 KB</span>
        <span>🗜️ Frag: 14%</span>
      </footer>

      {/* Всплывающие уведомления */}
      <div className="toast-container">
        {toasts.map(toast => (
          <div key={toast.id} className="toast-item">
            <span className="toast-message">{toast.message}</span>
            <button
              onClick={() => removeToast(toast.id)}
              className="toast-close"
            >
              ✕
            </button>
          </div>
        ))}
      </div>
    </div>
  );
}

// ✅ Оборачиваем App в NotificationProvider
export default function AppWithNotifications() {
  return (
    <NotificationProvider>
      <App />
    </NotificationProvider>
  );
}