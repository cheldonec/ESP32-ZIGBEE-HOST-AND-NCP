// src/components/AttributeList.js
import React, { useState } from 'react';

const ZCL_ATTR_TYPES = {
  0x00: 'null',
  0x08: '8bit', 0x09: '16bit', 0x0a: '24bit', 0x0b: '32bit',
  0x0c: '40bit', 0x0d: '48bit', 0x0e: '56bit', 0x0f: '64bit',
  0x10: 'bool',
  0x18: '8bitmap', 0x19: '16bitmap', 0x1a: '24bitmap', 0x1b: '32bitmap',
  0x20: 'u8', 0x21: 'u16', 0x22: 'u24', 0x23: 'u32',
  0x28: 's8', 0x29: 's16', 0x2a: 's24', 0x2b: 's32',
  0x30: 'enum8', 0x31: 'enum16',
  0x38: 'semi', 0x39: 'single', 0x3a: 'double',
  0x41: 'octet str', 0x42: 'char str', 0x43: 'long octet str', 0x44: 'long char str',
  0x48: 'array', 0x4c: 'struct',
  0xe0: 'time of day', 0xe1: 'date', 0xe2: 'utc time',
  0xf0: 'IEEE addr', 0xf1: '128-bit key',
  0xff: 'invalid'
};

const getAttrTypeName = (type) => ZCL_ATTR_TYPES[type] || `0x${type.toString(16)}`;

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
            const isCustom = attr.hasOwnProperty('p_value');
            const value = isCustom ? attr.p_value : attr.value;
            const name = isCustom ? attr.attr_id_text || 'Unknown' : attr.name;
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
                  <span style={{ color: '#aaa' }}>{String(value)}</span>
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