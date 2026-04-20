// src/components/BehaviorsPanel.js
import { useState } from 'react';

export default function BehaviorsPanel({ sceneId }) {
  const [sceneName, setSceneName] = useState(`Сцена ${sceneId.split('_')[1]}`);
  const [devices, setDevices] = useState([
    { id: 'device_1', name: 'Реле на столе', onOff: true, brightness: 100, colorTemp: 3500 },
    { id: 'device_2', name: 'Лампа в углу', onOff: false, brightness: 50, colorTemp: 4000 },
  ]);

  const handleDeviceChange = (deviceId, field, value) => {
    setDevices(prev =>
      prev.map(dev =>
        dev.id === deviceId ? { ...dev, [field]: typeof value === 'string' ? parseInt(value, 10) : value } : dev
      )
    );
  };

  const handleSave = () => {
    alert(`Сцена "${sceneName}" сохранена!`);
    console.log('Saved scene:', { sceneId, sceneName, devices });
  };

  return (
    <div className="p-6">
      <h2 className="text-lg font-semibold text-white mb-4">🎬 Редактирование сцены</h2>

      <div className="panel mb-6">
        <div className="panel-header">🔧 Настройки сцены</div>
        <div className="panel-body">
          <div className="form-row">
            <label className="form-label">Название</label>
            <input
              type="text"
              value={sceneName}
              onChange={(e) => setSceneName(e.target.value)}
              className="form-input"
            />
          </div>
        </div>
      </div>

      <div className="panel">
        <div className="panel-header">💡 Устройства в сцене</div>
        <div className="panel-body space-y-4">
          {devices.map((dev) => (
            <div key={dev.id} className="bg-gray-800/40 p-4 rounded border border-gray-700">
              <h4 className="text-sm font-medium text-gray-300 mb-3">{dev.name}</h4>

              <div className="space-y-3 text-xs">
                <div className="flex items-center gap-4">
                  <label className="w-20 text-gray-400">Включено</label>
                  <input
                    type="checkbox"
                    checked={dev.onOff}
                    onChange={(e) => handleDeviceChange(dev.id, 'onOff', e.target.checked)}
                    className="form-checkbox"
                  />
                </div>

                <div className="flex items-center gap-4">
                  <label className="w-20 text-gray-400">Яркость</label>
                  <input
                    type="range"
                    min="0"
                    max="100"
                    value={dev.brightness}
                    onChange={(e) => handleDeviceChange(dev.id, 'brightness', e.target.value)}
                    className="flex-1"
                  />
                  <span className="text-gray-300 w-8">{dev.brightness}%</span>
                </div>

                <div className="flex items-center gap-4">
                  <label className="w-20 text-gray-400">Цвет, К</label>
                  <input
                    type="range"
                    min="2000"
                    max="6500"
                    step="100"
                    value={dev.colorTemp}
                    onChange={(e) => handleDeviceChange(dev.id, 'colorTemp', e.target.value)}
                    className="flex-1"
                  />
                  <span className="text-gray-300 w-12">{dev.colorTemp}K</span>
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>

      <div className="mt-6 flex justify-end">
        <button onClick={handleSave} className="btn-primary">
          Сохранить сцену
        </button>
      </div>
    </div>
  );
}