# KoboLinkProbe

First native component: a NickelHook plugin that resolves nine notebook-related
symbols without invoking them, constructing native objects, or installing hooks.
This is not the linked-notebook feature. Compilation does not establish runtime
compatibility or notebook-navigation safety.

The GitHub workflow builds with NickelTC and a pinned NickelHook commit. It
uploads only an uninstalled shared library, not an auto-installing firmware
package. No deployment runs in CI. The NickelTC major-version image tag follows
upstream's documented example; it is not an immutable toolchain digest.

At initialization the probe requires both firmware `4.46.23836` and an explicit
`.adds/kobo-os-probe.enabled` file. It writes a short symbol-presence report to
`.adds/kobo-os-probe.txt`, closes the file immediately, and never reads notebooks
or the database. If disabled, it does nothing. A firmware mismatch is logged and
skipped. A stale `.tmp` report prevents a new report rather than being overwritten.
Presence of symbols is not proof of their runtime calling contracts.

Installation has not been performed. Before a device trial, back up user data,
verify the compiled artifact, prepare the NickelHook uninstall marker
`.adds/kobo-os-probe.uninstall`, and confirm recovery access via USB. The loader
itself runs inside Nickel even though this probe makes no native calls.

Local build requires Linux/Docker:

```sh
git clone https://github.com/pgaskin/NickelHook native/NickelHook
git -C native/NickelHook checkout bcd90ba94d90d22562fb64ef2920ed3323c1dae5
docker run --rm -v "$PWD:/src" -w /src/native ghcr.io/pgaskin/nickeltc:1 make
```

References:
- https://github.com/pgaskin/NickelHook
- https://github.com/pgaskin/NickelTC
