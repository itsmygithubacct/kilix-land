# Shared Kilix libraries

This directory contains recursive Git checkouts of the shared Kilix C
libraries used by the game:

| Library | Revision | Purpose |
| --- | --- | --- |
| `kilix-game-kit` | `1bcd352a73c0` | fixed-step clock and terminal runtime |
| `kilix-top-down-engine` | `37b3ade5ac6d` | fitted software rendering |
| `kilix-assets` | `db347356644a` | bounded PNG cache, atlas cells, and clips |
| `kilix-ui` | `b7289c238aad` | dialogue panel, text, and portrait rendering |

Each checkout retains its `.git` metadata, recursive submodules, upstream
license, and pinned dependency revisions.
