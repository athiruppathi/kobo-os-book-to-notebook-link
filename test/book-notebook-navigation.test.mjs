import test from 'node:test';
import assert from 'node:assert/strict';
import { BookNotebookNavigation } from '../src/book-notebook-navigation.mjs';

function fixture() {
  const calls = [];
  const context = {};
  const native = {
    async captureReader() { calls.push('capture'); return { bookId: 'book', context }; },
    async resolveNotebook(id) { calls.push(['resolve', id]); return { kind: 'basic' }; },
    async openNotebook() { calls.push('open'); },
    async prepareNotebookExit() { calls.push('save'); return true; },
    async restoreReader(value) { assert.equal(value, context); calls.push('restore'); },
    async releaseReader(value) { assert.equal(value, context); calls.push('release'); },
  };
  const links = { async get() { return 'notebook'; } };
  return { native, links, calls, navigation: new BookNotebookNavigation({ links, native }) };
}

test('opens the link and saves before restoring the original reader context', async () => {
  const f = fixture();
  await f.navigation.openLinkedNotebook();
  assert.equal(await f.navigation.returnToBook(), true);
  assert.deepEqual(f.calls, ['capture', ['resolve', 'notebook'], 'open', 'save', 'restore', 'release']);
  await assert.rejects(f.navigation.returnToBook(), /No linked book/);
});

test('missing links and non-basic notebooks do not navigate and release the context', async () => {
  for (const reason of ['missing', 'advanced']) {
    const f = fixture();
    if (reason === 'missing') f.links.get = async () => null;
    else f.native.resolveNotebook = async () => ({ kind: 'advanced' });
    await assert.rejects(f.navigation.openLinkedNotebook(), /No notebook|unavailable/);
    assert.equal(f.calls.includes('open'), false);
    assert.equal(f.calls.at(-1), 'release');
  }
});

test('failed open releases the handle and allows retry', async () => {
  const f = fixture();
  f.native.openNotebook = async () => { throw new Error('resources unavailable'); };
  await assert.rejects(f.navigation.openLinkedNotebook(), /resources/);
  assert.equal(f.calls.at(-1), 'release');
  f.native.openNotebook = async () => {};
  await f.navigation.openLinkedNotebook();
});

test('cancelled save and failed restore preserve the return context for retry', async () => {
  const f = fixture();
  await f.navigation.openLinkedNotebook();
  f.native.prepareNotebookExit = async () => false;
  assert.equal(await f.navigation.returnToBook(), false);
  assert.equal(f.calls.includes('release'), false);
  f.native.prepareNotebookExit = async () => true;
  f.native.restoreReader = async () => { throw new Error('restore failed'); };
  await assert.rejects(f.navigation.returnToBook(), /restore failed/);
  assert.equal(f.calls.includes('release'), false);
  f.native.restoreReader = async () => {};
  assert.equal(await f.navigation.returnToBook(), true);
});

test('double activation cannot replace the original book session', async () => {
  const f = fixture();
  let complete;
  f.native.openNotebook = () => new Promise(resolve => { complete = resolve; });
  const first = f.navigation.openLinkedNotebook();
  await assert.rejects(f.navigation.openLinkedNotebook(), /in progress/);
  // Wait for the adapter to be entered without scheduling another navigation.
  while (!complete) await Promise.resolve();
  complete();
  await first;
  await assert.rejects(f.navigation.openLinkedNotebook(), /Return to the book/);
  assert.equal(f.calls.filter(value => value === 'capture').length, 1);
});
