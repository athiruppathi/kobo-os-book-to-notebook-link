import { readFile, mkdir, writeFile, rename } from 'node:fs/promises';
import { dirname } from 'node:path';
import { randomUUID } from 'node:crypto';

function id(value) {
  if (typeof value !== 'string' || !value.trim()) throw new Error('IDs must be nonempty strings');
  return value;
}

export class BookLinks {
  constructor(file) { this.file = file; }
  async read() {
    let text;
    try { text = await readFile(this.file, 'utf8'); }
    catch (error) { if (error.code === 'ENOENT') return { version: 1, links: {} }; throw error; }
    const state = JSON.parse(text);
    if (state.version !== 1 || !state.links || typeof state.links !== 'object' || Array.isArray(state.links)) {
      throw new Error('Unsupported or invalid book-links file');
    }
    for (const [book, notebook] of Object.entries(state.links)) { id(book); id(notebook); }
    return state;
  }
  async get(book) {
    id(book);
    const { links } = await this.read();
    return Object.hasOwn(links, book) ? links[book] : null;
  }
  async save(state) {
    await mkdir(dirname(this.file), { recursive: true });
    const temp = `${this.file}.${randomUUID()}.tmp`;
    await writeFile(temp, JSON.stringify(state, null, 2) + '\n', { flag: 'wx' });
    await rename(temp, this.file);
  }
  async link(book, notebook) {
    id(book); id(notebook);
    const state = await this.read();
    Object.defineProperty(state.links, book, { value: notebook, enumerable: true, writable: true, configurable: true });
    await this.save(state);
  }
  async unlink(book) {
    id(book);
    const state = await this.read();
    delete state.links[book];
    await this.save(state);
  }
}
