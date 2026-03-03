// src/components/RuleList.js
import React from 'react';

const RuleList = ({ rules, onEdit, onRun, onDelete }) => {
  console.log("📋 RuleList получил правила:", rules);
  console.log("RuleList props:", { rules, onEdit, onRun });
  const getModuleLabel = (module) => {
    const labels = { light: 'Свет', relay: 'Реле', security: 'Безопасность' };
    return labels[module] || module;
  };

  return (
    <div className="rule-list">
      <h2>🎯 Автоматизация</h2>
      <p>Управляйте правилами: триггеры → действия</p>

      {rules.length === 0 ? (
        <p style={{ color: '#888', fontStyle: 'italic' }}>Нет созданных правил</p>
      ) : (
        <table className="rules-table">
          <thead>
            <tr>
              <th>Название</th>
              <th>Модуль</th>
              <th>Приоритет</th>
              <th>Статус</th>
              <th>Действия</th>
            </tr>
          </thead>
          <tbody>
            {rules.map((rule) => {
              console.log("🔍 [RuleList] Правило:", {
                id: rule.id,
                name: rule.name,
                enabled: rule.enabled,
                type_enabled: typeof rule.enabled,
                raw: rule
              });
              return (
                <tr key={rule.id}>
                  <td>{rule.name}</td>
                  <td>{getModuleLabel(rule.module)}</td>
                  <td style={{ fontWeight: 'normal' }}>{rule.priority}</td>
                  <td>
                    <span className={`status-badge ${rule.enabled ? 'active' : 'inactive'}`}>
                      {rule.enabled ? 'Включено' : 'Выключено'}
                    </span>
                  </td>
                  <td>
                    <button className="btn-icon" title="Редактировать" onClick={() => onEdit(rule)}>
                      ✏️
                    </button>
                    <button className="btn-icon" title="Запустить" onClick={() => onRun(rule.id)}>
                      ▶️
                    </button>
                    <button 
                      className="btn-icon" 
                      title="Удалить" 
                      onClick={(e) => {
                        e.stopPropagation();
                        if (window.confirm(`Удалить правило "${rule.name}"?`)) {
                          onDelete(rule.id);
                        }
                      }}
                    >
                      🗑️
                    </button>
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      )}

      <div style={{ textAlign: 'center', margin: '24px 0' }}>
        <button
          className="btn-primary add-rule-btn"
          onClick={(e) => {
            e.stopPropagation();
            onEdit({
              id: null,
              name: '',
              module: 'light',
              priority: 3,
              enabled: true,
              triggers: [],
              actions: []
            });
          }}
          style={{
            padding: '12px 20px',
            fontSize: '15px',
            borderRadius: '8px',
            display: 'inline-flex',
            alignItems: 'center',
            gap: '8px'
          }}
        >
          ➕ Новое правило
        </button>
        {/* КНОПКА ОЧИСТИТЬ ВСЁ */}
        {rules.length > 0 && (
          <button
            className="btn-danger"
            style={{ marginLeft: '16px', padding: '8px 16px', fontSize: '14px' }}
            onClick={(e) => {
              e.stopPropagation();
              if (window.confirm('Вы уверены, что хотите удалить ВСЕ правила?')) {
                fetch('/api/rules/clear', {
                  method: 'DELETE'
                })
                .then(r => r.json())
                .then(data => {
                  if (data.status === 'ok') {
                    alert('✅ Все правила удалены');
                    // Обновим список через родительский компонент или принудительно
                    window.location.reload(); // или передай колбэк
                  } else {
                    alert('❌ Ошибка: ' + data.error);
                  }
                })
                .catch(err => {
                  alert('❌ Ошибка сети: ' + err.message);
                });
              }
            }}
          >
            🗑️ Очистить всё
          </button>
        )}
      </div>
    </div>
  );
};

export default RuleList;
