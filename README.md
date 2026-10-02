# Kobo OS

Personal extensions for Kobo Libra Colour, retaining Kobo's native Nickel reader, notebooks, and Instapaper.

## Current status

Host-side prototype only. Nothing here modifies the Kobo firmware yet.

Device target: **Kobo Libra Colour, firmware 4.46.23836, basic notebooks only**. Native hook compatibility has not been verified.

The first implemented component stores a stable book ID -> notebook ID association. Native notebook opening, reader return, and device controls remain unimplemented until we inspect the device's firmware and navigation interfaces. This project does not assume NickelMenu can directly open an arbitrary notebook.

## Run

Requires Node.js 22 or newer on your computer (Node is not required or assumed on the Kobo).

```sh
node --test
node tools/book-links.mjs link book-id notebook-id
node tools/book-links.mjs get book-id
node tools/book-links.mjs unlink book-id
```

Run commands from this repository. The local state file defaults to `data/book-links.json`; set `KOBO_LINKS_FILE` to change it. Use actual stable IDs once discovered; filenames and titles are not reliable identifiers.

## Planned features

- Disable reading page-turn swipes independently from buttons, selection, and menus.
- Choose a notebook for each book, open it from the reader, and return to the same book position.
- Export notebooks and upload readable copies to Nextcloud/WebDAV, with an offline retry queue.
- Download DRM-free books from Calibre/Calibre-Web.
- Keep native Instapaper; optionally add a server-side article-to-EPUB inbox later.

See [development plan](docs/development.md) for implementation gates and source references.

## Inspect a connected Kobo

Connect the reader by USB and choose Connect on the device. Run the read-only inventory with its drive letter (replace `E:/` with the actual mount):

```sh
node tools/inspect-device.mjs E:/
```

It reports firmware, database presence, possible notebook directory names and counts, notebook file and ZIP-signature counts, exported file counts, and NickelMenu configuration presence. It does not open the database or output serial numbers, notebook names, or notebook contents. Internal directory detection is heuristic.

See [device findings](docs/device-discovery.md) for the initial read-only inspection.
