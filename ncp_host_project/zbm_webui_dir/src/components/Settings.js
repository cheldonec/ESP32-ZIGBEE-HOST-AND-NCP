import { useState, useEffect, useMemo } from 'react';
import { useCoordinator } from '../hooks/useCoordinator';
import { api } from '../api/httpClient';

export default function Settings({ activeSection = 'network', reloadVariables }) {
  const [config, setConfig] = useState({
    pan_id: '',
    radio_channel: '',
    coordinator_name: '',
    hostname: 'esp32-zigbee',
    wifi_mode: 'ap',
    wifi_ap_ssid: 'Zigbee-Gateway-Setup',
    wifi_ap_password: '12345678',
    wifi_sta_ssid: '',
    wifi_sta_password: '',

    ssdp_manufacturer: 'CheldonecCo',
    ssdp_model_name: 'Zigbee NCP Host',
    ssdp_model_number: '1.0',
    ssdp_serial_number: '00000001',
    ssdp_server_name: 'Linux/ESP32 UPnP/1.1 ZBM-GW/1.0',
  });

  const [status, setStatus] = useState('');
  const [isSaving, setIsSaving] = useState(false);
  const [showApPassword, setShowApPassword] = useState(false);
  const [showStaPassword, setShowStaPassword] = useState(false);

  // Загрузка конфигурации координатора
  useEffect(() => {
    const loadConfig = async () => {
      try {
        const coordRes = await fetch('/api/get/coordinator');
        const coordData = await coordRes.json();

        setConfig(prev => ({
          ...prev,
          pan_id: coordData.pan_id || '',
          radio_channel: coordData.radio_channel || '',
          coordinator_name: coordData.friendly_name || 'Zigbee Coordinator',
          hostname: coordData.hostname || 'esp32-zigbee',
          wifi_mode: coordData.wifi_mode || 'ap',
          wifi_ap_ssid: coordData.wifi_ap_ssid || prev.wifi_ap_ssid,
          wifi_ap_password: coordData.wifi_ap_password || prev.wifi_ap_password,
          wifi_sta_ssid: coordData.wifi_sta_ssid || '',
          wifi_sta_password: coordData.wifi_sta_password || '',

          ssdp_manufacturer: coordData.ssdp_manufacturer || prev.ssdp_manufacturer,
          ssdp_model_name: coordData.ssdp_model_name || prev.ssdp_model_name,
          ssdp_model_number: coordData.ssdp_model_number || prev.ssdp_model_number,
          ssdp_serial_number: coordData.ssdp_serial_number || prev.ssdp_serial_number,
          ssdp_server_name: coordData.ssdp_server_name || prev.ssdp_server_name,
        }));
      } catch (err) {
        setStatus('Ошибка загрузки координатора');
        console.error(err);
      }
    };

    loadConfig();
  }, []);

  const handleChange = (e) => {
    const { name, value } = e.target;
    if (name === 'hostname') {
      const clean = value.replace(/\.local$/i, '');
      setConfig(prev => ({ ...prev, [name]: clean }));
    } else {
      setConfig(prev => ({ ...prev, [name]: value }));
    }
  };

  const handleSubmit = (e) => {
    e.preventDefault();
    setIsSaving(true);
    setStatus('Сохраняем...');

    const payload = {
      pan_id: parseInt(config.pan_id, 10),
      radio_channel: parseInt(config.radio_channel, 10),
      friendly_name: config.coordinator_name,
      hostname: config.hostname,
      wifi_mode: config.wifi_mode,
      wifi_ap_ssid: config.wifi_ap_ssid,
      wifi_ap_password: config.wifi_ap_password,
      wifi_sta_ssid: config.wifi_sta_ssid,
      wifi_sta_password: config.wifi_sta_password,
      ssdp_manufacturer: config.ssdp_manufacturer,
      ssdp_model_name: config.ssdp_model_name,
      ssdp_model_number: config.ssdp_model_number,
      ssdp_serial_number: config.ssdp_serial_number,
      ssdp_server_name: config.ssdp_server_name,
    };

    if (isNaN(payload.pan_id) || payload.pan_id < 0 || payload.pan_id > 65535) {
      setStatus('❌ PAN ID должен быть числом от 0 до 65535');
      setIsSaving(false);
      return;
    }

    if (isNaN(payload.radio_channel) || payload.radio_channel < 11 || payload.radio_channel > 26) {
      setStatus('❌ Канал должен быть от 11 до 26');
      setIsSaving(false);
      return;
    }

    api.updateCoordinator(payload)
      .then(() => {
        setStatus('✅ Настройки сохранены');
      })
      .catch(err => {
        setStatus('❌ Не удалось сохранить');
        console.error(err);
      })
      .finally(() => {
        setIsSaving(false);
      });
    };

  const handleReboot = () => {
    if (window.confirm('Перезагрузить устройство?')) {
      fetch('/api/post/coordinator', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ action: 'reboot' }),
      }).then(() => {
        alert('Устройство перезагружается...');
      });
    }
  };

  // === Переменные (внутри Settings.js) ===
  const VariablesSettings = () => {
    const { variables } = useCoordinator(); // ← берём из хука
    const [localVars, setLocalVars] = useState([]);
    const [savingIdx, setSavingIdx] = useState(null); // индекс переменной, которая сохраняется

    const VAR_TYPES = [
      { value: 0x20, label: 'uint8' },
      { value: 0x28, label: 'int8' },
      { value: 0x21, label: 'uint16' },
      { value: 0x29, label: 'int16' },
      { value: 0x42, label: 'char_string' },
      { value: 0x44, label: 'long_char_string' },
    ];

    // Инициализация локального состояния
    useEffect(() => {
      if (variables.length > 0) {
        setLocalVars(prev => {
          const map = new Map(prev.map(v => [v.idx, v]));
          return variables.map(v => {
            const current = map.get(v.idx);
            return {
              ...v,
              name: current ? current.name : v.name,
              init_value: current ? current.init_value : v.init_value,
            };
          });
        });
      }
    }, [variables]);

    const handleNameChange = (idx, value) => {
      setLocalVars(prev => prev.map(v => v.idx === idx ? { ...v, name: value } : v));
    };

    const handleTypeChange = (idx, value) => {
      const numValue = Number(value);
      setLocalVars(prev => prev.map(v => v.idx === idx ? { ...v, type: numValue } : v));
    };

    const handleValueChange = (idx, value) => {
      setLocalVars(prev => prev.map(v => v.idx === idx ? { ...v, value } : v));
    };

    const handleInitValueChange = (idx, value) => {
      setLocalVars(prev => prev.map(v => v.idx === idx ? { ...v, init_value: value } : v));
    };

    // === Сохранение ОДНОЙ переменной ===
    const saveSingle = async (v) => {
      setSavingIdx(v.idx);

      let initValueToSend;
      if (v.type === 0x42 || v.type === 0x44) {
        initValueToSend = v.init_value;
      } else {
        const num = Number(v.init_value);
        if (isNaN(num)) {
          setSavingIdx(null);
          return alert('Неверное начальное значение');
        }
        initValueToSend = num;
      }

      const payload = {
        name: v.name,
        type: v.type,
        init_value: initValueToSend,
      };

      try {
        await api.updateVariable(v.idx, payload);
        setStatus('✅ Переменная сохранена');
        reloadVariables(); // ← принудительно обновляем список
      } catch (err) {
        console.error('Ошибка сохранения:', err);
        setStatus('❌ Не удалось сохранить');
      } finally {
        setSavingIdx(null);
      }
    };

    return (
      <div className="panel">
        <div className="panel-header">🔢 Переменные</div>
        <div className="panel-body">
          <table className="w-full text-sm">
            <thead>
              <tr>
                <th className="text-left py-2 text-gray-400">Имя</th>
                <th className="text-left py-2 text-gray-400">Тип</th>
                <th className="text-left py-2 text-gray-400">Значение при старте</th>
                <th className="text-left py-2 text-gray-400">Текущее значение</th>
                <th className="text-left py-2 text-gray-400">Действие</th>
              </tr>
            </thead>
            <tbody>
              {localVars.map((v) => (
                <tr key={v.idx}>
                  <td className="py-1">
                    <input
                      type="text"
                      value={v.name}
                      onChange={(e) => handleNameChange(v.idx, e.target.value)}
                      placeholder={`var_${v.idx}`}
                      className="form-input text-xs px-2 py-1 h-6"
                      style={{ fontSize: '11px', padding: '1px 4px' }}
                    />
                  </td>
                  <td className="py-1 text-gray-500">
                    <select
                      value={v.type}
                      onChange={(e) => handleTypeChange(v.idx, e.target.value)}
                      className="form-input text-xs px-2 py-1 h-6"
                      style={{ fontSize: '11px', padding: '1px 4px' }}
                    >
                      {VAR_TYPES.map(({ value, label }) => (
                        <option key={value} value={value}>{label}</option>
                      ))}
                    </select>
                  </td>
                  <td className="py-1">
                    <input
                      type="text"
                      value={v.init_value ?? ''}
                      onChange={(e) => handleInitValueChange(v.idx, e.target.value)}
                      placeholder="начальное значение"
                      className="form-input text-xs px-2 py-1 h-6"
                      style={{ fontSize: '11px', padding: '1px 4px', width: '100px' }}
                    />
                  </td>
                  <td className="py-1">
                    <input
                      type="text"
                      value={v.value ?? ''}
                      onChange={(e) => handleValueChange(v.idx, e.target.value)}
                      placeholder="текущее значение"
                      className="form-input text-xs px-2 py-1 h-6"
                      style={{ fontSize: '11px', padding: '1px 4px' }}
                    />
                  </td>
                  <td className="py-1">
                    <button
                      onClick={() => saveSingle(v)}
                      disabled={savingIdx === v.idx}
                      className="btn-icon"
                      title="Сохранить переменную"
                      style={{ padding: '2px 6px', fontSize: '14px' }}
                    >
                      {savingIdx === v.idx ? '⏳' : '💾'}
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>

          <div className="mt-4 text-xs text-gray-500">
            Изменения сохраняются по одной переменной.
          </div>
        </div>
      </div>
    );
  };

  // Оптимизация: не пересоздаём компонент при каждом рендере
  const variablesSettingsElement = useMemo(() => <VariablesSettings />, []);

  const renderSection = () => {
    if (activeSection === 'network') {
      return (
        <div className="panel">
          <div className="panel-header">📶 Параметры сети</div>
          <div className="panel-body">
            <form onSubmit={handleSubmit} className="form-fields">
              <div className="form-row">
                <label className="form-label">Сервер</label>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flex: 1 }}>
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <input
                      type="text"
                      name="hostname"
                      value={config.hostname}
                      onChange={handleChange}
                      placeholder="Например: zigbee-gw"
                      className="form-input"
                      style={{ width: '100%', minWidth: 0 }}
                    />
                  </div>
                  <span style={{ whiteSpace: 'nowrap', fontSize: '11px', color: '#666' }}>
                    <code>http://{config.hostname}.local</code>
                  </span>
                </div>
              </div>
              <div className="form-row">
                <label className="form-label">PAN ID</label>
                <input
                  type="number"
                  name="pan_id"
                  value={config.pan_id}
                  onChange={handleChange}
                  min="0"
                  max="65535"
                  className="form-input"
                  required
                />
              </div>
              <div className="form-row">
                <label className="form-label">Канал Zigbee</label>
                <select
                  name="radio_channel"
                  value={config.radio_channel}
                  onChange={handleChange}
                  className="form-input"
                  required
                >
                  {[11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26].map(ch => (
                    <option key={ch} value={ch}>Канал {ch}</option>
                  ))}
                </select>
              </div>
              <div className="form-row">
                <label className="form-label">Имя координатора</label>
                <input
                  type="text"
                  name="coordinator_name"
                  value={config.coordinator_name}
                  onChange={handleChange}
                  placeholder="Например: Main Coordinator"
                  className="form-input"
                />
              </div>
              <div className="form-row">
                <label className="form-label">Режим Wi-Fi</label>
                <div className="form-static-value">
                  <div className="form-static-text">
                    {config.wifi_mode === 'ap' && 'Точка доступа (AP)'}
                    {config.wifi_mode === 'sta' && 'Клиент (STA)'}
                    {config.wifi_mode === 'ap+sta' && 'AP + STA'}
                    {!['ap', 'sta', 'ap+sta'].includes(config.wifi_mode) && 'Неизвестно'}
                  </div>
                </div>
              </div>
              <div className="form-row">
                <label className="form-label">Настройки AP</label>
                <div className="form-ap-info">
                  <div>
                    <input
                      type="text"
                      name="wifi_ap_ssid"
                      value={config.wifi_ap_ssid}
                      onChange={handleChange}
                      placeholder="Например: Zigbee-Hotspot"
                      className="form-input"
                    />
                  </div>
                  <div>
                    <input
                      type={showApPassword ? 'text' : 'password'}
                      name="wifi_ap_password"
                      value={config.wifi_ap_password}
                      onChange={handleChange}
                      placeholder="••••••••"
                      className="form-input"
                    />
                    <button
                      type="button"
                      onClick={() => setShowApPassword(prev => !prev)}
                      aria-label={showApPassword ? 'Скрыть пароль' : 'Показать пароль'}
                    >
                      👁️
                    </button>
                  </div>
                </div>
              </div>
              <div className="form-row">
                <label className="form-label">Настройки STA</label>
                <div className="form-ap-info">
                  <div>
                    <input
                      type="text"
                      name="wifi_sta_ssid"
                      value={config.wifi_sta_ssid}
                      onChange={handleChange}
                      placeholder="Например: Home-WiFi"
                      className="form-input"
                    />
                  </div>
                  <div>
                    <input
                      type={showStaPassword ? 'text' : 'password'}
                      name="wifi_sta_password"
                      value={config.wifi_sta_password}
                      onChange={handleChange}
                      placeholder="••••••••"
                      className="form-input"
                    />
                    <button
                      type="button"
                      onClick={() => setShowStaPassword(prev => !prev)}
                      aria-label={showStaPassword ? 'Скрыть пароль' : 'Показать пароль'}
                    >
                      👁️
                    </button>
                  </div>
                </div>
              </div>
              <div className="form-actions">
                <button
                  type="submit"
                  disabled={isSaving}
                  className={`btn-primary ${isSaving ? 'btn-disabled' : ''}`}
                >
                  {isSaving ? 'Сохранение...' : 'Сохранить'}
                </button>
                <button type="button" onClick={handleReboot} className="btn-danger">
                  Перезагрузить
                </button>
              </div>
            </form>
          </div>
        </div>
      );
    }

    if (activeSection === 'ssdp') {
      return (
        <div className="panel">
          <div className="panel-header">🌐 Информация о SSDP</div>
          <div className="panel-body">
            <div className="form-fields">
              <div className="form-row">
                <label className="form-label">Имя в сети</label>
                <input type="text" value={config.coordinator_name} disabled className="form-input" />
              </div>
              <div className="form-row">
                <label className="form-label">Производитель</label>
                <input type="text" value={config.ssdp_manufacturer} disabled className="form-input" />
              </div>
              <div className="form-row">
                <label className="form-label">Модель</label>
                <input type="text" value={config.ssdp_model_name} disabled className="form-input" />
              </div>
              <div className="form-row">
                <label className="form-label">Номер модели</label>
                <input type="text" value={config.ssdp_model_number} disabled className="form-input" />
              </div>
              <div className="form-row">
                <label className="form-label">Серийный номер</label>
                <input type="text" value={config.ssdp_serial_number} disabled className="form-input" />
              </div>
              <div className="form-row">
                <label className="form-label">Server Name</label>
                <input type="text" value={config.ssdp_server_name} disabled className="form-input" />
              </div>
            </div>
          </div>
        </div>
      );
    }

    if (activeSection === 'variables') {
      return variablesSettingsElement;
    }

    return <div>Раздел не найден</div>;
  };

  return (
    <div className="settings-page">
      {status && (
        <div className={`mb-4 p-3 rounded text-sm ${status.includes('Ошибка') ? 'bg-red-900/30 text-red-300' : 'bg-green-900/30 text-green-300'}`}>
          {status}
        </div>
      )}
      {renderSection()}
      <div className="settings-footer">
        <p>После сохранения может потребоваться переподключение к точке доступа.</p>
      </div>
    </div>
  );
}