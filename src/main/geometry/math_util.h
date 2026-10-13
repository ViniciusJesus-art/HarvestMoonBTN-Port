#ifndef MATH_UTIL_H
#define MATH_UTIL_H


/* Vectors, lerps and integer powers. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* 1.0 in the 1.12 fixed-point format (a scale of 1.0, or a full step of a 20.12 position). */
#define FIXED_ONE 0x1000

/* bits16_set or bits16_clear, picked at run time. */
typedef void (*Bits16Fn)();
void vec_from_polar(s32 *out, s32 radius, s32 y, s32 angle);
s32 math_int_pow(s32 base, s32 exponent);
void math_lerp_vec3(s32 step, s32 steps, s32 *from, s32 *to, s32 *out);
void math_lerp_scalar(s32 step, s32 steps, s32 from, s32 to, s32 *out);
void vec3_copy(s32 *dst, s32 *src);
void vec3_sub(s32 *dst, s32 *src);

#endif /* MATH_UTIL_H */
