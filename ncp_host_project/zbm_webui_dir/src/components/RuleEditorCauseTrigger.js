// src/components/RuleEditorCauseTrigger.js
import { useEffect } from 'react';
//import { virtualVariables, getVariable, formatDataType } from './variables';
import { formatDataType } from '../utils/variables';
const conditionTypes = [
  { value: 'eq', label: 'равно' },
  { value: 'ne', label: 'не равно' },
  { value: 'gt', label: 'больше' },
  { value: 'lt', label: 'меньше' },
  { value: 'gte', label: 'больше или равно' },
  { value: 'lte', label: 'меньше или равно' },
];

const sourceTypes = [
  { value: 'attr_rep', label: 'Значение атрибута/репорта' },
  { value: 'variable', label: 'Значение переменной' },
];

function getClusterOptions(device, epId) {
  if (!device || !epId) return [];
  const ep = device.endpoints?.find(e => e.id === parseInt(epId));
  if (!ep) return [];
  return [...(ep.standard_clusters?.filter(c => c.role === 'server') || []), ...(ep.custom_clusters || [])];
}

function getAttrRepOptions(cluster) {
  if (!cluster) return [];
  const attrs = cluster.attributes?.map(a => ({
    value: a.guid,
    label: `${a.name} (0x${a.id.toString(16).padStart(4, '0')})`,
    type: a.type,
  })) || [];
  const reps = cluster.custom_reports?.map(r => ({
    value: r.guid,
    label: `${r.name} (0x${r.id.toString(16).padStart(2, '0')})`,
    type: r.type,
  })) || [];
  return [...attrs, ...reps];
}

export default function RuleEditorCauseTrigger({ devices, cause, onChange, variables }) {
  const sourceType = cause.sourceType || 'attr_rep';
  const isVarMode = sourceType === 'variable';

  const selectedDevice = devices.find(d => d.ieee_addr === cause.device);
  //const selectedEp = selectedDevice?.endpoints.find(e => e.id === parseInt(cause.ep));
  const selectedCluster = getClusterOptions(selectedDevice, cause.ep).find(c => c.id === parseInt(cause.cluster));
  const selectedAttrOrRep = getAttrRepOptions(selectedCluster).find(a => a.value === cause.attrOrRep);

  const selectedVar = isVarMode ? variables.find(v => v.guid === cause.var) : null;

  // === Сброс эндпоинта при смене устройства ===
  /*useEffect(() => {
    if (cause.device && !cause.ep) {
      // Можно оставить пустым, но если хочешь — выбрать первый EP
    }
  }, [cause.device]);

  // === Сброс кластера при смене EP ===
  useEffect(() => {
    if (cause.ep && !cause.cluster) {
      // Можно выбрать первый кластер
    }
  }, [cause.ep]);
*/
  const updateCause = (updates) => {
    onChange({ ...cause, ...updates });
  };

  return (
    <div className="panel mb-6">
      <div className="panel-header">⚡ Побуждающий триггер</div>
      <div className="panel-body space-y-4">

        {/* Тип источника */}
        <div className="form-row">
          <label className="form-label">Источник</label>
          <select
            value={sourceType}
            onChange={(e) => {
              const newType = e.target.value;
              if (newType === 'variable') {
                updateCause({
                  sourceType: newType,
                  device: '',
                  ep: '',
                  cluster: '',
                  attrOrRep: '',
                  cond: 'eq'
                });
              } else {
                updateCause({
                  sourceType: newType,
                  var: '',
                  cond: 'eq'
                });
              }
            }}
            className="form-input"
          >
            {sourceTypes.map(t => (
              <option key={t.value} value={t.value}>{t.label}</option>
            ))}
          </select>
        </div>

        {/* Блок: Переменная */}
        {isVarMode && (
          <div className="form-section-indent">
            <div className="form-row">
              <label className="form-label">Переменная</label>
              <select
                value={cause.var || ''}
                onChange={(e) => {
                  const guid = e.target.value;
                  const variable = variables.find(v => v.guid === guid);
                  updateCause({
                    var: guid,
                    guid,
                    expected_type: variable?.type || 0x20,
                    cond: 'eq',
                    value: cause.value || '0'
                  });
                }}
                className="form-input"
              >
                <option value="">Выберите</option>
                {variables.map(v => (
                  <option key={v.guid} value={v.guid}>{v.name} ({v.guid})</option>
                ))}
              </select>
            </div>
            {selectedVar && (
              <div className="form-row">
                <label className="form-label">Тип</label>
                <div className="form-static-text font-mono text-green-400">
                  {formatDataType(selectedVar.type)}
                </div>
              </div>
            )}
          </div>
        )}

        {/* Блок: Атрибут/Репорт */}
        {!isVarMode && (
          <div className="space-y-3 ml-4 border-l-2 border-gray-700 pl-4">
            <div className="grid grid-cols-1 md:grid-cols-4 gap-4 text-sm">

              {/* Устройство */}
              <div className="form-row">
                <label className="form-label">Устройство</label>
                <select
                  value={cause.device || ''}
                  onChange={(e) => {
                    updateCause({
                      device: e.target.value,
                      ep: '',
                      cluster: '',
                      attrOrRep: ''
                    });
                  }}
                  className="form-input"
                >
                  <option value="">Выберите</option>
                  {devices.map(d => (
                    <option key={d.ieee_addr} value={d.ieee_addr}>
                      {d.name || d.friendly_name || d.ieee_addr}
                    </option>
                  ))}
                </select>
              </div>

              {/* Эндпоинт */}
              <div className="form-row">
                <label className="form-label">Эндпоинт</label>
                <select
                  value={cause.ep || ''}
                  onChange={(e) => {
                    updateCause({
                      ep: e.target.value,
                      cluster: '',
                      attrOrRep: ''
                    });
                  }}
                  disabled={!cause.device}
                  className="form-input"
                >
                  <option value="">EP</option>
                  {selectedDevice?.endpoints.map(ep => (
                    <option key={ep.id} value={ep.id}>EP {ep.id}</option>
                  ))}
                </select>
              </div>

              {/* Кластер */}
              <div className="form-row">
                <label className="form-label">Кластер</label>
                <select
                  value={cause.cluster || ''}
                  onChange={(e) => {
                    updateCause({
                      cluster: e.target.value,
                      attrOrRep: ''
                    });
                  }}
                  disabled={!cause.ep}
                  className="form-input"
                >
                  <option value="">Кластер</option>
                  {getClusterOptions(selectedDevice, cause.ep).map(c => (
                    <option key={c.id} value={c.id}>
                      {c.name} (0x{c.id.toString(16).padStart(4, '0')})
                    </option>
                  ))}
                </select>
              </div>

              {/* Атрибут/Репорт */}
              <div className="form-row">
                <label className="form-label">Атрибут/Репорт</label>
                <select
                  value={cause.attrOrRep || ''}
                  onChange={(e) => {
                    const guid = e.target.value;
                    const option = getAttrRepOptions(selectedCluster)?.find(a => a.value === guid);
                    updateCause({
                      attrOrRep: guid,
                      guid,
                      expected_type: option?.type || 0x20,
                    });
                  }}
                  disabled={!cause.cluster}
                  className="form-input"
                >
                  <option value="">Выберите</option>
                  {getAttrRepOptions(selectedCluster).map(opt => (
                    <option key={opt.value} value={opt.value}>{opt.label}</option>
                  ))}
                </select>
              </div>
            </div>

            {/* Тип атрибута */}
            {selectedAttrOrRep && (
              <div className="form-row">
                <label className="form-label">Тип</label>
                <div className="form-static-text font-mono text-green-400">
                  {formatDataType(selectedAttrOrRep.type)}
                </div>
              </div>
            )}
          </div>
        )}

        {/* Условие и значение */}
        <div className="grid grid-cols-1 md:grid-cols-2 gap-4 ml-4">
          <div className="form-row">
            <label className="form-label">Условие</label>
            <select
              value={cause.cond}
              onChange={(e) => updateCause({ cond: e.target.value })}
              className="form-input"
            >
              {conditionTypes.map(c => (
                <option key={c.value} value={c.value}>{c.label}</option>
              ))}
            </select>
          </div>

          <div className="form-row">
            <label className="form-label">Значение</label>
            <input
              type="text"
              value={cause.value !== undefined && cause.value !== null ? String(cause.value) : ''}
              onChange={(e) => updateCause({ value: e.target.value })}
              placeholder="1, true..."
              className="form-input"
            />
          </div>
        </div>
      </div>
    </div>
  );
}