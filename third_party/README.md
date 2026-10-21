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

`pleb-plant-grower` is a second recursive submodule: the houseplant game
behind Land's conservatory. Land links only its archive,
`libpleb-plant-grower.a`, through `pleb-plant-grower/mk/pleb-plant-grower.mk`,
and compiles it against Land's own `kilix-game-sdk` checkout, never the copy
nested inside it. `make sdk-check` confirms the two pins are the same
commit.
