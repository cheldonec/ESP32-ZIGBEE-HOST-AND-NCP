// src/components/RulesSidebar.js
import { useRules } from '../hooks/useRules';
import { api } from '../api/httpClient';

export default function RulesSidebar({ selectedRuleId, onSelect }) {
  const { rules, loading } = useRules();

  const handleAdd = async () => {
    const newRuleTemplate = {
      name: 'Новое правило',
      enabled: true,
      priority: 0,
      exec_mode: 0,
      allowing_logic_op: 0,
      cause_trigger: { guid: '', cond: 0, expected_type: 1, value: 1 },
      allowing_triggers: [],
      actions: []
    };

    try {
      const result = await api.createRule(newRuleTemplate);
      const newId = result.id;

      window.dispatchEvent(new CustomEvent('rule_updated', {
        detail: { action: 'create', rule: { ...newRuleTemplate, id: newId } }
      }));

      onSelect('rule', newId); // можно оставить как есть — ID нужен для выбора
    } catch (err) {
      alert('Ошибка создания правила');
      console.error(err);
    }
  };

  const handleDelete = async (id) => {
    if (!window.confirm('Удалить правило?')) return;
    try {
      await api.deleteRule(id);

      window.dispatchEvent(new CustomEvent('rule_updated', {
        detail: { action: 'delete', id }
      }));
    } catch (err) {
      alert('Ошибка удаления');
    }
  };

  if (loading && rules.length === 0) {
    return (
      <div className="device-list">
        <div className="sidebar-header"><span>📋 Правила</span></div>
        <p className="text-center text-gray-500 mt-4">Загрузка...</p>
      </div>
    );
  }

  return (
    <div className="device-list">
      <div className="sidebar-header flex justify-between items-center">
        <span>📋 Правила</span>
        <button onClick={handleAdd} className="add-icon-button">➕</button>
      </div>

      <nav className="device-list-content">
        {rules.length === 0 ? (
          <p className="device-list-empty">Нет правил</p>
        ) : (
          rules.map((rule) => (
            <div key={rule.id} className={`device-item ${selectedRuleId === rule.id ? 'selected' : ''}`}>
              <div
                className="device-item-content"
                onClick={() => onSelect('rule', rule.id)}
                title={`ID: ${rule.id}`}
              >
                <div className="device-text">
                  <div className="device-name">{rule.name}</div>
                  <div className="device-meta">{rule.enabled ? 'Включено' : 'Отключено'}</div>
                </div>
                <span className={`device-status ${rule.enabled ? 'status-online' : 'status-offline'}`}>
                  {rule.enabled ? 'On' : 'Off'}
                </span>
              </div>

              <button
                type="button"
                onClick={(e) => {
                  e.stopPropagation();
                  handleDelete(rule.id);
                }}
                aria-label="Удалить правило"
                className="rule-action-remove"
              >
                ×
              </button>
            </div>
          ))
        )}
      </nav>
    </div>
  );
}