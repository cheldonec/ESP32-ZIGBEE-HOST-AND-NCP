// src/App.js
import { useState, useEffect } from 'react';
import './App.css';

// Компоненты
import DeviceSidebar from './components/DeviceSidebar';
import DeviceDetails from './components/DeviceDetails';
import Settings from './components/Settings';
import NotificationProvider from './components/NotificationProvider';
import { useNotification } from './context/NotificationContext';
import { useServerHealth } from './hooks/useServerHealth';
import Navbar from './components/Navbar'; 
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



// src/App.js

function App() {
  const [currentPath, setCurrentPath] = useState(window.location.hash || '#/');
  const { coordinator } = useCoordinator();
  const { addToast } = useNotification(); // ✅ Глобальный addToast

  // ✅ Список устройств с WebSocket
  const { devices: fullDevices } = useDevices({
    onAttributeUpdate: ({ attribute, value, isCustomReport, short, ep, clusterId, attrId }) => {
      const { name } = attribute;

      const iconMap = {
        OnOff: value === '1' || value === 'true' ? '💡' : '⚫️',
        Voltage: '🔋',
        Battery: '⚡',
        Temperature: '🌡️',
        Humidity: '💧',
        Pressure: '📊',
        LinkQuality: '📶',
        Default: '📡'
      };

      const emoji = iconMap[name] || iconMap.Default;

      let readableValue = value;
      if (name === 'OnOff') {
        readableValue = value === '1' || value === 'true' ? 'включено' : 'выключено';
      } else if (name === 'Temperature') {
        readableValue = `${parseFloat(value).toFixed(1)}°C`;
      } else if (name === 'Humidity') {
        readableValue = `${value}%`;
      } else if (name === 'Voltage') {
        readableValue = `${parseFloat(value).toFixed(2)} В`;
      } else if (name === 'Battery') {
        readableValue = `${value}%`;
      }

      const source = isCustomReport ? 'репорт' : 'атрибут';
      const timeStr = new Date().toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit' });
      const technical = `[${short}/${ep}] Cl: ${clusterId}, ${source}: ${attrId}`;
      const message = `${emoji} ${name} → ${readableValue}\n${technical} (${timeStr})`;

      addToast(message);
    },

    // ✅ Новый колбэк: реакция на системные события
    onSystemNotify: ({ type, message, emoji }) => {
      let userMessage = message;

      if (type === 'zigbee_permit_join_started') {
        userMessage = '🌐 Сеть Zigbee открыта для новых устройств';
      } else if (type === 'zigbee_permit_join_stopped') {
        userMessage = '🛑 Сеть Zigbee закрыта';
      }

      addToast(`${emoji} ${userMessage}`, 5000);
    }
  });

  // 🔁 Храним только IEEE выбранного устройства
  const [selectedIEEE, setSelectedIEEE] = useState(null);
  const selectedDevice = fullDevices.find(d => d.ieee_addr === selectedIEEE) || null;

  const briefList = fullDevices.map(dev => ({
    ieee: dev.ieee_addr,
    short: parseInt(dev.short_addr.replace('0x', ''), 16),
    friendly_name: dev.name || dev.friendly_name,
    online: dev.is_online,
    linkquality: dev.lqi
  }));

  const handleSelectDevice = (briefDev) => {
    setSelectedIEEE(briefDev.ieee);
  };

  // Следим за хэшем URL
  useEffect(() => {
    const onHashChange = () => setCurrentPath(window.location.hash || '#/');
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
      <Navbar />

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

      {/* ❌ Убираем локальные тосты — они теперь в NotificationProvider */}
      {/* <div className="toast-container">...</div> */}
    </div>
  );
}

// ✅ Оборачиваем App в NotificationProvider
export default function AppWithNotifications() {
  useServerHealth(); // ← автоматически следит за состоянием

  return (
    <NotificationProvider>
      <App />
    </NotificationProvider>
  );
}