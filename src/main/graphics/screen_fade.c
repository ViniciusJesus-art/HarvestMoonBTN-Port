/* Screen fades, full-screen fills, the display environments a fade resets, and the random number generator. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libetc.h>
#include "types.h"
#include "main.h"
#include "pad.h"
#include "screen_fade.h"

#define FRAME_DRAW_ENABLE(i) frame_records[i].drawEnv.dfe
#define FRAME_ISBG(i) frame_records[i].drawEnv.isbg
/* The fades' levels are 20.12 fixed point: 4096 is one step of the level. */
#define FADE_FIXED_SHIFT 12
#define FADE_FIXED_ONE (1 << FADE_FIXED_SHIFT)
/* The full level, 255 in 20.12 fixed point. */
#define FADE_FULL_LEVEL (255 * FADE_FIXED_ONE)
/* The debug display pan moves by this many pixels a frame. */
#define DEBUG_PAN_STEP 8

typedef struct {
    u32 tag;
    u32 code[2];
} DrTPage;

static void fade_set_enabled(signed int enabled);
static void display_reset_envs(void);

s32 fade_half_step;
s16 fade_level;
s16 fade_length;
s32 fade_own_ot;
/* Steps left in a stepped fade: 255 / the step size. */
s32 fade_steps_left;
s32 fade_step_size;
s32 fade_step_mode;
signed int fade_hold;

static POLY_F4 fade_quads[2];
static DR_MODE fade_quad_modes[2];
static POLY_F4 fill_quads[2];
static DR_MODE fill_quad_modes[2];
static POLY_F4 fade_step_quads[2];
static DrTPage fade_step_tpages[2];

s32 fade_put_pending = 1;
u32 rand_seed = 0;
/* The out-fade stays at full dark for this many steps past fade_length + 1 before it ends. */
#define FADE_OUT_HOLD 6

/* The fade's ScreenFadeMode. */
s16 fade_mode = SCREEN_FADE_IN;
s32 fade_skip_next = 0;
s32 fade_enabled = 1;
s16 fade_blend = 2;

/* Step the random seed (a linear congruential generator) and return a value in 0..range-1. */
s32 rand_range(s32 range) {
    unsigned int value = rand_seed * 0x41C64E6D + 12345;
    rand_seed = value;
    return ((value >> 16) * (range & 0xFFFF)) >> 16;
}

/* Clear the rectangle (x, y, w, h) of VRAM to the colour (r, g, b), and wait for the GPU. */
void screen_clear_rect(signed short x, signed short y, signed short w, signed short h, unsigned char r,
                   unsigned char g, unsigned char b) {
    RECT rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    ClearImage(&rect, r, g, b);
    DrawSync(0);
}

/* Debug: move the display with the d-pad until L2 is pressed. */
static void screen_pan_display(s32 w, s32 h) {
    DISPENV env;

    SetDefDispEnv(&env, 0, 0, (s16)w, (s16)h);
    PutDispEnv(&env);
    input_pad1->pressed = 0;
    do {
        if (input_pad1->held & PAD_LEFT) {
            env.disp.x -= DEBUG_PAN_STEP;
        }
        if (input_pad1->held & PAD_RIGHT) {
            env.disp.x += DEBUG_PAN_STEP;
        }
        if (input_pad1->held & PAD_UP) {
            env.disp.y -= DEBUG_PAN_STEP;
        }
        if (input_pad1->held & PAD_DOWN) {
            env.disp.y += DEBUG_PAN_STEP;
        }
        VSync(0);
        pad_poll_ports();
        PutDispEnv(&env);
        if (input_pad1->pressed & PAD_CIRCLE) {
            SetDefDispEnv(&frame_records[0].dispEnv, env.disp.x, env.disp.y, 640, 480);
            SetDefDispEnv(&frame_records[1].dispEnv, env.disp.x, env.disp.y, 640, 480);
        }
        if (input_pad1->pressed & PAD_CROSS) {
            SetDefDispEnv(&frame_records[0].dispEnv, 0, 240, 320, 240);
            SetDefDispEnv(&frame_records[1].dispEnv, 0, 0, 320, 240);
        }
    } while (!(input_pad1->pressed & PAD_L2));
}

/* True once `interval` vsyncs have passed since *last (the first call sets *last). With
 * restart 1, a true result also moves *last to now. */
s32 vsync_elapsed(s32 interval, s32 *last, s32 restart) {
    s32 now;

    now = VSync(-1);
    if (*last == 0) {
        *last = now;
    }
    if ((now - *last) < interval) {
        return 0;
    }
    if (restart == 1) {
        *last = now;
    }
    return 1;
}

/* One frame of the screen fade. fade_mode is a ScreenFadeMode: fade in, fade out or frame
 * blend. The level steps towards fade_length and is queued as a full-screen quad
 * with blend mode fade_blend. */
void screen_fade_frame(void) {
    s32 step;
    u8 level;
    u_long *ot;
    POLY_F4 *poly;
    DR_MODE *mode;
    s32 shade;
    s32 blend;
    s32 value;

    step = 0;
    if (fade_is_settled() == 1 && fade_enabled != 1) {
        return;
    }
    fade_set_enabled(1);
    fade_half_step = fade_half_step == 0;
    if (fade_skip_next == 1) {
        fade_skip_next = 0;
        return;
    }

    if (fade_mode == SCREEN_FADE_IN) {
        if (fade_level > 0) {
            fade_level--;
        }
    } else if (fade_mode == SCREEN_FADE_OUT) {
        if (fade_level <= fade_length + FADE_OUT_HOLD) {
            fade_level++;
        }
    } else if (fade_mode == SCREEN_FADE_BLEND) {
        main_set_clear_screen(0);
        if (fade_length > fade_level) {
            if (main_get_speed_mode() == MAIN_SPEED_HALF) {
                fade_level += fade_half_step;
            } else {
                fade_level++;
            }
        }
    }

    if (fade_mode == SCREEN_FADE_BLEND) {
        if (fade_length == fade_level) {
            fade_mode = SCREEN_FADE_NONE;
            FRAME_ISBG(0) = 1;
            FRAME_ISBG(1) = 1;
            FRAME_DRAW_ENABLE(0) = 0;
            FRAME_DRAW_ENABLE(1) = 0;
            screen_clear_rect(0, 0, 320, 480, 0, 0, 0);
        } else if (fade_length < fade_level) {
            fade_level = fade_length;
        }
        step = FADE_FULL_LEVEL / fade_length;
        level = 255;
    } else if (fade_level >= fade_length + 1) {
        level = 255;
        if (fade_mode != SCREEN_FADE_OUT || fade_level <= fade_length + FADE_OUT_HOLD) {
            fade_mode = SCREEN_FADE_NONE;
        }
    } else if (fade_length != 0) {
        step = FADE_FULL_LEVEL / fade_length;
        value = step * fade_level / FADE_FIXED_ONE;
        if (value < 0) {
            value = 0;
        }
        if (value >= 256) {
            value = 255;
        }
        level = value;
    } else {
        level = 0;
        fade_mode = SCREEN_FADE_NONE;
    }

    if (fade_own_ot == 0) {
        ot = active_ot;
    } else {
        ot = &current_frame->otTags[0][5];
    }
    if (level == 0) {
        return;
    }
    poly = &fade_quads[frame_index];
    mode = &fade_quad_modes[frame_index];
    if (fade_mode == SCREEN_FADE_BLEND) {
        if (fade_put_pending == 1) {
            fade_put_pending = 0;
            PutDispEnv(&frame_records[frame_index].dispEnv);
        }
        FRAME_ISBG(0) = 0;
        FRAME_ISBG(1) = 0;
        FRAME_DRAW_ENABLE(0) = 1;
        FRAME_DRAW_ENABLE(1) = 1;
        shade = step / FADE_FIXED_ONE;
        blend = 2;
    } else {
        shade = level;
        blend = fade_blend;
    }
    setlen(poly, 5);
    poly->r0 = shade;
    poly->g0 = shade;
    poly->b0 = shade;
    poly->x1 = 320;
    poly->x3 = 320;
    poly->y2 = 240;
    poly->y3 = 240;
    poly->code = 0x2A;
    poly->x0 = 0;
    poly->y0 = 0;
    poly->y1 = 0;
    poly->x2 = 0;
    setlen(mode, 2);
    mode->code[0] = 0xE1000600 | ((blend & 3) << 5);
    mode->code[1] = 0;
    addPrim(ot, poly);
    addPrim(ot, mode);
}

/* A full-screen 320x240 flat quad of colour (r, g, b) (20.12 fixed point), in
 * front of a blend mode 2 draw mode. */
void screen_fill_quad(u32 r, u32 g, u32 b) {
    POLY_F4 *poly;
    DR_MODE *mode;
    FrameState *display;

    poly = &fill_quads[frame_index];
    mode = &fill_quad_modes[frame_index];

    setlen(poly, 5);
    poly->x1 = 320;
    poly->x3 = 320;
    poly->y2 = 240;
    poly->y3 = 240;
    poly->r0 = r >> FADE_FIXED_SHIFT;
    poly->g0 = g >> FADE_FIXED_SHIFT;
    poly->b0 = b >> FADE_FIXED_SHIFT;
    poly->code = 0x2A;
    poly->x0 = 0;
    poly->y0 = 0;
    poly->y1 = 0;
    poly->x2 = 0;
    setlen(mode, 2);
    mode->code[0] = 0xE1000640;
    mode->code[1] = 0;
    display = current_frame;
    addPrim(display->otTags[1], poly);
    addPrim(display->otTags[1], mode);
}

/* Reset the fade to a fade in of the default 60 steps, with blend 2 and no own ordering table. */
void fade_reset(void) {
    signed int x;
    x = SCREEN_FADE_IN;
    fade_mode = x;
    x = 60;
    fade_length = x;
    fade_level = 0;
    fade_skip_next = 0;
    fade_blend = 2;
    fade_own_ot = 0;
}

/* Start a fade in of `length` steps. own_ot draws it into the fade's own ordering table,
 * blend is the draw mode's semi-transparency. */
void fade_start_in(signed short length, signed short own_ot, signed short blend) {
    fade_mode = SCREEN_FADE_IN;
    fade_level = length - 1;
    fade_length = length;
    fade_skip_next = 0;
    fade_blend = blend;
    fade_own_ot = own_ot;
}

/* Start a fade out of `length` steps (one more than given). own_ot and blend as fade_start_in. */
void fade_start_out(signed short length, signed short own_ot, signed short blend) {
    fade_mode = SCREEN_FADE_OUT;
    fade_level = 0;
    fade_length = length + 1;
    fade_skip_next = 0;
    fade_blend = blend;
    fade_own_ot = own_ot;
}

/* Start a frame blend over length + 1 steps; a length of 31 or more is treated as 30. */
void fade_start_blend(s16 length) {
    fade_mode = SCREEN_FADE_BLEND;
    fade_level = 0;
    if (length >= 31) {
        length = 30;
        fade_length = length;
    }
    fade_length = length + 1;
    fade_put_pending = 1;
    fade_skip_next = 1;
    main_set_put_env(0);
    main_set_clear_screen(0);
    fade_blend = 2;
    fade_own_ot = 0;
}

/* True when the current fade has finished. */
signed int fade_is_settled(void) {
    if (fade_mode == SCREEN_FADE_IN) return fade_level < 1;
    if (fade_mode == SCREEN_FADE_BLEND) return (fade_level ^ (fade_length + 3)) == 0;
    if (fade_mode == SCREEN_FADE_OUT) return !(fade_level < fade_length + 1);
    return 1;
}

/* The fade's mode as none, in or out: a blend reads as none. */
signed int fade_get_mode(void) {
    switch (fade_mode) {
    case SCREEN_FADE_IN:
        return SCREEN_FADE_IN;
    case SCREEN_FADE_OUT:
        return SCREEN_FADE_OUT;
    default:
        return SCREEN_FADE_NONE;
    }
}

static void fade_set_enabled(signed int enabled) {
    fade_enabled = enabled;
}

static signed int fade_is_enabled(void) {
    return fade_enabled;
}

/* Start a stepped fade of 255 / size steps at the constant level `size` (mode 2); the
 * first step is held. */
static void fade_start_steps_inverse(signed int size) {
    fade_step_mode = 2;
    fade_step_size = size;
    fade_steps_left = 255 / size;
    fade_hold = 1;
}

/* Start a stepped fade of 255 / size steps, the level rising as the steps run down (mode 1). */
static void fade_start_steps(signed int size) {
    fade_step_mode = 1;
    fade_step_size = size;
    fade_steps_left = 255 / size;
}

static unsigned int fade_steps_done(void) {
    return fade_steps_left == 0;
}

/* One step of the screen fade: while the counter runs, reset the display
 * environment and queue a semi-transparent full-screen quad of the current level
 * (with its texture page) into the active ordering table. */
void screen_fade_step(void) {
    s32 buffer;
    u8 level;
    POLY_F4 *quad;
    DrTPage *tpage;
    u_long *ot;

    if (fade_steps_left > 0) {
        buffer = current_frame != frame_records;
        display_reset_envs();
        main_set_put_env(0);
        main_set_clear_screen(0);
        if (fade_hold == 1) {
            fade_hold = 0;
            return;
        }
        if (fade_step_mode == 1) {
            level = ~(fade_step_size * fade_steps_left);
        } else {
            level = fade_step_size;
        }
        quad = &fade_step_quads[buffer];
        tpage = &fade_step_tpages[buffer];
        setlen(quad, 5);
        quad->x1 = 640;
        quad->x3 = 640;
        quad->y2 = 480;
        quad->y3 = 480;
        quad->code = 0x2A;
        quad->r0 = level;
        quad->g0 = level;
        quad->b0 = level;
        quad->x0 = 0;
        quad->y0 = 0;
        quad->y1 = 0;
        quad->x2 = 0;
        setlen(tpage, 2);
        tpage->code[0] = 0xE1000740;
        tpage->code[1] = 0;
        ot = active_ot;
        addPrim(ot, quad);
        addPrim(ot, tpage);
        fade_steps_left--;
        if (fade_steps_left >= 0) {
            return;
        }
    }
    fade_steps_left = 0;
}

/* Reset the display and draw environments to the full 640x480 screen. */
static void display_reset_envs(void) {
    DISPENV disp;
    DRAWENV draw;

    SetDefDispEnv(&disp, 0, 0, 640, 480);
    SetDefDrawEnv(&draw, 0, 0, 640, 480);
    PutDispEnv(&disp);
    PutDrawEnv(&draw);
}
