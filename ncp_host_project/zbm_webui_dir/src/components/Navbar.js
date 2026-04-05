// src/components/Navbar.js
import { Link, useLocation } from 'react-router-dom';

const tabs = [
  { name: 'Устройства', path: '/', icon: '🔌' },
  { name: 'Связи', path: '/links', icon: '🔗' },
  { name: 'Сценарии', path: '/scenes', icon: '🎬' },
  { name: 'Настройки', path: '/settings', icon: '⚙️' },
  { name: 'Мониторинг', path: '/monitor', icon: '📊' },
];

export default function Navbar() {
  const location = useLocation();

  return (
    <nav className="bg-gray-800 border-b border-gray-700 px-2 py-1 sticky top-10 z-10">
      <div className="flex space-x-1 overflow-x-auto">
        {tabs.map((tab) => (
          <Link
            key={tab.path}
            to={tab.path}
            className={`flex items-center px-4 py-2 rounded-md text-sm whitespace-nowrap transition-all
              ${
                location.pathname === tab.path
                  ? 'bg-blue-600 text-white shadow-md'
                  : 'text-gray-300 hover:text-white hover:bg-gray-700'
              }
            `}
          >
            <span className="mr-2">{tab.icon}</span>
            {tab.name}
          </Link>
        ))}
      </div>
    </nav>
  );
}