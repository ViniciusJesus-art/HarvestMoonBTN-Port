#include <sys/types.h>
#include <libgte.h>
#include "types.h"
#include "gte.h"

/* Length of `v`, or -1 when any component lies outside [-limit, limit]. */
static s32 vec_length_bounded(VECTOR *v, s32 limit) {
    VECTOR sq;

    sq.vx = v->vx;
    sq.vy = v->vy;
    sq.vz = v->vz;
    if (sq.vx > limit || sq.vx < -limit) {
        return -1;
    }
    gte_ldlvl(&sq);
    if (sq.vy > limit || sq.vy < -limit) {
        return -1;
    }
    gte_sqr0_b();
    if (sq.vz > limit || sq.vz < -limit) {
        return -1;
    }
    gte_stlvnl(&sq);
    return SquareRoot0(sq.vx + sq.vy + sq.vz);
}
