# Native diagnostic trial

The probe resolves symbols without invoking native notebook functions. Loading a
plugin still executes inside Nickel. This is a diagnostic, not book/notebook
navigation, and it does not change reading or swipe controls.

## Build and package

Build source: `af4858af7d6ea854d2c5ad8fcbe3899fb25b6e8f`.
Successful CI: https://github.com/athiruppathi/kobo-os/actions/runs/37331444499
Artifact ZIP SHA-256: `5b830f7da43dc83262f4e7e9cc0b6068b24fd033c39705323109bc5944d1fedb`.

The extracted library was checked as ELF32 little-endian ARM with an exported
NickelHook descriptor. Package using:

```sh
python tools/package-probe.py PATH_TO_LIBKOBOLINKPROBE_SO OUTPUT_TGZ
```

The package contains one regular file only:
`./usr/local/Kobo/imageformats/libkobolinkprobe.so` (root:root, mode 0644).
It does not replace NickelMenu or the native reader library.

## Device trial sequence

1. Confirm firmware 4.46.23836 and back up all USB-visible contents. Verify the
   copy before staging. These private backups stay under ignored device-dumps/.
2. Ensure `.kobo/KoboRoot.tgz` is absent; never replace an unrelated pending update.
3. Create `.adds/kobo-os-probe.enabled` and copy the reviewed package to
   `.kobo/KoboRoot.tgz`. The marker enables the diagnostic at Nickel startup.
4. Safely eject using Windows, unplug, and let the reader finish its update and
   return to the home screen. Avoid interrupting the update.
5. Connect USB again and tap Connect. Read `.adds/kobo-os-probe.txt`.
   Expected: firmware 4.46.23836, all nine symbols present, missing=0,
   native_calls=0. An absent report is inconclusive, not a successful result.

## Remove the diagnostic

Create `.adds/kobo-os-probe.uninstall` and restart the reader. NickelHook uses
this marker to uninstall this plugin. Removing `.adds/kobo-os-probe.enabled`
only disables the probe body; it does not unload or remove the plugin.
After successful removal, the report and marker may be deleted over USB.

If the reader does not return to the home screen, report what is visible before
trying recovery. The user-data backup is not a full root-filesystem image and is
not a guarantee of recovery from every firmware failure.
