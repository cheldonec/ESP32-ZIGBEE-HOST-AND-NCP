// src/components/RulesPanel.js
import { useState, useEffect } from 'react';
import { useDevices } from '../hooks/useDevices';

// === Маппинг типов данных для отображения ===
const dataTypesMap = {
  16: { label: 'Boolean', jsType: 'bool' },
  32: { label: 'Uint8', jsType: 'u8' },
  33: { label: 'Uint16', jsType: 'u16' },
  35: { label: 'Uint32', jsType: 'u32' },
  48: { label: 'Enum8', jsType: 'u8' },
  66: { label: 'String', jsType: 'string' },
};

const conditionTypes = [
  { value: 'eq', label: 'равно' },
  { value: 'ne', label: 'не равно' },
  { value: 'gt', label: 'больше' },
  { value: 'lt', label: 'меньше' },
  { value: 'gte', label: 'больше или равно' },
  { value: 'lte', label: 'меньше или равно' },
];

const actionTypes = [
  { value: 'send_cmd_device', label: 'Отправить команду устройству' },
  { value: 'set_var', label: 'Изменить переменную' }, // временно
];

function getClusterOptions(device, epId) {
  if (!device || !epId) return [];
  const ep = device.endpoints?.find(e => e.id === parseInt(epId));
  if (!ep) return [];

  return [
    ...(ep.standard_clusters?.filter(c => c.role === 'server') || []),
    ...(ep.custom_clusters || [])
  ].map(cluster => {
    if (cluster.commands) {
      cluster.commands = cluster.commands.map(cmd => ({
        ...cmd,
        guid: cmd.guid || `${device.ieee_addr}:${ep.id}:cmd:${cmd.cluster_id?.toString(16).padStart(4, '0')}:${cmd.id.toString(16).padStart(2, '0')}`
      }));
    }
    return cluster;
  });
}

function getCommandOptions(cluster) {
  return cluster?.commands || [];
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

export default function RulesPanel({ ruleId }) {
  const { devices: allDevices } = useDevices();
  const [ruleName, setRuleName] = useState(`Правило ${ruleId.split('_')[1]}`);
  const [enabled, setEnabled] = useState(true);
  const [priority, setPriority] = useState(0);
  const [execMode, setExecMode] = useState('first');
  const [logicOp, setLogicOp] = useState('or');

  const [cause, setCause] = useState({
    device: '',
    ep: '',
    cluster: '',
    attrOrRep: '',
    cond: 'eq',
    expected_type: 'bool',
    value: '1',
  });

  const [allowingTriggers, setAllowingTriggers] = useState([]);
  const [actions, setActions] = useState([
    {
      id: 'a1',
      type: 'send_cmd_device',
      device: '',
      ep: '',
      cluster: '',
      cmd: '',
      params: {}
    }
  ]);

  // === Выпадающие списки ===
  const devicesOptions = allDevices.map(d => ({
    value: d.ieee_addr,
    label: `${d.name || d.friendly_name || d.ieee_addr} (${d.short_addr})`
  }));

  const endpointOptions = (deviceIeee) => {
    const device = allDevices.find(d => d.ieee_addr === deviceIeee);
    return device?.endpoints.map(ep => ({ value: ep.id, label: `EP ${ep.id}` })) || [];
  };

  const clusterOptions = (deviceIeee, epId) => {
    const device = allDevices.find(d => d.ieee_addr === deviceIeee);
    const ep = device?.endpoints.find(e => e.id === parseInt(epId));
    return getClusterOptions(device, epId).map(c => ({
      value: c.id,
      label: `${c.name} (0x${c.id.toString(16).padStart(4, '0')})`
    }));
  };

  const attrRepOptions = (deviceIeee, epId, clusterId) => {
    const device = allDevices.find(d => d.ieee_addr === deviceIeee);
    const ep = device?.endpoints.find(e => e.id === parseInt(epId));
    const cluster = getClusterOptions(device, epId).find(c => c.id === parseInt(clusterId));
    return getAttrRepOptions(cluster);
  };

  const selectedAttr = cause.device && cause.ep && cause.cluster
    ? attrRepOptions(cause.device, cause.ep, cause.cluster).find(a => a.value === cause.attrOrRep)
    : null;

  // === Сброс зависимых полей при изменении родителя (побуждающий триггер) ===
  useEffect(() => {
    if (!cause.device) setCause(prev => ({ ...prev, ep: '', cluster: '', attrOrRep: '' }));
  }, [cause.device]);

  useEffect(() => {
    if (!cause.ep) setCause(prev => ({ ...prev, cluster: '', attrOrRep: '' }));
  }, [cause.ep]);

  useEffect(() => {
    if (!cause.cluster) setCause(prev => ({ ...prev, attrOrRep: '' }));
  }, [cause.cluster]);

  // === Функция для изменения действий ===
  const handleActionChange = (actionId, field, value) => {
    setActions(actions.map(a => {
      if (a.id !== actionId) return a;
      const updated = { ...a, [field]: value };

      if (field === 'device') {
        updated.ep = '';
        updated.cluster = '';
        updated.cmd = '';
        updated.params = {};
      }
      if (field === 'ep') {
        updated.cluster = '';
        updated.cmd = '';
        updated.params = {};
      }
      if (field === 'cluster') {
        updated.cmd = '';
        updated.params = {};
      }

      return updated;
    }));
  };

  // === Функция для изменения разрешающих условий ===
  const handleTriggerChange = (triggerId, field, value) => {
    setAllowingTriggers(allowingTriggers.map(t => {
      if (t.id !== triggerId) return t;
      const updated = { ...t, [field]: value };

      if (field === 'device') {
        updated.ep = '';
        updated.cluster = '';
        updated.attrOrRep = '';
      }
      if (field === 'ep') {
        updated.cluster = '';
        updated.attrOrRep = '';
      }
      if (field === 'cluster') {
        updated.attrOrRep = '';
      }

      return updated;
    }));
  };

  const handleParamChange = (actionId, paramName, value) => {
    setActions(actions.map(a => {
      if (a.id === actionId && a.cmd) {
        return {
          ...a,
          params: { ...a.params, [paramName]: value }
        };
      }
      return a;
    }));
  };

  const addAction = () => {
    setActions([
      ...actions,
      {
        id: `a${Date.now()}`,
        type: 'send_cmd_device',
        device: '',
        ep: '',
        cluster: '',
        cmd: '',
        params: {}
      }
    ]);
  };

  const removeAction = (id) => {
    setActions(actions.filter(a => a.id !== id));
  };

  const removeAllowingTrigger = (id) => {
    setAllowingTriggers(allowingTriggers.filter(t => t.id !== id));
  };

  // === Получение команды по GUID ===
  const getCommandById = (deviceIeee, epId, clusterId, cmdGuid) => {
    const device = allDevices.find(d => d.ieee_addr === deviceIeee);
    const ep = device?.endpoints.find(e => e.id === parseInt(epId));
    const cluster = getClusterOptions(device, epId).find(c => c.id === parseInt(clusterId));
    return cluster?.commands.find(c => c.guid === cmdGuid) || null;
  };

  // === Сохранение правила ===
  const handleSave = async () => {
    const findAttrOrRepByGuid = (guid) => {
      for (const dev of allDevices) {
        for (const ep of dev.endpoints || []) {
          for (const cl of [...ep.standard_clusters, ...ep.custom_clusters]) {
            for (const attr of cl.attributes || []) {
              if (attr.guid === guid) return { ...attr, isAttr: true, ep, cluster: cl };
            }
            for (const rep of cl.custom_reports || []) {
              if (rep.guid === guid) return { ...rep, isRep: true, ep, cluster: cl };
            }
          }
        }
      }
      return null;
    };

    const causeItem = findAttrOrRepByGuid(cause.attrOrRep);
    const causeType = causeItem?.type || 16;

    const ruleData = {
      id: ruleId,
      name: ruleName,
      enabled,
      priority: parseInt(priority),
      exec_mode: execMode === 'first' ? 0 : 1,
      allowing_logic_op: logicOp === 'or' ? 0 : 1,
      cause_trigger: {
        guid: cause.attrOrRep,
        cond: ['eq', 'ne', 'gt', 'lt', 'gte', 'lte'].indexOf(cause.cond),
        expected_type: causeType,
        value: (() => {
          if (causeType === 16) return cause.value === 'true' || cause.value === '1' ? 1 : 0;
          if ([32, 33, 48].includes(causeType)) return parseInt(cause.value) || 0;
          return cause.value || '';
        })(),
      },
      allowing_triggers: allowingTriggers.map(t => {
        const item = findAttrOrRepByGuid(t.attrOrRep);
        const type = item?.type || 16;
        return {
          guid: t.attrOrRep,
          cond: ['eq', 'ne', 'gt', 'lt', 'gte', 'lte'].indexOf(t.cond),
          expected_type: type,
          value: (() => {
            if (type === 16) return t.value === 'true' || t.value === '1' ? 1 : 0;
            if ([32, 33, 48].includes(type)) return parseInt(t.value) || 0;
            return t.value || '';
          })(),
        };
      }),
      actions: actions.map(a => {
        if (a.type === 'set_var') {
          const varIdx = parseInt(a.target.replace('var_', ''));
          return {
            type: 1 + varIdx % 4,
            var_idx: varIdx,
            value: parseInt(a.value) || 0
          };
        }
        if (a.type === 'send_cmd_device') {
          return {
            type: 0,
            cmd_guid: a.cmd,
            params: a.params
          };
        }
        return { type: 0 };
      })
    };

    try {
      const res = await fetch('/api/rules/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(ruleData)
      });

      if (res.ok) {
        alert('Правило успешно сохранено!');
      } else {
        alert('Ошибка: ' + await res.text());
      }
    } catch (err) {
      alert('Ошибка сети');
    }
  };

  return (
    <div className="p-6">
      <h2 className="text-lg font-semibold text-white mb-4">📋 Редактирование правила</h2>

      {/* Основные */}
      <div className="panel mb-6">
        <div className="panel-header">🔧 Основные</div>
        <div className="panel-body space-y-4">
          <div className="form-row">
            <label className="form-label">Название</label>
            <input
              type="text"
              value={ruleName}
              onChange={(e) => setRuleName(e.target.value)}
              className="form-input"
            />
          </div>
          <div className="grid grid-cols-2 gap-4">
            <div className="form-row">
              <label className="form-label">Приоритет</label>
              <input
                type="number"
                value={priority}
                onChange={(e) => setPriority(e.target.value)}
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
                  onChange={(e) => setEnabled(e.target.checked)}
                  className="form-checkbox"
                />
                <span>{enabled ? 'Да' : 'Нет'}</span>
              </label>
            </div>
          </div>
          <div className="grid grid-cols-2 gap-4">
            <div className="form-row">
              <label className="form-label">Выполнять</label>
              <select
                value={execMode}
                onChange={(e) => setExecMode(e.target.value)}
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
                onChange={(e) => setLogicOp(e.target.value)}
                className="form-input"
              >
                <option value="or">Хотя бы одно</option>
                <option value="and">Все должны быть верны</option>
              </select>
            </div>
          </div>
        </div>
      </div>

      {/* Побуждающий триггер */}
      <div className="panel mb-6">
        <div className="panel-header">⚡ Побуждающий триггер</div>
        <div className="panel-body space-y-3">
          <div className="bg-gray-800/40 p-4 rounded border border-gray-700 grid grid-cols-1 md:grid-cols-5 gap-3 text-xs">
            <div>
              <label className="block text-gray-400 mb-1">Устройство</label>
              <select
                value={cause.device}
                onChange={(e) => setCause(prev => ({ ...prev, device: e.target.value, ep: '', cluster: '', attrOrRep: '' }))}
                className="form-input"
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
                onChange={(e) => setCause(prev => ({ ...prev, ep: e.target.value, cluster: '', attrOrRep: '' }))}
                disabled={!cause.device}
                className="form-input"
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
                onChange={(e) => setCause(prev => ({ ...prev, cluster: e.target.value, attrOrRep: '' }))}
                disabled={!cause.ep}
                className="form-input"
              >
                <option value="">Выберите</option>
                {clusterOptions(cause.device, cause.ep).map(c => (
                  <option key={c.value} value={c.value}>{c.label}</option>
                ))}
              </select>
            </div>
            <div>
              <label className="block text-gray-400 mb-1">Атрибут/Репорт</label>
              <select
                value={cause.attrOrRep}
                onChange={(e) => setCause(prev => ({ ...prev, attrOrRep: e.target.value }))}
                disabled={!cause.cluster}
                className="form-input"
              >
                <option value="">Выберите</option>
                {attrRepOptions(cause.device, cause.ep, cause.cluster).map(opt => (
                  <option key={opt.value} value={opt.value}>{opt.label}</option>
                ))}
              </select>
            </div>
            <div>
              <label className="block text-gray-400 mb-1">Тип атрибута</label>
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
                onChange={(e) => setCause(prev => ({ ...prev, value: e.target.value }))}
                placeholder="1, true..."
                className="form-input"
              />
            </div>
          </div>
          <div className="flex gap-3 text-xs mt-2">
            <div className="flex-1">
              <label className="block text-gray-400 mb-1">Условие</label>
              <select
                value={cause.cond}
                onChange={(e) => setCause(prev => ({ ...prev, cond: e.target.value }))}
                className="form-input"
              >
                {conditionTypes.map(c => (
                  <option key={c.value} value={c.value}>{c.label}</option>
                ))}
              </select>
            </div>
          </div>
        </div>
      </div>

      {/* Разрешающие условия */}
      <div className="panel mb-6">
        <div className="panel-header">🎯 Разрешающие условия</div>
        <div className="panel-body space-y-4">
          {allowingTriggers.map((t) => {
            const attr = attrRepOptions(t.device, t.ep, t.cluster).find(a => a.value === t.attrOrRep);
            return (
              <div key={t.id} className="bg-gray-800/40 p-4 rounded border border-gray-700">
                <div className="grid grid-cols-1 md:grid-cols-5 gap-3 text-xs mb-3">
                  <div>
                    <select
                      value={t.device}
                      onChange={(e) => handleTriggerChange(t.id, 'device', e.target.value)}
                      className="form-input"
                    >
                      <option value="">Устройство</option>
                      {devicesOptions.map(opt => (
                        <option key={opt.value} value={opt.value}>{opt.label}</option>
                      ))}
                    </select>
                  </div>
                  <div>
                    <select
                      value={t.ep}
                      onChange={(e) => handleTriggerChange(t.id, 'ep', e.target.value)}
                      disabled={!t.device}
                      className="form-input"
                    >
                      <option value="">Выберите</option>
                      {endpointOptions(t.device).map(opt => (
                        <option key={opt.value} value={opt.value}>{opt.label}</option>
                      ))}
                    </select>
                  </div>
                  <div>
                    <select
                      value={t.cluster}
                      onChange={(e) => handleTriggerChange(t.id, 'cluster', e.target.value)}
                      disabled={!t.ep}
                      className="form-input"
                    >
                      <option value="">Кластер</option>
                      {clusterOptions(t.device, t.ep).map(c => (
                        <option key={c.value} value={c.value}>{c.label}</option>
                      ))}
                    </select>
                  </div>
                  <div>
                    <select
                      value={t.attrOrRep}
                      onChange={(e) => handleTriggerChange(t.id, 'attrOrRep', e.target.value)}
                      disabled={!t.cluster}
                      className="form-input"
                    >
                      <option value="">Атрибут/Репорт</option>
                      {attrRepOptions(t.device, t.ep, t.cluster).map(opt => (
                        <option key={opt.value} value={opt.value}>{opt.label}</option>
                      ))}
                    </select>
                  </div>
                  <div>
                    <label className="block text-gray-400 mb-1">Тип</label>
                    {attr ? (
                      <div className="text-sm text-green-400 font-mono">
                        {formatDataType(attr.type)}
                      </div>
                    ) : (
                      <div className="text-sm text-gray-500">—</div>
                    )}
                  </div>
                </div>
                <div className="flex gap-3 text-xs">
                  <div className="flex-1">
                    <select
                      value={t.cond}
                      onChange={(e) => handleTriggerChange(t.id, 'cond', e.target.value)}
                      className="form-input"
                    >
                      {conditionTypes.map(c => (
                        <option key={c.value} value={c.value}>{c.label}</option>
                      ))}
                    </select>
                  </div>
                  <div className="flex-1">
                    <input
                      type="text"
                      value={t.value}
                      onChange={(e) => handleTriggerChange(t.id, 'value', e.target.value)}
                      placeholder="значение"
                      className="form-input"
                    />
                  </div>
                  <button
                    onClick={() => removeAllowingTrigger(t.id)}
                    className="text-red-400 hover:text-red-300"
                  >
                    ✕
                  </button>
                </div>
              </div>
            );
          })}
          <button
            onClick={() => setAllowingTriggers([...allowingTriggers, { id: `c${Date.now()}`, ...cause }])}
            className="btn-primary text-xs px-3 py-1"
          >
            + Добавить условие
          </button>
        </div>
      </div>

      {/* Действия */}
      <div className="panel mb-6">
        <div className="panel-header">✅ Действия</div>
        <div className="panel-body space-y-4">
          {actions.map((a) => {
            const device = allDevices.find(d => d.ieee_addr === a.device);
            const ep = device?.endpoints.find(e => e.id === parseInt(a.ep));
            const cluster = getClusterOptions(device, a.ep).find(c => c.id === parseInt(a.cluster));
            const cmd = getCommandById(a.device, a.ep, cluster?.id, a.cmd);

            return (
                <div key={a.id} className="bg-gray-800/40 p-4 rounded border border-gray-700">
                <div className="grid grid-cols-1 md:grid-cols-4 gap-3 text-xs mb-3">
                    <div>
                    <label className="block text-gray-400 mb-1">Тип</label>
                    <select
                        value={a.type}
                        onChange={(e) => handleActionChange(a.id, 'type', e.target.value)}
                        className="form-input"
                    >
                        {actionTypes.map(act => (
                        <option key={act.value} value={act.value}>{act.label}</option>
                        ))}
                    </select>
                    </div>

                    {/* === set_var === */}
                    {a.type === 'set_var' && (
                    <>
                        <div>
                        <label className="block text-gray-400 mb-1">Переменная</label>
                        <select
                            value={a.target || ''}
                            onChange={(e) => handleActionChange(a.id, 'target', e.target.value)}
                            className="form-input"
                        >
                            <option value="">Выберите</option>
                            <option value="var_0">Ночное время (var_0)</option>
                            <option value="var_1">Освещённость (var_1)</option>
                        </select>
                        </div>
                        <div>
                        <label className="block text-gray-400 mb-1">Значение</label>
                        <input
                            type="text"
                            value={a.value || ''}
                            onChange={(e) => handleActionChange(a.id, 'value', e.target.value)}
                            placeholder="1, true..."
                            className="form-input"
                        />
                        </div>
                    </>
                    )}

                    {/* === send_cmd_device === */}
                    {a.type === 'send_cmd_device' && (
                    <>
                        <div>
                        <label className="block text-gray-400 mb-1">Устройство</label>
                        <select
                            value={a.device}
                            onChange={(e) => handleActionChange(a.id, 'device', e.target.value)}
                            className="form-input"
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
                            value={a.ep}
                            onChange={(e) => handleActionChange(a.id, 'ep', e.target.value)}
                            disabled={!a.device}
                            className="form-input"
                        >
                            <option value="">EP</option>
                            {endpointOptions(a.device).map(epOpt => (
                            <option key={epOpt.value} value={epOpt.value}>{epOpt.label}</option>
                            ))}
                        </select>
                        </div>

                        <div>
                        <label className="block text-gray-400 mb-1">Кластер</label>
                        <select
                            value={a.cluster}
                            onChange={(e) => handleActionChange(a.id, 'cluster', e.target.value)}
                            disabled={!a.ep}
                            className="form-input"
                        >
                            <option value="">Кластер</option>
                            {clusterOptions(a.device, a.ep).map(c => (
                            <option key={c.value} value={c.value}>{c.label}</option>
                            ))}
                        </select>
                        </div>

                        <div>
                        <label className="block text-gray-400 mb-1">Команда</label>
                        <select
                            value={a.cmd}
                            onChange={(e) => {
                            const newCmdGuid = e.target.value;
                            handleActionChange(a.id, 'cmd', newCmdGuid);
                            const newCmd = getCommandById(a.device, a.ep, a.cluster, newCmdGuid);
                            if (newCmd?.params) {
                                const emptyParams = {};
                                newCmd.params.forEach(p => {
                                emptyParams[p.name] = '';
                                });
                                handleActionChange(a.id, 'params', emptyParams);
                            }
                            }}
                            disabled={!a.cluster}
                            className="form-input"
                        >
                            <option value="">Выберите команду</option>
                            {cluster && getCommandOptions(cluster).map(cmd => (
                            <option key={cmd.guid} value={cmd.guid}>
                                {cmd.name}
                            </option>
                            ))}
                        </select>
                        </div>
                    </>
                    )}
                </div> {/* ✅ Закрытие grid */}

                {/* Параметры команды */}
                {a.type === 'send_cmd_device' && a.cmd && cmd?.params?.length > 0 && (
                    <div className="mt-2">
                    <table className="w-full text-xs border-collapse">
                        <thead>
                        <tr className="text-left text-gray-500">
                            <th>Параметр</th>
                            <th>Тип</th>
                            <th>Ввод</th>
                        </tr>
                        </thead>
                        <tbody>
                        {cmd.params.map((param, i) => (
                            <tr key={i}>
                            <td className="py-1 text-gray-300">{param.name}</td>
                            <td className="py-1 text-gray-500">{formatDataType(param.type)}</td>
                            <td className="py-1">
                                <input
                                type="text"
                                placeholder="значение"
                                className="form-input text-xs px-2 py-1 h-6"
                                style={{ fontSize: '11px', padding: '1px 4px' }}
                                value={a.params[param.name] || ''}
                                onChange={(e) => handleParamChange(a.id, param.name, e.target.value)}
                                />
                            </td>
                            </tr>
                        ))}
                        </tbody>
                    </table>
                    </div>
                )}

                <button
                    onClick={() => removeAction(a.id)}
                    className="text-red-400 hover:text-red-300 text-sm mt-2 md:mt-0 md:ml-2"
                >
                    ✕ Удалить
                </button>
                </div>
            );
            })}
        </div>
      </div>

      {/* Кнопки управления */}
      <div className="mt-6 flex justify-end gap-3">
        <button
          onClick={() => alert('Тестирование правила...')}
          className="btn-primary bg-gray-600 hover:bg-gray-700"
        >
          Тестировать
        </button>
        <button onClick={handleSave} className="btn-primary">
          Сохранить правило
        </button>
      </div>
    </div>
  );
}
