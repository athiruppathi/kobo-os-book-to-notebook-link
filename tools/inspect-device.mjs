import { stat, readFile, readdir } from 'node:fs/promises';
import { resolve, join } from 'node:path';

// Read-only: no database access, notebook contents, or account credentials.
const [mount, ...extra] = process.argv.slice(2);
try {
  if (!mount || extra.length) throw new Error('Usage: node tools/inspect-device.mjs KOBO_MOUNT_PATH');
  const root = resolve(mount);
  const kobo = join(root, '.kobo');
  if (!(await stat(kobo)).isDirectory()) throw new Error('Not a Kobo mount: .kobo directory is missing');
  const report = { firmware: null, databasePresent: false, notebookDirectories: [], notebookFileCount: 0, notebookArchiveCount: 0, exportedNotebookFileCount: 0, nickelMenuPresent: false };
  async function exists(path) {
    try { await stat(path); return true; }
    catch (error) { if (error.code === 'ENOENT') return false; throw error; }
  }
  const versionPath = join(kobo, 'version');
  if (await exists(versionPath)) {
    // The complete version file may contain a device serial. Emit only firmware.
    const value = await readFile(versionPath, 'utf8');
    report.firmware = value.match(/\b\d+\.\d+\.\d{5}\b/)?.[0] ?? null;
  }
  report.databasePresent = await exists(join(kobo, 'KoboReader.sqlite'));
  report.nickelMenuPresent = await exists(join(root, '.adds', 'nm'));
  for (const entry of await readdir(kobo, { withFileTypes: true })) {
    if (entry.isDirectory() && /notebook|nebo|markup|stylus/i.test(entry.name)) {
      report.notebookDirectories.push({ directory: `.kobo/${entry.name}`, entryCount: (await readdir(join(kobo, entry.name))).length });
    }
  }
  async function inventory(path, depth = 0) {
    if (depth > 10) throw new Error('Notebook directory nesting exceeds inspection limit');
    let notebooks = 0, archives = 0, files = 0;
    for (const entry of await readdir(path, { withFileTypes: true })) {
      const child = join(path, entry.name);
      if (entry.isDirectory()) {
        const nested = await inventory(child, depth + 1);
        notebooks += nested.notebooks; archives += nested.archives; files += nested.files;
      } else if (entry.isFile()) {
        files++;
        if (entry.name.toLowerCase().endsWith('.nebo')) {
          notebooks++;
          // Inspect the ZIP signature only, never handwriting or notebook metadata.
          const { open } = await import('node:fs/promises');
          const handle = await open(child, 'r');
          try {
            const header = Buffer.alloc(4);
            const { bytesRead } = await handle.read(header, 0, 4, 0);
            if (bytesRead === 4 && header.equals(Buffer.from([0x50, 0x4b, 0x03, 0x04]))) archives++;
          } finally { await handle.close(); }
        }
      }
    }
    return { notebooks, archives, files };
  }
  const notebooksPath = join(root, 'My Notebooks');
  if (await exists(notebooksPath)) {
    const count = await inventory(notebooksPath);
    report.notebookFileCount = count.notebooks;
    report.notebookArchiveCount = count.archives;
  }
  const exportsPath = join(root, 'Exported Notebooks');
  if (await exists(exportsPath)) report.exportedNotebookFileCount = (await inventory(exportsPath)).files;
  console.log(JSON.stringify(report, null, 2));
} catch (error) { console.error(error.message); process.exitCode = 1; }
