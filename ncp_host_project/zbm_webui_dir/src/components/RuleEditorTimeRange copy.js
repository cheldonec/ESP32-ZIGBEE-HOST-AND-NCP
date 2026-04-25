// src/components/RuleEditorTimeRange.js
import { useState } from 'react';

const DAYS = [
  { value: 0, label: 'Пн' },
  { value: 1, label: 'Вт' },
  { value: 2, label: 'Ср' },
  { value: 3, label: 'Чт' },
  { value: 4, label: 'Пт' },
  { value: 5, label: 'Сб' },
  { value: 6, label: 'Вс' },
];

export default function RuleEditorTimeRange({ timeRange, onChange }) {
  const actualTimeRange = timeRange || {
    enabled: false,
    from: '00:00',
    to: '23:59',
    days: []
  };

  const [expanded, setExpanded] = useState(true);

  const toggleDay = (dayValue) => {
    const newDays = actualTimeRange.days.includes(dayValue)
      ? actualTimeRange.days.filter(d => d !== dayValue)
      : [...actualTimeRange.days, dayValue].sort((a, b) => a - b);
    onChange({ ...actualTimeRange, days: newDays });
  };

  const selectAllDays = () => {
    onChange({ ...timeRange, days: [0, 1, 2, 3, 4, 5, 6] });
  };

  const clearDays = () => {
    onChange({ ...timeRange, days: [] });
  };

  return (
    <div className="panel mb-6">
      <div
        className="panel-header cursor-pointer"
        onClick={() => setExpanded(!expanded)}
      >
        ⏰ Временные ограничения
        <span className="text-xs ml-2 opacity-70">
          {expanded ? '▲ Свернуть' : '▼ Развернуть'}
        </span>
      </div>

      {expanded && (
        <div className="panel-body space-y-4">
          {/* Активно */}
          <div className="form-row">
            <label className="form-label">Ограничить по времени</label>
            <label className="flex items-center gap-2 cursor-pointer">
              <input
                type="checkbox"
                checked={timeRange.enabled}
                onChange={(e) =>
                  onChange({ ...timeRange, enabled: e.target.checked })
                }
                className="form-checkbox"
              />
              <span>{timeRange.enabled ? 'Да' : 'Нет'}</span>
            </label>
          </div>

          {/* Время: from/to */}
          {timeRange.enabled && (
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4 ml-4">
              <div className="form-row">
                <label className="form-label">С</label>
                <input
                  type="time"
                  value={timeRange.from}
                  onChange={(e) =>
                    onChange({ ...timeRange, from: e.target.value })
                  }
                  className="form-input"
                />
              </div>
              <div className="form-row">
                <label className="form-label">По</label>
                <input
                  type="time"
                  value={timeRange.to}
                  onChange={(e) =>
                    onChange({ ...timeRange, to: e.target.value })
                  }
                  className="form-input"
                />
              </div>
            </div>
          )}

          {/* Дни недели */}
          {timeRange.enabled && (
            <div className="ml-4">
              <div className="form-row mb-2">
                <label className="form-label">Дни</label>
                <div className="flex gap-2 flex-wrap">
                  {DAYS.map(({ value, label }) => (
                    <button
                      key={value}
                      type="button"
                      onClick={() => toggleDay(value)}
                      className={`w-8 h-8 text-xs font-medium rounded-full transition ${
                        timeRange.days.includes(value)
                          ? 'bg-blue-600 text-white'
                          : 'bg-gray-700 text-gray-300 hover:bg-gray-600'
                      }`}
                    >
                      {label}
                    </button>
                  ))}
                </div>
              </div>
              <div className="flex gap-2 ml-2">
                <button
                  type="button"
                  onClick={selectAllDays}
                  className="text-xs px-2 py-1 bg-gray-700 hover:bg-gray-600 rounded"
                >
                  Все
                </button>
                <button
                  type="button"
                  onClick={clearDays}
                  className="text-xs px-2 py-1 bg-gray-700 hover:bg-gray-600 rounded"
                >
                  Очистить
                </button>
              </div>
            </div>
          )}

          {!timeRange.enabled && (
            <p className="text-sm text-gray-500 ml-4">
              Правило активно в любое время. Настройки сохранены.
            </p>
          )}
        </div>
      )}
    </div>
  );
}