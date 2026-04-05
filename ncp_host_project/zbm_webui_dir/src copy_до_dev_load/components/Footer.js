// src/components/Footer.js
export default function Footer() {
  const ramUsage = 48;
  const freeHeap = "28 KB";
  const frag = "14%";

  return (
    <footer className="bg-black/20 backdrop-blur-md border-t border-gray-700/50 text-xs text-gray-400 px-5 py-2 flex justify-between items-center font-mono">
      <span className="flex items-center gap-1"><span>🧠</span> RAM: {ramUsage}%</span>
      <span className="flex items-center gap-1"><span>💾</span> Heap: {freeHeap}</span>
      <span className="flex items-center gap-1"><span>🗜️</span> Frag: {frag}</span>
    </footer>
  );
}