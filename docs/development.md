# Developing Kilix Land

What the test suite checks and how the shared libraries are wired in. Moved from the [README](../README.md); the text is unchanged.

## Verify

```sh
make test
```

The test suite checks:

- that Land and pleb-plant-grower build from one `kilix-game-sdk` commit;
- menu wrapping, cast confirmation, all four cast profiles, and all 24
  first/repeat dialogue scripts;
- the conservatory door for every cast: entering only through the marked
  stretch of wall, the room standing still inside, and Escape returning the
  player beneath it;
- all 21 runtime graphics resources—including four social backgrounds, four
  source-game training backdrops, four enemy atlases, and the generated
  in-memory mirror cells;
- all 49 source-game WAV cues and all 17 plant cues through an offline mixer
  render;
- headless menu, room, and dialogue scenes;
- room and dialogue output for every cast under `build/cast-preview/`;
- source-enemy and in-progress attack output for every training range under
  `build/training-preview/`;
- 128 simulation-driven walking frames covering all four directions for every
  cast under `build/walk-preview/`;
- a conservatory session against a fixed clock and a private data root: Escape
  routing, a care verb and its sound, wall-clock aging, a byte-identical
  reload, three days away reconciled, a first run creating Land's data
  directory, and play without one;
- the door, the plant, its key list and calendar, and the plant at 800x450
  and 1280x720 under `build/conservatory-preview/`.

`make sanitize` rebuilds with clang's address and undefined-behavior
sanitizers, with leak detection on, and runs the same suite.

## Shared libraries

The shared libraries come from one recursive submodule,
`third_party/kilix-game-sdk`; `git submodule status` shows its pinned
revision. The conservatory's game is a second submodule,
`third_party/pleb-plant-grower`, pinned to a commit that uses the same SDK
revision. The four libraries Land builds against are reached through
symlinks beside it:

| Path | Target |
| --- | --- |
| `third_party/kilix-game-kit` | `kilix-game-sdk/kilix-game-kit` |
| `third_party/kilix-top-down-engine` | `kilix-game-sdk/kilix-top-down-engine` |
| `third_party/kilix-assets` | `kilix-game-sdk/kilix-assets` |
| `third_party/kilix-ui` | `kilix-game-sdk/kilix-ui` |

Clone with `--recurse-submodules`, or run
`git submodule update --init --recursive` in an existing checkout. The
top-level Makefile builds and links these libraries directly. Graphics
provenance is recorded in [`assets/graphics/README.md`](../assets/graphics/README.md).
