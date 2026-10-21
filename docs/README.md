# Documentation

## Start here

- [Repository overview](../README.md): purpose, features, and supported build.
- [User guide](usage.md): installation, linking, navigation, settings, and troubleshooting.
- [Change history](../CHANGELOG.md): delivered features and validation.
- [Development guide](development.md): architecture, tests, packaging, and future work.

## Implementation and research archive

These documents record earlier milestones and exact-firmware analysis. Statements such as “not implemented,” “candidate,” or “device verification pending” describe the milestone when written. Use the current overview and user guide for installation and feature status; older artifact links may have expired.

| Document | Topic |
| --- | --- |
| [Book/notebook implementation](book-notebook-implementation.md) | Host-side link model and initial integration design. |
| [Device discovery](device-discovery.md) | Initial read-only device inventory. |
| [Firmware 4.46.23836](firmware-4.46.23836.md) | Native symbols, compatibility checks, and firmware evidence. |
| [Notebook identities](notebook-identities.md) | Stable notebook identities and metadata discovery. |
| [Notebook navigation](notebook-navigation.md) | Investigation of native navigation interfaces. |
| [Native probe trial](native-probe-trial.md) | Early symbol-only diagnostic. |
| [Native book-links trial](native-book-links-trial.md) | Earlier chooser and book-to-notebook iterations. |
| [Notebook to book](notebook-to-book.md) | Reverse navigation and notebook menu integration. |
| [Live catalog and last page](live-notebooks-last-page.md) | Live folder browsing and native last-page navigation. |

Selective swipe implementation details and exact call-site evidence are available on the [tested feature branch](https://github.com/athiruppathi/kobo-os/tree/fix/swipe-pages-preserve-shortcuts/docs).
