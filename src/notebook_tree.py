"""Private chooser tree; directories never carry notebook identities."""
from pathlib import Path


def chooser_catalog(mount, rows):
    root = (Path(mount) / 'My Notebooks').resolve()
    notebooks = []
    for row in rows:
        local = Path(row['localPath']).resolve()
        relative = local.relative_to(root)
        notebooks.append({'notebookId': row['notebookId'], 'title': relative.stem,
                          'relativePath': relative.as_posix()})
    folders = []
    for directory in root.rglob('*'):
        if directory.is_dir():
            if directory.is_symlink() or not directory.resolve().is_relative_to(root):
                raise ValueError('Folder escapes notebook root')
            folders.append(directory.relative_to(root).as_posix())
    return {'version': 1, 'notebooks': notebooks, 'folders': sorted(folders)}
