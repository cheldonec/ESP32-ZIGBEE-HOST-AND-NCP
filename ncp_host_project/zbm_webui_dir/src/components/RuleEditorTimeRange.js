// RuleEditorTimeRange.js
import { useState, useEffect } from 'react';

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

  useEffect(() => {
    console.log('⏱ Current selected days:', actualTimeRange.days);
  }, [actualTimeRange.days]);

  const toggleDay = (dayValue) => {
    const newDays = actualTimeRange.days.includes(dayValue)
      ? actualTimeRange.days.filter(d => d !== dayValue)
      : [...actualTimeRange.days, dayValue].sort((a, b) => a - b);

    onChange({ ...actualTimeRange, days: newDays });
  };

  const selectAllDays = () => {
    onChange({ ...actualTimeRange, days: [0, 1, 2, 3, 4, 5, 6] });
  };

  const clearDays = () => {
    onChange({ ...actualTimeRange, days: [] });
  };

  return (
    <div className="panel mb-6">
      <div className="panel-header cursor-pointer" onClick={() => setExpanded(!expanded)}>
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
                checked={actualTimeRange.enabled}
                onChange={(e) =>
                  onChange({ ...actualTimeRange, enabled: e.target.checked })
                }
                className="form-checkbox"
              />
              <span>{actualTimeRange.enabled ? 'Да' : 'Нет'}</span>
            </label>
          </div>

          {/* Время */}
          {actualTimeRange.enabled && (
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4 ml-4">
              <div className="form-row">
                <label className="form-label">С</label>
                <input
                  type="time"
                  value={actualTimeRange.from}
                  onChange={(e) =>
                    onChange({ ...actualTimeRange, from: e.target.value })
                  }
                  className="form-input"
                />
              </div>
              <div className="form-row">
                <label className="form-label">По</label>
                <input
                  type="time"
                  value={actualTimeRange.to}
                  onChange={(e) =>
                    onChange({ ...actualTimeRange, to: e.target.value })
                  }
                  className="form-input"
                />
              </div>
            </div>
          )}

          {/* Дни и сообщение */}
          {actualTimeRange.enabled || !actualTimeRange.enabled ? (
            <div>
              {/* Дни */}
              {actualTimeRange.enabled && (
                <div className="ml-4 mt-2">
                  <div className="form-row mb-2">
                    <label className="form-label">Дни</label>
                    <div style={{
                        display: 'flex',
                        flexWrap: 'wrap',
                        gap: '8px',
                        alignItems: 'center'
                        }}>
                        {DAYS.map(({ value, label }) => (
                            <button
                            key={value}
                            type="button"
                            onClick={() => toggleDay(value)}
                            style={{
                                width: '36px',
                                height: '36px',
                                fontSize: '12px',
                                fontWeight: '600',
                                borderRadius: '50%',
                                border: '1px solid #4B5563',
                                backgroundColor: actualTimeRange.days.includes(value)
                                ? '#4F46E5'
                                : '#1F2937',
                                color: actualTimeRange.days.includes(value)
                                ? 'white'
                                : '#9CA3AF',
                                transition: 'all 0.2s ease',
                                cursor: 'pointer',
                                display: 'flex',
                                alignItems: 'center',
                                justifyContent: 'center',
                                transform: actualTimeRange.days.includes(value)
                                ? 'scale(1.05)'
                                : 'scale(1)',
                                boxShadow: actualTimeRange.days.includes(value)
                                ? 'inset 0 1px 3px rgba(0,0,0,0.3)'
                                : 'none'
                            }}
                            onMouseEnter={(e) => {
                                if (!actualTimeRange.days.includes(value)) {
                                e.target.style.backgroundColor = '#374151';
                                e.target.style.color = '#D1D5DB';
                                }
                            }}
                            onMouseLeave={(e) => {
                                if (!actualTimeRange.days.includes(value)) {
                                e.target.style.backgroundColor = '#1F2937';
                                e.target.style.color = '#9CA3AF';
                                }
                            }}
                            >
                            {label}
                            </button>
                        ))}
                        </div>
                  </div>

                  {/* Кнопки "Все" / "Очистить" */}
                  <div className="flex gap-2 ml-1">
                    <button
                      type="button"
                      onClick={selectAllDays}
                      style={{
                        fontSize: '12px',
                        padding: '6px 12px',
                        backgroundColor: '#4F46E5',
                        color: 'white',
                        border: 'none',
                        borderRadius: '6px',
                        fontWeight: '500',
                        cursor: 'pointer',
                        transition: 'background 0.2s'
                      }}
                      onMouseEnter={(e) => (e.target.style.backgroundColor = '#4338CA')}
                      onMouseLeave={(e) => (e.target.style.backgroundColor = '#4F46E5')}
                    >
                      Все
                    </button>
                    <button
                      type="button"
                      onClick={clearDays}
                      style={{
                        fontSize: '12px',
                        padding: '6px 12px',
                        backgroundColor: '#4B5563',
                        color: '#D1D5DB',
                        border: 'none',
                        borderRadius: '6px',
                        fontWeight: '500',
                        cursor: 'pointer',
                        transition: 'background 0.2s'
                      }}
                      onMouseEnter={(e) => (e.target.style.backgroundColor = '#374151')}
                      onMouseLeave={(e) => (e.target.style.backgroundColor = '#4B5563')}
                    >
                      Очистить
                    </button>
                  </div>
                </div>
              )}

              {/* Сообщение, если неактивно */}
              {!actualTimeRange.enabled && (
                <p className="text-sm text-gray-500 ml-4">
                  Правило активно в любое время. Настройки сохранены.
                </p>
              )}
            </div>
          ) : null}
        </div>
      )}
    </div>
  );
}