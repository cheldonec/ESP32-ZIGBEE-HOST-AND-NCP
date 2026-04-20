// src/components/RuleEditorCauseTrigger.js
import { useState } from 'react';
import { virtualVariables, getVariable } from './variables';

const conditionTypes = [
  { value: 'eq', label: 'равно' },
  { value: 'ne', label: 'не равно' },
  { value: 'gt', label: 'больше' },
  { value: 'lt', label: 'меньше' },
  { value: 'gte', label: 'больше или равно' },
  { value: 'lte', label: 'меньше или равно' },
];


function getClusterOptions(device, epId) {
  if (!device || !epId) return [];
  const ep = device.endpoints?.find(e => e.id === parseInt(epId));
  if (!ep) return [];

  return [
    ...(ep.standard_clusters?.filter(c => c.role === 'server') || []),
    ...(ep.custom_clusters || [])
  ];
}

function getAttrRepOptions(cluster) {
  if (!cluster) return [];

  const attrs = cluster.attributes?.map(a => ({
    value: a.guid,
    label: `${a.name} (0x${a.id.toString(16).padStart(4, '0')})`,
    type: a.type,
    isAttr: true
  })) || [];

  const reps = cluster.custom_reports?.map(r => ({
    value: r.guid,
    label: `${r.name} (0x${r.id.toString(16).padStart(2, '0')})`,
    type: r.type,
    isRep: true
  })) || [];

  return [...attrs, ...reps];
}

function formatDataType(type) {
  const types = {
    16: 'bool',
    32: 'uint8_t',
    33: 'uint16_t',
    35: 'uint32_t',
    48: 'enum',
    66: 'char_str',
    72: 'long_char_str',
    65: 'octet_str',
    71: 'long_octet_str'
  };
  return types[type] || `unknown (${type})`;
}

// Специальный "кластер" для переменных
const VAR_CLUSTER = {
  id: -1,
  name: '📋 Виртуальные переменные',
};

export default function RuleEditorCauseTrigger({ devices, cause, onChange }) {
  const [editingVarName, setEditingVarName] = useState('');

  const devicesOptions = devices.map(d => ({
    value: d.ieee_addr,
    label: `${d.name || d.friendly_name || d.ieee_addr} (${d.short_addr})`
  }));

  const endpointOptions = (deviceIeee) => {
    const device = devices.find(d => d.ieee_addr === deviceIeee);
    return device?.endpoints.map(ep => ({ value: ep.id, label: `EP ${ep.id}` })) || [];
  };

  const clusterOptions = (deviceIeee, epId) => {
    const device = devices.find(d => d.ieee_addr === deviceIeee);
    const ep = device?.endpoints.find(e => e.id === parseInt(epId));
    const clusters = getClusterOptions(device, epId).map(c => ({
      value: c.id,
      label: `${c.name} (0x${c.id.toString(16).padStart(4, '0')})`
    }));

    // Добавляем кластер "Переменные"
    return [
      ...clusters,
      { value: VAR_CLUSTER.id, label: VAR_CLUSTER.name }
    ];
  };

  const attrRepOptions = (deviceIeee, epId, clusterId) => {
    // Если выбран кластер переменных
    if (parseInt(clusterId) === VAR_CLUSTER.id) {
      return virtualVariables.map(v => ({
        value: v.guid,
        label: `${v.name} (${v.guid})`,
        type: v.type,
        isVar: true
      }));
    }

    // Иначе — обычные атрибуты/репорты
    const device = devices.find(d => d.ieee_addr === deviceIeee);
    const ep = device?.endpoints.find(e => e.id === parseInt(epId));
    const cluster = getClusterOptions(device, epId).find(c => c.id === parseInt(clusterId));
    return getAttrRepOptions(cluster);
  };

  const selectedAttr = cause.device && cause.ep && cause.cluster
    ? attrRepOptions(cause.device, cause.ep, cause.cluster).find(a => a.value === cause.attrOrRep)
    : null;

  return (
    <div className="panel mb-6">
      <div className="panel-header">⚡ Побуждающий триггер</div>
      <div className="panel-body space-y-3">
        <div className="bg-gray-800/40 p-4 rounded border border-gray-700 grid grid-cols-1 md:grid-cols-5 gap-2 text-xs">
          <div>
            <label className="block text-gray-400 mb-1">Устройство</label>
            <select
              value={cause.device}
              onChange={(e) => onChange({ ...cause, device: e.target.value, ep: '', cluster: '', attrOrRep: '' })}
              className="form-input text-sm"
            >
              <option value="">Выберите</option>
              {devicesOptions.map(opt => (
                <option key={opt.value} value={opt.value}>{opt.label}</option>
              ))}
            </select>
          </div>

          <div>
            <label className="block text-gray-400 mb-1">Эндпоинт</label>
            <select
              value={cause.ep}
              onChange={(e) => onChange({ ...cause, ep: e.target.value, cluster: '', attrOrRep: '' })}
              disabled={!cause.device}
              className="form-input text-sm"
            >
              <option value="">Выберите</option>
              {endpointOptions(cause.device).map(opt => (
                <option key={opt.value} value={opt.value}>{opt.label}</option>
              ))}
            </select>
          </div>

          <div>
            <label className="block text-gray-400 mb-1">Кластер</label>
            <select
              value={cause.cluster}
              onChange={(e) => onChange({ ...cause, cluster: e.target.value, attrOrRep: '' })}
              disabled={!cause.ep}
              className="form-input text-sm"
            >
              <option value="">Выберите</option>
              {clusterOptions(cause.device, cause.ep).map(c => (
                <option key={c.value} value={c.value}>{c.label}</option>
              ))}
            </select>
          </div>

          <div>
            <label className="block text-gray-400 mb-1">Атрибут/Репорт/Переменная</label>
            <select
              value={cause.attrOrRep}
              onChange={(e) => onChange({ ...cause, attrOrRep: e.target.value })}
              disabled={!cause.cluster}
              className="form-input text-sm"
            >
              <option value="">Выберите</option>
              {attrRepOptions(cause.device, cause.ep, cause.cluster).map(opt => (
                <option key={opt.value} value={opt.value}>{opt.label}</option>
              ))}
            </select>
          </div>

          <div>
            <label className="block text-gray-400 mb-1">Тип</label>
            {selectedAttr ? (
              <div className="text-sm text-green-400 font-mono">
                {formatDataType(selectedAttr.type)}
              </div>
            ) : (
              <div className="text-sm text-gray-500">—</div>
            )}
          </div>

          <div>
            <label className="block text-gray-400 mb-1">Значение</label>
            <input
              type="text"
              value={cause.value}
              onChange={(e) => onChange({ ...cause, value: e.target.value })}
              placeholder="1, true..."
              className="form-input text-sm"
            />
          </div>
        </div>

        <div className="flex gap-3 text-xs mt-2">
          <div className="flex-1">
            <label className="block text-gray-400 mb-1">Условие</label>
            <select
              value={cause.cond}
              onChange={(e) => onChange({ ...cause, cond: e.target.value })}
              className="form-input text-sm"
            >
              {conditionTypes.map(c => (
                <option key={c.value} value={c.value}>{c.label}</option>
              ))}
            </select>
          </div>
        </div>
      </div>
    </div>
  );
}