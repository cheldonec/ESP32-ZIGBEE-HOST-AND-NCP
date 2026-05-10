// src/api/websocket.js

let ws = null;
let isReconnecting = false;
let reconnectDelay = 1000; // начальная задержка — 1 сек
const MAX_RECONNECT_DELAY = 10000; // максимум 10 сек
const subscribers = [];

// Подписка на события WebSocket
export const subscribeToWebSocket = (callback) => {
  if (typeof callback !== 'function') {
    console.warn('⚠️ Подписка пропущена: callback не является функцией');
    return () => {};
  }

  subscribers.push(callback);
  return () => {
    const index = subscribers.indexOf(callback);
    if (index > -1) {
      subscribers.splice(index, 1);
    }
  };
};

// Отправка сообщения через WebSocket
export const sendWebSocketMessage = (data) => {
  if (ws && ws.readyState === WebSocket.OPEN) {
    try {
      ws.send(JSON.stringify(data));
      console.debug('📤 Отправлено WS-сообщение:', data);
    } catch (err) {
      console.error('❌ Ошибка отправки WS:', err);
    }
  } else {
    console.warn('🟡 WebSocket не подключён. Сообщение не отправлено:', data);
  }
};

// Инициализация WebSocket
export const initWebSocket = () => {
  // Защита от повторной инициализации
  if (isReconnecting || ws?.readyState === WebSocket.CONNECTING) {
    console.debug('🟡 initWebSocket: уже подключается...');
    return;
  }

  console.log(`🚀 Попытка подключения к WebSocket: ws://${window.location.host}/ws`);
  ws = new WebSocket(`ws://${window.location.host}/ws`);

  ws.onopen = () => {
    console.log('✅ WebSocket соединён');
    isReconnecting = false;
    reconnectDelay = 1000; // сбрасываем задержку
    window.ws = ws;

    // Уведомляем подписчиков
    window.dispatchEvent(new CustomEvent('websocket_open'));
  };

  ws.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data);
      console.debug('📩 Получено WS-сообщение:', data);

      // Рассылаем всем подписчикам
      subscribers.forEach((callback) => {
        try {
          callback(data);
        } catch (err) {
          console.error('❌ Ошибка в обработчике подписчика:', err);
        }
      });
    } catch (err) {
      console.error('❌ Ошибка парсинга WS:', event.data, err);
    }
  };

  ws.onerror = (err) => {
    console.error('⚠️ Ошибка WebSocket:', err);
    // onerror не означает закрытие, но может предшествовать ему
  };

  ws.onclose = () => {
    console.log(`🔴 WebSocket закрыт. Переподключение через ${reconnectDelay} мс...`);
    window.ws = null;

    isReconnecting = true;
    setTimeout(() => {
      initWebSocket();
      // Увеличиваем задержку экспоненциально
      reconnectDelay = Math.min(reconnectDelay * 2, MAX_RECONNECT_DELAY);
    }, reconnectDelay);
  };
};

// Закрытие соединения
export const closeWebSocket = () => {
  if (ws) {
    console.log('🔌 Закрытие WebSocket...');
    ws.close();
    ws = null;
  }
  isReconnecting = false;
  reconnectDelay = 1000;
};