# Change history

## Current tested build — 2026-10-21 UTC

Source: [20d38a1](https://github.com/athiruppathi/kobo-os/commit/20d38a1d498f2999ba429207f53e6b9558d35ed2) on `fix/swipe-pages-preserve-shortcuts`.

- Disable swipe-triggered page turns in books and basic notebooks.
- Preserve vertical quick-access and footer gestures by changing specific page-turn calls instead of filtering entire gestures.
- Cover both notebook rotation paths while retaining native page buttons, taps, menus, and handwriting.
- Add the `allow-page-swipes` marker to restore native behavior.
- Validate native library hashes and six exact call sites; patch process memory only.
- Pass catalog and swipe-patching tests, ARM compilation, and update packaging in [Actions run 39531898766](https://github.com/athiruppathi/kobo-os/actions/runs/39531898766).
- Owner installed the update and reported everything working well. Broader device/firmware compatibility has not been established.

## Notebook catalog fix — 2026-10-20

Source: [abf262e](https://github.com/athiruppathi/kobo-os/commit/abf262e1f4a518f345c4fe35110c1c8659cdb4c0) on `fix/notebook-catalog-spaces`; included in the current tested build.

- Fix “Invalid notebook path in catalog” for native file URLs containing literal spaces while retaining strict URL validation.
- Add regression coverage and package `KoboRoot.tgz` directly in Actions.
- Owner installed and confirmed the fix worked.

## Earlier delivered functionality

- Store book-to-basic-notebook associations using stable identities.
- Add **Link notebook** and **Open linked notebook** reading-menu actions.
- Add **Open linked book** to the native notebook menu, including a chooser when several books share a notebook.
- Browse the live notebook catalog and nested folders with Up / Previous / Next navigation.
- Open a linked notebook at its last existing page after native initialization.
- Keep associations in the plugin's own JSON file and query Kobo metadata read-only.

The current feature branches have not been merged into `main`. Historical development notes remain under [docs](docs/README.md).
