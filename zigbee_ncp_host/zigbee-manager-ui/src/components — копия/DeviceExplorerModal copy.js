// src/components/DeviceExplorerModal.js
import React, { useState } from 'react';

const DeviceExplorerModal = ({ show, onClose, devices }) => {
  const [expandedDevices, setExpandedDevices] = useState(new Set());
  const [expandedEndpoints, setExpandedEndpoints] = useState(new Set());
  const [expandedClusters, setExpandedClusters] = useState(new Set());
  const [discoveryForm, setDiscoveryForm] = useState({
    short_addr: '',
    endpoint: '',
    cluster: '',
    start_attr: 0,
    max_attr_count: 10,
  });

  if (!show) return null;

  const toggleDevice = (short) => {
    const newSet = new Set(expandedDevices);
    if (newSet.has(short)) newSet.delete(short);
    else newSet.add(short);
    setExpandedDevices(newSet);
  };

  const toggleEndpoint = (key) => {
    const newSet = new Set(expandedEndpoints);
    if (newSet.has(key)) newSet.delete(key);
    else newSet.add(key);
    setExpandedEndpoints(newSet);
  };

  const toggleCluster = (key) => {
    const newSet = new Set(expandedClusters);
    if (newSet.has(key)) newSet.delete(key);
    else newSet.add(key);
    setExpandedClusters(newSet);
  };

  const handleDiscover = () => {
    const { short_addr, endpoint, cluster, start_attr, max_attr_count } = discoveryForm;
    if (!short_addr || !cluster) {
      alert('Заполните адрес и кластер');
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

  // === Стандартные атрибуты по кластеру ===
  const getStandardAttrsForCluster = (clusterId, clusterData) => {
    const attrs = [];

    if (clusterId === 0 && clusterData) {
      attrs.push(
        { id: 0x0000, name: 'ZCL Version', value: clusterData.zcl_version },
        { id: 0x0001, name: 'App Version', value: clusterData.application_version },
        { id: 0x0002, name: 'Stack Version', value: clusterData.stack_version },
        { id: 0x0003, name: 'HW Version', value: clusterData.hw_version },
        { id: 0x0004, name: 'Manufacturer', value: clusterData.manufacturer_name },
        { id: 0x0005, name: 'Model ID', value: clusterData.model_id },
        { id: 0x0006, name: 'Date Code', value: clusterData.date_code },
        { id: 0x0007, name: 'Power Source', value: clusterData.power_source_text || clusterData.power_source }
      );
    }

    if (clusterId === 1 && clusterData) {
      attrs.push(
        { id: 0x0020, name: 'Battery Voltage', value: `${(clusterData.battery_voltage * 0.1).toFixed(1)} V` },
        { id: 0x0021, name: 'Battery %', value: clusterData.battery_percentage === 0xFF ? 'Unknown' : `${(clusterData.battery_percentage / 2).toFixed(1)}%` }
      );
    }

    if (clusterId === 1026 && clusterData) {
      const val = clusterData.measured_value;
      attrs.push({
        id: 0x0000,
        name: 'Measured Value',
        value: val === 0x8000 ? 'Unknown' : `${(val / 100).toFixed(2)} °C`
      });
    }

    if (clusterId === 1029 && clusterData) {
      const val = clusterData.measured_value;
      attrs.push({
        id: 0x0000,
        name: 'Measured Value',
        value: val === 0xFFFF ? 'Unknown' : `${(val / 100).toFixed(2)} %`
      });
    }

    if (clusterId === 6 && clusterData) {
      attrs.push({
        id: 0x0000,
        name: 'On/Off',
        value: clusterData.on ? 'ON' : 'OFF'
      });
    }

    return attrs;
  };

  // === Отображение всех атрибутов одного кластера ===
  const renderClusterAttributes = (clusterId, standardAttrs, customAttrs, deviceShort, endpointId) => {
    const clusterKey = `${deviceShort}-${endpointId}-${clusterId}`;
    const isExpanded = expandedClusters.has(clusterKey);

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
          onClick={() => toggleCluster(clusterKey)}
        >
          <span>Атрибуты {isExpanded ? '▼' : '▶'}</span>
        </div>

        {isExpanded && (
          <div style={{ marginLeft: '20px', marginTop: '6px', fontSize: '12px', color: '#ccc' }}>
            {/* Стандартные атрибуты */}
            {standardAttrs.map((attr) => (
              <div key={attr.id} style={{ marginBottom: '4px' }}>
                <strong>0x{attr.id.toString(16).padStart(4, '0')}</strong>: {attr.name} ={' '}
                <span style={{ color: '#aaa' }}>{String(attr.value)}</span>
              </div>
            ))}

            {/* Кастомные атрибуты */}
            {Array.isArray(customAttrs) && customAttrs.length > 0 ? (
              customAttrs.map((attr) => (
                <div key={attr.id} style={{ marginBottom: '4px' }}>
                  <strong>0x{attr.id.toString(16).padStart(4, '0')}</strong>: {attr.attr_id_text || 'Unknown'}{' '}
                  <span style={{ color: '#888' }}>(type: {attr.type}, val: {String(attr.p_value)})</span>
                </div>
              ))
            ) : (
              <div style={{ color: '#666', fontStyle: 'italic' }}>Нет обнаруженных атрибутов</div>
            )}
          </div>
        )}
      </div>
    );
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
            {devices.map((dev) => {
              const fullDev = dev._full;
              if (!fullDev) return null;

              const addrHex = dev.short.toString(16).padStart(4, '0').toUpperCase();

              return (
                <div key={dev.short} style={{ marginBottom: '12px' }}>
                  {/* Устройство */}
                  <div
                    style={{
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      padding: '8px',
                      background: '#2d2d2d',
                      borderRadius: '6px',
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                    }}
                    onClick={() => toggleDevice(dev.short)}
                  >
                    <span>📦 {dev.name} (0x{addrHex})</span>
                    <span>{expandedDevices.has(dev.short) ? '▼' : '▶'}</span>
                  </div>

                  {expandedDevices.has(dev.short) && (
                    <div style={{ marginLeft: '20px', marginTop: '8px', borderLeft: '2px solid #444', paddingLeft: '12px' }}>
                      {/* === КЛАСТЕРЫ НА УРОВНЕ УСТРОЙСТВА === */}
                      {fullDev.device_basic_cluster && (
                        <div style={{ marginBottom: '10px' }}>
                          <div
                            style={{
                              display: 'flex',
                              justifyContent: 'space-between',
                              alignItems: 'center',
                              padding: '6px',
                              background: '#3a3a3a',
                              borderRadius: '6px',
                              fontSize: '14px',
                            }}
                          >
                            <span>⚙️ Basic Cluster (0x0000)</span>
                            <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                              <input
                                type="number"
                                placeholder="от"
                                defaultValue="0"
                                style={{ width: '60px', padding: '2px' }}
                                onChange={(e) =>
                                  setDiscoveryForm((prev) => ({
                                    ...prev,
                                    short_addr: dev.short,
                                    endpoint: '',
                                    cluster: 0,
                                    start_attr: Number(e.target.value),
                                  }))
                                }
                              />
                              <input
                                type="number"
                                placeholder="кол-во"
                                defaultValue="10"
                                style={{ width: '60px', padding: '2px' }}
                                onChange={(e) =>
                                  setDiscoveryForm((prev) => ({
                                    ...prev,
                                    max_attr_count: Number(e.target.value),
                                  }))
                                }
                              />
                              <button
                                className="btn-primary"
                                style={{ padding: '2px 6px', fontSize: '10px' }}
                                onClick={(e) => {
                                  e.stopPropagation();
                                  handleDiscover();
                                }}
                              >
                                🔍
                              </button>
                            </div>
                          </div>
                          {renderClusterAttributes(
                            0,
                            getStandardAttrsForCluster(0, fullDev.device_basic_cluster),
                            fullDev.server_BasicClusterObj?.nostandart_attr_array || [],
                            dev.short,
                            ''
                          )}
                        </div>
                      )}

                      {fullDev.device_power_config_cluster && (
                        <div style={{ marginBottom: '10px' }}>
                          <div
                            style={{
                              display: 'flex',
                              justifyContent: 'space-between',
                              alignItems: 'center',
                              padding: '6px',
                              background: '#3a3a3a',
                              borderRadius: '6px',
                              fontSize: '14px',
                            }}
                          >
                            <span>🔋 Power Config (0x0001)</span>
                            <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                              <input
                                type="number"
                                placeholder="от"
                                defaultValue="0"
                                style={{ width: '60px', padding: '2px' }}
                                onChange={(e) =>
                                  setDiscoveryForm((prev) => ({
                                    ...prev,
                                    short_addr: dev.short,
                                    endpoint: '',
                                    cluster: 1,
                                    start_attr: Number(e.target.value),
                                  }))
                                }
                              />
                              <input
                                type="number"
                                placeholder="кол-во"
                                defaultValue="10"
                                style={{ width: '60px', padding: '2px' }}
                                onChange={(e) =>
                                  setDiscoveryForm((prev) => ({
                                    ...prev,
                                    max_attr_count: Number(e.target.value),
                                  }))
                                }
                              />
                              <button
                                className="btn-primary"
                                style={{ padding: '2px 6px', fontSize: '10px' }}
                                onClick={(e) => {
                                  e.stopPropagation();
                                  handleDiscover();
                                }}
                              >
                                🔍
                              </button>
                            </div>
                          </div>
                          {renderClusterAttributes(
                            1,
                            getStandardAttrsForCluster(1, fullDev.device_power_config_cluster),
                            fullDev.server_PowerConfigurationClusterObj?.nostandart_attr_array || [],
                            dev.short,
                            ''
                          )}
                        </div>
                      )}

                      {/* === ЭНДПОИНТЫ === */}
                      {fullDev.endpoints?.map((ep) => {
                        const epKey = `${dev.short}-${ep.ep_id}`;
                        const epObj = fullDev.endpoints_array?.find((e) => e.ep_id === ep.ep_id);

                        // Известные кластеры
                        const knownClusters = [];
                        if (ep.device_basic_cluster) knownClusters.push({ id: 0, name: 'Basic', data: ep.device_basic_cluster });
                        if (ep.device_power_config_cluster) knownClusters.push({ id: 1, name: 'Power Config', data: ep.device_power_config_cluster });
                        if (ep.temperature) knownClusters.push({ id: 1026, name: 'Temperature', data: ep.temperature });
                        if (ep.humidity) knownClusters.push({ id: 1029, name: 'Humidity', data: ep.humidity });
                        if (ep.onoff) knownClusters.push({ id: 6, name: 'On/Off', data: ep.onoff });

                        // Неизвестные input-кластеры
                        const unknownInputClusters = epObj?.UnKnowninputClusters_array || [];

                        return (
                          <div key={ep.ep_id}>
                            {/* Эндпоинт */}
                            <div
                              style={{
                                display: 'flex',
                                justifyContent: 'space-between',
                                alignItems: 'center',
                                padding: '6px 0',
                                cursor: 'pointer',
                              }}
                              onClick={() => toggleEndpoint(epKey)}
                            >
                              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                <input
                                  type="checkbox"
                                  checked={!!ep.is_use_on_device}
                                  onChange={() => {}}
                                  onClick={(e) => e.stopPropagation()}
                                />
                                <span style={{ fontWeight: 500 }}>
                                  EP {ep.ep_id} — {ep.friendly_name || 'Unknown'}
                                </span>
                              </div>
                              <span>{expandedEndpoints.has(epKey) ? '▼' : '▶'}</span>
                            </div>

                            {expandedEndpoints.has(epKey) && (
                              <div style={{ marginLeft: '24px', marginTop: '6px' }}>
                                {/* Известные кластеры */}
                                {knownClusters.map((cl) => (
                                  <div key={cl.id} style={{ marginBottom: '10px' }}>
                                    <div
                                      style={{
                                        display: 'flex',
                                        justifyContent: 'space-between',
                                        alignItems: 'center',
                                        padding: '6px',
                                        background: '#333',
                                        borderRadius: '6px',
                                        fontSize: '14px',
                                      }}
                                    >
                                      <span>🔗 {cl.name} (0x{cl.id.toString(16).padStart(4, '0')})</span>
                                      <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                                        <input
                                          type="number"
                                          placeholder="от"
                                          defaultValue="0"
                                          style={{ width: '60px', padding: '2px' }}
                                          onChange={(e) =>
                                            setDiscoveryForm((prev) => ({
                                              ...prev,
                                              short_addr: dev.short,
                                              endpoint: ep.ep_id,
                                              cluster: cl.id,
                                              start_attr: Number(e.target.value),
                                            }))
                                          }
                                        />
                                        <input
                                          type="number"
                                          placeholder="кол-во"
                                          defaultValue="10"
                                          style={{ width: '60px', padding: '2px' }}
                                          onChange={(e) =>
                                            setDiscoveryForm((prev) => ({
                                              ...prev,
                                              max_attr_count: Number(e.target.value),
                                            }))
                                          }
                                        />
                                        <button
                                          className="btn-primary"
                                          style={{ padding: '2px 6px', fontSize: '10px' }}
                                          onClick={(e) => {
                                            e.stopPropagation();
                                            handleDiscover();
                                          }}
                                        >
                                          🔍
                                        </button>
                                      </div>
                                    </div>
                                    {renderClusterAttributes(
                                      cl.id,
                                      getStandardAttrsForCluster(cl.id, cl.data),
                                      (() => {
                                        if (cl.id === 0) return epObj?.server_BasicClusterObj?.nostandart_attr_array || [];
                                        if (cl.id === 1) return epObj?.server_PowerConfigurationClusterObj?.nostandart_attr_array || [];
                                        if (cl.id === 1026) return epObj?.server_TemperatureMeasurementClusterObj?.nostandart_attr_array || [];
                                        if (cl.id === 1029) return epObj?.server_HumidityMeasurementClusterObj?.nostandart_attr_array || [];
                                        if (cl.id === 6) return epObj?.server_OnOffClusterObj?.nostandart_attr_array || [];
                                        return [];
                                      })(),
                                      dev.short,
                                      ep.ep_id
                                    )}
                                  </div>
                                ))}

                                {/* Неизвестные input-кластеры */}
                                {unknownInputClusters.length > 0 &&
                                  unknownInputClusters.map((cl) => (
                                    <div key={cl.id} style={{ marginBottom: '10px' }}>
                                      <div
                                        style={{
                                          display: 'flex',
                                          justifyContent: 'space-between',
                                          alignItems: 'center',
                                          padding: '6px',
                                          background: '#444',
                                          borderRadius: '6px',
                                          fontSize: '14px',
                                        }}
                                      >
                                        <span>❓ Unknown Input Cluster (0x{cl.id.toString(16).padStart(4, '0')})</span>
                                        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                                          <input
                                            type="number"
                                            placeholder="от"
                                            defaultValue="0"
                                            style={{ width: '60px', padding: '2px' }}
                                            onChange={(e) =>
                                              setDiscoveryForm((prev) => ({
                                                ...prev,
                                                short_addr: dev.short,
                                                endpoint: ep.ep_id,
                                                cluster: cl.id,
                                                start_attr: Number(e.target.value),
                                              }))
                                            }
                                          />
                                          <input
                                            type="number"
                                            placeholder="кол-во"
                                            defaultValue="10"
                                            style={{ width: '60px', padding: '2px' }}
                                            onChange={(e) =>
                                              setDiscoveryForm((prev) => ({
                                                ...prev,
                                                max_attr_count: Number(e.target.value),
                                              }))
                                            }
                                          />
                                          <button
                                            className="btn-primary"
                                            style={{ padding: '2px 6px', fontSize: '10px' }}
                                            onClick={(e) => {
                                              e.stopPropagation();
                                              handleDiscover();
                                            }}
                                          >
                                            🔍
                                          </button>
                                        </div>
                                      </div>
                                      {renderClusterAttributes(
                                        cl.id,
                                        [],
                                        [],
                                        dev.short,
                                        ep.ep_id
                                      )}
                                    </div>
                                  ))}

                                {/* Output-кластеры */}
                                {(epObj?.output_clusters_array || []).length > 0 && (
                                  <div style={{ marginLeft: '10px', marginTop: '6px', fontSize: '13px', color: '#888' }}>
                                    <strong>📤 Output Clusters:</strong>
                                    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px', marginTop: '4px' }}>
                                      {(epObj.output_clusters_array || []).map((id) => (
                                        <span
                                          key={id}
                                          style={{
                                            background: '#555',
                                            padding: '2px 6px',
                                            borderRadius: '4px',
                                            fontSize: '12px',
                                          }}
                                        >
                                          0x{id.toString(16).padStart(4, '0')}
                                        </span>
                                      ))}
                                    </div>
                                  </div>
                                )}
                              </div>
                            )}
                          </div>
                        );
                      })}
                    </div>
                  )}
                </div>
              );
            })}
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