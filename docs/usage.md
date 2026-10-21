# User guide

## Supported build

Use the latest tested branch and firmware listed in the [repository README](../README.md). The current target is Kobo Libra Colour, firmware **4.46.23836**, using **basic notebooks**.

The plugin uses NickelHook to load into Kobo's native interface. Its reading-menu actions use [NickelMenu](https://github.com/pgaskin/NickelMenu). The notebook-to-book action is added to the native notebook menu by the plugin.

## Updating an existing installation

1. Open a successful native Actions run for `fix/swipe-pages-preserve-shortcuts`.
2. Download the artifact named **kobo-notebook-update** while signed in to GitHub with repository access.
3. Extract the outer ZIP. It contains **KoboRoot.tgz**.
4. Connect the Kobo by USB and choose **Connect** on the device.
5. Copy **KoboRoot.tgz** to `.kobo/KoboRoot.tgz` on the Kobo drive. On Windows, enter a path such as `D:\.kobo` directly in File Explorer's address bar; the drive letter may differ.
6. Safely eject, unplug, and let installation finish.

Leave `KoboRoot.tgz` compressed. If an unrelated update is already staged under that name, let it finish before staging this one. Keep existing `.adds/kobo-os/` data and NickelMenu configuration.

The package installs only `/usr/local/Kobo/imageformats/libkobobooklinks.so`. It does not include first-time menu or marker setup.

## First-time setup

Back up the Kobo's USB-visible contents before installing. Set up NickelMenu using its own installation instructions, then install the plugin package as above.

On the Kobo drive:

1. Create `.adds/kobo-os/`.
2. Create empty files named **enabled** and **navigation.enabled** inside it. These have no extension; Windows must not save them as `.txt`.
3. Create a plain-text NickelMenu file at `.adds/nm/kobo-os-links` with these entries:

```text
menu_item : reader : Link notebook : cmd_spawn : quiet : test -p /tmp/kobo-os-book-links.fifo && printf "link\n" > /tmp/kobo-os-book-links.fifo
menu_item : reader : Open linked notebook : cmd_spawn : quiet : test -p /tmp/kobo-os-book-links.fifo && printf "open\n" > /tmp/kobo-os-book-links.fifo
```

Preserve other NickelMenu files. Safely eject and restart the Kobo so the plugin initializes with its enabled marker present. The current build reads the live notebook catalog; it does not require a prepared `notebooks.json`.

## Link a book to a notebook

1. Open a book in Kobo's native reader.
2. Open the reading NickelMenu and choose **Link notebook**.
3. Browse notebook folders. Use **Up**, **Previous**, and **Next** as needed.
4. Select a basic notebook and confirm the path.

Each book has one notebook link. Linking it again replaces that book's previous mapping. Several books may share a notebook.

## Move between reading and writing

- From the reading menu, choose **Open linked notebook**. The plugin opens the notebook at its last existing page; it does not create an extra page.
- From the native notebook menu, choose **Open linked book**. If several books link to it, choose a book from the list.
- An unlinked book shows **No linked notebook**; an unlinked notebook shows **No linked book**.

Native navigation handles opening and closing. The plugin uses stable book ContentID and notebook ExternalId values for saved associations.

## Swipe behavior

Swipe page turns are disabled by default when the plugin is enabled. Native quick-access and footer swipe shortcuts remain available. Page buttons, taps, menus, and handwriting keep their native behavior.

The change targets page-turn calls within swipe handlers, including both basic-notebook rotation paths. It does not discard whole gesture events.

To restore native swipe page turns, create an empty file:

```text
.adds/kobo-os/allow-page-swipes
```

The setting is checked on the next gesture after reconnecting/disconnecting USB. Remove that file to disable swipe page turns again.

## Files and settings

Paths below are relative to the Kobo's USB drive.

| Path | Purpose |
| --- | --- |
| `.adds/kobo-os/enabled` | Enables plugin initialization and commands. |
| `.adds/kobo-os/navigation.enabled` | Enables opening linked books/notebooks. |
| `.adds/kobo-os/allow-page-swipes` | Restores native swipe page turns when present. |
| `.adds/kobo-os/book-links.json` | Saved book-to-notebook associations. Preserve this when updating. |
| `.adds/kobo-os/status.json` | Diagnostic status from plugin actions. |
| `.adds/nm/kobo-os-links` | This project's reading-menu entries. |

The plugin reads Kobo's notebook catalog through a read-only database connection. It saves its own link JSON separately; it does not rewrite notebook archives or the Kobo database.

## Disable or remove

- **Disable navigation:** remove `.adds/kobo-os/navigation.enabled`.
- **Disable the plugin:** remove `.adds/kobo-os/enabled` and restart the Kobo.
- **Uninstall:** create the empty marker `.adds/kobo-os-links.uninstall` and restart. Remove only this project's NickelMenu file afterward.

Keep `book-links.json` if you want to preserve associations for a later reinstall. Swipe patches affect process memory only and disappear when the process restarts.

## Troubleshooting

- **No reading-menu entries:** confirm NickelMenu and this project's menu file are installed.
- **Menu action does nothing:** check the enabled marker, restart, and confirm the device/firmware matches the supported build. The plugin refuses unknown native library hashes.
- **Notebook navigation is disabled:** check `navigation.enabled` has no hidden `.txt` extension.
- **Invalid notebook path in catalog:** use the latest tested build, which includes the fix for spaces in native notebook file URLs.
- **Cannot find the update on GitHub:** open the run, scroll to **Artifacts**, and choose **kobo-notebook-update**. Use Edge/Chrome if an embedded browser does not save the download. Expired artifacts require a new build.
