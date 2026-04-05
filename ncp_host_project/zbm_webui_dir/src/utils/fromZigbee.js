export const fromZigbeeType = (type, buffer) => {
  switch (type) {
    case 16: return buffer[0] ? 'true' : 'false';
    case 32: return buffer[0];
    case 33: return buffer[0] + buffer[1] * 256;
    case 35: return new DataView(buffer.buffer).getUint32(0, true);
    case 48: return `enum8.${buffer[0]}`;
    case 66: {
      const len = buffer[0];
      return String.fromCharCode(...buffer.slice(1, 1 + len));
    }
    default:
      return `0x${Array.from(buffer).map(b => b.toString(16).padStart(2, '0')).join('')}`;
  }
};