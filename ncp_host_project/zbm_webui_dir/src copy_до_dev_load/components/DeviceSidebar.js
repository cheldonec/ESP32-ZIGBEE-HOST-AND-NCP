// src/components/DeviceSidebar.js
export default function DeviceSidebar({ devices, selectedIEEE, onSelect }) {
  return (
    <div className="w-80 bg-gray-900/40 border-r border-gray-700/50 flex-shrink-0 flex flex-col backdrop-blur-sm">
      <h2 className="p-4 text-xl font-semibold text-white border-b border-gray-700/60 bg-gray-800/50">
        📱 Устройства
      </h2>
      <nav className="flex-1 overflow-y-auto">
        {devices.length === 0 ? (
          <p className="p-6 text-gray-500 text-sm italic text-center">Нет подключённых устройств</p>
        ) : (
          devices.map((dev) => (
            <div
              key={dev.ieee}
              onClick={() => onSelect(dev)}
              className={`device-item group cursor-pointer hover:bg-gray-700/50 p-3 border-b border-gray-800 ${
                dev.ieee === selectedIEEE ? 'bg-gray-700/70' : ''
              }`}
            >
              <div className="flex items-center justify-between">
                <div className="flex-1">
                  <div className="flex items-center gap-3">
                    <span className="text-lg">{dev.online ? '🟢' : '🔴'}</span>
                    <div>
                      <div className="font-semibold text-white text-sm">
                        {dev.friendly_name}
                      </div>
                      <div className="text-xs text-gray-400 mt-0.5">
                        0x{dev.short.toString(16).toUpperCase().padStart(4, '0')} • LQI: {dev.linkquality || '?'}
                      </div>
                    </div>
                  </div>
                </div>
                <span className={`device-status ${dev.online ? 'status-online' : 'status-offline'}`}>
                  {dev.online ? 'Online' : 'Offline'}
                </span>
              </div>
            </div>
          ))
        )}
      </nav>
    </div>
  );
}