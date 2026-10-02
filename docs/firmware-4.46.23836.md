# Firmware 4.46.23836: offline native navigation analysis

Inspected 2026-10-02. No firmware or hooks installed on the reader.

## Reproducible inputs

Official package: https://ereaderfiles.kobo.com/firmwares/kobo13/Aug2026/kobo-update-4.46.23836.zip

- Package SHA-256: `5edee18fdf6ca815cff13b36ffa612b66b54c234761ea0aef0ed79ff184f5458`
- Nested archive: `KoboRoot.tgz`
- Inspected file: `./usr/local/Kobo/libnickel.so.1.0.0`
- Library SHA-256: `25ec19024666dca7f134335af311593a21cfbd75a0235f1b77c27d9a28f733dd`
- Format: ELF32, little-endian ARM; inspected functions use Thumb instructions.
- Parser found 68,952 defined global/weak dynamic symbols with default/protected visibility.

Hashes identify the downloaded artifacts; they are not a vendor signature verification. Firmware remains under ignored `device-dumps/`, outside Git.

## Confirmed exported symbols

| Purpose inferred from name/signature | Exact exported symbol |
| --- | --- |
| Notebook list navigation | `_ZN15LibraryNavMixin13showNotebooksE14MainNavSection` |
| Lookup by notebook object ID | `_ZN13VolumeManager21getNotebookByObjectIdERK7QStringRK6Device` |
| Native notebook opening | `_ZN14N3ReaderOpener11openNotePadERK6Volume` |
| Notebook entry dispatch | `_ZN14N3ReaderOpener11showNotePadERK6Volume` |
| Notebook controller constructor | `_ZN21IInkNotePadControllerC1ERK6Volume` |
| Basic-notebook predicate | `_ZNK6Volume15isBasicNotebookEv` |
| Main window controller | `_ZN20MainWindowController14sharedInstanceEv` |
| Native view stack push | `_ZN20MainWindowController4pushEP18AbstractControllerb` |

The names encode parameter types, but do not establish return types or whether a method is static. In particular, do not call notebook lookup through a guessed function pointer returning a fabricated Volume.

## Observed direct-call path

Offline disassembly of `N3ReaderOpener::openNotePad(Volume const&)` resolves calls through ELF PLT relocations to:

1. `Device::getCurrentDevice`, `IInkBundleManager::hasRequiredBundles`, and `N3ReaderOpener::hasRequiredMyScriptFonts`.
2. A branch to `N3ReaderOpener::showBundleDownloadDialog` when the resource-check path requires it.
3. An allocation and `IInkNotePadController` construction on the controller-opening path.
4. `MainWindowController::sharedInstance`, `closeActiveDialogs`, and `push(AbstractController*, bool)`.

`showNotePad` has calls to `Content::idBytes`, a string comparison, and separate `openNewNoteDialog` and `openNotePad` paths. It is not safe to treat every input as an existing notebook.

This confirms a native opening path, not a working external API. Object construction, ownership, notebook lookup return ABI, valid IDs, GUI-thread execution, and reader-return behavior remain unverified. Do not bypass the native resource checks just because a controller constructor is exported.

## Run analysis

Extract only on the development computer, outside the reader's `.kobo` update directory.

```sh
node tools/inspect-firmware.mjs PATH_TO_LIBNICKEL
python -m pip install -r requirements-analysis.txt
python tools/inspect-native-calls.py PATH_TO_LIBNICKEL
node --test
```

The Node tool requires no extra packages and validates ELF section/name bounds. The optional Python tool is hash-gated to this exact library. It reports resolved direct calls only, not a complete control-flow graph. Native addresses in its output are analysis locations, never installation offsets.

## Next prototype

Resolve `VolumeManager` notebook identity storage and lookup ABI, then determine a valid `N3ReaderOpener` lifecycle. Start with a diagnostic that resolves symbols and reports current view/object availability without invoking navigation. A guarded opening action comes after these contracts are verified; device testing must cover unsaved handwriting and returning to the original reading position.
