# Development plan

## Device discovery

Confirmed user target: Libra Colour, firmware 4.46.23836, basic notebooks only. Prioritize native basic-notebook navigation and export. Advanced-notebook handwriting recognition and extensions are outside the initial scope.

Record firmware version from Settings -> Device information. Determine whether notebooks used are basic, advanced, or both. Connect via USB and back up user-visible contents before installation. Keep device databases, notebook data, credentials, and exports out of Git.

Inspect a copy of device metadata read-only. Identify stable book and notebook IDs and the native notebook-opening interface for the installed firmware. Do not directly edit the live Kobo database.

## First device milestone

1. Install a compatible NickelMenu build after verifying firmware support.
2. Prototype opening a chosen existing notebook through a native navigation hook.
3. Capture the active book and rely on native saved reading position when returning.
4. Connect the proven actions to the book-links storage model, ported to the device runtime.

If no reliable notebook-opening hook is found, report the limitation before choosing UI automation or a separate notebook implementation. Firmware-specific symbols must be validated; no guessed binary addresses.

## Subsequent milestones

- Hook only reader page-turn swipe handling, with a persistent toggle. Verify physical buttons, taps, selection, stylus, and menus separately.
- Start notebook sync with files explicitly exported by the user. Upload through WebDAV to Nextcloud. Add native export triggering only after validation. Readable exports and restorable native backups are separate products.
- Use Calibre-Web Kobo sync or a custom OPDS downloader. Check effects on Kobo services and Instapaper before changing the sync endpoint; downloading a file also requires native library import.
- Preserve Instapaper initially. Optional article extraction and EPUB generation belong on the server.

## Architecture boundaries

Device: native Nickel + small C/C++ extensions + lightweight sync scripts/client. Host prototype: Node.js for storage and tests only. Server: Nextcloud/WebDAV, Calibre-Web, optional article conversion.

The prototype store uses temporary-file replacement to avoid partial JSON writes. It assumes one writer; concurrent writers and device locking are not implemented.

## Sources

- https://github.com/pgaskin/NickelMenu
- https://github.com/pgaskin/NickelHook
- https://github.com/janeczku/calibre-web/wiki/Kobo-Integration
- https://help.kobo.com/hc/en-us/articles/360062226733-Use-your-Kobo-eReader-as-a-notebook
- https://help.kobo.com/hc/en-us/articles/33359968957463-Use-Instapaper-with-your-Kobo-eReader
