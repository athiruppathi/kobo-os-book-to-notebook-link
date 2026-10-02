// Read ELF32 little-endian ARM dynamic symbols without loading executable code.
export function exportedSymbols(data) {
  function range(offset, size) {
    if (!Number.isSafeInteger(offset) || !Number.isSafeInteger(size) || offset < 0 || size < 0 || offset + size > data.length) throw new Error('Truncated ELF data');
  }
  range(0, 52);
  if (!data.subarray(0, 4).equals(Buffer.from([127, 69, 76, 70])) || data[4] !== 1 || data[5] !== 1 || data.readUInt16LE(18) !== 40) throw new Error('Expected ELF32 little-endian ARM');
  const start = data.readUInt32LE(32), stride = data.readUInt16LE(46), count = data.readUInt16LE(48);
  if (stride < 40 || !count) throw new Error('Unsupported ELF section table');
  range(start, stride * count);
  const sections = Array.from({ length: count }, (_, i) => {
    const at = start + stride * i;
    return { type: data.readUInt32LE(at + 4), offset: data.readUInt32LE(at + 16), size: data.readUInt32LE(at + 20), link: data.readUInt32LE(at + 24), entry: data.readUInt32LE(at + 36) };
  });
  const result = [];
  for (const section of sections.filter(s => s.type === 11)) {
    range(section.offset, section.size);
    if (section.entry !== 16 || section.size % 16) throw new Error('Unsupported dynamic symbol entries');
    const strings = sections[section.link];
    if (!strings || strings.type !== 3) throw new Error('Invalid symbol string table');
    range(strings.offset, strings.size);
    for (let at = section.offset; at < section.offset + section.size; at += 16) {
      const nameOffset = data.readUInt32LE(at), info = data[at + 12], visibility = data[at + 13] & 3, index = data.readUInt16LE(at + 14);
      if (!index || ![1, 2].includes(info >> 4) || ![0, 3].includes(visibility)) continue;
      if (nameOffset >= strings.size) throw new Error('Invalid symbol name offset');
      const end = data.indexOf(0, strings.offset + nameOffset);
      if (end < 0 || end >= strings.offset + strings.size) throw new Error('Unterminated symbol name');
      result.push({ name: data.toString('utf8', strings.offset + nameOffset, end), address: data.readUInt32LE(at + 4), size: data.readUInt32LE(at + 8), type: info & 15 });
    }
  }
  return result;
}
