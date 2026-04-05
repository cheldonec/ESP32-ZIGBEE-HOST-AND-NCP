// src/pages/DevicesPage.js
import { useState } from 'react';
import DeviceSidebar from '../components/DeviceSidebar';
import DeviceDetails from '../components/DeviceDetails';
import { useDevices } from '../hooks/useDevices';

export default function DevicesPage() {
  const { devices: fullDevices, loading, error } = useDevices();
  const [selectedDevice, setSelectedDevice] = useState(null);

  // Краткий список для сайдбара (из /api/devices)
  const briefList = fullDevices.map(dev => ({
    ieee: dev.ieee_addr,
    short: parseInt(dev.short_addr.replace('0x', ''), 16),
    friendly_name: dev.name || dev.friendly_name,
    online: dev.is_online,
    linkquality: dev.lqi
  }));

  // При клике на устройство — ищем полную модель
  const handleSelect = (briefDev) => {
    const full = fullDevices.find(d => d.ieee_addr === briefDev.ieee);
    setSelectedDevice(full || null);
  };

  if (loading) {
    return (
      <div className="flex-1 p-8 text-center text-gray-500">
        <p>Загрузка устройств...</p>
      </div>
    );
  }

  if (error) {
    return (
      <div className="flex-1 p-8 text-center text-red-500">
        <p>Ошибка: {error}</p>
      </div>
    );
  }

  return (
    <div className="flex h-screen bg-gray-900 text-white">
      <DeviceSidebar
        devices={briefList}
        selectedIEEE={selectedDevice?.ieee_addr}
        onSelect={handleSelect}
      />
      <DeviceDetails device={selectedDevice} />
    </div>
  );
}