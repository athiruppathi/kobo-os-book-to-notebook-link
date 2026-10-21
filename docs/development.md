# Development guide

## Current implementation

The latest tested plugin is on [fix/swipe-pages-preserve-shortcuts](https://github.com/athiruppathi/kobo-os/tree/fix/swipe-pages-preserve-shortcuts). It includes the catalog-space fix and selective swipe page-turn suppression. These changes are not yet merged into main.

The device runs a C++/Qt plugin inside Nickel. Node.js and Python tools run on the development computer; neither is required on the Kobo.

| Location | Purpose |
| --- | --- |
| `native/links/links.cc` | Native navigation, saved links, command dispatch, and notebook menu action. |
| `native/links/native_ui.h` | Firmware-specific native dialogs and touch controls. |
| `native/links/live_catalog.h` | Live, read-only notebook catalog and folder validation. |
| `native/links/page_swipes.h` | In-memory suppression/restoration of swipe page-turn calls; latest branch only. |
| `native/links/page_swipe_sites.h` | Exact call-site bytes for the pinned firmware; latest branch only. |
| `native/` | Separate diagnostic probe and NickelHook build integration. |
| `src/`, `test/` | Host-side storage/navigation prototypes and regression tests. |
| `tools/` | Device inspection, packaging, and offline firmware analysis. |
| `.github/workflows/native-probe.yml` | Native catalog/swipe tests, ARM builds, and update packaging on the latest branch. |

## Tests and builds

Check out the latest tested branch before building the current plugin.

Host prototype tests require Node.js 22 or later:

```sh
node --test
```

The native Actions workflow runs catalog tests against temporary SQLite databases and tests call-site patch isolation, restoration, and mismatch rejection. It then compiles with `ghcr.io/pgaskin/nickeltc:1` and NickelHook pinned to `bcd90ba94d90d22562fb64ef2920ed3323c1dae5`.

The **kobo-notebook-update** artifact contains the installable `KoboRoot.tgz`. Other artifacts ending in **uninstalled** contain development libraries.

For manual packaging after a successful ARM build:

```sh
python3 tools/package-probe.py native/links/libkobobooklinks.so dist/KoboRoot.tgz --component links
```

The packager validates the ARM shared object and packages only the plugin library. Marker files and menu setup are documented in the [user guide](usage.md).

## Firmware boundaries

The supported target is Libra Colour firmware 4.46.23836 and basic notebooks. Native library SHA-256 checks gate the adapter. Swipe changes additionally validate exact instruction bytes before modifying loaded process memory.

Keep native navigation and save/close handling in Nickel. Read device metadata through read-only connections. Saved link JSON belongs to the plugin; do not edit the live Kobo database or notebook archives.

Keep device databases, notebook content, exports, credentials, and firmware dumps out of Git. Firmware analysis documents are evidence for this exact build, not a compatibility promise for future versions.

Passing host tests and compiling an ARM library do not establish device behavior. Record device feedback separately. The owner has reported the latest installed update works well.

## Future ideas

These are not implemented features:

- Export readable notebook copies and upload them to Nextcloud/WebDAV, with offline retries.
- Download DRM-free books through Calibre/Calibre-Web or OPDS.
- Add a server-side article-to-EPUB inbox.

Existing Kobo services remain outside the plugin's current feature scope.

## References

- [NickelHook](https://github.com/pgaskin/NickelHook)
- [NickelMenu](https://github.com/pgaskin/NickelMenu)
- [Firmware findings](firmware-4.46.23836.md)
- [Documentation index](README.md)
