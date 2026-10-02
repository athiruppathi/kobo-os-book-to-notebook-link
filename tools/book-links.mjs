import { BookLinks } from '../src/book-links.mjs';
const [command, book, notebook, ...extra] = process.argv.slice(2);
const store = new BookLinks(process.env.KOBO_LINKS_FILE || 'data/book-links.json');
try {
  if (extra.length || !book || !['get', 'link', 'unlink'].includes(command) || (command === 'link' ? !notebook : notebook !== undefined)) {
    throw new Error('Usage: node tools/book-links.mjs link BOOK NOTEBOOK | get BOOK | unlink BOOK');
  }
  if (command === 'get') console.log((await store.get(book)) ?? 'No linked notebook');
  if (command === 'link') { await store.link(book, notebook); console.log('Notebook linked'); }
  if (command === 'unlink') { await store.unlink(book); console.log('Notebook unlinked'); }
} catch (error) { console.error(error.message); process.exitCode = 1; }
