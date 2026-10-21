#include "kilix_land.h"
#include "kilix_ui.h"

#include <stdio.h>
#include <string.h>

#define COLOR_INK UINT32_C(0xf8ead0)
#define COLOR_MUTED UINT32_C(0xb8b2a6)
#define COLOR_PANEL UINT32_C(0x0b1020)
#define COLOR_PANEL_LIGHT UINT32_C(0x18233a)
#define COLOR_GOLD UINT32_C(0xf0c36b)
#define KILIX_RIFLE_SOURCE_HEIGHT 44
#define KILIX_RIFLE_RENDER_WIDTH 93
#define KILIX_RIFLE_RENDER_HEIGHT 64
#define STANDARD_CHARACTER_CELL 222
#define ARDEN_ACTION_EXTENSION 44
#define STANDARD_PLAYER_RENDER_SIZE 92
#define PLAYER_DEFEAT_REACTION_TICKS 10
#define PLAYER_DEFEAT_FALL_TICKS 18

static int text_scale(const ki_td_view *view)
{
    return view->scale >= 1.85f ? 2 : 1;
}

/* A glyph is at most 8 logical pixels wide, and exactly 8 at 1x and 2x.
 * Panel widths and string lengths below are sized to that; several had been
 * laid out for 6 and ran past their panels. */
static void text_at(sr_canvas *canvas, const ki_td_view *view,
                    float x, float y, const char *text, uint32_t color)
{
    sr_text_shadow(canvas, (float)ki_td_screen_x(view, x),
                   (float)ki_td_screen_y(view, y), text, color, 1.0f,
                   text_scale(view));
}

/*
 * The cast sheets no longer share one aspect: fantasy is 16x24 pixel art
 * where the others are square. Anchor on the height the layout already uses
 * and take the width from the cell itself, so a non-square sheet is drawn at
 * its true proportion instead of being squashed into a square box. Square
 * cells return the height unchanged, so the other three casts are untouched.
 */
static int aspect_width(const ki_td_rgba8 *cell, int height)
{
    if (!ki_td_rgba8_is_valid(cell) || cell->height <= 0) return height;
    return cell->width * height / cell->height;
}

/*
 * Pixel art wants a whole-number scale. Fantasy's 16x24 cells at the house
 * height of 92 land on 7.67x, which gives neighbouring source pixels
 * different sizes on screen -- obvious next to a background authored at its
 * native resolution. Round to the nearest whole multiple of the cell. Sheets
 * whose cells are already at or above the house height (the 222x222 casts,
 * which downscale) are returned unchanged.
 */
static int pixel_art_height(const ki_td_rgba8 *cell, int height)
{
    int scale;
    if (!ki_td_rgba8_is_valid(cell) || cell->height <= 0 ||
        cell->height >= height)
        return height;
    scale = (height + cell->height / 2) / cell->height;
    if (scale < 1) scale = 1;
    return cell->height * scale;
}

static bool draw_cell(ki_td_soft_renderer *renderer, const ki_td_view *view,
                      const land_graphics *graphics, land_graphic graphic,
                      int column, int row, float x, float y,
                      int width, int height, float alpha)
{
    ki_td_rgba8 image;
    if (!land_graphics_cell(graphics, graphic, column, row, &image))
        return false;
    ki_td_soft_rgba_resized(renderer, view, x, y, &image, width, height, alpha);
    return true;
}

static land_graphic room_graphic(land_cast cast)
{
    if (cast == LAND_CAST_LEGEND) return LAND_GRAPHIC_LEGEND_ROOM;
    if (cast == LAND_CAST_CHUMRUNNER) return LAND_GRAPHIC_CHUM_ROOM;
    if (cast == LAND_CAST_FANTASY) return LAND_GRAPHIC_FANTASY_ROOM;
    return LAND_GRAPHIC_PLEB_ROOM;
}

static land_graphic training_graphic(land_cast cast)
{
    if (cast == LAND_CAST_LEGEND) return LAND_GRAPHIC_LEGEND_TRAINING;
    if (cast == LAND_CAST_CHUMRUNNER) return LAND_GRAPHIC_CHUM_TRAINING;
    if (cast == LAND_CAST_FANTASY) return LAND_GRAPHIC_FANTASY_TRAINING;
    return LAND_GRAPHIC_PLEB_TRAINING;
}

static land_graphic enemy_graphic(land_cast cast)
{
    if (cast == LAND_CAST_LEGEND) return LAND_GRAPHIC_LEGEND_ENEMIES;
    if (cast == LAND_CAST_CHUMRUNNER) return LAND_GRAPHIC_CHUM_ENEMIES;
    if (cast == LAND_CAST_FANTASY) return LAND_GRAPHIC_FANTASY_ENEMIES;
    return LAND_GRAPHIC_PLEB_ENEMIES;
}

static void draw_room(ki_td_soft_renderer *renderer, const ki_td_view *view,
                      const land_state *game, const land_graphics *graphics)
{
    land_cast cast = land_visible_cast(game);
    ki_td_rgba8 background;
    unsigned int flicker =
        (unsigned int)((game->simulation_tick / 4u) % 5u);
    land_graphic background_graphic =
        game->location == LAND_LOCATION_TRAINING ?
        training_graphic(cast) : room_graphic(cast);
    if (land_graphics_image(graphics, background_graphic, &background))
        ki_td_soft_rgba_resized(renderer, view, 0.0f, 0.0f, &background,
                                LAND_LOGICAL_WIDTH, LAND_LOGICAL_HEIGHT, 1.0f);

    if (game->location == LAND_LOCATION_TRAINING) {
        ki_td_soft_fill_rect(renderer, view, 0.0f, 0.0f,
                             (float)LAND_LOGICAL_WIDTH,
                             (float)LAND_LOGICAL_HEIGHT,
                             UINT32_C(0x02050b), 0.18f);
        ki_td_soft_fill_rect(renderer, view, 0.0f, 158.0f,
                             (float)LAND_LOGICAL_WIDTH, 112.0f,
                             UINT32_C(0x050912), 0.18f);
        ki_td_soft_fill_rect(renderer, view, 0.0f, 0.0f,
                             (float)LAND_LOGICAL_WIDTH, 34.0f,
                             UINT32_C(0x050912), 0.42f);
        return;
    }

    /* Cast-local light keeps each generated environment subtly alive. */
    if (cast == LAND_CAST_LEGEND || cast == LAND_CAST_FANTASY) {
        float fire_x = cast == LAND_CAST_LEGEND ? 115.0f : 82.0f;
        float fire_y = cast == LAND_CAST_LEGEND ? 145.0f : 151.0f;
        int mote;
        ki_td_soft_fill_circle(renderer, view, fire_x, fire_y,
                               13.0f + (float)flicker,
                               UINT32_C(0xff8a35), 0.052f);
        for (mote = 0; mote < 6; ++mote) {
            unsigned int phase = (unsigned int)(
                (game->simulation_tick / 3u +
                 (uint64_t)(unsigned int)mote * 17u) % 70u);
            float x = fire_x - 10.0f + (float)((mote * 13) % 29);
            float y = fire_y - (float)phase * 0.55f;
            if (y > fire_y - 39.0f)
                ki_td_soft_fill_circle(renderer, view, x, y, 0.8f,
                                       UINT32_C(0xffd27a), 0.55f);
        }
    } else if (cast == LAND_CAST_CHUMRUNNER) {
        ki_td_soft_fill_circle(renderer, view, 328.0f, 111.0f,
                               10.0f + (float)flicker * 0.5f,
                               UINT32_C(0x32d9ff), 0.038f);
    } else {
        ki_td_soft_fill_circle(renderer, view, 438.0f, 94.0f,
                               9.0f + (float)flicker * 0.35f,
                               UINT32_C(0xffb34f), 0.038f);
    }
    ki_td_soft_fill_rect(renderer, view, 0.0f, 0.0f,
                         (float)LAND_LOGICAL_WIDTH, 34.0f,
                         UINT32_C(0x050912), 0.34f);
}

static void draw_shadow(ki_td_soft_renderer *renderer, const ki_td_view *view,
                        float x, float y, float radius)
{
    ki_td_soft_fill_ellipse(renderer, view, x, y, radius, radius * 0.29f,
                            UINT32_C(0x02040a), 0.52f);
}

static bool sprite_anchor(const ki_td_rgba8 *image, float *anchor_x,
                          float *anchor_y)
{
    uint64_t head_x_total = 0u;
    uint64_t head_pixels = 0u;
    int top;
    int bottom;
    int y;
    if (!ki_td_rgba8_is_valid(image) || !anchor_x || !anchor_y)
        return false;
    top = image->height;
    bottom = -1;
    for (y = 0; y < image->height; ++y) {
        const uint8_t *row =
            image->pixels + (size_t)y * image->stride;
        int x;
        for (x = 0; x < image->width; ++x) {
            if (row[(size_t)x * 4u + 3u] < UINT8_C(8)) continue;
            if (y < top) top = y;
            if (y > bottom) bottom = y;
        }
    }
    if (bottom < top) return false;
    {
        int head_bottom = top + (bottom - top + 1) * 2 / 5;
        for (y = top; y <= head_bottom; ++y) {
            const uint8_t *row =
                image->pixels + (size_t)y * image->stride;
            int x;
            for (x = 0; x < image->width; ++x) {
                if (row[(size_t)x * 4u + 3u] < UINT8_C(8)) continue;
                head_x_total += (uint64_t)(unsigned int)x;
                ++head_pixels;
            }
        }
    }
    *anchor_x = head_pixels > 0u ?
        (float)((double)head_x_total / (double)head_pixels) :
        (float)image->width * 0.5f;
    *anchor_y = (float)bottom;
    return true;
}

static void draw_foot_anchored(ki_td_soft_renderer *renderer,
                               const ki_td_view *view,
                               const ki_td_rgba8 *image,
                               float foot_x, float foot_y,
                               int width, int height, float alpha)
{
    float anchor_x;
    float anchor_y;
    float scale_x;
    float scale_y;
    if (!sprite_anchor(image, &anchor_x, &anchor_y)) return;
    scale_x = (float)width / (float)image->width;
    scale_y = (float)height / (float)image->height;
    ki_td_soft_rgba_resized(renderer, view,
                            foot_x - anchor_x * scale_x,
                            foot_y - anchor_y * scale_y,
                            image, width, height, alpha);
}

static void draw_ground_centered(ki_td_soft_renderer *renderer,
                                 const ki_td_view *view,
                                 const ki_td_rgba8 *image,
                                 float ground_x, float ground_y,
                                 int width, int height, float alpha)
{
    int left;
    int right = -1;
    int bottom = -1;
    int y;
    float scale_x;
    float scale_y;
    if (!ki_td_rgba8_is_valid(image) || width <= 0 || height <= 0) return;
    left = image->width;
    for (y = 0; y < image->height; ++y) {
        const uint8_t *row =
            image->pixels + (size_t)y * image->stride;
        int x;
        for (x = 0; x < image->width; ++x) {
            if (row[(size_t)x * 4u + 3u] < UINT8_C(8)) continue;
            if (x < left) left = x;
            if (x > right) right = x;
            if (y > bottom) bottom = y;
        }
    }
    if (right < left || bottom < 0) return;
    scale_x = (float)width / (float)image->width;
    scale_y = (float)height / (float)image->height;
    ki_td_soft_rgba_resized(
        renderer, view,
        ground_x -
            ((float)(left + right + 1) * 0.5f) * scale_x,
        ground_y - (float)(bottom + 1) * scale_y,
        image, width, height, alpha);
}

/*
 * The imported 8-column cast sheets provide a strong impact pose but no
 * horizontal knockout cell. Rotate that authored pose only after its reaction
 * beat, keeping it centered on the same floor contact point.
 */
static void draw_standard_player_downed(
    ki_td_soft_renderer *renderer, const ki_td_view *view,
    const ki_td_rgba8 *image, float ground_x, float ground_y,
    int quarter_turns)
{
    uint8_t pixels[
        STANDARD_PLAYER_RENDER_SIZE * STANDARD_PLAYER_RENDER_SIZE * 4];
    ki_td_rgba8 rotated;
    int y;
    if (!ki_td_rgba8_is_valid(image)) return;
    for (y = 0; y < STANDARD_PLAYER_RENDER_SIZE; ++y) {
        int x;
        for (x = 0; x < STANDARD_PLAYER_RENDER_SIZE; ++x) {
            int source_x;
            int source_y;
            const uint8_t *source;
            uint8_t *destination =
                pixels +
                ((size_t)y * STANDARD_PLAYER_RENDER_SIZE + (size_t)x) * 4u;
            if (quarter_turns == 3) {
                source_x =
                    (STANDARD_PLAYER_RENDER_SIZE - 1 - y) *
                    image->width / STANDARD_PLAYER_RENDER_SIZE;
                source_y =
                    x * image->height / STANDARD_PLAYER_RENDER_SIZE;
            } else {
                source_x =
                    y * image->width / STANDARD_PLAYER_RENDER_SIZE;
                source_y =
                    (STANDARD_PLAYER_RENDER_SIZE - 1 - x) *
                    image->height / STANDARD_PLAYER_RENDER_SIZE;
            }
            source = image->pixels +
                (size_t)source_y * image->stride +
                (size_t)source_x * 4u;
            destination[0] = source[0];
            destination[1] = source[1];
            destination[2] = source[2];
            destination[3] = source[3];
        }
    }
    rotated = ki_td_rgba8_make(
        pixels, STANDARD_PLAYER_RENDER_SIZE,
        STANDARD_PLAYER_RENDER_SIZE);
    draw_ground_centered(
        renderer, view, &rotated, ground_x, ground_y,
        STANDARD_PLAYER_RENDER_SIZE, STANDARD_PLAYER_RENDER_SIZE, 0.92f);
}

static int player_defeat_elapsed(const land_state *game)
{
    int elapsed = LAND_PLAYER_RECOVERY_TICKS - game->player_recovery_ticks;
    return elapsed > 0 ? elapsed : 0;
}

/*
 * Arden's authored blade and slash trail continue into the transparent first
 * 44 pixels of the following atlas cell. Stitch that strip onto the base pose
 * at the same scale without admitting the neighboring reaction pose.
 */
static void draw_arden_action_extension(
    ki_td_soft_renderer *renderer, const ki_td_view *view,
    const land_graphics *graphics, float foot_x, float foot_y,
    int width, int height)
{
    ki_td_rgba8 base;
    ki_td_rgba8 atlas;
    ki_td_rgba8 extension;
    float anchor_x;
    float anchor_y;
    float origin_x;
    float origin_y;
    int extension_width;
    if (!land_graphics_cell(graphics, LAND_GRAPHIC_FANTASY_CHARACTERS,
                            6, 0, &base) ||
        !land_graphics_image(graphics, LAND_GRAPHIC_FANTASY_CHARACTERS,
                             &atlas) ||
        atlas.width < STANDARD_CHARACTER_CELL * 7 + ARDEN_ACTION_EXTENSION ||
        atlas.height < STANDARD_CHARACTER_CELL ||
        !sprite_anchor(&base, &anchor_x, &anchor_y))
        return;
    extension = ki_td_rgba8_make(
        atlas.pixels +
            (size_t)(STANDARD_CHARACTER_CELL * 7) * 4u,
        ARDEN_ACTION_EXTENSION, STANDARD_CHARACTER_CELL);
    extension.stride = atlas.stride;
    if (!ki_td_rgba8_is_valid(&extension)) return;
    origin_x = foot_x -
        anchor_x * (float)width / (float)STANDARD_CHARACTER_CELL;
    origin_y = foot_y -
        anchor_y * (float)height / (float)STANDARD_CHARACTER_CELL;
    extension_width =
        (ARDEN_ACTION_EXTENSION * width +
         STANDARD_CHARACTER_CELL / 2) / STANDARD_CHARACTER_CELL;
    ki_td_soft_rgba_resized(renderer, view, origin_x + (float)width,
                            origin_y, &extension, extension_width,
                            height, 1.0f);
}

static int clip_frame(uint64_t tick, const int *durations,
                      int duration_count)
{
    int total = 0;
    int elapsed;
    int index;
    for (index = 0; index < duration_count; ++index)
        total += durations[index];
    elapsed = (int)(((tick % UINT64_C(2160)) * UINT64_C(1000) /
                     (uint64_t)LAND_SIMULATION_HZ) % (uint64_t)total);
    for (index = 0; index < duration_count - 1; ++index) {
        if (elapsed < durations[index]) return index;
        elapsed -= durations[index];
    }
    return duration_count - 1;
}

static int player_idle_frame(const land_state *game)
{
    static const int idle_durations[] = {180, 180, 180, 180};
    return clip_frame(game->simulation_tick, idle_durations, 4);
}

static int attack_clip_frame(const land_state *game, const int *durations,
                             int duration_count)
{
    int elapsed =
        (game->attack_total_ticks - game->attack_ticks) * 1000 /
        LAND_SIMULATION_HZ;
    int index;
    if (elapsed < 0) elapsed = 0;
    for (index = 0; index < duration_count - 1; ++index) {
        if (elapsed < durations[index]) return index;
        elapsed -= durations[index];
    }
    return duration_count - 1;
}

static void ranged_recoil_offset(const land_state *game,
                                 float *offset_x, float *offset_y)
{
    int elapsed;
    float strength;
    *offset_x = 0.0f;
    *offset_y = 0.0f;
    if (game->attack_ticks <= 0 ||
        (game->attack != LAND_ATTACK_KILIX_LASER &&
         game->attack != LAND_ATTACK_ROOK_PREDATOR))
        return;
    elapsed = game->attack_total_ticks - game->attack_ticks;
    if (elapsed < 2)
        strength = game->attack == LAND_ATTACK_KILIX_LASER ? 5.0f : 4.0f;
    else if (elapsed < 4)
        strength = game->attack == LAND_ATTACK_KILIX_LASER ? 3.0f : 2.5f;
    else if (elapsed == 4)
        strength = 1.0f;
    else if (elapsed == 5)
        strength = -0.75f;
    else
        return;
    if (game->attack_facing == LAND_FACING_LEFT)
        *offset_x = strength;
    else if (game->attack_facing == LAND_FACING_RIGHT)
        *offset_x = -strength;
    else if (game->attack_facing == LAND_FACING_UP)
        *offset_y = strength;
    else
        *offset_y = -strength;
}

static void rook_melee_lunge_offset(const land_state *game,
                                    float *offset_x, float *offset_y)
{
    int elapsed;
    float strength;
    *offset_x = 0.0f;
    *offset_y = 0.0f;
    if (game->attack_ticks <= 0 ||
        game->attack != LAND_ATTACK_ROOK_PISTOL_BUTT)
        return;
    elapsed = game->attack_total_ticks - game->attack_ticks;
    if (elapsed < 3)
        strength = (float)(elapsed + 1) * 2.0f;
    else if (elapsed < 7)
        strength = 8.0f;
    else if (elapsed < 10)
        strength = (float)(10 - elapsed) * 2.0f;
    else
        return;
    if (game->attack_facing == LAND_FACING_LEFT)
        *offset_x = -strength;
    else if (game->attack_facing == LAND_FACING_RIGHT)
        *offset_x = strength;
    else if (game->attack_facing == LAND_FACING_UP)
        *offset_y = -strength;
    else
        *offset_y = strength;
}

static land_graphic character_graphic(land_cast cast)
{
    if (cast == LAND_CAST_CHUMRUNNER) return LAND_GRAPHIC_CHUM_CHARACTERS;
    if (cast == LAND_CAST_FANTASY) return LAND_GRAPHIC_FANTASY_CHARACTERS;
    return LAND_GRAPHIC_PLEB_CHARACTERS;
}

static land_graphic portrait_graphic(land_cast cast)
{
    if (cast == LAND_CAST_CHUMRUNNER) return LAND_GRAPHIC_CHUM_PORTRAITS;
    if (cast == LAND_CAST_FANTASY) return LAND_GRAPHIC_FANTASY_PORTRAITS;
    return LAND_GRAPHIC_PLEB_PORTRAITS;
}

static int standard_walk_phase(const land_state *game)
{
    static const int durations[] = {120, 120, 120, 120};
    return clip_frame(game->simulation_tick, durations, 4);
}

static bool legend_player_cell(const land_state *game,
                               const land_graphics *graphics,
                               ki_td_rgba8 *cell)
{
    enum { KILIX_HELD_AIM_FRAME = 19 };
    static const int fire_frames[] = {19, 20, 21, 22, 21, 19};
    static const int butt_durations[] = {70, 70, 55, 55, 80, 110};
    static const int fire_durations[] = {50, 45, 45, 65, 80, 100};
    int column;
    int local_frame;
    int phase;
    if (game->player_recovery_ticks > 0) {
        int elapsed = player_defeat_elapsed(game);
        if (elapsed < PLAYER_DEFEAT_REACTION_TICKS)
            local_frame = 24 + (elapsed >= 5 ? 1 : 0);
        else
            local_frame =
                elapsed < PLAYER_DEFEAT_FALL_TICKS ? 28 : 29;
        return land_graphics_cell(
            graphics, LAND_GRAPHIC_LEGEND_PLAYER,
            local_frame % 16,
            (int)game->facing * 2 + local_frame / 16, cell);
    }
    if (game->attack_ticks > 0 &&
        (game->attack == LAND_ATTACK_KILIX_RIFLE_BUTT ||
         game->attack == LAND_ATTACK_KILIX_LASER)) {
        if (game->attack == LAND_ATTACK_KILIX_RIFLE_BUTT)
            local_frame = 12 + attack_clip_frame(
                game, butt_durations,
                (int)(sizeof butt_durations / sizeof butt_durations[0]));
        else
            local_frame = fire_frames[attack_clip_frame(
                game, fire_durations,
                (int)(sizeof fire_durations / sizeof fire_durations[0]))];
        column = local_frame % 16;
        return land_graphics_cell(
            graphics, LAND_GRAPHIC_LEGEND_PLAYER, column,
            (int)game->facing * 2 + local_frame / 16, cell);
    }
    if (game->aiming) {
        local_frame = KILIX_HELD_AIM_FRAME;
        return land_graphics_cell(
            graphics, LAND_GRAPHIC_LEGEND_PLAYER,
            local_frame % 16,
            (int)game->facing * 2 + local_frame / 16, cell);
    }
    if (!game->player_moving) {
        column = player_idle_frame(game);
        return land_graphics_cell(
            graphics, LAND_GRAPHIC_LEGEND_PLAYER,
            column, (int)game->facing * 2, cell);
    }
    phase = standard_walk_phase(game);
    /*
     * Kilix's eight authored walk cells are all variations of one held
     * stride: the sheet has no opposite footfall. Land used to fabricate one
     * by reflecting the legs around the torso, which put pixels in a pose the
     * source game never drew. The cycle now alternates the two authored
     * frames and stays entirely art; a real second footfall has to come from
     * legend-of-kilix.
     */
    column = (phase & 1) ? 4 : 0;
    return land_graphics_cell(
        graphics, LAND_GRAPHIC_LEGEND_PLAYER,
        column, (int)game->facing * 2, cell);
}

static bool standard_player_cell(const land_state *game,
                                 const land_graphics *graphics,
                                 land_cast cast, ki_td_rgba8 *cell)
{
    static const land_character_slot side_slots[] = {
        LAND_SLOT_SIDE_A, LAND_SLOT_SIDE_B,
        LAND_SLOT_SIDE_A, LAND_SLOT_SIDE_B};
    int phase = standard_walk_phase(game);
    land_character_slot slot = LAND_SLOT_IDLE_DOWN;
    bool mirrored = false;
    land_hero_motion_variant variant = LAND_HERO_MOTION_DOWN_A;
    bool authored_side_faces_right =
        cast == LAND_CAST_FANTASY || cast == LAND_CAST_PLEB_BOUND;
    if (game->player_recovery_ticks > 0)
        return land_graphics_cell(
            graphics, character_graphic(cast),
            land_character_column(cast, LAND_SLOT_RECOVERY), 0, cell);
    if (game->attack_ticks > 0 &&
        game->attack >= LAND_ATTACK_ROOK_PREDATOR &&
        game->attack <= LAND_ATTACK_PIP_BENT_BAT)
        return land_graphics_cell(
            graphics, character_graphic(cast),
            land_character_column(cast, LAND_SLOT_ACTION), 0, cell);
    if (cast == LAND_CAST_CHUMRUNNER && game->aiming)
        return land_graphics_cell(
            graphics, character_graphic(cast),
            land_character_column(cast, LAND_SLOT_ACTION), 0, cell);
    if (game->facing == LAND_FACING_DOWN) {
        if (!game->player_moving || (phase & 1) == 0)
            slot = LAND_SLOT_IDLE_DOWN;
        else
            return land_graphics_hero_motion_cell(
                graphics, cast,
                phase == 1 ? LAND_HERO_MOTION_DOWN_A :
                             LAND_HERO_MOTION_DOWN_B,
                cell);
    }
    else if (game->facing == LAND_FACING_UP) {
        slot = LAND_SLOT_IDLE_UP;
        if (game->player_moving && (phase & 1) != 0)
            return land_graphics_hero_motion_cell(
                graphics, cast, LAND_HERO_MOTION_UP_STEP, cell);
    } else if (game->facing == LAND_FACING_LEFT) {
        slot = game->player_moving ? side_slots[phase] : LAND_SLOT_SIDE_A;
        mirrored = authored_side_faces_right;
    } else {
        slot = game->player_moving ? side_slots[phase] : LAND_SLOT_SIDE_A;
        mirrored = !authored_side_faces_right;
    }
    if (mirrored) {
        variant = slot == LAND_SLOT_SIDE_A ?
            LAND_HERO_MOTION_MIRRORED_SIDE :
            LAND_HERO_MOTION_MIRRORED_WALK;
        return land_graphics_hero_motion_cell(
            graphics, cast, variant, cell);
    }
    return land_graphics_cell(graphics, character_graphic(cast),
                              land_character_column(cast, slot), 0, cell);
}

static void draw_actor(ki_td_soft_renderer *renderer, const ki_td_view *view,
                       const land_state *game, const land_graphics *graphics,
                       int actor)
{
    land_cast cast = land_visible_cast(game);
    float x;
    float y;
    if (actor == LAND_ACTOR_HERO) {
        float recoil_x;
        float recoil_y;
        float lunge_x;
        float lunge_y;
        float sprite_x;
        float sprite_y;
        int defeat_elapsed = player_defeat_elapsed(game);
        x = game->player_x;
        y = game->player_y;
        ranged_recoil_offset(game, &recoil_x, &recoil_y);
        rook_melee_lunge_offset(game, &lunge_x, &lunge_y);
        sprite_x = x + recoil_x + lunge_x;
        sprite_y = y + recoil_y + lunge_y;
        if (game->player_hit_ticks > 0 &&
            ((game->player_hit_ticks / 2) & 1) != 0)
            sprite_x -= 3.0f;
        draw_shadow(renderer, view, x, y,
                    game->player_recovery_ticks > 0 &&
                    defeat_elapsed >= PLAYER_DEFEAT_FALL_TICKS ?
                    27.0f : 18.0f);
        if (cast == LAND_CAST_LEGEND) {
            ki_td_rgba8 cell;
            if (legend_player_cell(game, graphics, &cell)) {
                bool rifle_pose =
                    game->aiming ||
                    (game->attack_ticks > 0 &&
                     game->attack == LAND_ATTACK_KILIX_LASER);
                if (rifle_pose &&
                    cell.width >= 64 &&
                    cell.height >= KILIX_RIFLE_SOURCE_HEIGHT) {
                    cell = ki_td_rgba8_subimage(
                        &cell, 0, 0, 64, KILIX_RIFLE_SOURCE_HEIGHT);
                    draw_foot_anchored(
                        renderer, view, &cell, sprite_x, sprite_y,
                        KILIX_RIFLE_RENDER_WIDTH,
                        KILIX_RIFLE_RENDER_HEIGHT, 1.0f);
                } else if (
                    game->player_recovery_ticks > 0 &&
                    defeat_elapsed >= PLAYER_DEFEAT_REACTION_TICKS) {
                    draw_ground_centered(
                        renderer, view, &cell, sprite_x, sprite_y,
                        64, 64, 0.94f);
                } else {
                    draw_foot_anchored(renderer, view, &cell,
                                       sprite_x, sprite_y,
                                       64, 64, 1.0f);
                }
            }
        } else {
            ki_td_rgba8 cell;
            if (standard_player_cell(game, graphics, cast, &cell)) {
                if (game->player_recovery_ticks > 0 &&
                    defeat_elapsed >= PLAYER_DEFEAT_FALL_TICKS) {
                    draw_standard_player_downed(
                        renderer, view, &cell, sprite_x, sprite_y,
                        cast == LAND_CAST_FANTASY ? 3 : 1);
                } else {
                    int player_height = pixel_art_height(
                        &cell, STANDARD_PLAYER_RENDER_SIZE);
                    draw_foot_anchored(
                        renderer, view, &cell, sprite_x, sprite_y,
                        aspect_width(&cell, player_height),
                        player_height, 1.0f);
                }
                if (cast == LAND_CAST_FANTASY &&
                    game->attack_ticks > 0 &&
                    game->attack == LAND_ATTACK_ARDEN_BRONZE_BLADE)
                    draw_arden_action_extension(
                        renderer, view, graphics,
                        sprite_x, sprite_y, 92, 92);
            }
        }
    } else {
        float bob = (game->simulation_tick / 32u +
                     (uint64_t)(unsigned int)actor) % 2u == 0u ?
                    0.0f : -1.0f;
        land_actor_position(actor, &x, &y);
        draw_shadow(renderer, view, x, y - 1.0f, 21.0f);
        if (cast == LAND_CAST_LEGEND) {
            ki_td_rgba8 cell;
            if (land_graphics_cell(graphics, LAND_GRAPHIC_LEGEND_NPCS,
                                   actor - LAND_ACTOR_ALLY_1, 0, &cell))
                ki_td_soft_rgba_pixel_art(
                    renderer, view, x - 32.0f, y - 60.0f + bob,
                    &cell, 1.0f);
        } else {
            ki_td_rgba8 npc;
            int npc_width = 92;
            int npc_height = 92;
            if (land_graphics_cell(graphics, character_graphic(cast),
                                   land_character_column(
                                       cast, LAND_SLOT_IDLE_DOWN),
                                   actor, &npc)) {
                npc_height = pixel_art_height(&npc, 92);
                npc_width = aspect_width(&npc, npc_height);
            }
            (void)draw_cell(renderer, view, graphics,
                            character_graphic(cast),
                            land_character_column(cast,
                                                  LAND_SLOT_IDLE_DOWN),
                            actor,
                            x - (float)npc_width * 0.5f, y - 87.0f + bob,
                            npc_width, npc_height, 1.0f);
        }
    }
}

static void draw_training_enemy(ki_td_soft_renderer *renderer,
                                const ki_td_view *view,
                                const land_state *game,
                                const land_graphics *graphics)
{
    land_cast cast = land_visible_cast(game);
    ki_td_rgba8 cell;
    int visual = land_enemy_visual_index(game);
    int column;
    int row;
    int width;
    int height;
    float x = game->enemy_x;
    float y = game->enemy_y;
    float alpha = game->enemy_hp > 0 ? 1.0f : 0.42f;
    if (game->enemy_kind < 0) return;
    if (game->enemy_hit_ticks > 0 &&
        ((game->enemy_hit_ticks / 2) & 1) != 0)
        x += 3.0f;
    if (game->enemy_action == LAND_ENEMY_ACTION_ATTACK &&
        !game->enemy_attack_ranged &&
        game->enemy_attack_total_ticks > 0) {
        int elapsed =
            game->enemy_attack_total_ticks - game->enemy_attack_ticks;
        int impact_elapsed =
            game->enemy_attack_total_ticks -
            game->enemy_attack_impact_ticks;
        if (elapsed >= impact_elapsed - 5 &&
            elapsed <= impact_elapsed + 4)
            x -= 12.0f;
    }
    if (cast == LAND_CAST_LEGEND) {
        column = visual % 6;
        row = visual / 6;
        width = 74;
        height = 74;
    } else {
        row = visual;
        if (cast == LAND_CAST_CHUMRUNNER)
            column = game->enemy_hp <= 0 ? 7 :
                     (game->enemy_hit_ticks > 0 ? 5 :
                      (game->enemy_action == LAND_ENEMY_ACTION_ATTACK ? 4 :
                       (game->enemy_moving ?
                        1 + (int)((game->simulation_tick / 9u) % 2u) : 0)));
        else if (cast == LAND_CAST_FANTASY)
            column = game->enemy_hit_ticks > 0 || game->enemy_hp <= 0 ? 2 :
                     (game->enemy_action != LAND_ENEMY_ACTION_IDLE ? 1 : 0);
        else
            column = game->enemy_hp <= 0 ? 3 :
                     (game->enemy_hit_ticks > 0 ? 2 :
                      (game->enemy_action == LAND_ENEMY_ACTION_ATTACK ? 1 : 0));
        /* The fantasy cell changed from 384x192 to 96x112 when its source
         * atlas was regenerated. Drawing the new near-square cell into the
         * old 2:1 box would squash every enemy by half; 58x68 keeps them the
         * height they were and restores the aspect. */
        width = cast == LAND_CAST_FANTASY ? 58 : 102;
        height = cast == LAND_CAST_FANTASY ? 68 : 102;
    }
    draw_shadow(renderer, view, x, y, cast == LAND_CAST_FANTASY ? 34.0f :
                25.0f);
    if (land_graphics_cell(graphics, enemy_graphic(cast),
                           column, row, &cell))
        draw_foot_anchored(renderer, view, &cell, x, y,
                           width, height, alpha);
}

static uint32_t enemy_attack_color(const land_state *game)
{
    if (game->cast == LAND_CAST_LEGEND) return UINT32_C(0x66eaff);
    if (game->cast == LAND_CAST_CHUMRUNNER) return UINT32_C(0xff5f70);
    if (game->cast == LAND_CAST_FANTASY) return UINT32_C(0xc48cff);
    return UINT32_C(0xff9f52);
}

static void draw_enemy_attack_effect(ki_td_soft_renderer *renderer,
                                     const ki_td_view *view,
                                     const land_state *game)
{
    uint32_t color;
    float source_x;
    float source_y;
    float target_x;
    float target_y;
    int elapsed;
    int impact_elapsed;
    float progress;
    if (game->enemy_action == LAND_ENEMY_ACTION_IDLE ||
        game->enemy_attack_ticks <= 0)
        return;
    color = enemy_attack_color(game);
    source_x = land_enemy_target_x(game);
    source_y = land_enemy_target_y(game);
    if (game->enemy_action == LAND_ENEMY_ACTION_CHARGE) {
        float pulse =
            13.0f + (float)((game->enemy_attack_ticks / 3) % 5);
        ki_td_soft_fill_circle(renderer, view, source_x, source_y,
                               pulse, color, 0.16f);
        ki_td_soft_fill_circle(renderer, view, source_x, source_y,
                               5.0f, UINT32_C(0xffffff), 0.42f);
        return;
    }

    target_x = game->enemy_attack_target_x;
    target_y = game->enemy_attack_target_y;
    elapsed = game->enemy_attack_total_ticks - game->enemy_attack_ticks;
    impact_elapsed =
        game->enemy_attack_total_ticks - game->enemy_attack_impact_ticks;
    if (impact_elapsed < 1) impact_elapsed = 1;
    progress = (float)elapsed / (float)impact_elapsed;
    if (progress > 1.0f) progress = 1.0f;

    if (!game->enemy_attack_landed) {
        float pulse = 9.0f + (float)((game->enemy_attack_ticks / 3) % 4);
        ki_td_soft_line(renderer, view, source_x, source_y,
                        target_x, target_y, 3.0f,
                        UINT32_C(0x080b12), 0.72f);
        ki_td_soft_line(renderer, view, source_x, source_y,
                        target_x, target_y, 1.0f, color, 0.82f);
        ki_td_soft_fill_circle(renderer, view, target_x, target_y,
                               pulse, color, 0.20f);
        ki_td_soft_line(renderer, view, target_x - 8.0f, target_y,
                        target_x + 8.0f, target_y,
                        1.0f, color, 0.94f);
        ki_td_soft_line(renderer, view, target_x, target_y - 8.0f,
                        target_x, target_y + 8.0f,
                        1.0f, color, 0.94f);
    }
    if (game->enemy_attack_ranged) {
        float bolt_x = source_x + (target_x - source_x) * progress;
        float bolt_y = source_y + (target_y - source_y) * progress;
        ki_td_soft_line(renderer, view, source_x, source_y,
                        bolt_x, bolt_y, 2.0f, color, 0.86f);
        ki_td_soft_fill_circle(renderer, view, bolt_x, bolt_y,
                               3.0f, UINT32_C(0xffffff), 0.94f);
    } else if (progress > 0.55f) {
        float sweep = (progress - 0.55f) / 0.45f;
        float tip_x = source_x - 48.0f * sweep;
        float tip_y = source_y + 18.0f - 25.0f * sweep;
        ki_td_soft_line(renderer, view, source_x - 4.0f, source_y - 12.0f,
                        tip_x, tip_y, 3.0f, color, 0.88f);
    }
    if (game->enemy_attack_landed && game->enemy_attack_hit &&
        game->player_hit_ticks > 0) {
        float radius = 4.0f +
            (float)(10 - game->player_hit_ticks) * 1.5f;
        ki_td_soft_fill_circle(renderer, view,
                               game->player_x, game->player_y - 34.0f,
                               radius, color, 0.48f);
    }
}

static void draw_attack_effect(ki_td_soft_renderer *renderer,
                               const ki_td_view *view,
                               const land_state *game)
{
    int elapsed;
    int impact_elapsed;
    float start_x;
    float start_y;
    float target_x = game->attack_target_x;
    float target_y = game->attack_target_y;
    float recoil_x;
    float recoil_y;
    float progress;
    if (game->attack_ticks <= 0 ||
        game->attack == LAND_ATTACK_NONE)
        return;
    elapsed = game->attack_total_ticks - game->attack_ticks;
    impact_elapsed =
        game->attack_total_ticks - game->attack_impact_ticks;
    if (impact_elapsed < 1) impact_elapsed = 1;
    progress = (float)elapsed / (float)impact_elapsed;
    if (progress > 1.0f) progress = 1.0f;
    start_x = game->player_x +
              (game->cast == LAND_CAST_LEGEND ? 20.0f : 29.0f);
    start_y = game->player_y -
              (game->cast == LAND_CAST_LEGEND ? 31.0f : 45.0f);
    ranged_recoil_offset(game, &recoil_x, &recoil_y);
    start_x += recoil_x;
    start_y += recoil_y;

    if (game->attack == LAND_ATTACK_KILIX_LASER) {
        float pulse_x = start_x + (target_x - start_x) * progress;
        float pulse_y = start_y + (target_y - start_y) * progress;
        ki_td_soft_line(renderer, view, start_x, start_y,
                        pulse_x, pulse_y, 2.0f,
                        UINT32_C(0x59efff), 0.84f);
        ki_td_soft_fill_circle(renderer, view, pulse_x, pulse_y, 3.5f,
                               UINT32_C(0xd8ffff), 1.0f);
    } else if (game->attack == LAND_ATTACK_ROOK_PREDATOR) {
        float pulse_x = start_x + (target_x - start_x) * progress;
        float pulse_y = start_y + (target_y - start_y) * progress;
        ki_td_soft_line(renderer, view, start_x, start_y,
                        pulse_x, pulse_y, 1.5f,
                        UINT32_C(0xffd36a), 0.92f);
        ki_td_soft_fill_circle(renderer, view, start_x, start_y, 4.0f,
                               UINT32_C(0xffa43a),
                               elapsed < 4 ? 0.92f : 0.30f);
    } else if (progress > 0.32f) {
        uint32_t color =
            game->attack == LAND_ATTACK_ARDEN_BRONZE_BLADE ?
            UINT32_C(0xc9f7ff) :
            (game->attack == LAND_ATTACK_PIP_BENT_BAT ?
             UINT32_C(0xffd176) :
             (game->attack == LAND_ATTACK_ROOK_PISTOL_BUTT ?
              UINT32_C(0xffb14a) : UINT32_C(0xf7e5be)));
        float reach =
            game->attack == LAND_ATTACK_PIP_BENT_BAT ? 48.0f :
            (game->attack == LAND_ATTACK_ROOK_PISTOL_BUTT ?
             42.0f : 58.0f);
        float swing_x = game->player_x + 19.0f;
        float swing_y = game->player_y - 42.0f;
        float sweep = progress < 0.76f ? progress / 0.76f : 1.0f;
        float tip_x = swing_x + reach * sweep;
        float tip_y = swing_y - 20.0f + 37.0f * sweep;
        ki_td_soft_line(renderer, view, swing_x, swing_y - 14.0f,
                        tip_x, tip_y,
                        game->attack == LAND_ATTACK_PIP_BENT_BAT ?
                        4.0f :
                        (game->attack == LAND_ATTACK_ROOK_PISTOL_BUTT ?
                         3.0f : 2.0f), color, 0.88f);
        ki_td_soft_line(renderer, view, swing_x + 7.0f, swing_y - 20.0f,
                        tip_x + 7.0f, tip_y - 6.0f,
                        1.0f, color, 0.56f);
    }
    if (game->attack_landed && game->enemy_hit_ticks > 0) {
        float radius = 3.0f + (float)(6 - game->enemy_hit_ticks) * 1.6f;
        ki_td_soft_fill_circle(renderer, view, target_x, target_y,
                               radius, UINT32_C(0xffffff),
                               0.48f);
    }
}

static void draw_aim_guide(ki_td_soft_renderer *renderer,
                           const ki_td_view *view,
                           const land_state *game)
{
    float start_x;
    float start_y;
    float recoil_x;
    float recoil_y;
    uint32_t color;
    if (!game->aiming || game->location != LAND_LOCATION_TRAINING ||
        (game->cast != LAND_CAST_LEGEND &&
         game->cast != LAND_CAST_CHUMRUNNER))
        return;
    start_x = game->player_x +
              (game->cast == LAND_CAST_LEGEND ? 20.0f : 29.0f);
    start_y = game->player_y -
              (game->cast == LAND_CAST_LEGEND ? 31.0f : 45.0f);
    ranged_recoil_offset(game, &recoil_x, &recoil_y);
    start_x += recoil_x;
    start_y += recoil_y;
    color = land_game_target_locked(game) &&
            (game->cast != LAND_CAST_CHUMRUNNER ||
             land_rook_hit_chance(game) > 0) ?
            UINT32_C(0x69f7b2) : UINT32_C(0xffc85b);
    ki_td_soft_line(renderer, view, start_x, start_y,
                    game->aim_x, game->aim_y,
                    1.0f, color, 0.72f);
    ki_td_soft_fill_circle(renderer, view, game->aim_x,
                           game->aim_y, 7.0f, color, 0.20f);
    ki_td_soft_line(renderer, view, game->aim_x - 10.0f,
                    game->aim_y, game->aim_x + 10.0f,
                    game->aim_y, 1.0f, color, 0.95f);
    ki_td_soft_line(renderer, view, game->aim_x,
                    game->aim_y - 10.0f, game->aim_x,
                    game->aim_y + 10.0f, 1.0f, color, 0.95f);
}

static float actor_depth(const land_state *game, int actor)
{
    float x;
    float y;
    if (actor == LAND_ACTOR_HERO) return game->player_y;
    land_actor_position(actor, &x, &y);
    (void)x;
    return y;
}

static void draw_actors(ki_td_soft_renderer *renderer, const ki_td_view *view,
                        const land_state *game,
                        const land_graphics *graphics)
{
    int order[LAND_ACTOR_COUNT] = {
        LAND_ACTOR_ALLY_1, LAND_ACTOR_ALLY_2,
        LAND_ACTOR_ALLY_3, LAND_ACTOR_HERO
    };
    int left;
    if (game->location == LAND_LOCATION_TRAINING) {
        draw_actor(renderer, view, game, graphics, LAND_ACTOR_HERO);
        draw_training_enemy(renderer, view, game, graphics);
        draw_enemy_attack_effect(renderer, view, game);
        draw_aim_guide(renderer, view, game);
        draw_attack_effect(renderer, view, game);
        return;
    }
    for (left = 0; left < LAND_ACTOR_COUNT - 1; ++left) {
        int right;
        for (right = left + 1; right < LAND_ACTOR_COUNT; ++right) {
            if (actor_depth(game, order[right]) < actor_depth(game, order[left])) {
                int swap = order[left];
                order[left] = order[right];
                order[right] = swap;
            }
        }
    }
    for (left = 0; left < LAND_ACTOR_COUNT; ++left)
        draw_actor(renderer, view, game, graphics, order[left]);
}

static void draw_name_tags(ki_td_soft_renderer *renderer,
                           const ki_td_view *view, sr_canvas *canvas,
                           const land_state *game)
{
    int actor;
    if (game->location != LAND_LOCATION_SOCIAL) return;
    for (actor = LAND_ACTOR_ALLY_1; actor <= LAND_ACTOR_ALLY_3; ++actor) {
        const char *name = land_actor_name(game, actor);
        float x;
        float y;
        float label_y;
        float label_width = 16.0f + (float)strlen(name) * 6.0f;
        if (game->mode == LAND_MODE_ROOM && game->nearest_actor == actor)
            continue;
        if (label_width < 50.0f) label_width = 50.0f;
        if (label_width > 106.0f) label_width = 106.0f;
        land_actor_position(actor, &x, &y);
        label_y = y + 2.0f;
        ki_td_soft_fill_rect(renderer, view, x - label_width * 0.5f, label_y,
                             label_width, 13.0f,
                             UINT32_C(0x070b13), 0.78f);
        ki_td_soft_fill_rect(renderer, view, x - label_width * 0.5f, label_y,
                             2.0f, 13.0f,
                             land_actor_color(game, actor), 1.0f);
        text_at(canvas, view, x - label_width * 0.5f + 7.0f,
                label_y - 1.0f, name, UINT32_C(0xf6e7cb));
    }
}

static void draw_header(ki_td_soft_renderer *renderer, const ki_td_view *view,
                        sr_canvas *canvas, const land_state *game)
{
    char title[40];
    const char *controls = "WASD  ENTER TALK  C CAST  Q EXIT";
    (void)snprintf(title, sizeof title, "%s / %s",
                   game->location == LAND_LOCATION_TRAINING ?
                   "TRAINING" :
                   land_cast_room_name(land_visible_cast(game)),
                   land_actor_name(game, LAND_ACTOR_HERO));
    ki_td_soft_fill_rect(renderer, view, 8.0f, 7.0f, 178.0f, 21.0f,
                         UINT32_C(0x070b14), 0.84f);
    ki_td_soft_fill_rect(renderer, view, 8.0f, 7.0f, 3.0f, 21.0f,
                         COLOR_GOLD, 1.0f);
    text_at(canvas, view, 17.0f, 9.0f, title, COLOR_INK);

    ki_td_soft_fill_rect(renderer, view, 195.0f, 7.0f, 277.0f, 21.0f,
                         UINT32_C(0x070b14), 0.78f);
    if (game->location == LAND_LOCATION_TRAINING) {
        if (game->cast == LAND_CAST_CHUMRUNNER)
            controls = game->aiming ?
                       "WASD AIM  Z FIRE  X LOWER  R LOAD" :
                       "WASD  Z BUTT  X AIM  R LOAD  ESC";
        else if (game->cast == LAND_CAST_LEGEND)
            controls = "WASD  X AIM  Z USE  ESC  C CAST";
        else
            controls = "WASD  Z SWING  ESC BACK  C CAST";
    }
    text_at(canvas, view, 204.0f, 9.0f, controls, COLOR_MUTED);
}

static const char *ready_attack_name(const land_state *game)
{
    if (game->player_recovery_ticks > 0)
        return "KNOCKED OUT / RECOVERING";
    if (game->rook_reload_ticks > 0)
        return "RELOADING ARES PREDATOR";
    if (game->attack != LAND_ATTACK_NONE)
        return land_attack_name(game->attack);
    if (game->aiming) {
        if (game->cast == LAND_CAST_CHUMRUNNER &&
            game->rook_magazine <= 0)
            return "PREDATOR EMPTY  R RELOAD";
        if (game->cast == LAND_CAST_CHUMRUNNER &&
            land_rook_hit_chance(game) == 0)
            return "TOO FAR / MOVE CLOSER";
        return land_game_target_locked(game) ?
               "TARGET LOCKED  /  Z FIRE" :
               "AIM MODE / MOVE RETICLE";
    }
    if (game->cast == LAND_CAST_LEGEND)
        return game->kilix_rifle_charge > 0 ?
               "Z BUTT  /  X AIM MODE" : "Z RIFLE BUTT";
    if (game->cast == LAND_CAST_CHUMRUNNER)
        return game->rook_magazine > 0 ?
               "Z PISTOL BUTT / X AIM" :
               "Z PISTOL BUTT / R RELOAD";
    if (game->cast == LAND_CAST_FANTASY)
        return "BRONZE BLADE";
    return "BENT BAT BASH";
}

static float pip_timing_marker(const land_state *game)
{
    if (land_pip_timing_pattern(game) == 1) {
        int phase = (int)(game->simulation_tick % UINT64_C(48));
        if (phase > 24) phase = 48 - phase;
        return (float)phase / 24.0f;
    }
    if (land_pip_timing_pattern(game) == 2)
        return (float)(game->simulation_tick % UINT64_C(72)) / 71.0f;
    return (float)(game->simulation_tick % UINT64_C(60)) / 59.0f;
}

static void draw_training_hud(ki_td_soft_renderer *renderer,
                              const ki_td_view *view, sr_canvas *canvas,
                              const land_state *game)
{
    char line[96];
    float health_fraction = game->enemy_max_hp > 0 ?
        (float)game->enemy_hp / (float)game->enemy_max_hp : 0.0f;
    float player_health_fraction = game->player_max_hp > 0 ?
        (float)game->player_hp / (float)game->player_max_hp : 0.0f;
    const char *grade =
        game->pip_timing_grade == 2 ? "PERFECT" :
        (game->pip_timing_grade == 1 ? "GOOD" : "NORMAL");
    if (game->location != LAND_LOCATION_TRAINING) return;

    ki_td_soft_fill_rect(renderer, view, 8.0f, 38.0f, 202.0f, 43.0f,
                         UINT32_C(0x070b14), 0.88f);
    ki_td_soft_fill_rect(renderer, view, 8.0f, 38.0f, 3.0f, 43.0f,
                         land_actor_color(game, LAND_ACTOR_HERO), 1.0f);
    text_at(canvas, view, 17.0f, 42.0f, ready_attack_name(game), COLOR_INK);
    if (game->cast == LAND_CAST_LEGEND) {
        (void)snprintf(line, sizeof line, "HP %d/%d  CHARGE %d/%d",
                       game->player_hp, game->player_max_hp,
                       game->kilix_rifle_charge,
                       LAND_KILIX_MAX_RIFLE_CHARGE);
    } else if (game->cast == LAND_CAST_CHUMRUNNER) {
        (void)snprintf(line, sizeof line, "HP %d/%d MAG %d RES %d",
                       game->player_hp, game->player_max_hp,
                       game->rook_magazine, game->rook_reserve_ammo);
    } else if (game->cast == LAND_CAST_PLEB_BOUND) {
        (void)snprintf(line, sizeof line, "HP %d/%d  %s",
                       game->player_hp, game->player_max_hp, grade);
    } else {
        (void)snprintf(line, sizeof line, "HP %d/%d  DEF %d",
                       game->player_hp, game->player_max_hp,
                       game->player_defense);
    }
    text_at(canvas, view, 17.0f, 57.0f, line, COLOR_MUTED);
    ki_td_soft_fill_rect(renderer, view, 17.0f, 71.0f, 184.0f, 5.0f,
                         UINT32_C(0x20283a), 1.0f);
    if (player_health_fraction > 0.0f)
        ki_td_soft_fill_rect(
            renderer, view, 17.0f, 71.0f,
            184.0f * player_health_fraction, 5.0f,
            game->player_hp * 3 < game->player_max_hp ?
            UINT32_C(0xff786b) :
            land_actor_color(game, LAND_ACTOR_HERO), 1.0f);

    ki_td_soft_fill_rect(renderer, view, 218.0f, 38.0f, 254.0f, 43.0f,
                         UINT32_C(0x070b14), 0.88f);
    ki_td_soft_fill_rect(renderer, view, 469.0f, 38.0f, 3.0f, 43.0f,
                         COLOR_GOLD, 1.0f);
    text_at(canvas, view, 227.0f, 42.0f, land_enemy_name(game), COLOR_INK);
    ki_td_soft_fill_rect(renderer, view, 227.0f, 62.0f, 150.0f, 8.0f,
                         UINT32_C(0x20283a), 1.0f);
    if (health_fraction > 0.0f)
        ki_td_soft_fill_rect(renderer, view, 227.0f, 62.0f,
                             150.0f * health_fraction, 8.0f,
                             game->enemy_hp * 3 < game->enemy_max_hp ?
                             UINT32_C(0xff786b) :
                             land_actor_color(game, LAND_ACTOR_HERO),
                             1.0f);
    if (game->cast == LAND_CAST_LEGEND)
        (void)snprintf(line, sizeof line, "TAG %d",
                       game->enemy_hp);
    else
        (void)snprintf(line, sizeof line, "HP %d D%d",
                       game->enemy_hp,
                       game->enemy_defense);
    /* Right-aligned against the edge bar, so a longer reading grows into
     * the space the meter leaves rather than past the panel. */
    text_at(canvas, view, 466.0f - 8.0f * (float)strlen(line), 59.0f, line,
            COLOR_MUTED);

    if (game->enemy_action != LAND_ENEMY_ACTION_IDLE) {
        const char *warning;
        float width;
        if (game->enemy_action == LAND_ENEMY_ACTION_CHARGE)
            warning = "CHARGING";
        else if (game->enemy_action == LAND_ENEMY_ACTION_RELOAD)
            warning = "RELOADING";
        else
            warning = game->enemy_attack_landed ?
                      "RECOVERING" : "!! ATTACK !!";
        width = 24.0f + (float)strlen(warning) * 8.0f;
        ki_td_soft_fill_rect(renderer, view, 472.0f - width, 85.0f,
                             width, 18.0f, UINT32_C(0x070b14), 0.86f);
        ki_td_soft_fill_rect(renderer, view, 469.0f, 85.0f,
                             3.0f, 18.0f,
                             enemy_attack_color(game), 1.0f);
        text_at(canvas, view, 481.0f - width, 87.0f,
                warning, enemy_attack_color(game));
    }

    if (game->cast == LAND_CAST_PLEB_BOUND &&
        game->enemy_hp > 0) {
        float x = 30.0f;
        float width = 142.0f;
        float marker = x + width * pip_timing_marker(game);
        ki_td_soft_fill_rect(renderer, view, 20.0f, 88.0f, 163.0f, 25.0f,
                             UINT32_C(0x070b14), 0.78f);
        ki_td_soft_fill_rect(renderer, view, x, 99.0f, width, 6.0f,
                             UINT32_C(0x26324a), 1.0f);
        if (land_pip_timing_pattern(game) == 2) {
            ki_td_soft_fill_rect(renderer, view, x + 36.0f, 99.0f,
                                 12.0f, 6.0f, COLOR_GOLD, 1.0f);
            ki_td_soft_fill_rect(renderer, view, x + 108.0f, 99.0f,
                                 12.0f, 6.0f, COLOR_GOLD, 1.0f);
        } else {
            ki_td_soft_fill_rect(renderer, view, x + 64.0f, 99.0f,
                                 14.0f, 6.0f, COLOR_GOLD, 1.0f);
        }
        ki_td_soft_line(renderer, view, marker, 95.0f, marker, 109.0f,
                        2.0f, COLOR_INK, 1.0f);
        text_at(canvas, view, 29.0f, 86.0f, "TIMING", COLOR_MUTED);
    } else if (game->cast == LAND_CAST_CHUMRUNNER &&
               game->enemy_hp > 0) {
        (void)snprintf(line, sizeof line, "RANGE %d/%d  HIT %d%%",
                       land_rook_range(game),
                       LAND_PARITY_CHUM_PREDATOR_RANGE,
                       land_rook_hit_chance(game));
        ki_td_soft_fill_rect(renderer, view, 20.0f, 88.0f, 163.0f, 19.0f,
                             UINT32_C(0x070b14), 0.78f);
        text_at(canvas, view, 29.0f, 91.0f, line, COLOR_MUTED);
    }
}

static void draw_exit_prompt(ki_td_soft_renderer *renderer,
                             const ki_td_view *view, sr_canvas *canvas,
                             const land_state *game)
{
    float bob = (game->simulation_tick / 12u) % 2u == 0u ? 0.0f : 2.0f;
    if (game->location == LAND_LOCATION_TRAINING) {
        ki_td_soft_fill_rect(renderer, view, 0.0f, 205.0f, 72.0f, 28.0f,
                             UINT32_C(0x070b14), 0.78f);
        text_at(canvas, view, 7.0f, 208.0f, "< CREW", COLOR_INK);
        ki_td_soft_line(renderer, view, 12.0f - bob, 225.0f,
                        5.0f - bob, 219.0f, 2.0f, COLOR_GOLD, 1.0f);
    } else if (game->mode == LAND_MODE_ROOM) {
        ki_td_soft_fill_rect(renderer, view, 397.0f, 205.0f, 83.0f, 28.0f,
                             UINT32_C(0x070b14), 0.78f);
        /* Eight glyphs and a drawn chevron: "TRAINING >" as text ran eight
         * logical pixels past the right edge. */
        text_at(canvas, view, 403.0f, 208.0f, "TRAINING", COLOR_INK);
        ki_td_soft_line(renderer, view, 467.0f + bob, 213.0f,
                        472.0f + bob, 219.0f, 2.0f, COLOR_GOLD, 1.0f);
        ki_td_soft_line(renderer, view, 472.0f + bob, 219.0f,
                        467.0f + bob, 225.0f, 2.0f, COLOR_GOLD, 1.0f);
    }
}

static void draw_talk_prompt(ki_td_soft_renderer *renderer,
                             const ki_td_view *view, sr_canvas *canvas,
                             const land_state *game)
{
    char message[64];
    float x;
    float y;
    float bob;
    float prompt_width;
    int actor = game->nearest_actor;
    if (actor < LAND_ACTOR_ALLY_1 || game->mode != LAND_MODE_ROOM) return;
    land_actor_position(actor, &x, &y);
    bob = (game->simulation_tick / 12u) % 2u == 0u ? 0.0f : -2.0f;
    (void)snprintf(message, sizeof message, "ENTER  TALK - %s",
                   land_actor_name(game, actor));
    prompt_width = 22.0f + (float)strlen(message) * 8.0f;
    if (prompt_width < 154.0f) prompt_width = 154.0f;
    if (prompt_width > 464.0f) prompt_width = 464.0f;
    if (x - prompt_width * 0.5f < 8.0f)
        x = 8.0f + prompt_width * 0.5f;
    if (x + prompt_width * 0.5f > 472.0f)
        x = 472.0f - prompt_width * 0.5f;
    ki_td_soft_fill_rect(renderer, view, x - prompt_width * 0.5f,
                         70.0f + bob, prompt_width, 25.0f,
                         UINT32_C(0x070b14), 0.93f);
    ki_td_soft_fill_rect(renderer, view, x - prompt_width * 0.5f + 2.0f,
                         72.0f + bob, prompt_width - 4.0f, 21.0f,
                         UINT32_C(0x17243a), 0.96f);
    ki_td_soft_fill_rect(renderer, view, x - prompt_width * 0.5f + 2.0f,
                         72.0f + bob, 4.0f, 21.0f,
                         land_actor_color(game, actor), 1.0f);
    text_at(canvas, view, x - prompt_width * 0.5f + 13.0f,
            74.0f + bob, message, COLOR_INK);
    ki_td_soft_line(renderer, view, x, 95.0f + bob, x, 121.0f,
                    1.0f, land_actor_color(game, actor), 0.85f);
}

static void wrap_text(const char *text, size_t width, char lines[3][48])
{
    size_t line;
    size_t offset = 0u;
    size_t length = strlen(text);
    (void)memset(lines, 0, 3u * 48u);
    for (line = 0u; line < 3u && offset < length; ++line) {
        size_t remaining = length - offset;
        size_t take = remaining < width ? remaining : width;
        if (take < remaining) {
            size_t split = take;
            while (split > 20u && text[offset + split] != ' ') split--;
            if (split > 20u) take = split;
        }
        (void)snprintf(lines[line], 48u, "%.*s", (int)take, text + offset);
        offset += take;
        while (offset < length && text[offset] == ' ') offset++;
    }
}

/* One line when it fits the box, else two: many toasts are longer than the
 * 47 glyphs a line holds, and they used to run off the edge of the screen.
 * The taller box grows down rather than up, so it stays clear of the
 * training prompt above it. */
static void draw_toast(ki_td_soft_renderer *renderer, const ki_td_view *view,
                       sr_canvas *canvas, const land_state *game)
{
    char lines[3][48];
    float top;
    float height;
    if (game->toast_ticks <= 0 || game->mode != LAND_MODE_ROOM) return;
    wrap_text(game->toast, 47u, lines);
    top = lines[1][0] != '\0' ? 234.0f : 239.0f;
    height = lines[1][0] != '\0' ? 34.0f : 23.0f;
    ki_td_soft_fill_rect(renderer, view, 44.0f, top, 392.0f, height,
                         UINT32_C(0x070b14), 0.86f);
    ki_td_soft_fill_rect(renderer, view, 44.0f, top, 3.0f, height,
                         COLOR_GOLD, 1.0f);
    text_at(canvas, view, 55.0f, top + 1.0f, lines[0], COLOR_INK);
    if (lines[1][0] != '\0')
        text_at(canvas, view, 55.0f, top + 17.0f, lines[1], COLOR_INK);
}

static void visible_line(char output[48], const char *line, size_t *remaining)
{
    size_t length = strlen(line);
    size_t count = *remaining < length ? *remaining : length;
    (void)snprintf(output, 48u, "%.*s", (int)count, line);
    if (*remaining > count) *remaining -= count;
    else *remaining = 0u;
}

static bool portrait_image(const land_state *game,
                           const land_graphics *graphics,
                           int speaker, int expression,
                           ki_td_rgba8 *portrait)
{
    static const int standard_columns[LAND_CAST_COUNT][LAND_ACTOR_COUNT] = {
        {0, 0, 0, 0},
        {0, 1, 2, 3},
        {0, 2, 1, 3},
        {0, 1, 2, 3}
    };
    land_cast cast = land_visible_cast(game);
    if (portrait) *portrait = (ki_td_rgba8){0};
    if (!portrait || speaker < LAND_ACTOR_HERO ||
        speaker > LAND_ACTOR_ALLY_3)
        return false;
    if (cast == LAND_CAST_LEGEND) {
        static const int legend_columns[LAND_ACTOR_COUNT] = {0, 2, 3, 0};
        static const int legend_rows[LAND_ACTOR_COUNT] = {0, 0, 0, 1};
        int column = speaker == LAND_ACTOR_HERO && expression != 0 ?
                     1 : legend_columns[speaker];
        return land_graphics_cell(graphics, LAND_GRAPHIC_LEGEND_PORTRAITS,
                                  column, legend_rows[speaker], portrait);
    }
    return land_graphics_cell(
        graphics, portrait_graphic(cast),
        standard_columns[cast][speaker], expression != 0 ? 1 : 0, portrait);
}

static void draw_portrait(ki_td_soft_renderer *renderer,
                          const ki_td_view *view,
                          const land_state *game,
                          const land_graphics *graphics,
                          int speaker, int expression, float panel_y)
{
    uint32_t color = land_actor_color(game, speaker);
    ki_td_rgba8 portrait = {0};
    int portrait_y = (int)panel_y + 22;
    ki_td_soft_fill_rect(renderer, view, 19.0f, panel_y + 17.0f,
                         82.0f, 82.0f, UINT32_C(0x050810), 0.96f);
    ki_td_soft_fill_rect(renderer, view, 21.0f, panel_y + 19.0f,
                         78.0f, 78.0f, color, 0.86f);
    ki_td_soft_fill_rect(renderer, view, 24.0f, panel_y + 22.0f,
                         72.0f, 72.0f, UINT32_C(0x10192a), 1.0f);
    (void)portrait_image(game, graphics, speaker, expression, &portrait);
    kilix_ui_draw_portrait(renderer, view,
                           (ki_td_rect){24, portrait_y, 72, 72},
                           &portrait, 1.0f);
}

static void draw_dialogue(ki_td_soft_renderer *renderer,
                          const ki_td_view *view, sr_canvas *canvas,
                          const land_state *game,
                          const land_graphics *graphics)
{
    char lines[3][48];
    char visible[3][48];
    const char *visible_lines[3];
    char counter[24];
    const char *text = land_dialogue_text(game);
    int speaker = land_dialogue_speaker(game);
    int count = land_dialogue_count(game);
    size_t remaining = land_dialogue_visible_chars(game);
    size_t full_length = strlen(text);
    float slide = game->dialogue_age < 10 ?
                  (float)(10 - game->dialogue_age) : 0.0f;
    float panel_y = 151.0f + slide;
    int dialogue_y = (int)panel_y;
    kilix_ui_style panel_style;
    kilix_ui_style text_style;
    int line;
    int dot;

    ki_td_soft_fill_rect(renderer, view, 0.0f, 0.0f,
                         (float)LAND_LOGICAL_WIDTH,
                         (float)LAND_LOGICAL_HEIGHT,
                         UINT32_C(0x02040a), 0.20f);
    ki_td_soft_fill_rect(renderer, view, 9.0f, panel_y - 3.0f,
                         462.0f, 115.0f, UINT32_C(0x02040a), 0.72f);
    kilix_ui_style_init(&panel_style);
    panel_style.panel_color = COLOR_PANEL;
    panel_style.border_color = COLOR_PANEL;
    panel_style.text_color = COLOR_INK;
    panel_style.muted_color = COLOR_MUTED;
    panel_style.accent_color = land_actor_color(game, speaker);
    panel_style.panel_alpha = 0.97f;
    kilix_ui_draw_panel(renderer, view,
                        (ki_td_rect){12, dialogue_y, 456, 108},
                        &panel_style, NULL);
    ki_td_soft_fill_rect(renderer, view, 14.0f, panel_y + 2.0f,
                         452.0f, 3.0f,
                         land_actor_color(game, speaker), 0.95f);
    ki_td_soft_fill_rect(renderer, view, 104.0f, panel_y + 13.0f,
                         126.0f, 22.0f, COLOR_PANEL_LIGHT, 1.0f);
    ki_td_soft_fill_rect(renderer, view, 104.0f, panel_y + 13.0f,
                         4.0f, 22.0f,
                         land_actor_color(game, speaker), 1.0f);
    draw_portrait(renderer, view, game, graphics, speaker,
                  land_dialogue_expression(game), panel_y);

    text_at(canvas, view, 114.0f, panel_y + 15.0f,
            land_actor_name(game, speaker),
            land_actor_color(game, speaker));
    (void)snprintf(counter, sizeof counter, "%02d / %02d",
                   game->dialogue_beat + 1, count);
    text_at(canvas, view, 397.0f, panel_y + 15.0f, counter, COLOR_MUTED);

    wrap_text(text, 42u, lines);
    for (line = 0; line < 3; ++line) {
        visible_line(visible[line], lines[line], &remaining);
        visible_lines[line] = visible[line];
    }
    text_style = panel_style;
    text_style.panel_alpha = 0.0f;
    text_style.border_color = COLOR_PANEL;
    text_style.padding = 8;
    text_style.row_height = 18;
    text_style.font_scale = 1;
    kilix_ui_draw_dialogue(
        renderer, view, (ki_td_rect){103, dialogue_y + 14, 363, 77},
        &text_style, NULL, NULL, NULL, visible_lines, 3u, NULL);

    text_at(canvas, view, 111.0f, panel_y + 93.0f,
            land_dialogue_visible_chars(game) < full_length ?
            "ENTER / SPACE  REVEAL" :
            (game->dialogue_beat + 1 < count ?
             "ENTER / SPACE  NEXT" : "ENTER / SPACE  FINISH"),
            COLOR_MUTED);
    text_at(canvas, view, 306.0f, panel_y + 93.0f,
            "ESC  CLOSE", COLOR_MUTED);
    for (dot = 0; dot < count; ++dot) {
        float x = 417.0f + (float)dot * 7.0f;
        if (x < 461.0f)
            ki_td_soft_fill_circle(renderer, view, x, panel_y + 99.0f,
                                   dot == game->dialogue_beat ? 2.0f : 1.25f,
                                   dot <= game->dialogue_beat ?
                                   land_actor_color(game, speaker) :
                                   COLOR_MUTED,
                                   dot <= game->dialogue_beat ? 1.0f : 0.45f);
    }
}

static void draw_cast_select(ki_td_soft_renderer *renderer,
                             const ki_td_view *view, sr_canvas *canvas,
                             const land_state *game,
                             const land_graphics *graphics)
{
    static const char *const items[LAND_CAST_COUNT] = {
        "LEGEND OF KILIX  /  KILIX",
        "CHUMRUNNER       /  ROOK",
        "KILIX FANTASY    /  ARDEN",
        "PLEB BOUND       /  PIP"
    };
    char crew[112];
    ki_td_rgba8 portrait = {0};
    kilix_ui_focus focus;
    kilix_ui_style outer_style;
    kilix_ui_style list_style;
    land_cast cast = land_visible_cast(game);

    kilix_ui_focus_init(&focus, LAND_CAST_COUNT, LAND_CAST_COUNT);
    focus.selected = (size_t)cast;
    kilix_ui_style_init(&outer_style);
    outer_style.panel_color = COLOR_PANEL;
    outer_style.border_color = COLOR_GOLD;
    outer_style.text_color = COLOR_INK;
    outer_style.muted_color = COLOR_MUTED;
    outer_style.accent_color = land_actor_color(game, LAND_ACTOR_HERO);
    outer_style.panel_alpha = 0.98f;
    outer_style.padding = 8;

    list_style = outer_style;
    list_style.border_color = COLOR_PANEL_LIGHT;
    list_style.row_height = 22;
    list_style.padding = 6;

    ki_td_soft_fill_rect(renderer, view, 0.0f, 0.0f,
                         (float)LAND_LOGICAL_WIDTH,
                         (float)LAND_LOGICAL_HEIGHT,
                         UINT32_C(0x02040a), 0.66f);
    ki_td_soft_fill_rect(renderer, view, 19.0f, 32.0f, 442.0f, 207.0f,
                         UINT32_C(0x02040a), 0.78f);
    kilix_ui_draw_panel(renderer, view, (ki_td_rect){24, 36, 432, 199},
                        &outer_style, NULL);
    ki_td_soft_fill_rect(renderer, view, 27.0f, 39.0f, 426.0f, 3.0f,
                         land_actor_color(game, LAND_ACTOR_HERO), 1.0f);
    text_at(canvas, view, 41.0f, 48.0f, "CHOOSE YOUR HERO", COLOR_INK);
    text_at(canvas, view, 41.0f, 62.0f,
            "THEIR THREE ALLIES COME WITH THEM", COLOR_MUTED);

    kilix_ui_draw_list(renderer, view, (ki_td_rect){40, 81, 260, 104},
                       &list_style, NULL, &focus, items, NULL,
                       LAND_CAST_COUNT);
    kilix_ui_draw_panel(renderer, view, (ki_td_rect){309, 81, 131, 104},
                        &list_style, NULL);
    if (portrait_image(game, graphics, LAND_ACTOR_HERO, 0, &portrait))
        kilix_ui_draw_portrait(renderer, view,
                               (ki_td_rect){342, 87, 64, 64},
                               &portrait, 1.0f);
    ki_td_soft_fill_rect(renderer, view, 322.0f, 157.0f, 105.0f, 18.0f,
                         COLOR_PANEL_LIGHT, 1.0f);
    ki_td_soft_fill_rect(renderer, view, 322.0f, 157.0f, 3.0f, 18.0f,
                         land_actor_color(game, LAND_ACTOR_HERO), 1.0f);
    text_at(canvas, view, 331.0f, 159.0f,
            land_actor_name(game, LAND_ACTOR_HERO),
            land_actor_color(game, LAND_ACTOR_HERO));

    (void)snprintf(crew, sizeof crew, "CREW // %s  /  %s  /  %s",
                   land_actor_name(game, LAND_ACTOR_ALLY_1),
                   land_actor_name(game, LAND_ACTOR_ALLY_2),
                   land_actor_name(game, LAND_ACTOR_ALLY_3));
    text_at(canvas, view, 41.0f, 193.0f, crew, COLOR_INK);
    text_at(canvas, view, 41.0f, 215.0f,
            game->cast_confirmed ?
            "UP/DOWN SELECT   ENTER SWITCH   ESC BACK" :
            "UP/DOWN SELECT   ENTER START   ESC LEAVE",
            COLOR_MUTED);
}

bool land_render(ki_td_soft_renderer *renderer, const land_state *game,
                 const land_graphics *graphics)
{
    ki_td_fit_spec spec;
    ki_td_view view;
    sr_canvas *canvas;
    if (!renderer || !game || !graphics ||
        !ki_td_fit_spec_init(&spec, LAND_LOGICAL_WIDTH, LAND_LOGICAL_HEIGHT,
                             ki_td_soft_width(renderer),
                             ki_td_soft_height(renderer)))
        return false;
    spec.scale_policy = KI_TD_SCALE_PIXEL_ART;
    spec.integer_scale_threshold = 2.0f;
    spec.minimum_scale = 0.5f;
    if (!ki_td_view_fit(&view, &spec)) return false;

    ki_td_soft_clear(renderer, UINT32_C(0x05070d));
    draw_room(renderer, &view, game, graphics);
    draw_actors(renderer, &view, game, graphics);
    canvas = ki_td_soft_canvas(renderer);
    draw_name_tags(renderer, &view, canvas, game);
    draw_header(renderer, &view, canvas, game);
    draw_training_hud(renderer, &view, canvas, game);
    draw_exit_prompt(renderer, &view, canvas, game);
    draw_talk_prompt(renderer, &view, canvas, game);
    draw_toast(renderer, &view, canvas, game);
    if (game->mode == LAND_MODE_DIALOGUE)
        draw_dialogue(renderer, &view, canvas, game, graphics);
    else if (game->mode == LAND_MODE_CAST_SELECT)
        draw_cast_select(renderer, &view, canvas, game, graphics);
    return ki_td_soft_pack_rgba(renderer) != NULL;
}
