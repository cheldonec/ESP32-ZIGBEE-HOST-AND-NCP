// src/App.js
import { useState, useEffect } from 'react';
import './App.css';

// Компоненты
import Sidebar from './components/Sidebar';
import Navbar from './components/Navbar';
import DeviceDetails from './components/DeviceDetails';
import Settings from './components/Settings';
import NotificationProvider from './components/NotificationProvider';
import { useNotification } from './context/NotificationContext';
import BehaviorsPanel from './components/BehaviorsPanel';
import RuleEditor from './components/RuleEditor';

// Хуки
import { useDevices } from './hooks/useDevices';
import { useCoordinator } from './hooks/useCoordinator';
import { useRules } from './hooks/useRules';
import { initWebSocket, subscribeToWebSocket, sendWebSocketMessage } from './api/websocket';
import { api } from './api/httpClient';
import { useServerStatus } from './hooks/useServerStatus';

function App() {
  const [currentPath, setCurrentPath] = useState(window.location.hash || '#/');
  const [selectedItem, setSelectedItem] = useState(null);
  const [isWsConnected, setIsWsConnected] = useState(false); // Флаг подключения WS
  const { addToast } = useNotification();
  const { isReady: isServerReady } = useServerStatus(); // ← ждём сервер
  const { memory } = useServerStatus();
  const usedRamPercent = memory ? memory.used_percent.toFixed(1) : '?';
  const heapFreeKb = memory ? (memory.free / 1024).toFixed(0) : '?';
  const fragPercent = memory ? memory.fragmentation_percent.toFixed(1) : '?';
  // Инициализация WebSocket и подписка на события
  useEffect(() => {
    // Подписываемся на системные уведомления
    const unsubscribe = subscribeToWebSocket((data) => {
      if (data.event === 'system_notify') {
        let emoji = 'ℹ️';
        if (data.type === 'zigbee_permit_join_started') emoji = '🔓';
        if (data.type === 'zigbee_permit_join_stopped') emoji = '🔒';
        addToast(`${emoji} ${data.message}`, 5000);
      }

      if (data.type === 'rule_updated' || data.type === 'rule_deleted') {
        window.dispatchEvent(new CustomEvent('rules_changed'));
      }
      if (data.type === 'var_updated') {
        window.dispatchEvent(new CustomEvent('variables_changed'));
      }
    });

    // Слушаем событие успешного подключения
    const onWebSocketOpen = () => {
      console.log('🟢 WebSocket открыт → разрешаем загрузку');
      setIsWsConnected(true);
    };

    window.addEventListener('websocket_open', onWebSocketOpen);

    // Инициализируем WebSocket
    initWebSocket();

    return () => {
      window.removeEventListener('websocket_open', onWebSocketOpen);
      unsubscribe();
    };
  }, [addToast]);

  // Загружаем данные только после подключения WebSocket
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
    enabled: isWsConnected
  });

  const { coordinator, variables, reloadVariables } = useCoordinator({ enabled: isWsConnected });
  const { rules: allRules, loading: rulesLoading, reload: reloadRules } = useRules({ enabled: isWsConnected });

  // Хеш-роутинг
  useEffect(() => {
    const onHashChange = () => setCurrentPath(window.location.hash || '#/');
    window.addEventListener('hashchange', onHashChange);
    return () => window.removeEventListener('hashchange', onHashChange);
  }, []);

  // Health-check: проверка токена сессии
  useEffect(() => {
    if (!isWsConnected) return;

    const checkServer = async () => {
      try {
        const data = await api.getServerStatus(); // ← лучше через httpClient
        const currentToken = data.session_token;
        const savedToken = localStorage.getItem('server_session_token');

        if (!savedToken) {
          // Первый запуск — просто сохраняем
          localStorage.setItem('server_session_token', currentToken);
        } else if (savedToken !== currentToken) {
          // Только если токен изменился во время работы
          console.log('🔄 Сервер перезагрузился. Перезагружаем UI...');
          localStorage.setItem('server_session_token', currentToken);
          window.location.reload();
        }
      } catch (err) {
        console.debug('[Health] Ошибка:', err.message);
      }
    };

    checkServer();
    const interval = setInterval(checkServer, 10000);
    return () => clearInterval(interval);
  }, [isWsConnected]);

  // 🛑 Пока сервер не готов
  if (!isServerReady) {
    return (
      <div className="app-container">
        <header className="header">
          <div className="left">🌀 Ожидание сервера...</div>
        </header>
        <div className="main-layout">
          <div className="content-area text-center">
            <p>Подключение к шлюзу...</p>
            <p className="text-sm text-gray-500 mt-2">Пытаемся связаться с сервером</p>
          </div>
        </div>
      </div>
    );
  }

  // 🛑 Пока WebSocket не подключён
  if (!isWsConnected) {
    return (
      <div className="app-container">
        <header className="header">
          <div className="left">🔄 Устанавливаем соединение...</div>
        </header>
        <div className="main-layout">
          <div className="content-area text-center">
            <p>Соединение с WebSocket...</p>
            <p className="text-sm text-gray-500 mt-2">Ожидаем подключения к шине Zigbee</p>
          </div>
        </div>
      </div>
    );
  }

  // 🛑 Координатор не загружен
  if (!coordinator) {
    return (
      <div className="app-container">
        <header className="header">
          <div className="left">🌀 Загрузка координатора...</div>
        </header>
        <div className="main-layout">
          <div className="device-list">
            <p>Данные не получены. Проверьте подключение.</p>
          </div>
          <div className="content-area">
            <button onClick={() => window.location.reload()} className="btn-primary">
              Перезагрузить
            </button>
          </div>
        </div>
      </div>
    );
  }

  const selectedDevice = fullDevices.find(d => d.ieee_addr === selectedItem?.id) || null;

  return (
    <div className="app-container">
      <header className="header">
        <div className="left">
          <div><span>🌀</span> <strong>Zigbee:</strong> PAN {coordinator.pan_id} | CH {coordinator.radio_channel}</div>
          <div><span>📶</span> <strong>AP:</strong> {coordinator.wifi_ap_ssid || 'N/A'}</div>
        </div>
        <div className="ieee">{coordinator.ieee_addr}</div>
      </header>

      <Navbar />

      <div className="main-layout">
        <Sidebar
          currentTab={currentPath.replace('#', '')}
          selectedItem={selectedItem}
          onSelectItem={(type, id) => setSelectedItem({ type, id })}
          devices={fullDevices}
        />

        <div className="content-area">
          {currentPath === '#/settings' && selectedItem?.type === 'settings' && (
            <Settings activeSection={selectedItem.id} reloadVariables={reloadVariables} />
          )}
          {currentPath === '#/' && selectedItem?.type === 'device' && (
            <DeviceDetails key={selectedItem.id} device={selectedDevice} />
          )}
          {currentPath === '#/scenes' && selectedItem?.type === 'scene' && (
            <BehaviorsPanel sceneId={selectedItem.id} />
          )}
          {currentPath === '#/rules' && selectedItem?.type === 'rule' && (
            rulesLoading ? (
              <div className="p-8 text-center text-gray-400">
                <div className="animate-spin inline-block w-6 h-6 border-t-2 border-b-2 border-blue-500 rounded-full mr-3"></div>
                Загрузка правил...
              </div>
            ) : (
              <RuleEditor ruleId={selectedItem.id} />
            )
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
        <span>🧠 RAM: {usedRamPercent}%</span>
        <span>💾 Heap: {heapFreeKb} KB</span>
        <span>🗜️ Frag: {fragPercent}%</span>
      </footer>
    </div>
  );
}

export default function AppWithNotifications() {
  return (
    <NotificationProvider>
      <App />
    </NotificationProvider>
  );
}