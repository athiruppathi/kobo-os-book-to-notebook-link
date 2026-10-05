"""Read-only notebook identity resolution for the observed Kobo schema."""
import sqlite3
import json
from pathlib import Path
from urllib.parse import unquote, urlsplit
from uuid import UUID

MIME = 'application/vnd.myscript.nebo+raw'


def notebook_id(value):
    if not isinstance(value, str):
        raise ValueError('Notebook ID must be a UUID string')
    try:
        parsed = UUID(value)
    except (ValueError, AttributeError):
        raise ValueError('Notebook ID must be a UUID string') from None
    if str(parsed) != value.lower():
        raise ValueError('Notebook ID must use the hyphenated UUID form')
    return str(parsed)


def local_notebook_path(mount, content_id):
    uri = urlsplit(content_id)
    if uri.scheme != 'file' or uri.netloc or uri.query or uri.fragment:
        raise ValueError('Notebook content ID is not a local file URI')
    path = unquote(uri.path)
    prefix = '/mnt/onboard/My Notebooks/'
    if not path.startswith(prefix):
        raise ValueError('Notebook path is outside My Notebooks')
    parts = path[len(prefix):].split('/')
    if any(p in ('', '.', '..') or any(c in p for c in ('\\', ':', '\0')) for p in parts):
        raise ValueError('Unsafe notebook path')
    if not parts[-1].lower().endswith('.nebo'):
        raise ValueError('Unsupported notebook extension')
    root = (Path(mount) / 'My Notebooks').resolve()
    result = root.joinpath(*parts).resolve()
    if not result.is_relative_to(root):
        raise ValueError('Notebook path escapes My Notebooks')
    return result


def read_catalog(mount):
    database = Path(mount).resolve() / '.kobo' / 'KoboReader.sqlite'
    # mode=ro prevents SQLite from creating or modifying the database.
    connection = sqlite3.connect(database.as_uri() + '?mode=ro', uri=True)
    try:
        connection.execute('PRAGMA query_only=ON')
        rows = connection.execute(
            'SELECT ExternalId, ContentID FROM content WHERE MimeType = ?', (MIME,)
        ).fetchall()
    finally:
        connection.close()
    result = []
    seen = set()
    for external_id, content_id in rows:
        identity = notebook_id(external_id)
        if identity in seen:
            raise ValueError('Duplicate notebook object ID; refusing ambiguous resolution')
        seen.add(identity)
        if not isinstance(content_id, str):
            raise ValueError('Notebook has no valid content ID')
        local_path = local_notebook_path(mount, content_id)
        result.append({'notebookId': external_id, 'contentId': content_id,
                       'localPath': str(local_path), 'filePresent': local_path.is_file()})
    return result


def resolve_notebook(mount, identity):
    wanted = notebook_id(identity)
    matches = [r for r in read_catalog(mount) if notebook_id(r['notebookId']) == wanted]
    if not matches:
        raise ValueError('Linked notebook is not in the current catalog')
    if not matches[0]['filePresent']:
        raise ValueError('Linked notebook file is unavailable')
    return matches[0]


def resolve_book_link(mount, links_file, book_id):
    """Resolve the existing Node BookLinks v1 format without changing it."""
    with Path(links_file).open(encoding='utf-8') as handle:
        state = json.load(handle)
    if (not isinstance(state, dict) or type(state.get('version')) is not int
            or state['version'] != 1 or not isinstance(state.get('links'), dict)):
        raise ValueError('Unsupported book-links file')
    if any(not isinstance(k, str) or not k.strip() or not isinstance(v, str)
           or not v.strip() for k, v in state['links'].items()):
        raise ValueError('Invalid book-links file')
    if not isinstance(book_id, str) or not book_id.strip() or book_id not in state['links']:
        raise ValueError('No notebook linked to this book')
    return resolve_notebook(mount, state['links'][book_id])
