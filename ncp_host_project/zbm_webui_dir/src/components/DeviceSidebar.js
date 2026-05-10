// src/components/DeviceSidebar.js
import { useState } from 'react';

export default function DeviceSidebar({ devices, selectedIEEE, onSelect }) {
  const [editingIEEE, setEditingIEEE] = useState(null);
  const [editValue, setEditValue] = useState('');

  const handleStartEdit = (e, dev) => {
    e.stopPropagation();
    setEditingIEEE(dev.ieee);
    setEditValue(dev.friendly_name || '');
  };

  const handleSave = async (dev) => {
    try {
      await api.updateFriendlyName(dev.ieee, editValue.trim() || '');
      // ✅ Успешно
    } catch (err) {
      alert(`Сеть недоступна: ${err.message}`);
    } finally {
      setEditingIEEE(null);
    }
  };

  const handleKeyDown = (e, dev) => {
    if (e.key === 'Enter') {
      handleSave(dev);
    } else if (e.key === 'Escape') {
      setEditingIEEE(null);
    }
  };

  return (
    <div className="device-list">
      <h2 className="sidebar-header">📱 Устройства</h2>
      <nav className="device-list-content">
        {devices.length === 0 ? (
          <p className="device-list-empty">Нет подключённых устройств</p>
        ) : (
          devices.map((dev) => (
            <div
              key={dev.ieee}
              onClick={() => {
                // Если сейчас идёт редактирование — отменяем без сохранения
                if (editingIEEE) {
                  setEditingIEEE(null);
                }
                onSelect(dev);
              }}
              className={`device-item ${dev.ieee === selectedIEEE ? 'selected' : ''}`}
            >
              <div className="device-item-content">
                <div className="device-info">
                  {/* Режим редактирования */}
                  {editingIEEE === dev.ieee ? (
                    <div className="flex items-center gap-1 flex-1">
                      <input
                        type="text"
                        value={editValue}
                        onChange={(e) => setEditValue(e.target.value)}
                        onKeyDown={(e) => handleKeyDown(e, dev)}
                        onBlur={() => handleSave(dev)}
                        autoFocus
                        className="form-input text-sm px-2 py-1 h-6 flex-1"
                        style={{ fontSize: '12px' }}
                        onClick={(e) => e.stopPropagation()}
                      />
                    </div>
                  ) : (
                    <div className="device-text">
                      <div className="device-name">
                        {dev.friendly_name || <span className="text-gray-500 italic">Без имени</span>}
                        <button
                          onClick={(e) => handleStartEdit(e, dev)}
                          className="edit-button"
                          title="Переименовать"
                          aria-label="Переименовать устройство"
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