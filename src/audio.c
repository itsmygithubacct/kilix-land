#include "kilix_land.h"

#include "pleb_plant_grower.h"
#include "pg_audio.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define LAND_AUDIO_CUE(cast, event) \
    ((uint32_t)(cast) * (uint32_t)LAND_AUDIO_EVENT_COUNT + \
     (uint32_t)(event))
#define LAND_AUDIO_CUE_COUNT \
    ((uint32_t)LAND_CAST_COUNT * (uint32_t)LAND_AUDIO_EVENT_COUNT)
/* The conservatory plays the plant game's own sounds through Land's mixer.
 * The plant installs nothing on the mixer; it only queues semantic events
 * for the host to drain. */
#define LAND_PLANT_SFX "third_party/pleb-plant-grower/assets/sfx"
#define LAND_PLANT_BANK_SEED UINT32_C(0x504c4e54)
#define CUE(cast, event, path) \
    {LAND_AUDIO_CUE((cast), (event)), 0u, (path), 1.0f, 1.0f, true}

static const kilix_game_audio_cue_spec AUDIO_CUES[] = {
    CUE(LAND_CAST_LEGEND, LAND_AUDIO_UI_MOVE,
        "assets/audio/legend/ui-move.wav"),
    CUE(LAND_CAST_LEGEND, LAND_AUDIO_UI_CONFIRM,
        "assets/audio/legend/ui-confirm.wav"),
    CUE(LAND_CAST_LEGEND, LAND_AUDIO_DIALOGUE,
        "assets/audio/legend/dialogue.wav"),
    CUE(LAND_CAST_LEGEND, LAND_AUDIO_BATTLE_START,
        "assets/audio/legend/battle-start.wav"),
    CUE(LAND_CAST_LEGEND, LAND_AUDIO_RANGED_ATTACK,
        "assets/audio/legend/ranged-attack.wav"),
    CUE(LAND_CAST_LEGEND, LAND_AUDIO_MELEE_ATTACK,
        "assets/audio/legend/melee-attack.wav"),
    CUE(LAND_CAST_LEGEND, LAND_AUDIO_RANGED_HIT,
        "assets/audio/legend/ranged-hit.wav"),
    CUE(LAND_CAST_LEGEND, LAND_AUDIO_MELEE_HIT,
        "assets/audio/legend/melee-hit.wav"),
    CUE(LAND_CAST_LEGEND, LAND_AUDIO_MISS,
        "assets/audio/legend/miss.wav"),
    CUE(LAND_CAST_LEGEND, LAND_AUDIO_ENEMY_ATTACK,
        "assets/audio/legend/enemy-attack.wav"),
    CUE(LAND_CAST_LEGEND, LAND_AUDIO_PLAYER_HURT,
        "assets/audio/legend/player-hurt.wav"),
    CUE(LAND_CAST_LEGEND, LAND_AUDIO_ENEMY_DEFEAT,
        "assets/audio/legend/enemy-defeat.wav"),
    CUE(LAND_CAST_LEGEND, LAND_AUDIO_PLAYER_DEFEAT,
        "assets/audio/legend/player-defeat.wav"),

    CUE(LAND_CAST_CHUMRUNNER, LAND_AUDIO_UI_MOVE,
        "assets/audio/chumrunner/ui-move.wav"),
    CUE(LAND_CAST_CHUMRUNNER, LAND_AUDIO_UI_CONFIRM,
        "assets/audio/chumrunner/ui-confirm.wav"),
    CUE(LAND_CAST_CHUMRUNNER, LAND_AUDIO_DIALOGUE,
        "assets/audio/chumrunner/dialogue.wav"),
    CUE(LAND_CAST_CHUMRUNNER, LAND_AUDIO_BATTLE_START,
        "assets/audio/chumrunner/battle-start.wav"),
    CUE(LAND_CAST_CHUMRUNNER, LAND_AUDIO_RANGED_ATTACK,
        "assets/audio/chumrunner/ranged-attack.wav"),
    CUE(LAND_CAST_CHUMRUNNER, LAND_AUDIO_MELEE_ATTACK,
        "assets/audio/chumrunner/melee-attack.wav"),
    CUE(LAND_CAST_CHUMRUNNER, LAND_AUDIO_RANGED_HIT,
        "assets/audio/chumrunner/ranged-hit.wav"),
    CUE(LAND_CAST_CHUMRUNNER, LAND_AUDIO_MELEE_HIT,
        "assets/audio/chumrunner/melee-hit.wav"),
    CUE(LAND_CAST_CHUMRUNNER, LAND_AUDIO_MISS,
        "assets/audio/chumrunner/miss.wav"),
    CUE(LAND_CAST_CHUMRUNNER, LAND_AUDIO_ENEMY_ATTACK,
        "assets/audio/chumrunner/enemy-attack.wav"),
    CUE(LAND_CAST_CHUMRUNNER, LAND_AUDIO_PLAYER_HURT,
        "assets/audio/chumrunner/player-hurt.wav"),
    CUE(LAND_CAST_CHUMRUNNER, LAND_AUDIO_ENEMY_DEFEAT,
        "assets/audio/chumrunner/enemy-defeat.wav"),
    CUE(LAND_CAST_CHUMRUNNER, LAND_AUDIO_PLAYER_DEFEAT,
        "assets/audio/chumrunner/player-defeat.wav"),
    CUE(LAND_CAST_CHUMRUNNER, LAND_AUDIO_RELOAD,
        "assets/audio/chumrunner/reload.wav"),

    CUE(LAND_CAST_FANTASY, LAND_AUDIO_UI_MOVE,
        "assets/audio/fantasy/ui-move.wav"),
    CUE(LAND_CAST_FANTASY, LAND_AUDIO_UI_CONFIRM,
        "assets/audio/fantasy/ui-confirm.wav"),
    CUE(LAND_CAST_FANTASY, LAND_AUDIO_DIALOGUE,
        "assets/audio/fantasy/dialogue.wav"),
    CUE(LAND_CAST_FANTASY, LAND_AUDIO_BATTLE_START,
        "assets/audio/fantasy/battle-start.wav"),
    CUE(LAND_CAST_FANTASY, LAND_AUDIO_MELEE_ATTACK,
        "assets/audio/fantasy/melee-attack.wav"),
    CUE(LAND_CAST_FANTASY, LAND_AUDIO_MELEE_HIT,
        "assets/audio/fantasy/melee-hit.wav"),
    CUE(LAND_CAST_FANTASY, LAND_AUDIO_MISS,
        "assets/audio/fantasy/miss.wav"),
    CUE(LAND_CAST_FANTASY, LAND_AUDIO_ENEMY_ATTACK,
        "assets/audio/fantasy/enemy-attack.wav"),
    CUE(LAND_CAST_FANTASY, LAND_AUDIO_PLAYER_HURT,
        "assets/audio/fantasy/player-hurt.wav"),
    CUE(LAND_CAST_FANTASY, LAND_AUDIO_ENEMY_DEFEAT,
        "assets/audio/fantasy/enemy-defeat.wav"),
    CUE(LAND_CAST_FANTASY, LAND_AUDIO_PLAYER_DEFEAT,
        "assets/audio/fantasy/player-defeat.wav"),

    CUE(LAND_CAST_PLEB_BOUND, LAND_AUDIO_UI_MOVE,
        "assets/audio/pleb-bound/ui-move.wav"),
    CUE(LAND_CAST_PLEB_BOUND, LAND_AUDIO_UI_CONFIRM,
        "assets/audio/pleb-bound/ui-confirm.wav"),
    CUE(LAND_CAST_PLEB_BOUND, LAND_AUDIO_DIALOGUE,
        "assets/audio/pleb-bound/dialogue.wav"),
    CUE(LAND_CAST_PLEB_BOUND, LAND_AUDIO_BATTLE_START,
        "assets/audio/pleb-bound/battle-start.wav"),
    CUE(LAND_CAST_PLEB_BOUND, LAND_AUDIO_MELEE_ATTACK,
        "assets/audio/pleb-bound/melee-attack.wav"),
    CUE(LAND_CAST_PLEB_BOUND, LAND_AUDIO_MELEE_HIT,
        "assets/audio/pleb-bound/melee-hit.wav"),
    CUE(LAND_CAST_PLEB_BOUND, LAND_AUDIO_MISS,
        "assets/audio/pleb-bound/miss.wav"),
    CUE(LAND_CAST_PLEB_BOUND, LAND_AUDIO_ENEMY_ATTACK,
        "assets/audio/pleb-bound/enemy-attack.wav"),
    CUE(LAND_CAST_PLEB_BOUND, LAND_AUDIO_PLAYER_HURT,
        "assets/audio/pleb-bound/player-hurt.wav"),
    CUE(LAND_CAST_PLEB_BOUND, LAND_AUDIO_ENEMY_DEFEAT,
        "assets/audio/pleb-bound/enemy-defeat.wav"),
    CUE(LAND_CAST_PLEB_BOUND, LAND_AUDIO_PLAYER_DEFEAT,
        "assets/audio/pleb-bound/player-defeat.wav")
};

_Static_assert(
    sizeof AUDIO_CUES / sizeof AUDIO_CUES[0] ==
        LAND_AUDIO_SOURCE_CUE_COUNT,
    "native audio cue table does not match its public contract");
_Static_assert(
    LAND_AUDIO_PLANT_CUE_COUNT == PG_AUDIO_KIND_COUNT,
    "plant cue count does not match pleb-plant-grower's audio kinds");

/*
 * Plant cue names come from the archive, so they cannot drift from the
 * sounds the plant game ships. They get a bank of their own: the mixer's cue
 * table holds PCMMIX_BANK_CUES_MAX entries and the four casts already number
 * 56 of them. Every plant cue is required, as every cast cue is.
 */
static bool load_plant_cues(land_audio *audio,
                            const kilix_game_data_roots *roots)
{
    uint32_t kind;
    if (!pcmmix_bank_init(&audio->plant_bank,
                          (uint32_t)LAND_AUDIO_PLANT_CUE_COUNT,
                          LAND_PLANT_BANK_SEED))
        return false;
    for (kind = 0u; kind < (uint32_t)LAND_AUDIO_PLANT_CUE_COUNT; ++kind) {
        const char *name = pg_audio_cue_name((uint8_t)kind);
        char relative[96];
        char path[PATH_MAX];
        char error[160];
        int written;
        if (!name) return false;
        written = snprintf(relative, sizeof relative, "%s/%s.wav",
                           LAND_PLANT_SFX, name);
        if (written <= 0 || (size_t)written >= sizeof relative ||
            !kilix_game_data_resolve(roots, relative, path, sizeof path) ||
            !pcmmix_bank_load_wav(&audio->plant_bank, kind, 0u, path,
                                  1.0f, 1.0f, error, sizeof error))
            return false;
        audio->loaded_plant_cues++;
    }
    return true;
}

bool land_audio_init(land_audio *audio, const char *asset_root, bool live)
{
    kilix_game_audio_options options;
    char error[160];
    if (!audio || !asset_root) return false;
    (void)memset(audio, 0, sizeof *audio);
    kilix_game_audio_options_init(&options);
    options.cue_count = LAND_AUDIO_CUE_COUNT;
    options.random_seed = UINT32_C(0x4c414e44);
    options.data.environment_variable = "KILIX_LAND_ASSETS";
    options.data.local_root = asset_root;
    options.cues = AUDIO_CUES;
    options.cue_spec_count =
        sizeof AUDIO_CUES / sizeof AUDIO_CUES[0];
    options.mixer.offline = !live;
    options.require_mixer = !live;
    if (!kilix_game_audio_init(
            &audio->runtime, &options, error, sizeof error))
        return false;
    if (!load_plant_cues(audio, &options.data)) {
        land_audio_shutdown(audio);
        return false;
    }
    return true;
}

void land_audio_play(land_audio *audio, land_cast cast,
                     land_audio_event event)
{
    kilix_game_audio_bus bus;
    if (!audio || cast < LAND_CAST_LEGEND ||
        cast >= LAND_CAST_COUNT ||
        event < LAND_AUDIO_UI_MOVE ||
        event >= LAND_AUDIO_EVENT_COUNT)
        return;
    bus = event <= LAND_AUDIO_DIALOGUE ?
          KILIX_GAME_AUDIO_BUS_UI : KILIX_GAME_AUDIO_BUS_SFX;
    (void)kilix_game_audio_play(
        &audio->runtime, LAND_AUDIO_CUE(cast, event),
        bus, 1.0f, 1.0f);
}

void land_audio_play_plant(land_audio *audio, uint8_t kind, float gain,
                           float pitch)
{
    kilix_game_audio_bus bus;
    if (!audio || kind >= (uint8_t)LAND_AUDIO_PLANT_CUE_COUNT) return;
    bus = kind == (uint8_t)PG_AUDIO_UI_MOVE ||
          kind == (uint8_t)PG_AUDIO_UI_ACCEPT ||
          kind == (uint8_t)PG_AUDIO_UI_REJECT ?
          KILIX_GAME_AUDIO_BUS_UI : KILIX_GAME_AUDIO_BUS_SFX;
    /* kilix_game_audio_play's guard and gain, applied to the plant's bank. */
    if (!kilix_game_audio_is_ready(&audio->runtime) ||
        !audio->runtime.mixer_started)
        return;
    if (!(gain > 0.0f)) return;
    if (gain > 2.0f) gain = 2.0f;
    gain *= kilix_game_audio_bus_gain(&audio->runtime,
                                      KILIX_GAME_AUDIO_BUS_MASTER) *
            kilix_game_audio_bus_gain(&audio->runtime, bus);
    (void)pcmmix_bank_play(&audio->runtime.mixer, &audio->plant_bank, kind,
                           gain, pitch);
}

void land_audio_update(land_audio *audio, float seconds)
{
    if (audio) kilix_game_audio_update(&audio->runtime, seconds);
}

void land_audio_shutdown(land_audio *audio)
{
    if (!audio) return;
    /* The runtime stops the mixer first, so no voice still reads a plant
     * sample when its bank is freed. */
    kilix_game_audio_shutdown(&audio->runtime);
    pcmmix_bank_clear(&audio->plant_bank);
    (void)memset(audio, 0, sizeof *audio);
}

static bool mixes_audible(land_audio *audio)
{
    int16_t output[8192];
    size_t index;
    (void)memset(output, 0, sizeof output);
    pcmmix_mix_block(
        &audio->runtime.mixer, output,
        sizeof output / sizeof output[0]);
    for (index = 0u; index < sizeof output / sizeof output[0]; ++index)
        if (output[index] != 0) return true;
    return false;
}

bool land_audio_assets_selftest(const char *asset_root,
                                size_t *loaded_cues,
                                size_t *loaded_plant_cues)
{
    land_audio audio;
    bool cast_audible;
    bool plant_audible;
    if (loaded_cues) *loaded_cues = 0u;
    if (loaded_plant_cues) *loaded_plant_cues = 0u;
    if (!land_audio_init(&audio, asset_root, false)) return false;
    if (audio.runtime.loaded_cues != LAND_AUDIO_SOURCE_CUE_COUNT ||
        audio.loaded_plant_cues != LAND_AUDIO_PLANT_CUE_COUNT) {
        land_audio_shutdown(&audio);
        return false;
    }
    land_audio_play(
        &audio, LAND_CAST_LEGEND, LAND_AUDIO_MELEE_ATTACK);
    land_audio_play(
        &audio, LAND_CAST_CHUMRUNNER, LAND_AUDIO_RANGED_ATTACK);
    land_audio_play(
        &audio, LAND_CAST_FANTASY, LAND_AUDIO_MELEE_HIT);
    land_audio_play(
        &audio, LAND_CAST_PLEB_BOUND, LAND_AUDIO_PLAYER_HURT);
    cast_audible = mixes_audible(&audio);
    land_audio_shutdown(&audio);
    /* A fresh mixer, so a cast voice still ringing cannot pass for the
     * plant's. */
    if (!land_audio_init(&audio, asset_root, false)) return false;
    land_audio_play_plant(&audio, (uint8_t)PG_AUDIO_WATER, 1.0f, 1.0f);
    plant_audible = mixes_audible(&audio);
    if (loaded_cues) *loaded_cues = LAND_AUDIO_SOURCE_CUE_COUNT;
    if (loaded_plant_cues) *loaded_plant_cues = audio.loaded_plant_cues;
    land_audio_shutdown(&audio);
    return cast_audible && plant_audible;
}

#undef CUE
