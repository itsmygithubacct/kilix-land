#ifndef KILIX_LAND_CONSERVATORY_H
#define KILIX_LAND_CONSERVATORY_H

/*
 * The conservatory: pleb-plant-grower embedded as Land's third location.
 *
 * Land owns the terminal, the loop, the clock and the mixer; the plant owns
 * the canvas and the keys while the player is inside. Everything that knows
 * pleb-plant-grower's internals stays behind this header, so game.c only ever
 * sees LAND_LOCATION_CONSERVATORY. The contract is the plant game's
 * docs/EMBEDDING.md: pg_state held by value, a host-filled pg_now, and both
 * binaries built from one kilix-game-sdk checkout.
 */
#include "kilix_land.h"

#include "pleb_plant_grower.h"
#include "pg_state.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct land_conservatory {
    pg_state plant;
    pg_graphics graphics;
    pg_store store;
    pg_input input;            /* edges gathered since the last step */
    int64_t last_advance_wall_s;
    bool started;
    bool graphics_ready;
    bool store_ready;
} land_conservatory;

/* False when this build and the linked archive disagree about pg_state:
 * the host would corrupt its own memory, so it must refuse to run. */
bool land_conservatory_abi_ok(void);

/* Wall clock, boot clock, boot id and local offset, sampled now. */
pg_now land_conservatory_now(void);

/* $XDG_DATA_HOME/kilix-land, else ~/.local/share/kilix-land. The plant adds
 * its own pleb-plant-grower component, so it never shares the standalone
 * game's save. False when neither base is an absolute path. */
bool land_conservatory_data_root(char *out, size_t size);

void land_conservatory_init(land_conservatory *conservatory);

/* Called on every entry. The first loads graphics from the embedded
 * submodule under asset_root and the save from data_root (NULL plays without
 * persistence); every entry reconciles the time spent away. */
void land_conservatory_enter(land_conservatory *conservatory,
                             const char *asset_root, const char *data_root,
                             pg_now now);

/* One key press while inside. Escape is the host's; see
 * land_conservatory_escape. */
void land_conservatory_key(land_conservatory *conservatory,
                           const kittykb_event *event);

/* Escape leaves the conservatory only when the plant has nothing of its own
 * to close; otherwise it is the plant's cancel and is queued as one. */
bool land_conservatory_escape(land_conservatory *conservatory);

/* One 60 Hz step: animation and UI every step, biology once a wall second. */
void land_conservatory_step(land_conservatory *conservatory, pg_now now,
                            double seconds);

size_t land_conservatory_take_audio(land_conservatory *conservatory,
                                    pg_audio_event *events, size_t capacity);

bool land_conservatory_render(ki_td_soft_renderer *renderer,
                              const land_conservatory *conservatory);

/* Saves. The plant keeps aging from its anchor while the player is away. */
void land_conservatory_leave(land_conservatory *conservatory);
void land_conservatory_shutdown(land_conservatory *conservatory);

#endif
