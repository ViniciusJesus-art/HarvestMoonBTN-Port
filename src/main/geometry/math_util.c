#include <sys/types.h>
#include <libgte.h>
#include "types.h"
#include "math_util.h"

/* Build the vector (x, -y, z) of length `radius` at `angle` (4096 to a turn). */
void vec_from_polar(s32 *out, s32 radius, s32 y, s32 angle) {
    s32 a = angle & 0xFFF;
    out[1] = -y;
    out[0] = (rcos(a) * radius) >> 12;
    out[2] = (rsin(a) * radius) >> 12;
}

signed int math_int_pow(signed int base, signed int exponent) {
    signed int result;
    result = 1;
    if (exponent > 0) {
        do {
            result *= base;
            exponent -= 1;
        } while (exponent != 0);
    }
    return result;
}

/* Blend each channel of a vector from `from` to `to`, by `step` of `steps`. */
void math_lerp_vec3(s32 step, s32 steps, s32 *from, s32 *to, s32 *out) {
    s32 i;
    s32 *p;
    if (steps < step) step = steps;
    step = steps - step;
    p = out;
    i = 2;
    do {
        *p = ((*from - *to) * step) / steps + *to;
        p++;
        i--;
        from++;
        to++;
    } while (i >= 0);
}

/* Blend one value from `from` to `to`, by `step` of `steps`. */
void math_lerp_scalar(signed int step, signed int steps, signed int from, signed int to, signed int *out) {
    signed int remaining;
    if (steps < step) {
        step = steps;
    }
    remaining = steps - step;
    *out = ((from - to) * remaining) / steps + to;
}

void vec3_copy(signed int *dst, signed int *src) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static void vec3_add(s32 *dst, s32 *src) {
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

void vec3_sub(s32 *dst, s32 *src) {
    dst[0] = dst[0] - src[0];
    dst[1] = dst[1] - src[1];
    dst[2] = dst[2] - src[2];
}

static s32 vec3_differs(s32 *a, s32 *b) {
    if (a[0] != b[0]) return 1;
    if (a[1] != b[1]) return 1;
    return a[2] != b[2];
}

/* Move `vector` toward `origin` by `scale`, writing the result to `result`
 * when it is given. Returns 0 while the vector is still farther than `scale`
 * away, 1 once it is within reach. */
static s32 vec3_move_toward(s32 *vector, s32 scale, s32 *origin, s32 *result) {
    s32 delta[3];
    s32 length;
    s32 within;
    s32 ok;

    ok = 1;
    vec3_copy(delta, vector);
    vec3_sub(delta, origin);
    length = SquareRoot0(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
    if (result != 0) {
        if (length == 0) {
            vec3_copy(result, origin);
        } else {
            result[0] = origin[0] + delta[0] * scale / length;
            result[1] = origin[1] + delta[1] * scale / length;
            result[2] = origin[2] + delta[2] * scale / length;
        }
    }
    within = length < scale;
    if (within == 0) {
        ok = 0;
    }
    return ok;
}

/* The sign of the 2D cross product (x0, y0) x (x1, y1): bit 0 is set when it is
 * not negative, and bit 1 when it is zero. */
static unsigned int math_cross2_sign(signed int x0, signed int y0, signed int x1, signed int y1) {
    unsigned int cross;
    unsigned int result;
    cross = (x0 * y1) - (y0 * x1);
    result = ~cross >> 31;
    if (cross == 0) {
        result |= 2;
    }
    return result;
}

/* The dot product of a and b after each is normalized to unit length. */
static signed int vec_dot_normalized(VECTOR *a, VECTOR *b) {
    VECTOR n1;
    VECTOR n2;
    VectorNormal(a, &n1);
    VectorNormal(b, &n2);
    return n1.vx * n2.vx + n1.vy * n2.vy + n1.vz * n2.vz;
}
