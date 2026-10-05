# Book-to-notebook implementation

## Implemented

`src/book-notebook-navigation.mjs` implements navigation policy against an
injected native adapter and the existing BookLinks store. It captures the active
book, resolves its linked basic notebook, retains an opaque reader return handle,
and restores that handle only after native notebook exit preparation succeeds.
Concurrent activation and nested notebook opens are rejected. Missing links,
unavailable notebooks, failed opens, cancelled exits, and failed restores have
explicit ownership/retry behavior. Tests use a simulated adapter.

This JavaScript module runs on the development host. It is an executable policy
reference, not code currently running inside Nickel. No native adapter or menu
action is implemented yet; no new package is ready to install.

## Native adapter contract

All UI operations must execute on Nickel's GUI thread. Never block that thread
waiting for a view transition. Adapter methods:

| Method | Required behavior |
| --- | --- |
| captureReader | Return `{bookId, context}` for an active book, or null. Context retains the native reader safely, including its reading position. |
| resolveNotebook | Resolve current ExternalId against the current device; return a basic notebook descriptor or an unavailable result. Own and release any temporary native Volume internally. |
| openNotebook | Use native resource checks; resolve only after the notebook is shown. Rejection must leave or restore the original reader. |
| prepareNotebookExit | Follow the native handwriting save/close path; return false on cancellation. Do not bypass it by writing SQLite or copying the notebook archive. |
| restoreReader | Restore the retained reader and confirm its view; reject without invalidating the context if restoration fails. |
| releaseReader | Release one acquired context without throwing; it must remain valid until this call. |

Return handles are session-only and must not be serialized. The final device
implementation will need view-change/destruction tracking so that manually
leaving a notebook cannot reuse a stale reader handle.

## Runtime and ABI evidence

On October 5, firmware 4.46.23836 ran the installed symbol-only diagnostic:
all nine symbols present, `missing=0`, `native_calls=0`. This establishes symbol
availability, not object ownership or a successful navigation call.

Offline instruction analysis of notebook lookup at 0xa96cc8 observes r1 used as
the QString object, r2 passed as Device to getDbName, and incoming r0 forwarded
to VolumeManager::find and returned. This is consistent with a static method
using a hidden aggregate-return pointer. The returned type/layout still needs
confirmation through find and callers; do not encode a guessed C function
pointer or allocate a guessed Volume size.

The N3ReaderOpener constructor calls QObject construction and copies reference-
counted data; cleanup includes Volume and QObject destructors. A raw allocation
with a guessed object size is not a valid adapter implementation.

Use `tools/inspect-native-calls.py --instructions` with `--symbol` to reproduce
instruction-level inspection of the pinned firmware. It is linear disassembly,
not a complete control-flow analysis; trailing literal pools may decode as data.

## Next device work

1. Establish Volume return ABI and opener lifetime from native call sites.
2. Implement active-reader capture, GUI-thread dispatch, and destruction tracking.
3. Connect an explicit menu action to lookup and the native resource-checking opener.
4. Verify native notebook save/close and reader restoration on a test book/notebook.

Host verification: `node --test`. Adapter tests do not establish device safety
or prove that unsaved handwriting survives a native transition.
