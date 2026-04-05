// src/App.js
import { useState, useEffect } from 'react';
import './App.css';
import Settings from './components/Settings'; // ← подключаем внешний компонент

function App() {
  const [coordinator] = useState({
    pan_id: 6754,
    radio_channel: 11,
    wifi_ap_ssid: "Zigbee-Gateway-Setup",
    ieee_addr: "00:12:4B:00:00:00:00:01"
  });

  const [devices] = useState([
    {
      ieee: "AC:66:E7:4C:18:38:C1:A4",
      short: 10143,
      friendly_name: "Dev AC:66..A4",
      online: false,
      linkquality: 0
    }
  ]);

  // Определяем активную страницу по хешу
  const [currentPath, setCurrentPath] = useState(window.location.hash || '#/');

  useEffect(() => {
    const onHashChange = () => {
      const hash = window.location.hash || '#/';
      setCurrentPath(hash);
    };

    window.addEventListener('hashchange', onHashChange);
    return () => window.removeEventListener('hashchange', onHashChange);
  }, []);

  // === Выбор контента ===
  const renderContent = () => {
    if (currentPath === '#/settings') {
      return <Settings />; // ← используем внешний компонент
    }

    return (
      <div className="device-details">
        <h2>Выберите устройство</h2>
        <p className="ieee">Чтобы посмотреть свойства</p>
        <div style={{ fontSize: '50px', marginTop: '20px', textAlign: 'center' }}>🔌</div>
      </div>
    );
  };

  return (
    <div className="app-container">
      {/* Шапка */}
      <header className="header">
        <div className="left">
          <div><span>🌀</span> <strong>Zigbee:</strong> PAN {coordinator.pan_id} | CH {coordinator.radio_channel}</div>
          <div><span>📶</span> <strong>AP:</strong> {coordinator.wifi_ap_ssid}</div>
        </div>
        <div className="ieee">{coordinator.ieee_addr}</div>
      </header>

      {/* Навигация */}
      <nav className="navbar">
        <a href="#/" className={`nav-link ${currentPath === '#/' ? 'active' : ''}`}>🔌 Устройства</a>
        <a href="#/links" className={`nav-link ${currentPath === '#/links' ? 'active' : ''}`}>🔗 Связи</a>
        <a href="#/scenes" className={`nav-link ${currentPath === '#/scenes' ? 'active' : ''}`}>🎬 Сценарии</a>
        <a href="#/settings" className={`nav-link ${currentPath === '#/settings' ? 'active' : ''}`}>⚙️ Настройки</a>
        <a href="#/monitor" className={`nav-link ${currentPath === '#/monitor' ? 'active' : ''}`}>📊 Мониторинг</a>
      </nav>

      {/* Основной макет */}
      <div className="main-layout">
        {/* Список устройств — только на главной */}
        {currentPath === '#/' && (
          <div className="device-list">
            <h2>📱 Устройства</h2>
            {devices.map(dev => (
              <div key={dev.ieee} className="device-item">
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <div>
                    <div className="name">{dev.friendly_name}</div>
                    <div className="info">0x{dev.short.toString(16).toUpperCase()} • LQI: {dev.linkquality || '?'}</div>
                  </div>
                  <span className={`device-status ${dev.online ? 'status-online' : 'status-offline'}`}>
                    {dev.online ? 'Online' : 'Offline'}
                  </span>
                </div>
              </div>
            ))}
          </div>
        )}

        {/* Контент (настройки или выбор устройства) */}
        {renderContent()}
      </div>

      {/* Футер */}
      <footer className="footer">
        <span>🧠 RAM: 48%</span>
        <span>💾 Heap: 28 KB</span>
        <span>🗜️ Frag: 14%</span>
      </footer>
    </div>
  );
}

export default App;