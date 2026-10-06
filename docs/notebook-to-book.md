# Notebook-to-book navigation

Prepared October 6; not installed because the Kobo is disconnected:
- Source: `a3955c36dc162f1b3ef02a7641b6f5ef1fd5d682`.
- Successful ARM build: https://github.com/athiruppathi/kobo-os/actions/runs/37463446716
- Artifact ZIP SHA-256: `566197d5084285119b659ede42bd7207b89f0912d467126bd2e91594e692b0ce`.
- Library SHA-256: `7802a8460b1ebdd9986798cfb0ce1ae7809f718af301445eb6237f1816579356`.
- Package SHA-256: `52c2e355f55f5153f97afa73b5b7cd4e59484ab589f793c186b57df713126213`.
- Local private package/menu: `device-dumps/book-links-bidirectional-20261006/`.

This replaces the pending opening-only candidate; install one package, not both.
Back up current device contents, replace the project's menu file with the Open
linked notebook version, and enable navigation.enabled as explicitly requested
by the user. Preserve current book-links.json and folder catalog.

The native adapter adds **Open linked book** to IInk notebook dropdown menus
when their QMenu is shown. It adds one native tap-aware row per menu instance.
It does not alter the handwriting canvas, toolbar layout, or native menu entries.
The menu integration uses a Qt event filter, not a binary hook. The menu closes
before navigation is queued onto the GUI event loop.

## Behavior

- Read the open basic notebook's native ExternalId and reverse-search the same
  BookLinks v1 mapping used for book-to-notebook navigation. There is no second
  mapping file to get out of sync.
- If the original linked reader remains immediately below this notebook in the
  view stack, return to it. This preserves its live reading position.
- Otherwise, open the sole linked book, or present a native paginated book chooser
  for multiple links. Resolve book titles and content from the native repository.
- No association shows `No linked book`. An unavailable target is reported
  before closing the notebook.
- Use the notebook's own quit/Back handler, then either retain the revealed
  reader or invoke the normal native book opener with a default Bookmark.
- Book and notebook navigation require the existing navigation.enabled marker.

## Exact-firmware evidence

Both libnickel and the notebook plugin are hash-gated. The latter was extracted
from the same previously downloaded official firmware archive:

`libiinknote.so` SHA-256:
`2fa70e17773c645d659ff10eefc3692ffe474feb53a9b4b43d964917653c6b7c`.

- IInkNotePad::getVolume at 0x48b24 delegates to the widget getter. It returns a
  Volume **by value**, with hidden storage in r0 and this in r1. The adapter
  destroys this reference-counted copy using the native Volume destructor.
- IInkNotePad::quit at 0x47a78 invokes MainWindowController::topController/pop.
- IInkNotePadController::viewWillDisappear dispatches to the plugin view's
  virtual disappearance handler. IInkNotePad::viewWillDisappear at 0x490ac
  invokes IInkNotePadWidget::save at 0x49112 in the normal non-demo path.
- VolumeManager::getById at 0xa96fbc takes hidden return storage, ContentID,
  and database name. It constructs an eight-byte reference-counted Volume.
- Device::getDbName returns a QString reference. Content::getTitle at 0x977cbc
  returns QString by value, not a reference.
- Bookmark::Bookmark at 0x96b958 constructs an eight-byte native shared-data
  object. The adapter passes it to N3ReaderOpener::openReader and releases it
  with Bookmark's native destructor; it does not invent a reading position.

The notebook API is resolved from an already-loaded module using RTLD_NOLOAD.
Holding its loader reference prevents API pointers from becoming stale while
the bridge is alive. No notebook plugin is force-loaded at startup.

## Device verification still required

Compilation and symbol inspection do not establish successful on-device behavior.
Test the notebook menu row, no association, single and multiple associations,
and returning to the originating reader at the same position. Write a disposable
test stroke before leaving, then reopen the notebook to verify persistence.
Also test a notebook opened independently from My Notebooks and a repeated
book/notebook round trip. Native save failures are not independently detected
by the bridge; it uses the same save/close path as native Back.
