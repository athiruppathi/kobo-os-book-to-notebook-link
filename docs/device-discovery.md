# Device discovery: 2026-10-02

Read-only inspection of the connected Kobo Libra Colour confirmed:

- Firmware: 4.46.23836.
- User uses basic notebooks.
- Native reader database exists at `.kobo/KoboReader.sqlite`.
- Notebook files live under `My Notebooks`, including nested folders.
- Seven `.nebo` files have ZIP local-file header signatures.
- A sample archive contains `meta.json`, `rel.json`, `index.bdom`, and page-specific `ink.bink`, `page.bdom`, metadata, and styles.
- `Exported Notebooks` exists. No exported files were present during inspection.
- NickelMenu configuration directory exists, with a Cobalt launcher entry. Actual installed binary compatibility was not verified.
- The Cobalt launcher comment says it stops the native reader and requires a device restart to return. Do not use this launcher for seamless reader/notebook switching.
- Database schema includes native content IDs and saved reading-position fields. No dedicated notebook table was found in the inspected reader database. This does not establish where notebook IDs are stored.

No device files were modified. No credentials, serial numbers, titles, notebook contents, or database copies are stored in this repository.

## Implications

Original `.nebo` containers can be copied as backup artifacts. Restoration and cross-version compatibility are untested. A copied container is not a readable PDF export and should not be described as a tested restore solution.

Next: inspect notebook metadata structure and upstream native navigation code to identify notebook IDs and a reliable native opening interface. Keep the proprietary ink engine handling handwriting. Validate reader return separately.
