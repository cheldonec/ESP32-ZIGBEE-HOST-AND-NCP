// src/components/ReportModal.js
import React from 'react';

const ReportModal = ({
  show,
  onClose,
  bindingTargets,
  reportForm,
  setReportForm,
  onSubmit, // ← исправлено
}) => {
  if (!show) return null;

  const getInputClusters = (short, epId) => {
    const dev = bindingTargets.find((d) => d.short === short);
    const ep = dev?.endpoints.find((e) => e.id === epId);
    return ep?.input_clusters || [];
  };

  const nameMap = {
    0x0006: 'On/Off',
    0x0402: 'Temperature',
    0x0405: 'Humidity',
    0x0001: 'Power Config',
  };

  return (
    <div className="modal-overlay">
      <div className="modal-content">
        <h3>📊 Настроить Reporting</h3>

        <label>Устройство:</label>
        <select
          value={reportForm.device}
          onChange={(e) =>
            setReportForm((prev) => ({
              ...prev,
              device: e.target.value,
              ep: '',
              cluster: '',
            }))
          }
          className="form-select"
        >
          <option value="">Выберите устройство...</option>
          {bindingTargets.map((dev) => (
            <option key={dev.short} value={dev.short}>
              {dev.name} (0x{dev.short.toString(16).padStart(4, '0').toUpperCase()})
            </option>
          ))}
        </select>

        {reportForm.device && (
          <>
            <label>Endpoint:</label>
            <select
              value={reportForm.ep}
              onChange={(e) =>
                setReportForm((prev) => ({
                  ...prev,
                  ep: e.target.value,
                  cluster: '',
                }))
              }
              className="form-select"
            >
              <option value="">Выберите endpoint...</option>
              {bindingTargets
                .find((d) => d.short === Number(reportForm.device))
                ?.endpoints.filter((ep) => (ep.input_clusters || []).length > 0)
                .map((ep) => (
                  <option key={ep.id} value={ep.id}>
                    EP {ep.id}
                  </option>
                ))}
            </select>
          </>
        )}

        {reportForm.ep && (
          <>
            <label>Кластер:</label>
            <select
              value={reportForm.cluster}
              onChange={(e) =>
                setReportForm((prev) => ({ ...prev, cluster: e.target.value }))
              }
              className="form-select"
            >
              <option value="">Выберите кластер...</option>
              {getInputClusters(Number(reportForm.device), Number(reportForm.ep)).map(
                (id) => (
                  <option key={id} value={id}>
                    0x{id.toString(16).padStart(4, '0')} ({nameMap[id] || 'Unknown'})
                  </option>
                )
              )}
            </select>
          </>
        )}

        {reportForm.cluster && (
          <div className="report-fields-grid">
            <div>
              <label>Мин. интервал (сек):</label>
              <input
                type="number"
                value={reportForm.min}
                onChange={(e) =>
                  setReportForm((prev) => ({ ...prev, min: Number(e.target.value) }))
                }
                min="1"
                className="form-input"
                placeholder="1"
              />
            </div>

            <div>
              <label>Макс. интервал (сек):</label>
              <input
                type="number"
                value={reportForm.max}
                onChange={(e) =>
                  setReportForm((prev) => ({ ...prev, max: Number(e.target.value) }))
                }
                min="1"
                className="form-input"
                placeholder="300"
              />
            </div>

            <div>
              <label>Изменение для отчёта (опц.):</label>
              <input
                type="number"
                value={reportForm.change}
                onChange={(e) =>
                  setReportForm((prev) => ({ ...prev, change: Number(e.target.value) }))
                }
                min="0"
                step="any"
                className="form-input"
                placeholder="0"
              />
            </div>
          </div>
        )}

        <div className="modal-buttons">
          <button onClick={onSubmit} className="btn-primary"> {/* ← теперь вызывается */}
            Применить
          </button>
          <button onClick={onClose} className="btn-danger">
            Отмена
          </button>
        </div>
      </div>
    </div>
  );
};

export default ReportModal;