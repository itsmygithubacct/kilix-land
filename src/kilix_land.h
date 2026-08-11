#ifndef KILIX_LAND_H
#define KILIX_LAND_H

#include "kilix_game_kit.h"
#include "kilix_assets.h"
#include "kilix_top_down.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "source_parity.h"

#define LAND_LOGICAL_WIDTH 480
#define LAND_LOGICAL_HEIGHT 270
#define LAND_SIMULATION_HZ 60
#define LAND_TICK_SECONDS (1.0f / 60.0f)
#define LAND_LEGEND_AIM_SPEED 180.0f
#define LAND_ACTOR_COUNT 4
#define LAND_CAST_COUNT 4
#define LAND_GRAPHICS_COUNT 21
#define LAND_AUDIO_EVENT_COUNT 14
#define LAND_AUDIO_SOURCE_CUE_COUNT 49
#define LAND_DIALOGUE_TEXT_CAPACITY 192
#define LAND_HERO_MOTION_VARIANT_COUNT 5
#define LAND_KILIX_MAX_RIFLE_CHARGE \
    LAND_PARITY_LEGEND_MAX_RIFLE_CHARGE
#define LAND_TRAINING_ENEMY_X 382.0f
#define LAND_TRAINING_ENEMY_Y 191.0f
#define LAND_ENEMY_INITIAL_COOLDOWN_TICKS 84
#define LAND_PLAYER_RECOVERY_TICKS \
    LAND_PARITY_LEGEND_KNOCKOUT_TICKS

typedef enum land_mode {
    LAND_MODE_ROOM = 0,
    LAND_MODE_DIALOGUE = 1,
    LAND_MODE_CAST_SELECT = 2
} land_mode;

typedef enum land_location {
    LAND_LOCATION_SOCIAL = 0,
    LAND_LOCATION_TRAINING = 1
} land_location;

typedef enum land_cast {
    LAND_CAST_LEGEND = 0,
    LAND_CAST_CHUMRUNNER = 1,
    LAND_CAST_FANTASY = 2,
    LAND_CAST_PLEB_BOUND = 3
} land_cast;

typedef enum land_actor {
    LAND_ACTOR_HERO = 0,
    LAND_ACTOR_ALLY_1 = 1,
    LAND_ACTOR_ALLY_2 = 2,
    LAND_ACTOR_ALLY_3 = 3
} land_actor;

typedef enum land_facing {
    LAND_FACING_DOWN = 0,
    LAND_FACING_LEFT = 1,
    LAND_FACING_RIGHT = 2,
    LAND_FACING_UP = 3
} land_facing;

typedef enum land_graphic {
    LAND_GRAPHIC_LEGEND_ROOM = 0,
    LAND_GRAPHIC_CHUM_ROOM = 1,
    LAND_GRAPHIC_FANTASY_ROOM = 2,
    LAND_GRAPHIC_PLEB_ROOM = 3,
    LAND_GRAPHIC_LEGEND_PLAYER = 4,
    LAND_GRAPHIC_LEGEND_NPCS = 5,
    LAND_GRAPHIC_LEGEND_PORTRAITS = 6,
    LAND_GRAPHIC_CHUM_CHARACTERS = 7,
    LAND_GRAPHIC_CHUM_PORTRAITS = 8,
    LAND_GRAPHIC_FANTASY_CHARACTERS = 9,
    LAND_GRAPHIC_FANTASY_PORTRAITS = 10,
    LAND_GRAPHIC_PLEB_CHARACTERS = 11,
    LAND_GRAPHIC_PLEB_PORTRAITS = 12,
    LAND_GRAPHIC_LEGEND_TRAINING = 13,
    LAND_GRAPHIC_CHUM_TRAINING = 14,
    LAND_GRAPHIC_FANTASY_TRAINING = 15,
    LAND_GRAPHIC_PLEB_TRAINING = 16,
    LAND_GRAPHIC_LEGEND_ENEMIES = 17,
    LAND_GRAPHIC_CHUM_ENEMIES = 18,
    LAND_GRAPHIC_FANTASY_ENEMIES = 19,
    LAND_GRAPHIC_PLEB_ENEMIES = 20
} land_graphic;

typedef enum land_attack {
    LAND_ATTACK_NONE = 0,
    LAND_ATTACK_KILIX_RIFLE_BUTT = 1,
    LAND_ATTACK_KILIX_LASER = 2,
    LAND_ATTACK_ROOK_PREDATOR = 3,
    LAND_ATTACK_ROOK_PISTOL_BUTT = 4,
    LAND_ATTACK_ARDEN_BRONZE_BLADE = 5,
    LAND_ATTACK_PIP_BENT_BAT = 6
} land_attack;

typedef enum land_enemy_action {
    LAND_ENEMY_ACTION_IDLE = 0,
    LAND_ENEMY_ACTION_ATTACK = 1,
    LAND_ENEMY_ACTION_CHARGE = 2,
    LAND_ENEMY_ACTION_RELOAD = 3
} land_enemy_action;

typedef enum land_audio_event {
    LAND_AUDIO_UI_MOVE = 0,
    LAND_AUDIO_UI_CONFIRM = 1,
    LAND_AUDIO_DIALOGUE = 2,
    LAND_AUDIO_BATTLE_START = 3,
    LAND_AUDIO_RANGED_ATTACK = 4,
    LAND_AUDIO_MELEE_ATTACK = 5,
    LAND_AUDIO_RANGED_HIT = 6,
    LAND_AUDIO_MELEE_HIT = 7,
    LAND_AUDIO_MISS = 8,
    LAND_AUDIO_ENEMY_ATTACK = 9,
    LAND_AUDIO_PLAYER_HURT = 10,
    LAND_AUDIO_ENEMY_DEFEAT = 11,
    LAND_AUDIO_PLAYER_DEFEAT = 12,
    LAND_AUDIO_RELOAD = 13
} land_audio_event;

/*
 * Every cast is drawn through one house layout -- idle-down, the down-walk
 * pair, idle-up, the authored side pair, the action pose, recovery -- which
 * used to be columns 0..7 of every character atlas because all four sheets
 * were authored to that shape.
 *
 * The fantasy cast is now synced from kilix-fantasy's own art, which is
 * 24-column pixel art with its own vocabulary (walk-left-a, kneel-hurt,
 * signature-work, ...). The slots below name what Land actually wants; the
 * mapping turns a slot into that cast's real column, so no call site has to
 * know which sheet it is talking to.
 */
typedef enum land_character_slot {
    LAND_SLOT_IDLE_DOWN = 0,
    LAND_SLOT_WALK_DOWN_A = 1,
    LAND_SLOT_WALK_DOWN_B = 2,
    LAND_SLOT_IDLE_UP = 3,
    LAND_SLOT_SIDE_A = 4,
    LAND_SLOT_SIDE_B = 5,
    LAND_SLOT_ACTION = 6,
    LAND_SLOT_RECOVERY = 7,
    LAND_SLOT_COUNT = 8
} land_character_slot;

static inline int land_character_column(land_cast cast,
                                        land_character_slot slot)
{
    /*
     * Fantasy authors both sides, so the side pair is walk-right-* and the
     * mirrored variants can use the authored walk-left-* rather than a
     * reflection. signature-work is the action pose an RPG cast swings on;
     * kneel-hurt is its recovery frame.
     */
    static const int FANTASY[LAND_SLOT_COUNT] = {0, 4, 5, 3, 10, 11, 21, 19};
    static const int HOUSE[LAND_SLOT_COUNT] = {0, 1, 2, 3, 4, 5, 6, 7};
    if (slot < 0 || slot >= LAND_SLOT_COUNT) return 0;
    return cast == LAND_CAST_FANTASY ? FANTASY[slot] : HOUSE[slot];
}

typedef enum land_hero_motion_variant {
    LAND_HERO_MOTION_DOWN_A = 0,
    LAND_HERO_MOTION_DOWN_B = 1,
    LAND_HERO_MOTION_UP_STEP = 2,
    LAND_HERO_MOTION_MIRRORED_SIDE = 3,
    LAND_HERO_MOTION_MIRRORED_WALK = 4
} land_hero_motion_variant;

typedef struct land_state {
    float player_x;
    float player_y;
    uint64_t simulation_tick;
    uint32_t talked_mask;
    uint32_t rng;
    land_mode mode;
    land_location location;
    land_cast cast;
    land_cast cast_cursor;
    land_facing facing;
    land_facing attack_facing;
    bool player_moving;
    bool cast_confirmed;
    bool aiming;
    int nearest_actor;
    int conversation_actor;
    int dialogue_beat;
    int dialogue_age;
    int toast_ticks;
    land_attack attack;
    int attack_ticks;
    int attack_total_ticks;
    int attack_impact_ticks;
    int attack_cooldown_ticks;
    int attack_damage;
    int attack_hit_chance;
    int attack_hit_roll;
    int attack_variance;
    int attack_critical_chance;
    int attack_critical_roll;
    int player_hp;
    int player_max_hp;
    int player_defense;
    int player_hit_ticks;
    int player_recovery_ticks;
    int enemy_kind;
    int enemy_hp;
    int enemy_max_hp;
    int enemy_defense;
    int enemy_hit_ticks;
    int enemy_respawn_ticks;
    land_enemy_action enemy_action;
    int enemy_attack_ticks;
    int enemy_attack_total_ticks;
    int enemy_attack_impact_ticks;
    int enemy_attack_cooldown_ticks;
    int enemy_attack_damage;
    int enemy_attack_hit_chance;
    int enemy_attack_hit_roll;
    int enemy_attack_variance;
    int enemy_attack_hits_planned;
    int enemy_attack_hits_landed;
    int enemy_round;
    int enemy_charge_level;
    int enemy_magazine;
    int enemy_reserve_ammo;
    int training_generation;
    int training_wins;
    int training_losses;
    int kilix_rifle_charge;
    int rook_magazine;
    int rook_reserve_ammo;
    int rook_reload_ticks;
    int pip_timing_grade;
    bool attack_critical;
    bool attack_landed;
    bool attack_will_hit;
    bool enemy_attack_landed;
    bool enemy_attack_hit;
    bool enemy_attack_critical;
    bool enemy_attack_ranged;
    bool enemy_moving;
    float aim_x;
    float aim_y;
    float attack_target_x;
    float attack_target_y;
    float enemy_x;
    float enemy_y;
    float enemy_attack_target_x;
    float enemy_attack_target_y;
    char toast[96];
} land_state;

typedef struct land_graphics {
    kilix_asset_cache cache;
    kilix_asset_manifest manifest;
    const kilix_asset_image *images[LAND_GRAPHICS_COUNT];
    kilix_asset_atlas atlases[LAND_GRAPHICS_COUNT];
    uint8_t *hero_motion_pixels;
    ki_td_rgba8 hero_motion_cells[
        LAND_CAST_COUNT - 1][LAND_HERO_MOTION_VARIANT_COUNT];
    bool cache_ready;
} land_graphics;

typedef struct land_audio {
    kilix_game_audio runtime;
} land_audio;

void land_game_init(land_state *game);
void land_game_update(land_state *game, int move_x, int move_y, float seconds);
bool land_game_interact(land_state *game);
bool land_game_attack(land_state *game);
bool land_game_reload(land_state *game);
void land_game_set_aiming(land_state *game, bool aiming);
bool land_game_target_locked(const land_state *game);
int land_rook_range(const land_state *game);
int land_rook_hit_chance(const land_state *game);
void land_game_cancel(land_state *game);
void land_game_open_cast_select(land_state *game);
void land_game_move_cast_cursor(land_state *game, int delta);
bool land_game_confirm_cast(land_state *game);
bool land_game_validate(const land_state *game, char *error, size_t error_size);

land_cast land_visible_cast(const land_state *game);
const char *land_cast_name(land_cast cast);
const char *land_cast_subtitle(land_cast cast);
const char *land_cast_room_name(land_cast cast);
const char *land_attack_name(land_attack attack);
const char *land_enemy_name(const land_state *game);
int land_enemy_catalog_count(land_cast cast);
int land_enemy_visual_index(const land_state *game);
float land_enemy_target_x(const land_state *game);
float land_enemy_target_y(const land_state *game);
int land_pip_timing_grade(const land_state *game);
int land_pip_timing_pattern(const land_state *game);
const char *land_actor_name(const land_state *game, int actor);
uint32_t land_actor_color(const land_state *game, int actor);
void land_actor_position(int actor, float *x, float *y);
int land_nearest_actor(const land_state *game);
const char *land_dialogue_text(const land_state *game);
int land_dialogue_speaker(const land_state *game);
int land_dialogue_expression(const land_state *game);
int land_dialogue_count(const land_state *game);
size_t land_dialogue_visible_chars(const land_state *game);

bool land_graphics_init(land_graphics *graphics, const char *asset_root);
void land_graphics_shutdown(land_graphics *graphics);
size_t land_graphics_loaded_count(const land_graphics *graphics);
bool land_graphics_image(const land_graphics *graphics, land_graphic graphic,
                         ki_td_rgba8 *image);
bool land_graphics_cell(const land_graphics *graphics, land_graphic graphic,
                        int column, int row, ki_td_rgba8 *image);
bool land_graphics_hero_motion_cell(
    const land_graphics *graphics, land_cast cast,
    land_hero_motion_variant variant, ki_td_rgba8 *image);

bool land_render(ki_td_soft_renderer *renderer, const land_state *game,
                 const land_graphics *graphics);

bool land_audio_init(land_audio *audio, const char *asset_root, bool live);
void land_audio_play(land_audio *audio, land_cast cast,
                     land_audio_event event);
void land_audio_update(land_audio *audio, float seconds);
void land_audio_shutdown(land_audio *audio);
bool land_audio_assets_selftest(const char *asset_root,
                                size_t *loaded_cues);

#endif
