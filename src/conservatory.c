#include "conservatory.h"

#include "pg_save.h"
#include "pg_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LAND_PLANT_SEED UINT64_C(0x6c616e64)
#define LAND_PLANT_ASSETS "third_party/pleb-plant-grower/assets"

bool land_conservatory_abi_ok(void)
{
    return pg_state_size() == sizeof(pg_state);
}

/*
 * The kernel boot id, read once per process. Sixteen zero bytes are a valid
 * answer: the plant's gap policy treats a host without one as a single long
 * boot and falls back to the wall gap (pleb-plant-grower D-095).
 */
static void read_boot_id(uint8_t out[PG_BOOT_ID_BYTES])
{
    static uint8_t cached[PG_BOOT_ID_BYTES];
    static bool loaded;
    if (!loaded) {
        FILE *file = fopen("/proc/sys/kernel/random/boot_id", "re");
        char text[64];
        loaded = true;
        if (file && fgets(text, (int)sizeof text, file)) {
            size_t index;
            size_t digits = 0u;
            for (index = 0u; text[index] != '\0' &&
                             digits < (size_t)PG_BOOT_ID_BYTES * 2u;
                 ++index) {
                char c = text[index];
                int value;
                if (c >= '0' && c <= '9') value = c - '0';
                else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
                else continue;
                if (digits % 2u == 0u)
                    cached[digits / 2u] = (uint8_t)(value << 4);
                else
                    cached[digits / 2u] |= (uint8_t)value;
                digits++;
            }
        }
        if (file) (void)fclose(file);
    }
    (void)memcpy(out, cached, sizeof cached);
}

pg_now land_conservatory_now(void)
{
    pg_now now;
    struct timespec wall;
    struct timespec boot;
    struct tm local;
    time_t seconds;
    (void)memset(&now, 0, sizeof now);
    if (clock_gettime(CLOCK_REALTIME, &wall) == 0)
        now.wall_s = (int64_t)wall.tv_sec;
    /* Boottime keeps counting across suspend; a closed lid is still time
     * the plant lived through. Monotonic is the honest fallback: it can only
     * under-credit. */
#if defined(CLOCK_BOOTTIME)
    if (clock_gettime(CLOCK_BOOTTIME, &boot) == 0)
        now.boot_s = (int64_t)boot.tv_sec;
#else
    if (clock_gettime(CLOCK_MONOTONIC, &boot) == 0)
        now.boot_s = (int64_t)boot.tv_sec;
#endif
    read_boot_id(now.boot_id);
    /* Every sample: the offset is a simulation input, and a session left
     * running across a DST change must see the new one. */
    seconds = (time_t)now.wall_s;
    if (localtime_r(&seconds, &local)) {
        long offset = local.tm_gmtoff / 60;
        if (offset >= PG_TZ_OFFSET_MIN && offset <= PG_TZ_OFFSET_MAX)
            now.tz_offset_minutes = (int32_t)offset;
    }
    return now;
}

bool land_conservatory_data_root(char *out, size_t size)
{
    const char *data_home = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    int written;
    if (!out || size == 0u) return false;
    if (data_home && data_home[0] == '/')
        written = snprintf(out, size, "%s/kilix-land", data_home);
    else if (home && home[0] == '/')
        written = snprintf(out, size, "%s/.local/share/kilix-land", home);
    else
        return false;
    return written > 0 && (size_t)written < size;
}

static void reset_input(pg_input *input)
{
    (void)memset(input, 0, sizeof *input);
    input->pointer_x = PG_POINTER_NONE;
    input->pointer_y = PG_POINTER_NONE;
}

void land_conservatory_init(land_conservatory *conservatory)
{
    if (!conservatory) return;
    (void)memset(conservatory, 0, sizeof *conservatory);
    reset_input(&conservatory->input);
}

void land_conservatory_enter(land_conservatory *conservatory,
                             const char *asset_root, const char *data_root,
                             pg_now now)
{
    pg_catchup_report report;
    bool first = false;
    if (!conservatory) return;
    if (!conservatory->started) {
        char plant_assets[1024];
        char error[160];
        bool recovered = false;
        int written = snprintf(plant_assets, sizeof plant_assets, "%s/%s",
                               asset_root ? asset_root : ".",
                               LAND_PLANT_ASSETS);
        pg_init(&conservatory->plant, LAND_PLANT_SEED);
        /* A missing plate set is not fatal: pg_render draws its designed
         * procedural stage when handed no graphics. */
        conservatory->graphics_ready =
            written > 0 && (size_t)written < sizeof plant_assets &&
            pg_graphics_init(&conservatory->graphics, plant_assets,
                             error, sizeof error);
        /* Without a data root the plant still plays, it just does not
         * persist; it never falls back to the standalone game's save. The
         * standalone's save override would take precedence over Land's root
         * inside pg_store_open, so it is dropped first. */
        (void)unsetenv("PLEB_PLANT_CONFIG_HOME");
        conservatory->store_ready =
            data_root && data_root[0] == '/' &&
            pg_store_open(&conservatory->store, data_root);
        if (conservatory->store_ready)
            (void)pg_store_load(&conservatory->plant, &conservatory->store,
                                &recovered);
        conservatory->started = true;
        first = true;
    }
    reset_input(&conservatory->input);
    (void)memset(&report, 0, sizeof report);
    (void)pg_advance(&conservatory->plant, now, &report);
    conservatory->last_advance_wall_s = now.wall_s;
    if (report.entry_count > 0u || report.abandoned)
        pg_ui_goto(&conservatory->plant.ui, (uint8_t)PG_SCREEN_AWAY);
    else if (first)
        pg_ui_goto(&conservatory->plant.ui,
                   pg_ui_entry_screen(
                       &conservatory->plant,
                       conservatory->store_ready ?
                       pg_store_last_status(&conservatory->store) :
                       (uint8_t)PG_STORE_UNAVAILABLE));
}

static bool key_letter(const kittykb_event *event, char lower)
{
    char upper = (char)(lower - 'a' + 'A');
    return kittykb_event_matches_key(event, (uint32_t)(unsigned char)lower) ||
           kittykb_event_matches_key(event, (uint32_t)(unsigned char)upper);
}

/*
 * The standalone game's bindings (its pg_term.c, which the archive leaves
 * out): eleven verb shortcuts, three panel toggles, help, arrows and Enter.
 * Arrows repeat while held so lists can be scrolled; verbs do not, so a held
 * key cannot water a plant twice.
 */
void land_conservatory_key(land_conservatory *conservatory,
                           const kittykb_event *event)
{
    static const struct {
        char letter;
        size_t offset;
    } VERBS[] = {
        {'c', offsetof(pg_input, check_soil)},
        {'l', offsetof(pg_input, lift_pot)},
        {'w', offsetof(pg_input, water)},
        {'f', offsetof(pg_input, feed)},
        {'m', offsetof(pg_input, mist)},
        {'r', offsetof(pg_input, rotate)},
        {'p', offsetof(pg_input, wipe)},
        {'t', offsetof(pg_input, trim)},
        {'o', offsetof(pg_input, repot)},
        {'d', offsetof(pg_input, drain)},
        {'s', offsetof(pg_input, sleeve)},
        {'k', offsetof(pg_input, toggle_calendar)},
        {'j', offsetof(pg_input, toggle_journal)},
        {'b', offsetof(pg_input, toggle_label)},
        {'h', offsetof(pg_input, help)}
    };
    pg_input *input;
    size_t index;
    if (!conservatory || !event ||
        event->action == KITTYKB_ACTION_RELEASE)
        return;
    input = &conservatory->input;
    switch (event->key) {
    case KITTYKB_KEY_UP: input->move_y = -1; return;
    case KITTYKB_KEY_DOWN: input->move_y = 1; return;
    case KITTYKB_KEY_LEFT: input->move_x = -1; return;
    case KITTYKB_KEY_RIGHT: input->move_x = 1; return;
    default: break;
    }
    if (event->action != KITTYKB_ACTION_PRESS) return;
    if (event->key == KITTYKB_KEY_ENTER ||
        kittykb_event_matches_key(event, (uint32_t)' ')) {
        input->confirm = true;
        return;
    }
    for (index = 0u; index < sizeof VERBS / sizeof VERBS[0]; ++index) {
        if (key_letter(event, VERBS[index].letter)) {
            *(bool *)((char *)input + VERBS[index].offset) = true;
            return;
        }
    }
}

bool land_conservatory_escape(land_conservatory *conservatory)
{
    const pg_ui_state *ui;
    bool plant_has_something_open;
    if (!conservatory || !conservatory->started) return true;
    ui = &conservatory->plant.ui;
    switch ((pg_screen)ui->screen) {
    case PG_SCREEN_CALENDAR:
    case PG_SCREEN_JOURNAL:
    case PG_SCREEN_AWAY:
        plant_has_something_open = true;
        break;
    case PG_SCREEN_CHOOSER:
        plant_has_something_open =
            ui->chooser_step != (uint8_t)PG_CHOOSER_SPECIES;
        break;
    default:
        plant_has_something_open = false;
        break;
    }
    if (ui->help_open || plant_has_something_open) {
        conservatory->input.cancel = true;
        return false;
    }
    return true;
}

void land_conservatory_step(land_conservatory *conservatory, pg_now now,
                            double seconds)
{
    if (!conservatory || !conservatory->started) return;
    pg_update(&conservatory->plant, &conservatory->input, seconds);
    reset_input(&conservatory->input);
    /* pg_advance is idempotent and carries sub-tick remainders, so calling
     * it whenever the wall second changes ages the plant exactly. */
    if (now.wall_s != conservatory->last_advance_wall_s) {
        pg_catchup_report report;
        (void)pg_advance(&conservatory->plant, now, &report);
        conservatory->last_advance_wall_s = now.wall_s;
    }
}

size_t land_conservatory_take_audio(land_conservatory *conservatory,
                                    pg_audio_event *events, size_t capacity)
{
    if (!conservatory || !conservatory->started) return 0u;
    return pg_take_audio_events(&conservatory->plant, events, capacity);
}

static land_conservatory_hint current_hint(
    const land_conservatory *conservatory)
{
    const pg_ui_state *ui = &conservatory->plant.ui;
    if (ui->help_open) return LAND_CONSERVATORY_HINT_KEYS;
    switch ((pg_screen)ui->screen) {
    case PG_SCREEN_CALENDAR:
    case PG_SCREEN_JOURNAL:
    case PG_SCREEN_AWAY:
        return LAND_CONSERVATORY_HINT_PANEL;
    case PG_SCREEN_CHOOSER:
        return LAND_CONSERVATORY_HINT_CHOOSER;
    case PG_SCREEN_DAMAGED:
    case PG_SCREEN_REPLANT:
        return LAND_CONSERVATORY_HINT_NOTICE;
    default:
        return LAND_CONSERVATORY_HINT_PLANT;
    }
}

bool land_conservatory_render(ki_td_soft_renderer *renderer,
                              const land_conservatory *conservatory)
{
    if (!renderer || !conservatory || !conservatory->started) return false;
    if (!pg_render(renderer, &conservatory->plant,
                   conservatory->graphics_ready ?
                   &conservatory->graphics : NULL,
                   NULL) ||
        !land_render_conservatory_chrome(renderer,
                                         current_hint(conservatory)))
        return false;
    return ki_td_soft_pack_rgba(renderer) != NULL;
}

void land_conservatory_leave(land_conservatory *conservatory)
{
    if (conservatory && conservatory->started && conservatory->store_ready)
        (void)pg_store_save(&conservatory->plant, &conservatory->store);
}

void land_conservatory_shutdown(land_conservatory *conservatory)
{
    if (!conservatory) return;
    land_conservatory_leave(conservatory);
    if (conservatory->store_ready) pg_store_close(&conservatory->store);
    if (conservatory->graphics_ready)
        pg_graphics_shutdown(&conservatory->graphics);
    (void)memset(conservatory, 0, sizeof *conservatory);
}
