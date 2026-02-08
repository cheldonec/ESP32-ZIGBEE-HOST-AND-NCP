// src/components/BindModal.js
import React from 'react';

const BindModal = ({ show, onClose, bindingTargets, bindForm, setBindForm, performBind }) => {
  if (!show) return null;

  const getInputClusters = (short, epId) => {
    const dev = bindingTargets.find((d) => d.short === short);
    const ep = dev?.endpoints.find((e) => e.id === epId);
    return ep?.input_clusters || [];
  };

  return (
    <div className="modal-overlay">
      <div className="modal-content">
        <h3>Создать привязку</h3>
        <p className="modal-description">
          <strong>Источник</strong> будет отправлять отчёты <strong>получателю</strong>
        </p>

        <label>1. Источник (input):</label>
        <select
          value={bindForm.srcDevice}
          onChange={(e) =>
            setBindForm((prev) => ({
              ...prev,
              srcDevice: e.target.value,
              srcEp: '',
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

        {bindForm.srcDevice && (
          <>
            <label>2. Endpoint (input):</label>
            <select
              value={bindForm.srcEp}
              onChange={(e) =>
                setBindForm((prev) => ({
                  ...prev,
                  srcEp: e.target.value,
                  cluster: '',
                }))
              }
              className="form-select"
            >
              <option value="">Выберите endpoint...</option>
              {bindingTargets
                .find((d) => d.short === Number(bindForm.srcDevice))
                ?.endpoints.filter((ep) => (ep.input_clusters || []).length > 0)
                .map((ep) => (
                  <option key={ep.id} value={ep.id}>
                    EP {ep.id}
                  </option>
                ))}
            </select>
          </>
        )}

        {bindForm.srcEp && (
          <>
            <label>3. Кластер (input):</label>
            <select
              value={bindForm.cluster}
              onChange={(e) =>
                setBindForm((prev) => ({ ...prev, cluster: e.target.value }))
              }
              className="form-select"
            >
              <option value="">Выберите кластер...</option>
              {getInputClusters(Number(bindForm.srcDevice), Number(bindForm.srcEp)).map(
                (id) => (
                  <option key={id} value={id}>
                    0x{id.toString(16).padStart(4, '0')} (input)
                  </option>
                )
              )}
            </select>
          </>
        )}

        <label>4. Получатель (output):</label>
        <select
          value={bindForm.tgtDevice}
          onChange={(e) =>
            setBindForm((prev) => ({
              ...prev,
              tgtDevice: e.target.value,
              tgtEp: '',
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

        {bindForm.tgtDevice && (
          <>
            <label>5. Endpoint (output):</label>
            <select
              value={bindForm.tgtEp}
              onChange={(e) =>
                setBindForm((prev) => ({ ...prev, tgtEp: e.target.value }))
              }
              className="form-select"
            >
              <option value="">Выберите endpoint...</option>
              {bindingTargets
                .find((d) => d.short === Number(bindForm.tgtDevice))
                ?.endpoints.filter((ep) => (ep.output_clusters || []).length > 0)
                .map((ep) => (
                  <option key={ep.id} value={ep.id}>
                    EP {ep.id}
                  </option>
                ))}
            </select>
          </>
        )}

        <div className="modal-buttons">
          <button onClick={performBind} className="btn-primary">
            Привязать
          </button>
          <button onClick={onClose} className="btn-danger">
            Отмена
          </button>
        </div>
      </div>
    </div>
  );
};

export default BindModal;