#ifndef FARM_MAIN_H
#define FARM_MAIN_H

/* The farm overlay's entry point: the per-frame update and the resource loads. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "game_types.h"

/* Startup phase of the farm (a FarmStartupPhase step). */
enum FarmStartupPhase {
    FARM_STARTUP_BRING_UP = 0,
    FARM_STARTUP_PLAYER_SPRITE = 1,
    FARM_STARTUP_SET_AREA = 2,
    FARM_STARTUP_WAIT = 3,
    FARM_STARTUP_FRAME = 4,
    FARM_STARTUP_MODE_CHANGE = 5,
    FARM_STARTUP_FADER_WAIT = 6,
    FARM_STARTUP_FINISH = 7,
    FARM_STARTUP_PAUSE = 9,
    FARM_STARTUP_LAYOUT = 10,
    FARM_STARTUP_WAIT_2 = 11,
    FARM_STARTUP_FRAME_2 = 12,
    FARM_STARTUP_MODE_CHANGE_2 = 13,
    FARM_STARTUP_FADER_WAIT_2 = 14,
    FARM_STARTUP_FINISH_2 = 15,
};


/* The position a farm step probe starts from. */
typedef struct FarmProbePosition {
    s32 x;
    s32 y;
    s32 z;
} FarmProbePosition;


void farm_run_on_scratchpad(void);
void farm_mode_change(void);
void farm_set_message_timing(void);
void farm_unpause(void);

/* The query the farm's tile and object tests share. Each test reads the members
 * it needs; the occupant test fills it once per call. */
typedef struct FarmTileQuery {
    VECTOR *pos;          /* the point under test */
    LimitPair *place;     /* the map and area the test is for */
    void **tile;          /* receives the tile record or the object that was hit */
    void *current;        /* the object asking, which does not block itself */
    s32 *flags;           /* receives the result flags on a hit */
    s32 mask;             /* the OccupantClass bits being tested */
    s32 hitFlags;         /* the object class's flags, written on a hit */
} FarmTileQuery;

/* The object classes the occupant test checks, one bit each in its mask. */
enum OccupantClass {
    OCCUPANT_PLAYER          = (1 << 0),
    OCCUPANT_CATTLE          = (1 << 1),   /* the calf, young cow, cow and visiting cow kinds */
    OCCUPANT_SHEEP           = (1 << 2),   /* the lamb, sheep and visiting sheep kinds */
    OCCUPANT_CHICKEN         = (1 << 3),
    OCCUPANT_CHOICE          = (1 << 4),   /* the placement boxes of the location pick */
    OCCUPANT_TILE            = (1 << 5),   /* the tile rules */
    OCCUPANT_TILE_BIT6       = (1 << 6),
    OCCUPANT_VILLAGER        = (1 << 7),
    OCCUPANT_DOG             = (1 << 8),
    OCCUPANT_HORSE           = (1 << 9),
    OCCUPANT_HELPER          = (1 << 10),
    OCCUPANT_ACTION          = (1 << 11),  /* the six animal action records */
    OCCUPANT_SPECIAL         = (1 << 12),  /* the special tiles */
    OCCUPANT_ESTATE_RECORD   = (1 << 15),  /* the estate's runs records */
    OCCUPANT_FLAGGED_CHOICES = (1 << 17),  /* the flagged choices */
};

void farm_enter_dispatch(void);

#endif /* FARM_MAIN_H */
