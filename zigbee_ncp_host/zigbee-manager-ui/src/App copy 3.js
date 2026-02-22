// src/App.js
import React, { useState, useEffect } from 'react';
import './App.css';

import BindModal from './components/BindModal';
import ReportModal from './components/ReportModal';
import OnOffCommandModal from './components/OnOffCommandModal';
import OnOffTimedOffModal from './components/OnOffTimedOffModal';
import RuleList from './components/RuleList';
import RuleEditor from './components/RuleEditor';
import DeviceCard from './components/DeviceCard';

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

  // === Сценарии (правила) ===
  const [showRules, setShowRules] = useState(false);
  const [rules, setRules] = useState([]);
  const [editingRule, setEditingRule] = useState(null);

  // === Команды для endpoint ===
  const [showCommandModal, setShowCommandModal] = useState(false);
  const [selectedDevice, setSelectedDevice] = useState(null);
  const [selectedEndpoint, setSelectedEndpoint] = useState(null);

  const openCommandModal = (device, endpointId) => {
    setSelectedDevice(device);
    setSelectedEndpoint(endpointId);
    setShowCommandModal(true);
  };

  // === Основные состояния ===
  const [devices, setDevices] = useState([]);
  const [bindingTargets, setBindingTargets] = useState([]);
  const [theme, setTheme] = useState('dark');
  const [wifiSSID, setWifiSSID] = useState('...');
  const [isNetworkOpen, setIsNetworkOpen] = useState(false);

  // === Модальные окна ===
  const [showBindModal, setShowBindModal] = useState(false);
  const [bindForm, setBindForm] = useState({
    srcDevice: '',
    srcEp: '',
    cluster: '',
    tgtDevice: '',
    tgtEp: '',
  });

  const [showReportModal, setShowReportModal] = useState(false);
  const [reportForm, setReportForm] = useState({
    device: '',
    ep: '',
    cluster: '',
    min: 1,
    max: 300,
    change: 0,
  });

  // 🔍 Фильтры и сортировка
  const [searchTerm, setSearchTerm] = useState('');
  const [filterStatus, setFilterStatus] = useState('all');
  const [filterType, setFilterType] = useState('all');
  const [sortOrder, setSortOrder] = useState('name-asc');

  // === Тест команды: ON_WITH_TIMED_OFF ===
  const [showTimedOffModal, setShowTimedOffModal] = useState(false);
  const [timedOffForm, setTimedOffForm] = useState({
    on_time: 600,
    off_wait_time: 0,
    on_off_control: 1,
  });

  // === Конвертация полного JSON → в формат DeviceCard ===
  const convertToDeviceCardFormat = (fullDev) => {
    if (!fullDev) return null;

    const short = fullDev.short_addr;
    const name = fullDev.friendly_name || 'unknown';
    const online = fullDev.is_online;

    let battery = null;
    if (online && fullDev.device_power_config_cluster) {
      const pc = fullDev.device_power_config_cluster;
      const voltageRaw = pc.battery_voltage;
      if (voltageRaw !== 0xFF) {
        const voltage = voltageRaw * 0.1;
        const percentageRaw = pc.battery_percentage;
        const percentage = percentageRaw !== 0xFF ? Math.round(percentageRaw / 2) : -1;

        battery = {
          voltage,
          percent: percentage >= 0 && percentage <= 100 ? percentage : undefined,
          display: `${voltage.toFixed(1)} В`,
          percent_display: percentage >= 0 && percentage <= 100 ? `${percentage}%` : undefined,
        };
      }
    }

    const clusters = [];

    if (fullDev.endpoints && Array.isArray(fullDev.endpoints)) {
      for (const ep of fullDev.endpoints) {
        const epId = ep.ep_id;
        const epName = ep.friendly_name || `[0x${short.toString(16)}] [0x${epId.toString(16)}]`;

        // On/Off Cluster
        if (ep.onoff) {
          clusters.push({
            type: 'on_off',
            endpoint_id: epId,
            endpoint_name: epName,
            value: ep.onoff.on,
            display: ep.onoff.on ? 'ON' : 'OFF',
            unit: '',
          });
        }

        // Temperature Cluster
        if (ep.temperature) {
          const raw = ep.temperature.measured_value;
          if (raw !== undefined && raw !== -32768) {
            const temp = raw / 100.0;
            clusters.push({
              type: 'temperature',
              endpoint_id: epId,
              endpoint_name: epName,
              value: temp,
              display: `${temp.toFixed(1)} °C`,
              unit: '°C',
            });
          }
        }

        // Humidity Cluster
        if (ep.humidity) {
          const raw = ep.humidity.measured_value;
          if (raw !== undefined && raw !== 0xffff) {
            const hum = raw / 100.0;
            clusters.push({
              type: 'humidity',
              endpoint_id: epId,
              endpoint_name: epName,
              value: hum,
              display: `${hum.toFixed(1)} %`,
              unit: '%',
            });
          }
        }
      }
    }

    if (clusters.length === 0) {
      clusters.push({
        type: 'unknown',
        display: 'No data',
        unit: '',
        endpoint_id: 0,
        endpoint_name: 'Unknown',
      });
    }

    return {
      short,
      name,
      online,
      clusters,
      ...(battery && { battery }),
      model_id: fullDev.device_basic_cluster?.model_id,
      manufacturer_name: fullDev.device_basic_cluster?.manufacturer_name,
      _full: fullDev, // для отладки/расширения
    };
  };

  // Загрузка настроек и правил
  useEffect(() => {
    fetch('/api/config')
      .then((r) => r.json())
      .then((data) => {
        setSettings(data);
        setTheme(data.web.theme);
        document.documentElement.setAttribute('class', data.web.theme);
        localStorage.setItem('zigbee-ui-theme', data.web.theme);
      })
      .catch((err) => console.warn('Не удалось загрузить настройки:', err));

    fetch('/api/rules/load')
      .then((r) => r.json())
      .then((data) => setRules(Array.isArray(data) ? data : []))
      .catch((err) => {
        console.warn('Не удалось загрузить правила:', err);
        setRules([]);
      });
  }, []);

  // Переключение темы
  const toggleTheme = () => {
    const newTheme = theme === 'dark' ? 'light' : 'dark';
    setTheme(newTheme);
    setSettings((prev) => ({
      ...prev,
      web: { ...prev.web, theme: newTheme },
    }));
    document.documentElement.setAttribute('class', newTheme);
    localStorage.setItem('zigbee-ui-theme', newTheme);
  };

  // Сохранение настроек
  const saveSettings = () => {
    fetch('/api/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(settings),
    })
      .then((r) => r.json())
      .then((data) => {
        if (data.status === 'ok') {
          alert('✅ Настройки сохранены');
          setShowSettings(false);
        } else {
          alert('❌ Ошибка: ' + data.error);
        }
      })
      .catch(() => {
        alert('❌ Ошибка сети');
      });
  };

  // Отправка команды через WebSocket
  const sendCommand = (cmd, payload = {}) => {
    let ws;
    try {
      ws = new WebSocket(`ws://${window.location.host}/ws`);

      ws.onopen = () => {
        console.log('WebSocket открыто → отправка:', { cmd, ...payload });
        ws.send(JSON.stringify({ cmd, ...payload }));

        setTimeout(() => {
          if (ws.readyState === WebSocket.OPEN) {
            ws.close();
          }
        }, 100);
      };

      ws.onerror = (err) => {
        console.warn(`WebSocket ошибка при отправке ${cmd}:`, err);
      };

    } catch (error) {
      console.error(`Не удалось отправить команду ${cmd}:`, error);
    }
  };

  // ID команд
  const getCommandId = (cmdStr) => {
    const map = {
      OFF: 0x00,
      ON: 0x01,
      TOGGLE: 0x02,
      ON_WITH_TIMED_OFF: 0x42,
    };
    return map[cmdStr];
  };

  // Отправка тестовой команды
  const sendTestCommand = (dev, epId, command) => {
    const cmdId = getCommandId(command.id);
    if (cmdId === undefined) {
      alert('❌ Неизвестная команда');
      return;
    }

    if (command.id === 'ON_WITH_TIMED_OFF') {
      setTimedOffForm({ on_time: 600, off_wait_time: 0, on_off_control: 1 });
      setShowTimedOffModal(true);
      return;
    }

    sendCommand('send_automation_command', {
      cmd: 'send_automation_command',
      short_addr: dev.short,
      endpoint: Number(epId),
      cmd_id: cmdId,
    });
    setShowCommandModal(false);
  };

  // Переключение устройства
  const toggleDevice = (short, endpoint) => {
    sendCommand('toggle', { short, endpoint });
  };

  // Обновление имени устройства
  const updateFriendlyName = (short, newName) => {
    if (!newName?.trim()) return;
    sendCommand('update_friendly_name', { short, name: newName.trim() });
    setDevices((prev) =>
      prev.map((d) => (d.short === short ? { ...d, name: newName.trim() } : d))
    );
  };

  // Обновление имени endpoint
  const updateEndpointName = (short, endpoint, newName) => {
    if (!newName?.trim()) return;
    sendCommand('update_endpoint_name', { short, endpoint, name: newName.trim() });
    setDevices((prev) =>
      prev.map((d) =>
        d.short === short
          ? {
              ...d,
              clusters: d.clusters.map((c) =>
                c.endpoint_id === endpoint ? { ...c, endpoint_name: newName.trim() } : c
              ),
            }
          : d
      )
    );
  };

  // Подключение по WebSocket
  useEffect(() => {
  let websocket = null;
  let reconnectTimeout = null;
  let reconnectAttempts = 0;
  const maxReconnectAttempts = 10;
  const maxReconnectDelay = 10000;

  // === Функция: подключиться к WebSocket ===
  const connectWebSocket = () => {
    if (reconnectAttempts > 0) {
      console.log(`Попытка переподключения (${reconnectAttempts}/${maxReconnectAttempts})...`);
    }

    websocket = new WebSocket(`ws://${window.location.host}/ws`);

    websocket.onopen = () => {
      console.log('✅ WebSocket подключён');
      reconnectAttempts = 0;

      // ❌ Убрали: websocket.send(JSON.stringify({ cmd: 'get_devices' }));
      // Теперь загрузка идёт через fetch, не через WS

      // Отправляем запрос состояния сети
      websocket.send(JSON.stringify({ cmd: 'get_network_status' }));
    };

    websocket.onmessage = (event) => {
      const data = JSON.parse(event.data);

      // 1. Полная замена всех устройств (на всякий случай)
      if (data.devices) {
        const converted = data.devices.map(convertToDeviceCardFormat).filter(Boolean);
        setDevices(converted);
      }

      // 2. Частичное обновление одного устройства
      else if (data.event === 'device_update') {
        const converted = convertToDeviceCardFormat(data);
        if (!converted) return;

        setDevices((prev) => {
          const exists = prev.some((d) => d.short === converted.short);
          if (exists) {
            return prev.map((d) => (d.short === converted.short ? { ...d, ...converted } : d));
          } else {
            return [...prev, converted];
          }
        });
      }

      // 3. Обновление онлайн-статуса
      else if (data.event === 'state_update') {
        setDevices((prev) =>
          prev.map((d) =>
            d.short === data.short ? { ...d, online: data.online } : d
          )
        );
      }

      // 4. Обновление имени
      else if (data.event === 'friendly_name_updated') {
        setDevices((prev) =>
          prev.map((d) => (d.short === data.short ? { ...d, name: data.name } : d))
        );
      }

      // 5. Обновление имени endpoint
      else if (data.event === 'endpoint_name_updated') {
        setDevices((prev) =>
          prev.map((d) =>
            d.short === data.short
              ? {
                  ...d,
                  clusters: d.clusters.map((c) =>
                    c.endpoint_id === data.endpoint_id
                      ? { ...c, endpoint_name: data.name }
                      : c
                  ),
                }
              : d
          )
        );
      }

      // 6. Статус сети Zigbee/WiFi
      else if (data.event === 'network_status') {
        setWifiSSID(data.wifi_ssid || '—');
        setIsNetworkOpen(data.zigbee_open || false);
      }

      // 7. Обновление правил
      else if (data.event === 'rules_updated') {
        fetch('/api/rules/load')
          .then((r) => r.json())
          .then((newRules) => Array.isArray(newRules) && setRules(newRules))
          .catch((err) => console.error('Ошибка при обновлении правил:', err));
      }
    };

    websocket.onclose = () => {
      console.log('WebSocket закрыт. Переподключение...');
      if (reconnectAttempts < maxReconnectAttempts) {
        const delay = Math.min(1000 * 2 ** reconnectAttempts, maxReconnectDelay);
        reconnectAttempts++;
        reconnectTimeout = setTimeout(connectWebSocket, delay);
      }
    };

    websocket.onerror = (err) => {
      console.error('WebSocket ошибка:', err);
      websocket.close();
    };
  };

  // === Загрузка устройств по одному ===
  const loadAllDevices = () => {
    fetch('/api/devices/list')
      .then((r) => r.json())
      .then(async (deviceList) => {
        const devicesArray = [];

        // Загружаем каждое устройство по очереди
        for (const dev of deviceList) {
          try {
            const response = await fetch(`/api/device/${dev.short}`);
            const fullData = await response.json();
            const converted = convertToDeviceCardFormat(fullData);
            if (converted) devicesArray.push(converted);
          } catch (err) {
            console.warn(`Не удалось загрузить устройство ${dev.short}`, err);
          }
        }

        setDevices(devicesArray); // Все загружены → рендерим
      })
      .catch((err) => {
        console.error('Ошибка загрузки списка устройств:', err);
        setDevices([]);
      });
  };

  // === Запуск ===
  loadAllDevices();       // Загружаем устройства
  connectWebSocket();     // Подключаем WebSocket для обновлений

  // === Загрузка привязок ===
  fetch('/api/binding_targets')
    .then((r) => r.json())
    .then((data) => setBindingTargets(data))
    .catch((err) => console.error('Ошибка загрузки устройств для привязки:', err));

  // === Очистка ===
  return () => {
    if (reconnectTimeout) clearTimeout(reconnectTimeout);
    if (websocket) websocket.close();
  };
}, []); // Пустой массив → выполнится один раз //11111111111111111111111111111111111111111111111

  // === Привязка ===
  const performBind = () => {
    const { srcDevice, srcEp, cluster, tgtDevice, tgtEp } = bindForm;
    if (![srcDevice, srcEp, cluster, tgtDevice, tgtEp].every(Boolean)) {
      alert('Заполните все поля');
      return;
    }

    fetch('/api/bind', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        src_addr: Number(srcDevice),
        src_ep: Number(srcEp),
        cluster_id: Number(cluster),
        tgt_addr: Number(tgtDevice),
        tgt_ep: Number(tgtEp),
      }),
    })
      .then((r) => r.json())
      .then((data) => {
        if (data.status === 'ok') {
          alert('✅ Привязка отправлена');
          setShowBindModal(false);
        } else {
          alert('❌ Ошибка: ' + data.error);
        }
      })
      .catch((err) => {
        alert('Ошибка сети: ' + err.message);
      });
  };

  const getInputClusters = (short, epId) => {
    const dev = bindingTargets.find((d) => d.short === short);
    const ep = dev?.endpoints.find((e) => e.id === epId);
    return ep?.input_clusters || [];
  };

  // === Reporting ===
  const handleSetReport = () => {
    const { device, ep, cluster, min, max, change } = reportForm;
    if (![device, ep, cluster].every(Boolean)) {
      alert('Заполните все обязательные поля');
      return;
    }

    fetch('/api/report_config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        short_addr: Number(device),
        endpoint: Number(ep),
        cluster_id: Number(cluster),
        min_interval: Number(min),
        max_interval: Number(max),
        reportable_change: Number(change),
      }),
    })
      .then((r) => r.json())
      .then((data) => {
        if (data.status === 'ok') {
          alert('✅ Reporting настроен');
          setShowReportModal(false);
        } else {
          alert('❌ Ошибка: ' + data.error);
        }
      })
      .catch(() => alert('❌ Ошибка сети'));
  };

  // === Определение типа устройства ===
  const getDeviceType = (device) => {
    const hasOnOff = device.clusters.some((c) => c.type === 'on_off');
    const hasTemp = device.clusters.some((c) => c.type === 'temperature');
    const hasHumidity = device.clusters.some((c) => c.type === 'humidity');

    if (hasOnOff && !hasTemp && !hasHumidity) return 'light';
    if (hasTemp || hasHumidity) return 'sensor';
    if (hasOnOff) return 'switch';
    return 'other';
  };

  // === Фильтрация и сортировка ===
  const filteredAndSortedDevices = devices
    .filter((dev) => {
      const matchesSearch =
        dev.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
        `0x${dev.short.toString(16)}`.includes(searchTerm.toLowerCase());
      const matchesStatus =
        filterStatus === 'all' ||
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
          return a.online === b.online ? 0 : a.online ? -1 : 1;
        default:
          return 0;
      }
    });

  // === Рендер ===
  return (
    <div className="App">
      <header className="App-header">
        {/* Панель управления */}
        <div className="control-panel">
          <div className="network-info">
            <span title="Подключён к Wi-Fi">📶 {wifiSSID}</span>
          </div>

          <button
            className={`network-toggle-btn ${isNetworkOpen ? 'open' : 'closed'}`}
            onClick={() => sendCommand('toggle_network', { duration: 60 })}
          >
            {isNetworkOpen ? '🔓 Сеть открыта' : '🔐 Закрыто'}
          </button>

          <button onClick={() => setShowBindModal(true)} className="bind-btn">
            🔗 Привязка
          </button>

          <button onClick={() => setShowReportModal(true)} className="bind-btn">
            📊 Репорты
          </button>

          <button className="theme-toggle-inline" onClick={toggleTheme}>
            {theme === 'dark' ? '☀️' : '🌙'}
          </button>

          <button className="settings-btn" onClick={() => setShowSettings(!showSettings)} title="Настройки">
            ⚙️
          </button>

          <button className="rules-btn" onClick={() => setShowRules(!showRules)} title="Модуль автоматизации">
            🎯 Сценарии
          </button>
        </div>

        {/* Панель настроек */}
        {showSettings && (
          <div className="settings-panel">
            <div className="settings-card">
              <h3>🔧 Основные</h3>
              <label>
                Язык интерфейса:
                <select
                  value={settings.ha.language}
                  onChange={(e) =>
                    setSettings((prev) => ({
                      ...prev,
                      ha: { ...prev.ha, language: e.target.value },
                    }))
                  }
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
                  onChange={(e) =>
                    setSettings((prev) => ({
                      ...prev,
                      web: { ...prev.web, theme: e.target.value },
                    }))
                  }
                >
                  <option value="dark">Тёмная</option>
                  <option value="light">Светлая</option>
                </select>
              </label>
            </div>

            <div className="settings-card">
              <h3>🏠 Home Assistant</h3>
              <label>
                <input
                  type="checkbox"
                  checked={settings.ha.enabled}
                  onChange={(e) =>
                    setSettings((prev) => ({
                      ...prev,
                      ha: { ...prev.ha, enabled: e.target.checked },
                    }))
                  }
                />
                Включить интеграцию
              </label>
              <label>
                <input
                  type="checkbox"
                  checked={settings.ha.mqtt.enabled}
                  onChange={(e) =>
                    setSettings((prev) => ({
                      ...prev,
                      ha: {
                        ...prev.ha,
                        mqtt: { ...prev.ha.mqtt, enabled: e.target.checked },
                      },
                    }))
                  }
                />
                Включить MQTT
              </label>
              <label>
                Брокер:
                <input
                  type="text"
                  value={settings.ha.mqtt.broker}
                  onChange={(e) =>
                    setSettings((prev) => ({
                      ...prev,
                      ha: {
                        ...prev.ha,
                        mqtt: { ...prev.ha.mqtt, broker: e.target.value },
                      },
                    }))
                  }
                />
              </label>
              <label>
                Порт:
                <input
                  type="number"
                  value={settings.ha.mqtt.port}
                  onChange={(e) =>
                    setSettings((prev) => ({
                      ...prev,
                      ha: {
                        ...prev.ha,
                        mqtt: { ...prev.ha.mqtt, port: Number(e.target.value) },
                      },
                    }))
                  }
                />
              </label>
              <label>
                Логин:
                <input
                  type="text"
                  value={settings.ha.mqtt.username}
                  onChange={(e) =>
                    setSettings((prev) => ({
                      ...prev,
                      ha: {
                        ...prev.ha,
                        mqtt: { ...prev.ha.mqtt, username: e.target.value },
                      },
                    }))
                  }
                />
              </label>
              <label>
                Пароль:
                <input
                  type="password"
                  value={settings.ha.mqtt.password}
                  onChange={(e) =>
                    setSettings((prev) => ({
                      ...prev,
                      ha: {
                        ...prev.ha,
                        mqtt: { ...prev.ha.mqtt, password: e.target.value },
                      },
                    }))
                  }
                />
              </label>
              <label>
                <input
                  type="checkbox"
                  checked={settings.ha.mqtt.discovery}
                  onChange={(e) =>
                    setSettings((prev) => ({
                      ...prev,
                      ha: {
                        ...prev.ha,
                        mqtt: { ...prev.ha.mqtt, discovery: e.target.checked },
                      },
                    }))
                  }
                />
                Авто-дискавери
              </label>
              <label>
                <input
                  type="checkbox"
                  checked={settings.ha.mqtt.availability}
                  onChange={(e) =>
                    setSettings((prev) => ({
                      ...prev,
                      ha: {
                        ...prev.ha,
                        mqtt: { ...prev.ha.mqtt, availability: e.target.checked },
                      },
                    }))
                  }
                />
                Availability
              </label>
            </div>

            <div className="settings-actions">
              <button className="btn-primary" onClick={saveSettings}>
                💾 Сохранить
              </button>
              <button className="btn-danger" onClick={() => setShowSettings(false)}>
                ❌ Отмена
              </button>
            </div>
          </div>
        )}

        <h1>📡 Zigbee Устройства</h1>

        {/* Фильтры */}
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

        <p className="devices-count">
          Найдено: <strong>{filteredAndSortedDevices.length}</strong> из {devices.length}
        </p>

        {filteredAndSortedDevices.length === 0 ? (
          <p className="no-devices">устройств не найдено</p>
        ) : (
          <div className="devices-grid">
            {filteredAndSortedDevices.map((dev) => (
              <DeviceCard
                key={dev.short}
                device={dev}
                onToggleDevice={toggleDevice}
                onUpdateFriendlyName={updateFriendlyName}
                onUpdateEndpointName={updateEndpointName}
                onOpenCommandModal={openCommandModal}
              />
            ))}
          </div>
        )}

                {/* === Модальное окно: Привязка === */}
        <BindModal
          show={showBindModal}
          onClose={() => setShowBindModal(false)}
          bindingTargets={bindingTargets}
          bindForm={bindForm}
          setBindForm={setBindForm}
          performBind={performBind}
        />

        {/* === Модальное окно: Reporting === */}
        <ReportModal
          show={showReportModal}
          onClose={() => setShowReportModal(false)}
          bindingTargets={bindingTargets}
          reportForm={reportForm}
          setReportForm={setReportForm}
          onSubmit={handleSetReport}
        />

        {/* === Модальное окно: Тест команды === */}
        <OnOffCommandModal
          show={showCommandModal}
          onClose={() => setShowCommandModal(false)}
          device={selectedDevice}
          endpoint={selectedEndpoint}
          sendTestCommand={sendTestCommand}
        />

        {/* === Модальное окно: Вкл с таймером === */}
        <OnOffTimedOffModal
          show={showTimedOffModal}
          onClose={() => setShowTimedOffModal(false)}
          device={selectedDevice}
          endpoint={selectedEndpoint}
          timedOffForm={timedOffForm}
          setTimedOffForm={setTimedOffForm}
          getCommandId={getCommandId}
          sendCommand={sendCommand}
        />
      </header>

      {/* === Правила === */}
      {showRules && (
        <div className="rules-section">
          <RuleList
            rules={rules}
            onEdit={(rule) => setEditingRule(rule)}
            onRun={(id) => {
              fetch(`/api/rules/run/${id}`, { method: 'POST' })
                .then((r) => r.json())
                .then((data) => {
                  if (data.status === 'ok') alert('✅ Правило запущено');
                  else alert('❌ Ошибка: ' + data.error);
                })
                .catch(() => alert('❌ Ошибка сети'));
            }}
            onDelete={(id) => {
              fetch(`/api/rules/delete/${id}`, { method: 'DELETE' })
                .then((r) => r.json())
                .then((data) => {
                  if (data.status === 'ok') {
                    setRules((prev) => prev.filter((r) => r.id !== id));
                    alert('✅ Правило удалено');
                  } else {
                    alert('❌ Ошибка: ' + (data.error || 'неизвестная'));
                  }
                })
                .catch(() => alert('❌ Ошибка сети'));
            }}
          />
        </div>
      )}

      {/* Редактор правил */}
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
                  body: JSON.stringify(updatedRule),
                })
                  .then((r) => r.json())
                  .then((data) => {
                    if (data.status === 'ok') {
                      setRules((prev) =>
                        isEdit
                          ? prev.map((r) => (r.id === updatedRule.id ? updatedRule : r))
                          : [...prev, updatedRule]
                      );
                      setEditingRule(null);
                      alert('✅ Правило сохранено');
                    } else {
                      alert('❌ Ошибка: ' + (data.error || 'неизвестная'));
                    }
                  })
                  .catch(() => alert('❌ Ошибка сети'));
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