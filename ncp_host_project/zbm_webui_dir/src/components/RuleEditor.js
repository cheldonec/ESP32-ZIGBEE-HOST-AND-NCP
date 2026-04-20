// src/components/RuleEditor.js
import { useState, useEffect } from 'react';
import { useDevices } from '../hooks/useDevices';
import { useRules } from '../hooks/useRules';
import {
  parseGuid,
  resolveGuidPath,
  findDeviceByShort,
  findDataTypeByGuid 
} from '../utils/guidUtils'; // ✅ Добавлен findDeviceByShort
import { virtualVariables, getVariable, formatDataType } from './variables';

import RuleEditorBasics from './RuleEditorBasics';
import RuleEditorCauseTrigger from './RuleEditorCauseTrigger';
import RuleEditorAllowingTriggers from './RuleEditorAllowingTriggers';
import RuleEditorActions from './RuleEditorActions';

export default function RuleEditor({ ruleId, onDeviceRename }) {
  const { devices: allDevices } = useDevices({ onDeviceRename });
  const { rules: allRules, reload: reloadRules } = useRules();

  const [ruleData, setRuleData] = useState(null);

  // === Найдём правило по ID ===
  useEffect(() => {
    if (!allRules || !allDevices) return;

    const rule = allRules.find(r => r.id === ruleId);
    if (!rule) {
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
        actions: []
      });
      return;
    }

    // === Преобразуем разрешающие триггеры ===
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
            value: String(t.value || '')
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
            value: String(t.value || '')
          }
        : null;
    };

    // === Парсим cause_trigger ===
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
            value: String(rule.cause_trigger.value || '')
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
      allowingTriggers: (rule.allowing_triggers || [])
        .map(toTrigger)
        .filter(Boolean),
      actions: (rule.actions || []).map((a, index) => {
        const cmdGuid = a.cmd_guid;
        const cmdParsed = cmdGuid ? parseGuid(cmdGuid) : null;
        const device = cmdParsed ? findDeviceByShort(allDevices, cmdParsed.short) : null; // ✅ Исправлено: allDevices

        return {
          id: `a${index}`,
          type: 'send_cmd_device',
          device: device?.ieee_addr || '',
          ep: cmdParsed?.epId || '',
          cluster: cmdParsed?.clusterId || '',
          cmd: cmdGuid,
          params: a.params || {}
        };
      })
    });
  }, [ruleId, allRules, allDevices]);

  if (!ruleData) {
    return <div>Загрузка правила...</div>;
  }

  // === Сохранение ===
  const saveRule = async () => {
    // --- Валидация: Cause Trigger ---
    if (!ruleData.cause.guid || ruleData.cause.guid.trim() === '') {
        alert('❌ Не выбран побуждающий триггер (источник)');
        return;
    }

    if (!ruleData.cause.value || isNaN(Number(ruleData.cause.value))) {
        alert('❌ Укажите корректное значение для условия');
        return;
    }

    // --- Валидация: Действия ---
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
        if (!a.value || isNaN(Number(a.value))) {
            alert(`❌ В действии ${i + 1}: укажите число`);
            return;
        }
        }
    }

    // 🔎 Определяем тип данных из устройства
    const causeGuid = ruleData.cause.guid;
    const expectedType = causeGuid
        ? findDataTypeByGuid(causeGuid, allDevices) ?? 1 // fallback к U8
        : 1;
    // временно
    //const expectedType = 32;
    console.log('🔍 Cause GUID:', ruleData.cause.guid);
    console.log('🔧 Found expected_type:', expectedType);
    if (!expectedType) {
        console.error('❌ Failed to determine data type for guid:', causeGuid);
    }
    // --- Если всё ок — формируем тело запроса ---
    const body = {
        id: ruleData.id,
        name: ruleData.name.trim() || 'Без названия',
        enabled: ruleData.enabled,
        priority: ruleData.priority,
        exec_mode: ruleData.execMode === 'all' ? 1 : 0,
        allowing_logic_op: ruleData.logicOp === 'and' ? 1 : 0,
        cause_trigger: {
        guid: ruleData.cause.guid,
        cond: { eq: 0, ne: 1, gt: 2, lt: 3, gte: 4, lte: 5 }[ruleData.cause.cond] || 0,
        expected_type: expectedType,
        value: Number(ruleData.cause.value)
        },
        allowing_triggers: ruleData.allowingTriggers.map(t => ({
        guid: t.guid,
        cond: { eq: 0, ne: 1, gt: 2, lt: 3, gte: 4, lte: 5 }[t.cond] || 0,
        expected_type: 1,
        value: Number(t.value)
        })),
        actions: []
    };

    ruleData.actions.forEach(a => {
        if (a.type === 'send_cmd_device') {
        body.actions.push({
            type: 0,
            cmd_guid: a.cmd,
            params: a.params || {}
        });
        } else if (a.type === 'set_var') {
        const varData = getVariable(a.target);
        if (!varData) return;
        body.actions.push({
            type: 1,
            var_idx: varData.idx,
            value: Number(a.value) || 0
        });
        }
    });

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
 // конец сохранения
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

      <RuleEditorCauseTrigger
        devices={allDevices}
        cause={ruleData.cause}
        onChange={(cause) => setRuleData(prev => ({ ...prev, cause }))}
      />

      <RuleEditorAllowingTriggers
        devices={allDevices}
        triggers={ruleData.allowingTriggers}
        onAdd={() => {
          const newTrigger = {
            id: `t_${Date.now()}`,
            device: '',
            ep: '',
            cluster: '',
            attrOrRep: '',
            cond: 'eq',
            value: '1'
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