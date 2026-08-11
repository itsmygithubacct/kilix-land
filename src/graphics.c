#include "kilix_land.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define LAND_ASSET_MANIFEST "assets/graphics/manifest.json"
#define LAND_ASSET_MANIFEST_LIMIT (2u * 1024u * 1024u)
#define LAND_ASSET_CACHE_LIMIT (128u * 1024u * 1024u)


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
    {"fantasy-characters", 384u, 1632u, 24u, 68u},
    {"fantasy-portraits", 1080u, 160u, 27u, 4u},
    {"pleb-bound-characters", 1776u, 888u, 8u, 4u},
    {"pleb-bound-portraits", 1776u, 888u, 4u, 2u},
    {"legend-training", 512u, 352u, 1u, 1u},
    {"chumrunner-training", 320u, 180u, 1u, 1u},
    {"fantasy-training", 256u, 152u, 1u, 1u},
    {"pleb-bound-training", 320u, 180u, 1u, 1u},
    {"legend-enemies", 384u, 256u, 6u, 4u},
    {"chumrunner-enemies", 1776u, 888u, 8u, 4u},
    /* 15 columns are five banks of three poses (idle, action,
     * hurt-or-defeated) and 25 rows are enemy types; Land uses bank 1,
     * so columns 0..2 keep the meaning they always had. */
    {"fantasy-enemies", 1440u, 2800u, 15u, 25u},
    {"pleb-bound-enemies", 888u, 1776u, 4u, 8u}
};

static land_graphic character_graphic(land_cast cast)
{
    if (cast == LAND_CAST_CHUMRUNNER) return LAND_GRAPHIC_CHUM_CHARACTERS;
    if (cast == LAND_CAST_FANTASY) return LAND_GRAPHIC_FANTASY_CHARACTERS;
    return LAND_GRAPHIC_PLEB_CHARACTERS;
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

static bool build_hero_motion_cells(land_graphics *graphics)
{
    /*
     * Every cast's motion cells are the size of that cast's own atlas cell.
     * Fantasy is 16x24 pixel art where the others are 222x222, so a single
     * fixed stride would overrun the small cells and waste most of the large
     * ones. The buffer is one allocation with a per-cast offset.
     *
     * Every frame here is authored art. The down-walk pair is
     * front-walk-a/b and the up frame is back-idle, all straight off the
     * sheet; fantasy takes its own labelled columns. Nothing is fabricated
     * from a pose that the source game did not draw.
     *
     * The one transform left is a horizontal flip for the right-facing pair,
     * because chumrunner and pleb-bound author only a left side. That is a
     * reflection of their own art, not an invented pose -- but it is also the
     * last place Land puts a pixel somewhere the source game did not, and it
     * goes away the moment those two sheets grow a right-facing walk.
     */
    static const int FANTASY_SOURCE[LAND_HERO_MOTION_VARIANT_COUNT] = {
        4, 5, 13, 7, 8};
    size_t offsets[LAND_CAST_COUNT - 1];
    size_t total_bytes = 0u;
    int cast_index;
    for (cast_index = 0; cast_index < LAND_CAST_COUNT - 1; ++cast_index) {
        land_cast cast = (land_cast)(cast_index + 1);
        ki_td_rgba8 probe;
        if (!land_graphics_cell(graphics, character_graphic(cast), 0, 0,
                                &probe))
            return false;
        offsets[cast_index] = total_bytes;
        total_bytes += (size_t)probe.width * (size_t)probe.height * 4u *
                       (size_t)LAND_HERO_MOTION_VARIANT_COUNT;
    }
    graphics->hero_motion_pixels = malloc(total_bytes);
    if (!graphics->hero_motion_pixels) return false;
    for (cast_index = 0; cast_index < LAND_CAST_COUNT - 1; ++cast_index) {
        land_cast cast = (land_cast)(cast_index + 1);
        ki_td_rgba8 cells[6];
        size_t cell_bytes;
        int column;
        int variant;
        for (column = 0; column < 6; ++column)
            if (!land_graphics_cell(graphics, character_graphic(cast),
                                    column, 0, &cells[column]))
                return false;
        cell_bytes = (size_t)cells[0].width * (size_t)cells[0].height * 4u;
        for (variant = 0;
             variant < LAND_HERO_MOTION_VARIANT_COUNT; ++variant) {
            uint8_t *destination =
                graphics->hero_motion_pixels + offsets[cast_index] +
                (size_t)variant * cell_bytes;
            bool built;
            if (cast == LAND_CAST_FANTASY) {
                ki_td_rgba8 authored;
                built = land_graphics_cell(graphics,
                                           character_graphic(cast),
                                           FANTASY_SOURCE[variant], 0,
                                           &authored);
                if (built) copy_cell(destination, &authored);
            }
            else if (variant == LAND_HERO_MOTION_DOWN_A) {
                copy_cell(destination, &cells[1]);
                built = true;
            }
            else if (variant == LAND_HERO_MOTION_DOWN_B) {
                copy_cell(destination, &cells[2]);
                built = true;
            }
            else if (variant == LAND_HERO_MOTION_UP_STEP) {
                copy_cell(destination, &cells[3]);
                built = true;
            }
            else
                built = build_full_mirror(
                    destination,
                    &cells[
                        variant == LAND_HERO_MOTION_MIRRORED_SIDE ? 4 : 5]);
            if (!built) return false;
            graphics->hero_motion_cells[cast_index][variant] =
                ki_td_rgba8_make(destination, cells[0].width,
                                 cells[0].height);
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
    limits.max_file_bytes = 16u * 1024u * 1024u;
    limits.max_image_bytes = 16u * 1024u * 1024u;
    /* The fantasy cast's enemy atlas is 1440x2800: 25 enemy types down,
     * five banks of three poses across. 2048 rejected it, and because a
     * single failed atlas tears down the whole set, that surfaced as
     * "loaded=0/21" rather than as one missing sheet. Soft raster keeps
     * these in ordinary memory, so the ceiling is about bounding a bad
     * file, not a GPU texture limit. */
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
    if (!build_hero_motion_cells(graphics)) {
        land_graphics_shutdown(graphics);
        return false;
    }
    return true;
}

void land_graphics_shutdown(land_graphics *graphics)
{
    if (!graphics) return;
    free(graphics->hero_motion_pixels);
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
