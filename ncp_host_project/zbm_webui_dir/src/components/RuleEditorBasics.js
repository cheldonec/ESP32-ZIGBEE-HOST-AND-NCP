// src/components/RuleEditorBasics.js
export default function RuleEditorBasics({ ruleName, enabled, priority, execMode, logicOp, onChange }) {
  return (
    <div className="panel mb-6">
      <div className="panel-header">🔧 Основные</div>
      <div className="panel-body space-y-4"> {/* ← здесь уже есть gap */}
        <div className="form-row"> {/* убрали gap-3 — он дублируется */}
          <label className="form-label">Название</label>
          <input
            type="text"
            value={ruleName}
            onChange={(e) => onChange('name', e.target.value)}
            className="form-input"
          />
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 gap-4"> {/* ← одинаковые колонки */}
          <div className="form-row">
            <label className="form-label">Приоритет</label>
            <input
              type="number"
              value={priority}
              onChange={(e) => onChange('priority', parseInt(e.target.value) || 0)}
              className="form-input"
              min="-128"
              max="127"
            />
          </div>
          <div className="form-row">
            <label className="form-label">Активно</label>
            <label className="flex items-center gap-2 cursor-pointer">
              <input
                type="checkbox"
                checked={enabled}
                onChange={(e) => onChange('enabled', e.target.checked)}
                className="form-checkbox"
              />
              <span>{enabled ? 'Да' : 'Нет'}</span>
            </label>
          </div>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          <div className="form-row">
            <label className="form-label">Выполнять</label>
            <select
              value={execMode}
              onChange={(e) => onChange('execMode', e.target.value)}
              className="form-input"
            >
              <option value="first">Первое подходящее</option>
              <option value="all">Все подходящие</option>
            </select>
          </div>
          <div className="form-row">
            <label className="form-label">Логика условий</label>
            <select
              value={logicOp}
              onChange={(e) => onChange('logicOp', e.target.value)}
              className="form-input"
            >
              <option value="or">Хотя бы одно</option>
              <option value="and">Все должны быть верны</option>
            </select>
          </div>
        </div>
      </div>
    </div>
  );
}