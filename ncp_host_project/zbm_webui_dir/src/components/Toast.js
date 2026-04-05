// src/components/Toast.js
import { useEffect } from 'react';

export default function Toast({ id, message, onClose }) {
  useEffect(() => {
    const timer = setTimeout(() => {
      onClose(id);
    }, 5000);

    return () => clearTimeout(timer);
  }, [id, onClose]);

  return (
    <div className="toast-item">
      <span className="toast-message">{message}</span>
      <button onClick={() => onClose(id)} className="toast-close">
        ✕
      </button>
    </div>
  );
}