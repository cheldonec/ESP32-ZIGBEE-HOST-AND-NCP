// src/components/OnOffCommandModal.js
import React from 'react';

const OnOffCommandModal = ({ show, onClose, device, endpoint, sendTestCommand }) => {
  if (!show || !device) return null;

  return (
    <div className="modal-overlay">
      <div className="modal-content" style={{ maxWidth: '500px' }}>
        <h3>🧪 Тест команды On/Off</h3>
        <p>
          <strong>{device.name}</strong> → EP {endpoint}
        </p>

        <div className="command-list">
          {[
            { id: 'ON', label: 'Включить' },
            { id: 'OFF', label: 'Выключить' },
            { id: 'TOGGLE', label: 'Переключить' },
            { id: 'ON_WITH_TIMED_OFF', label: 'Вкл с таймером' },
          ].map((cmd) => (
            <button
              key={cmd.id}
              className="command-item-btn"
              onClick={() => sendTestCommand(device, endpoint, cmd)}
            >
              {cmd.label}
              {cmd.id === 'ON_WITH_TIMED_OFF' && (
                <span style={{ fontSize: '0.8em', opacity: 0.7 }}> → нажмите для настройки</span>
              )}
            </button>
          ))}
        </div>

        <div className="modal-buttons">
          <button className="btn-danger" onClick={onClose}>
            ❌ Закрыть
          </button>
        </div>
      </div>
    </div>
  );
};

export default OnOffCommandModal;