// src/components/RuleEditor.js
import { useState, useEffect } from 'react';
import { useDevices } from '../hooks/useDevices';
import { useRules } from '../hooks/useRules';
import { useVariables } from '../hooks/useVariables';
import {
  parseGuid,
  resolveGuidPath,
  findDeviceByShort,
  findDataTypeByGuid
} from '../utils/guidUtils';

import { formatDataType } from './variables';

import RuleEditorBasics from './RuleEditorBasics';
import RuleEditorCauseTrigger from './RuleEditorCauseTrigger';
import RuleEditorAllowingTriggers from './RuleEditorAllowingTriggers';
import RuleEditorActions from './RuleEditorActions';
import RuleEditorTimeRange from './RuleEditorTimeRange';

export default function RuleEditor({ ruleId, onDeviceRename }) {
  const { devices: allDevices } = useDevices({ onDeviceRename });
  const { rules: allRules, reload: reloadRules } = useRules();
  const { variables: realVariables } = useVariables();

  const [ruleData, setRuleData] = useState(null);
  const getVariable = (guid) => realVariables.find(v => v.guid === guid) || null;

  // Логируем актуальное состояние
  useEffect(() => {
    console.log('🔄 [RuleEditor] Re-evaluating ruleData. ruleId:', ruleId);
    console.log('🔍 Current rule time_range:', allRules.find(r => r.id === ruleId)?.time_range);
  }, [ruleId, allRules]);

  // === ОСНОВНОЙ ЭФФЕКТ: только при изменении ruleId или allRules ===
  useEffect(() => {
    if (!allRules || !ruleId) return;

    const rule = allRules.find(r => r.id === ruleId);

    if (rule) {
      console.log('✅ [RuleEditor] Loading existing rule with time_range:', rule.time_range);

      const toTrigger = (t) => {
        const path = t.guid ? resolveGuidPath(t.guid, allDevices) : null;
        const isVar = t.guid?.startsWith('var_');

        return isVar
          ? {
              id: `t_${t.guid}`,
              sourceType: 'variable',
              var: t.guid,
              guid: t.guid,
              cond: ['eq', 'ne', 'gt', 'lt', 'gte', 'lte'][t.cond] || 'eq',
              value: typeof t.value === 'undefined' ? '' : String(t.value),
              dataType: realVariables.find(v => v.guid === t.guid)?.type ?? null
            }
          : path
          ? {
              id: `t_${t.guid}`,
              sourceType: 'attr_rep',
              guid: t.guid,
              device: path.device.ieee_addr,
              ep: path.ep.id,
              cluster: path.cluster.id,
              attrOrRep: path.attr.guid,
              cond: ['eq', 'ne', 'gt', 'lt', 'gte', 'lte'][t.cond] || 'eq',
              value: typeof t.value === 'undefined' ? '' : String(t.value),
              dataType: path.attr.type ?? null
            }
          : null;
      };

      const causePath = rule.cause_trigger?.guid
        ? resolveGuidPath(rule.cause_trigger.guid, allDevices)
        : null;
      const causeIsVar = rule.cause_trigger?.guid?.startsWith('var_');

      setRuleData({
        id: rule.id,
        name: rule.name,
        enabled: rule.enabled,
        priority: rule.priority,
        execMode: rule.exec_mode === 1 ? 'all' : 'first',
        logicOp: rule.allowing_logic_op === 1 ? 'and' : 'or',
        cause: causeIsVar
          ? {
              sourceType: 'variable',
              guid: rule.cause_trigger.guid,
              var: rule.cause_trigger.guid,
              cond: ['eq', 'ne', 'gt', 'lt', 'gte', 'lte'][rule.cause_trigger.cond] || 'eq',
              value: typeof rule.cause_trigger.value === 'undefined' ? '' : String(rule.cause_trigger.value)
            }
          : causePath
          ? {
              sourceType: 'attr_rep',
              guid: rule.cause_trigger.guid,
              device: causePath.device.ieee_addr,
              ep: causePath.ep.id,
              cluster: causePath.cluster.id,
              attrOrRep: causePath.attr.guid,
              cond: ['eq', 'ne', 'gt', 'lt', 'gte', 'lte'][rule.cause_trigger.cond] || 'eq',
              value: String(rule.cause_trigger.value || '')
            }
          : {
              sourceType: 'attr_rep',
              guid: '',
              device: '',
              ep: '',
              cluster: '',
              attrOrRep: '',
              cond: 'eq',
              value: '1'
            },
        allowingTriggers: (rule.allowing_triggers || []).map(toTrigger).filter(Boolean),
        actions: (rule.actions || []).map((a, index) => {
          if (a.type === 0) {
            const cmdGuid = a.cmd_guid;
            const cmdParsed = cmdGuid ? parseGuid(cmdGuid) : null;
            const device = cmdParsed ? findDeviceByShort(allDevices, cmdParsed.short) : null;

            return {
              id: `a${index}`,
              type: 'send_cmd_device',
              device: device?.ieee_addr || '',
              ep: cmdParsed?.epId || '',
              cluster: cmdParsed?.clusterId || '',
              cmd: cmdGuid,
              params: a.params || {}
            };
          } else if (a.type === 1) {
            const varIdx = a.var_idx;
            const variable = realVariables.find(v => v.idx === varIdx);
            const guid = variable ? variable.guid : `var_${varIdx}`;
            const value = String(a.value);

            return {
              id: `a${index}`,
              type: 'set_var',
              target: guid,
              value: value,
              dataType: variable?.type ?? null,
              varIdx: varIdx
            };
          }

          return {
            id: `a${index}`,
            type: 'send_cmd_device',
            device: '',
            ep: '',
            cluster: '',
            cmd: '',
            params: {}
          };
        }),
        time_range: rule.time_range
          ? {
              enabled: rule.time_range.enabled,
              from: rule.time_range.from || '00:00',
              to: rule.time_range.to || '23:59',
              days: Array.isArray(rule.time_range.days) ? [...rule.time_range.days] : []
            }
          : {
              enabled: false,
              from: '00:00',
              to: '23:59',
              days: []
            }
      });
    } else {
      console.log('🆕 [RuleEditor] Creating new rule template');
      setRuleData({
        id: ruleId,
        name: 'Новое правило',
        enabled: true,
        priority: 0,
        execMode: 'first',
        logicOp: 'or',
        cause: {
          sourceType: 'attr_rep',
          guid: '',
          device: '',
          ep: '',
          cluster: '',
          attrOrRep: '',
          cond: 'eq',
          value: '1'
        },
        allowingTriggers: [],
        actions: [],
        time_range: {
          enabled: false,
          from: '00:00',
          to: '23:59',
          days: []
        }
      });
    }
  }, [ruleId, allRules]); // ⚠️ ВАЖНО: убрали allDevices и realVariables

  // === ДОПОЛНИТЕЛЬНЫЙ ЭФФЕКТ: обновление dataType при изменении устройств или переменных ===
  useEffect(() => {
    if (!ruleData || !allDevices || !realVariables) return;

    const updateDataType = (item) => {
      if (!item.guid) return item;

      let dataType = null;
      if (item.guid.startsWith('var_')) {
        const varIdx = parseInt(item.guid.replace('var_', ''), 10);
        const variable = realVariables.find(v => v.idx === varIdx);
        dataType = variable?.type ?? null;
      } else {
        const path = resolveGuidPath(item.guid, allDevices);
        dataType = path?.attr?.type ?? null;
      }

      return { ...item, dataType };
    };

    setRuleData(prev => ({
      ...prev,
      cause: updateDataType(prev.cause),
      allowingTriggers: prev.allowingTriggers.map(updateDataType),
      actions: prev.actions.map(a => {
        if (a.type === 'set_var') {
          const variable = realVariables.find(v => v.guid === a.target);
          return { ...a, dataType: variable?.type ?? null };
        }
        return a;
      })
    }));
  }, [ruleData, allDevices, realVariables]);

  if (!ruleData) {
    return <div>Загрузка правила...</div>;
  }

  // === Сохранение ===
  const saveRule = async () => {
    if (!ruleData.cause.guid || ruleData.cause.guid.trim() === '') {
      alert('❌ Не выбран побуждающий триггер (источник)');
      return;
    }

    const causeValue = ruleData.cause.value;
    if (causeValue === '' || isNaN(Number(causeValue))) {
      alert('❌ Укажите корректное значение для условия');
      return;
    }

    for (let i = 0; i < ruleData.actions.length; i++) {
      const a = ruleData.actions[i];
      if (a.type === 'send_cmd_device') {
        if (!a.cmd || a.cmd.trim() === '') {
          alert(`❌ В действии ${i + 1}: не выбрана команда`);
          return;
        }
      }
      if (a.type === 'set_var') {
        if (!a.target || a.target.trim() === '') {
          alert(`❌ В действии ${i + 1}: не выбрана переменная`);
          return;
        }
        const value = a.value;
        if (value === '' || (isNaN(Number(value)) && typeof value !== 'string')) {
          alert(`❌ В действии ${i + 1}: укажите корректное значение`);
          return;
        }
      }
    }

    let expectedType = 1;
    const causeGuid = ruleData.cause.guid;
    if (causeGuid) {
      if (causeGuid.startsWith('var_')) {
        const varIdx = parseInt(causeGuid.replace('var_', ''), 10);
        const variable = realVariables.find(v => v.idx === varIdx);
        if (variable) expectedType = variable.type;
      } else {
        expectedType = findDataTypeByGuid(causeGuid, allDevices) ?? 1;
      }
    }

    const body = {
      id: ruleData.id,
      name: ruleData.name.trim() || 'Без названия',
      enabled: ruleData.enabled,
      priority: ruleData.priority,
      exec_mode: ruleData.execMode === 'all' ? 1 : 0,
      allowing_logic_op: ruleData.logicOp === 'and' ? 1 : 0,
      time_range: {
        from: ruleData.time_range.from,
        to: ruleData.time_range.to,
        days: ruleData.time_range.days,
        enabled: ruleData.time_range.enabled
      },
      cause_trigger: {
        guid: ruleData.cause.guid,
        cond: { eq: 0, ne: 1, gt: 2, lt: 3, gte: 4, lte: 5 }[ruleData.cause.cond] || 0,
        expected_type: expectedType,
        value: Number(ruleData.cause.value)
      },
      allowing_triggers: ruleData.allowingTriggers.map(t => {
        let expected_type = 0x20;
        if (t.guid?.startsWith('var_')) {
          const varIdx = parseInt(t.guid.replace('var_', ''), 10);
          const variable = realVariables.find(v => v.idx === varIdx);
          expected_type = variable?.type ?? 0x20;
        } else {
          const path = t.guid ? resolveGuidPath(t.guid, allDevices) : null;
          expected_type = path?.attr?.type ?? 0x20;
        }
        return {
          guid: t.guid,
          cond: { eq: 0, ne: 1, gt: 2, lt: 3, gte: 4, lte: 5 }[t.cond] || 0,
          expected_type,
          value: Number(t.value)
        };
      }),
      actions: []
    };

    for (const a of ruleData.actions) {
      if (a.type === 'send_cmd_device') {
        body.actions.push({
          type: 0,
          cmd_guid: a.cmd,
          params: a.params || {}
        });
      } else if (a.type === 'set_var') {
        const varData = getVariable(a.target);
        if (!varData) {
          const match = a.target.match(/^var_(\d+)$/);
          if (!match) continue;
          const idx = parseInt(match[1], 10);
          if (isNaN(idx) || idx < 0 || idx >= 32) continue;

          const fakeVar = { idx, type: 0x20 };
          let actionValue = 0;
          switch (fakeVar.type) {
            case 0x20: actionValue = Math.max(0, Math.min(255, parseInt(a.value, 10))) || 0; break;
            case 0x28: actionValue = Math.max(-128, Math.min(127, parseInt(a.value, 10))) || 0; break;
            case 0x30: actionValue = Math.max(0, Math.min(65535, parseInt(a.value, 10))) || 0; break;
            case 0x42:
            case 0x43: actionValue = String(a.value); break;
            default: actionValue = 0; break;
          }
          body.actions.push({ type: 1, var_idx: fakeVar.idx, value: actionValue });
        } else {
          let actionValue = 0;
          const valStr = typeof a.value === 'undefined' ? '0' : String(a.value);
          switch (varData.type) {
            case 0x20: actionValue = Math.max(0, Math.min(255, parseInt(valStr, 10))) || 0; break;
            case 0x28: actionValue = Math.max(-128, Math.min(127, parseInt(valStr, 10))) || 0; break;
            case 0x30: actionValue = Math.max(0, Math.min(65535, parseInt(valStr, 10))) || 0; break;
            case 0x42:
            case 0x43: actionValue = String(valStr); break;
            default: actionValue = 0; break;
          }
          body.actions.push({ type: 1, var_idx: varData.idx, value: actionValue });
        }
      }
    }

    try {
      const res = await fetch('/api/rule', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
      });

      if (res.ok) {
        alert('✅ Правило сохранено!');
        reloadRules();
      } else {
        const text = await res.text();
        console.error('Ошибка сервера:', text);
        alert('❌ Ошибка: ' + (res.status === 400 ? 'Некорректные данные' : 'Сервер вернул ошибку'));
      }
    } catch (err) {
      console.error('Сетевая ошибка:', err);
      alert('❌ Ошибка сети: ' + err.message);
    }
  };

  return (
    <div className="p-6 max-w-5xl mx-auto">
      <h2 className="text-xl font-bold text-white mb-6">🔧 Редактирование правила</h2>

      <RuleEditorBasics
        ruleName={ruleData.name}
        enabled={ruleData.enabled}
        priority={ruleData.priority}
        execMode={ruleData.execMode}
        logicOp={ruleData.logicOp}
        onChange={(field, value) => setRuleData(prev => ({ ...prev, [field]: value }))}
      />

      <RuleEditorTimeRange
        timeRange={ruleData.time_range}
        onChange={(time_range) => setRuleData(prev => ({ ...prev, time_range }))}
      />

      <RuleEditorCauseTrigger
        devices={allDevices}
        variables={realVariables}
        cause={ruleData.cause}
        onChange={(cause) => setRuleData(prev => ({ ...prev, cause }))}
      />

      <RuleEditorAllowingTriggers
        devices={allDevices}
        variables={realVariables}
        triggers={ruleData.allowingTriggers}
        onAdd={() => {
          const newTrigger = {
            id: `t_${Date.now()}`,
            device: '',
            ep: '',
            cluster: '',
            attrOrRep: '',
            cond: 'eq',
            value: '1',
            dataType: 0x20
          };
          setRuleData(prev => ({
            ...prev,
            allowingTriggers: [...prev.allowingTriggers, newTrigger]
          }));
        }}
        onChange={(id, updates) => {
          setRuleData(prev => ({
            ...prev,
            allowingTriggers: prev.allowingTriggers.map(t =>
              t.id === id ? { ...t, ...updates } : t
            )
          }));
        }}
        onRemove={(id) => {
          setRuleData(prev => ({
            ...prev,
            allowingTriggers: prev.allowingTriggers.filter(t => t.id !== id)
          }));
        }}
      />

      <RuleEditorActions
        devices={allDevices}
        variables={realVariables}
        actions={ruleData.actions}
        onAdd={() => {
          const newAction = {
            id: `a${Date.now()}`,
            type: 'send_cmd_device',
            cmd_guid: '',
            params: {}
          };
          setRuleData(prev => ({
            ...prev,
            actions: [...prev.actions, newAction]
          }));
        }}
        onChange={(id, field, value) => {
          setRuleData(prev => ({
            ...prev,
            actions: prev.actions.map(a =>
              a.id === id ? { ...a, [field]: value } : a
            )
          }));
        }}
        onParamChange={(actionId, paramName, value) => {
          setRuleData(prev => ({
            ...prev,
            actions: prev.actions.map(a => {
              if (a.id !== actionId) return a;
              return {
                ...a,
                params: { ...a.params, [paramName]: value }
              };
            })
          }));
        }}
        onRemove={(id) => {
          setRuleData(prev => ({
            ...prev,
            actions: prev.actions.filter(a => a.id !== id)
          }));
        }}
      />

      <div className="mt-8 flex justify-end gap-4">
        <button onClick={() => window.history.back()} className="btn-secondary">
          Отмена
        </button>
        <button onClick={saveRule} className="btn-primary">
          Сохранить
        </button>
      </div>
    </div>
  );
}