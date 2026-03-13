// src/components/DeviceExplorerAttributeList.js
import React, { useState } from 'react';
import { formatAttributeValue, getAttrTypeName } from '../utils/zclAttributes'; 


//const getAttrTypeName = (type) => ZCL_ATTR_TYPES[type] || `0x${type.toString(16)}`;

export const AttributeList = ({
  clusterId,
  standardAttrs,
  customAttrs,
  deviceShort,
  endpointId,
  onReadAttribute,
  onWriteAttribute,
}) => {
  const [expanded, setExpanded] = useState(false);

  return (
    <div>
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          fontSize: '13px',
          cursor: 'pointer',
          marginTop: '4px',
          color: '#888',
        }}
        onClick={() => setExpanded(!expanded)}
      >
        <span>Атрибуты {expanded ? '▼' : '▶'}</span>
      </div>

      {expanded && (
        <div style={{ marginLeft: '20px', marginTop: '6px', fontSize: '12px', color: '#ccc' }}>
          {[...standardAttrs, ...customAttrs].map((attr) => {
            //const isCustom = attr.hasOwnProperty('attr_id_text') || attr.hasOwnProperty('value_hex') || attr.hasOwnProperty('p_value');
            const displayValue = formatAttributeValue(attr);
            const name = attr.attr_id_text || attr.name || 'Unknown';
            const type = attr.type ?? 0;

            return (
              <div
                key={attr.id}
                style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  marginBottom: '6px',
                  padding: '4px 0',
                  borderBottom: '1px solid #444',
                }}
              >
                <div style={{ flex: 1 }}>
                  <strong>0x{attr.id.toString(16).padStart(4, '0')}</strong>: {name} ={' '}
                  <span style={{ color: '#aaa' }}>{displayValue}</span>
                </div>
                <div
                  style={{
                    display: 'flex',
                    gap: '6px',
                    alignItems: 'center',
                    fontSize: '11px',
                    whiteSpace: 'nowrap',
                  }}
                >
                  <span style={{ color: '#777' }}>тип: {getAttrTypeName(type)}</span>
                  <button
                    className="btn-small"
                    title="Прочитать значение с устройства"
                    style={{ padding: '2px 6px', fontSize: '10px' }}
                    onClick={(e) => {
                      e.stopPropagation();
                      onReadAttribute(deviceShort, endpointId, clusterId, attr.id);
                    }}
                  >
                    🔄
                  </button>
                  <input
                    type="text"
                    placeholder="значение"
                    style={{ width: '80px', padding: '2px', fontSize: '11px' }}
                    onKeyDown={(e) => {
                      if (e.key === 'Enter') {
                        onWriteAttribute(deviceShort, endpointId, clusterId, attr.id, e.target.value, type);
                        e.target.value = '';
                      }
                    }}
                  />
                  <button
                    className="btn-primary"
                    style={{ padding: '2px 6px', fontSize: '10px' }}
                    onClick={(e) => {
                      e.stopPropagation();
                      const input = e.currentTarget.previousElementSibling;
                      const value = input.value;
                      if (!value) {
                        alert('Введите значение');
                        return;
                      }
                      onWriteAttribute(deviceShort, endpointId, clusterId, attr.id, value, type);
                      input.value = '';
                    }}
                  >
                    ✏️
                  </button>
                </div>
              </div>
            );
          })}

          {[...standardAttrs, ...customAttrs].length === 0 && (
            <div style={{ color: '#666', fontStyle: 'italic' }}>Нет атрибутов</div>
          )}
        </div>
      )}
    </div>
  );
};