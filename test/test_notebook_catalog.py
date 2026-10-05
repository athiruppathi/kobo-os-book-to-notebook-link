import hashlib
import json
import subprocess
import os
from contextlib import closing
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
from notebook_catalog import MIME, read_catalog, resolve_notebook, local_notebook_path, resolve_book_link

IDENTITY = '12345678-1234-4234-8234-123456789abc'


class NotebookCatalogTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / '.kobo').mkdir()
        (self.root / 'My Notebooks').mkdir()
        self.db = self.root / '.kobo' / 'KoboReader.sqlite'
        with closing(sqlite3.connect(self.db)) as c, c:
            c.execute('CREATE TABLE content (ExternalId TEXT, ContentID TEXT, MimeType TEXT)')

    def add(self, name='Notes.nebo', identity=IDENTITY, mime=MIME):
        (self.root / 'My Notebooks' / name).touch()
        with closing(sqlite3.connect(self.db)) as c, c:
            c.execute('INSERT INTO content VALUES (?, ?, ?)',
                      (identity, 'file:///mnt/onboard/My Notebooks/' + name, mime))

    def test_resolution_is_read_only_and_uses_latest_path(self):
        self.add()
        before = hashlib.sha256(self.db.read_bytes()).digest()
        self.assertTrue(resolve_notebook(self.root, IDENTITY)['filePresent'])
        self.assertEqual(before, hashlib.sha256(self.db.read_bytes()).digest())
        # Simulate Kobo preserving ExternalId during rename; this is not an
        # assertion that firmware rename behavior has been tested on-device.
        (self.root / 'My Notebooks/Notes.nebo').rename(self.root / 'My Notebooks/Renamed.nebo')
        with closing(sqlite3.connect(self.db)) as c, c:
            c.execute('UPDATE content SET ContentID = ?', ('file:///mnt/onboard/My Notebooks/Renamed.nebo',))
        self.assertTrue(resolve_notebook(self.root, IDENTITY)['contentId'].endswith('Renamed.nebo'))

    def test_rejects_ambiguous_missing_and_invalid_ids(self):
        self.add()
        self.add('Other.nebo')
        with self.assertRaises(ValueError): read_catalog(self.root)
        with self.assertRaises(ValueError): resolve_notebook(self.root, 'bad')

    def test_missing_file_and_unrelated_content(self):
        self.add(mime='application/epub+zip')
        self.assertEqual([], read_catalog(self.root))
        with self.assertRaises(ValueError): resolve_notebook(self.root, IDENTITY)
        self.add()
        (self.root / 'My Notebooks/Notes.nebo').unlink()
        with self.assertRaises(ValueError): resolve_notebook(self.root, IDENTITY)

    def test_rejects_traversal_and_remote_paths(self):
        for path in ['file:///mnt/onboard/My Notebooks/%2e%2e/secret.nebo',
                     'file://remote/mnt/onboard/My Notebooks/a.nebo',
                     'file:///mnt/onboard/My Notebooks/a%5cb.nebo',
                     'file:///mnt/onboard/elsewhere/a.nebo']:
            with self.subTest(path=path), self.assertRaises(ValueError):
                local_notebook_path(self.root, path)

    def test_native_id_case_preserved(self):
        self.add(identity=IDENTITY.upper())
        self.assertEqual(IDENTITY.upper(), resolve_notebook(self.root, IDENTITY)['notebookId'])

    def test_node_links_resolve_and_bad_state_is_preserved(self):
        self.add()
        links = self.root / 'links.json'
        project = Path(__file__).resolve().parents[1]
        subprocess.run(['node', str(project / 'tools/book-links.mjs'), 'link', 'book-1', IDENTITY],
                       env={**os.environ, 'KOBO_LINKS_FILE': str(links)}, check=True, capture_output=True)
        self.assertEqual(IDENTITY, resolve_book_link(self.root, links, 'book-1')['notebookId'])
        with self.assertRaises(ValueError): resolve_book_link(self.root, links, 'unlinked')
        links.write_text('{broken', encoding='utf-8')
        with self.assertRaises(ValueError): resolve_book_link(self.root, links, 'book-1')
        self.assertEqual('{broken', links.read_text(encoding='utf-8'))

    def test_cli_summary_is_private_and_export_wont_overwrite(self):
        self.add('PRIVATE_TITLE.nebo')
        cli = Path(__file__).resolve().parents[1] / 'tools/notebook-catalog.py'
        command = [sys.executable, str(cli), str(self.root)]
        run = subprocess.run(command, capture_output=True, text=True, check=True)
        self.assertEqual(1, json.loads(run.stdout)['notebookCount'])
        self.assertNotIn(IDENTITY, run.stdout)
        self.assertNotIn('PRIVATE_TITLE', run.stdout)
        destination = self.root / 'catalog.json'
        destination.write_text('preserve', encoding='utf-8')
        run = subprocess.run(command + ['--output', str(destination)], capture_output=True, text=True)
        self.assertEqual(1, run.returncode)
        self.assertEqual('preserve', destination.read_text(encoding='utf-8'))

    def test_native_trial_preparation_is_private_read_only_and_exclusive(self):
        self.add('PRIVATE_TITLE.nebo')
        project = Path(__file__).resolve().parents[1]
        private = project / 'device-dumps'
        private.mkdir(exist_ok=True)
        before = hashlib.sha256(self.db.read_bytes()).digest()
        with tempfile.TemporaryDirectory(dir=private) as output_root:
            output = Path(output_root) / 'trial'
            command = [sys.executable, str(project / 'tools/prepare-link-trial.py'),
                       str(self.root), str(output)]
            run = subprocess.run(command, capture_output=True, text=True, check=True)
            self.assertFalse(json.loads(run.stdout)['navigationEnabled'])
            self.assertNotIn(IDENTITY, run.stdout)
            self.assertNotIn('PRIVATE_TITLE', run.stdout)
            catalog = json.loads((output / 'notebooks.json').read_text(encoding='utf-8'))
            self.assertEqual(IDENTITY, catalog['notebooks'][0]['notebookId'])
            self.assertEqual('PRIVATE_TITLE', catalog['notebooks'][0]['title'])
            menu = (output / 'kobo-os-links').read_text(encoding='utf-8')
            self.assertNotIn(IDENTITY, menu)
            self.assertNotIn('PRIVATE_TITLE', menu)
            self.assertNotIn('printf "open', menu)
            run = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(1, run.returncode)
            self.assertEqual(catalog, json.loads((output / 'notebooks.json').read_text(encoding='utf-8')))
        self.assertEqual(before, hashlib.sha256(self.db.read_bytes()).digest())
        self.assertEqual({'.kobo', 'My Notebooks'}, {p.name for p in self.root.iterdir()})


if __name__ == '__main__':
    unittest.main()
