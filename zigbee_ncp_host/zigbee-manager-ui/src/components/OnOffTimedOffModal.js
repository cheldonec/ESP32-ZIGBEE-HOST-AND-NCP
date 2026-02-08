// src/components/OnOffTimedOffModal.js
import React from 'react';

const OnOffTimedOffModal = ({
  show,
  onClose,
  device,
  endpoint,
  timedOffForm,
  setTimedOffForm,
  getCommandId,
  sendCommand,
}) => {
  if (!show || !device) return null;

  return (
    <div className="modal-overlay">
      <div className="modal-content" style={{ maxWidth: '400px' }}>
        <h3>⏱️ Вкл с таймером (On/Off)</h3>
        <p>
          <strong>{device.name}</strong> → EP {endpoint}
        </p>

        <div className="form-group">
          <label>
            ⏱️ Время включения (0.1 сек)
            <input
              type="number"
              value={timedOffForm.on_time}
              onChange={(e) =>
                setTimedOffForm((prev) => ({
                  ...prev,
                  on_time: parseInt(e.target.value) || 0,
                }))
              }
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
              onChange={(e) =>
                setTimedOffForm((prev) => ({
                  ...prev,
                  off_wait_time: parseInt(e.target.value) || 0,
                }))
              }
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
              onChange={(e) =>
                setTimedOffForm((prev) => ({
                  ...prev,
                  on_off_control: parseInt(e.target.value),
                }))
              }
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
                cmd: 'send_automation_command',
                short_addr: device.short,
                endpoint: endpoint,
                cmd_id: getCommandId('ON_WITH_TIMED_OFF'),
                params: {
                  on_time: timedOffForm.on_time,
                  off_wait_time: timedOffForm.off_wait_time,
                },
              };
              sendCommand('send_automation_command', payload);
              onClose();
            }}
          >
            ✅ Отправить
          </button>
          <button className="btn-danger" onClick={onClose}>
            ❌ Отмена
          </button>
        </div>
      </div>
    </div>
  );
};

export default OnOffTimedOffModal;