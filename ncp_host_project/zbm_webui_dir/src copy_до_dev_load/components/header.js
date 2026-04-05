// src/components/Header.js
export default function Header({ coordinator }) {
  if (!coordinator) return null;

  return (
    <header className="bg-gradient-to-r from-gray-900 to-gray-850 border-b border-blue-900/30 px-5 py-3 text-sm text-gray-200 flex items-center justify-between font-mono shadow-sm">
      <div className="flex items-center gap-6">
        <div className="flex items-center gap-2">
          <span className="text-blue-400 text-lg">🌀</span>
          <span className="text-sm">
            <strong className="text-white">Zigbee:</strong> PAN {coordinator.pan_id} | CH {coordinator.radio_channel}
          </span>
        </div>

        <div className="flex items-center gap-2">
          <span className="text-green-400 text-lg">📶</span>
          <span className="text-sm">
            <strong className="text-white">AP:</strong> {coordinator.wifi_ap_ssid}
          </span>
        </div>
      </div>

      <div className="text-xs text-gray-400 tabular-nums tracking-tight bg-gray-800/60 px-3 py-1 rounded border border-gray-700/50">
        {coordinator.ieee_addr}
      </div>
    </header>
  );
}