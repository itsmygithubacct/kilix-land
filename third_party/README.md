# Shared Kilix libraries

`kilix-game-sdk` is a recursive Git submodule holding every shared Kilix game
library at one pinned revision; `git submodule status` shows it. Land builds
against four of its components through the symlinks in this directory:

| Library | Purpose |
| --- | --- |
| `kilix-game-kit` | fixed-step clock and terminal runtime |
| `kilix-top-down-engine` | fitted software rendering |
| `kilix-assets` | bounded PNG cache, atlas cells, and clips |
| `kilix-ui` | dialogue panel, text, and portrait rendering |

The SDK retains each component's license and its pinned external
dependencies below `kilix-game-sdk/kilix-game-kit/third_party/`.
