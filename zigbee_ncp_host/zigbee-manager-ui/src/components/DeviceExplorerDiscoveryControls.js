// src/components/DeviceExplorerDiscoveryControls.js
import React from 'react';

export const DiscoveryControls = ({ form, onChange, onDiscover }) => {
  return (
    <div style={{ display: 'flex', gap: '12px', alignItems: 'center', fontSize: '12px' }}>
      {/* Заголовок */}
      <span style={{ fontSize: '13px', color: '#888', whiteSpace: 'nowrap' }}>Исследовать</span>

      {/* Группа: начать с атрибута */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '2px' }}>
        <span style={{ fontSize: '11px', color: '#777', whiteSpace: 'nowrap' }}>начать с</span>
        <input
          type="number"
          placeholder="0"
          defaultValue={form.start_attr}
          style={{ width: '70px', padding: '2px', fontSize: '12px' }}
          onChange={(e) =>
            onChange({
              ...form,
              start_attr: Number(e.target.value),
            })
          }
        />
      </div>

      {/* Группа: количество */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '2px' }}>
        <span style={{ fontSize: '11px', color: '#777', whiteSpace: 'nowrap' }}>количество</span>
        <input
          type="number"
          placeholder="10"
          defaultValue={form.max_attr_count}
          style={{ width: '70px', padding: '2px', fontSize: '12px' }}
          onChange={(e) =>
            onChange({
              ...form,
              max_attr_count: Number(e.target.value),
            })
          }
        />
      </div>

      {/* Кнопка */}
      <button
        className="btn-primary"
        title="Запустить обнаружение атрибутов"
        style={{ padding: '4px 8px', fontSize: '12px', marginTop: '8px' }}
        onClick={(e) => {
          e.stopPropagation();
          onDiscover();
        }}
      >
        🔍
      </button>
    </div>
  );
};