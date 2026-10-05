"""Inspect notebook identities; default output never includes IDs or titles."""
import argparse
import json
import sqlite3
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
from notebook_catalog import read_catalog, resolve_notebook, resolve_book_link

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('mount')
mode = parser.add_mutually_exclusive_group()
mode.add_argument('--resolve', metavar='UUID', help='Explicitly print the selected notebook ID and current path')
mode.add_argument('--output', type=Path, help='Write a private catalog for choosing links; refuses to overwrite files')
mode.add_argument('--book', metavar='BOOK_ID', help='Resolve a stored book link; prints the notebook ID and current path without opening it')
parser.add_argument('--links', type=Path, help='BookLinks v1 JSON file; defaults to data/book-links.json with --book')
args = parser.parse_args()
try:
    if args.links and args.book is None:
        parser.error('--links requires --book')
    if args.book is not None:
        print(json.dumps(resolve_book_link(args.mount, args.links or 'data/book-links.json', args.book), indent=2))
    elif args.resolve:
        print(json.dumps(resolve_notebook(args.mount, args.resolve), indent=2))
    else:
        catalog = read_catalog(args.mount)
        if args.output:
            # This catalog includes private paths. Keep it under ignored data/.
            with args.output.open('x', encoding='utf-8') as handle:
                json.dump({'version': 1, 'notebooks': catalog}, handle, indent=2)
                handle.write('\n')
        print(json.dumps({'notebookCount': len(catalog),
                          'filesPresent': sum(r['filePresent'] for r in catalog),
                          'identityField': 'ExternalId', 'catalogWritten': bool(args.output)}, indent=2))
except (OSError, ValueError, sqlite3.Error):
    # Native exception text may contain a private notebook path or ID.
    print('Catalog inspection failed: check the mount, notebook IDs, paths, and output destination. No database changes were made.', file=sys.stderr)
    sys.exit(1)
