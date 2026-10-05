# Native book links: first lookup trial

Built source: `a9893611e423a4df3c934be1c4621b6893318f1b`.
Successful CI: https://github.com/athiruppathi/kobo-os/actions/runs/37338588152
Artifact ZIP SHA-256: `c29d15d2c39efa2fba7fff447afc5260082120082ab0c38ea33f378fe069eed3`.
Library SHA-256: `8d71160af568239cc45420670d4b2a534e619064d970ac2c2bb00695172acf4c`.
Prepared package SHA-256: `1369f9af24a4bffc82c860263b1aa17bbe9cf9ef0225b828c58490d4d9920713`.

Chooser fix staged October 5 after the initial dropdown failed on-device:
- Source: `da554faa06e04b7dd7776305219b6eeb6db8f571`.
- Successful CI: https://github.com/athiruppathi/kobo-os/actions/runs/37340580521
- Artifact ZIP SHA-256: `f169c8744a73540c992a3b73eb7d4cb7979fad6e92f87342dd646d472efdf077`.
- Library SHA-256: `106a3922587338da1b1e6a31b41520489d1fc11b67f0228c585fdccc959846df`.
- Package SHA-256: `0774430d4898a20d92e31fbd8c2f247b659089fecd0180d8bb117e1397b0a64d`.
- Current USB-visible backup verified: 1,833 files. Opening remains disabled.

This plugin is separate from the symbol-only diagnostic. It calls native reader
and notebook lookup functions on the GUI thread, and offers a Qt notebook chooser.
It does not require Node or Python on the Kobo. The earlier JavaScript navigation
module remains a host-side policy reference, not a device runtime dependency.

## Implemented device behavior

- Exact libnickel SHA-256 gate before resolving the adapter API.
- Reader class check, native current-book ContentID, native notebook ExternalId
  lookup, basic-notebook/validity checks, and native Volume destruction.
- A notebook chooser populated from a private USB-prepared catalog.
- BookLinks v1 mapping saved atomically using QSaveFile without direct-write fallback.
- Fixed FIFO commands dispatched by QSocketNotifier on the GUI thread, with
  bounded input and reentrancy protection. No polling timer or native hooks.
- A separately gated native open path using ReadingViewMixin::createReader and
  N3ReaderOpener::openNotePad, preserving native bundle/font checks. The opener is
  parented to the original reader and tracked by QPointer.

The first trial installs **Link notebook** and **Check linked notebook** in the
reading menu. It intentionally does not install an Open menu action or create
`navigation.enabled`. Successful lookup is required before an opening trial.

The initial Qt dropdown was reported unusable on-device. The revised chooser
uses a single modal window with large, scrollable list rows and explicit Link
and Cancel buttons. Link is disabled until a row is selected; no typing is
required. Touch behavior still requires device verification.
Opening code is experimental and allows only one opener per surviving reader;
resource-download cancellation/completion tracking is not implemented yet.
No custom return action is available. A later opening trial must verify native
Back behavior, handwriting persistence, and original reading position before
normal use. Do not force-pop views or invoke viewWillDisappear manually.

## Firmware evidence

Pinned library hash is recorded in firmware-4.46.23836.md. Further observations:

| Function | Observed ABI/lifetime evidence |
| --- | --- |
| getNotebookByObjectId, 0xa96cc8 | r0 is return storage, r1 QString reference, r2 Device reference; forwards storage to find. |
| VolumeManager::find, 0xa96a28 | Writes vtable/data words at 0xa96b42 and 0xa96ba0; increments shared data refcount; constructs a default Volume for no match. |
| Volume::~Volume, 0xa8e720 | Releases shared data; calls Content destructor. |
| ReadingView::getVolume, 0xf97728 | Returns address of the member at this+0x74; borrowed reference. |
| Content::idBytes, 0x977c9c | Returns address of a QByteArray member; borrowed reference. |
| Content::getExternalId, 0x97a6dc | Returns address of a QString member; borrowed reference. |
| ReadingViewMixin::createReader, 0xfd1e20 | Does not dereference its mixin argument; allocates 0x20 bytes and invokes native opener constructor. Null library controller gives null QObject parent. |
| N3ReaderOpener constructor, 0xfd1ce4 | Copies Volume and QVector shared data. The wrapper supplies only a default empty Qt vector (common shared-null header), never fabricated ContentKey elements. |
| IInkNotePadController::viewWillDisappear, 0xdc3e04 | Delegates to a plugin virtual function. This does not yet establish save completion or a safe custom return implementation. |

These are exact-build observations, not an ABI promise for other firmware.
Runtime symbol availability was verified by the prior diagnostic; the new
native calls still need the first on-device trial.

## Prepare and install

1. Build through the native workflow. Verify artifact digest and ELF ARM type.
2. Run `python tools/prepare-link-trial.py D:/ device-dumps/NEW_PRIVATE_DIRECTORY`.
   It creates only local chooser data and fixed menu entries, never device files.
3. Package with `python tools/package-probe.py LIBRARY OUTPUT --component links`.
   The package contains only `/usr/local/Kobo/imageformats/libkobobooklinks.so`.
4. Back up and verify current USB-visible data before staging. Create
   `.adds/kobo-os/`, copy `notebooks.json` there, and create its empty `enabled`
   marker. Copy the prepared menu file to `.adds/nm/kobo-os-links` without
   overwriting an existing file. Stage the verified `.kobo/KoboRoot.tgz` only if
   no pending update exists. Do not create `navigation.enabled`.
5. Safely eject, unplug, allow the update to finish, then open a test book.
6. In the reading NickelMenu choose Link notebook, select a basic notebook,
   then choose Check linked notebook. Success produces a confirmation dialog.
7. Reconnect USB. `.adds/kobo-os/status.json` should report
   `reader-and-notebook-validated`. Report files omit book/notebook identities.
   The private mapping file contains the identities needed for operation.

Disabling `.adds/kobo-os/enabled` prevents commands on the next invocation and
prevents initialization on next restart. To uninstall, create
`.adds/kobo-os-links.uninstall` and restart; remove only this plugin's menu file.
Preserve book-links.json if links should survive reinstallation.

Loading a native plugin can crash Nickel even when notebook opening is disabled.
The USB-visible backup is not a full root-filesystem recovery image.
