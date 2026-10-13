#ifndef FARM_H
#define FARM_H

#include "types.h"
#include "actor.h"
#include "animals.h"

/* Global data and types of the farm overlay. They are kept out of
 * globals.h because only this overlay uses them. */
#define SLOT_NONE (-1)
/* Adds an actor's speed to its position, and zeroes the speed. */
static inline void farm_pair_at_38(FarmActor *actor) {
    farm_vec3_accumulate(&actor->position, &actor->speed, 0);
}
/* One step on the field grid in a facing's direction. */
typedef struct FarmStep {
    s16 x;
    s16 z;
} FarmStep;

#endif /* FARM_H */
