#ifndef CLUT_H
#define CLUT_H


/* CLUT (palette) fades, blends and scrolls. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* The CLUT-fade record that the colour-fade effects step and redraw. */
typedef struct ClutFade {
    u_long *clut;   /* two saved 16-colour rows */
    s16 direction;  /* bounce latch */
    s16 unused6;
    s16 span;       /* number of blend steps */
    s16 position;   /* current blend step */
    RECT load;      /* where the blended CLUT is uploaded */
} ClutFade;


void clut_fade_setup(ClutFade *fade, u_long *clut, s32 span, s32 fromX, s32 fromY, s32 toX, s32 toY, s32 dstX, s32 dstY);
void clut_blend_palettes(u_long *dst, u_long *from, u_long *to, s32 step, s32 steps);
void clut_scroll_step(ClutFade *arg, s32 amount, s32 mode);

s32 clut_fade_page(void);
void clut_fade_step(ClutFade *arg, s32 value);

#endif /* CLUT_H */
