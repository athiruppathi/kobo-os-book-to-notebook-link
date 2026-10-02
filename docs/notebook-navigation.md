# Native notebook navigation investigation

Target: Libra Colour, 4.46.23836, basic notebooks. Updated 2026-10-02.

## Verified findings

- Upstream NickelMenu `src/action_cc.cc`, blob `034ff7dc2db6ce78e1ff47b54ddb9c0cec580739`, implements library, store, discovery, and reading-life navigation. Its `nickel_open` handler does not implement a notebook category or a notebook ID argument.
- NickelMenu issue 117 remains open. A proposed `nickel_open:my notebooks` menu entry is not an implemented action in the source inspected.
- The published NickelDBus API documents class metadata inspection (`ndbNickelClassDetails`) and current-view inspection (`ndbCurrentView`). It does not document an open-notebook method. This is not a claim that every fork or future version lacks one.
- All seven connected notebook archives have `meta.json`. Top-level fields observed: `format-version`, `Document_Version`, `OS`, `languageLocaleIdentifier`, `iink-user-metadata`, `Application`, `Application_Version`, `lastModificationDate`.
- `iink-user-metadata.kobo` contains `createdApplicationVersion`, `modifiedApplicationVersion`, `locale`, `dpi`, `geometry`, and `device`. No notebook identifier was found in those inspected fields. IDs may still exist elsewhere; page/object IDs are not established notebook IDs.

## Repeat inspection

Run on Windows with PowerShell:

```powershell
pwsh -NoProfile -File tools/inspect-notebooks.ps1 -Mount D:/
```

This reads metadata schemas only. Output contains field names/types and anonymous sample numbers, not values or notebook filenames. It does not extract archives or modify the reader. Exit 2 means at least one archive was unsupported or unreadable; exit 1 means the mount could not be inspected. Keep any future value-bearing inventories under ignored `device-dumps/`.

## Next implementation gate

1. Obtain the exact 4.46.23836 firmware for this model from Kobo's official download service, or copy its native reader library through a separately reviewed device diagnostic. USB storage alone does not expose the root filesystem's native library.
2. Inspect exported ELF symbols for notebook controllers, workflow managers, and notebook-open functions. Record the firmware hash and full signatures; symbol names alone do not establish safe calling conventions or object lifetime.
3. Resolve the native notebook identity and launch contract before choosing the persisted mapping format. Do not substitute filenames or content hashes and call them stable notebook IDs.
4. Build a guarded extension only once the contract is understood. Test list navigation, opening one test notebook, and returning to a book independently on-device.

No navigation hook, UI automation, firmware patch, or device installation is included in this milestone. Existing book-links storage remains a host-side model accepting opaque IDs.

## Sources

- https://github.com/pgaskin/NickelMenu/blob/master/src/action_cc.cc
- https://github.com/pgaskin/NickelMenu/issues/117
- https://shermp.github.io/NickelDBus/ndb-ndbdbus.html
- https://pgaskin.net/KoboStuff/kobofirmware.html
