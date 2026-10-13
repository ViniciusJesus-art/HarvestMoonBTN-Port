#ifndef POND_H
#define POND_H

/* The pond and its fish. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "dog.h"
#include "items.h"

#define POND_FISH 99
#define POND_FED 1              /* the bit of FarmPond.fed that means fed today */
#define POND_FISH_SIZES 3       /* small, medium and large */
#define POND_MEDIUM_DAYS 10     /* a fish from this many days is medium */
#define POND_LARGE_DAYS 20      /* and from this many days it is large */
#define POND_SPAWN_DAYS 30      /* a fish from this many days spawns small ones */

/* One of the pond's 99 fish. */
typedef struct FarmFish {
    ItemId item;        /* ITEM_SMALL_FISH, ITEM_MEDIUM_FISH, ITEM_LARGE_FISH; 0 = none */
    u16 days;
} FarmFish;

/* The pond: its fish and whether they were fed today. */
typedef struct FarmPond {
    FarmFish fish[POND_FISH];
    u16 fed;                /* |= 1 once fed today */
} FarmPond;

void pet_reset_actor(FarmPet *pet);
s32 pond_add_fish(s32 kind);
void pond_clear(void);
void pond_daily_update(void);
void pond_mark_fed(void);
s32 pond_take_fish(void);

#endif /* POND_H */
