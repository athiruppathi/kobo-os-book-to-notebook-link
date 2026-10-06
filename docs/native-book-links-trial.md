# Native book links: first lookup trial

## Opening milestone (October 6)

The opening-only candidate below is superseded by the
[bidirectional build](notebook-to-book.md), which also adds Open linked book
to native notebook menus. Deploy the newer package for both requested actions.

Prepared build (not yet installed):
- Source: `baa1f930b1ee558920d104cc0a228dce8af41755`.
- Successful CI: https://github.com/athiruppathi/kobo-os/actions/runs/37462000797
- ZIP SHA-256: `78eb7004be32cdc3aee4c89bb0e72ba648901f9b7bdf4d98c8a8ec9269b6231a`.
- Library SHA-256: `bfc1ba05f40a02979d12af2615fd0d9657a9d0b0cf2c56eb2f1beddfa4a00470`.
- Package SHA-256: `f8200e0f432891c0150c47990f8c0b231adcdb3cab5233fce7b39e81e0314c3d`.
- Private local package/menu: `device-dumps/book-links-open-20261006/`.

The user confirmed the native chooser worked and Check linked notebook reported
that the current book and linked basic notebook validated. They explicitly
requested replacing Check with Open linked notebook and enabling navigation.

The new reading-menu action sends `open`. Missing mappings show the native
popup body exactly `No linked notebook`. Linked books use the native reader
factory and notebook opener, including Nickel's bundle/font checks. After a
confirmed notebook view is destroyed by native navigation, its opener is
released with deleteLater so later opens can create a fresh opener. Resource
download transitions remain conservative: an unconfirmed opener is retained.

Deployment requires the updated plugin, replacement of only this project's
menu file, and creation of `.adds/kobo-os/navigation.enabled`. Preserve the
existing book-links.json; do not restore mappings from a pre-link backup.
The user has authorized this opening behavior. Native notebook opening, Back,
handwriting persistence, and repeat opening still require device verification.

Test a linked book with Open linked notebook, then native Back, then Open again.
Also test an unlinked book for the exact missing-link message. Do not modify or
force-pop the native notebook controller to simulate a successful return.

## Current candidate: native touch controls and folder browser

The plain Qt dropdown and list trials both displayed but did not respond to
touch. The current candidate uses Nickel's ConfirmationDialog factory and
MenuTextItem rows registered for native TapGesture delivery. Confirmation and
error messages also use native dialogs. Device touch behavior remains unverified.

Folder navigation preserves relative paths and empty directories, shows only
the current folder's contents, and includes Up and pagination. Only notebook
leaves can be linked. The catalog is a USB-prepared snapshot; refresh it when
notebooks or folders change. Nested folders and duplicate notebook titles in
different folders are covered by host tests.

- Source: `d175269c721da53485af9dab8a238d68aab1148f`.
- Successful ARM build: https://github.com/athiruppathi/kobo-os/actions/runs/37342294243
- Artifact ZIP SHA-256: `bb083e91417e9694616a6716b65ab50d99b44f2113b2ae2699fe0be875b1c964`.
- Library SHA-256: `96a2c28bb2fb7a8d5bc5577099c7990aab66d3e0c9f45ed7b5643032257f1f94`.
- Package SHA-256: `1a70fc7588cb7a48e1b048ff36e1109979ebf2418fd764c153e79caabb6266ff`.
- Prepared locally October 6; not staged because the Kobo was disconnected.

Before staging, refresh chooser data from the connected device, back up the
existing catalog, and replace only notebooks.json plus the plugin package.
Preserve book-links.json, menu entries, and the disabled navigation state.

Native UI ABI evidence in the pinned firmware: getConfirmationDialog at
0x11179c0 allocates and constructs a QDialog-derived native object; no guessed
dialog layout is used. Native createMenuTextItem at 0xee34e0 allocates 0x60 bytes,
which the wrapper uses with the native MenuTextItem constructor. Its registration
method at 0xf02bfc grabs the native gesture and sets its delegate. QObject signal
connections dispatch row taps; no global touch/mouse synthesis setting is changed.

## Earlier trial builds

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
