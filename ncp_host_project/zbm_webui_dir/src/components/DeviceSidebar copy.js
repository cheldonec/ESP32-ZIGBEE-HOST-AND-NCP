// src/components/DeviceSidebar.js
import { useState } from 'react';
export default function DeviceSidebar({ devices, selectedIEEE, onSelect }) {
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
              onClick={() => onSelect(dev)}
              className={`device-item ${dev.ieee === selectedIEEE ? 'selected' : ''}`}
            >
              <div className="device-item-content">
                <div className="device-info">
                  <span className="device-status-icon">{dev.online ? '🟢' : '🔴'}</span>
                  <div className="device-text">
                    <div className="device-name">{dev.friendly_name}</div>
                    <div className="device-meta">
                      0x{dev.short.toString(16).toUpperCase().padStart(4, '0')} • LQI: {dev.linkquality || '?'}
                    </div>
                  </div>
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