/* CLUT fades: saving the palette rows, blending two palettes and scrolling the strip. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "clut.h"
#include "frame_buffers.h"
#include "math_util.h"

/* Set up a CLUT fade: read the 16-colour "from" row at (fromX, fromY) and
 * the "to" row at (toX, toY) back from VRAM into `clut`, and aim the blended
 * result at (dstX, dstY). All three rows are offset by clut_fade_page's page. */
void clut_fade_setup(ClutFade *fade, u_long *clut, s32 span, s32 fromX, s32 fromY,
                   s32 toX, s32 toY, s32 dstX, s32 dstY) {
    RECT rect;
    s32 page;

    fade->clut = clut;
    fade->direction = -2;
    fade->span = span;
    fade->unused6 = 0;
    fade->position = 0;
    page = clut_fade_page();
    DrawSync(0);
    rect.x = fromX;
    rect.y = fromY + page;
    rect.w = 16;
    rect.h = 1;
    StoreImage(&rect, clut);
    DrawSync(0);
    rect.x = toX;
    rect.y = toY + page;
    rect.w = 16;
    rect.h = 1;
    StoreImage(&rect, clut + 8);
    DrawSync(0);
    fade->load.x = dstX;
    fade->load.y = dstY + page;
    fade->load.w = 16;
    fade->load.h = 1;
}

/* Blend two 16-entry 15-bit CLUTs, `from` towards `to`, by `step` of `steps`
 * (math_lerp_vec3 per channel, blue first) and write the result to `dst`.
 * A non-black result gets the semi-transparency bit. */
void clut_blend_palettes(u_long *dst, u_long *from, u_long *to, s32 step, s32 steps) {
    u16 *out = (u16 *)dst;
    u16 *src0 = (u16 *)from;
    u16 *src1 = (u16 *)to;
    s32 c0[3];
    s32 c1[3];
    s32 mixed[3];
    s32 color;
    s32 i;

    for (i = 0; i < 16; i++) {
        c0[2] = src0[i] & 0x1F;
        c0[1] = (src0[i] >> 5) & 0x1F;
        c0[0] = (src0[i] >> 10) & 0x1F;
        c1[2] = src1[i] & 0x1F;
        c1[1] = (src1[i] >> 5) & 0x1F;
        c1[0] = (src1[i] >> 10) & 0x1F;
        math_lerp_vec3(step, steps, c0, c1, mixed);
        color = (u16)mixed[0] & 0x1F;
        color <<= 5;
        color |= (u16)mixed[1] & 0x1F;
        color <<= 5;
        color |= (u16)mixed[2] & 0x1F;
        if (color != 0) {
            color = (s16)(color | 0x8000);
        }
        out[i] = color;
    }
}

/* Step the scroll position by `amount` and re-upload the strip. Mode 1 bounces
 * off both ends, flipping direction; any other mode wraps. */
void clut_scroll_step(ClutFade *fade, s32 amount, s32 mode) {
    RECT vals;
    s16 count;
    DR_LOAD *result;
    u16 limit;

    if (fade->span <= 0) {
        return;
    }

    if (fade->position < 0) {
        fade->position = 0;
    }
    if (fade->direction < 0) {
        fade->direction = 0;
    }
    count = fade->span;
    limit = fade->span;
    if (count < amount) {
        amount = count;
    }

    if (mode == 1) {
        if (fade->direction != 0) {
            fade->position = fade->position - amount;
            if ((fade->position << 16) < 0) {
                fade->position = 0;
                fade->direction = 0;
            }
        } else {
            fade->position = fade->position + amount;
            if (count < fade->position) {
                fade->position = limit;
                fade->direction = mode;
            }
        }
    } else {
        fade->position = fade->position + amount;
        if (count <= fade->position) {
            fade->position = fade->position - limit;
        }
    }

    vals.x = fade->load.x;
    vals.y = fade->load.y;
    vals.w = fade->load.w;
    vals.h = fade->load.h;
    result = ot_insert_prim(0, 0x44);
    SetDrawLoad(result, &vals);
    clut_blend_palettes(result->p, fade->clut, fade->clut + 8, fade->position, fade->span);
}

static s32 clut_serial = 0;

/* Blend the fade's two palettes at `value` of its span and load the result. */
void clut_fade_step(ClutFade *arg, s32 value) {
    RECT vals;
    DR_LOAD *result;
    vals.x = arg->load.x;
    vals.y = arg->load.y;
    vals.w = arg->load.w;
    vals.h = arg->load.h;
    result = ot_insert_prim(0, 0x44);
    SetDrawLoad(result, &vals);
    clut_blend_palettes(result->p, arg->clut, arg->clut + 8, value, arg->span);
}

static signed int clut_next_serial(void) {
    return clut_serial++;
}

int clut_fade_page(void) {
    return 0;
}
