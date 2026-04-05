import { useState, useEffect } from 'react';

export default function Settings() {
  const [config, setConfig] = useState({
    // Zigbee & WiFi
    pan_id: '',
    radio_channel: '',
    coordinator_name: '',
    hostname: 'esp32-zigbee',
    wifi_mode: 'ap',
    wifi_ap_ssid: 'Zigbee-Gateway-Setup',
    wifi_ap_password: '12345678',
    wifi_sta_ssid: '',
    wifi_sta_password: '',

    // SSDP — только для чтения
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

  useEffect(() => {
    fetch('/api/get/coordinator')
      .then(res => res.json())
      .then(data => {
        setConfig(prev => ({
          ...prev,
          pan_id: data.pan_id || '',
          radio_channel: data.radio_channel || '',
          coordinator_name: data.friendly_name || 'Zigbee Coordinator',
          hostname: data.hostname || 'esp32-zigbee',
          wifi_mode: data.wifi_mode || 'ap',
          wifi_ap_ssid: data.wifi_ap_ssid || prev.wifi_ap_ssid,
          wifi_ap_password: data.wifi_ap_password || prev.wifi_ap_password,
          wifi_sta_ssid: data.wifi_sta_ssid || '',
          wifi_sta_password: data.wifi_sta_password || '',

          // Загружаем SSDP-настройки (только для просмотра)
          ssdp_manufacturer: data.ssdp_manufacturer || prev.ssdp_manufacturer,
          ssdp_model_name: data.ssdp_model_name || prev.ssdp_model_name,
          ssdp_model_number: data.ssdp_model_number || prev.ssdp_model_number,
          ssdp_serial_number: data.ssdp_serial_number || prev.ssdp_serial_number,
          ssdp_server_name: data.ssdp_server_name || prev.ssdp_server_name,
        }));
      })
      .catch(err => {
        setStatus('Ошибка загрузки настроек');
        console.error(err);
      });
  }, []);

  const handleChange = (e) => {
    const { name, value } = e.target;
    if (name === 'hostname') {
      // Удаляем .local, если вдруг введено
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

      // SSDP — только для информации
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

    fetch('/api/post/coordinator', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
    })
      .then(res => res.json())
      .then(data => {
        if (data.success) {
          setStatus('✅ Настройки сохранены');
        } else {
          setStatus('❌ Ошибка: ' + (data.message || 'неизвестная'));
        }
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

  return (
    <div className="settings-page">
      <h2 className="text-2xl font-bold text-white mb-6">⚙️ Настройки системы</h2>

      {status && (
        <div className={`mb-4 p-3 rounded text-sm ${status.includes('Ошибка') ? 'bg-red-900/30 text-red-300' : 'bg-green-900/30 text-green-300'}`}>
          {status}
        </div>
      )}

      <div className="settings-scroll-container">
        <div className="settings-grid">
          
          {/* === Карточка: Zigbee & Wi-Fi === */}
          <div className="panel">
            <div className="panel-header">📶 Параметры сети</div>
            <div className="panel-body">
              <form onSubmit={handleSubmit} className="form-fields">

                {/* Сервер (mDNS) */}
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
                  {/*<p className="form-hint" style={{ gridColumn: 'span 2' }}>
                    Адрес для доступа: <code>http://{config.hostname}.local</code>
                  </p>*/}
                </div>

                {/* PAN ID */}
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

                {/* Radio Channel */}
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

                {/* Имя координатора */}
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

                {/* Текущий режим Wi-Fi */}
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

                {/* Настройки AP */}
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

                {/* Настройки STA */}
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

                {/* Кнопки */}
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

          {/* === Карточка: SSDP (UPnP) === */}
          <div className="panel">
            <div className="panel-header">🌐 Информация о SSDP</div>
            <div className="panel-body">
              <div className="form-fields">
                <div className="form-row">
                  <label className="form-label">Friendly Name</label>
                  <input
                    type="text"
                    value={config.coordinator_name}
                    disabled
                    className="form-input"
                    style={{ background: '#2c2c2c', color: '#999', fontStyle: 'italic' }}
                  />
                </div>

                <div className="form-row">
                  <label className="form-label">Производитель</label>
                  <input
                    type="text"
                    value={config.ssdp_manufacturer}
                    disabled
                    className="form-input"
                    style={{ background: '#2c2c2c', color: '#999', fontStyle: 'italic' }}
                  />
                </div>

                <div className="form-row">
                  <label className="form-label">Модель</label>
                  <input
                    type="text"
                    value={config.ssdp_model_name}
                    disabled
                    className="form-input"
                    style={{ background: '#2c2c2c', color: '#999', fontStyle: 'italic' }}
                  />
                </div>

                <div className="form-row">
                  <label className="form-label">Номер модели</label>
                  <input
                    type="text"
                    value={config.ssdp_model_number}
                    disabled
                    className="form-input"
                    style={{ background: '#2c2c2c', color: '#999', fontStyle: 'italic' }}
                  />
                </div>

                <div className="form-row">
                  <label className="form-label">Серийный номер</label>
                  <input
                    type="text"
                    value={config.ssdp_serial_number}
                    disabled
                    className="form-input"
                    style={{ background: '#2c2c2c', color: '#999', fontStyle: 'italic' }}
                  />
                </div>

                <div className="form-row">
                  <label className="form-label">Server Name</label>
                  <input
                    type="text"
                    value={config.ssdp_server_name}
                    disabled
                    className="form-input"
                    style={{ background: '#2c2c2c', color: '#999', fontStyle: 'italic' }}
                  />
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>

      <div className="settings-footer">
        <p>После сохранения может потребоваться переподключение к точке доступа.</p>
      </div>
    </div>
  );
}