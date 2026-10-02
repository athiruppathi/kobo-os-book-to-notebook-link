import { readFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { exportedSymbols } from '../src/elf-symbols.mjs';

try {
  const [file, ...extra] = process.argv.slice(2);
  if (!file || extra.length) throw new Error('Usage: node tools/inspect-firmware.mjs PATH_TO_LIBNICKEL');
  const data = await readFile(file);
  const all = exportedSymbols(data);
  const candidates = all.filter(s => /notebook|notepad|nebo|iink|stylus|swipe|N3ReaderOpener/i.test(s.name));
  console.log(JSON.stringify({ sha256: createHash('sha256').update(data).digest('hex'), exportedSymbolCount: all.length, candidates }, null, 2));
} catch (error) { console.error(error.message); process.exitCode = 1; }
