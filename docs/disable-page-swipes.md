# Disable swipe page turns, preserve vertical shortcuts

Revision for Kobo Libra Colour firmware 4.46.23836, based on the working notebook catalog fix (abf262e1f4a518f345c4fe35110c1c8659cdb4c0).

The previous whole-gesture filter is removed. Kobo receives the original gesture events, including mixed events, without interception or changes to gesture fields.

Six specific Thumb BLX instructions in loaded process memory are replaced by two 16-bit NOPs each:
- ReadingView::processSwipe: nextPageWithTimer and prevPageWithTimer.
- SandboxReadingView::swipeGesture: pageLeft and pageRight.
- IInkNotePadWidget::swipeGesture: horizontalSwipe and verticalSwipe.

Other callers of the page methods remain unchanged, including buttons/taps. The notebook's quick-access and touch-cancellation paths still run. Both notebook rotation paths are covered. Book quick-access and footer menu calls remain intact. This is an in-memory code patch, not a Qt event filter or an on-disk firmware patch.

The module is acquired with RTLD_NOLOAD and retained; notebook code is not loaded at startup. Before the first gesture is delivered, validate the exact library SHA-256 and every original instruction. Obtain writable access to every affected page before changing any bytes, flush the instruction cache, and restore read/execute protection. Unknown firmware or unexpected bytes disable this feature and log the failure. Runtime patching still needs device testing.

To restore native swipe page turns, create /mnt/onboard/.adds/kobo-os/allow-page-swipes (D:\.adds\kobo-os\allow-page-swipes from Windows). Original calls are restored on the next gesture; a restart also resets process memory. Removing the marker disables swipe page turns again. The existing enabled marker is also respected.

Validation:
- Offline verified all six sites are 4-byte BLX instructions inside the expected swipe handlers and resolve to the specified PLT symbols in the pinned libraries.
- docs/swipe-call-sites.json records exact original bytes and symbols.
- Added host tests for patch isolation, idempotence, restoration, cross-page instructions, and all-or-nothing rejection on an unexpected instruction.
- Host tests and ARM compilation still require CI; they have NOT run locally. No new installable package exists yet.
- Device checks after build: EPUB/KEPUB page buttons and taps; horizontal swipe does not turn pages; vertical footer/quick-access gestures; basic notebooks in both rotations; handwriting, menus, notebook page buttons; allow-page-swipes restoration.
