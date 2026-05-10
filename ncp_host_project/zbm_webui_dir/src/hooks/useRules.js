import { useState, useEffect, useCallback } from 'react';
import { api } from '../api/httpClient';
import { subscribeToWebSocket } from '../api/websocket';

// 🟩 Глобальный кэш правил и флаг
let rulesCache = [];
let hasLoaded = false;

// 🔁 Универсальная функция с повторными попытками
const fetchWithRetry = async (fetchFn, maxRetries = 2, delayMs = 300) => {
  for (let i = 0; i <= maxRetries; i++) {
    try {
      return await fetchFn();
    } catch (err) {
      if (i === maxRetries) throw err;
      console.warn(`🔁 Повтор запроса правила (попытка ${i + 1})`, err.message);
      await new Promise(resolve => setTimeout(resolve, delayMs));
    }
  }
};

export const useRules = ({ enabled = true } = {}) => {
  const [rules, setRules] = useState(rulesCache);
  const [loading, setLoading] = useState(() => !hasLoaded);
  const [error, setError] = useState(null);

  const loadAllRules = useCallback(async () => {
    if (hasLoaded) {
      console.log('🟡 [useRules] Загрузка пропущена — уже загружены ранее');
      return;
    }

    console.log('🔁 [useRules] Начало загрузки всех правил...');
    try {
      setLoading(true);
      setError(null);

      const briefList = await api.getRulesList();
      console.log('✅ [useRules] Получен краткий список:', briefList);

      if (!briefList.length) {
        console.log('ℹ️ [useRules] Нет правил — оставляем предыдущие');
        return;
      }

      const validRules = [];

      for (const brief of briefList) {
        try {
          console.log(`📥 [useRules] Загружаю правило: ${brief.id}`);
          const rule = await fetchWithRetry(() => api.getRuleById(brief.id));
          console.log(`✅ [useRules] Успешно загружено правило: ${brief.id}`, rule);
          validRules.push(rule);
        } catch (err) {
          console.warn(`❌ Не удалось загрузить правило после повторов: ${brief.id}`, err);
        }

        await new Promise(resolve => setTimeout(resolve, 200));
      }

      console.log('✅ [useRules] Все правила обработаны. Загружено:', validRules.length);

      rulesCache = validRules;
      setRules(validRules);
      hasLoaded = true;
    } catch (err) {
      console.error('💥 [useRules] Критическая ошибка при загрузке правил:', err);
      setError(err.message);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    if (!enabled) return;
    loadAllRules();
  }, [loadAllRules, enabled]);

  useEffect(() => {
    const unsubscribe = subscribeToWebSocket((data) => {
      setRules(prev => {
        let nextRules = [...prev];

        if (data.type === 'rule_update' || data.type === 'rule_create') {
          const existsIndex = nextRules.findIndex(r => r.id === data.rule.id);
          if (existsIndex > -1) {
            nextRules[existsIndex] = data.rule;
            console.log(`✏️ [WS] Обновлено правило: ${data.rule.id}`);
          } else {
            nextRules.push(data.rule);
            console.log(`🆕 [WS] Добавлено новое правило: ${data.rule.id}`);
          }
        }

        if (data.type === 'rule_delete') {
          nextRules = nextRules.filter(r => r.id !== data.id);
          console.log(`🗑️ [WS] Удалено правило: ${data.id}. Осталось: ${nextRules.length}`);
        }

        rulesCache = nextRules;
        return nextRules;
      });
    });

    return () => unsubscribe();
  }, []);

  useEffect(() => {
    const handleRuleUpdated = (e) => {
      const { action, rule: updatedRule } = e.detail;

      setRules(prev => {
        let nextRules = [...prev];

        if (action === 'delete') {
          nextRules = nextRules.filter(r => r.id !== updatedRule.id);
        } else if (action === 'create' || action === 'update') {
          const existsIndex = nextRules.findIndex(r => r.id === updatedRule.id);
          if (existsIndex > -1) {
            nextRules[existsIndex] = updatedRule;
          } else {
            nextRules.push(updatedRule);
          }
        }

        rulesCache = nextRules;
        return nextRules;
      });
    };

    window.addEventListener('rule_updated', handleRuleUpdated);
    return () => window.removeEventListener('rule_updated', handleRuleUpdated);
  }, []);

  const reload = useCallback(() => {
    console.log('🔁 [useRules] Принудительная перезагрузка правил...');
    hasLoaded = false;
    rulesCache = [];
    setRules([]);
    setLoading(true);
    loadAllRules();
  }, [loadAllRules]);

  useEffect(() => {
    console.log('📊 [useRules] Хук обновлён → rules:', rules.length, 'loading:', loading);
  }, [rules, loading]);

  return { rules, loading, error, reload };
};