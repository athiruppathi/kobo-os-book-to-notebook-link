# Live notebook browsing and last-page opening

October 20, 2026 candidate; device verification pending.

- Source: `03721907f48ead89d009ff2ae5e89a919ed311a8`.
- ARM build and native catalog tests passed:
  https://github.com/athiruppathi/kobo-os/actions/runs/39474158237
- Artifact ZIP SHA-256: `e990056eb83126c911b58dd9452522ec3cfbac67b3eb82034ffc32fb78f92c7f`.
- Library SHA-256: `6670da36108a70bd6649680cdc42ba92a84ff71fec8ed235049c0d9a92fb923b`.
- Package SHA-256: `783fc118d0a8e073dd6442e0dbe02a8d55284d0219e0db503f9c07859f26dff7`.
- Prepared package: `device-dumps/book-links-live-20261020/KoboRoot.tgz`.

For a device already running the prior opening trial, copy only this package
to `.kobo/KoboRoot.tgz` (do not extract), then safely eject and disconnect.
Do not replace an unrelated pending update. Existing enabled/navigation.enabled,
menu, and saved book links remain in place. The package contains only the plugin.

The old chooser read a USB-prepared snapshot. It could miss subsequent creations
and moves. It also stacked four entries plus up to three navigation rows inside
a confirmation dialog. The new chooser reads the current database using its own
read-only Qt SQLite connection, scans current folders (including empty nested
folders), and shows three entries with a separate horizontal Up/Previous/Next row.
The heading includes the current page number. Long names are elided; the final
link confirmation retains the full relative notebook path.

The mapping format and saved book links are unchanged. Deleted database rows
are skipped. Duplicate identities, traversal, and escaped symlinks are rejected.
The chooser no longer depends on notebooks.json or USB catalog refreshes.
Only basic notebooks in Nickel's current catalog are selectable.

After a confirmed native book-to-notebook transition, a bounded GUI timer waits
for the notebook's page range to settle, then calls the native scrollTo method
with scrollMaximum. This selects the last existing page; it does not create one.
The timer checks the current notebook identity and stops on navigation, disabled
markers, or user touch/key/pen input. A zero maximum is given longer to initialize.
Unconfirmed asynchronous opener transitions retain the existing status handling;
last-page navigation is only scheduled for a confirmed notebook view.

Exact-firmware evidence from libiinknote.so, SHA-256
2fa70e17773c645d659ff10eefc3692ffe474feb53a9b4b43d964917653c6b7c:

- scrollMaximum at 0x56a6c uses the basic-notebook branch at 0x56b3c to
  return the native package part count minus one.
- setScrollValue at 0x62c24 selects the requested package part on that branch.
- scrollTo at 0x6320c delegates directly to setScrollValue.
- Native setupScrubberMenu calls scrollValue and scrollMaximum to populate
  the notebook page controls, confirming the shared index convention.
- setupVolume restores a saved offset and queues native follow-up work; the
  new jump waits for initialization instead of changing notebook archive data.

CI builds the ARM plugin and separately runs the live catalog against a temporary
SQLite database: nested/empty folders, same titles, deleted entries, additions
between scans, read-only enforcement, symlink rejection, and connection cleanup.

On-device checks still needed: every Next/Previous/Up control is visible and
tappable; recent additions appear; linked notebooks with one and many pages open
at their last existing page; handwriting and returning to the book still work.
Direct USB access from this Windows session is currently denied, so deployment
must await restored device access or a manual package copy.

Firmware compatibility check: Nickel uses the QSQLITE driver; the firmware's
Qt5Sql aliases resolve to the bundled QtSql library, and its bundled SQLite
driver contains the QSQLITE_OPEN_READONLY option.
