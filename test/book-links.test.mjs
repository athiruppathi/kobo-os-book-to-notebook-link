import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, rm, writeFile, readFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { BookLinks } from '../src/book-links.mjs';

async function fixture(t) {
  const dir = await mkdtemp(join(tmpdir(), 'kobo-links-'));
  t.after(() => rm(dir, { recursive: true, force: true }));
  const file = join(dir, 'links.json');
  return { file, store: new BookLinks(file) };
}
test('links persist, update, and unlink independently', async t => {
  const { file, store } = await fixture(t);
  assert.equal(await store.get('a'), null);
  await store.link('a', 'notes-a'); await store.link('b', 'notes-b');
  assert.equal(await new BookLinks(file).get('a'), 'notes-a');
  await store.link('a', 'replacement'); await store.unlink('a');
  assert.equal(await store.get('a'), null);
  assert.equal(await store.get('b'), 'notes-b');
});
test('malformed state is preserved rather than overwritten', async t => {
  const { file, store } = await fixture(t);
  await writeFile(file, '{broken');
  await assert.rejects(store.link('a', 'b'));
  assert.equal(await readFile(file, 'utf8'), '{broken');
});
test('rejects empty IDs and handles object-property names as IDs', async t => {
  const { store } = await fixture(t);
  await assert.rejects(store.link(' ', 'b'));
  assert.equal(await store.get('constructor'), null);
  await store.link('__proto__', 'notebook');
  assert.equal(await store.get('__proto__'), 'notebook');
});
