"""Prepare private notebook chooser data and menu entries locally; never install."""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
from notebook_catalog import read_catalog

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('mount', type=Path)
parser.add_argument('output', type=Path, help='New private directory under ignored device-dumps/')
args = parser.parse_args()

try:
    mount = args.mount.resolve()
    output = args.output.resolve()
    private_root = Path(__file__).resolve().parents[1] / 'device-dumps'
    if not output.is_relative_to(private_root) or output == private_root:
        raise ValueError('Output must be inside the private device-dumps directory')
    if output.is_relative_to(mount):
        raise ValueError('Preparation must not write onto the device')
    rows = read_catalog(mount)
    if not rows or any(not row['filePresent'] for row in rows):
        raise ValueError('Notebook catalog is empty or contains missing files')
    catalog = {'version': 1, 'notebooks': [
        {'notebookId': row['notebookId'], 'title': Path(row['localPath']).stem}
        for row in rows
    ]}
    output.mkdir(parents=True, exist_ok=False)
    (output / 'notebooks.json').write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    # Fixed strings only. No book/notebook identity ever enters a shell command.
    entries = []
    for title, command in [('Link notebook', 'link'), ('Check linked notebook', 'check')]:
        entries.append(f'menu_item : reader : {title} : cmd_spawn : quiet : test -p /tmp/kobo-os-book-links.fifo && printf "{command}\\n" > /tmp/kobo-os-book-links.fifo')
    (output / 'kobo-os-links').write_text('\n'.join(entries) + '\n', encoding='utf-8')
    print(json.dumps({'notebooksPrepared': len(rows), 'navigationEnabled': False}))
except (OSError, ValueError):
    print('Preparation failed. Check the mount and use a new output directory under device-dumps.', file=sys.stderr)
    sys.exit(1)
