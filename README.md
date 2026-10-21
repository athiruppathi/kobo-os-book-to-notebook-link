# Kobo OS

A native Kobo plugin for reading and handwritten notes on the **Kobo Libra Colour**. Link a book to a basic notebook, jump between them, and prevent accidental swipe page turns while keeping Kobo's vertical shortcuts.

The plugin extends Kobo's existing Nickel reader and notebooks. Books and handwriting stay in Kobo's native apps; Node.js and Python are only development tools on your computer.

## What it adds

| Feature | Behavior |
| --- | --- |
| **Link notebook** | Associate the current book with an existing basic notebook. |
| **Browse notebook folders** | Choose from the live notebook catalog, with nested folders and Up / Previous / Next controls. No USB catalog refresh is needed. |
| **Open linked notebook** | Jump from a book to its notebook at the **last existing page**, ready to continue writing. |
| **Open linked book** | Return from a notebook through its native menu. If several books link to that notebook, choose which to open. |
| **Disable swipe page turns** | Stop swipe-triggered page changes in books and basic notebooks, including both notebook rotation paths. |
| **Keep vertical shortcuts** | Preserve native quick-access and footer gestures, along with page buttons, taps, menus, and handwriting. |

Links use stable book and notebook identities rather than titles. The notebook catalog also handles spaces in notebook paths.

## Compatibility and current version

- **Tested device:** Kobo Libra Colour.
- **Firmware:** **4.46.23836**. Native library hashes and swipe call-site bytes are checked at runtime.
- **Notebook type:** basic notebooks. Advanced notebooks and other device/firmware combinations are not supported by this build.
- The owner has installed the latest build and reported that it works well. This is a personal plugin with limited device coverage.

**Latest tested plugin source:** [`fix/swipe-pages-preserve-shortcuts`](https://github.com/athiruppathi/kobo-os/tree/fix/swipe-pages-preserve-shortcuts), commit [`20d38a1`](https://github.com/athiruppathi/kobo-os/commit/20d38a1d498f2999ba429207f53e6b9558d35ed2).

The catalog-space fix and swipe changes are still on feature branches; they have **not been merged into `main`**. Use the source/build above for the complete feature set described here.

## Install or update

For an existing installation:

1. Download the **`kobo-notebook-update`** artifact from a successful [native build](https://github.com/athiruppathi/kobo-os/actions/workflows/native-probe.yml) on the latest tested branch.
2. Extract the downloaded ZIP to obtain **`KoboRoot.tgz`**. Leave the TGZ compressed.
3. Connect the Kobo by USB and copy `KoboRoot.tgz` into its `.kobo` folder.
4. Safely eject and unplug. Let the Kobo install the update and restart.

Preserve existing menu entries, marker files, and saved links. The update package installs the plugin library only.

[Installation, first-time setup, usage, and disabling the plugin →](docs/usage.md)

GitHub's **Code → Download ZIP** and the old `Kobo_Swipe_Filter_Source.zip` are source archives, not installable updates. Actions artifacts expire; the [verified build run](https://github.com/athiruppathi/kobo-os/actions/runs/39531898766) records the build and tests.

## Documentation

- [User guide](docs/usage.md): setup, book/notebook actions, settings, and troubleshooting.
- [Change history](CHANGELOG.md): delivered features and validation.
- [Development guide](docs/development.md): source layout, tests, builds, and future ideas.
- [Documentation index](docs/README.md): implementation notes and historical research.

## Scope

The current plugin focuses on book/notebook navigation and swipe controls. Nextcloud/WebDAV notebook uploads, Calibre downloads, and an article-to-EPUB inbox remain ideas; this plugin does not implement them.
