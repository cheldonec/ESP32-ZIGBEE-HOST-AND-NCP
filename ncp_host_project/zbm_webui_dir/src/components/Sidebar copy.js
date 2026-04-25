// src/components/Sidebar.js
import { useState } from 'react';
import { useDevices } from '../hooks/useDevices';
import DeviceSidebar from './DeviceSidebar';

import RulesSidebar from './RulesSidebar';

const SETTINGS_ITEMS = [
  { id: 'network', label: 'Параметры сети', icon: '📶' },
  { id: 'ssdp', label: 'Информация о SSDP', icon: '🌐' },
  { id: 'variables', label: 'Переменные', icon: '🔢' },
];

export default function Sidebar({ currentTab, selectedItem, onSelectItem }) {
  const [editingIEEE, setEditingIEEE] = useState(null);
  const { devices } = useDevices();

  // Краткий список для UI
  const briefDevices = devices.map(dev => ({
    ieee: dev.ieee_addr,
    short: parseInt(dev.short_addr.replace('0x', ''), 16),
    friendly_name: dev.name || dev.friendly_name,
    online: dev.is_online,
    linkquality: dev.lqi
  }));

  const handleSelectDevice = (dev) => {
    onSelectItem('device', dev.ieee);
  };

  const handleStartEdit = (e, dev) => {
    e.stopPropagation();
    setEditingIEEE(dev.ieee);
  };

  const handleSaveName = async (dev, newName) => {
    try {
      await fetch('/api/device/update_friendly_name', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          ieee_addr: dev.ieee,
          friendly_name: newName.trim() || ''
        })
      });
    } catch (err) {
      console.error('Ошибка переименования:', err);
    } finally {
      setEditingIEEE(null);
    }
  };

  if (currentTab === '/' || currentTab === '') {
    return (
      <div className="device-list">
        <h2 className="sidebar-header">📱 Устройства</h2>
        <nav className="device-list-content">
          {briefDevices.length === 0 ? (
            <p className="device-list-empty">Нет устройств</p>
          ) : (
            briefDevices.map((dev) => (
              <div
                key={dev.ieee}
                onClick={() => handleSelectDevice(dev)}
                className={`device-item ${selectedItem?.type === 'device' && selectedItem.id === dev.ieee ? 'selected' : ''}`}
              >
                <div className="device-item-content">
                  <div className="device-info">
                    {editingIEEE === dev.ieee ? (
                      <input
                        autoFocus
                        defaultValue={dev.friendly_name}
                        onBlur={(e) => handleSaveName(dev, e.target.value)}
                        onKeyDown={(e) => e.key === 'Enter' && handleSaveName(dev, e.target.value)}
                        className="form-input text-sm px-2 py-1 h-6"
                        style={{ fontSize: '12px' }}
                        onClick={(e) => e.stopPropagation()}
                      />
                    ) : (
                      <div className="device-text">
                        <div className="device-name">
                          {dev.friendly_name || <span className="text-gray-500 italic">Без имени</span>}
                          <button
                            onClick={(e) => handleStartEdit(e, dev)}
                            className="edit-button"
                            title="Переименовать"
                          >
                            ✏️
                          </button>
                        </div>
                        <div className="device-meta">
                          0x{dev.short.toString(16).toUpperCase().padStart(4, '0')} • LQI: {dev.linkquality || '?'}
                        </div>
                      </div>
                    )}
                  </div>
                  <span className={`device-status ${dev.online ? 'status-online' : 'status-offline'}`}>
                    {dev.online ? 'Online' : 'Offline'}
                  </span>
                </div>
              </div>
            ))
          )}
        </nav>
      </div>
    );
  }

  if (currentTab === '/scenes') {
    const scenes = [
      { id: 'scene_1', name: 'Вечернее освещение', active: true },
      { id: 'scene_2', name: 'Киноночь', active: false },
      { id: 'scene_3', name: 'Выключение всех', active: true },
    ];

    return (
      <div className="device-list">
        <h2 className="sidebar-header">🎬 Сценарии</h2>
        <nav className="device-list-content">
          {scenes.map((scene) => (
            <div
              key={scene.id}
              onClick={() => onSelectItem('scene', scene.id)}
              className={`device-item ${selectedItem?.type === 'scene' && selectedItem.id === scene.id ? 'selected' : ''}`}
            >
              <div className="device-item-content">
                <div className="device-text">
                  <div className="device-name">{scene.name}</div>
                  <div className="device-meta">{scene.active ? 'Активен' : 'Неактивен'}</div>
                </div>
                <span className={`device-status ${scene.active ? 'status-online' : 'status-offline'}`}>
                  {scene.active ? 'On' : 'Off'}
                </span>
              </div>
            </div>
          ))}
        </nav>
      </div>
    );
  }

  if (currentTab === '/rules') {
    return (
      <RulesSidebar
        selectedRuleId={selectedItem?.id}
        onSelect={onSelectItem}
        onAddRule={(newId) => {
          console.log('🆕 Создано новое правило:', newId);
          // Здесь можно отправить запрос на сервер
        }}
      />
    );
  }
  
  if (currentTab === '/settings') {
    return (
      <div className="device-list">
        <h2 className="sidebar-header">⚙️ Настройки</h2>
        <nav className="device-list-content">
          {SETTINGS_ITEMS.map((item) => (
            <div
              key={item.id}
              onClick={() => onSelectItem('settings', item.id)}
              className={`device-item ${selectedItem?.type === 'settings' && selectedItem.id === item.id ? 'selected' : ''}`}
            >
              <div className="device-item-content">
                <div className="device-text">
                  <div className="device-name">{item.icon} {item.label}</div>
                </div>
              </div>
            </div>
          ))}
        </nav>
      </div>
    );
  }
  
  // По умолчанию — пусто
  return (
    <div className="device-list">
      <h2 className="sidebar-header">📋 {currentTab}</h2>
      <p className="p-4 text-gray-500">Контент не реализован</p>
    </div>
  );
}