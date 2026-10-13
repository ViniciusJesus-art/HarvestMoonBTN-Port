#ifndef STAFF_H
#define STAFF_H

/* Types for the staff roll (overlay 28_GF_STAFF). */

#include "types.h"

/* The rectangle the fade covers. */
typedef struct {
    s16 x;
    s16 y;
    s16 width;
    s16 height;
} StaffEntry;

/* One scrolling credit. */
typedef struct StaffParticle {
    s32 live;               /* 0 free, 1 live */
    s16 code;               /* the sprite drawn for it */
    s16 pad6;
    s32 time;               /* counts down by the scroll speed until it retires */
} StaffParticle;

void staff_tick(void);

#endif /* STAFF_H */
