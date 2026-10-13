#include "types.h"
#include "animals.h"
#include "cue.h"
#include "dog.h"
#include "farm_state.h"
#include "items.h"
#include "pond.h"
#include "screen_fade.h"

static void pond_set_fish_days(FarmFish *fish, s32 kind);
static void pond_fish_set_item(FarmFish *fish);
static void pond_fish_spawn(FarmFish *fish);
static s32 pond_fish_count(void);

/* Draw a pet and reset its cue, unless the screen is in transition. */
void pet_reset_actor(FarmPet *pet) {
    if (!(game_state.globalFlags & GLOBAL_FLAG_TRANSITION)) {
        farm_draw_actor(&pet->actor);
        cue_reset(&pet->actor.cue);
    }
}

/* Put a fish of `kind` in the first free pond slot and report success. */
s32 pond_add_fish(s32 kind) {
    s32 slot;
    FarmFish *entry;

    if (((u32)(kind - ITEM_LARGE_FISH)) < POND_FISH_SIZES) {
        entry = game_state.animals.pond.fish;
        slot = 0;
        if (entry->item != ITEM_NOTHING) {
            do {
                slot++;
                entry++;
                if (slot >= POND_FISH) {
                    break;
                }
            } while (entry->item != ITEM_NOTHING);
        }
        if (slot != POND_FISH) {
            entry->item = kind;
            pond_set_fish_days(entry, kind);
            game_state.progress.estate.pondFish = pond_fish_count();
            return 1;
        }
    }
    return 0;
}

/* Empty the pond and zero the fish count. */
void pond_clear(void) {
    FarmFish *entry = game_state.animals.pond.fish;
    s32 i;
    for (i = 0; i < POND_FISH; i++) {
        entry->item = ITEM_NOTHING;
        entry++;
    }
    game_state.animals.fishCount = 0;
}

/* Age the fish of a fed pond and grow or spawn them, then clear the fed flag and
 * recount the fish. */
void pond_daily_update(void) {
    FarmFish *entry = game_state.animals.pond.fish;
    s32 i;

    if (game_state.animals.pond.fed & POND_FED) {
        for (i = POND_FISH - 1; i >= 0; i--, entry++) {
            if (entry->item != ITEM_NOTHING) {
                entry->days++;
                pond_fish_set_item(entry);
                pond_fish_spawn(entry);
            }
        }
    }
    game_state.animals.pond.fed &= ~POND_FED;
    game_state.animals.fishCount = pond_fish_count();
}

/* Mark the pond fed today. */
void pond_mark_fed(void) {
    game_state.animals.pond.fed = game_state.animals.pond.fed | POND_FED;
}

/* The fish sizes, largest first, that pond_take_fish looks for. */
enum PondFishSize {
    POND_FISH_LARGE = 0,
    POND_FISH_MEDIUM = 1,
    POND_FISH_SMALL = 2,
};

/* Take the first large fish out of the pond, else a medium, else a small one,
 * and return its item, or 0 when the pond is empty. */
s32 pond_take_fish(void) {
    FarmFish *entry;
    s32 category;
    s32 target;
    s32 i;

    for (category = POND_FISH_LARGE; category < POND_FISH_SIZES; category++) {
        switch (category) {
        case POND_FISH_LARGE:
            target = ITEM_LARGE_FISH;
            break;
        case POND_FISH_MEDIUM:
            target = ITEM_MEDIUM_FISH;
            break;
        case POND_FISH_SMALL:
            target = ITEM_SMALL_FISH;
            break;
        default:
            target = ITEM_SMALL_FISH;
            break;
        }
        entry = game_state.animals.pond.fish;
        for (i = 0; i < POND_FISH; i++, entry++) {
            if (entry->item == target) {
                entry->item = ITEM_NOTHING;
                return target;
            }
        }
    }
    return 0;
}

/* A new fish of `kind` starts at age 0 when small, or at its size's days when medium or large. */
static void pond_set_fish_days(FarmFish *fish, s32 kind) {
    switch (kind) {
    case ITEM_SMALL_FISH:
        fish->days = 0;
        break;
    case ITEM_MEDIUM_FISH:
        fish->days = POND_MEDIUM_DAYS;
        break;
    case ITEM_LARGE_FISH:
        fish->days = POND_LARGE_DAYS;
        break;
    }
}

/* The item follows the age: small, then medium, then large. */
static void pond_fish_set_item(FarmFish *fish) {
    if (fish->days < POND_MEDIUM_DAYS)
        fish->item = ITEM_SMALL_FISH;
    else if (fish->days < POND_LARGE_DAYS)
        fish->item = ITEM_MEDIUM_FISH;
    else
        fish->item = ITEM_LARGE_FISH;
}

/* A fish of POND_SPAWN_DAYS or more goes back to POND_LARGE_DAYS and may spawn small
 * fish; the chance falls as the pond fills. */
static void pond_fish_spawn(FarmFish *fish) {
    s32 start;
    s32 count;
    start = pond_fish_count();
    if (fish->days >= POND_SPAWN_DAYS) {
        count = (rand_range(100) - start) / 25;
        fish->days = POND_LARGE_DAYS;
        if (count > 0) {
            start = count;
            do {
                pond_add_fish(ITEM_SMALL_FISH);
                start--;
            } while (start != 0);
        }
    }
}

/* Count the live fish in the pond. */
static s32 pond_fish_count(void) {
    s32 count = 0;
    FarmFish *p = game_state.animals.pond.fish;
    s32 i = POND_FISH - 1;
    do {
        if (p->item != ITEM_NOTHING) count++;
        i--;
        p++;
    } while (i >= 0);
    return count;
}
