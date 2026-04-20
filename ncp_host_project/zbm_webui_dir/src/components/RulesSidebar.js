// src/components/RulesSidebar.js
import { useRules } from '../hooks/useRules';

export default function RulesSidebar({ selectedRuleId, onSelect, onAddRule }) {
  const { rules, loading } = useRules();

  const handleAdd = () => {
    onSelect('rule', 'new'); // или 'temp_' + Date.now()
    onAddRule?.('new');
    };

  const handleDelete = async (id) => {
    if (!window.confirm('Удалить правило?')) return;
    try {
      await fetch(`/api/rule/${id}`, { method: 'DELETE' });
      // useRules подписывается на удаление через WebSocket → автоматом обновится
    } catch (err) {
      alert('Ошибка удаления');
    }
  };

  if (loading && rules.length === 0) {
    return (
      <div className="device-list">
        <div className="sidebar-header">
          <span>📋 Правила</span>
        </div>
        <p className="text-center text-gray-500 mt-4">Загрузка...</p>
      </div>
    );
  }

  return (
    <div className="device-list">
      <div className="sidebar-header flex justify-between items-center">
        <span>📋 Правила</span>
        <button onClick={handleAdd} className="text-blue-400 hover:text-blue-300 text-sm">➕</button>
      </div>

      <nav className="device-list-content">
        {rules.length === 0 ? (
          <p className="device-list-empty">Нет правил</p>
        ) : (
          rules.map((rule) => (
            <div
              key={rule.id}
              className={`device-item ${selectedRuleId === rule.id ? 'selected' : ''}`}
            >
              <div className="device-item-content" onClick={() => onSelect('rule', rule.id)}>
                <div className="device-text">
                  <div className="device-name">{rule.name}</div>
                  <div className="device-meta">{rule.enabled ? 'Включено' : 'Отключено'}</div>
                </div>
                <span className={`device-status ${rule.enabled ? 'status-online' : 'status-offline'}`}>
                  {rule.enabled ? 'On' : 'Off'}
                </span>
              </div>
              <button
                onClick={(e) => {
                  e.stopPropagation();
                  handleDelete(rule.id);
                }}
                className="text-red-400 hover:text-red-300 text-xs"
              >
                ✕
              </button>
            </div>
          ))
        )}
      </nav>
    </div>
  );
}