// src/components/DeviceExplorerModal.js
import React, { useState } from 'react';
import { DeviceItem } from './DeviceExplorerDeviceItem';

const DeviceExplorerModal = ({ show, onClose, devices }) => {
  const [discoveryForm, setDiscoveryForm] = useState({
    short_addr: '',
    endpoint: '',
    cluster: '',
    start_attr: 0,
    max_attr_count: 10,
  });

  if (!show) return null;

  const handleDiscover = (form) => {
    const { short_addr, endpoint, cluster, start_attr, max_attr_count } = form;
    if (short_addr === '' || short_addr == null || cluster == null) {
      alert('Ошибка приложения "Пустой адрес или кластер"');
      return;
    }

    try {
      const ws = new WebSocket(`ws://${window.location.host}/ws`);
      ws.onopen = () => {
        ws.send(
          JSON.stringify({
            cmd: 'discover_attributes',
            short_addr: Number(short_addr),
            endpoint: endpoint !== '' ? Number(endpoint) : null,
            cluster_id: Number(cluster),
            start_attr: Number(start_attr),
            max_attr_count: Number(max_attr_count),
          })
        );
        setTimeout(() => ws.close(), 100);
      };
    } catch (err) {
      console.error('Ошибка:', err);
      alert('Не удалось отправить команду');
    }
  };

  const handleReadAttribute = (short_addr, endpoint, cluster_id, attr_id) => {
    try {
      const ws = new WebSocket(`ws://${window.location.host}/ws`);
      ws.onopen = () => {
        ws.send(
          JSON.stringify({
            cmd: 'read_attribute',
            short_addr: Number(short_addr),
            endpoint: endpoint !== '' ? Number(endpoint) : null,
            cluster_id: Number(cluster_id),
            attr_id: Number(attr_id),
          })
        );
        setTimeout(() => ws.close(), 100);
      };
    } catch (err) {
      console.error('Ошибка чтения атрибута:', err);
      alert('Не удалось прочитать атрибут');
    }
  };

  const handleWriteAttribute = (short_addr, endpoint, cluster_id, attr_id, value, type) => {
    try {
      const ws = new WebSocket(`ws://${window.location.host}/ws`);
      ws.onopen = () => {
        ws.send(
          JSON.stringify({
            cmd: 'write_attribute',
            short_addr: Number(short_addr),
            endpoint: endpoint !== '' ? Number(endpoint) : null,
            cluster_id: Number(cluster_id),
            attr_id: Number(attr_id),
            value: value,
            type: type
          })
        );
        setTimeout(() => ws.close(), 100);
      };
    } catch (err) {
      console.error('Ошибка записи атрибута:', err);
      alert('Не удалось записать атрибут');
    }
  };

  return (
    <div className="modal-overlay">
      <div className="modal-content" style={{ maxWidth: '900px', maxHeight: '90vh', overflow: 'auto' }}>
        <h3>🔍 Исследование устройств</h3>
        <p style={{ fontSize: '13px', color: '#aaa', marginBottom: '16px' }}>
          Просмотр структуры устройств и атрибутов.
        </p>

        {devices.length === 0 ? (
          <p>Нет доступных устройств.</p>
        ) : (
          <div>
           {devices.map((dev) => (
            <DeviceItem
              key={dev.short}
              dev={dev}
              fullDev={dev._full} // ← явно передаём
              discoveryForm={discoveryForm}
              setDiscoveryForm={setDiscoveryForm}
              onDiscover={handleDiscover}
              onReadAttribute={handleReadAttribute}
              onWriteAttribute={handleWriteAttribute}
            />
          ))}
          </div>
        )}

        <div className="modal-buttons">
          <button onClick={onClose} className="btn-danger">
            Закрыть
          </button>
        </div>
      </div>
    </div>
  );
};

export default DeviceExplorerModal;