# Notebook identity resolution

## Evidence from October 2 inspection

The connected reader's `content` table contained seven records with MIME type
`application/vnd.myscript.nebo+raw`. All seven had distinct UUID-shaped
`ExternalId` values and file-URI `ContentID` values. The absence of a dedicated
notebook table does not mean notebooks are absent from the reader database.

Offline analysis of the pinned firmware's
`VolumeManager::getNotebookByObjectId(QString const&, Device const&)` showed a
call to `VolumeManager::find`. Its comparison predicate calls
`Content::getExternalId()` and QString equality. This establishes `ExternalId`
as the lookup key for that native function. It does not establish persistence
across device reset, restore, sync conflicts, or native rename operations.

Analysis locations for reproducing that finding in the hash-pinned library:
lookup function at `0xa96cc8`; predicate at `0xa92604`; calls at `0xa9260c`
and `0xa92618` resolve through PLT targets `0x6ac4b0` and `0x682580` to
`_ZNK7Content13getExternalIdEv` and `_ZeqRK7QStringS1_`. These are offline
analysis addresses, not offsets for an installed patch.

## Implemented host workflow

The Python catalog reader uses SQLite `mode=ro` and `query_only`, filters the
observed MIME type, rejects ambiguous UUIDs and unsafe paths, and resolves the
latest file URI at lookup time. It preserves the native ID's original spelling.
The Node book-links v1 file stores that UUID as the notebook value.

From the project directory, with Python 3.10+ and Node 22+:

```powershell
# Anonymous summary; no IDs or titles printed.
python tools/notebook-catalog.py D:/

# Private local inventory; make data/ first. Existing output is never overwritten.
New-Item -ItemType Directory -Force data | Out-Null
python tools/notebook-catalog.py D:/ --output data/notebooks.json

# Choose a notebook UUID from the local inventory, then link it to a book ID.
node tools/book-links.mjs link BOOK_ID NOTEBOOK_UUID

# Read-only resolution; explicitly prints the linked notebook's current path.
python tools/notebook-catalog.py D:/ --book BOOK_ID
```

Substitute actual IDs and the current mount. `data/` is ignored by Git. The
inventory includes private notebook paths and should stay local. No personal
catalog or IDs have been committed. Missing files and malformed records cause
failure rather than an attempt to launch a different notebook.

This is host-side resolution only: it does not open a notebook, read handwriting,
or modify the Kobo database. Python and Node are not assumed to run on the reader.

## Validation and remaining work

On October 5, seven Python tests passed, including integration with the actual
Node linking CLI, non-mutating database reads, a simulated rename, duplicate
IDs, unavailable files, path traversal rejection, private summary output, and
refusal to overwrite exports. Six Node tests and the metadata-inspector checks
also passed. The simulated rename tests our resolver's behavior; firmware's own
ID preservation during a rename still needs physical testing.

After reconnection on October 5, the reader mounted at D:/ and still reported
firmware 4.46.23836. The live read-only catalog contained seven notebooks and
all seven files were present. All seven resolved directly by ExternalId, and all
seven also resolved through temporary host-side book-links v1 records. These
records used synthetic book IDs and were removed after the check; they were not
user-selected book associations. SHA-256 of KoboReader.sqlite matched before
and after validation. No device files were changed and no personal IDs or paths
were included in the report.

This validates host-side identity resolution against the actual reader. Next,
validate native object construction,
return-value ABI, GUI-thread dispatch and lifetime before invoking the notebook
opening method. The constructor's exported signature alone is insufficient to
choose an allocation size or safely fabricate a Volume object.
