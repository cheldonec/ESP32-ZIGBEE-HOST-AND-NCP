// src/App.js
import { useState, useEffect } from 'react';
import './App.css';

// Компоненты
import Sidebar from './components/Sidebar';
import Header from './components/Header';
import Footer from './components/Footer';
import DeviceDetails from './components/DeviceDetails';
import Settings from './components/Settings';
import NotificationProvider from './components/NotificationProvider';
import { useNotification } from './context/NotificationContext';
import { useServerHealth } from './hooks/useServerHealth';
import Navbar from './components/Navbar';
import { useDevices } from './hooks/useDevices';
import BehaviorsPanel from './components/BehaviorsPanel';
import RuleEditor from './components/RuleEditor';

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
  const [selectedItem, setSelectedItem] = useState(null); // { type: 'device', id: '...' }
  const { coordinator } = useCoordinator();
  const { addToast } = useNotification();

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

    onSystemNotify: ({ type, message, emoji, data }) => {
      let userMessage = message;
      if (type === 'device_renamed') {
        const friendlyName = data.friendly_name || 'Без имени';
        userMessage = `🔄 Устройство переименовано: ${friendlyName}`;
      } else if (type === 'zigbee_permit_join_started') {
        userMessage = '🌐 Сеть Zigbee открыта для новых устройств';
      } else if (type === 'zigbee_permit_join_stopped') {
        userMessage = '🛑 Сеть Zigbee закрыта';
      }

      addToast(`${emoji} ${userMessage}`, 5000);
    }
  });

  const selectedDevice = fullDevices.find(d => d.ieee_addr === selectedItem?.id) || null;

  useEffect(() => {
    const onHashChange = () => setCurrentPath(window.location.hash || '#/');
    window.addEventListener('hashchange', onHashChange);
    return () => window.removeEventListener('hashchange', onHashChange);
  }, []);

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
          <div className="content-area">
            <p>Ожидание подключения к шлюзу...</p>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="app-container">
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

      <Navbar />

      <div className="main-layout">
        <Sidebar
          currentTab={currentPath.replace('#', '')}
          selectedItem={selectedItem}
          onSelectItem={(type, id) => setSelectedItem({ type, id })}
        />

        <div className="content-area">
          {currentPath === '#/settings' && selectedItem?.type === 'settings' && (
            <Settings activeSection={selectedItem.id} />
          )}
          {currentPath === '#/' && selectedItem?.type === 'device' && (
            <DeviceDetails key={selectedItem.id} device={selectedDevice} />
          )}

          {currentPath === '#/scenes' && selectedItem?.type === 'scene' && (
            <BehaviorsPanel sceneId={selectedItem.id} />
          )}

          {currentPath === '#/rules' && selectedItem?.type === 'rule' && (
            <RuleEditor ruleId={selectedItem.id} />  // ✅ Теперь используем модульный редактор
          )}

          {![ '/', '/settings', '/scenes', '/rules'].includes(currentPath.replace('#', '')) && (
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
    </div>
  );
}

export default function AppWithNotifications() {
  useServerHealth();
  return (
    <NotificationProvider>
      <App />
    </NotificationProvider>
  );
}