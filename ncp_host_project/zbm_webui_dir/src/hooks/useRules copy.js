// src/hooks/useRules.js
import { useState, useEffect, useCallback } from 'react';
import { api } from '../api/httpClient';
import { subscribeToWebSocket } from '../api/websocket'; // ← Добавляем импорт

export const useRules = () => {
  const [rules, setRules] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  const loadAllRules = useCallback(async () => {
    console.log('🔁 [useRules] Начало загрузки всех правил...');
    try {
      setLoading(true);
      setError(null);

      const briefList = await api.getRulesList();
      console.log('✅ [useRules] Получен краткий список:', briefList);

      if (!briefList.length) {
        // ❌ Не очищаем
        // setRules([]);
        console.log('ℹ️ [useRules] Нет правил — оставляем предыдущие');
        return;
      }

      const fullRulesPromises = briefList.map(async (brief) => {
        try {
          console.log(`📥 [useRules] Загружаю правило: ${brief.id}`);
          const rule = await api.getRuleById(brief.id);
          console.log(`✅ [useRules] Успешно загружено правило: ${brief.id}`, rule);
          return rule;
        } catch (err) {
          console.warn(`❌ [useRules] Не удалось загрузить правило: ${brief.id}`, err);
          return null;
        }
      });

      const results = await Promise.allSettled(fullRulesPromises);
      const validRules = results
        .filter(p => p.status === 'fulfilled' && p.value !== null)
        .map(p => p.value);

      console.log('✅ [useRules] Все правила обработаны. Загружено:', validRules.length);
      setRules(validRules);
    } catch (err) {
      console.error('💥 [useRules] Критическая ошибка при загрузке правил:', err);
      setError(err.message);
      //setRules([]); // не очищаем правила при ошибке
    } finally {
      setLoading(false);
    }
  }, []);

  // Загрузка при монтировании
  useEffect(() => {
    loadAllRules();
  }, [loadAllRules]);

  // 🔔 Подписка на WebSocket-события
  useEffect(() => {
    const unsubscribe = subscribeToWebSocket((data) => {
      // Пример ожидаемых сообщений (адаптируйте под ваш бэкенд):
      // { type: 'rule_update', rule: { id: 1, ... } }
      // { type: 'rule_delete', id: 1 }
      // { type: 'rule_create', rule: { ... } }

      if (data.type === 'rule_update' || data.type === 'rule_create') {
        setRules(prev => {
          const existsIndex = prev.findIndex(r => r.id === data.rule.id);
          if (existsIndex > -1) {
            console.log(`✏️ [WS] Обновлено правило: ${data.rule.id}`);
            return [
              ...prev.slice(0, existsIndex),
              data.rule,
              ...prev.slice(existsIndex + 1)
            ];
          } else {
            console.log(`🆕 [WS] Добавлено новое правило: ${data.rule.id}`);
            return [...prev, data.rule];
          }
        });
      }

      if (data.type === 'rule_delete') {
        const { id } = data;
        setRules(prev => {
          const newRules = prev.filter(r => r.id !== id);
          console.log(`🗑️ [WS] Удалено правило: ${id}. Осталось: ${newRules.length}`);
          return newRules;
        });
      }
    });

    // Отписка при размонтировании
    return () => unsubscribe();
  }, []); // Зависимости пустые — колбэк не зависит от внешних переменных

  // 🔔 Обработка локальных событий (например, после saveRule)
  useEffect(() => {
    const handleRuleUpdated = (e) => {
      console.log('🧩 [useRules] Получено событие window:rule_updated', e.detail);

      const { action, rule: updatedRule } = e.detail;

      setRules((prev) => {
        if (action === 'delete') {
          console.log(`🗑️ [useRules] Удаление правила: ${updatedRule.id}`);
          return prev.filter((r) => r.id !== updatedRule.id);
        }

        if (action === 'create' || action === 'update') {
          const existsIndex = prev.findIndex((r) => r.id === updatedRule.id);
          if (existsIndex > -1) {
            console.log(`✏️ [useRules] Обновление правила: ${updatedRule.id}`);
            return [...prev.slice(0, existsIndex), updatedRule, ...prev.slice(existsIndex + 1)];
          } else {
            console.log(`🆕 [useRules] Добавление нового правила: ${updatedRule.id}`);
            return [...prev, updatedRule];
          }
        }

        console.warn('⚠️ [useRules] Неизвестное действие в rule_updated:', action);
        return prev;
      });
    };

    window.addEventListener('rule_updated', handleRuleUpdated);
    return () => window.removeEventListener('rule_updated', handleRuleUpdated);
  }, []);

  const reload = loadAllRules;

  useEffect(() => {
    console.log('📊 [useRules] Хук обновлён → rules:', rules.length, 'loading:', loading);
  }, [rules, loading]);

  return { rules, loading, error, reload };
};