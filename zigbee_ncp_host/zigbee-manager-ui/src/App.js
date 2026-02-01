// src/App.js
import React, { useState, useEffect } from 'react';
import './App.css';
import RuleList from './components/RuleList';
import RuleEditor from './components/RuleEditor';

function App() {
  const [showSettings, setShowSettings] = useState(false);
  const [settings, setSettings] = useState({
    ha: {
      enabled: true,
      mqtt: {
        enabled: true,
        broker: 'core-mosquitto',
        port: 1883,
        username: '',
        password: '',
        discovery: true,
        availability: true
      },
      language: 'ru'
    },
    web: {
      theme: 'dark'
    }
  });

  {/* Редактор сценариев */}
  const [showRules, setShowRules] = useState(false);
  const [rules, setRules] = useState([]);
  const [editingRule, setEditingRule] = useState(null);

  {/* Меню команд для эндпоинта */}
  const [showCommandModal, setShowCommandModal] = useState(false);
  const [selectedDevice, setSelectedDevice] = useState(null);
  const [selectedEndpoint, setSelectedEndpoint] = useState(null);

  const openCommandModal = (device, endpointId) => {
    setSelectedDevice(device);
    setSelectedEndpoint(endpointId);
    setShowCommandModal(true);
  };

  const [devices, setDevices] = useState([]);
  const [bindingTargets, setBindingTargets] = useState([]);
  const [theme, setTheme] = useState('dark');
  const [wifiSSID, setWifiSSID] = useState('...');
  const [isNetworkOpen, setIsNetworkOpen] = useState(false);
  const [showBindModal, setShowBindModal] = useState(false);
  const [bindForm, setBindForm] = useState({
    srcDevice: '',
    srcEp: '',
    cluster: '',
    tgtDevice: '',
    tgtEp: '',
  });
  const [editingEndpoint, setEditingEndpoint] = useState(null);

  // 🔍 Фильтры и сортировка
  const [searchTerm, setSearchTerm] = useState('');
  const [filterStatus, setFilterStatus] = useState('all');
  const [filterType, setFilterType] = useState('all');
  const [sortOrder, setSortOrder] = useState('name-asc');

  // Загрузка темы
  useEffect(() => {
    fetch('/api/config')
      .then(r => r.json())
      .then(data => {
        setSettings(data);
        setTheme(data.web.theme);
        document.documentElement.setAttribute('class', data.web.theme);
        localStorage.setItem('zigbee-ui-theme', data.web.theme);
      })
      .catch(err => console.warn("Не удалось загрузить настройки:", err));

    // Загрузка правил
    fetch('/api/rules')
      .then(r => r.json())
      .then(data => setRules(Array.isArray(data) ? data : []))
      .catch(err => {
        console.warn("Не удалось загрузить правила:", err);
        setRules([]);
      });
  }, []);

  const saveSettings = () => {
    fetch('/api/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(settings)
    })
    .then(r => r.json())
    .then(data => {
      if (data.status === 'ok') {
        alert('✅ Настройки сохранены');
        setShowSettings(false);
        setTheme(settings.web.theme);
        document.documentElement.setAttribute('class', settings.web.theme);
        localStorage.setItem('zigbee-ui-theme', settings.web.theme);
      }
    })
    .catch(err => {
      alert('❌ Ошибка сети');
    });
  };

  const toggleTheme = () => {
    const newTheme = theme === 'dark' ? 'light' : 'dark';
    setTheme(newTheme);
    localStorage.setItem('zigbee-ui-theme', newTheme);
    document.documentElement.setAttribute('class', newTheme);
  };

  const sendCommand = (cmd, payload = {}) => {
    const ws = new WebSocket(`ws://${window.location.host}/ws`);
    ws.onopen = () => {
      ws.send(JSON.stringify({ cmd, ...payload }));
      ws.close();
    };
  };

  // Состояние для ON_WITH_TIMED_OFF
  const [showTimedOffModal, setShowTimedOffModal] = useState(false);
  const [timedOffForm, setTimedOffForm] = useState({
    on_time: 600,
    off_wait_time: 0,
    on_off_control: 1,
  });

  const getCommandId = (cmdStr) => {
    const map = {
      'OFF': 0x00,
      'ON': 0x01,
      'TOGGLE': 0x02,
      'ON_WITH_TIMED_OFF': 0x42,
    };
    return map[cmdStr];
  };

  const sendTestCommand = (dev, epId, command) => {
    const cmdId = getCommandId(command.id);
    if (cmdId === undefined) {
      alert('❌ Неизвестная команда');
      return;
    }

    let payload = {
      cmd: "send_automation_command",
      short_addr: dev.short,
      endpoint: Number(epId),
      cmd_id: cmdId,
    };

    if (command.id === 'ON_WITH_TIMED_OFF') {
      setTimedOffForm({
        on_time: 600,
        off_wait_time: 0,
        on_off_control: 1,
      });
      setShowTimedOffModal(true);
      return;
    }

    sendCommand("send_automation_command", payload);
    setShowCommandModal(false);
  };

  const toggleDevice = (short, endpoint) => {
    sendCommand("toggle", { short, endpoint });
  };

  const updateFriendlyName = (short, newName) => {
    if (!newName.trim()) return;
    sendCommand("update_friendly_name", { short, name: newName.trim() });
    setDevices(prev =>
      prev.map(d => d.short === short ? { ...d, name: newName.trim() } : d)
    );
  };

  const updateEndpointName = (short, endpoint, newName) => {
    if (!newName.trim()) return;
    sendCommand("update_endpoint_name", { short, endpoint, name: newName.trim() });
    setEditingEndpoint(null);
  };

  // Подключение к WebSocket
  useEffect(() => {
    let websocket = null;
    let reconnectTimeout = null;
    let reconnectAttempts = 0;
    const maxReconnectAttempts = 10;
    const maxReconnectDelay = 10000;

    const connect = () => {
      if (reconnectAttempts > 0) {
        console.log(`Попытка переподключения (${reconnectAttempts}/${maxReconnectAttempts})...`);
      }

      websocket = new WebSocket(`ws://${window.location.host}/ws`);

      websocket.onopen = () => {
        console.log("WebSocket подключён");
        reconnectAttempts = 0;
        websocket.send(JSON.stringify({ cmd: "get_devices" }));
        websocket.send(JSON.stringify({ cmd: "get_network_status" }));
      };

      websocket.onmessage = (event) => {
        const data = JSON.parse(event.data);

        if (data.devices) {
          setDevices(data.devices);
        } else if (data.event === "device_update") {
          setDevices(prev => {
            const exists = prev.some(d => d.short === data.short);
            if (exists) {
              return prev.map(d => d.short === data.short ? { ...d, ...data } : d);
            } else {
              return [...prev, data];
            }
          });
        } else if (data.event === "friendly_name_updated") {
          setDevices(prev => prev.map(d => d.short === data.short ? { ...d, name: data.name } : d));
        } else if (data.event === "network_status") {
          setWifiSSID(data.wifi_ssid || '—');
          setIsNetworkOpen(data.zigbee_open || false);
        } else if (data.event === "endpoint_name_updated") {
          setDevices(prev => prev.map(d =>
            d.short === data.short
              ? {
                  ...d,
                  clusters: d.clusters.map(c =>
                    c.endpoint_id === data.endpoint_id
                      ? { ...c, endpoint_name: data.name }
                      : c
                  )
                }
              : d
          ));
        } else if (data.event === "rules_updated") {
          fetch('/api/rules')
            .then(r => r.json())
            .then(newRules => {
              if (Array.isArray(newRules)) {
                setRules(newRules);
              }
            })
            .catch(err => console.error("Ошибка при обновлении правил:", err));
        }
      };

      websocket.onclose = () => {
        console.log("WebSocket закрыт.");
        if (reconnectAttempts < maxReconnectAttempts) {
          const delay = Math.min(1000 * 2 ** reconnectAttempts, maxReconnectDelay);
          reconnectAttempts++;
          reconnectTimeout = setTimeout(connect, delay);
        }
      };

      websocket.onerror = (err) => {
        console.error("WebSocket ошибка:", err);
        websocket.close();
      };
    };

    connect();

    fetch('/api/binding_targets')
      .then(r => r.json())
      .then(data => setBindingTargets(data))
      .catch(err => console.error("Ошибка загрузки устройств для привязки:", err));

    return () => {
      if (reconnectTimeout) clearTimeout(reconnectTimeout);
      if (websocket) websocket.close();
    };
  }, []);

  // === Функции для привязки ===
  const performBind = () => {
    const { srcDevice, srcEp, cluster, tgtDevice, tgtEp } = bindForm;
    if (![srcDevice, srcEp, cluster, tgtDevice, tgtEp].every(Boolean)) {
      alert("Заполните все поля");
      return;
    }

    fetch('/api/bind', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        src_addr: Number(bindForm.srcDevice),
        src_ep: Number(bindForm.srcEp),
        cluster_id: Number(bindForm.cluster),
        tgt_addr: Number(bindForm.tgtDevice),
        tgt_ep: Number(bindForm.tgtEp),
      })
    })
    .then(r => r.json())
    .then(data => {
      if (data.status === 'ok') {
        alert('✅ Привязка отправлена');
        setShowBindModal(false);
      } else {
        alert('❌ Ошибка: ' + data.error);
      }
    })
    .catch(err => {
      alert('Ошибка сети: ' + err.message);
    });
  };

  const getOutputClusters = (short, epId) => {
    const dev = bindingTargets.find(d => d.short === short);
    const ep = dev?.endpoints.find(e => e.id === epId);
    return ep?.output_clusters || [];
  };

  const getInputClusters = (short, epId) => {
    const dev = bindingTargets.find(d => d.short === short);
    const ep = dev?.endpoints.find(e => e.id === epId);
    return ep?.input_clusters || [];
  };

  // === Определение типа устройства ===
  const getDeviceType = (device) => {
    const hasOnOff = device.clusters.some(c => c.type === 'on_off');
    const hasTemp = device.clusters.some(c => c.type === 'temperature');
    const hasHumidity = device.clusters.some(c => c.type === 'humidity');

    if (hasOnOff && !hasTemp && !hasHumidity) return 'light';
    if (hasTemp || hasHumidity) return 'sensor';
    if (hasOnOff) return 'switch';
    return 'other';
  };

  // === Фильтрация и сортировка ===
  const filteredAndSortedDevices = devices
    .filter(dev => {
      const matchesSearch = dev.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
                            `0x${dev.short.toString(16)}`.includes(searchTerm.toLowerCase());
      const matchesStatus = filterStatus === 'all' ||
                            (filterStatus === 'online' && dev.online) ||
                            (filterStatus === 'offline' && !dev.online);
      const devType = getDeviceType(dev);
      const matchesType = filterType === 'all' || filterType === devType;
      return matchesSearch && matchesStatus && matchesType;
    })
    .sort((a, b) => {
      switch (sortOrder) {
        case 'name-asc':
          return a.name.localeCompare(b.name);
        case 'name-desc':
          return b.name.localeCompare(a.name);
        case 'status':
          return a.online === b.online ? 0 : (a.online ? -1 : 1);
        default:
          return 0;
      }
    });

  // === Рендер ===
  return (
    <div className="App">
      <header className="App-header">
        <div className="control-panel">
          <div className="network-info">
            <span title="Подключён к Wi-Fi">📶 {wifiSSID}</span>
          </div>

          <button
            className={`network-toggle-btn ${isNetworkOpen ? 'open' : 'closed'}`}
            onClick={() => sendCommand("toggle_network", { duration: 60 })}
          >
            {isNetworkOpen ? '🔓 Сеть открыта' : '🔐 Закрыто'}
          </button>

          <button onClick={() => setShowBindModal(true)} className="bind-btn">
            🔗 Привязка
          </button>

          <button className="theme-toggle-inline" onClick={toggleTheme}>
            {theme === 'dark' ? '☀️' : '🌙'}
          </button>

          <button
            className="settings-btn"
            onClick={() => setShowSettings(!showSettings)}
            title="Настройки"
          >
            ⚙️
          </button>

          <button
            className="rules-btn"
            onClick={() => setShowRules(!showRules)}
            title="Модуль автоматизации"
          >
            🎯 Сценарии
          </button>
        </div>

        {/* === ПАНЕЛЬ НАСТРОЕК === */}
        {showSettings && (
          <div className="settings-panel">
            {/* Карточка: Основные */}
            <div className="settings-card">
              <h3>🔧 Основные</h3>

              <label>
                Язык интерфейса:
                <select
                  value={settings.ha.language}
                  onChange={(e) => setSettings(prev => ({
                    ...prev,
                    ha: { ...prev.ha, language: e.target.value }
                  }))}
                >
                  <option value="ru">Русский</option>
                  <option value="en">English</option>
                  <option value="de">Deutsch</option>
                </select>
              </label>

              <label>
                Тема:
                <select
                  value={settings.web.theme}
                  onChange={(e) => setSettings(prev => ({
                    ...prev,
                    web: { ...prev.web, theme: e.target.value }
                  }))}
                >
                  <option value="dark">Тёмная</option>
                  <option value="light">Светлая</option>
                </select>
              </label>
            </div>

            {/* Карточка: Home Assistant */}
            <div className="settings-card">
              <h3>🏠 Home Assistant</h3>

              <label>
                <input
                  type="checkbox"
                  checked={settings.ha.enabled}
                  onChange={(e) => setSettings(prev => ({
                    ...prev,
                    ha: { ...prev.ha, enabled: e.target.checked }
                  }))}
                />
                Включить интеграцию
              </label>

              <label>
                <input
                  type="checkbox"
                  checked={settings.ha.mqtt.enabled}
                  onChange={(e) => setSettings(prev => ({
                    ...prev,
                    ha: {
                      ...prev.ha,
                      mqtt: { ...prev.ha.mqtt, enabled: e.target.checked }
                    }
                  }))}
                />
                Включить MQTT
              </label>

              <label>
                Брокер:
                <input
                  type="text"
                  value={settings.ha.mqtt.broker}
                  onChange={(e) => setSettings(prev => ({
                    ...prev,
                    ha: {
                      ...prev.ha,
                      mqtt: { ...prev.ha.mqtt, broker: e.target.value }
                    }
                  }))}
                />
              </label>

              <label>
                Порт:
                <input
                  type="number"
                  value={settings.ha.mqtt.port}
                  onChange={(e) => setSettings(prev => ({
                    ...prev,
                    ha: {
                      ...prev.ha,
                      mqtt: { ...prev.ha.mqtt, port: Number(e.target.value) }
                    }
                  }))}
                />
              </label>

              <label>
                Логин:
                <input
                  type="text"
                  value={settings.ha.mqtt.username}
                  onChange={(e) => setSettings(prev => ({
                    ...prev,
                    ha: {
                      ...prev.ha,
                      mqtt: { ...prev.ha.mqtt, username: e.target.value }
                    }
                  }))}
                />
              </label>

              <label>
                Пароль:
                <input
                  type="password"
                  value={settings.ha.mqtt.password}
                  onChange={(e) => setSettings(prev => ({
                    ...prev,
                    ha: {
                      ...prev.ha,
                      mqtt: { ...prev.ha.mqtt, password: e.target.value }
                    }
                  }))}
                />
              </label>

              <label>
                <input
                  type="checkbox"
                  checked={settings.ha.mqtt.discovery}
                  onChange={(e) => setSettings(prev => ({
                    ...prev,
                    ha: {
                      ...prev.ha,
                      mqtt: { ...prev.ha.mqtt, discovery: e.target.checked }
                    }
                  }))}
                />
                Авто-дискавери
              </label>

              <label>
                <input
                  type="checkbox"
                  checked={settings.ha.mqtt.availability}
                  onChange={(e) => setSettings(prev => ({
                    ...prev,
                    ha: {
                      ...prev.ha,
                      mqtt: { ...prev.ha.mqtt, availability: e.target.checked }
                    }
                  }))}
                />
                Availability
              </label>
            </div>

            {/* Кнопки */}
            <div className="settings-actions">
              <button className="btn-primary" onClick={saveSettings}>💾 Сохранить</button>
              <button className="btn-danger" onClick={() => setShowSettings(false)}>❌ Отмена</button>
            </div>
          </div>
        )}

        <h1>📡 Zigbee Устройства</h1>

        {/* 🔍 Фильтры */}
        <div className="filters-bar">
          <input
            type="text"
            placeholder="🔍 Поиск по имени..."
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
            className="search-input"
          />

          <select value={filterStatus} onChange={(e) => setFilterStatus(e.target.value)} className="filter-select">
            <option value="all">Все</option>
            <option value="online">Онлайн</option>
            <option value="offline">Офлайн</option>
          </select>

          <select value={filterType} onChange={(e) => setFilterType(e.target.value)} className="filter-select">
            <option value="all">Все типы</option>
            <option value="light">Свет</option>
            <option value="sensor">Датчики</option>
            <option value="switch">Розетки</option>
            <option value="other">Прочее</option>
          </select>

          <select value={sortOrder} onChange={(e) => setSortOrder(e.target.value)} className="filter-select">
            <option value="name-asc">Имя ↑</option>
            <option value="name-desc">Имя ↓</option>
            <option value="status">Онлайн сверху</option>
          </select>
        </div>

        {/* Счётчик */}
        <p className="devices-count">
          Найдено: <strong>{filteredAndSortedDevices.length}</strong> из {devices.length}
        </p>

        {filteredAndSortedDevices.length === 0 ? (
          <p className="no-devices">устройств не найдено</p>
        ) : (
          <div className="devices-grid">
            {filteredAndSortedDevices.map(dev => (
              <div key={dev.short} className="device-card">
                <div className="device-header">
                  <div className="device-name-edit">
                    {dev.isEditing ? (
                      <input
                        type="text"
                        className="friendly-name-input editing"
                        defaultValue={dev.name}
                        onBlur={(e) => {
                          updateFriendlyName(dev.short, e.target.value);
                          setDevices(prev =>
                            prev.map(d => d.short === dev.short ? { ...d, isEditing: false } : d)
                          );
                        }}
                        onKeyDown={(e) => {
                          if (e.key === 'Enter') {
                            updateFriendlyName(dev.short, e.target.value);
                            setDevices(prev =>
                              prev.map(d => d.short === dev.short ? { ...d, isEditing: false } : d)
                            );
                          } else if (e.key === 'Escape') {
                            setDevices(prev =>
                              prev.map(d => d.short === dev.short ? { ...d, isEditing: false } : d)
                            );
                          }
                        }}
                        autoFocus
                        onClick={(e) => e.target.select()}
                      />
                    ) : (
                      <span
                        className="friendly-name-display"
                        onClick={() =>
                          setDevices(prev =>
                            prev.map(d => d.short === dev.short ? { ...d, isEditing: true } : d)
                          )
                        }
                        title="Нажмите, чтобы изменить имя"
                      >
                        {dev.name || `0x${dev.short.toString(16).padStart(4, '0').toUpperCase()}`}
                      </span>
                    )}
                  </div>
                  <span className={`status-dot ${dev.online ? 'online' : 'offline'}`}></span>
                </div>

                <div className="device-info">
                  <small>
                    🔗 {`0x${dev.short.toString(16).padStart(4, '0').toUpperCase()}`}
                    {dev.model_id && (
                      <>
                        {' | '}
                        <span title="Model ID" style={{ color: '#aaa', fontSize: '0.9em' }}>
                          {dev.model_id}
                        </span>
                      </>
                    )}
                    {dev.manufacturer_name && (
                      <>
                        {' | '}
                        <span title="Manufacturer" style={{ color: '#888', fontSize: '0.8em' }}>
                          {dev.manufacturer_name}
                        </span>
                      </>
                    )}
                    {dev.battery && (
                      <>
                        {' | '}
                        <span title={`Батарея: ${dev.battery.display}${dev.battery.percent_display ? ' (' + dev.battery.percent_display + ')' : ''}`}>
                          🔋 {dev.battery.display}
                          {dev.battery.percent_display && <> ({dev.battery.percent_display})</>}
                        </span>
                      </>
                    )}
                  </small>
                </div>

                <div className="device-content">
                  {(() => {
                    const clustersByEp = {};
                    dev.clusters.forEach(c => {
                      if (!clustersByEp[c.endpoint_id]) clustersByEp[c.endpoint_id] = [];
                      clustersByEp[c.endpoint_id].push(c);
                    });

                    return Object.entries(clustersByEp).map(([epId, clusters]) => (
                      <div key={epId} className="cluster-group">
                        <div className="endpoint-name-edit">
                          {editingEndpoint && editingEndpoint.short === dev.short && editingEndpoint.endpoint_id === Number(epId) ? (
                            <input
                              autoFocus
                              defaultValue={editingEndpoint.originalName}
                              onBlur={(e) => updateEndpointName(dev.short, Number(epId), e.target.value)}
                              onKeyDown={(e) => {
                                if (e.key === 'Enter') {
                                  updateEndpointName(dev.short, Number(epId), e.target.value);
                                } else if (e.key === 'Escape') {
                                  setEditingEndpoint(null);
                                }
                              }}
                              className="endpoint-edit-input"
                            />
                          ) : (
                            <div className="endpoint-header">
                              <span
                                onClick={() => setEditingEndpoint({
                                  short: dev.short,
                                  endpoint_id: Number(epId),
                                  originalName: clusters[0].endpoint_name
                                })}
                                className="endpoint-name-label"
                                title="Нажмите, чтобы переименовать endpoint"
                              >
                                🏷️ {clusters[0].endpoint_name}
                              </span>

                              <button
                                className="action-menu-btn"
                                title="Тестовые команды"
                                onClick={(e) => {
                                  e.stopPropagation();
                                  openCommandModal(dev, Number(epId));
                                }}
                              >
                                ⋮
                              </button>
                            </div>
                          )}
                        </div>

                        {clusters.map((cluster, idx) => (
                          <div key={idx} className="cluster-item">
                            {cluster.type === 'on_off' && (
                              <div className={`state-indicator ${cluster.value ? 'on' : 'off'}`}>
                                {cluster.value ? '🟢 ВКЛ' : '🔴 ВЫКЛ'}
                                <button
                                  className="toggle-btn"
                                  onClick={() => toggleDevice(dev.short, cluster.endpoint_id)}
                                >
                                  {cluster.value ? 'Выключить' : 'Включить'}
                                </button>
                              </div>
                            )}

                            {cluster.type === 'temperature' && (
                              <div className="sensor-value">
                                <span>🌡️</span>
                                <span>Температура: <strong>{cluster.display}</strong></span>
                              </div>
                            )}

                            {cluster.type === 'humidity' && (
                              <div className="sensor-value">
                                <span>💧</span>
                                <span>Влажность: <strong>{cluster.display}</strong></span>
                              </div>
                            )}

                            {cluster.type === 'unknown' && (
                              <div className="unknown">⚠️ {cluster.display}</div>
                            )}
                          </div>
                        ))}
                      </div>
                    ));
                  })()}
                </div>
              </div>
            ))}
          </div>
        )}

        {/* === Модальное окно привязки === */}
        {showBindModal && (
          <div className="modal-overlay">
            <div className="modal-content">
              <h3>Создать привязку</h3>
              <p className="modal-description">
                <strong>Источник</strong> будет отправлять отчёты <strong>получателю</strong>
              </p>

              <label>1. Источник (input):</label>
              <select
                value={bindForm.srcDevice}
                onChange={e => setBindForm(prev => ({ ...prev, srcDevice: e.target.value, srcEp: '', cluster: '' }))}
                className="form-select"
              >
                <option value="">Выберите устройство...</option>
                {bindingTargets.map(dev => (
                  <option key={dev.short} value={dev.short}>
                    {dev.name} (0x{dev.short.toString(16).padStart(4, '0').toUpperCase()})
                  </option>
                ))}
              </select>

              {bindForm.srcDevice && (
                <>
                  <label>2. Endpoint (input):</label>
                  <select
                    value={bindForm.srcEp}
                    onChange={e => setBindForm(prev => ({ ...prev, srcEp: e.target.value, cluster: '' }))}
                    className="form-select"
                  >
                    <option value="">Выберите endpoint...</option>
                    {bindingTargets
                      .find(d => d.short === Number(bindForm.srcDevice))
                      ?.endpoints
                      .filter(ep => (ep.input_clusters || []).length > 0)
                      .map(ep => (
                        <option key={ep.id} value={ep.id}>EP {ep.id}</option>
                      ))
                    }
                  </select>
                </>
              )}


              {bindForm.srcEp && (
                <>
                  <label>3. Кластер (input):</label>
                  <select
                    value={bindForm.cluster}
                    onChange={e => setBindForm(prev => ({ ...prev, cluster: e.target.value }))}
                    className="form-select"
                  >
                    <option value="">Выберите кластер...</option>
                    {getInputClusters(Number(bindForm.srcDevice), Number(bindForm.srcEp)).map(id => (
                      <option key={id} value={id}>0x{id.toString(16).padStart(4, '0')} (input)</option>
                    ))}
                  </select>
                </>
              )}

              <label>4. Получатель (output):</label>
              <select
                value={bindForm.tgtDevice}
                onChange={e => setBindForm(prev => ({ ...prev, tgtDevice: e.target.value, tgtEp: '' }))}
                className="form-select"
              >
                <option value="">Выберите устройство...</option>
                {bindingTargets.map(dev => (
                  <option key={dev.short} value={dev.short}>
                    {dev.name} (0x{dev.short.toString(16).padStart(4, '0').toUpperCase()})
                  </option>
                ))}
              </select>

              {bindForm.tgtDevice && (
                <>
                  <label>5. Endpoint (output):</label>
                  <select
                    value={bindForm.tgtEp}
                    onChange={e => setBindForm(prev => ({ ...prev, tgtEp: e.target.value }))}
                    className="form-select"
                  >
                    <option value="">Выберите endpoint...</option>
                    {bindingTargets
                      .find(d => d.short === Number(bindForm.tgtDevice))
                      ?.endpoints
                      .filter(ep => (ep.output_clusters || []).length > 0)
                      .map(ep => (
                        <option key={ep.id} value={ep.id}>EP {ep.id}</option>
                      ))
                    }
                  </select>
                </>
              )}

              <div className="modal-buttons">
                <button onClick={performBind} className="btn-primary">Привязать</button>
                <button onClick={() => setShowBindModal(false)} className="btn-danger">Отмена</button>
              </div>
            </div>
          </div>
        )}

        {/* === Модальное окно: Тест команды === */}
        {showCommandModal && selectedDevice && (
          <div className="modal-overlay">
            <div className="modal-content" style={{ maxWidth: '500px' }}>
              <h3>🧪 Тест команды</h3>
              <p>
                <strong>{selectedDevice.name}</strong> → EP {selectedEndpoint}
              </p>

              <div className="command-list">
                {[
                  { id: 'ON', label: 'Включить', params: null },
                  { id: 'OFF', label: 'Выключить', params: null },
                  { id: 'TOGGLE', label: 'Переключить', params: null },
                  { id: 'ON_WITH_TIMED_OFF', label: 'Вкл с таймером', params: ['on_time', 'off_wait_time'] }
                ].map(cmd => (
                  <button
                    key={cmd.id}
                    className="command-item-btn"
                    onClick={() => sendTestCommand(selectedDevice, selectedEndpoint, cmd)}
                  >
                    {cmd.label}
                    {cmd.params && (<span style={{ fontSize: '0.8em', opacity: 0.7 }}> → нажмите для настройки</span>)}
                  </button>
                ))}
              </div>

              <div className="modal-buttons">
                <button className="btn-danger" onClick={() => setShowCommandModal(false)}>
                  ❌ Закрыть
                </button>
              </div>
            </div>
          </div>
        )}

        {/* === Модальное окно: Вкл с таймером === */}
        {showTimedOffModal && selectedDevice && (
          <div className="modal-overlay">
            <div className="modal-content" style={{ maxWidth: '400px' }}>
              <h3>⏱️ Вкл с таймером</h3>
              <p>
                <strong>{selectedDevice.name}</strong> → EP {selectedEndpoint}
              </p>

              <div className="form-group">
                <label>
                  ⏱️ Время включения (0.1 сек)
                  <input
                    type="number"
                    value={timedOffForm.on_time}
                    onChange={(e) => setTimedOffForm(prev => ({
                      ...prev,
                      on_time: parseInt(e.target.value) || 0
                    }))}
                    placeholder="Например: 600 = 60 сек"
                    min="0"
                    required
                  />
                </label>
              </div>

              <div className="form-group">
                <label>
                  ⏳ Задержка перед выключением (0.1 сек)
                  <input
                    type="number"
                    value={timedOffForm.off_wait_time}
                    onChange={(e) => setTimedOffForm(prev => ({
                      ...prev,
                      off_wait_time: parseInt(e.target.value) || 0
                    }))}
                    placeholder="0 — выключится сразу"
                    min="0"
                  />
                </label>
              </div>

              <div className="form-group">
                <label>
                  🔧 On/Off Control
                  <select
                    value={timedOffForm.on_off_control}
                    onChange={(e) => setTimedOffForm(prev => ({
                      ...prev,
                      on_off_control: parseInt(e.target.value)
                    }))}
                  >
                    <option value={0}>0 — Прервать если уже включено</option>
                    <option value={1}>1 — Не прерывать, если уже включено</option>
                  </select>
                </label>
                <small style={{ opacity: 0.7 }}>
                  Управляет поведением, если устройство уже включено.
                </small>
              </div>

              <div className="modal-buttons">
                <button
                  className="btn-primary"
                  onClick={() => {
                    const payload = {
                      cmd: "send_automation_command",
                      short_addr: selectedDevice.short,
                      endpoint: selectedEndpoint,
                      cmd_id: getCommandId('ON_WITH_TIMED_OFF'),
                      params: {
                        on_time: timedOffForm.on_time,
                        off_wait_time: timedOffForm.off_wait_time,
                      },
                    };
                    sendCommand("send_automation_command", payload);
                    setShowTimedOffModal(false);
                    setShowCommandModal(false);
                  }}
                >
                  ✅ Отправить
                </button>
                <button
                  className="btn-danger"
                  onClick={() => setShowTimedOffModal(false)}
                >
                  ❌ Отмена
                </button>
              </div>
            </div>
          </div>
        )}
      </header>

      {/* === ВКЛАДКА: ПРАВИЛА === */}
      {showRules && (
        <div className="rules-section">
          <RuleList
            rules={rules}
            onEdit={(rule) => setEditingRule(rule)}
            onRun={(id) => {
              fetch(`/api/rules/run/${id}`, { method: 'POST' })
                .then(r => r.json())
                .then(data => {
                  if (data.status === 'ok') {
                    alert('✅ Правило запущено');
                  } else {
                    alert('❌ Ошибка: ' + data.error);
                  }
                })
                .catch(err => {
                  console.error("Run error:", err);
                  alert('❌ Ошибка сети');
                });
            }}
            onDelete={(id) => {
              if (!id) return;
              fetch(`/api/rules/${id}`, { method: 'DELETE' })
                .then(r => r.json())
                .then(data => {
                  if (data.status === 'ok') {
                    setRules(prev => prev.filter(r => r.id !== id));
                    alert('✅ Правило удалено');
                  } else {
                    alert('❌ Ошибка: ' + (data.error || 'неизвестная'));
                  }
                })
                .catch(err => {
                  alert('❌ Ошибка сети: ' + err.message);
                });
            }}
          />
        </div>
      )}

      {/* === МОДАЛЬНОЕ ОКНО РЕДАКТОРА — ВНЕ вкладки! === */}
      {editingRule !== null && (
        <div className="modal-overlay">
          <div className="modal-content" style={{ maxWidth: '600px' }}>
            <RuleEditor
              rule={editingRule}
              devices={devices}
              onSave={(updatedRule) => {
                const isEdit = !!editingRule.id;
                fetch('/api/rules', {
                  method: isEdit ? 'PUT' : 'POST',
                  headers: { 'Content-Type': 'application/json' },
                  body: JSON.stringify(updatedRule)
                })
                  .then(r => r.json())
                  .then(data => {
                    if (data.status === 'ok') {
                      if (isEdit) {
                        setRules(prev => prev.map(r => r.id === updatedRule.id ? updatedRule : r));
                      } else {
                        setRules(prev => [...prev, updatedRule]);
                      }
                      setEditingRule(null);
                      alert('✅ Правило сохранено');
                    } else {
                      alert('❌ Ошибка: ' + (data.error || 'неизвестная'));
                    }
                  })
                  .catch(err => {
                    console.error("Fetch error:", err);
                    alert('❌ Ошибка сети: ' + err.message);
                  });
              }}
              onCancel={() => setEditingRule(null)}
            />
          </div>
        </div>
      )}
    </div>
  );
}

export default App;