#include "kilix_land.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct land_input {
    int move_x;
    int move_y;
    int menu_delta;
    bool interact_pressed;
    bool attack_pressed;
    bool aim_pressed;
    bool reload_pressed;
    bool cancel_pressed;
    bool cast_pressed;
} land_input;

static kittyts_session terminal_session;
static uint32_t last_movement_key;

static bool event_letter(const kittykb_event *event, char lower)
{
    char upper = (char)(lower - 'a' + 'A');
    return kittykb_event_matches_key(event, (uint32_t)(unsigned char)lower) ||
           kittykb_event_matches_key(event, (uint32_t)(unsigned char)upper);
}

static uint32_t normalize_movement_key(uint32_t key)
{
    if (key == (uint32_t)'w' || key == (uint32_t)'W') return KITTYKB_KEY_UP;
    if (key == (uint32_t)'s' || key == (uint32_t)'S') return KITTYKB_KEY_DOWN;
    if (key == (uint32_t)'a' || key == (uint32_t)'A') return KITTYKB_KEY_LEFT;
    if (key == (uint32_t)'d' || key == (uint32_t)'D') return KITTYKB_KEY_RIGHT;
    if (key == KITTYKB_KEY_UP || key == KITTYKB_KEY_DOWN ||
        key == KITTYKB_KEY_LEFT || key == KITTYKB_KEY_RIGHT)
        return key;
    return KITTYKB_KEY_NONE;
}

static bool movement_key_down(uint32_t key)
{
    switch (key) {
    case KITTYKB_KEY_UP:
        return kittyts_key_down(&terminal_session, KITTYKB_KEY_UP) ||
               kittyts_key_down(&terminal_session, (uint32_t)'w');
    case KITTYKB_KEY_DOWN:
        return kittyts_key_down(&terminal_session, KITTYKB_KEY_DOWN) ||
               kittyts_key_down(&terminal_session, (uint32_t)'s');
    case KITTYKB_KEY_LEFT:
        return kittyts_key_down(&terminal_session, KITTYKB_KEY_LEFT) ||
               kittyts_key_down(&terminal_session, (uint32_t)'a');
    case KITTYKB_KEY_RIGHT:
        return kittyts_key_down(&terminal_session, KITTYKB_KEY_RIGHT) ||
               kittyts_key_down(&terminal_session, (uint32_t)'d');
    default:
        return kittyts_key_down(&terminal_session, key);
    }
}

static void apply_movement_key(land_input *input, uint32_t key)
{
    if (!input) return;
    if (key == KITTYKB_KEY_UP) input->move_y = -1;
    else if (key == KITTYKB_KEY_DOWN) input->move_y = 1;
    else if (key == KITTYKB_KEY_LEFT) input->move_x = -1;
    else if (key == KITTYKB_KEY_RIGHT) input->move_x = 1;
}

static bool poll_input(land_input *input, bool *quit_requested)
{
    static const uint32_t directions[] = {
        KITTYKB_KEY_UP, KITTYKB_KEY_DOWN,
        KITTYKB_KEY_LEFT, KITTYKB_KEY_RIGHT
    };
    kittykb_event event;
    size_t index;

    if (!input || !quit_requested) {
        errno = EINVAL;
        return false;
    }
    (void)memset(input, 0, sizeof *input);
    if (kittyts_read_input(&terminal_session) < 0 && errno != EAGAIN &&
        errno != EWOULDBLOCK && errno != EINTR)
        return false;

    while (kittyts_next_key_event(&terminal_session, &event)) {
        uint32_t movement_key = normalize_movement_key(event.key);
        if (movement_key != KITTYKB_KEY_NONE &&
            event.action != KITTYKB_ACTION_RELEASE)
            last_movement_key = movement_key;
        if (event.action != KITTYKB_ACTION_PRESS) continue;
        if (movement_key == KITTYKB_KEY_UP ||
            movement_key == KITTYKB_KEY_LEFT)
            input->menu_delta = -1;
        else if (movement_key == KITTYKB_KEY_DOWN ||
                 movement_key == KITTYKB_KEY_RIGHT)
            input->menu_delta = 1;
        if ((event_letter(&event, 'c') &&
             (event.modifiers & KITTYKB_MOD_CTRL) != 0u) ||
            event_letter(&event, 'q')) {
            *quit_requested = true;
        } else if (event_letter(&event, 'c')) {
            input->cast_pressed = true;
        } else if (event_letter(&event, 'z')) {
            input->attack_pressed = true;
        } else if (event_letter(&event, 'x')) {
            input->aim_pressed = true;
        } else if (event_letter(&event, 'r')) {
            input->reload_pressed = true;
        } else if (event.key == KITTYKB_KEY_ESCAPE) {
            input->cancel_pressed = true;
        } else if (event.key == KITTYKB_KEY_ENTER ||
                   kittykb_event_matches_key(&event, (uint32_t)' ')) {
            input->interact_pressed = true;
        }
    }
    if (last_movement_key != KITTYKB_KEY_NONE &&
        movement_key_down(last_movement_key)) {
        apply_movement_key(input, last_movement_key);
        return true;
    }
    for (index = 0u; index < sizeof directions / sizeof directions[0];
         ++index) {
        if (movement_key_down(directions[index])) {
            last_movement_key = directions[index];
            apply_movement_key(input, directions[index]);
            break;
        }
    }
    return true;
}

static void merge_pending_input(land_input *pending,
                                const land_input *current)
{
    pending->move_x = current->move_x;
    pending->move_y = current->move_y;
    if (current->menu_delta != 0)
        pending->menu_delta = current->menu_delta;
    pending->interact_pressed = pending->interact_pressed ||
                                current->interact_pressed;
    pending->attack_pressed = pending->attack_pressed ||
                              current->attack_pressed;
    pending->aim_pressed = pending->aim_pressed ||
                           current->aim_pressed;
    pending->reload_pressed = pending->reload_pressed ||
                              current->reload_pressed;
    pending->cancel_pressed = pending->cancel_pressed ||
                              current->cancel_pressed;
    pending->cast_pressed = pending->cast_pressed ||
                            current->cast_pressed;
}

static void consume_edge_input(land_input *input)
{
    input->menu_delta = 0;
    input->interact_pressed = false;
    input->attack_pressed = false;
    input->aim_pressed = false;
    input->reload_pressed = false;
    input->cancel_pressed = false;
    input->cast_pressed = false;
}

static const char *asset_root(void)
{
    const char *override = getenv("KILIX_LAND_ASSETS");
    return override && override[0] != '\0' ? override : ".";
}

static bool ranged_attack(land_attack attack)
{
    return attack == LAND_ATTACK_KILIX_LASER ||
           attack == LAND_ATTACK_ROOK_PREDATOR;
}

static void play_attack_start(land_audio *audio,
                              const land_state *game)
{
    if (!audio || !game || game->attack == LAND_ATTACK_NONE) return;
    land_audio_play(
        audio, game->cast,
        ranged_attack(game->attack) ?
        LAND_AUDIO_RANGED_ATTACK : LAND_AUDIO_MELEE_ATTACK);
}

static void play_state_audio(land_audio *audio,
                             const land_state *before,
                             const land_state *after)
{
    if (!audio || !before || !after) return;
    if (before->location == LAND_LOCATION_SOCIAL &&
        after->location == LAND_LOCATION_TRAINING)
        land_audio_play(
            audio, after->cast, LAND_AUDIO_BATTLE_START);
    if (!before->attack_landed && after->attack_landed) {
        if (after->enemy_hp < before->enemy_hp) {
            land_audio_play(
                audio, after->cast,
                ranged_attack(after->attack) ?
                LAND_AUDIO_RANGED_HIT : LAND_AUDIO_MELEE_HIT);
            if (before->enemy_hp > 0 && after->enemy_hp == 0)
                land_audio_play(
                    audio, after->cast, LAND_AUDIO_ENEMY_DEFEAT);
        } else {
            land_audio_play(audio, after->cast, LAND_AUDIO_MISS);
        }
    }
    if (before->enemy_attack_ticks <= 0 &&
        after->enemy_attack_ticks > 0)
        land_audio_play(
            audio, after->cast, LAND_AUDIO_ENEMY_ATTACK);
    if (after->player_hp < before->player_hp) {
        land_audio_play(
            audio, after->cast,
            after->player_hp == 0 ?
            LAND_AUDIO_PLAYER_DEFEAT : LAND_AUDIO_PLAYER_HURT);
    }
}

static int audio_test(void)
{
    size_t loaded = 0u;
    if (!land_audio_assets_selftest(asset_root(), &loaded) ||
        loaded != LAND_AUDIO_SOURCE_CUE_COUNT) {
        (void)fprintf(stderr,
                      "FAIL native audio loaded=%zu/%d\n",
                      loaded, LAND_AUDIO_SOURCE_CUE_COUNT);
        return EXIT_FAILURE;
    }
    (void)printf(
        "PASS native-audio casts=%d source-cues=%zu mixer=offline\n",
        LAND_CAST_COUNT, loaded);
    return EXIT_SUCCESS;
}

static int selftest(void)
{
    static const char *const expected_heroes[LAND_CAST_COUNT] = {
        "KILIX", "ROOK", "ARDEN", "PIP"
    };
    static const char *const expected_third_allies[LAND_CAST_COUNT] = {
        "HARE COURIER", LAND_PARITY_CHUM_ALLY_3,
        LAND_PARITY_FANTASY_ALLY_3, "BERYL"
    };
    static const int expected_enemy_counts[LAND_CAST_COUNT] = {
        18, LAND_PARITY_CHUM_ENEMY_COUNT,
        LAND_PARITY_FANTASY_ENEMY_COUNT,
        LAND_PARITY_PLEB_ENEMY_COUNT
    };
    static const int expected_player_hp[LAND_CAST_COUNT] = {
        6, LAND_PARITY_CHUM_PLAYER_HP,
        LAND_PARITY_FANTASY_PLAYER_HP, LAND_PARITY_PLEB_PLAYER_HP
    };
    static const int expected_player_defense[LAND_CAST_COUNT] = {
        0, LAND_PARITY_CHUM_PLAYER_MITIGATION,
        LAND_PARITY_FANTASY_PLAYER_DEFENSE,
        LAND_PARITY_PLEB_PLAYER_DEFENSE
    };
    land_state game;
    char error[128];
    int guard = 0;
    int cast;
    int actor;
    land_game_init(&game);
    if (!land_game_validate(&game, error, sizeof error) ||
        game.mode != LAND_MODE_CAST_SELECT || game.cast_confirmed) {
        (void)fprintf(stderr, "FAIL initial state: %s\n", error);
        return EXIT_FAILURE;
    }
    land_game_move_cast_cursor(&game, -1);
    if (game.cast_cursor != LAND_CAST_PLEB_BOUND) {
        (void)fprintf(stderr, "FAIL cast selector did not wrap backward\n");
        return EXIT_FAILURE;
    }
    land_game_move_cast_cursor(&game, 1);
    if (!land_game_confirm_cast(&game) || game.mode != LAND_MODE_ROOM ||
        !game.cast_confirmed ||
        strcmp(land_actor_name(&game, LAND_ACTOR_HERO), "KILIX") != 0) {
        (void)fprintf(stderr, "FAIL initial cast did not confirm\n");
        return EXIT_FAILURE;
    }
    game.player_x = 118.0f;
    game.player_y = 220.0f;
    if (land_nearest_actor(&game) != LAND_ACTOR_ALLY_1 ||
        !land_game_interact(&game) || game.mode != LAND_MODE_DIALOGUE) {
        (void)fprintf(stderr, "FAIL dialogue did not start\n");
        return EXIT_FAILURE;
    }
    while (game.mode == LAND_MODE_DIALOGUE && guard++ < 32) {
        (void)land_game_interact(&game);
        (void)land_game_interact(&game);
    }
    if (game.mode != LAND_MODE_ROOM ||
        (game.talked_mask & (UINT32_C(1) << LAND_ACTOR_ALLY_1)) == 0u) {
        (void)fprintf(stderr, "FAIL dialogue did not complete\n");
        return EXIT_FAILURE;
    }
    for (cast = LAND_CAST_CHUMRUNNER; cast < LAND_CAST_COUNT; ++cast) {
        land_game_open_cast_select(&game);
        land_game_move_cast_cursor(&game, 1);
        if (game.cast_cursor != (land_cast)cast ||
            land_visible_cast(&game) != (land_cast)cast ||
            strcmp(land_actor_name(&game, LAND_ACTOR_HERO),
                   expected_heroes[cast]) != 0 ||
            strcmp(land_actor_name(&game, LAND_ACTOR_ALLY_3),
                   expected_third_allies[cast]) != 0 ||
            !land_game_confirm_cast(&game) || game.talked_mask != 0u) {
            (void)fprintf(stderr, "FAIL cast switch index=%d\n", cast);
            return EXIT_FAILURE;
        }
        game.player_x = 118.0f;
        game.player_y = 220.0f;
        if (!land_game_interact(&game) ||
            land_dialogue_text(&game)[0] == '\0') {
            (void)fprintf(stderr, "FAIL cast dialogue index=%d\n", cast);
            return EXIT_FAILURE;
        }
        land_game_cancel(&game);
    }
    for (cast = 0; cast < LAND_CAST_COUNT; ++cast) {
        land_state scripts;
        land_game_init(&scripts);
        scripts.cast_cursor = (land_cast)cast;
        (void)land_game_confirm_cast(&scripts);
        for (actor = LAND_ACTOR_ALLY_1;
             actor <= LAND_ACTOR_ALLY_3; ++actor) {
            float actor_x;
            float actor_y;
            uint32_t bit = UINT32_C(1) << (unsigned int)actor;
            land_actor_position(actor, &actor_x, &actor_y);
            scripts.player_x = actor_x;
            scripts.player_y = actor_y + 36.0f;
            if (!land_game_interact(&scripts) ||
                land_dialogue_count(&scripts) != 5 ||
                land_dialogue_speaker(&scripts) != actor ||
                land_dialogue_text(&scripts)[0] == '\0') {
                (void)fprintf(stderr,
                              "FAIL first dialogue cast=%d actor=%d\n",
                              cast, actor);
                return EXIT_FAILURE;
            }
            land_game_cancel(&scripts);
            scripts.talked_mask |= bit;
            if (!land_game_interact(&scripts) ||
                land_dialogue_count(&scripts) != 2 ||
                land_dialogue_speaker(&scripts) != actor ||
                land_dialogue_text(&scripts)[0] == '\0') {
                (void)fprintf(stderr,
                              "FAIL repeat dialogue cast=%d actor=%d\n",
                              cast, actor);
                return EXIT_FAILURE;
            }
            land_game_cancel(&scripts);
            scripts.talked_mask &= ~bit;
        }
    }
    for (cast = 0; cast < LAND_CAST_COUNT; ++cast) {
        land_state training;
        int hp_before;
        int damage;
        int defense;
        int grade;
        int expected_damage;
        int impact_guard = 0;
        float target_x;
        float target_y;
        land_game_init(&training);
        training.cast_cursor = (land_cast)cast;
        (void)land_game_confirm_cast(&training);
        training.player_x = 459.0f;
        land_game_update(&training, 1, 0, LAND_TICK_SECONDS);
        if (training.location != LAND_LOCATION_TRAINING ||
            training.enemy_kind < 0 ||
            training.enemy_kind >=
                land_enemy_catalog_count((land_cast)cast) ||
            land_enemy_catalog_count((land_cast)cast) !=
                expected_enemy_counts[cast] ||
            training.player_max_hp != expected_player_hp[cast] ||
            training.player_defense != expected_player_defense[cast] ||
            strcmp(land_enemy_name(&training), "NO TARGET") == 0) {
            (void)fprintf(stderr,
                          "FAIL training entry/roster cast=%d\n", cast);
            return EXIT_FAILURE;
        }
        {
            int practice_hp = training.enemy_hp;
            int practice_guard = 0;
            if (!land_game_attack(&training) ||
                training.attack == LAND_ATTACK_NONE ||
                (cast == LAND_CAST_CHUMRUNNER &&
                 training.attack != LAND_ATTACK_ROOK_PISTOL_BUTT) ||
                training.attack_will_hit) {
                (void)fprintf(stderr,
                              "FAIL free melee swing cast=%d\n", cast);
                return EXIT_FAILURE;
            }
            while (!training.attack_landed &&
                   practice_guard++ < 256)
                land_game_update(
                    &training, 0, 0, LAND_TICK_SECONDS);
            if (!training.attack_landed ||
                training.enemy_hp != practice_hp) {
                (void)fprintf(stderr,
                              "FAIL out-of-range swing damaged cast=%d\n",
                              cast);
                return EXIT_FAILURE;
            }
            while (training.attack_ticks > 0)
                land_game_update(
                    &training, 0, 0, LAND_TICK_SECONDS);
            if (cast == LAND_CAST_CHUMRUNNER) {
                int melee_hp = training.enemy_hp;
                int melee_expected =
                    LAND_PARITY_CHUM_MELEE_BASE_DAMAGE +
                    LAND_PARITY_CHUM_CLOSE_COMBAT_SCORE +
                    training.enemy_defense / 2 -
                    training.enemy_defense;
                practice_guard = 0;
                if (melee_expected < 1) melee_expected = 1;
                training.player_x = training.enemy_x - 40.0f;
                training.player_y = training.enemy_y;
                if (!land_game_attack(&training) ||
                    training.attack != LAND_ATTACK_ROOK_PISTOL_BUTT ||
                    !training.attack_will_hit) {
                    (void)fprintf(
                        stderr,
                        "FAIL adjacent Rook pistol-butt start\n");
                    return EXIT_FAILURE;
                }
                while (!training.attack_landed &&
                       practice_guard++ < 256)
                    land_game_update(
                        &training, 0, 0, LAND_TICK_SECONDS);
                if (!training.attack_landed ||
                    melee_hp - training.enemy_hp !=
                        (melee_expected < melee_hp ?
                         melee_expected : melee_hp)) {
                    (void)fprintf(
                        stderr,
                        "FAIL adjacent Rook pistol-butt damage=%d/%d\n",
                        melee_hp - training.enemy_hp,
                        melee_expected);
                    return EXIT_FAILURE;
                }
                while (training.attack_ticks > 0)
                    land_game_update(
                        &training, 0, 0, LAND_TICK_SECONDS);
                training.enemy_hp = training.enemy_max_hp;
                training.enemy_respawn_ticks = 0;
            }
        }
        training.player_x = 320.0f;
        if (cast == LAND_CAST_LEGEND ||
            cast == LAND_CAST_CHUMRUNNER) {
            float planted_x = training.player_x;
            float cursor_x;
            float cursor_y;
            land_game_set_aiming(&training, true);
            cursor_x = training.aim_x;
            cursor_y = training.aim_y;
            land_game_update(&training, 1, -1, LAND_TICK_SECONDS);
            if (!training.aiming || training.player_x != planted_x ||
                training.player_moving ||
                training.aim_x <= cursor_x ||
                training.aim_y >= cursor_y) {
                (void)fprintf(stderr,
                              "FAIL 2D planted aim cast=%d\n", cast);
                return EXIT_FAILURE;
            }
            training.aim_x = land_enemy_target_x(&training);
            training.aim_y = land_enemy_target_y(&training);
            if (!land_game_target_locked(&training)) {
                (void)fprintf(stderr,
                              "FAIL target lock cast=%d\n", cast);
                return EXIT_FAILURE;
            }
        }
        hp_before = training.enemy_hp;
        defense = training.enemy_defense;
        grade = land_pip_timing_grade(&training);
        target_x = land_enemy_target_x(&training);
        target_y = land_enemy_target_y(&training);
        training.rng = UINT32_C(1);
        if (!land_game_attack(&training) ||
            training.attack == LAND_ATTACK_NONE) {
            (void)fprintf(stderr,
                          "FAIL source attack did not start cast=%d\n", cast);
            return EXIT_FAILURE;
        }
        while (!training.attack_landed && impact_guard++ < 256)
            land_game_update(&training, 0, 0, LAND_TICK_SECONDS);
        damage = hp_before - training.enemy_hp;
        expected_damage = 1;
        if (cast == LAND_CAST_CHUMRUNNER)
            expected_damage =
                LAND_PARITY_CHUM_PREDATOR_RAW_MIN + 1 - defense;
        else if (cast == LAND_CAST_FANTASY) {
            expected_damage =
                LAND_PARITY_FANTASY_STANDARD_POWER +
                LAND_PARITY_FANTASY_PLAYER_LEVEL * 2 +
                LAND_PARITY_FANTASY_PLAYER_ATTACK - defense;
            if (expected_damage < 1) expected_damage = 1;
            expected_damage = expected_damage * 95 / 100;
            if (expected_damage < 1) expected_damage = 1;
        } else if (cast == LAND_CAST_PLEB_BOUND) {
            expected_damage =
                LAND_PARITY_PLEB_BASH_BASE + grade * 4 -
                defense + 2;
            if (expected_damage < 1) expected_damage = 1;
            if (grade == 2)
                expected_damage = expected_damage * 3 / 2;
        }
        if (!training.attack_landed ||
            ((cast == LAND_CAST_LEGEND ||
             cast == LAND_CAST_CHUMRUNNER) &&
             (training.attack_target_x != target_x ||
              training.attack_target_y != target_y)) ||
            (cast == LAND_CAST_LEGEND &&
             (training.attack_damage != 1 ||
              training.kilix_rifle_charge !=
                  LAND_KILIX_MAX_RIFLE_CHARGE - 1)) ||
            (cast == LAND_CAST_CHUMRUNNER &&
             (training.attack_hit_chance != 95 ||
              training.attack_hit_roll != 70 ||
              training.rook_magazine !=
                  LAND_PARITY_CHUM_PREDATOR_MAGAZINE - 1)) ||
            (cast == LAND_CAST_FANTASY &&
             (training.attack_variance != -5 ||
              training.attack_critical)) ||
            (cast == LAND_CAST_PLEB_BOUND &&
             (training.attack_hit_roll != 69 ||
              training.attack_variance != 2)) ||
            training.attack_damage != expected_damage ||
            damage != (training.attack_damage < hp_before ?
                       training.attack_damage : hp_before)) {
            (void)fprintf(stderr,
                          "FAIL source attack parity cast=%d damage=%d\n",
                          cast, training.attack_damage);
            return EXIT_FAILURE;
        }
        while (training.attack_ticks > 0)
            land_game_update(&training, 0, 0, LAND_TICK_SECONDS);
        if (cast == LAND_CAST_LEGEND ||
            cast == LAND_CAST_CHUMRUNNER) {
            if (!training.aiming) {
                (void)fprintf(stderr,
                              "FAIL aim mode did not persist cast=%d\n",
                              cast);
                return EXIT_FAILURE;
            }
            land_game_cancel(&training);
            if (training.aiming ||
                training.location != LAND_LOCATION_TRAINING) {
                (void)fprintf(stderr,
                              "FAIL aim cancel left the range cast=%d\n",
                              cast);
                return EXIT_FAILURE;
            }
            if (cast == LAND_CAST_CHUMRUNNER) {
                if (!land_game_reload(&training) ||
                    training.rook_magazine !=
                        LAND_PARITY_CHUM_PREDATOR_MAGAZINE ||
                    training.rook_reserve_ammo !=
                        LAND_PARITY_CHUM_RESERVE_AMMO - 1 ||
                    training.rook_reload_ticks != 30 ||
                    training.aiming) {
                    (void)fprintf(stderr,
                                  "FAIL source reload parity\n");
                    return EXIT_FAILURE;
                }
                training.rook_reload_ticks = 0;
                training.attack_cooldown_ticks = 0;
            }
        }
        {
            int player_hp_before;
            int enemy_guard = 0;
            training.player_hp = training.player_max_hp;
            training.player_hit_ticks = 0;
            training.player_recovery_ticks = 0;
            training.enemy_hp = training.enemy_max_hp;
            training.enemy_respawn_ticks = 0;
            training.enemy_attack_cooldown_ticks = 0;
            training.enemy_action = LAND_ENEMY_ACTION_IDLE;
            training.enemy_attack_ticks = 0;
            training.enemy_attack_total_ticks = 0;
            training.enemy_attack_impact_ticks = 0;
            training.enemy_attack_damage = 0;
            training.enemy_attack_landed = false;
            training.enemy_attack_hit = false;
            training.player_x = 320.0f;
            training.player_y = training.enemy_y;
            player_hp_before = training.player_hp;
            while (training.player_hp == player_hp_before &&
                   enemy_guard++ < 1000)
                land_game_update(
                    &training, 0, 0, LAND_TICK_SECONDS);
            if (training.player_hp >= player_hp_before ||
                !training.enemy_attack_landed ||
                !training.enemy_attack_hit ||
                player_hp_before - training.player_hp !=
                    training.enemy_attack_damage) {
                (void)fprintf(
                    stderr,
                    "FAIL active enemy attack cast=%d hp=%d/%d damage=%d\n",
                    cast, training.player_hp, player_hp_before,
                    training.enemy_attack_damage);
                return EXIT_FAILURE;
            }
        }
        {
            float knocked_x;
            int recovery_guard = 0;
            int attack_guard = 0;
            training.player_hp = 1;
            training.player_hit_ticks = 0;
            training.player_recovery_ticks = 0;
            training.enemy_hp = training.enemy_max_hp;
            training.enemy_respawn_ticks = 0;
            training.enemy_attack_cooldown_ticks = 0;
            training.enemy_action = LAND_ENEMY_ACTION_IDLE;
            training.enemy_attack_ticks = 0;
            training.enemy_attack_total_ticks = 0;
            training.enemy_attack_impact_ticks = 0;
            training.enemy_attack_damage = 0;
            training.enemy_attack_landed = false;
            training.enemy_attack_hit = false;
            training.player_x = 320.0f;
            training.player_y = training.enemy_y;
            while (training.player_hp > 0 && attack_guard++ < 1000)
                land_game_update(
                    &training, 0, 0, LAND_TICK_SECONDS);
            knocked_x = training.player_x;
            if (training.player_hp != 0 ||
                training.player_recovery_ticks <= 0 ||
                land_game_attack(&training)) {
                (void)fprintf(stderr,
                              "FAIL player knockout state cast=%d\n", cast);
                return EXIT_FAILURE;
            }
            land_game_update(&training, 1, 0, LAND_TICK_SECONDS);
            if (training.player_x != knocked_x ||
                training.player_moving ||
                training.player_recovery_ticks <= 0) {
                (void)fprintf(stderr,
                              "FAIL knockout did not hold cast=%d\n", cast);
                return EXIT_FAILURE;
            }
            /* Leaving a knockout must cancel its delayed training reset. */
            for (int transition = 0; transition < 3; ++transition) {
                land_state escaped = training;
                land_state returned;
                float room_x;
                float room_y;
                if (transition == 0) {
                    land_game_cancel(&escaped);
                } else {
                    land_game_open_cast_select(&escaped);
                    if (transition == 2)
                        land_game_move_cast_cursor(&escaped, 1);
                    if (!land_game_confirm_cast(&escaped)) {
                        (void)fprintf(stderr,
                            "FAIL knockout cast confirmation cast=%d\n", cast);
                        return EXIT_FAILURE;
                    }
                }
                returned = escaped;
                room_x = escaped.player_x;
                room_y = escaped.player_y;
                for (int tick = 0; tick <= LAND_PLAYER_RECOVERY_TICKS; ++tick)
                    land_game_update(&escaped, 0, 0, LAND_TICK_SECONDS);
                if (escaped.location != LAND_LOCATION_SOCIAL ||
                    escaped.player_x != room_x || escaped.player_y != room_y ||
                    escaped.player_hp != escaped.player_max_hp ||
                    escaped.player_hit_ticks != 0 ||
                    escaped.player_recovery_ticks != 0 ||
                    !land_game_validate(&escaped, error, sizeof error)) {
                    (void)fprintf(stderr,
                        "FAIL knockout room return cast=%d transition=%d\n",
                        cast, transition);
                    return EXIT_FAILURE;
                }
                /* Also re-enter immediately, before the old timer expires. */
                escaped = returned;
                escaped.player_x = 459.0f;
                land_game_update(&escaped, 1, 0, LAND_TICK_SECONDS);
                if (escaped.location != LAND_LOCATION_TRAINING ||
                    escaped.player_recovery_ticks != 0 ||
                    !land_game_attack(&escaped)) {
                    (void)fprintf(stderr,
                        "FAIL knockout range re-entry cast=%d transition=%d\n",
                        cast, transition);
                    return EXIT_FAILURE;
                }
            }
            while (training.player_recovery_ticks > 0 &&
                   recovery_guard++ <= LAND_PLAYER_RECOVERY_TICKS)
                land_game_update(
                    &training, 0, 0, LAND_TICK_SECONDS);
            if (training.player_recovery_ticks != 0 ||
                training.player_hp != training.player_max_hp) {
                (void)fprintf(stderr,
                              "FAIL knockout recovery cast=%d\n", cast);
                return EXIT_FAILURE;
            }
        }
        training.player_x = 21.0f;
        land_game_update(&training, -1, 0, LAND_TICK_SECONDS);
        if (training.location != LAND_LOCATION_SOCIAL ||
            training.enemy_kind != -1) {
            (void)fprintf(stderr,
                          "FAIL training exit cast=%d\n", cast);
            return EXIT_FAILURE;
        }
    }
    game.player_x = 100.0f;
    game.player_y = 220.0f;
    land_game_update(&game, 1, 0, LAND_TICK_SECONDS);
    if (!game.player_moving || game.facing != LAND_FACING_RIGHT ||
        game.simulation_tick != 1u || game.player_x <= 102.0f) {
        (void)fprintf(stderr, "FAIL fixed-step movement did not begin\n");
        return EXIT_FAILURE;
    }
    for (guard = 0; guard < 4; ++guard)
        land_game_update(&game, 1, 0, LAND_TICK_SECONDS);
    if (!game.player_moving || game.simulation_tick != 5u) {
        (void)fprintf(stderr, "FAIL walk state did not remain active\n");
        return EXIT_FAILURE;
    }
    land_game_update(&game, 0, 0, LAND_TICK_SECONDS);
    if (game.player_moving || game.simulation_tick != 6u) {
        (void)fprintf(stderr, "FAIL walk state did not return to idle\n");
        return EXIT_FAILURE;
    }
    for (guard = 0; guard < 200; ++guard)
        land_game_update(&game, 1, 0, LAND_TICK_SECONDS);
    for (guard = 0; guard < 200; ++guard)
        land_game_update(&game, 0, -1, LAND_TICK_SECONDS);
    if (!land_game_validate(&game, error, sizeof error)) {
        (void)fprintf(stderr, "FAIL bounded movement: %s\n", error);
        return EXIT_FAILURE;
    }
    (void)printf(
        "PASS selftest casts=%d actors-per-cast=%d dialogue-scripts=24 "
        "training-rosters=4 source-attacks=4 active-enemies=4 "
        "bounds=two-room\n",
        LAND_CAST_COUNT, LAND_ACTOR_COUNT);
    return EXIT_SUCCESS;
}

/*
 * Fantasy ships authored art for both sides, so its "mirrored" motion cells
 * are copies rather than reflections and have to be compared as such.
 */
static bool cells_match(const ki_td_rgba8 *a, const ki_td_rgba8 *b)
{
    int y;
    if (!ki_td_rgba8_is_valid(a) || !ki_td_rgba8_is_valid(b) ||
        a->width != b->width || a->height != b->height)
        return false;
    for (y = 0; y < a->height; ++y)
        if (memcmp(a->pixels + (size_t)y * a->stride,
                   b->pixels + (size_t)y * b->stride,
                   (size_t)a->width * 4u) != 0)
            return false;
    return true;
}

static bool mirror_matches(const ki_td_rgba8 *source,
                           const ki_td_rgba8 *mirror)
{
    int y;
    if (!ki_td_rgba8_is_valid(source) || !ki_td_rgba8_is_valid(mirror) ||
        source->width != mirror->width || source->height != mirror->height)
        return false;
    for (y = 0; y < source->height; ++y) {
        int x;
        for (x = 0; x < source->width; ++x) {
            const uint8_t *left =
                source->pixels + (size_t)y * source->stride +
                (size_t)(source->width - 1 - x) * 4u;
            const uint8_t *right =
                mirror->pixels + (size_t)y * mirror->stride +
                (size_t)x * 4u;
            if (memcmp(left, right, 4u) != 0) return false;
        }
    }
    return true;
}


static int graphics_test(void)
{
    static const land_graphic room_graphics[LAND_CAST_COUNT] = {
        LAND_GRAPHIC_LEGEND_ROOM,
        LAND_GRAPHIC_CHUM_ROOM,
        LAND_GRAPHIC_FANTASY_ROOM,
        LAND_GRAPHIC_PLEB_ROOM
    };
    static const land_graphic character_graphics[LAND_CAST_COUNT - 1] = {
        LAND_GRAPHIC_CHUM_CHARACTERS,
        LAND_GRAPHIC_FANTASY_CHARACTERS,
        LAND_GRAPHIC_PLEB_CHARACTERS
    };
    static const land_graphic portrait_graphics[LAND_CAST_COUNT - 1] = {
        LAND_GRAPHIC_CHUM_PORTRAITS,
        LAND_GRAPHIC_FANTASY_PORTRAITS,
        LAND_GRAPHIC_PLEB_PORTRAITS
    };
    static const land_graphic training_graphics[LAND_CAST_COUNT] = {
        LAND_GRAPHIC_LEGEND_TRAINING,
        LAND_GRAPHIC_CHUM_TRAINING,
        LAND_GRAPHIC_FANTASY_TRAINING,
        LAND_GRAPHIC_PLEB_TRAINING
    };
    static const land_graphic enemy_graphics[LAND_CAST_COUNT] = {
        LAND_GRAPHIC_LEGEND_ENEMIES,
        LAND_GRAPHIC_CHUM_ENEMIES,
        LAND_GRAPHIC_FANTASY_ENEMIES,
        LAND_GRAPHIC_PLEB_ENEMIES
    };
    land_graphics graphics;
    ki_td_rgba8 image;
    ki_td_rgba8 source;
    ki_td_rgba8 authored;
    ki_td_rgba8 mirror;
    bool result = land_graphics_init(&graphics, asset_root());
    size_t loaded = land_graphics_loaded_count(&graphics);
    int cast_index;
    if (!result || loaded != (size_t)LAND_GRAPHICS_COUNT ||
        !land_graphics_cell(&graphics, LAND_GRAPHIC_LEGEND_PLAYER,
                            15, 7, &image) ||
        image.width != 64 || image.height != 64 ||
        !land_graphics_cell(&graphics, LAND_GRAPHIC_LEGEND_NPCS,
                            3, 3, &image) ||
        image.width != 64 || image.height != 64 ||
        !land_graphics_cell(&graphics, LAND_GRAPHIC_LEGEND_PORTRAITS,
                            3, 2, &image) ||
        image.width != 96 || image.height != 96) {
        (void)fprintf(stderr, "FAIL graphics loaded=%zu/%d\n",
                      loaded, LAND_GRAPHICS_COUNT);
        land_graphics_shutdown(&graphics);
        return EXIT_FAILURE;
    }
    for (cast_index = 0; cast_index < LAND_CAST_COUNT; ++cast_index) {
        if (!land_graphics_image(&graphics, room_graphics[cast_index],
                                 &image) ||
            image.width != 960 || image.height != 540) {
            (void)fprintf(stderr,
                          "FAIL room graphics cast=%d size=%dx%d\n",
                          cast_index, image.width, image.height);
            land_graphics_shutdown(&graphics);
            return EXIT_FAILURE;
        }
        if (!land_graphics_image(&graphics,
                                 training_graphics[cast_index], &image) ||
            image.width <= 0 || image.height <= 0 ||
            !land_graphics_cell(&graphics, enemy_graphics[cast_index],
                                0, 0, &image) ||
            (cast_index == LAND_CAST_LEGEND &&
             (image.width != 64 || image.height != 64)) ||
            (cast_index == LAND_CAST_CHUMRUNNER &&
             (image.width != 222 || image.height != 222)) ||
            (cast_index == LAND_CAST_FANTASY &&
             (image.width != 96 || image.height != 112)) ||
            (cast_index == LAND_CAST_PLEB_BOUND &&
             (image.width != 222 || image.height != 222))) {
            (void)fprintf(stderr,
                          "FAIL training graphics cast=%d size=%dx%d\n",
                          cast_index, image.width, image.height);
            land_graphics_shutdown(&graphics);
            return EXIT_FAILURE;
        }
    }
    for (cast_index = 0; cast_index < LAND_CAST_COUNT - 1; ++cast_index) {
        land_cast cast = (land_cast)(cast_index + 1);
        bool fantasy = cast == LAND_CAST_FANTASY;
        /* Fantasy is pixel art synced from kilix-fantasy: 16x24 characters
         * and 40x40 portraits, against the 222/444 the other sheets use. */
        int character_cell = fantasy ? 16 : 222;
        int character_rows = fantasy ? 24 : 222;
        int portrait_cell = fantasy ? 40 : 444;
        if (!land_graphics_cell(&graphics, character_graphics[cast_index],
                                7, 3, &image) ||
            image.width != character_cell ||
            image.height != character_rows ||
            !land_graphics_cell(&graphics, portrait_graphics[cast_index],
                                3, 1, &image) ||
            image.width != portrait_cell ||
            image.height != portrait_cell ||
            !land_graphics_cell(
                &graphics, character_graphics[cast_index],
                land_character_column(cast, LAND_SLOT_SIDE_A), 0,
                &source) ||
            !land_graphics_hero_motion_cell(
                &graphics, cast, LAND_HERO_MOTION_MIRRORED_SIDE, &mirror) ||
            (fantasy ?
                (!land_graphics_cell(&graphics,
                                     character_graphics[cast_index],
                                     7, 0, &authored) ||
                 !cells_match(&authored, &mirror)) :
                !mirror_matches(&source, &mirror))) {
            (void)fprintf(stderr,
                          "FAIL cast graphics index=%d mirror=runtime\n",
                          cast_index + 1);
            land_graphics_shutdown(&graphics);
            return EXIT_FAILURE;
        }
    }
    land_graphics_shutdown(&graphics);
    (void)printf(
        "PASS graphics resources=%zu backgrounds=8 cast-atlases=13 "
        "runtime-motion=validated\n", loaded);
    return EXIT_SUCCESS;
}

typedef enum render_test_scene {
    RENDER_TEST_MENU = 0,
    RENDER_TEST_ROOM = 1,
    RENDER_TEST_DIALOGUE = 2
} render_test_scene;

static int render_test(const char *path, render_test_scene scene)
{
    land_graphics graphics;
    ki_td_soft_renderer renderer = {0};
    land_state game;
    int tick;
    bool success;
    if (!path || !land_graphics_init(&graphics, asset_root()))
        return EXIT_FAILURE;
    if (!ki_td_soft_renderer_init(&renderer, 960, 540)) {
        land_graphics_shutdown(&graphics);
        return EXIT_FAILURE;
    }
    land_game_init(&game);
    if (scene != RENDER_TEST_MENU)
        (void)land_game_confirm_cast(&game);
    if (scene == RENDER_TEST_DIALOGUE) {
        game.player_x = 240.0f;
        game.player_y = 224.0f;
        (void)land_game_interact(&game);
        for (tick = 0; tick < 92; ++tick)
            land_game_update(&game, 0, 0, LAND_TICK_SECONDS);
    } else {
        for (tick = 0; tick < 24; ++tick)
            land_game_update(&game, 0, 0, LAND_TICK_SECONDS);
    }
    success = land_render(&renderer, &game, &graphics) &&
              sr_write_ppm(ki_td_soft_canvas(&renderer), path);
    ki_td_soft_renderer_destroy(&renderer);
    land_graphics_shutdown(&graphics);
    if (!success) return EXIT_FAILURE;
    (void)printf("PASS render scene=%s size=960x540 file=%s\n",
                 scene == RENDER_TEST_MENU ? "cast-menu" :
                 (scene == RENDER_TEST_DIALOGUE ? "dialogue" : "room"),
                 path);
    return EXIT_SUCCESS;
}

static int cast_render_test(const char *directory)
{
    static const char *const cast_ids[LAND_CAST_COUNT] = {
        "legend", "chumrunner", "fantasy", "pleb-bound"
    };
    land_graphics graphics;
    ki_td_soft_renderer renderer = {0};
    bool success = true;
    int cast;
    if (!directory || !land_graphics_init(&graphics, asset_root()))
        return EXIT_FAILURE;
    if (!ki_td_soft_renderer_init(&renderer, 960, 540)) {
        land_graphics_shutdown(&graphics);
        return EXIT_FAILURE;
    }
    for (cast = 0; cast < LAND_CAST_COUNT && success; ++cast) {
        land_state game;
        char room_path[1024];
        char dialogue_path[1024];
        int room_written;
        int dialogue_written;
        int tick;
        land_game_init(&game);
        game.cast_cursor = (land_cast)cast;
        (void)land_game_confirm_cast(&game);
        game.toast_ticks = 0;
        for (tick = 0; tick < 24; ++tick)
            land_game_update(&game, 0, 0, LAND_TICK_SECONDS);
        room_written = snprintf(room_path, sizeof room_path,
                                "%s/%s-room.ppm", directory,
                                cast_ids[cast]);
        if (room_written < 0 || (size_t)room_written >= sizeof room_path ||
            !land_render(&renderer, &game, &graphics) ||
            !sr_write_ppm(ki_td_soft_canvas(&renderer), room_path)) {
            success = false;
            break;
        }
        game.player_x = 118.0f;
        game.player_y = 220.0f;
        (void)land_game_interact(&game);
        for (tick = 0; tick < 240; ++tick)
            land_game_update(&game, 0, 0, LAND_TICK_SECONDS);
        dialogue_written = snprintf(dialogue_path, sizeof dialogue_path,
                                    "%s/%s-dialogue.ppm", directory,
                                    cast_ids[cast]);
        if (dialogue_written < 0 ||
            (size_t)dialogue_written >= sizeof dialogue_path ||
            !land_render(&renderer, &game, &graphics) ||
            !sr_write_ppm(ki_td_soft_canvas(&renderer), dialogue_path))
            success = false;
    }
    ki_td_soft_renderer_destroy(&renderer);
    land_graphics_shutdown(&graphics);
    if (!success) return EXIT_FAILURE;
    (void)printf(
        "PASS render scene=cast-room-and-dialogue casts=4 files=8 "
        "size=960x540 directory=%s\n", directory);
    return EXIT_SUCCESS;
}

static int training_render_test(const char *directory)
{
    static const char *const cast_ids[LAND_CAST_COUNT] = {
        "legend", "chumrunner", "fantasy", "pleb-bound"
    };
    land_graphics graphics;
    ki_td_soft_renderer renderer = {0};
    bool success = true;
    int cast;
    if (!directory || !land_graphics_init(&graphics, asset_root()))
        return EXIT_FAILURE;
    if (!ki_td_soft_renderer_init(&renderer, 960, 540)) {
        land_graphics_shutdown(&graphics);
        return EXIT_FAILURE;
    }
    for (cast = 0; cast < LAND_CAST_COUNT && success; ++cast) {
        land_state game;
        char path[1024];
        int written;
        int tick;
        land_game_init(&game);
        game.cast_cursor = (land_cast)cast;
        (void)land_game_confirm_cast(&game);
        game.player_x = 459.0f;
        land_game_update(&game, 1, 0, LAND_TICK_SECONDS);
        game.player_x = 314.0f;
        game.player_y = 226.0f;
        game.toast_ticks = 0;
        if (cast == LAND_CAST_LEGEND ||
            cast == LAND_CAST_CHUMRUNNER) {
            land_game_set_aiming(&game, true);
            game.aim_x = land_enemy_target_x(&game);
            game.aim_y = land_enemy_target_y(&game);
            written = snprintf(path, sizeof path, "%s/%s-aim.ppm",
                               directory, cast_ids[cast]);
            if (written < 0 || (size_t)written >= sizeof path ||
                !land_render(&renderer, &game, &graphics) ||
                !sr_write_ppm(ki_td_soft_canvas(&renderer), path)) {
                success = false;
                break;
            }
        }
        (void)land_game_attack(&game);
        if (cast == LAND_CAST_LEGEND ||
            cast == LAND_CAST_CHUMRUNNER) {
            land_game_update(&game, 0, 0, LAND_TICK_SECONDS);
            written = snprintf(path, sizeof path, "%s/%s-recoil.ppm",
                               directory, cast_ids[cast]);
            if (written < 0 || (size_t)written >= sizeof path ||
                !land_render(&renderer, &game, &graphics) ||
                !sr_write_ppm(ki_td_soft_canvas(&renderer), path)) {
                success = false;
                break;
            }
            tick = 1;
        } else {
            tick = 0;
        }
        for (; tick < 5; ++tick)
            land_game_update(&game, 0, 0, LAND_TICK_SECONDS);
        written = snprintf(path, sizeof path, "%s/%s-training.ppm",
                           directory, cast_ids[cast]);
        if (written < 0 || (size_t)written >= sizeof path ||
            !land_render(&renderer, &game, &graphics) ||
            !sr_write_ppm(ki_td_soft_canvas(&renderer), path))
            success = false;

        if (success) {
            land_game_init(&game);
            game.cast_cursor = (land_cast)cast;
            (void)land_game_confirm_cast(&game);
            game.player_x = 459.0f;
            land_game_update(&game, 1, 0, LAND_TICK_SECONDS);
            game.player_x = 230.0f;
            game.player_y = 226.0f;
            game.toast_ticks = 0;
            (void)land_game_attack(&game);
            for (tick = 0; tick < 5; ++tick)
                land_game_update(&game, 0, 0, LAND_TICK_SECONDS);
            written = snprintf(path, sizeof path,
                               "%s/%s-free-swing.ppm",
                               directory, cast_ids[cast]);
            if (written < 0 || (size_t)written >= sizeof path ||
                !land_render(&renderer, &game, &graphics) ||
                !sr_write_ppm(ki_td_soft_canvas(&renderer), path))
                success = false;
        }

        if (success) {
            int player_hp_before;
            int guard = 0;
            land_game_init(&game);
            game.cast_cursor = (land_cast)cast;
            (void)land_game_confirm_cast(&game);
            game.player_x = 459.0f;
            land_game_update(&game, 1, 0, LAND_TICK_SECONDS);
            game.player_x = 320.0f;
            game.player_y = game.enemy_y;
            game.toast_ticks = 0;
            game.enemy_attack_cooldown_ticks = 0;
            land_game_update(&game, 0, 0, LAND_TICK_SECONDS);
            written = snprintf(path, sizeof path,
                               "%s/%s-enemy-warning.ppm",
                               directory, cast_ids[cast]);
            if (written < 0 || (size_t)written >= sizeof path ||
                !land_render(&renderer, &game, &graphics) ||
                !sr_write_ppm(ki_td_soft_canvas(&renderer), path)) {
                success = false;
                break;
            }
            player_hp_before = game.player_hp;
            while (game.player_hp == player_hp_before && guard++ < 1000)
                land_game_update(&game, 0, 0, LAND_TICK_SECONDS);
            written = snprintf(path, sizeof path,
                               "%s/%s-enemy-hit.ppm",
                               directory, cast_ids[cast]);
            if (game.player_hp >= player_hp_before ||
                written < 0 || (size_t)written >= sizeof path ||
                !land_render(&renderer, &game, &graphics) ||
                !sr_write_ppm(ki_td_soft_canvas(&renderer), path)) {
                success = false;
                break;
            }
            game.player_hp = 0;
            game.player_hit_ticks = 10;
            game.player_recovery_ticks = LAND_PLAYER_RECOVERY_TICKS;
            game.enemy_action = LAND_ENEMY_ACTION_IDLE;
            game.enemy_attack_ticks = 0;
            game.enemy_attack_total_ticks = 0;
            game.enemy_attack_impact_ticks = 0;
            written = snprintf(path, sizeof path,
                               "%s/%s-player-knockout.ppm",
                               directory, cast_ids[cast]);
            if (written < 0 || (size_t)written >= sizeof path ||
                !land_render(&renderer, &game, &graphics) ||
                !sr_write_ppm(ki_td_soft_canvas(&renderer), path)) {
                success = false;
                break;
            }
            game.player_hit_ticks = 0;
            game.player_recovery_ticks =
                LAND_PLAYER_RECOVERY_TICKS - 30;
            written = snprintf(path, sizeof path,
                               "%s/%s-player-downed.ppm",
                               directory, cast_ids[cast]);
            if (written < 0 || (size_t)written >= sizeof path ||
                !land_render(&renderer, &game, &graphics) ||
                !sr_write_ppm(ki_td_soft_canvas(&renderer), path)) {
                success = false;
                break;
            }
        }
    }
    ki_td_soft_renderer_destroy(&renderer);
    land_graphics_shutdown(&graphics);
    if (!success) return EXIT_FAILURE;
    (void)printf(
        "PASS render scene=training casts=4 files=28 "
        "source-enemies=yes source-attacks=yes aim-guides=2 "
        "recoil-frames=2 free-swings=4 enemy-warnings=4 enemy-hits=4 "
        "player-knockouts=4 player-downed=4 "
        "size=960x540 directory=%s\n",
        directory);
    return EXIT_SUCCESS;
}

static int walk_render_test(const char *directory)
{
    static const char *const cast_ids[LAND_CAST_COUNT] = {
        "legend", "chumrunner", "fantasy", "pleb-bound"
    };
    static const char *const direction_ids[] = {
        "down", "left", "right", "up"
    };
    static const int movement[][2] = {
        {0, 1}, {-1, 0}, {1, 0}, {0, -1}
    };
    land_graphics graphics;
    ki_td_soft_renderer renderer = {0};
    land_state game;
    int cast;
    bool success = true;
    if (!directory || !land_graphics_init(&graphics, asset_root()))
        return EXIT_FAILURE;
    if (!ki_td_soft_renderer_init(&renderer, 960, 540)) {
        land_graphics_shutdown(&graphics);
        return EXIT_FAILURE;
    }
    for (cast = 0; cast < LAND_CAST_COUNT && success; ++cast) {
        int direction;
        for (direction = LAND_FACING_DOWN;
             direction <= LAND_FACING_UP && success; ++direction) {
            int frame;
            land_game_init(&game);
            game.cast_cursor = (land_cast)cast;
            (void)land_game_confirm_cast(&game);
            game.player_x = direction == LAND_FACING_LEFT ? 300.0f :
                            (direction == LAND_FACING_RIGHT ? 100.0f :
                             180.0f);
            game.player_y = direction == LAND_FACING_DOWN ? 180.0f :
                            (direction == LAND_FACING_UP ? 252.0f :
                             235.0f);
            game.toast_ticks = 0;
            for (frame = 0; frame < 8; ++frame) {
                char path[1024];
                uint64_t target_tick =
                    (uint64_t)(unsigned int)frame * 5u + 1u;
                int written = snprintf(
                    path, sizeof path, "%s/%s-%s-%02d.ppm",
                    directory, cast_ids[cast], direction_ids[direction],
                    frame);
                while (game.simulation_tick < target_tick)
                    land_game_update(&game, movement[direction][0],
                                     movement[direction][1],
                                     LAND_TICK_SECONDS);
                if (written < 0 || (size_t)written >= sizeof path ||
                    !game.player_moving ||
                    game.facing != (land_facing)direction ||
                    !land_render(&renderer, &game, &graphics) ||
                    !sr_write_ppm(ki_td_soft_canvas(&renderer), path)) {
                    success = false;
                    break;
                }
            }
        }
    }
    ki_td_soft_renderer_destroy(&renderer);
    land_graphics_shutdown(&graphics);
    if (!success) return EXIT_FAILURE;
    (void)printf(
        "PASS render scene=walk casts=4 directions=4 frames=128 "
        "source=simulation "
        "size=960x540 "
        "directory=%s\n", directory);
    return EXIT_SUCCESS;
}

static int run_interactive(void)
{
    kittyts_options terminal_options;
    kilix_game_signal_scope signals = {0};
    kilix_game_clock clock;
    kilix_game_clock_options clock_options;
    ki_td_soft_renderer renderer = {0};
    land_graphics graphics;
    land_audio audio = {0};
    land_state game;
    land_input current_input;
    land_input pending_input = {0};
    bool quit_requested = false;
    bool failed = false;
    bool first_frame = true;
    uint64_t last_presented_step = UINT64_MAX;
    int width;
    int height;

    land_game_init(&game);
    if (!land_graphics_init(&graphics, asset_root())) {
        (void)fprintf(stderr,
            "kilix-land: could not load graphics under %s "
            "(set KILIX_LAND_ASSETS)\n", asset_root());
        return EXIT_FAILURE;
    }
    if (!land_audio_init(&audio, asset_root(), true))
        (void)fprintf(
            stderr,
            "kilix-land: native sound assets could not be loaded; "
            "continuing silently\n");
    kittyts_session_init(&terminal_session);
    kittyts_options_init(&terminal_options);
    terminal_options.framebuffer.min_width = 800;
    terminal_options.framebuffer.min_height = 450;
    terminal_options.framebuffer.max_width = 1280;
    terminal_options.framebuffer.max_height = 720;
    if (kittyts_start(&terminal_session, STDIN_FILENO, STDOUT_FILENO,
                      &terminal_options) != 0) {
        (void)fprintf(stderr, "kilix-land: terminal start failed: %s\n",
                      strerror(errno));
        land_audio_shutdown(&audio);
        land_graphics_shutdown(&graphics);
        return EXIT_FAILURE;
    }
    if (!kilix_game_signals_install(&signals)) {
        kittyts_stop(&terminal_session);
        land_audio_shutdown(&audio);
        land_graphics_shutdown(&graphics);
        (void)fprintf(stderr, "kilix-land: signal scope failed\n");
        return EXIT_FAILURE;
    }
    last_movement_key = KITTYKB_KEY_NONE;
    width = kittyts_width(&terminal_session);
    height = kittyts_height(&terminal_session);
    if (!ki_td_soft_renderer_init(&renderer, width, height)) {
        failed = true;
        goto done;
    }
    kilix_game_clock_options_init(&clock_options);
    clock_options.step_ns = KILIX_GAME_NANOSECONDS_PER_SECOND /
                            LAND_SIMULATION_HZ;
    if (!kilix_game_clock_init(&clock, &clock_options)) {
        failed = true;
        goto done;
    }

    while (!quit_requested && !kilix_game_signals_requested(&signals)) {
        kilix_game_frame frame;
        uint32_t step;
        int resized_width;
        int resized_height;
        int64_t now;

        if (!poll_input(&current_input, &quit_requested)) {
            failed = true;
            break;
        }
        if (quit_requested) break;
        if (current_input.cancel_pressed &&
            ((game.mode == LAND_MODE_ROOM &&
              game.location == LAND_LOCATION_SOCIAL) ||
             (game.mode == LAND_MODE_CAST_SELECT &&
              !game.cast_confirmed))) {
            quit_requested = true;
            break;
        }
        merge_pending_input(&pending_input, &current_input);

        now = kilix_game_monotonic_ns();
        frame = kilix_game_clock_advance(&clock, now);
        for (step = 0u; step < frame.steps; ++step) {
            if (pending_input.cast_pressed) {
                land_mode previous_mode = game.mode;
                land_game_open_cast_select(&game);
                if (game.mode != previous_mode)
                    land_audio_play(
                        &audio, land_visible_cast(&game),
                        LAND_AUDIO_UI_CONFIRM);
            }
            if (pending_input.menu_delta != 0) {
                land_cast previous_cursor = game.cast_cursor;
                land_game_move_cast_cursor(&game,
                                           pending_input.menu_delta);
                if (game.cast_cursor != previous_cursor)
                    land_audio_play(
                        &audio, land_visible_cast(&game),
                        LAND_AUDIO_UI_MOVE);
            }
            if (pending_input.cancel_pressed)
                land_game_cancel(&game);
            else if (pending_input.interact_pressed) {
                land_mode previous_mode = game.mode;
                land_cast sound_cast = land_visible_cast(&game);
                if (land_game_interact(&game))
                    land_audio_play(
                        &audio, sound_cast,
                        previous_mode == LAND_MODE_CAST_SELECT ?
                        LAND_AUDIO_UI_CONFIRM : LAND_AUDIO_DIALOGUE);
            }
            if (pending_input.aim_pressed) {
                bool previous_aiming = game.aiming;
                land_game_set_aiming(&game, !game.aiming);
                if (game.aiming != previous_aiming)
                    land_audio_play(
                        &audio, game.cast, LAND_AUDIO_UI_CONFIRM);
            }
            if (pending_input.reload_pressed &&
                land_game_reload(&game))
                land_audio_play(
                    &audio, game.cast, LAND_AUDIO_RELOAD);
            if (pending_input.attack_pressed &&
                land_game_attack(&game))
                play_attack_start(&audio, &game);
            {
                land_state before_update = game;
                land_game_update(&game, pending_input.move_x,
                             pending_input.move_y, LAND_TICK_SECONDS);
                play_state_audio(&audio, &before_update, &game);
            }
            land_audio_update(&audio, LAND_TICK_SECONDS);
            consume_edge_input(&pending_input);
        }

        if (kittyts_check_resize(&terminal_session, &resized_width,
                                 &resized_height)) {
            if (!ki_td_soft_renderer_resize(&renderer, resized_width,
                                            resized_height)) {
                failed = true;
                break;
            }
            width = resized_width;
            height = resized_height;
            first_frame = true;
        }
        if (first_frame ||
            game.simulation_tick / 2u != last_presented_step / 2u) {
            if (!land_render(&renderer, &game, &graphics) ||
                !kittyts_present(&terminal_session, renderer.rgba,
                                 width, height)) {
                failed = true;
                break;
            }
            first_frame = false;
            last_presented_step = game.simulation_tick;
        }
        if (frame.steps == 0u)
            (void)kilix_game_sleep_until_ns(now + INT64_C(1000000));
    }

done:
    ki_td_soft_renderer_destroy(&renderer);
    kittyts_stop(&terminal_session);
    kilix_game_signals_restore(&signals);
    land_audio_shutdown(&audio);
    land_graphics_shutdown(&graphics);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

static void usage(const char *program)
{
    (void)fprintf(stderr,
        "usage: %s [--selftest | --graphics-test | --menu-render-test FILE | "
        "--room-render-test FILE | --render-test FILE | "
        "--cast-render-test DIR | --training-render-test DIR | "
        "--walk-render-test DIR | --audio-test | --version]\n",
        program);
}

int main(int argc, char **argv)
{
    if (argc == 1) return run_interactive();
    if (argc == 2 && strcmp(argv[1], "--selftest") == 0) return selftest();
    if (argc == 2 && strcmp(argv[1], "--graphics-test") == 0)
        return graphics_test();
    if (argc == 2 && strcmp(argv[1], "--audio-test") == 0)
        return audio_test();
    if (argc == 3 && strcmp(argv[1], "--menu-render-test") == 0)
        return render_test(argv[2], RENDER_TEST_MENU);
    if (argc == 3 && strcmp(argv[1], "--room-render-test") == 0)
        return render_test(argv[2], RENDER_TEST_ROOM);
    if (argc == 3 && strcmp(argv[1], "--render-test") == 0)
        return render_test(argv[2], RENDER_TEST_DIALOGUE);
    if (argc == 3 && strcmp(argv[1], "--cast-render-test") == 0)
        return cast_render_test(argv[2]);
    if (argc == 3 && strcmp(argv[1], "--training-render-test") == 0)
        return training_render_test(argv[2]);
    if (argc == 3 && strcmp(argv[1], "--walk-render-test") == 0)
        return walk_render_test(argv[2]);
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        (void)printf("kilix-land 0.3.0\n");
        return EXIT_SUCCESS;
    }
    usage(argv[0]);
    return 2;
}
