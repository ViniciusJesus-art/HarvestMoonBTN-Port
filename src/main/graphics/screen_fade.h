#ifndef SCREEN_FADE_H
#define SCREEN_FADE_H


/* Full-screen fades, the display and draw environments. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* The screen fade's mode: none, fading in, fading out, or a frame blend. */
enum ScreenFadeMode {
    SCREEN_FADE_NONE = 0,
    SCREEN_FADE_IN = 1,
    SCREEN_FADE_OUT = 2,
    SCREEN_FADE_BLEND = 3,
};

s32 rand_range(s32 range);
void screen_clear_rect(s16 x, s16 y, s16 w, s16 h, u8 r, u8 g, u8 b);
s32 vsync_elapsed(s32 interval, s32 *last, s32 restart);
void screen_fade_frame(void);
void screen_fill_quad(u32 r, u32 g, u32 b);
void fade_reset(void);
void fade_start_in(s16 length, s16 own_ot, s16 blend);
void fade_start_out(s16 length, s16 own_ot, s16 blend);
void fade_start_blend(s16 length);
s32 fade_is_settled(void);
s32 fade_get_mode(void);
void screen_fade_step(void);

/* The random seed. rand_range steps it. */
extern u32 rand_seed;

#endif /* SCREEN_FADE_H */
