#include "kilix_land.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define LAND_ASSET_MANIFEST "assets/graphics/manifest.json"
#define LAND_ASSET_MANIFEST_LIMIT (2u * 1024u * 1024u)
#define LAND_ASSET_CACHE_LIMIT (128u * 1024u * 1024u)
#define LAND_CHARACTER_CELL 222u

typedef struct sprite_metrics {
    int left;
    int right;
    int top;
    int bottom;
    double spine_x;
} sprite_metrics;

typedef struct graphic_spec {
    const char *id;
    uint32_t width;
    uint32_t height;
    uint32_t columns;
    uint32_t rows;
} graphic_spec;

static const graphic_spec GRAPHIC_SPECS[LAND_GRAPHICS_COUNT] = {
    {"legend-room", 960u, 540u, 1u, 1u},
    {"chumrunner-room", 960u, 540u, 1u, 1u},
    {"fantasy-room", 960u, 540u, 1u, 1u},
    {"pleb-bound-room", 960u, 540u, 1u, 1u},
    {"legend-player", 1024u, 512u, 16u, 8u},
    {"legend-npcs", 256u, 256u, 4u, 4u},
    {"legend-portraits", 384u, 288u, 4u, 3u},
    {"chumrunner-characters", 1776u, 888u, 8u, 4u},
    {"chumrunner-portraits", 1776u, 888u, 4u, 2u},
    {"fantasy-characters", 128u, 1632u, 8u, 68u},
    {"fantasy-portraits", 1080u, 160u, 27u, 4u},
    {"pleb-bound-characters", 1776u, 888u, 8u, 4u},
    {"pleb-bound-portraits", 1776u, 888u, 4u, 2u},
    {"legend-training", 512u, 352u, 1u, 1u},
    {"chumrunner-training", 320u, 180u, 1u, 1u},
    {"fantasy-training", 256u, 152u, 1u, 1u},
    {"pleb-bound-training", 320u, 180u, 1u, 1u},
    {"legend-enemies", 384u, 256u, 6u, 4u},
    {"chumrunner-enemies", 1776u, 888u, 8u, 4u},
    {"fantasy-enemies", 1440u, 2800u, 15u, 25u},
    {"pleb-bound-enemies", 888u, 1776u, 4u, 8u}
};

static land_graphic character_graphic(land_cast cast)
{
    if (cast == LAND_CAST_CHUMRUNNER) return LAND_GRAPHIC_CHUM_CHARACTERS;
    if (cast == LAND_CAST_FANTASY) return LAND_GRAPHIC_FANTASY_CHARACTERS;
    return LAND_GRAPHIC_PLEB_CHARACTERS;
}

static bool measure_sprite(const ki_td_rgba8 *image, sprite_metrics *metrics)
{
    uint64_t spine_total = 0u;
    uint64_t spine_pixels = 0u;
    int y;
    if (!ki_td_rgba8_is_valid(image) || !metrics) return false;
    metrics->left = image->width;
    metrics->right = -1;
    metrics->top = image->height;
    metrics->bottom = -1;
    for (y = 0; y < image->height; ++y) {
        const uint8_t *row =
            image->pixels + (size_t)y * image->stride;
        int x;
        for (x = 0; x < image->width; ++x) {
            if (row[(size_t)x * 4u + 3u] < UINT8_C(8)) continue;
            if (x < metrics->left) metrics->left = x;
            if (x > metrics->right) metrics->right = x;
            if (y < metrics->top) metrics->top = y;
            if (y > metrics->bottom) metrics->bottom = y;
        }
    }
    if (metrics->bottom < metrics->top ||
        metrics->right < metrics->left)
        return false;
    {
        int upper_bottom =
            metrics->top + (metrics->bottom - metrics->top + 1) * 2 / 5;
        for (y = metrics->top; y <= upper_bottom; ++y) {
            const uint8_t *row =
                image->pixels + (size_t)y * image->stride;
            int x;
            for (x = metrics->left; x <= metrics->right; ++x) {
                if (row[(size_t)x * 4u + 3u] < UINT8_C(8)) continue;
                spine_total += (uint64_t)(unsigned int)x;
                ++spine_pixels;
            }
        }
    }
    metrics->spine_x = spine_pixels > 0u ?
        (double)spine_total / (double)spine_pixels :
        ((double)metrics->left + (double)metrics->right) * 0.5;
    return true;
}

static void copy_cell(uint8_t *destination, const ki_td_rgba8 *source)
{
    int y;
    for (y = 0; y < source->height; ++y)
        (void)memcpy(
            destination + (size_t)y * (size_t)source->width * 4u,
            source->pixels + (size_t)y * source->stride,
            (size_t)source->width * 4u);
}

static bool build_authored_copy(uint8_t *destination,
                                const ki_td_rgba8 *source)
{
    if (!destination || !ki_td_rgba8_is_valid(source)) return false;
    copy_cell(destination, source);
    return true;
}

static bool build_leg_motion_cell(uint8_t *destination,
                                  const ki_td_rgba8 *base,
                                  const ki_td_rgba8 *step,
                                  bool mirror_step)
{
    sprite_metrics base_metrics;
    sprite_metrics step_metrics;
    int half_width;
    int left;
    int right;
    int split;
    int step_split;
    int offset_y;
    int y;
    if (!destination || !measure_sprite(base, &base_metrics) ||
        !measure_sprite(step, &step_metrics))
        return false;
    copy_cell(destination, base);
    half_width =
        (base_metrics.right - base_metrics.left + 1) * 31 / 100;
    if (half_width < 24) half_width = 24;
    left = (int)(base_metrics.spine_x + 0.5) - half_width;
    right = (int)(base_metrics.spine_x + 0.5) + half_width;
    if (left < 0) left = 0;
    if (right >= (int)LAND_CHARACTER_CELL)
        right = (int)LAND_CHARACTER_CELL - 1;
    split = base_metrics.bottom -
        (base_metrics.bottom - base_metrics.top + 1) * 34 / 100;
    step_split = step_metrics.bottom -
        (step_metrics.bottom - step_metrics.top + 1) * 34 / 100;
    offset_y = base_metrics.bottom - step_metrics.bottom;
    for (y = split; y < (int)LAND_CHARACTER_CELL; ++y) {
        int source_y = y - offset_y;
        int x;
        for (x = left; x <= right; ++x) {
            uint8_t *output =
                destination +
                ((size_t)y * LAND_CHARACTER_CELL + (size_t)x) * 4u;
            double aligned_x =
                (double)x - base_metrics.spine_x + step_metrics.spine_x;
            int source_x;
            (void)memset(output, 0, 4u);
            if (mirror_step)
                aligned_x = step_metrics.spine_x -
                            (aligned_x - step_metrics.spine_x);
            source_x = (int)(aligned_x + 0.5);
            if (source_y < step_split || source_y < 0 ||
                source_y >= step->height || source_x < 0 ||
                source_x >= step->width)
                continue;
            (void)memcpy(
                output,
                step->pixels + (size_t)source_y * step->stride +
                    (size_t)source_x * 4u,
                4u);
        }
    }
    return true;
}

/*
 * Arden's and Pip's authored front-walk cells turn the whole pose diagonally.
 * For straight-down movement, keep the frontal idle cell byte-for-byte above
 * the knees and alternately lift one of its existing legs.
 */
static bool build_straight_step_cell(uint8_t *destination,
                                     const ki_td_rgba8 *base,
                                     bool lift_left)
{
    sprite_metrics metrics;
    int half_width;
    int center;
    int left;
    int right;
    int split;
    int lift;
    int leg_height;
    int y;
    if (!destination || !measure_sprite(base, &metrics)) return false;
    copy_cell(destination, base);
    center = (int)(metrics.spine_x + 0.5);
    half_width = (metrics.right - metrics.left + 1) * 24 / 100;
    if (half_width < 20) half_width = 20;
    left = center - half_width;
    right = center + half_width;
    if (left < 0) left = 0;
    if (right >= (int)LAND_CHARACTER_CELL)
        right = (int)LAND_CHARACTER_CELL - 1;
    leg_height = (metrics.bottom - metrics.top + 1) * 27 / 100;
    split = metrics.bottom - leg_height;
    lift = (metrics.bottom - metrics.top + 24) / 24;
    if (lift < 5) lift = 5;
    for (y = split; y < (int)LAND_CHARACTER_CELL; ++y) {
        int x;
        for (x = left; x <= right; ++x)
            (void)memset(
                destination +
                    ((size_t)y * LAND_CHARACTER_CELL + (size_t)x) * 4u,
                0, 4u);
    }
    for (y = split; y <= metrics.bottom; ++y) {
        int x;
        for (x = left; x <= right; ++x) {
            bool pixel_is_left = x < center;
            int shift = pixel_is_left == lift_left && leg_height > 0 ?
                lift * (y - split) / leg_height : 0;
            int target_y = y - shift;
            if (target_y < 0 ||
                target_y >= (int)LAND_CHARACTER_CELL)
                continue;
            (void)memcpy(
                destination +
                    ((size_t)target_y * LAND_CHARACTER_CELL +
                     (size_t)x) * 4u,
                base->pixels + (size_t)y * base->stride +
                    (size_t)x * 4u,
                4u);
        }
    }
    return true;
}

/*
 * Kilix's eight authored walk cells are variations of one held stride. Keep
 * the authored upper-body pose, but reflect only the legs around the measured
 * torso center to create the missing opposite footfall.
 */
static bool build_opposite_step_cell(uint8_t *destination,
                                     const ki_td_rgba8 *step)
{
    sprite_metrics metrics;
    int center;
    int half_width;
    int left;
    int right;
    int split;
    int y;
    if (!destination || !measure_sprite(step, &metrics)) return false;
    copy_cell(destination, step);
    center = (int)(metrics.spine_x + 0.5);
    half_width = (metrics.right - metrics.left + 1) * 24 / 100;
    if (half_width < 7) half_width = 7;
    left = center - half_width;
    right = center + half_width;
    if (left < 0) left = 0;
    if (right >= step->width) right = step->width - 1;
    split = metrics.bottom -
        (metrics.bottom - metrics.top + 1) * 29 / 100;
    for (y = split; y <= metrics.bottom; ++y) {
        int x;
        for (x = left; x <= right; ++x) {
            uint8_t *output =
                destination +
                ((size_t)y * (size_t)step->width + (size_t)x) * 4u;
            int source_x = center * 2 - x;
            (void)memset(output, 0, 4u);
            if (source_x < 0 || source_x >= step->width) continue;
            (void)memcpy(
                output,
                step->pixels + (size_t)y * step->stride +
                    (size_t)source_x * 4u,
                4u);
        }
    }
    return true;
}

static bool build_full_mirror(uint8_t *destination,
                              const ki_td_rgba8 *source)
{
    int y;
    if (!destination || !ki_td_rgba8_is_valid(source)) return false;
    for (y = 0; y < source->height; ++y) {
        int x;
        for (x = 0; x < source->width; ++x) {
            const uint8_t *pixel =
                source->pixels + (size_t)y * source->stride +
                (size_t)(source->width - 1 - x) * 4u;
            uint8_t *output =
                destination +
                ((size_t)y * (size_t)source->width + (size_t)x) * 4u;
            (void)memcpy(output, pixel, 4u);
        }
    }
    return true;
}

static bool build_legend_opposite_steps(land_graphics *graphics)
{
    const size_t cell_bytes = 64u * 64u * 4u;
    int facing;
    graphics->legend_opposite_step_pixels =
        malloc(cell_bytes * 4u);
    if (!graphics->legend_opposite_step_pixels) return false;
    for (facing = LAND_FACING_DOWN; facing <= LAND_FACING_UP; ++facing) {
        uint8_t *destination =
            graphics->legend_opposite_step_pixels +
            (size_t)facing * cell_bytes;
        ki_td_rgba8 step;
        if (!land_graphics_cell(graphics, LAND_GRAPHIC_LEGEND_PLAYER,
                                4, facing * 2, &step) ||
            step.width != 64 || step.height != 64 ||
            !build_opposite_step_cell(destination, &step))
            return false;
        graphics->legend_opposite_step_cells[facing] =
            ki_td_rgba8_make(destination, 64, 64);
    }
    return true;
}

/*
 * Kilix Fantasy authors every field pose itself: two front strides, a back
 * pose, and a right-facing side pair. Land replays those cells directly and
 * only has to mirror the side pair, so none of the synthetic leg work below
 * applies to that cast.
 */
static bool cast_authors_full_motion(land_cast cast)
{
    return cast == LAND_CAST_FANTASY;
}

static void character_cell_size(land_cast cast, int *width, int *height)
{
    const graphic_spec *spec = &GRAPHIC_SPECS[character_graphic(cast)];
    *width = (int)(spec->width / spec->columns);
    *height = (int)(spec->height / spec->rows);
}

static bool build_hero_motion_cells(land_graphics *graphics)
{
    size_t total_bytes = 0u;
    size_t offset = 0u;
    int cast_index;
    for (cast_index = 0; cast_index < LAND_CAST_COUNT - 1; ++cast_index) {
        int width;
        int height;
        character_cell_size((land_cast)(cast_index + 1), &width, &height);
        if (width <= 0 || height <= 0) return false;
        total_bytes += (size_t)width * (size_t)height * 4u *
            LAND_HERO_MOTION_VARIANT_COUNT;
    }
    graphics->hero_motion_pixels = malloc(total_bytes);
    if (!graphics->hero_motion_pixels) return false;
    for (cast_index = 0; cast_index < LAND_CAST_COUNT - 1; ++cast_index) {
        land_cast cast = (land_cast)(cast_index + 1);
        bool authored = cast_authors_full_motion(cast);
        ki_td_rgba8 cells[6];
        int width;
        int height;
        int column;
        int variant;
        character_cell_size(cast, &width, &height);
        if (!authored &&
            (width != (int)LAND_CHARACTER_CELL ||
             height != (int)LAND_CHARACTER_CELL))
            return false;
        for (column = 0; column < 6; ++column)
            if (!land_graphics_cell(graphics, character_graphic(cast),
                                    column, 0, &cells[column]) ||
                cells[column].width != width ||
                cells[column].height != height)
                return false;
        for (variant = 0;
             variant < LAND_HERO_MOTION_VARIANT_COUNT; ++variant) {
            uint8_t *destination =
                graphics->hero_motion_pixels + offset;
            bool built;
            if (variant == LAND_HERO_MOTION_DOWN_A)
                built = authored ?
                    build_authored_copy(destination, &cells[1]) :
                    (cast == LAND_CAST_CHUMRUNNER ?
                        build_leg_motion_cell(
                            destination, &cells[0], &cells[1], false) :
                        build_straight_step_cell(
                            destination, &cells[0], false));
            else if (variant == LAND_HERO_MOTION_DOWN_B)
                built = authored ?
                    build_authored_copy(destination, &cells[2]) :
                    (cast == LAND_CAST_CHUMRUNNER ?
                        build_leg_motion_cell(
                            destination, &cells[0], &cells[2], false) :
                        build_straight_step_cell(
                            destination, &cells[0], true));
            else if (variant == LAND_HERO_MOTION_UP_STEP)
                built = authored ?
                    build_authored_copy(destination, &cells[3]) :
                    build_leg_motion_cell(
                        destination, &cells[3], &cells[3], true);
            else
                built = build_full_mirror(
                    destination,
                    &cells[
                        variant == LAND_HERO_MOTION_MIRRORED_SIDE ? 4 : 5]);
            if (!built) return false;
            graphics->hero_motion_cells[cast_index][variant] =
                ki_td_rgba8_make(destination, width, height);
            offset += (size_t)width * (size_t)height * 4u;
        }
    }
    return true;
}

/*
 * Kilix Fantasy authors its bestiary facing right and mirrors it in battle.
 * Land's opponent stands to the right of the training range, so it needs the
 * same mirrored presentation; build those cells once at load time.
 */
static bool build_fantasy_enemy_mirrors(land_graphics *graphics)
{
    const graphic_spec *spec = &GRAPHIC_SPECS[LAND_GRAPHIC_FANTASY_ENEMIES];
    int cell_width = (int)(spec->width / spec->columns);
    int cell_height = (int)(spec->height / spec->rows);
    size_t cell_bytes = (size_t)cell_width * (size_t)cell_height * 4u;
    size_t offset = 0u;
    int identity;
    if (cell_width <= 0 || cell_height <= 0) return false;
    graphics->fantasy_enemy_pixels =
        malloc(cell_bytes * LAND_PARITY_FANTASY_ENEMY_COUNT *
               LAND_PARITY_FANTASY_ENEMY_POSE_COUNT);
    if (!graphics->fantasy_enemy_pixels) return false;
    for (identity = 0; identity < LAND_PARITY_FANTASY_ENEMY_COUNT;
         ++identity) {
        int pose;
        for (pose = 0; pose < LAND_PARITY_FANTASY_ENEMY_POSE_COUNT; ++pose) {
            uint8_t *destination =
                graphics->fantasy_enemy_pixels + offset;
            ki_td_rgba8 source;
            if (!land_graphics_cell(
                    graphics, LAND_GRAPHIC_FANTASY_ENEMIES,
                    (identity % LAND_PARITY_FANTASY_ENEMY_IDENTITIES_PER_ROW) *
                        LAND_PARITY_FANTASY_ENEMY_POSE_COUNT + pose,
                    identity / LAND_PARITY_FANTASY_ENEMY_IDENTITIES_PER_ROW,
                    &source) ||
                source.width != cell_width ||
                source.height != cell_height ||
                !build_full_mirror(destination, &source))
                return false;
            graphics->fantasy_enemy_cells[identity][pose] =
                ki_td_rgba8_make(destination, cell_width, cell_height);
            offset += cell_bytes;
        }
    }
    return true;
}

static bool load_graphic(land_graphics *graphics,
                         const kilix_asset_locator *locator,
                         const kilix_asset_limits *limits,
                         int index)
{
    const graphic_spec *spec = &GRAPHIC_SPECS[index];
    const kilix_asset_manifest_atlas *record;
    const kilix_asset_image *image = NULL;
    char path[1024];
    kilix_asset_status status;

    record = kilix_asset_manifest_find_atlas(&graphics->manifest, spec->id);
    if (!record || record->width != spec->width ||
        record->height != spec->height ||
        record->columns != spec->columns || record->rows != spec->rows)
        return false;
    status = kilix_asset_resolve(locator, record->path, path, sizeof path);
    if (status == KILIX_ASSET_OK)
        status = kilix_asset_cache_load_png(&graphics->cache, path, limits,
                                            &image);
    if (status != KILIX_ASSET_OK || !image ||
        image->width != spec->width || image->height != spec->height ||
        !kilix_asset_atlas_init_grid(&graphics->atlases[index], image,
                                     spec->columns, spec->rows))
        return false;
    graphics->images[index] = image;
    return true;
}

bool land_graphics_init(land_graphics *graphics, const char *asset_root)
{
    kilix_asset_locator locator;
    kilix_asset_limits limits;
    const char *root = asset_root && asset_root[0] != '\0' ? asset_root : ".";
    char manifest_path[1024];
    kilix_asset_status status;
    int index;

    if (!graphics) return false;
    (void)memset(graphics, 0, sizeof *graphics);
    kilix_asset_locator_init(&locator);
    locator.environment_variable = "KILIX_LAND_ASSETS";
    locator.source_root = root;
    locator.installed_root = root;
    kilix_asset_limits_init(&limits);
    /*
     * Kilix Fantasy's bestiary sheet is a 1440x2800 column of 125 visual
     * identities, so the runtime ceilings have to clear a tall source atlas.
     */
    limits.max_file_bytes = 32u * 1024u * 1024u;
    limits.max_image_bytes = 32u * 1024u * 1024u;
    limits.max_dimension = 4096u;
    if (!kilix_asset_cache_init(&graphics->cache,
                                (size_t)LAND_GRAPHICS_COUNT,
                                LAND_ASSET_CACHE_LIMIT))
        return false;
    graphics->cache_ready = true;

    status = kilix_asset_resolve(&locator, LAND_ASSET_MANIFEST,
                                 manifest_path, sizeof manifest_path);
    if (status == KILIX_ASSET_OK)
        status = kilix_asset_manifest_load_json(
            &graphics->manifest, manifest_path, LAND_ASSET_MANIFEST_LIMIT);
    if (status != KILIX_ASSET_OK || !graphics->manifest.game ||
        strcmp(graphics->manifest.game, "kilix-land") != 0) {
        land_graphics_shutdown(graphics);
        return false;
    }
    for (index = 0; index < LAND_GRAPHICS_COUNT; ++index) {
        if (!load_graphic(graphics, &locator, &limits, index)) {
            land_graphics_shutdown(graphics);
            return false;
        }
    }
    if (!build_legend_opposite_steps(graphics) ||
        !build_hero_motion_cells(graphics) ||
        !build_fantasy_enemy_mirrors(graphics)) {
        land_graphics_shutdown(graphics);
        return false;
    }
    return true;
}

void land_graphics_shutdown(land_graphics *graphics)
{
    if (!graphics) return;
    free(graphics->legend_opposite_step_pixels);
    free(graphics->hero_motion_pixels);
    free(graphics->fantasy_enemy_pixels);
    kilix_asset_manifest_clear(&graphics->manifest);
    if (graphics->cache_ready) kilix_asset_cache_clear(&graphics->cache);
    (void)memset(graphics, 0, sizeof *graphics);
}

size_t land_graphics_loaded_count(const land_graphics *graphics)
{
    size_t count = 0u;
    int index;
    if (!graphics) return 0u;
    for (index = 0; index < LAND_GRAPHICS_COUNT; ++index)
        if (kilix_asset_image_is_valid(graphics->images[index])) ++count;
    return count;
}

bool land_graphics_image(const land_graphics *graphics, land_graphic graphic,
                         ki_td_rgba8 *image)
{
    const kilix_asset_image *asset;
    if (image) *image = (ki_td_rgba8){0};
    if (!graphics || !image || graphic < LAND_GRAPHIC_LEGEND_ROOM ||
        graphic >= LAND_GRAPHICS_COUNT)
        return false;
    asset = graphics->images[graphic];
    if (!kilix_asset_image_is_valid(asset) ||
        asset->width > (uint32_t)INT_MAX || asset->height > (uint32_t)INT_MAX)
        return false;
    *image = ki_td_rgba8_make(asset->pixels, (int)asset->width,
                              (int)asset->height);
    image->stride = asset->stride;
    return ki_td_rgba8_is_valid(image);
}

bool land_graphics_cell(const land_graphics *graphics, land_graphic graphic,
                        int column, int row, ki_td_rgba8 *image)
{
    kilix_asset_region region;
    if (image) *image = (ki_td_rgba8){0};
    if (!graphics || !image || graphic < LAND_GRAPHIC_LEGEND_ROOM ||
        graphic >= LAND_GRAPHICS_COUNT || column < 0 || row < 0)
        return false;
    region = kilix_asset_atlas_cell(&graphics->atlases[graphic],
                                    (uint32_t)column, (uint32_t)row);
    if (!kilix_asset_region_is_valid(&region) ||
        region.width > (uint32_t)INT_MAX ||
        region.height > (uint32_t)INT_MAX)
        return false;
    *image = ki_td_rgba8_make(region.pixels, (int)region.width,
                              (int)region.height);
    image->stride = region.stride;
    return ki_td_rgba8_is_valid(image);
}

bool land_graphics_legend_opposite_step(
    const land_graphics *graphics, land_facing facing, ki_td_rgba8 *image)
{
    if (image) *image = (ki_td_rgba8){0};
    if (!graphics || !image || facing < LAND_FACING_DOWN ||
        facing > LAND_FACING_UP)
        return false;
    *image = graphics->legend_opposite_step_cells[facing];
    return ki_td_rgba8_is_valid(image);
}

bool land_graphics_fantasy_enemy_cell(
    const land_graphics *graphics, int identity, int pose,
    ki_td_rgba8 *image)
{
    if (image) *image = (ki_td_rgba8){0};
    if (!graphics || !image || identity < 0 ||
        identity >= LAND_PARITY_FANTASY_ENEMY_COUNT || pose < 0 ||
        pose >= LAND_PARITY_FANTASY_ENEMY_POSE_COUNT)
        return false;
    *image = graphics->fantasy_enemy_cells[identity][pose];
    return ki_td_rgba8_is_valid(image);
}

bool land_graphics_hero_motion_cell(
    const land_graphics *graphics, land_cast cast,
    land_hero_motion_variant variant, ki_td_rgba8 *image)
{
    int cast_index;
    if (image) *image = (ki_td_rgba8){0};
    if (!graphics || !image || cast < LAND_CAST_CHUMRUNNER ||
        cast > LAND_CAST_PLEB_BOUND ||
        variant < LAND_HERO_MOTION_DOWN_A ||
        variant > LAND_HERO_MOTION_MIRRORED_WALK)
        return false;
    cast_index = (int)cast - 1;
    *image = graphics->hero_motion_cells[cast_index][variant];
    return ki_td_rgba8_is_valid(image);
}
