#!/usr/bin/env python3
"""Synchronize Kilix Land's playable subset with the four source games."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import struct
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]


def repository_root(path: Path) -> Path:
    for candidate in (path, *path.parents):
        if (candidate / ".git").exists():
            return candidate
    raise ValueError(f"no git repository contains {path}")


def git_output(root: Path, *arguments: str) -> bytes:
    result = subprocess.run(
        ["git", "-C", str(root), *arguments],
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.decode("utf-8", "replace").strip()
        raise ValueError(f"git {' '.join(arguments)} failed in {root}: {detail}")
    return result.stdout


def committed_bytes(path: Path) -> bytes:
    """Read a source game's committed content.

    Land pins itself to what each source game has committed, never to its
    working tree. A game mid-art-pass has files on disk that its own history
    has not accepted yet, and Land must not ship those.
    """
    root = repository_root(path)
    relative = path.resolve().relative_to(root.resolve()).as_posix()
    return git_output(root, "show", f"HEAD:{relative}")


def committed_text(path: Path) -> str:
    return committed_bytes(path).decode("utf-8")


def committed_commit(path: Path) -> str:
    root = repository_root(path)
    return git_output(root, "rev-parse", "HEAD").decode("ascii").strip()


def load_json(path: Path) -> dict[str, Any]:
    """Read one of Land's own files from disk."""
    return json.loads(path.read_text(encoding="utf-8"))


def load_committed_json(path: Path) -> dict[str, Any]:
    """Read a source game's file as that game committed it."""
    return json.loads(committed_text(path))


def record(records: list[dict[str, Any]], key: str, value: str) -> dict[str, Any]:
    for item in records:
        if item.get(key) == value:
            return item
    raise ValueError(f"missing {key}={value}")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def png_dimensions(data: bytes) -> tuple[int, int]:
    if (
        len(data) < 24
        or data[:8] != b"\x89PNG\r\n\x1a\n"
        or data[12:16] != b"IHDR"
    ):
        raise ValueError("copied graphics asset is not a valid PNG")
    return struct.unpack(">II", data[16:24])


def source_digest(paths: list[Path]) -> str:
    digest = hashlib.sha256()
    for path in paths:
        digest.update(path.name.encode("utf-8"))
        digest.update(b"\0")
        digest.update(committed_bytes(path))
    return digest.hexdigest()


def c_string(value: str) -> str:
    return json.dumps(value.upper())


def clamp(value: int, low: int, high: int) -> int:
    return max(low, min(high, value))


def c_define_number(path: Path, name: str) -> float:
    pattern = re.compile(
        rf"^#define\s+{re.escape(name)}\s+"
        r"([0-9]+(?:\.[0-9]+)?)f?\s*$",
        re.MULTILINE,
    )
    match = pattern.search(committed_text(path))
    if not match:
        raise ValueError(f"missing numeric C define {name} in {path}")
    return float(match.group(1))


def c_assignment_int(path: Path, field: str) -> int:
    pattern = re.compile(
        rf"\bgame->{re.escape(field)}\s*=\s*([0-9]+)\s*;"
    )
    match = pattern.search(committed_text(path))
    if not match:
        raise ValueError(f"missing C assignment {field} in {path}")
    return int(match.group(1))


def generated_header(games_root: Path) -> str:
    legend_game_path = games_root / "legend-of-kilix/src/game.c"
    legend_header_path = (
        games_root / "legend-of-kilix/src/legend_of_kilix.h"
    )
    chum_header_path = games_root / "chumrunner/src/game.h"
    chum_path = games_root / "chumrunner/content/campaign.json"
    fantasy_path = games_root / "kilix-fantasy/content/campaign.json"
    pleb_path = games_root / "pleb-bound/content/campaign.json"
    chum = load_committed_json(chum_path)
    fantasy = load_committed_json(fantasy_path)
    pleb = load_committed_json(pleb_path)

    simulation_hz = int(
        c_define_number(legend_header_path, "LOK_SIMULATION_HZ")
    )
    legend_hp = c_assignment_int(legend_game_path, "maximum_vitality")
    legend_charge = c_assignment_int(
        legend_game_path, "maximum_rifle_charge"
    )
    legend_player_speed = c_define_number(
        legend_game_path, "LOK_PLAYER_SPEED"
    )
    legend_laser_speed = c_define_number(
        legend_game_path, "LOK_LASER_SPEED"
    )
    legend_cooldown = c_define_number(
        legend_game_path, "LOK_ATTACK_COOLDOWN"
    )
    legend_butt_seconds = c_define_number(
        legend_header_path, "LOK_RIFLE_BUTT_ANIMATION_SECONDS"
    )
    legend_fire_seconds = c_define_number(
        legend_header_path, "LOK_RIFLE_FIRE_ANIMATION_SECONDS"
    )
    legend_knockout_seconds = c_define_number(
        legend_header_path, "LOK_KNOCKOUT_RECOVERY_SECONDS"
    )

    predator = record(chum["equipment"], "id", "predator")
    jackal = record(chum["starter_archetypes"], "id", "iron-jackal")
    mara = record(chum["contacts"], "id", "mara-velle-contact")
    armor = record(chum["equipment"], "id", jackal["armor"])
    cyberware = record(chum["equipment"], "id", jackal["cyberware"])
    ammunition = record(chum["ammunition"], "id", predator["ammo"])
    attributes = jackal["attributes"]
    skills = jackal["skills"]
    reflex_effect = (
        cyberware["power"]
        if cyberware.get("cyberware_effect") == "reflex"
        else 0
    )
    sensor_effect = (
        cyberware["power"]
        if cyberware.get("cyberware_effect") == "sensor"
        else 0
    )
    resilience_effect = (
        cyberware["power"]
        if cyberware.get("cyberware_effect") == "resilience"
        else 0
    )
    armor_effect = (
        cyberware["power"]
        if cyberware.get("cyberware_effect") == "armor"
        else 0
    )
    chum_hp = (
        40
        + attributes[1] * 8
        + attributes[0] * 4
        + resilience_effect * 3
    )
    chum_armor = attributes[1] // 2 + armor["power"] + armor_effect
    chum_derived_defense = (
        attributes[2] + attributes[1] + skills[4]
        + reflex_effect + sensor_effect
    )
    chum_mitigation = chum_armor + chum_derived_defense // 8
    firearm_score = attributes[2] + skills[0] * 2 + reflex_effect
    close_combat_score = attributes[0] + skills[1] * 2 + reflex_effect
    chum_raw_min = predator["power"] + firearm_score // 8
    chum_melee_base = int(
        c_define_number(chum_header_path, "CHUM_MELEE_BASE_DAMAGE")
    )
    chum_melee_ticks = int(
        c_define_number(chum_header_path, "CHUM_STANDARD_ATTACK_TICKS")
    )

    initial_roster = sorted(
        (
            item
            for item in fantasy["roster"]
            if item.get("initial_party") == 0
            and item.get("initial_slot", -1) >= 0
        ),
        key=lambda item: item["initial_slot"],
    )
    if len(initial_roster) < 4:
        raise ValueError("Kilix Fantasy no longer has four initial party members")
    arden = initial_roster[0]
    gear = fantasy["gear"]
    loadout = [
        arden.get("weapon", -1),
        arden.get("offhand", -1),
        arden.get("head", -1),
        arden.get("armor", -1),
        arden.get("accessory", -1),
        arden.get("relic2", -1),
    ]
    equipped = [gear[index] for index in loadout if index >= 0]
    fantasy_hp = (
        arden["base_hp"]
        + arden["hp_growth"]
        + sum(item["max_hp"] for item in equipped)
    )
    fantasy_defense = sum(item["defense"] for item in equipped)
    fantasy_attack = arden["attack"] + sum(item["attack"] for item in equipped)
    fantasy_standard_power = 13

    pip = record(pleb["members"], "id", "pip")
    bent_bat = record(pleb["items"], "id", "bent_bat")
    pip_stats = pip["stats"]
    pip_offense = pip_stats["offense"]["base"]
    pip_defense = pip_stats["defense"]["base"] + bent_bat.get("defense", 0)
    pip_speed = pip_stats["speed"]["base"]
    pip_nerve = pip_stats["nerve"]["base"]
    pip_vitality = pip_stats["vitality"]["base"]
    pip_luck = pip_stats["luck"]["base"]
    pip_hp = pip["hp_base"] + pip["hp_per_vitality"] * pip_vitality
    pip_accuracy = clamp(
        75 + pip_luck + bent_bat.get("accuracy", 0), 25, 100
    )
    pip_evasion = clamp(
        pip_luck // 2 + pip_speed // 8 + pip["evasion_bias"], 0, 75
    )
    pip_critical = clamp(
        5 + pip_nerve // 2 + bent_bat.get("accuracy", 0) // 2,
        5,
        35,
    )

    ai_values = {
        "assault": 0,
        "ranged": 1,
        "cover": 2,
        "hunter": 3,
        "sentry": 4,
        "support": 5,
        "breacher": 6,
        "suppressor": 7,
        "controller": 8,
        "saboteur": 9,
        "adept": 10,
        "matrix-linked": 11,
    }
    timing_values = {"center": 0, "bounce": 1, "double": 2}

    chum_rows = []
    for enemy in chum["enemies"]:
        move_speed = 36 + enemy["perception"]
        chum_rows.append(
            "    X("
            + ", ".join(
                [
                    c_string(enemy["name"]),
                    str(enemy["hp"]),
                    str(enemy["armor"]),
                    str(enemy["damage"]),
                    str(move_speed),
                    "true",
                    str(enemy["visual_row"]),
                    "100",
                    "0",
                    "3",
                    "1",
                    str(enemy["range"]),
                    str(enemy["preferred_range"]),
                    str(ai_values[enemy["ai"]]),
                    "0",
                    "false",
                    "0",
                ]
            )
            + ")"
        )

    fantasy_rows = []
    for index, enemy in enumerate(fantasy["enemies"][:5]):
        # Kilix Fantasy addresses its enemy art by visual identity: the atlas
        # holds three poses per identity, five identities per row.
        visual = enemy["sprite"]
        move_speed = 36 + enemy["ai"] * 3 + index % 5
        fantasy_rows.append(
            "    X("
            + ", ".join(
                [
                    c_string(enemy["name"]),
                    str(enemy["hp"]),
                    str(enemy["defense"]),
                    str(enemy["attack"]),
                    str(move_speed),
                    "true" if visual in (1, 3) else "false",
                    str(visual),
                    "100",
                    "0",
                    "5",
                    "1",
                    "0",
                    "0",
                    str(enemy["ai"]),
                    str(enemy["level"]),
                    "false",
                    "0",
                ]
            )
            + ")"
        )

    pleb_rows = []
    for visual, enemy in enumerate(pleb["enemies"][:7]):
        pleb_rows.append(
            "    X("
            + ", ".join(
                [
                    c_string(enemy["name"]),
                    str(enemy["max_hp"]),
                    str(enemy["defense"]),
                    str(enemy["attack"]),
                    str(enemy["speed"]),
                    "true" if visual in (1, 3, 5) else "false",
                    str(visual),
                    str(enemy["accuracy"]),
                    str(enemy["evasion"]),
                    str(enemy["damage_variance"]),
                    str(enemy["hits"]),
                    "0",
                    "0",
                    "0",
                    "0",
                    (
                        "true"
                        if enemy.get("attack_kind", "physical") == "ability"
                        else "false"
                    ),
                    str(timing_values[enemy["timing"]]),
                ]
            )
            + ")"
        )

    digest = source_digest(
        [
            legend_game_path,
            legend_header_path,
            chum_header_path,
            chum_path,
            fantasy_path,
            pleb_path,
        ]
    )
    legend_commit = committed_commit(legend_game_path)[:12]
    chum_commit = committed_commit(chum_path)[:12]
    fantasy_commit = committed_commit(fantasy_path)[:12]
    pleb_commit = committed_commit(pleb_path)[:12]

    def table(name: str, rows: list[str]) -> str:
        return (
            f"#define {name}(X) \\\n"
            + " \\\n".join(rows)
            + "\n"
        )

    return f"""\
/* Generated by tools/sync_source_parity.py; do not edit by hand.
 * Every value below is read from the source games' committed history, never
 * from their working trees, so an in-flight art or balance pass in another
 * repository cannot reach Kilix Land until that repository commits it.
 * Source content digest: {digest}
 */
#ifndef KILIX_LAND_SOURCE_PARITY_H
#define KILIX_LAND_SOURCE_PARITY_H

#define LAND_PARITY_LEGEND_SOURCE_COMMIT "{legend_commit}"
#define LAND_PARITY_CHUM_SOURCE_COMMIT "{chum_commit}"
#define LAND_PARITY_FANTASY_SOURCE_COMMIT "{fantasy_commit}"
#define LAND_PARITY_PLEB_SOURCE_COMMIT "{pleb_commit}"

#define LAND_PARITY_LEGEND_PLAYER_HP {legend_hp}
#define LAND_PARITY_LEGEND_MAX_RIFLE_CHARGE {legend_charge}
#define LAND_PARITY_LEGEND_PLAYER_SPEED {legend_player_speed:.1f}f
#define LAND_PARITY_LEGEND_LASER_SPEED {legend_laser_speed:.1f}f
#define LAND_PARITY_LEGEND_ATTACK_COOLDOWN_TICKS \
{math.ceil(legend_cooldown * simulation_hz)}
#define LAND_PARITY_LEGEND_RIFLE_BUTT_TICKS \
{math.ceil(legend_butt_seconds * simulation_hz)}
#define LAND_PARITY_LEGEND_RIFLE_FIRE_TICKS \
{math.ceil(legend_fire_seconds * simulation_hz)}
#define LAND_PARITY_LEGEND_KNOCKOUT_TICKS \
{math.ceil(legend_knockout_seconds * simulation_hz)}

#define LAND_PARITY_CHUM_PLAYER_HP {chum_hp}
#define LAND_PARITY_CHUM_PLAYER_ARMOR {chum_armor}
#define LAND_PARITY_CHUM_PLAYER_MITIGATION {chum_mitigation}
#define LAND_PARITY_CHUM_FIREARM_SCORE {firearm_score}
#define LAND_PARITY_CHUM_CLOSE_COMBAT_SCORE {close_combat_score}
#define LAND_PARITY_CHUM_MELEE_BASE_DAMAGE {chum_melee_base}
#define LAND_PARITY_CHUM_MELEE_TICKS {chum_melee_ticks}
#define LAND_PARITY_CHUM_PREDATOR_POWER {predator["power"]}
#define LAND_PARITY_CHUM_PREDATOR_RAW_MIN {chum_raw_min}
#define LAND_PARITY_CHUM_PREDATOR_RAW_MAX {chum_raw_min + 3}
#define LAND_PARITY_CHUM_PREDATOR_RANGE {predator["range"]}
#define LAND_PARITY_CHUM_PREDATOR_OPTIMAL_RANGE {predator["optimal_range"]}
#define LAND_PARITY_CHUM_PREDATOR_ACCURACY {predator["accuracy"]}
#define LAND_PARITY_CHUM_PREDATOR_MAGAZINE {predator["magazine"]}
#define LAND_PARITY_CHUM_PREDATOR_NOISE {predator["noise"]}
#define LAND_PARITY_CHUM_PREDATOR_ACTION_COST {predator["action_cost"]}
#define LAND_PARITY_CHUM_RESERVE_AMMO {ammunition["bundle"] * 2}
#define LAND_PARITY_CHUM_LOYALTY 5
#define LAND_PARITY_CHUM_CYBERWARE {cyberware["power"]}
#define LAND_PARITY_CHUM_ALLY_3 {c_string(mara["name"])}
#define LAND_PARITY_CHUM_ENEMY_COUNT {len(chum_rows)}

#define LAND_PARITY_FANTASY_PLAYER_HP {fantasy_hp}
#define LAND_PARITY_FANTASY_PLAYER_DEFENSE {fantasy_defense}
#define LAND_PARITY_FANTASY_PLAYER_ATTACK {fantasy_attack}
#define LAND_PARITY_FANTASY_PLAYER_LEVEL 1
#define LAND_PARITY_FANTASY_STANDARD_POWER {fantasy_standard_power}
#define LAND_PARITY_FANTASY_ALLY_3 {c_string(initial_roster[3]["name"])}
#define LAND_PARITY_FANTASY_ENEMY_COUNT {len(fantasy_rows)}

#define LAND_PARITY_PLEB_PLAYER_HP {pip_hp}
#define LAND_PARITY_PLEB_PLAYER_DEFENSE {pip_defense}
#define LAND_PARITY_PLEB_BASH_BASE {pip_offense + bent_bat["power"]}
#define LAND_PARITY_PLEB_ACCURACY {pip_accuracy}
#define LAND_PARITY_PLEB_EVASION {pip_evasion}
#define LAND_PARITY_PLEB_CRITICAL_BASE {pip_critical}
#define LAND_PARITY_PLEB_ENEMY_COUNT {len(pleb_rows)}

{table("LAND_PARITY_CHUM_ENEMIES", chum_rows)}
{table("LAND_PARITY_FANTASY_ENEMIES", fantasy_rows)}
{table("LAND_PARITY_PLEB_ENEMIES", pleb_rows)}

#endif
"""


def asset_mappings(games_root: Path) -> list[tuple[str, Path, Path]]:
    legend = games_root / "legend-of-kilix/assets/graphics"
    chum = games_root / "chumrunner/assets/graphics"
    fantasy = games_root / "kilix-fantasy/assets/graphics"
    pleb = games_root / "pleb-bound/assets/graphics"
    return [
        ("legend-player", legend / "atlases/player.png",
         ROOT / "assets/graphics/casts/legend-player.png"),
        ("legend-npcs", legend / "atlases/npcs.png",
         ROOT / "assets/graphics/casts/legend-npcs.png"),
        ("legend-portraits", legend / "portraits/dialogue-portraits.png",
         ROOT / "assets/graphics/casts/legend-portraits.png"),
        ("legend-training", legend / "backgrounds/minigames/ember-range.png",
         ROOT / "assets/graphics/training/legend-training.png"),
        ("legend-enemies", legend / "atlases/enemies.png",
         ROOT / "assets/graphics/casts/legend-enemies.png"),
        ("chumrunner-characters", chum / "atlases/characters.png",
         ROOT / "assets/graphics/casts/chumrunner-characters.png"),
        ("chumrunner-portraits", chum / "atlases/portraits.png",
         ROOT / "assets/graphics/casts/chumrunner-portraits.png"),
        ("chumrunner-training", chum / "bitmaps/backdrops/kestrel-lab.png",
         ROOT / "assets/graphics/training/chumrunner-training.png"),
        ("chumrunner-enemies", chum / "atlases/enemies.png",
         ROOT / "assets/graphics/casts/chumrunner-enemies.png"),
        ("fantasy-characters", fantasy / "atlases/characters.png",
         ROOT / "assets/graphics/casts/fantasy-characters.png"),
        ("fantasy-portraits", fantasy / "atlases/portraits.png",
         ROOT / "assets/graphics/casts/fantasy-portraits.png"),
        ("fantasy-training", fantasy / "bitmaps/backdrops/verdant-march.png",
         ROOT / "assets/graphics/training/fantasy-training.png"),
        ("fantasy-enemies", fantasy / "atlases/enemies.png",
         ROOT / "assets/graphics/casts/fantasy-enemies.png"),
        ("pleb-bound-characters", pleb / "atlases/characters.png",
         ROOT / "assets/graphics/casts/pleb-bound-characters.png"),
        ("pleb-bound-portraits", pleb / "atlases/portraits.png",
         ROOT / "assets/graphics/casts/pleb-bound-portraits.png"),
        ("pleb-bound-training", pleb / "bitmaps/backdrops/civic-mall-annex.png",
         ROOT / "assets/graphics/training/pleb-bound-training.png"),
        ("pleb-bound-enemies", pleb / "atlases/enemies.png",
         ROOT / "assets/graphics/casts/pleb-bound-enemies.png"),
    ]


def audio_mappings(games_root: Path) -> list[tuple[Path, Path]]:
    legend = games_root / "legend-of-kilix/assets/audio"
    chum = games_root / "chumrunner/assets/audio/core/noir"
    fantasy = games_root / "kilix-fantasy/assets/audio/core/fantasy"
    pleb = games_root / "pleb-bound/assets/audio/core/suburban"

    def mapped(
        root: Path, cast: str, items: list[tuple[str, str]]
    ) -> list[tuple[Path, Path]]:
        return [
            (root / source, ROOT / "assets/audio" / cast / destination)
            for source, destination in items
        ]

    return (
        mapped(
            legend,
            "legend",
            [
                ("sfx/ui/cursor_move_01.wav", "ui-move.wav"),
                ("sfx/ui/confirm_01.wav", "ui-confirm.wav"),
                ("sfx/dialogue/text_kilix_01.wav", "dialogue.wav"),
                ("sfx/combat/encounter_start_01.wav", "battle-start.wav"),
                ("sfx/combat/laser_launch_01.wav", "ranged-attack.wav"),
                ("sfx/combat/rifle_butt_swing_01.wav", "melee-attack.wav"),
                (
                    "sfx/combat/laser_impact_enemy_01.wav",
                    "ranged-hit.wav",
                ),
                (
                    "sfx/combat/rifle_butt_hit_enemy_01.wav",
                    "melee-hit.wav",
                ),
                ("sfx/minigame/miss_01.wav", "miss.wav"),
                ("sfx/combat/enemy_attack_01.wav", "enemy-attack.wav"),
                ("sfx/combat/kilix_hurt_01.wav", "player-hurt.wav"),
                ("sfx/combat/enemy_defeat_01.wav", "enemy-defeat.wav"),
                ("sfx/combat/kilix_defeat.wav", "player-defeat.wav"),
            ],
        )
        + mapped(
            chum,
            "chumrunner",
            [
                ("interface/cursor_move.wav", "ui-move.wav"),
                ("interface/terminal_confirm.wav", "ui-confirm.wav"),
                ("interface/dialogue_blip.wav", "dialogue.wav"),
                ("combat/combat_start.wav", "battle-start.wav"),
                ("combat/pistol_shot.wav", "ranged-attack.wav"),
                ("combat/melee_swing.wav", "melee-attack.wav"),
                ("combat/armor_hit.wav", "ranged-hit.wav"),
                ("combat/melee_hit.wav", "melee-hit.wav"),
                ("combat/ricochet.wav", "miss.wav"),
                ("combat/rifle_shot.wav", "enemy-attack.wav"),
                ("combat/runner_downed.wav", "player-hurt.wav"),
                ("combat/combat_victory.wav", "enemy-defeat.wav"),
                ("combat/mission_failure.wav", "player-defeat.wav"),
                ("combat/reload.wav", "reload.wav"),
            ],
        )
        + mapped(
            fantasy,
            "fantasy",
            [
                ("interface/cursor_move.wav", "ui-move.wav"),
                ("interface/menu_confirm.wav", "ui-confirm.wav"),
                ("interface/text_blip.wav", "dialogue.wav"),
                ("battle/battle_start.wav", "battle-start.wav"),
                ("battle/attack_swing.wav", "melee-attack.wav"),
                ("battle/attack_hit.wav", "melee-hit.wav"),
                ("battle/attack_miss.wav", "miss.wav"),
                # Kilix Fantasy has an enemy_attack cue on disk that it has
                # never committed. Until it does, Land takes the committed
                # swing its own attacks already use.
                ("battle/attack_swing.wav", "enemy-attack.wav"),
                ("battle/player_hurt.wav", "player-hurt.wav"),
                ("battle/enemy_defeat.wav", "enemy-defeat.wav"),
                ("battle/defeat.wav", "player-defeat.wav"),
            ],
        )
        + mapped(
            pleb,
            "pleb-bound",
            [
                ("interface/cursor_move.wav", "ui-move.wav"),
                ("interface/menu_confirm.wav", "ui-confirm.wav"),
                ("dialogue/text_blip.wav", "dialogue.wav"),
                ("battle/battle_start.wav", "battle-start.wav"),
                ("battle/attack_swing.wav", "melee-attack.wav"),
                ("battle/enemy_hit.wav", "melee-hit.wav"),
                ("battle/attack_miss.wav", "miss.wav"),
                # Same as Kilix Fantasy: Pleb Bound's enemy_attack cue is not
                # in its history yet.
                ("battle/attack_swing.wav", "enemy-attack.wav"),
                ("battle/player_hit.wav", "player-hurt.wav"),
                ("battle/enemy_defeat.wav", "enemy-defeat.wav"),
                ("battle/party_down.wav", "player-defeat.wav"),
            ],
        )
    )


def synchronize(games_root: Path, write: bool) -> list[str]:
    errors: list[str] = []
    header_path = ROOT / "src/source_parity.h"
    expected_header = generated_header(games_root)
    if write:
        header_path.write_text(expected_header, encoding="utf-8")
    elif not header_path.exists() or header_path.read_text(
        encoding="utf-8"
    ) != expected_header:
        errors.append("src/source_parity.h is stale")

    manifest_path = ROOT / "assets/graphics/manifest.json"
    manifest = load_json(manifest_path)
    entries = {item["id"]: item for item in manifest["atlases"]}
    manifest_changed = False
    for asset_id, source, destination in asset_mappings(games_root):
        try:
            source_data = committed_bytes(source)
        except ValueError as error:
            errors.append(f"uncommitted source asset: {source} ({error})")
            continue
        if write:
            if not destination.is_file() or destination.read_bytes() != source_data:
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(source_data)
        elif not destination.is_file() or destination.read_bytes() != source_data:
            errors.append(f"stale copied asset: {destination.relative_to(ROOT)}")
        expected_hash = sha256_bytes(source_data)
        entry = entries.get(asset_id)
        if not entry:
            errors.append(f"manifest is missing {asset_id}")
            continue
        if entry.get("sha256") != expected_hash:
            if write:
                entry["sha256"] = expected_hash
                manifest_changed = True
            else:
                errors.append(f"manifest hash is stale for {asset_id}")
        width, height = png_dimensions(source_data)
        grid = entry.get("grid")
        if not isinstance(grid, dict):
            errors.append(f"manifest grid is missing for {asset_id}")
            continue
        columns = grid.get("columns")
        rows = grid.get("rows")
        if (
            not isinstance(columns, int)
            or not isinstance(rows, int)
            or columns <= 0
            or rows <= 0
            or width % columns != 0
            or height % rows != 0
        ):
            errors.append(f"manifest grid is invalid for {asset_id}")
            continue
        expected_grid = {
            "columns": columns,
            "rows": rows,
            "width": width,
            "height": height,
            "cell_width": width // columns,
            "cell_height": height // rows,
        }
        if grid != expected_grid:
            if write:
                entry["grid"] = expected_grid
                manifest_changed = True
            else:
                errors.append(f"manifest grid is stale for {asset_id}")
    for source, destination in audio_mappings(games_root):
        try:
            source_data = committed_bytes(source)
        except ValueError as error:
            errors.append(f"uncommitted source audio: {source} ({error})")
            continue
        if write:
            if not destination.is_file() or destination.read_bytes() != source_data:
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(source_data)
        elif not destination.is_file() or destination.read_bytes() != source_data:
            errors.append(f"stale copied audio: {destination.relative_to(ROOT)}")
    if write and manifest_changed:
        manifest_path.write_text(
            json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
        )
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--games-root",
        type=Path,
        default=ROOT.parent,
        help="directory containing the four source game repositories",
    )
    parser.add_argument(
        "--write",
        action="store_true",
        help="rewrite the generated contract and copied source assets",
    )
    args = parser.parse_args()
    games_root = args.games_root.resolve()
    try:
        errors = synchronize(games_root, args.write)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"source parity error: {error}", file=sys.stderr)
        return 1
    if errors:
        for error in errors:
            print(f"source parity error: {error}", file=sys.stderr)
        print("run `make parity-sync` to update Kilix Land", file=sys.stderr)
        return 1
    mode = "synchronized" if args.write else "verified"
    print(f"PASS source parity {mode}: games={games_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
