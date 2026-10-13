#ifndef FARM_SHARED_H
#define FARM_SHARED_H

/* Helpers shared by the farm overlay's animal and pet code. */

#include "types.h"
#include "actor.h"
#include "animals.h"
#include "farm_state.h"

enum {
    FARM_TIMED_FLAG_TICKS = 300,
    FARM_DAY_HOUR_FIRST = 6,
    FARM_DAY_HOUR_COUNT = 15,   /* hours 6 to 20 */
};

/* Aims the actor at a point a fixed distance in front of the player, offset by the
 * actor's own position, and turns it to face the player's direction. */
static inline s32 farm_mover_update(FarmActor *o) {
    VECTOR v;

    farm_vector_adjust(game_state.player.facing38, &v, 800);
    v.vx += o->position.vx;
    v.vy += o->position.vy;
    v.vz += o->position.vz;
    farm_set_destination(o, v);
    o->facing = game_state.player.facing38;
    return 0;
}

/* Arms a 300-tick timer and clears flag 0x800 unless the global half-word is in [6, 21). */
static inline s32 farm_timed_flag_tick(FarmActor *p) {
    if (p->kind == 0) return -1;
    if ((u32)((u16)game_state.progress.clock.time.hm.hour - FARM_DAY_HOUR_FIRST) >= FARM_DAY_HOUR_COUNT) {
        p->interruptTime = FARM_TIMED_FLAG_TICKS;
        p->flags &= ~0x800;
    }
    return 0;
}

/* Carries ACTOR_FLAG_ADULT over to ACTOR_FLAG_ADULT_SPRITE, for dogs and horses. */
static inline s32 farm_record_promote_flag(FarmActor *arg) {
    if (arg->kind == 0) {
        return -1;
    }
    if (arg->flags & ACTOR_FLAG_ADULT) {
        arg->flags |= ACTOR_FLAG_ADULT_SPRITE;
    }
    return 0;
}

#endif /* FARM_SHARED_H */
