import { test } from 'node:test';
import assert from 'node:assert/strict';
import { exportedSymbols } from '../src/elf-symbols.mjs';

function fixture() {
  const b = Buffer.alloc(256);
  b.set([127, 69, 76, 70, 1, 1]); b.writeUInt16LE(40, 18);
  b.writeUInt32LE(52, 32); b.writeUInt16LE(40, 46); b.writeUInt16LE(3, 48);
  b.writeUInt32LE(3, 96); b.writeUInt32LE(172, 108); b.writeUInt32LE(16, 112);
  b.write('\0openNotebook\0', 172);
  b.writeUInt32LE(11, 136); b.writeUInt32LE(192, 148); b.writeUInt32LE(48, 152);
  b.writeUInt32LE(1, 156); b.writeUInt32LE(16, 168);
  for (const at of [208, 224]) {
    b.writeUInt32LE(1, at); b.writeUInt32LE(0x1235, at + 4); b.writeUInt32LE(20, at + 8); b[at + 12] = 0x12;
  }
  b.writeUInt16LE(1, 238); // Only the second named symbol is defined.
  return b;
}
test('extracts defined exports and excludes imports', () => {
  assert.deepEqual(exportedSymbols(fixture()), [{ name: 'openNotebook', address: 0x1235, size: 20, type: 2 }]);
});
test('rejects wrong architecture and truncated tables', () => {
  const wrong = fixture(); wrong.writeUInt16LE(62, 18);
  assert.throws(() => exportedSymbols(wrong), /Expected ELF32/);
  assert.throws(() => exportedSymbols(fixture().subarray(0, 200)), /Truncated/);
});
test('rejects invalid name offsets and unterminated strings', () => {
  const bad = fixture(); bad.writeUInt32LE(1000, 224);
  assert.throws(() => exportedSymbols(bad), /name offset/);
  const unterminated = fixture(); unterminated.fill(65, 173, 188);
  assert.throws(() => exportedSymbols(unterminated), /Unterminated/);
});
