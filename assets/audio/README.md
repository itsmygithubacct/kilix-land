# Native cast audio

Kilix Land's 49 WAV files are byte-identical, parity-managed selections from
the four source games. They cover cast-specific UI movement and confirmation,
dialogue, battle entry, weapon swings or shots, impacts and misses, enemy
attacks, player damage, victories, defeats, and Rook's reload.

`tools/sync_source_parity.py` owns the explicit source-to-runtime mapping.
Run `make parity-sync` after a source game changes; `make parity-check` rejects
missing or stale copies. `./kilix-land --audio-test` loads every cue through
`kilix-game-kit` and renders representative cues with its offline mixer.
