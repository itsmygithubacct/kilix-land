#include "kilix_land.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct dialogue_beat {
    land_actor speaker;
    int expression;
    const char *text;
} dialogue_beat;

typedef struct conversation {
    const dialogue_beat *beats;
    int count;
} conversation;

typedef struct cast_profile {
    const char *title;
    const char *subtitle;
    const char *room_name;
    const char *actor_names[LAND_ACTOR_COUNT];
    uint32_t actor_colors[LAND_ACTOR_COUNT];
} cast_profile;

typedef struct enemy_profile {
    const char *name;
    int hp;
    int defense;
    int attack;
    int speed;
    bool ranged;
    int visual;
    int accuracy;
    int evasion;
    int variance;
    int hits;
    int range;
    int preferred_range;
    int ai;
    int level;
    bool ability;
    int timing;
} enemy_profile;

#define LAND_PLAYER_SPEED LAND_PARITY_LEGEND_PLAYER_SPEED
#define LAND_SOCIAL_LEFT 24.0f
#define LAND_SOCIAL_EXIT_RIGHT 460.0f
#define LAND_TRAINING_EXIT_LEFT 20.0f
#define LAND_TRAINING_RIGHT 326.0f
#define LAND_FLOOR_TOP 180.0f
#define LAND_FLOOR_BOTTOM 252.0f
#define LAND_ENEMY_RESPAWN_TICKS 72
#define LAND_CHUM_AIM_SPEED 156.0f
#define LAND_AIM_LEFT 18.0f
#define LAND_AIM_RIGHT 462.0f
#define LAND_AIM_TOP 88.0f
#define LAND_AIM_BOTTOM 244.0f
#define LAND_AIM_HIT_RADIUS_X 26.0f
#define LAND_AIM_HIT_RADIUS_Y 28.0f
#define CONVERSATION_OF(beats_) \
    {beats_, (int)(sizeof beats_ / sizeof beats_[0])}

static const cast_profile CAST_PROFILES[LAND_CAST_COUNT] = {
    {
        "LEGEND OF KILIX",
        "KILIX + FRONTIER FRIENDS",
        "HEARTHMARKET",
        {"KILIX", "EMBER BADGER", "BEACON KEEPER", "HARE COURIER"},
        {UINT32_C(0xffa33c), UINT32_C(0xe98243),
         UINT32_C(0x7ed7dc), UINT32_C(0xbadf78)}
    },
    {
        "CHUMRUNNER",
        "ROOK + THE RUNNER CREW",
        "NULL SAFEHOUSE",
        {"ROOK", "CIPHER", "PATCH", LAND_PARITY_CHUM_ALLY_3},
        {UINT32_C(0x55d6d0), UINT32_C(0xb69cff),
         UINT32_C(0xf0c36b), UINT32_C(0xff7f6f)}
    },
    {
        "KILIX FANTASY",
        "ARDEN + THE CRYSTAL PARTY",
        "EMBERLIGHT",
        {"ARDEN", "MIRA", "SOL", LAND_PARITY_FANTASY_ALLY_3},
        {UINT32_C(0x55d6d0), UINT32_C(0xb69cff),
         UINT32_C(0x9ee27a), UINT32_C(0xf0c36b)}
    },
    {
        "PLEB BOUND",
        "PIP + THE MAPLE LOOP CREW",
        "MAPLE WORKROOM",
        {"PIP", "MO", "DOT", "BERYL"},
        {UINT32_C(0xffc15c), UINT32_C(0x75d8c8),
         UINT32_C(0xff8c87), UINT32_C(0xb69cff)}
    }
};

/*
 * Names and combat values are copied from the source games. Legend of Kilix
 * has visual enemies but no enemy-health combat system, so its range uses a
 * one-hit training tag instead of inventing per-enemy statistics.
 */
static const enemy_profile LEGEND_ENEMIES[] = {
#define LEGEND_ENEMY(name_, speed_, ranged_, visual_) \
    {name_, 1, 0, 1, speed_, ranged_, visual_, 100, 0, 0, 1, \
     0, 0, 0, 0, false, 0}
    LEGEND_ENEMY("ASH HOPPER", 62, false, 0),
    LEGEND_ENEMY("SOOT SPRIG", 50, false, 1),
    LEGEND_ENEMY("RELAY WISP", 42, true, 2),
    LEGEND_ENEMY("COPPERJAW CRAB", 38, false, 3),
    LEGEND_ENEMY("PUMP LEECH", 48, false, 4),
    LEGEND_ENEMY("CURRENT SKATER", 68, true, 5),
    LEGEND_ENEMY("GLASSWING KITE", 56, true, 6),
    LEGEND_ENEMY("SHARD ROLLER", 64, false, 7),
    LEGEND_ENEMY("BELLOWS BAT", 58, true, 8),
    LEGEND_ENEMY("RAIL TICK", 52, false, 9),
    LEGEND_ENEMY("KILN HOUND", 66, false, 10),
    LEGEND_ENEMY("CINDER CARRIAGE", 36, false, 11),
    LEGEND_ENEMY("FROST SIPHON", 44, true, 12),
    LEGEND_ENEMY("AURORA MOTH", 54, true, 13),
    LEGEND_ENEMY("BLACKFROST RAM", 60, false, 14),
    LEGEND_ENEMY("ECHO HUSK", 46, false, 15),
    LEGEND_ENEMY("NULL EYE", 40, true, 16),
    LEGEND_ENEMY("SIGNAL MIMIC", 48, true, 17)
#undef LEGEND_ENEMY
};

#define SOURCE_ENEMY_ROW(name_, hp_, defense_, attack_, speed_, ranged_, \
                         visual_, accuracy_, evasion_, variance_, hits_, \
                         range_, preferred_, ai_, level_, ability_, timing_) \
    {name_, hp_, defense_, attack_, speed_, ranged_, visual_, accuracy_, \
     evasion_, variance_, hits_, range_, preferred_, ai_, level_, ability_, \
     timing_},

static const enemy_profile CHUM_ENEMIES[] = {
    LAND_PARITY_CHUM_ENEMIES(SOURCE_ENEMY_ROW)
};

static const enemy_profile FANTASY_ENEMIES[] = {
    LAND_PARITY_FANTASY_ENEMIES(SOURCE_ENEMY_ROW)
};

static const enemy_profile PLEB_ENEMIES[] = {
    LAND_PARITY_PLEB_ENEMIES(SOURCE_ENEMY_ROW)
};

#undef SOURCE_ENEMY_ROW

static const float ACTOR_X[LAND_ACTOR_COUNT] = {
    0.0f, 118.0f, 240.0f, 362.0f
};

static const float ACTOR_Y[LAND_ACTOR_COUNT] = {
    0.0f, 184.0f, 180.0f, 184.0f
};

static const dialogue_beat LEGEND_BADGER_FIRST[] = {
    {LAND_ACTOR_ALLY_1, 0, "Kilix, I tuned your trail clasp. It should stop singing near old relay wire."},
    {LAND_ACTOR_HERO, 1, "But the singing was how I knew the relay wire was nervous."},
    {LAND_ACTOR_ALLY_2, 0, "The west beacon changed pitch too. Something woke beyond the ridge."},
    {LAND_ACTOR_ALLY_3, 0, "I brought a route map, two sealed notes, and one emergency biscuit."},
    {LAND_ACTOR_HERO, 0, "Good. The clasp, beacon, and biscuit agree: we investigate."}
};

static const dialogue_beat LEGEND_BEACON_FIRST[] = {
    {LAND_ACTOR_ALLY_2, 0, "The west beacon flashed your old call sign three times before dawn."},
    {LAND_ACTOR_HERO, 1, "That is either a warning or a very formal invitation."},
    {LAND_ACTOR_ALLY_1, 0, "Take the insulated wrench. Invitations rarely arc blue lightning."},
    {LAND_ACTOR_ALLY_3, 1, "I can reach the ridge first and mark the safe footholds."},
    {LAND_ACTOR_HERO, 0, "Then we answer together. No beacon gets to be mysterious alone."}
};

static const dialogue_beat LEGEND_COURIER_FIRST[] = {
    {LAND_ACTOR_ALLY_3, 0, "Package for Kilix: one impossible compass, sender listed as tomorrow."},
    {LAND_ACTOR_HERO, 1, "Tomorrow usually has better handwriting."},
    {LAND_ACTOR_ALLY_2, 0, "Its needle points through the hearth, toward the buried signal lines."},
    {LAND_ACTOR_ALLY_1, 1, "We are not dismantling my floor until I fetch the careful hammer."},
    {LAND_ACTOR_HERO, 0, "A careful hammer and an impossible compass. Excellent teamwork."}
};

static const dialogue_beat LEGEND_BADGER_REPEAT[] = {
    {LAND_ACTOR_ALLY_1, 0, "The clasp is ready. Try to return it with the same number of buckles."},
    {LAND_ACTOR_HERO, 1, "I can confidently promise approximately that."}
};

static const dialogue_beat LEGEND_BEACON_REPEAT[] = {
    {LAND_ACTOR_ALLY_2, 0, "The beacon is quiet now, but it is listening."},
    {LAND_ACTOR_HERO, 0, "Then let us give it a good story to overhear."}
};

static const dialogue_beat LEGEND_COURIER_REPEAT[] = {
    {LAND_ACTOR_ALLY_3, 0, "Emergency biscuit status: still exactly one."},
    {LAND_ACTOR_HERO, 1, "Your restraint is the true legend here."}
};

static const dialogue_beat CHUM_CIPHER_FIRST[] = {
    {LAND_ACTOR_ALLY_1, 0, "Rook, the market cameras blink in a pattern that spells our route."},
    {LAND_ACTOR_HERO, 1, "Convenient. Also deeply insulting to our operational security."},
    {LAND_ACTOR_ALLY_2, 0, "I can loop the feed, but only if nobody kicks another junction box."},
    {LAND_ACTOR_ALLY_3, 1, "For the record, that junction box owed me money."},
    {LAND_ACTOR_HERO, 0, "Cipher loops it, Patch watches the grid, and Mara keeps both boots holstered."}
};

static const dialogue_beat CHUM_PATCH_FIRST[] = {
    {LAND_ACTOR_ALLY_2, 0, "Your rig is losing six milliseconds whenever the blue relay fires."},
    {LAND_ACTOR_HERO, 1, "Six? I have wasted longer choosing noodles."},
    {LAND_ACTOR_ALLY_1, 0, "Six milliseconds is the difference between a clean ghost and an arrest."},
    {LAND_ACTOR_ALLY_3, 0, "It is also enough time for me to deny knowing any of you."},
    {LAND_ACTOR_HERO, 0, "Patch fixes the relay. I will handle the noodles with equal discipline."}
};

static const dialogue_beat CHUM_MARA_FIRST[] = {
    {LAND_ACTOR_ALLY_3, 0, "I found a buyer for the Kestrel shard. They requested no questions."},
    {LAND_ACTOR_HERO, 1, "Then they have chosen the least qualified room in the city."},
    {LAND_ACTOR_ALLY_1, 0, "The shard is broadcasting a handshake every forty seconds."},
    {LAND_ACTOR_ALLY_2, 1, "And something underground keeps answering."},
    {LAND_ACTOR_HERO, 0, "Cancel the buyer. We follow the handshake before it follows us."}
};

static const dialogue_beat CHUM_CIPHER_REPEAT[] = {
    {LAND_ACTOR_ALLY_1, 0, "The camera loop is stable for nine minutes."},
    {LAND_ACTOR_HERO, 1, "Luxury. We may even use the door this time."}
};

static const dialogue_beat CHUM_PATCH_REPEAT[] = {
    {LAND_ACTOR_ALLY_2, 0, "Blue relay is clean. Please stop naming the diagnostic lights."},
    {LAND_ACTOR_HERO, 0, "Too late. That one is Bernard."}
};

static const dialogue_beat CHUM_MARA_REPEAT[] = {
    {LAND_ACTOR_ALLY_3, 0, "Buyer cancelled. They sent a very expensive silence."},
    {LAND_ACTOR_HERO, 1, "Keep the silence. It matches the decor."}
};

static const dialogue_beat FANTASY_MIRA_FIRST[] = {
    {LAND_ACTOR_ALLY_1, 0, "Arden, the star compass points beneath the hearth whenever you cross the rug."},
    {LAND_ACTOR_HERO, 1, "We are not dismantling the guild hall five minutes after arriving."},
    {LAND_ACTOR_ALLY_2, 0, "What if we ask the hearth nicely before we dismantle it?"},
    {LAND_ACTOR_ALLY_3, 1, "The archive records do mention a sealed rootglass stair."},
    {LAND_ACTOR_HERO, 0, "We ask nicely, then Tamsin finds the least structural stone."}
};

static const dialogue_beat FANTASY_SOL_FIRST[] = {
    {LAND_ACTOR_ALLY_2, 0, "The lantern vine opened a new flower when you walked in, Arden."},
    {LAND_ACTOR_HERO, 0, "It blooms near impossible expeditions. I recognize the expression."},
    {LAND_ACTOR_ALLY_1, 1, "Its light has the same frequency as the observatory seal."},
    {LAND_ACTOR_ALLY_3, 0, "Keep the flower. The river road will be dark after moonrise."},
    {LAND_ACTOR_HERO, 1, "A glowing flower and a forbidden stair. A quiet evening, then."}
};

static const dialogue_beat FANTASY_TAMSIN_FIRST[] = {
    {LAND_ACTOR_ALLY_3, 0, "Three archive pages vanished, but their footnotes remained behind."},
    {LAND_ACTOR_HERO, 1, "Thieves with standards. That narrows the field."},
    {LAND_ACTOR_ALLY_1, 0, "The footnotes describe a route through Rootglass Hollow."},
    {LAND_ACTOR_ALLY_2, 1, "I packed berry buns. Scholarly pursuit requires provisions."},
    {LAND_ACTOR_HERO, 0, "Then at first light, we follow the scholarship and the buns."}
};

static const dialogue_beat FANTASY_MIRA_REPEAT[] = {
    {LAND_ACTOR_ALLY_1, 0, "The compass still reacts to your boots. Fascinating."},
    {LAND_ACTOR_HERO, 1, "Please record that they are officially instruments of science."}
};

static const dialogue_beat FANTASY_SOL_REPEAT[] = {
    {LAND_ACTOR_ALLY_2, 0, "I slipped an extra berry bun into your pack."},
    {LAND_ACTOR_HERO, 0, "And that is why you are the party's wisest member."}
};

static const dialogue_beat FANTASY_TAMSIN_REPEAT[] = {
    {LAND_ACTOR_ALLY_3, 0, "The least structural stone is marked with chalk."},
    {LAND_ACTOR_HERO, 1, "A sentence every guild master longs to hear."}
};

static const dialogue_beat PLEB_MO_FIRST[] = {
    {LAND_ACTOR_ALLY_1, 0, "Pip, the parking meter followed us home again."},
    {LAND_ACTOR_HERO, 1, "Did you feed it after midnight? The sticker was extremely clear."},
    {LAND_ACTOR_ALLY_2, 0, "It printed a receipt for tomorrow and circled the civic mall."},
    {LAND_ACTOR_ALLY_3, 1, "I can tune the receipt into a low-budget dimensional key."},
    {LAND_ACTOR_HERO, 0, "Great. Mo brings exact change. We investigate after snacks."}
};

static const dialogue_beat PLEB_DOT_FIRST[] = {
    {LAND_ACTOR_ALLY_2, 0, "The neighborhood pigeons formed a committee about our basement."},
    {LAND_ACTOR_HERO, 1, "Do they have jurisdiction? I refuse another surprise audit."},
    {LAND_ACTOR_ALLY_1, 0, "One is wearing a tie, so probably."},
    {LAND_ACTOR_ALLY_3, 1, "Their minutes contain a star map hidden in the bullet points."},
    {LAND_ACTOR_HERO, 0, "Dot decodes it. I will prepare a respectful breadcrumb statement."}
};

static const dialogue_beat PLEB_BERYL_FIRST[] = {
    {LAND_ACTOR_ALLY_3, 0, "My moon receiver picked up a coupon broadcasting from under the mall."},
    {LAND_ACTOR_HERO, 1, "Expired? I need to know what kind of evil we are facing."},
    {LAND_ACTOR_ALLY_2, 0, "No expiration date. No store name. Just coordinates."},
    {LAND_ACTOR_ALLY_1, 1, "That is how they get you. First coordinates, then a loyalty program."},
    {LAND_ACTOR_HERO, 0, "We go together, and nobody scans anything without asking."}
};

static const dialogue_beat PLEB_MO_REPEAT[] = {
    {LAND_ACTOR_ALLY_1, 0, "I found exact change and one suspicious arcade token."},
    {LAND_ACTOR_HERO, 1, "Bring both. Destiny rarely validates parking."}
};

static const dialogue_beat PLEB_DOT_REPEAT[] = {
    {LAND_ACTOR_ALLY_2, 0, "The pigeon committee approved our breadcrumb statement."},
    {LAND_ACTOR_HERO, 0, "Finally, responsible local government."}
};

static const dialogue_beat PLEB_BERYL_REPEAT[] = {
    {LAND_ACTOR_ALLY_3, 0, "Dimensional key is ready. It hums when pointed at discounts."},
    {LAND_ACTOR_HERO, 1, "Terrifying. Put it in the reusable bag."}
};

static const conversation FIRST_CONVERSATIONS[LAND_CAST_COUNT][3] = {
    {
        CONVERSATION_OF(LEGEND_BADGER_FIRST),
        CONVERSATION_OF(LEGEND_BEACON_FIRST),
        CONVERSATION_OF(LEGEND_COURIER_FIRST)
    },
    {
        CONVERSATION_OF(CHUM_CIPHER_FIRST),
        CONVERSATION_OF(CHUM_PATCH_FIRST),
        CONVERSATION_OF(CHUM_MARA_FIRST)
    },
    {
        CONVERSATION_OF(FANTASY_MIRA_FIRST),
        CONVERSATION_OF(FANTASY_SOL_FIRST),
        CONVERSATION_OF(FANTASY_TAMSIN_FIRST)
    },
    {
        CONVERSATION_OF(PLEB_MO_FIRST),
        CONVERSATION_OF(PLEB_DOT_FIRST),
        CONVERSATION_OF(PLEB_BERYL_FIRST)
    }
};

static const conversation REPEAT_CONVERSATIONS[LAND_CAST_COUNT][3] = {
    {
        CONVERSATION_OF(LEGEND_BADGER_REPEAT),
        CONVERSATION_OF(LEGEND_BEACON_REPEAT),
        CONVERSATION_OF(LEGEND_COURIER_REPEAT)
    },
    {
        CONVERSATION_OF(CHUM_CIPHER_REPEAT),
        CONVERSATION_OF(CHUM_PATCH_REPEAT),
        CONVERSATION_OF(CHUM_MARA_REPEAT)
    },
    {
        CONVERSATION_OF(FANTASY_MIRA_REPEAT),
        CONVERSATION_OF(FANTASY_SOL_REPEAT),
        CONVERSATION_OF(FANTASY_TAMSIN_REPEAT)
    },
    {
        CONVERSATION_OF(PLEB_MO_REPEAT),
        CONVERSATION_OF(PLEB_DOT_REPEAT),
        CONVERSATION_OF(PLEB_BERYL_REPEAT)
    }
};

static void set_toast(land_state *game, const char *message);

static bool cast_valid(land_cast cast)
{
    return cast >= LAND_CAST_LEGEND && cast <= LAND_CAST_PLEB_BOUND;
}

static const cast_profile *profile_for(land_cast cast)
{
    return cast_valid(cast) ? &CAST_PROFILES[cast] :
           &CAST_PROFILES[LAND_CAST_LEGEND];
}

int land_enemy_catalog_count(land_cast cast)
{
    switch (cast) {
    case LAND_CAST_LEGEND:
        return (int)(sizeof LEGEND_ENEMIES / sizeof LEGEND_ENEMIES[0]);
    case LAND_CAST_CHUMRUNNER:
        return (int)(sizeof CHUM_ENEMIES / sizeof CHUM_ENEMIES[0]);
    case LAND_CAST_FANTASY:
        return (int)(sizeof FANTASY_ENEMIES / sizeof FANTASY_ENEMIES[0]);
    case LAND_CAST_PLEB_BOUND:
        return (int)(sizeof PLEB_ENEMIES / sizeof PLEB_ENEMIES[0]);
    default:
        return 0;
    }
}

static const enemy_profile *enemy_profile_for(land_cast cast, int kind)
{
    if (kind < 0 || kind >= land_enemy_catalog_count(cast)) return NULL;
    switch (cast) {
    case LAND_CAST_LEGEND:
        return &LEGEND_ENEMIES[kind];
    case LAND_CAST_CHUMRUNNER:
        return &CHUM_ENEMIES[kind];
    case LAND_CAST_FANTASY:
        return &FANTASY_ENEMIES[kind];
    case LAND_CAST_PLEB_BOUND:
        return &PLEB_ENEMIES[kind];
    default:
        return NULL;
    }
}

static uint32_t next_random(land_state *game)
{
    uint32_t value = game->rng;
    if (value == 0u) value = UINT32_C(0x4b494c58);
    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    game->rng = value;
    return value;
}

static int clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void clear_attack(land_state *game)
{
    game->attack = LAND_ATTACK_NONE;
    game->attack_ticks = 0;
    game->attack_total_ticks = 0;
    game->attack_impact_ticks = 0;
    game->attack_cooldown_ticks = 0;
    game->attack_damage = 0;
    game->attack_hit_chance = 0;
    game->attack_hit_roll = 0;
    game->attack_variance = 0;
    game->attack_critical_chance = 0;
    game->attack_critical_roll = 0;
    game->attack_critical = false;
    game->attack_landed = false;
    game->attack_will_hit = false;
    game->attack_target_x = LAND_TRAINING_ENEMY_X;
    game->attack_target_y = LAND_TRAINING_ENEMY_Y;
    game->pip_timing_grade = 0;
}

static void clear_enemy_action(land_state *game)
{
    game->enemy_action = LAND_ENEMY_ACTION_IDLE;
    game->enemy_attack_ticks = 0;
    game->enemy_attack_total_ticks = 0;
    game->enemy_attack_impact_ticks = 0;
    game->enemy_attack_damage = 0;
    game->enemy_attack_hit_chance = 0;
    game->enemy_attack_hit_roll = 0;
    game->enemy_attack_variance = 0;
    game->enemy_attack_hits_planned = 0;
    game->enemy_attack_hits_landed = 0;
    game->enemy_attack_landed = false;
    game->enemy_attack_hit = false;
    game->enemy_attack_critical = false;
    game->enemy_attack_ranged = false;
    game->enemy_attack_target_x = game->player_x;
    game->enemy_attack_target_y = game->player_y - 34.0f;
}

static void set_player_profile(land_state *game, bool refill)
{
    int maximum_hp = LAND_PARITY_LEGEND_PLAYER_HP;
    int defense = 0;
    if (!game) return;
    if (game->cast == LAND_CAST_CHUMRUNNER) {
        maximum_hp = LAND_PARITY_CHUM_PLAYER_HP;
        defense = LAND_PARITY_CHUM_PLAYER_MITIGATION;
    } else if (game->cast == LAND_CAST_FANTASY) {
        maximum_hp = LAND_PARITY_FANTASY_PLAYER_HP;
        defense = LAND_PARITY_FANTASY_PLAYER_DEFENSE;
    } else if (game->cast == LAND_CAST_PLEB_BOUND) {
        maximum_hp = LAND_PARITY_PLEB_PLAYER_HP;
        defense = LAND_PARITY_PLEB_PLAYER_DEFENSE;
    }
    game->player_max_hp = maximum_hp;
    game->player_defense = defense;
    if (refill || game->player_hp <= 0 || game->player_hp > maximum_hp)
        game->player_hp = maximum_hp;
    if (refill) {
        game->player_hit_ticks = 0;
        game->player_recovery_ticks = 0;
    }
}

static void reset_player_resources(land_state *game)
{
    if (!game) return;
    game->kilix_rifle_charge = LAND_KILIX_MAX_RIFLE_CHARGE;
    game->rook_magazine = LAND_PARITY_CHUM_PREDATOR_MAGAZINE;
    game->rook_reserve_ammo = LAND_PARITY_CHUM_RESERVE_AMMO;
    game->rook_reload_ticks = 0;
}

static bool ranged_cast(land_cast cast)
{
    return cast == LAND_CAST_LEGEND || cast == LAND_CAST_CHUMRUNNER;
}

static float ranged_muzzle_y(const land_state *game)
{
    return game->player_y -
           (game->cast == LAND_CAST_LEGEND ? 31.0f : 45.0f);
}

static float ranged_muzzle_x(const land_state *game)
{
    return game->player_x +
           (game->cast == LAND_CAST_LEGEND ? 20.0f : 29.0f);
}

static bool enemy_available(const land_state *game)
{
    return game->enemy_kind >= 0 && game->enemy_hp > 0 &&
           game->enemy_respawn_ticks == 0;
}

static int grid_axis_distance(float difference, float cell_size)
{
    int cells;
    if (difference < 0.0f) difference = -difference;
    cells = (int)(difference / cell_size);
    if ((float)cells * cell_size < difference) cells++;
    return cells;
}

int land_rook_range(const land_state *game)
{
    if (!game || game->cast != LAND_CAST_CHUMRUNNER ||
        !enemy_available(game))
        return 0;
    return grid_axis_distance(game->enemy_x - game->player_x, 56.0f) +
           grid_axis_distance(game->enemy_y - game->player_y, 36.0f);
}

int land_rook_hit_chance(const land_state *game)
{
    int distance;
    int range_modifier;
    int chance;
    if (!game || game->cast != LAND_CAST_CHUMRUNNER ||
        game->rook_magazine <= 0)
        return 0;
    distance = land_rook_range(game);
    if (!enemy_available(game) ||
        distance > LAND_PARITY_CHUM_PREDATOR_RANGE)
        return 0;
    range_modifier =
        distance <= LAND_PARITY_CHUM_PREDATOR_OPTIMAL_RANGE ?
        6 : -(distance - LAND_PARITY_CHUM_PREDATOR_OPTIMAL_RANGE) * 8;
    chance = 45 + LAND_PARITY_CHUM_FIREARM_SCORE * 2 +
             LAND_PARITY_CHUM_CYBERWARE * 2 +
             LAND_PARITY_CHUM_LOYALTY -
             (distance > 1 ? distance - 1 : 0) * 3 +
             range_modifier +
             LAND_PARITY_CHUM_PREDATOR_ACCURACY * 4;
    return clamp_int(chance, 15, 95);
}

float land_enemy_target_x(const land_state *game)
{
    return game ? game->enemy_x - 8.0f : LAND_TRAINING_ENEMY_X;
}

float land_enemy_target_y(const land_state *game)
{
    return game ? game->enemy_y - 35.0f : LAND_TRAINING_ENEMY_Y;
}

static bool melee_target_in_range(const land_state *game)
{
    float width_delta = game->enemy_x - game->player_x;
    float height_delta = game->player_y - game->enemy_y;
    if (width_delta < 0.0f) width_delta = -width_delta;
    if (height_delta < 0.0f) height_delta = -height_delta;
    return enemy_available(game) && width_delta <= 108.0f &&
           height_delta <= 50.0f;
}

bool land_game_target_locked(const land_state *game)
{
    float dx;
    float dy;
    if (!game || !game->aiming || !ranged_cast(game->cast))
        return false;
    dx = game->aim_x - land_enemy_target_x(game);
    dy = game->aim_y - land_enemy_target_y(game);
    if (dx < 0.0f) dx = -dx;
    if (dy < 0.0f) dy = -dy;
    return enemy_available(game) &&
           dx <= LAND_AIM_HIT_RADIUS_X &&
           dy <= LAND_AIM_HIT_RADIUS_Y;
}

void land_game_set_aiming(land_state *game, bool aiming)
{
    if (!game) return;
    if (!aiming) {
        game->aiming = false;
        return;
    }
    if (!game->aiming &&
        game->mode == LAND_MODE_ROOM &&
        game->location == LAND_LOCATION_TRAINING &&
        game->player_hp > 0 && game->player_recovery_ticks == 0 &&
        ranged_cast(game->cast) && game->attack_ticks == 0 &&
        game->rook_reload_ticks == 0) {
        game->aim_x = ranged_muzzle_x(game) + 92.0f;
        game->aim_y = ranged_muzzle_y(game);
        if (game->aim_x < LAND_AIM_LEFT) game->aim_x = LAND_AIM_LEFT;
        if (game->aim_x > LAND_AIM_RIGHT) game->aim_x = LAND_AIM_RIGHT;
        if (game->aim_y < LAND_AIM_TOP) game->aim_y = LAND_AIM_TOP;
        if (game->aim_y > LAND_AIM_BOTTOM) game->aim_y = LAND_AIM_BOTTOM;
        game->aiming = true;
    }
    if (game->aiming) {
        game->facing = LAND_FACING_RIGHT;
        game->player_moving = false;
    }
}

static void spawn_training_enemy(land_state *game)
{
    const enemy_profile *profile;
    int count;
    int previous;
    int kind;
    if (!game || !cast_valid(game->cast)) return;
    count = land_enemy_catalog_count(game->cast);
    if (count <= 0) return;
    previous = game->enemy_kind;
    kind = (int)(next_random(game) % (uint32_t)count);
    if (count > 1 && kind == previous)
        kind = (kind + 1 + (int)(next_random(game) %
               (uint32_t)(count - 1))) % count;
    profile = enemy_profile_for(game->cast, kind);
    if (!profile) return;
    game->enemy_kind = kind;
    game->enemy_max_hp = profile->hp;
    game->enemy_hp = profile->hp;
    game->enemy_defense = profile->defense;
    game->enemy_hit_ticks = 0;
    game->enemy_respawn_ticks = 0;
    game->enemy_x = 390.0f;
    game->enemy_y = 226.0f;
    game->enemy_attack_cooldown_ticks =
        LAND_ENEMY_INITIAL_COOLDOWN_TICKS;
    game->enemy_round = 1;
    game->enemy_charge_level = 0;
    game->enemy_magazine =
        game->cast == LAND_CAST_CHUMRUNNER ? 3 : 0;
    game->enemy_reserve_ammo =
        game->cast == LAND_CAST_CHUMRUNNER ? 6 : 0;
    game->enemy_moving = false;
    clear_enemy_action(game);
    set_player_profile(game, true);
    game->training_generation++;
}

static void enter_training(land_state *game)
{
    char message[96];
    game->location = LAND_LOCATION_TRAINING;
    game->player_x = 42.0f;
    game->player_y = 226.0f;
    game->facing = LAND_FACING_RIGHT;
    game->attack_facing = LAND_FACING_RIGHT;
    game->player_moving = false;
    game->aiming = false;
    game->rook_reload_ticks = 0;
    game->nearest_actor = -1;
    clear_attack(game);
    reset_player_resources(game);
    spawn_training_enemy(game);
    (void)snprintf(message, sizeof message, "%s enters the range. Z: %s.",
                   land_actor_name(game, LAND_ACTOR_HERO),
                   game->cast == LAND_CAST_LEGEND ?
                   "LASER / RIFLE BUTT" :
                   (game->cast == LAND_CAST_CHUMRUNNER ?
                    "PREDATOR" :
                    (game->cast == LAND_CAST_FANTASY ?
                     "BRONZE BLADE" : "BENT BAT BASH")));
    set_toast(game, message);
}

static void return_to_social_room(land_state *game)
{
    game->location = LAND_LOCATION_SOCIAL;
    game->player_x = 448.0f;
    game->player_y = 238.0f;
    game->facing = LAND_FACING_LEFT;
    game->attack_facing = LAND_FACING_LEFT;
    game->player_moving = false;
    game->aiming = false;
    game->rook_reload_ticks = 0;
    game->nearest_actor = land_nearest_actor(game);
    game->enemy_kind = -1;
    game->enemy_hp = 0;
    game->enemy_max_hp = 0;
    game->enemy_defense = 0;
    game->enemy_hit_ticks = 0;
    game->enemy_respawn_ticks = 0;
    game->enemy_x = 0.0f;
    game->enemy_y = 0.0f;
    game->enemy_attack_cooldown_ticks = 0;
    game->enemy_round = 0;
    game->enemy_charge_level = 0;
    game->enemy_magazine = 0;
    game->enemy_reserve_ammo = 0;
    game->enemy_moving = false;
    clear_enemy_action(game);
    set_player_profile(game, true);
    clear_attack(game);
    set_toast(game, "Back with the crew. The training room is through the right exit.");
}

static void enter_conservatory(land_state *game)
{
    game->location = LAND_LOCATION_CONSERVATORY;
    game->player_y = LAND_FLOOR_TOP;
    game->facing = LAND_FACING_UP;
    game->attack_facing = LAND_FACING_UP;
    game->player_moving = false;
    game->aiming = false;
    game->nearest_actor = -1;
    game->toast_ticks = 0;
    clear_attack(game);
}

static void leave_conservatory(land_state *game)
{
    game->location = LAND_LOCATION_SOCIAL;
    game->player_x = (LAND_CONSERVATORY_DOOR_LEFT +
                      LAND_CONSERVATORY_DOOR_RIGHT) * 0.5f;
    game->player_y = LAND_FLOOR_TOP + 12.0f;
    game->facing = LAND_FACING_DOWN;
    game->attack_facing = LAND_FACING_DOWN;
    game->player_moving = false;
    game->nearest_actor = land_nearest_actor(game);
    set_toast(game, "Back with the crew. The plant keeps growing.");
}

static bool at_conservatory_door(const land_state *game)
{
    return game->player_x >= LAND_CONSERVATORY_DOOR_LEFT &&
           game->player_x <= LAND_CONSERVATORY_DOOR_RIGHT;
}

static const conversation *active_conversation(const land_state *game)
{
    int index;
    uint32_t bit;
    if (!game || !cast_valid(game->cast) ||
        game->conversation_actor < LAND_ACTOR_ALLY_1 ||
        game->conversation_actor > LAND_ACTOR_ALLY_3)
        return NULL;
    index = game->conversation_actor - LAND_ACTOR_ALLY_1;
    bit = UINT32_C(1) << (unsigned int)game->conversation_actor;
    return (game->talked_mask & bit) != 0u ?
           &REPEAT_CONVERSATIONS[game->cast][index] :
           &FIRST_CONVERSATIONS[game->cast][index];
}

static const dialogue_beat *current_beat(const land_state *game)
{
    const conversation *script = active_conversation(game);
    if (!script || game->dialogue_beat < 0 ||
        game->dialogue_beat >= script->count)
        return NULL;
    return &script->beats[game->dialogue_beat];
}

static void set_toast(land_state *game, const char *message)
{
    if (!game) return;
    (void)snprintf(game->toast, sizeof game->toast, "%s", message);
    game->toast_ticks = 180;
}

land_cast land_visible_cast(const land_state *game)
{
    if (!game) return LAND_CAST_LEGEND;
    return game->mode == LAND_MODE_CAST_SELECT && cast_valid(game->cast_cursor) ?
           game->cast_cursor : game->cast;
}

const char *land_cast_name(land_cast cast)
{
    return profile_for(cast)->title;
}

const char *land_cast_subtitle(land_cast cast)
{
    return profile_for(cast)->subtitle;
}

const char *land_cast_room_name(land_cast cast)
{
    return profile_for(cast)->room_name;
}

const char *land_attack_name(land_attack attack)
{
    switch (attack) {
    case LAND_ATTACK_KILIX_RIFLE_BUTT:
        return "RIFLE BUTT";
    case LAND_ATTACK_KILIX_LASER:
        return "BEAM EMITTER";
    case LAND_ATTACK_ROOK_PREDATOR:
        return "ARES PREDATOR";
    case LAND_ATTACK_ROOK_PISTOL_BUTT:
        return "PISTOL BUTT";
    case LAND_ATTACK_ARDEN_BRONZE_BLADE:
        return "BRONZE BLADE";
    case LAND_ATTACK_PIP_BENT_BAT:
        return "BENT BAT BASH";
    case LAND_ATTACK_NONE:
    default:
        return "STANDARD ATTACK";
    }
}

const char *land_enemy_name(const land_state *game)
{
    const enemy_profile *profile;
    if (!game) return "NO TARGET";
    profile = enemy_profile_for(game->cast, game->enemy_kind);
    return profile ? profile->name : "NO TARGET";
}

int land_enemy_visual_index(const land_state *game)
{
    const enemy_profile *profile;
    if (!game) return -1;
    profile = enemy_profile_for(game->cast, game->enemy_kind);
    return profile ? profile->visual : -1;
}

int land_pip_timing_grade(const land_state *game)
{
    int phase;
    int distance;
    int pattern;
    if (!game) return 0;
    pattern = land_pip_timing_pattern(game);
    if (pattern == 1) {
        phase = (int)(game->simulation_tick % UINT64_C(48));
        if (phase > 24) phase = 48 - phase;
        distance = phase > 12 ? phase - 12 : 12 - phase;
    } else if (pattern == 2) {
        int first;
        int second;
        phase = (int)(game->simulation_tick % UINT64_C(72));
        first = phase > 18 ? phase - 18 : 18 - phase;
        second = phase > 54 ? phase - 54 : 54 - phase;
        distance = first < second ? first : second;
    } else {
        phase = (int)(game->simulation_tick % UINT64_C(60));
        distance = phase > 30 ? phase - 30 : 30 - phase;
    }
    if (distance <= 3) return 2;
    if (distance <= 10) return 1;
    return 0;
}

int land_pip_timing_pattern(const land_state *game)
{
    const enemy_profile *profile;
    if (!game || game->cast != LAND_CAST_PLEB_BOUND) return 0;
    profile = enemy_profile_for(game->cast, game->enemy_kind);
    return profile ? profile->timing : 0;
}

const char *land_actor_name(const land_state *game, int actor)
{
    const cast_profile *profile = profile_for(land_visible_cast(game));
    return actor >= 0 && actor < LAND_ACTOR_COUNT ?
           profile->actor_names[actor] : "?";
}

uint32_t land_actor_color(const land_state *game, int actor)
{
    const cast_profile *profile = profile_for(land_visible_cast(game));
    return actor >= 0 && actor < LAND_ACTOR_COUNT ?
           profile->actor_colors[actor] : UINT32_C(0xffffff);
}

void land_actor_position(int actor, float *x, float *y)
{
    if (x) *x = actor >= LAND_ACTOR_ALLY_1 && actor < LAND_ACTOR_COUNT ?
              ACTOR_X[actor] : 0.0f;
    if (y) *y = actor >= LAND_ACTOR_ALLY_1 && actor < LAND_ACTOR_COUNT ?
              ACTOR_Y[actor] : 0.0f;
}

int land_nearest_actor(const land_state *game)
{
    float best_distance = 72.0f * 72.0f;
    int best = -1;
    int actor;
    if (!game || game->mode != LAND_MODE_ROOM ||
        game->location != LAND_LOCATION_SOCIAL)
        return -1;
    for (actor = LAND_ACTOR_ALLY_1; actor <= LAND_ACTOR_ALLY_3; ++actor) {
        float dx = game->player_x - ACTOR_X[actor];
        float dy = game->player_y - ACTOR_Y[actor];
        float distance = dx * dx + dy * dy;
        if (distance < best_distance) {
            best_distance = distance;
            best = actor;
        }
    }
    return best;
}

void land_game_init(land_state *game)
{
    if (!game) return;
    (void)memset(game, 0, sizeof *game);
    game->player_x = 240.0f;
    game->player_y = 242.0f;
    game->mode = LAND_MODE_CAST_SELECT;
    game->location = LAND_LOCATION_SOCIAL;
    game->cast = LAND_CAST_LEGEND;
    game->cast_cursor = LAND_CAST_LEGEND;
    game->facing = LAND_FACING_UP;
    game->attack_facing = LAND_FACING_UP;
    game->rng = UINT32_C(0x6c616e64);
    game->nearest_actor = -1;
    game->conversation_actor = -1;
    game->enemy_kind = -1;
    reset_player_resources(game);
    set_player_profile(game, true);
    clear_enemy_action(game);
}

void land_game_open_cast_select(land_state *game)
{
    if (!game || game->mode != LAND_MODE_ROOM || !game->cast_confirmed ||
        game->location == LAND_LOCATION_CONSERVATORY)
        return;
    game->cast_cursor = game->cast;
    game->mode = LAND_MODE_CAST_SELECT;
    game->aiming = false;
    game->player_moving = false;
    game->nearest_actor = -1;
}

void land_game_move_cast_cursor(land_state *game, int delta)
{
    int cursor;
    if (!game || game->mode != LAND_MODE_CAST_SELECT || delta == 0)
        return;
    cursor = (int)game->cast_cursor + (delta < 0 ? -1 : 1);
    if (cursor < 0) cursor = LAND_CAST_COUNT - 1;
    if (cursor >= LAND_CAST_COUNT) cursor = 0;
    game->cast_cursor = (land_cast)cursor;
}

bool land_game_confirm_cast(land_state *game)
{
    bool changed;
    char message[96];
    if (!game || game->mode != LAND_MODE_CAST_SELECT ||
        !cast_valid(game->cast_cursor))
        return false;
    changed = !game->cast_confirmed || game->cast != game->cast_cursor;
    game->cast = game->cast_cursor;
    game->cast_confirmed = true;
    game->mode = LAND_MODE_ROOM;
    game->location = LAND_LOCATION_SOCIAL;
    game->player_x = 240.0f;
    game->player_y = 242.0f;
    game->facing = LAND_FACING_UP;
    game->attack_facing = LAND_FACING_UP;
    game->player_moving = false;
    game->aiming = false;
    game->nearest_actor = land_nearest_actor(game);
    game->conversation_actor = -1;
    game->dialogue_beat = 0;
    game->dialogue_age = 0;
    game->enemy_kind = -1;
    game->enemy_hp = 0;
    game->enemy_max_hp = 0;
    game->enemy_defense = 0;
    game->enemy_hit_ticks = 0;
    game->enemy_respawn_ticks = 0;
    game->enemy_x = 0.0f;
    game->enemy_y = 0.0f;
    game->enemy_attack_cooldown_ticks = 0;
    game->enemy_round = 0;
    game->enemy_charge_level = 0;
    game->enemy_magazine = 0;
    game->enemy_reserve_ammo = 0;
    game->enemy_moving = false;
    game->training_generation = 0;
    game->training_wins = 0;
    game->training_losses = 0;
    reset_player_resources(game);
    set_player_profile(game, true);
    clear_enemy_action(game);
    clear_attack(game);
    if (changed) game->talked_mask = 0u;
    (void)snprintf(message, sizeof message,
                   "%s arrives with the whole crew. Walk over and press Enter.",
                   land_actor_name(game, LAND_ACTOR_HERO));
    set_toast(game, message);
    return true;
}

bool land_game_attack(land_state *game)
{
    int total_ticks;
    int impact_ticks;
    int cooldown_ticks;
    int damage = 1;
    land_attack attack;
    if (!game || game->mode != LAND_MODE_ROOM ||
        game->location != LAND_LOCATION_TRAINING ||
        game->player_hp <= 0 || game->player_recovery_ticks > 0 ||
        game->attack_cooldown_ticks > 0 ||
        game->rook_reload_ticks > 0)
        return false;

    if (!game->aiming)
        game->facing = LAND_FACING_RIGHT;
    game->attack_facing = game->facing;
    game->player_moving = false;
    game->pip_timing_grade = 0;
    game->attack_hit_chance = 100;
    game->attack_hit_roll = 0;
    game->attack_variance = 0;
    game->attack_critical_chance = 0;
    game->attack_critical_roll = 0;
    game->attack_critical = false;
    if (game->cast == LAND_CAST_LEGEND) {
        if (game->aiming && game->player_hp != game->player_max_hp) {
            set_toast(game,
                "Beam emitter needs full vitality. Lower it and use the rifle butt.");
            return false;
        }
        if (game->aiming && game->kilix_rifle_charge <= 0) {
            set_toast(game,
                "Beam emitter empty. Press X to lower it; Z then swings the rifle.");
            return false;
        }
        if (game->aiming) {
            attack = LAND_ATTACK_KILIX_LASER;
            total_ticks = LAND_PARITY_LEGEND_RIFLE_FIRE_TICKS;
            impact_ticks = 13;
            cooldown_ticks =
                LAND_PARITY_LEGEND_ATTACK_COOLDOWN_TICKS;
            damage = 1;
            game->attack_target_x = game->aim_x;
            game->attack_target_y = game->aim_y;
            game->attack_will_hit = land_game_target_locked(game);
            game->attack_hit_roll = 1;
            game->kilix_rifle_charge--;
        } else {
            attack = LAND_ATTACK_KILIX_RIFLE_BUTT;
            total_ticks = LAND_PARITY_LEGEND_RIFLE_BUTT_TICKS;
            impact_ticks = 19;
            cooldown_ticks =
                LAND_PARITY_LEGEND_ATTACK_COOLDOWN_TICKS;
            damage = 1;
            game->attack_target_x = land_enemy_target_x(game);
            game->attack_target_y = land_enemy_target_y(game);
            game->attack_will_hit = melee_target_in_range(game);
            game->attack_hit_roll = 1;
        }
    } else if (game->cast == LAND_CAST_CHUMRUNNER) {
        int distance;
        int raw_damage = 0;
        if (!game->aiming) {
            attack = LAND_ATTACK_ROOK_PISTOL_BUTT;
            total_ticks = LAND_PARITY_CHUM_MELEE_TICKS;
            impact_ticks = 7;
            cooldown_ticks = total_ticks;
            raw_damage =
                LAND_PARITY_CHUM_MELEE_BASE_DAMAGE +
                LAND_PARITY_CHUM_CLOSE_COMBAT_SCORE +
                game->enemy_defense / 2;
            damage = raw_damage - game->enemy_defense;
            if (damage < 1) damage = 1;
            game->attack_target_x = land_enemy_target_x(game);
            game->attack_target_y = land_enemy_target_y(game);
            game->attack_will_hit =
                enemy_available(game) && land_rook_range(game) == 1;
            game->attack_hit_roll = 1;
        } else {
            if (!enemy_available(game)) {
                set_toast(game,
                    "No active target. Hold aim while the next one arrives.");
                return false;
            }
            if (game->rook_magazine <= 0) {
                set_toast(game, "Predator empty. Press R to reload.");
                return false;
            }
            distance = land_rook_range(game);
            if (distance > LAND_PARITY_CHUM_PREDATOR_RANGE) {
                set_toast(game,
                    "Target outside Predator range. Lower aim and move closer.");
                return false;
            }
            attack = LAND_ATTACK_ROOK_PREDATOR;
            total_ticks = 18;
            impact_ticks = 10;
            cooldown_ticks = total_ticks;
            game->attack_hit_chance = land_rook_hit_chance(game);
            game->attack_hit_roll =
                1 + (int)(next_random(game) % UINT32_C(100));
            if (game->attack_hit_roll <= game->attack_hit_chance) {
                raw_damage = LAND_PARITY_CHUM_PREDATOR_RAW_MIN +
                             (int)(next_random(game) % UINT32_C(4));
                damage = raw_damage - game->enemy_defense;
                if (damage < 1) damage = 1;
            } else {
                damage = 0;
            }
            game->attack_target_x = game->aim_x;
            game->attack_target_y = game->aim_y;
            game->attack_will_hit =
                land_game_target_locked(game) &&
                game->attack_hit_roll <= game->attack_hit_chance;
            game->rook_magazine--;
        }
    } else if (game->cast == LAND_CAST_FANTASY) {
        uint32_t roll = next_random(game);
        int base_damage =
            LAND_PARITY_FANTASY_STANDARD_POWER +
            LAND_PARITY_FANTASY_PLAYER_LEVEL * 2 +
            LAND_PARITY_FANTASY_PLAYER_ATTACK -
            game->enemy_defense;
        int multiplier = 95 + (int)(roll % UINT32_C(11));
        if (base_damage < 1) base_damage = 1;
        attack = LAND_ATTACK_ARDEN_BRONZE_BLADE;
        total_ticks = 20;
        impact_ticks = 11;
        cooldown_ticks = total_ticks;
        game->attack_variance = multiplier - 100;
        damage = base_damage * multiplier / 100;
        if (damage < 1) damage = 1;
        game->attack_critical_chance = 1;
        game->attack_critical_roll =
            (int)(roll & UINT32_C(15));
        game->attack_critical =
            game->attack_critical_roll == 0;
        if (game->attack_critical)
            damage = damage * 3 / 2;
        game->attack_target_x = land_enemy_target_x(game);
        game->attack_target_y = land_enemy_target_y(game);
        game->attack_will_hit = melee_target_in_range(game);
    } else {
        const enemy_profile *profile =
            enemy_profile_for(game->cast, game->enemy_kind);
        uint32_t hit_random = next_random(game);
        uint32_t variance_random = next_random(game);
        uint32_t critical_random = next_random(game);
        int varied;
        game->pip_timing_grade = land_pip_timing_grade(game);
        attack = LAND_ATTACK_PIP_BENT_BAT;
        total_ticks = 20;
        impact_ticks = 10;
        cooldown_ticks = total_ticks;
        damage = LAND_PARITY_PLEB_BASH_BASE +
                 game->pip_timing_grade * 4 - game->enemy_defense;
        if (damage < 1) damage = 1;
        game->attack_hit_chance =
            game->pip_timing_grade == 2 ? 100 :
            clamp_int(
                LAND_PARITY_PLEB_ACCURACY +
                game->pip_timing_grade * 6 -
                (profile ? profile->evasion : 0),
                35, 100);
        game->attack_hit_roll =
            (int)(hit_random % UINT32_C(100));
        game->attack_variance =
            (int)(variance_random % UINT32_C(5)) - 2;
        varied = damage + game->attack_variance;
        if (varied < 1) varied = 1;
        game->attack_critical_chance =
            game->pip_timing_grade == 2 ? 100 :
            clamp_int(
                LAND_PARITY_PLEB_CRITICAL_BASE +
                game->pip_timing_grade * 8,
                5, 35);
        game->attack_critical_roll =
            (int)(critical_random % UINT32_C(100));
        game->attack_critical =
            game->attack_hit_roll < game->attack_hit_chance &&
            game->attack_critical_roll <
                game->attack_critical_chance;
        damage = game->attack_hit_roll < game->attack_hit_chance ?
                 varied : 0;
        if (game->attack_critical)
            damage = damage * 3 / 2;
        game->attack_target_x = land_enemy_target_x(game);
        game->attack_target_y = land_enemy_target_y(game);
        game->attack_will_hit =
            melee_target_in_range(game) &&
            game->attack_hit_roll < game->attack_hit_chance;
    }
    game->attack = attack;
    game->attack_ticks = total_ticks;
    game->attack_total_ticks = total_ticks;
    game->attack_impact_ticks = impact_ticks;
    game->attack_cooldown_ticks = cooldown_ticks;
    game->attack_damage = damage;
    game->attack_landed = false;
    return true;
}

bool land_game_reload(land_state *game)
{
    int needed;
    int loaded;
    char message[96];
    if (!game || game->cast != LAND_CAST_CHUMRUNNER ||
        game->mode != LAND_MODE_ROOM ||
        game->location != LAND_LOCATION_TRAINING ||
        game->player_hp <= 0 || game->player_recovery_ticks > 0 ||
        game->attack_ticks > 0 || game->rook_reload_ticks > 0)
        return false;
    needed = LAND_PARITY_CHUM_PREDATOR_MAGAZINE -
             game->rook_magazine;
    if (needed <= 0) {
        set_toast(game, "Predator magazine is already full.");
        return false;
    }
    if (game->rook_reserve_ammo <= 0) {
        set_toast(game, "No heavy-pistol rounds remain in reserve.");
        return false;
    }
    loaded = needed < game->rook_reserve_ammo ?
             needed : game->rook_reserve_ammo;
    game->rook_magazine += loaded;
    game->rook_reserve_ammo -= loaded;
    game->rook_reload_ticks = 30;
    game->attack_cooldown_ticks = 30;
    game->aiming = false;
    game->player_moving = false;
    (void)snprintf(message, sizeof message,
                   "Rook reloads %d round%s; %d remain in reserve.",
                   loaded, loaded == 1 ? "" : "s",
                   game->rook_reserve_ammo);
    set_toast(game, message);
    return true;
}

static void apply_attack_impact(land_state *game)
{
    char message[96];
    if (!game || game->attack_landed) return;
    game->attack_landed = true;
    if (!game->attack_will_hit || game->enemy_hp <= 0) {
        if (game->attack == LAND_ATTACK_ROOK_PREDATOR &&
            game->attack_hit_roll > game->attack_hit_chance) {
            (void)snprintf(message, sizeof message,
                           "Predator misses: roll %d exceeds HIT %d%%.",
                           game->attack_hit_roll,
                           game->attack_hit_chance);
            set_toast(game, message);
        } else if (game->attack == LAND_ATTACK_PIP_BENT_BAT &&
                   game->attack_hit_roll >=
                       game->attack_hit_chance) {
            (void)snprintf(message, sizeof message,
                           "Bash misses: roll %d against ACC %d%%.",
                           game->attack_hit_roll,
                           game->attack_hit_chance);
            set_toast(game, message);
        } else if (game->attack == LAND_ATTACK_KILIX_LASER ||
                   game->attack == LAND_ATTACK_ROOK_PREDATOR)
            set_toast(game,
                "Shot missed. Move the reticle until the target locks green.");
        else
            set_toast(game, "Good swing. Move closer for the weapon to connect.");
        return;
    }
    game->enemy_hp -= game->attack_damage;
    if (game->enemy_hp < 0) game->enemy_hp = 0;
    game->enemy_hit_ticks = 6;
    if (game->enemy_hp == 0) {
        clear_enemy_action(game);
        game->enemy_attack_cooldown_ticks = 0;
        game->enemy_moving = false;
        game->training_wins++;
        game->enemy_respawn_ticks = LAND_ENEMY_RESPAWN_TICKS;
        (void)snprintf(message, sizeof message,
                       "%s cleared. A new training target is incoming.",
                       land_enemy_name(game));
    } else {
        if (game->attack_critical)
            (void)snprintf(message, sizeof message,
                           "%s CRITICAL for %d. %s: %d/%d.",
                           land_attack_name(game->attack),
                           game->attack_damage,
                           land_enemy_name(game), game->enemy_hp,
                           game->enemy_max_hp);
        else
            (void)snprintf(message, sizeof message,
                           "%s lands for %d. %s: %d/%d.",
                           land_attack_name(game->attack),
                           game->attack_damage,
                           land_enemy_name(game), game->enemy_hp,
                           game->enemy_max_hp);
    }
    set_toast(game, message);
}

static float approach_float(float value, float target, float amount)
{
    if (value < target) {
        value += amount;
        return value > target ? target : value;
    }
    if (value > target) {
        value -= amount;
        return value < target ? target : value;
    }
    return value;
}

static int enemy_recovery_ticks(const land_state *game,
                                const enemy_profile *profile)
{
    int ticks;
    if (game->cast == LAND_CAST_PLEB_BOUND) {
        ticks = 104 - profile->speed * 3;
        return ticks < 48 ? 48 : ticks;
    }
    if (game->cast == LAND_CAST_LEGEND) return 72;
    if (game->cast == LAND_CAST_CHUMRUNNER) return 68;
    return 76;
}

static int fantasy_weighted_power(land_state *game,
                                  const enemy_profile *profile)
{
    static const int weights[5][3] = {
        {70, 20, 10},
        {45, 40, 15},
        {55, 30, 15},
        {40, 35, 25},
        {30, 30, 40}
    };
    int power = profile->attack;
    int pattern = clamp_int(profile->ai, 0, 4);
    int selected = (int)(next_random(game) % UINT32_C(100));
    if (selected >= weights[pattern][0]) {
        power += selected <
                 weights[pattern][0] + weights[pattern][1] ? 2 : 4;
    }
    if (game->enemy_hp <=
        (game->enemy_max_hp > 4 ? game->enemy_max_hp / 4 : 1))
        power = profile->attack +
                (profile->level / 2 > 5 ?
                 profile->level / 2 : 5);
    return power;
}

static void start_enemy_action(land_state *game,
                               const enemy_profile *profile)
{
    int raw_damage = profile ? profile->attack : 1;
    int fantasy_power = 0;
    int damage = 1;
    char message[96];
    if (!game || !profile || !enemy_available(game) ||
        game->player_recovery_ticks > 0)
        return;

    game->enemy_attack_hit_chance = 100;
    game->enemy_attack_hit_roll = 0;
    game->enemy_attack_variance = 0;
    game->enemy_attack_hits_planned = 1;
    game->enemy_attack_hits_landed = 1;
    game->enemy_attack_critical = false;
    if (game->cast == LAND_CAST_FANTASY)
        fantasy_power = fantasy_weighted_power(game, profile);

    if (game->cast == LAND_CAST_CHUMRUNNER &&
        game->enemy_magazine <= 0) {
        int loaded = game->enemy_reserve_ammo < 3 ?
                     game->enemy_reserve_ammo : 3;
        if (loaded <= 0) {
            game->enemy_hp = 0;
            game->enemy_respawn_ticks = LAND_ENEMY_RESPAWN_TICKS;
            game->training_wins++;
            set_toast(game,
                "The hostile is out of ammunition and withdraws.");
            return;
        }
        game->enemy_magazine = loaded;
        game->enemy_reserve_ammo -= loaded;
        game->enemy_action = LAND_ENEMY_ACTION_RELOAD;
        game->enemy_attack_ticks = 30;
        game->enemy_attack_total_ticks = 30;
        game->enemy_attack_impact_ticks = 0;
        game->enemy_attack_damage = 0;
        game->enemy_attack_landed = true;
        game->enemy_attack_hit = false;
        game->enemy_attack_hits_planned = 0;
        game->enemy_attack_hits_landed = 0;
        game->enemy_attack_ranged = true;
        game->enemy_round++;
        (void)snprintf(message, sizeof message,
                       "%s reloads %d round%s; %d in reserve.",
                       land_enemy_name(game), loaded,
                       loaded == 1 ? "" : "s",
                       game->enemy_reserve_ammo);
        set_toast(game, message);
        return;
    }

    /*
     * Kilix Fantasy AI pattern 3 guards on non-third rounds and releases
     * its stored +9 strike after charging. Its low-HP rule overrides guard.
     */
    if (game->cast == LAND_CAST_FANTASY && profile->ai == 3 &&
        game->enemy_round % 3 != 0 &&
        game->enemy_hp >
            (game->enemy_max_hp > 4 ? game->enemy_max_hp / 4 : 1)) {
        game->enemy_action = LAND_ENEMY_ACTION_CHARGE;
        game->enemy_attack_ticks = 30;
        game->enemy_attack_total_ticks = 30;
        game->enemy_attack_impact_ticks = 0;
        game->enemy_attack_damage = 0;
        game->enemy_attack_landed = true;
        game->enemy_attack_hit = false;
        game->enemy_attack_ranged = false;
        game->enemy_charge_level = 1;
        game->enemy_round++;
        (void)snprintf(message, sizeof message,
                       "%s gathers force behind its guard.",
                       land_enemy_name(game));
        set_toast(game, message);
        return;
    }

    if (game->cast == LAND_CAST_CHUMRUNNER) {
        raw_damage += (int)(next_random(game) % UINT32_C(4));
        damage = raw_damage - game->player_defense;
        if (damage < 1) damage = 1;
        game->enemy_magazine--;
    } else if (game->cast == LAND_CAST_FANTASY) {
        int phase = game->enemy_charge_level;
        uint32_t roll;
        raw_damage = fantasy_power;
        if (profile->ai == 3 && phase != 0)
            raw_damage += 9;
        raw_damage += phase * 2 + game->enemy_round / 4;
        damage = raw_damage - game->player_defense;
        if (damage < 1) damage = 1;
        roll = next_random(game);
        game->enemy_attack_variance =
            (int)(roll % UINT32_C(11)) - 5;
        damage = damage *
                 (100 + game->enemy_attack_variance) / 100;
        if (damage < 1) damage = 1;
        game->enemy_attack_critical =
            (roll & UINT32_C(15)) == 0u;
        if (game->enemy_attack_critical)
            damage = damage * 3 / 2;
        if (phase != 0) {
            game->enemy_charge_level = 0;
        }
    } else if (game->cast == LAND_CAST_PLEB_BOUND) {
        int hit;
        int base_damage =
            profile->attack -
            (profile->ability ?
             game->player_defense / 2 : game->player_defense);
        int total_damage = 0;
        int total_variance = 0;
        if (base_damage < 1) base_damage = 1;
        game->enemy_attack_hit_chance =
            clamp_int(profile->accuracy -
                      LAND_PARITY_PLEB_EVASION, 25, 100);
        game->enemy_attack_hits_planned = profile->hits;
        game->enemy_attack_hits_landed = 0;
        for (hit = 0; hit < profile->hits; ++hit) {
            uint32_t hit_random = next_random(game);
            uint32_t variance_random = next_random(game);
            int variance = 0;
            if (hit == 0)
                game->enemy_attack_hit_roll =
                    (int)(hit_random % UINT32_C(100));
            if ((int)(hit_random % UINT32_C(100)) <
                game->enemy_attack_hit_chance) {
                int one_damage;
                if (profile->variance > 0) {
                    uint32_t width =
                        (uint32_t)(profile->variance * 2 + 1);
                    variance = (int)(variance_random % width) -
                               profile->variance;
                }
                one_damage = base_damage + variance;
                if (one_damage < 1) one_damage = 1;
                total_damage += one_damage;
                total_variance += variance;
                game->enemy_attack_hits_landed++;
            }
            /* Status and nerve rolls are still consumed by the source game. */
            (void)next_random(game);
            (void)next_random(game);
        }
        game->enemy_attack_variance = total_variance;
        damage = total_damage;
    } else {
        damage = raw_damage - game->player_defense;
        if (damage < 1) damage = 1;
    }

    game->enemy_action = LAND_ENEMY_ACTION_ATTACK;
    game->enemy_attack_total_ticks = profile->ranged ? 42 : 34;
    game->enemy_attack_ticks = game->enemy_attack_total_ticks;
    game->enemy_attack_impact_ticks = profile->ranged ? 12 : 9;
    game->enemy_attack_damage = damage;
    game->enemy_attack_landed = false;
    game->enemy_attack_hit = false;
    game->enemy_attack_ranged = profile->ranged;
    game->enemy_attack_target_x = game->player_x;
    game->enemy_attack_target_y = game->player_y - 34.0f;
    game->enemy_round++;
}

static void apply_enemy_impact(land_state *game)
{
    float dx;
    float dy;
    bool spatial_hit;
    bool hit;
    char message[96];
    if (!game || game->enemy_attack_landed ||
        game->enemy_action != LAND_ENEMY_ACTION_ATTACK)
        return;
    game->enemy_attack_landed = true;
    if (game->enemy_attack_ranged) {
        dx = game->player_x - game->enemy_attack_target_x;
        dy = (game->player_y - 34.0f) - game->enemy_attack_target_y;
        if (dx < 0.0f) dx = -dx;
        if (dy < 0.0f) dy = -dy;
        spatial_hit = dx <= 30.0f && dy <= 38.0f;
    } else {
        dx = game->enemy_x - game->player_x;
        dy = game->enemy_y - game->player_y;
        if (dx < 0.0f) dx = -dx;
        if (dy < 0.0f) dy = -dy;
        spatial_hit = dx <= 108.0f && dy <= 52.0f;
    }
    hit = spatial_hit && game->enemy_attack_hits_landed > 0;
    game->enemy_attack_hit = hit;
    if (!hit) {
        if (!spatial_hit)
            (void)snprintf(message, sizeof message,
                           "%s commits, but the telegraphed attack misses.",
                           land_enemy_name(game));
        else
            (void)snprintf(
                message, sizeof message,
                "%s misses: %d/%d source hit rolls land.",
                land_enemy_name(game),
                game->enemy_attack_hits_landed,
                game->enemy_attack_hits_planned);
        set_toast(game, message);
        return;
    }

    game->player_hp -= game->enemy_attack_damage;
    if (game->player_hp < 0) game->player_hp = 0;
    game->player_hit_ticks = 10;
    if (game->player_hp == 0) {
        game->training_losses++;
        game->player_recovery_ticks = LAND_PLAYER_RECOVERY_TICKS;
        game->aiming = false;
        game->player_moving = false;
        clear_attack(game);
        (void)snprintf(message, sizeof message,
                       "%s is down. Training reset in a moment.",
                       land_actor_name(game, LAND_ACTOR_HERO));
    } else {
        if (game->enemy_attack_critical)
            (void)snprintf(message, sizeof message,
                           "%s CRITICAL for %d. %s: %d/%d HP.",
                           land_enemy_name(game),
                           game->enemy_attack_damage,
                           land_actor_name(game, LAND_ACTOR_HERO),
                           game->player_hp, game->player_max_hp);
        else if (game->enemy_attack_hits_planned > 1)
            (void)snprintf(message, sizeof message,
                           "%s lands %d/%d hits for %d. %s: %d/%d.",
                           land_enemy_name(game),
                           game->enemy_attack_hits_landed,
                           game->enemy_attack_hits_planned,
                           game->enemy_attack_damage,
                           land_actor_name(game, LAND_ACTOR_HERO),
                           game->player_hp, game->player_max_hp);
        else
            (void)snprintf(message, sizeof message,
                           "%s attacks for %d. %s: %d/%d HP.",
                           land_enemy_name(game),
                           game->enemy_attack_damage,
                           land_actor_name(game, LAND_ACTOR_HERO),
                           game->player_hp, game->player_max_hp);
    }
    set_toast(game, message);
}

static void recover_training_player(land_state *game)
{
    if (!game) return;
    set_player_profile(game, true);
    reset_player_resources(game);
    game->player_x = 42.0f;
    game->player_y = 226.0f;
    game->facing = LAND_FACING_RIGHT;
    game->attack_facing = LAND_FACING_RIGHT;
    game->player_hit_ticks = 0;
    game->enemy_hp = game->enemy_max_hp;
    game->enemy_hit_ticks = 0;
    game->enemy_respawn_ticks = 0;
    game->enemy_x = 390.0f;
    game->enemy_y = 226.0f;
    game->enemy_round = 1;
    game->enemy_charge_level = 0;
    game->enemy_magazine =
        game->cast == LAND_CAST_CHUMRUNNER ? 3 : 0;
    game->enemy_reserve_ammo =
        game->cast == LAND_CAST_CHUMRUNNER ? 6 : 0;
    game->enemy_attack_cooldown_ticks =
        LAND_ENEMY_INITIAL_COOLDOWN_TICKS;
    game->enemy_moving = false;
    clear_enemy_action(game);
    set_toast(game, "Training reset. Read the warning marker and move before impact.");
}

static void update_training_enemy(land_state *game, float seconds)
{
    const enemy_profile *profile;
    float movement;
    if (!game || game->location != LAND_LOCATION_TRAINING ||
        !enemy_available(game) || game->player_recovery_ticks > 0)
        return;
    profile = enemy_profile_for(game->cast, game->enemy_kind);
    if (!profile) return;

    game->enemy_moving = false;
    if (game->enemy_action == LAND_ENEMY_ACTION_IDLE) {
        movement = (float)(game->cast == LAND_CAST_PLEB_BOUND ?
                   22 + profile->speed * 2 : profile->speed) * seconds;
        if (game->cast == LAND_CAST_CHUMRUNNER) {
            int distance = land_rook_range(game);
            float preferred_x =
                game->player_x +
                (float)profile->preferred_range * 56.0f;
            float old_x = game->enemy_x;
            float old_y = game->enemy_y;
            if (preferred_x > 390.0f) preferred_x = 390.0f;
            if (preferred_x < 96.0f) preferred_x = 96.0f;
            if (distance > profile->range ||
                distance < profile->preferred_range)
                game->enemy_x = approach_float(
                    game->enemy_x, preferred_x, movement);
            game->enemy_y = approach_float(
                game->enemy_y, game->player_y, movement * 0.55f);
            game->enemy_moving =
                game->enemy_x != old_x || game->enemy_y != old_y;
        } else if (!profile->ranged) {
            float target_x = game->player_x + 84.0f;
            float old_x = game->enemy_x;
            float old_y = game->enemy_y;
            if (target_x > 390.0f) target_x = 390.0f;
            game->enemy_x = approach_float(game->enemy_x, target_x, movement);
            game->enemy_y = approach_float(
                game->enemy_y, game->player_y, movement * 0.72f);
            game->enemy_moving =
                game->enemy_x != old_x || game->enemy_y != old_y;
        } else {
            float patrol_y =
                (game->simulation_tick / UINT64_C(90)) % UINT64_C(2) == 0u ?
                214.0f : 238.0f;
            float old_y = game->enemy_y;
            game->enemy_y = approach_float(
                game->enemy_y, patrol_y, movement * 0.32f);
            game->enemy_moving = game->enemy_y != old_y;
        }
    }

    if (game->enemy_action != LAND_ENEMY_ACTION_IDLE) {
        if (game->enemy_attack_ticks > 0)
            game->enemy_attack_ticks--;
        if (game->enemy_action == LAND_ENEMY_ACTION_ATTACK &&
            !game->enemy_attack_landed &&
            game->enemy_attack_ticks <= game->enemy_attack_impact_ticks)
            apply_enemy_impact(game);
        if (game->enemy_attack_ticks == 0) {
            bool charged = game->enemy_action == LAND_ENEMY_ACTION_CHARGE;
            clear_enemy_action(game);
            game->enemy_attack_cooldown_ticks =
                charged ? 36 : enemy_recovery_ticks(game, profile);
        }
        return;
    }

    if (game->enemy_attack_cooldown_ticks > 0) {
        game->enemy_attack_cooldown_ticks--;
        return;
    }
    if ((game->cast == LAND_CAST_CHUMRUNNER &&
         land_rook_range(game) <= profile->range) ||
        (game->cast != LAND_CAST_CHUMRUNNER &&
         (profile->ranged || melee_target_in_range(game))))
        start_enemy_action(game, profile);
}

void land_game_update(land_state *game, int move_x, int move_y, float seconds)
{
    if (!game || !isfinite(seconds) || seconds <= 0.0f || seconds > 0.25f)
        return;
    game->player_moving = false;
    if (game->location == LAND_LOCATION_CONSERVATORY) {
        /* The plant owns the canvas and the keys in here; Land keeps only
         * its clock running so presentation and toasts stay in step. */
        game->simulation_tick += 1u;
        if (game->toast_ticks > 0) game->toast_ticks--;
        return;
    }
    if (game->mode == LAND_MODE_ROOM) {
        if (game->player_recovery_ticks > 0) {
            game->aiming = false;
            game->player_moving = false;
        } else if (game->aiming && ranged_cast(game->cast)) {
            float dx;
            float dy;
            float aim_speed =
                game->cast == LAND_CAST_LEGEND ?
                LAND_LEGEND_AIM_SPEED : LAND_CHUM_AIM_SPEED;
            if (game->attack_ticks == 0) {
                if (move_x < 0)
                    game->aim_x -= aim_speed * seconds;
                else if (move_x > 0)
                    game->aim_x += aim_speed * seconds;
                if (move_y < 0)
                    game->aim_y -= aim_speed * seconds;
                else if (move_y > 0)
                    game->aim_y += aim_speed * seconds;
            }
            if (game->aim_x < LAND_AIM_LEFT) game->aim_x = LAND_AIM_LEFT;
            if (game->aim_x > LAND_AIM_RIGHT)
                game->aim_x = LAND_AIM_RIGHT;
            if (game->aim_y < LAND_AIM_TOP) game->aim_y = LAND_AIM_TOP;
            if (game->aim_y > LAND_AIM_BOTTOM)
                game->aim_y = LAND_AIM_BOTTOM;
            dx = game->aim_x - ranged_muzzle_x(game);
            dy = game->aim_y - ranged_muzzle_y(game);
            if (fabsf(dx) >= fabsf(dy))
                game->facing = dx < 0.0f ?
                    LAND_FACING_LEFT : LAND_FACING_RIGHT;
            else
                game->facing = dy < 0.0f ?
                    LAND_FACING_UP : LAND_FACING_DOWN;
            game->attack_facing = game->facing;
        } else if (game->attack_ticks > 0) {
            game->player_moving = false;
        } else if (move_x < 0) {
            game->facing = LAND_FACING_LEFT;
            game->player_x -= LAND_PLAYER_SPEED * seconds;
            game->player_moving = true;
        } else if (move_x > 0) {
            game->facing = LAND_FACING_RIGHT;
            game->player_x += LAND_PLAYER_SPEED * seconds;
            game->player_moving = true;
        } else if (move_y < 0) {
            game->facing = LAND_FACING_UP;
            game->player_y -= LAND_PLAYER_SPEED * seconds;
            game->player_moving = true;
        } else if (move_y > 0) {
            game->facing = LAND_FACING_DOWN;
            game->player_y += LAND_PLAYER_SPEED * seconds;
            game->player_moving = true;
        }
        if (game->location == LAND_LOCATION_SOCIAL) {
            if (game->player_x >= LAND_SOCIAL_EXIT_RIGHT &&
                move_x > 0) {
                enter_training(game);
            } else if (game->player_y <= LAND_FLOOR_TOP && move_y < 0 &&
                       game->player_recovery_ticks == 0 &&
                       at_conservatory_door(game)) {
                enter_conservatory(game);
            } else {
                if (game->player_x < LAND_SOCIAL_LEFT)
                    game->player_x = LAND_SOCIAL_LEFT;
                if (game->player_x > LAND_SOCIAL_EXIT_RIGHT)
                    game->player_x = LAND_SOCIAL_EXIT_RIGHT;
            }
        } else if (game->player_recovery_ticks == 0 &&
                   game->player_x <= LAND_TRAINING_EXIT_LEFT &&
                   move_x < 0) {
            return_to_social_room(game);
        } else {
            if (game->player_x < LAND_TRAINING_EXIT_LEFT)
                game->player_x = LAND_TRAINING_EXIT_LEFT;
            if (game->player_x > LAND_TRAINING_RIGHT)
                game->player_x = LAND_TRAINING_RIGHT;
        }
        if (game->player_y < LAND_FLOOR_TOP)
            game->player_y = LAND_FLOOR_TOP;
        if (game->player_y > LAND_FLOOR_BOTTOM)
            game->player_y = LAND_FLOOR_BOTTOM;
    }
    if (game->mode == LAND_MODE_ROOM) {
        if (game->attack_cooldown_ticks > 0)
            game->attack_cooldown_ticks--;
        if (game->rook_reload_ticks > 0)
            game->rook_reload_ticks--;
        if (game->attack_ticks > 0) {
            game->attack_ticks--;
            if (!game->attack_landed &&
                game->attack_ticks <= game->attack_impact_ticks)
                apply_attack_impact(game);
            if (game->attack_ticks == 0)
                game->attack = LAND_ATTACK_NONE;
        }
        if (game->enemy_hit_ticks > 0) game->enemy_hit_ticks--;
        if (game->player_hit_ticks > 0) game->player_hit_ticks--;
        if (game->player_recovery_ticks > 0) {
            game->player_recovery_ticks--;
            if (game->player_recovery_ticks == 0)
                recover_training_player(game);
        }
        if (game->location == LAND_LOCATION_TRAINING &&
            game->enemy_respawn_ticks > 0) {
            game->enemy_respawn_ticks--;
            if (game->enemy_respawn_ticks == 0) {
                spawn_training_enemy(game);
                set_toast(game, "New source-game target ready.");
            }
        }
        update_training_enemy(game, seconds);
    }
    game->simulation_tick += 1u;
    if (game->toast_ticks > 0) game->toast_ticks--;
    if (game->mode == LAND_MODE_DIALOGUE) game->dialogue_age++;
    game->nearest_actor = land_nearest_actor(game);
}

size_t land_dialogue_visible_chars(const land_state *game)
{
    const char *text;
    size_t length;
    size_t visible;
    if (!game || game->mode != LAND_MODE_DIALOGUE) return 0u;
    text = land_dialogue_text(game);
    length = strlen(text);
    visible = game->dialogue_age > 0 ? (size_t)game->dialogue_age / 2u : 0u;
    return visible < length ? visible : length;
}

bool land_game_interact(land_state *game)
{
    const conversation *script;
    const char *text;
    size_t length;
    if (!game) return false;
    if (game->mode == LAND_MODE_CAST_SELECT)
        return land_game_confirm_cast(game);
    if (game->location == LAND_LOCATION_CONSERVATORY) return false;
    if (game->mode == LAND_MODE_ROOM) {
        int actor = land_nearest_actor(game);
        if (game->location == LAND_LOCATION_TRAINING) {
            set_toast(game,
                ranged_cast(game->cast) ?
                "Press X for aim mode, move the reticle, then press Z." :
                "Press Z to swing anywhere; move close enough to connect.");
            return false;
        }
        if (actor < LAND_ACTOR_ALLY_1) {
            char message[96];
            (void)snprintf(message, sizeof message,
                           "Move closer to %s, %s, or %s to talk.",
                           land_actor_name(game, LAND_ACTOR_ALLY_1),
                           land_actor_name(game, LAND_ACTOR_ALLY_2),
                           land_actor_name(game, LAND_ACTOR_ALLY_3));
            set_toast(game, message);
            return false;
        }
        game->mode = LAND_MODE_DIALOGUE;
        game->conversation_actor = actor;
        game->dialogue_beat = 0;
        game->dialogue_age = 0;
        game->player_moving = false;
        game->nearest_actor = -1;
        return true;
    }
    text = land_dialogue_text(game);
    length = strlen(text);
    if (land_dialogue_visible_chars(game) < length) {
        game->dialogue_age = (int)(length * 2u);
        return true;
    }
    script = active_conversation(game);
    if (!script) {
        land_game_cancel(game);
        return false;
    }
    game->dialogue_beat++;
    game->dialogue_age = 0;
    if (game->dialogue_beat >= script->count) {
        uint32_t bit = UINT32_C(1) << (unsigned int)game->conversation_actor;
        game->talked_mask |= bit;
        game->mode = LAND_MODE_ROOM;
        game->conversation_actor = -1;
        game->dialogue_beat = 0;
        set_toast(game, "The room settles into an easy quiet. Press C to change crews.");
    }
    return true;
}

void land_game_cancel(land_state *game)
{
    if (!game) return;
    if (game->aiming) {
        game->aiming = false;
        set_toast(game, "Aim mode cancelled.");
        return;
    }
    if (game->mode == LAND_MODE_CAST_SELECT && game->cast_confirmed) {
        game->cast_cursor = game->cast;
        game->mode = LAND_MODE_ROOM;
        game->nearest_actor = land_nearest_actor(game);
        return;
    }
    if (game->mode == LAND_MODE_ROOM &&
        game->location == LAND_LOCATION_TRAINING) {
        return_to_social_room(game);
        return;
    }
    if (game->location == LAND_LOCATION_CONSERVATORY) {
        leave_conservatory(game);
        return;
    }
    if (game->mode != LAND_MODE_DIALOGUE) return;
    game->mode = LAND_MODE_ROOM;
    game->conversation_actor = -1;
    game->dialogue_beat = 0;
    game->dialogue_age = 0;
    set_toast(game, "Conversation paused. You can return whenever you like.");
}

const char *land_dialogue_text(const land_state *game)
{
    const dialogue_beat *beat = current_beat(game);
    return beat ? beat->text : "";
}

int land_dialogue_speaker(const land_state *game)
{
    const dialogue_beat *beat = current_beat(game);
    return beat ? (int)beat->speaker : (int)LAND_ACTOR_HERO;
}

int land_dialogue_expression(const land_state *game)
{
    const dialogue_beat *beat = current_beat(game);
    return beat ? beat->expression : 0;
}

int land_dialogue_count(const land_state *game)
{
    const conversation *script = active_conversation(game);
    return script ? script->count : 0;
}

bool land_game_validate(const land_state *game, char *error, size_t error_size)
{
    const char *message = NULL;
    if (!game) message = "state is null";
    else if (game->location < LAND_LOCATION_SOCIAL ||
             game->location > LAND_LOCATION_CONSERVATORY)
        message = "location is invalid";
    else if (game->location == LAND_LOCATION_CONSERVATORY &&
             (game->mode != LAND_MODE_ROOM ||
              !at_conservatory_door(game) ||
              game->player_y != LAND_FLOOR_TOP))
        message = "conservatory visit is invalid";
    else if ((game->location == LAND_LOCATION_SOCIAL &&
              (game->player_x < LAND_SOCIAL_LEFT ||
               game->player_x > LAND_SOCIAL_EXIT_RIGHT)) ||
             (game->location == LAND_LOCATION_TRAINING &&
              (game->player_x < LAND_TRAINING_EXIT_LEFT ||
               game->player_x > LAND_TRAINING_RIGHT)) ||
             game->player_y < LAND_FLOOR_TOP ||
             game->player_y > LAND_FLOOR_BOTTOM)
        message = "player left the room floor";
    else if (game->mode < LAND_MODE_ROOM ||
             game->mode > LAND_MODE_CAST_SELECT)
        message = "mode is invalid";
    else if (!cast_valid(game->cast) || !cast_valid(game->cast_cursor))
        message = "cast is invalid";
    else if (game->facing < LAND_FACING_DOWN || game->facing > LAND_FACING_UP)
        message = "facing is invalid";
    else if (game->attack_facing < LAND_FACING_DOWN ||
             game->attack_facing > LAND_FACING_UP)
        message = "attack facing is invalid";
    else if (game->aiming &&
             (game->location != LAND_LOCATION_TRAINING ||
              !ranged_cast(game->cast) || !isfinite(game->aim_x) ||
              !isfinite(game->aim_y) ||
              game->aim_x < LAND_AIM_LEFT ||
              game->aim_x > LAND_AIM_RIGHT ||
              game->aim_y < LAND_AIM_TOP ||
              game->aim_y > LAND_AIM_BOTTOM))
        message = "aim state is invalid";
    else if (game->attack < LAND_ATTACK_NONE ||
             game->attack > LAND_ATTACK_PIP_BENT_BAT ||
             game->attack_ticks < 0 ||
             game->attack_total_ticks < game->attack_ticks ||
             game->attack_cooldown_ticks < 0 ||
             game->attack_hit_chance < 0 ||
             game->attack_hit_chance > 100 ||
             game->attack_hit_roll < 0 ||
             game->attack_hit_roll > 100 ||
             game->attack_variance < -5 ||
             game->attack_variance > 5 ||
             game->attack_critical_chance < 0 ||
             game->attack_critical_chance > 100 ||
             game->attack_critical_roll < 0 ||
             game->attack_critical_roll > 100)
        message = "attack state is invalid";
    else if (game->player_max_hp <= 0 || game->player_hp < 0 ||
             game->player_hp > game->player_max_hp ||
             game->player_defense < 0 || game->player_hit_ticks < 0 ||
             game->player_recovery_ticks < 0 ||
             game->player_recovery_ticks > LAND_PLAYER_RECOVERY_TICKS ||
             game->kilix_rifle_charge < 0 ||
             game->kilix_rifle_charge > LAND_KILIX_MAX_RIFLE_CHARGE ||
             game->rook_magazine < 0 ||
             game->rook_magazine >
                 LAND_PARITY_CHUM_PREDATOR_MAGAZINE ||
             game->rook_reserve_ammo < 0 ||
             game->rook_reserve_ammo >
                 LAND_PARITY_CHUM_RESERVE_AMMO ||
             game->rook_reload_ticks < 0 ||
             game->rook_reload_ticks > 30 ||
             (game->rook_reload_ticks > 0 &&
              (game->cast != LAND_CAST_CHUMRUNNER ||
               game->location != LAND_LOCATION_TRAINING)) ||
             (game->player_recovery_ticks > 0 && game->player_hp != 0))
        message = "player combat state is invalid";
    else if (game->location == LAND_LOCATION_TRAINING &&
             (game->enemy_kind < 0 ||
              game->enemy_kind >= land_enemy_catalog_count(game->cast) ||
              game->enemy_max_hp <= 0 || game->enemy_hp < 0 ||
              game->enemy_hp > game->enemy_max_hp ||
              !isfinite(game->enemy_x) || !isfinite(game->enemy_y) ||
              game->enemy_x < 20.0f || game->enemy_x > 430.0f ||
              game->enemy_y < LAND_FLOOR_TOP ||
              game->enemy_y > LAND_FLOOR_BOTTOM ||
              game->enemy_attack_cooldown_ticks < 0 ||
              game->enemy_round < 1 || game->enemy_charge_level < 0 ||
              game->enemy_charge_level > 1 ||
              game->enemy_magazine < 0 ||
              game->enemy_magazine > 3 ||
              game->enemy_reserve_ammo < 0 ||
              game->enemy_reserve_ammo > 6))
        message = "training target is invalid";
    else if (game->enemy_action < LAND_ENEMY_ACTION_IDLE ||
             game->enemy_action > LAND_ENEMY_ACTION_RELOAD ||
             game->enemy_attack_hit_chance < 0 ||
             game->enemy_attack_hit_chance > 100 ||
             game->enemy_attack_hit_roll < 0 ||
             game->enemy_attack_hit_roll > 100 ||
             game->enemy_attack_variance < -10 ||
             game->enemy_attack_variance > 10 ||
             game->enemy_attack_hits_planned < 0 ||
             game->enemy_attack_hits_landed < 0 ||
             game->enemy_attack_hits_landed >
                 game->enemy_attack_hits_planned ||
             (game->enemy_action == LAND_ENEMY_ACTION_IDLE &&
              (game->enemy_attack_ticks != 0 ||
               game->enemy_attack_total_ticks != 0 ||
               game->enemy_attack_impact_ticks != 0)) ||
             (game->enemy_action != LAND_ENEMY_ACTION_IDLE &&
              (game->location != LAND_LOCATION_TRAINING ||
               game->enemy_hp <= 0 || game->enemy_attack_ticks <= 0 ||
               game->enemy_attack_total_ticks <
                   game->enemy_attack_ticks ||
               game->enemy_attack_impact_ticks < 0 ||
               game->enemy_attack_impact_ticks >
                   game->enemy_attack_total_ticks ||
               !isfinite(game->enemy_attack_target_x) ||
               !isfinite(game->enemy_attack_target_y))) ||
             game->enemy_attack_damage < 0)
        message = "enemy attack state is invalid";
    else if (game->location != LAND_LOCATION_TRAINING &&
             (game->enemy_kind != -1 ||
              game->enemy_action != LAND_ENEMY_ACTION_IDLE))
        message = "social room has a training target";
    else if (game->mode == LAND_MODE_DIALOGUE && !current_beat(game))
        message = "dialogue cursor is invalid";
    if (message && error && error_size > 0u)
        (void)snprintf(error, error_size, "%s", message);
    return message == NULL;
}
