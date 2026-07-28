# Graphics provenance

## Per-cast rooms

All four backgrounds are environment-only 960×540 runtime assets. They share a
straight-on composition and clear lower floor so cast switching never changes
movement bounds or dialogue layout.

| Runtime file | Cast room | Research/style input | SHA-256 |
| --- | --- | --- | --- |
| `backgrounds/legend-room.png` | Hearthmarket beacon workshop | Legend of Kilix research: extracted Mystical Ninja scene contact sheet, used for retro pixel density and material cues with explicit no-copy constraints | `3f948c443c11…` |
| `backgrounds/chumrunner-room.png` | Null Market safehouse | `research/chumrunner/cinematics/contact-sheet.png` (private packet) | `34ed027f3c04…` |
| `backgrounds/fantasy-room.png` | Emberlight guild room | Kilix Fantasy character art and cinematic visual language | `c5a64afed33a…` |
| `backgrounds/pleb-bound-room.png` | Maple Loop repair-library | `research/pleb-bound/cinematics/contact-sheets/…-overview.png` (private packet) | `77e6cce1fa89…` |

The Legend, Chumrunner, and Pleb Bound rooms were generated on 2026-07-23
with OpenAI's built-in image-generation tool in new-image mode at 1672×941.
Each was center-cropped/downsampled with Lanczos filtering to 960×540. The
Fantasy room is the prior 2026-07-22 generation, now assigned specifically to
that cast. The original generated results remain in the built-in tool's image
store; every runtime derivative is present in this repository.

Research images were used as palette, material, lighting, and environment
references. The existing Fantasy room was used only as a functional
composition reference for the three new generations. No person, creature,
reference UI, legible text, or character silhouette is baked into a runtime
background.

The exact input roles and final prompt set are recorded in
[`backgrounds/PROMPTS.md`](backgrounds/PROMPTS.md).

The cast selector uses byte-identical copies of the source games' complete
runtime character and portrait atlases:

| Runtime file | Source | SHA-256 | Grid |
| --- | --- | --- | --- |
| `casts/legend-player.png` | Legend of Kilix `atlases/player.png` | `065f71f24064…` | 16×8 at 64×64 |
| `casts/legend-npcs.png` | Legend of Kilix `atlases/npcs.png` | `a320ed306b2b…` | 4×4 at 64×64 |
| `casts/legend-portraits.png` | Legend of Kilix `portraits/dialogue-portraits.png` | `d012051efcfb…` | 4×3 at 96×96 |
| `casts/chumrunner-characters.png` | Chumrunner `atlases/characters.png` | `439e1d9acf45…` | 8×4 at 222×222 |
| `casts/chumrunner-portraits.png` | Chumrunner `atlases/portraits.png` | `3e2791b32227…` | 4×2 at 444×444 |
| `casts/fantasy-characters.png` | Kilix Fantasy `atlases/characters.png` | `e269b9094b8c…` | 8×68 at 16×24 |
| `casts/fantasy-portraits.png` | Kilix Fantasy `atlases/portraits.png` | `341320c1f212…` | 27×4 at 40×40 |
| `casts/pleb-bound-characters.png` | Pleb Bound `atlases/characters.png` | `9431a60ead3b…` | 8×4 at 222×222 |
| `casts/pleb-bound-portraits.png` | Pleb Bound `atlases/portraits.png` | `fa9d7869c77c…` | 4×2 at 444×444 |

## Source-game training ranges

The training scene uses byte-identical gameplay assets from each original
game. No generated substitute is used for the range or its opponent.

| Runtime backdrop | Source | SHA-256 |
| --- | --- | --- |
| `training/legend-training.png` | Legend of Kilix `backgrounds/minigames/ember-range.png` | `c4994b6e5a2d…` |
| `training/chumrunner-training.png` | Chumrunner `bitmaps/backdrops/kestrel-lab.png` | `b6031d3a33d8…` |
| `training/fantasy-training.png` | Kilix Fantasy `bitmaps/backdrops/verdant-march.png` | `ec0572b7df93…` |
| `training/pleb-bound-training.png` | Pleb Bound `bitmaps/backdrops/civic-mall-annex.png` | `686c3a247d6e…` |

| Runtime enemy atlas | Source | SHA-256 | Grid |
| --- | --- | --- | --- |
| `casts/legend-enemies.png` | Legend of Kilix `atlases/enemies.png` | `aa23508d3676…` | 6×4 at 64×64 |
| `casts/chumrunner-enemies.png` | Chumrunner `atlases/enemies.png` | `9e1c0b277fc2…` | 8×4 at 222×222 |
| `casts/fantasy-enemies.png` | Kilix Fantasy `atlases/enemies.png` | `acfa13b13f6f…` | 15×25 at 96×112 |
| `casts/pleb-bound-enemies.png` | Pleb Bound `atlases/enemies.png` | `f0ae76f453ea…` | 4×8 at 222×222 |

Kilix Land selects only each game's ordinary enemy rows: 18 Legend creatures,
six Chumrunner archetypes, five Kilix Fantasy field enemies, or seven Pleb
Bound opponents. Boss-only cells remain in the copied atlases but are not
rolled by the training room.

Kilix Fantasy retains the former transparent enemy derivative as a source
master and deterministically component-repacks it. Detached attack and defeat
particles are assigned to their semantic monster before each 384×192 cell is
bottom-aligned and left-facing, so no adjacent monster fragment can cross a
runtime slice and every opponent faces the player from the right side of the
battlefield.

Every copied sheet comes from its source game's committed history, never its
working tree. Kilix Fantasy is rebuilding its cast around true SNES 16×24
rasters; that work is not in its history yet, so Land still carries the painted
sheets its history holds. The rebuilt-raster renderer is kept ready on the
`fantasy-snes-rasters` branch and lands when that pass is committed.

The full Kilix player atlas preserves Legend of Kilix's direction rows, frame
indices, timings, and `[32, 60]` foot anchor exactly.

Chumrunner, Kilix Fantasy, and Pleb Bound each author front walk A/B plus
left idle/walk poses. Kilix Land creates three small in-memory horizontal
mirrors per selected hero at load time: the mirrored left poses supply
rightward walking, while an alternating original/mirrored back pose makes
upward foot placement visible. The PNGs on disk remain byte-identical to their
source games.

The older root-level `room-background.png`, `companions.png`,
`companion-portraits.png`, `kilix.png`, and `kilix-portraits.png` are retained
only as prior-scene/source artifacts; they are not referenced by the current
manifest or loaded at runtime. The source games' provenance records remain
authoritative for all copied character art.
