/* The staff roll: the credits script and the scrolling credit list, and the
 * sprites and fade that run with them. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "afile.h"
#include "anim.h"
#include "farm_state.h"
#include "main.h"
#include "math_util.h"
#include "menu_window.h"
#include "prim.h"
#include "screen_fade.h"
#include "sound.h"
#include "sound_request.h"
#include "staff.h"
#include "text.h"
#include "texture_pack.h"

#define STAFF_ROUTE_CAP 10
#define STAFF_SCROLL_SPEED_DEFAULT 0x800
#define STAFF_LINE_SPAN 0x10000
#define STAFF_WAIT_SPAN 0xE000
#define STAFF_CREDITS_PER_ROW 25
#define STAFF_LINE_SIZE 80
#define STAFF_LINE_START_Y 144
#define STAFF_HOLD_COLOUR 0x4B6496
#define STAFF_PARTICLE_COUNT 20
#define STAFF_PARTICLE_START (VWD0 << 12)
#define STAFF_PARTICLE_END (-14 * FIXED_ONE)
#define STAFF_ROLL_WRAP 108000
#define STAFF_FADE_STEP 4
#define STAFF_FADE_MAX 0xFF
#define STAFF_FADE_FRAMES 10
#define STAFF_SPRITE_SECTOR 0x03D77000
#define STAFF_SPRITE_SIZE 0x6000
#define STAFF_SPRITE_PACK_OFFSET 0x4900
#define STAFF_ROLL_PACK_OFFSET 0x4FAC

static void staff_reset_state(void);
static void staff_enter(void);
static void staff_load_wait(void);
static void staff_load_tick(void);
static void staff_roll_tick(void);
static void staff_exit_scene(void);
static void staff_script_init(void);
static void staff_script_run(void);
static void staff_scroll_init(void);
static void staff_scroll_tick(void);
static void staff_particles_clear(void);
static void staff_credit_start(s16 code);
static void staff_particles_draw(void);
static void staff_particles_move(void);
static void staff_clear_screen(void);
static s32 staff_load_sprites(void);
static void staff_load_images(void);
static s32 staff_load_roll_file(void);
static void staff_gouraud_rect(u_long *ot, s16 x, s16 y, s16 w, s16 h, s32 c0, s32 c1, s32 c2, s32 c3,
                   s16 semiTrans, u8 blend);
static void staff_fade_draw(void);
static void staff_fade_out_begin(s16 x, s16 y);
static void staff_fade_in_begin(s16 x, s16 y);
static s32 staff_fade_idle(void);
static void staff_exit_fade_begin(void);
static void staff_exit_fade_wait(void);
static void staff_image_request(void);

/* Script opcodes, as staff_script_run decodes them. Every step first waits one
 * line's time, 0x10000 / staff_scroll_speed ticks; the two WAITs add to that. */
#define CREDIT(n)       (n)          /* 0x00..0xCF: start credit n scrolling */
#define WAIT(n)         (0xD0 | (n)) /* + n * (0xE000 / staff_scroll_speed) ticks */
#define WAIT_X30(n)     (0xE0 | (n)) /* + n * 30 ticks */
#define FADE            0xF0         /* sound_fade_out(60); next opcode at once */
#define SCROLL_STOP     0xF1         /* staff_scrolling = 0; next opcode at once */
#define SCROLL_BLOCK_ON 0xF2         /* staff_block_on = 1; next opcode at once */
#define SCROLL_BLOCK_OFF 0xF3        /* staff_block_on = 0; next opcode at once */
#define END             0xFF         /* staff_ended = 1 */

static u8 staff_script[66] = {
    WAIT_X30(2), CREDIT(0x00), SCROLL_BLOCK_ON,
    CREDIT(0x01), CREDIT(0x02), CREDIT(0x03), WAIT(1),
    CREDIT(0x04), CREDIT(0x05), WAIT(1),
    CREDIT(0x06), CREDIT(0x07), CREDIT(0x08), CREDIT(0x09), WAIT(1),
    CREDIT(0x0A), WAIT(1),
    CREDIT(0x0B), WAIT(3),
    CREDIT(0x0C), CREDIT(0x0D), WAIT(2),
    CREDIT(0x0E), CREDIT(0x0F), WAIT(2),
    CREDIT(0x10), CREDIT(0x11), WAIT(3),
    CREDIT(0x12), CREDIT(0x13), CREDIT(0x14), WAIT(1),
    CREDIT(0x15), WAIT(3),
    CREDIT(0x16), CREDIT(0x17), WAIT(1),
    CREDIT(0x18), WAIT(1),
    CREDIT(0x19), CREDIT(0x1A), WAIT(1),
    CREDIT(0x1B), CREDIT(0x1C), WAIT(1),
    CREDIT(0x1D), WAIT(1),
    CREDIT(0x1E), CREDIT(0x1F), CREDIT(0x20), WAIT(1),
    CREDIT(0x21), WAIT(1),
    CREDIT(0x22), CREDIT(0x23), WAIT(5), WAIT(5), WAIT(3),
    CREDIT(0x24), CREDIT(0x25), CREDIT(0x26), WAIT(1),
    SCROLL_STOP, FADE, WAIT_X30(10), END,
};

/* The interpreter's state (staff_script_run; reset by staff_script_init). */
static s16 staff_script_pos = 0;     /* script position */
static s16 staff_script_ticks = 0;     /* ticks since the last step */
static s16 staff_script_wait = 0;     /* ticks until the next step */
static s32 staff_scroll_speed = STAFF_SCROLL_SPEED_DEFAULT; /* scroll speed: one line takes STAFF_LINE_SPAN / this ticks */
static s32 staff_scrolling = 1;     /* credits scroll while 1; SCROLL_STOP clears it */
static s32 staff_ended = 0;     /* set at END */

/* 1 while the script's SCROLL_BLOCK_ON is in force: staff_scroll_tick runs only then. */
static u32 staff_block_on = 0;
/* The steps of the credit-line fade (staff_line_step). A line is picked, its text is
 * waited for, the old face fades out and holds, then the new one fades in. */
enum StaffLineStep {
    STAFF_LINE_PICK = -1,
    STAFF_LINE_WAIT_TEXT = -2,
    STAFF_LINE_FADE_OUT = -3,
    STAFF_LINE_FADE_IN = 0,
};
#define STAFF_LINE_HOLD_FRAMES 60
static s16 staff_line_step = STAFF_LINE_PICK;
static u16 staff_line_text = 0;
static s16 staff_line_x = 0;
static s16 staff_line_y = 0;
static s16 staff_line_count = 0;
static s16 staff_line_index = 0;

/* Credits scroll entries, 25 per row, indexed by min(ending.route, 10). */
static u16 staff_scroll_table[11 * STAFF_CREDITS_PER_ROW + 1] = {
    0x0FEE, 0x0FE8, 0x0FE4, 0x0FDB, 0x0FE3, 0x0FD6, 0x0FE2, 0x0FCE, 0x0FE1, 0x0FC0, 0x0FE0, 0x0FB0,
    0x0FDF, 0x0FAB, 0x0FDE, 0x0FA4, 0x0FDD, 0x0F9F, 0x0EF1, 0x0F8B, 0x0EF0, 0x0F86, 0x0EF2, 0x0F4B,
    0x0000, 0x0FCA, 0x0FEE, 0x0FC9, 0x0FE8, 0x0FC8, 0x0FDB, 0x0FC7, 0x0FD6, 0x0FC6, 0x0FCE, 0x0FC5,
    0x0FC0, 0x0FC4, 0x0FB0, 0x0FC3, 0x0FAB, 0x0FC2, 0x0FA4, 0x0EEE, 0x0F9F, 0x0EED, 0x0F8B, 0x0EEF,
    0x0F4B, 0x0000, 0x0FBA, 0x0FEE, 0x0FB9, 0x0FE8, 0x0FB8, 0x0FDB, 0x0FB7, 0x0FD6, 0x0FB6, 0x0FCE,
    0x0FB5, 0x0FC0, 0x0FB4, 0x0FB0, 0x0FB3, 0x0FAB, 0x0FB2, 0x0FA4, 0x0EE7, 0x0F9F, 0x0EE6, 0x0F8B,
    0x0EE8, 0x0F4B, 0x0000, 0x0F9B, 0x0F9A, 0x0F99, 0x0F98, 0x0FEE, 0x0F97, 0x0FE8, 0x0F96, 0x0FDB,
    0x0F95, 0x0FD6, 0x0F94, 0x0FCE, 0x0F93, 0x0FC0, 0x0F92, 0x0FB0, 0x0EEB, 0x0FAB, 0x0EE9, 0x0FA4,
    0x0EEC, 0x0F9F, 0x0F4B, 0x0000, 0x0F76, 0x0FEE, 0x0F75, 0x0FE8, 0x0F74, 0x0FDB, 0x0F73, 0x0FD6,
    0x0F72, 0x0FCE, 0x0F71, 0x0FC0, 0x0F70, 0x0FB0, 0x0F6F, 0x0FAB, 0x0F6E, 0x0FA4, 0x0EE4, 0x0F9F,
    0x0EE3, 0x0F8B, 0x0EE5, 0x0F4B, 0x0000, 0x0FE4, 0x0FEE, 0x0FE8, 0x0FE3, 0x0FDB, 0x0FD6, 0x0FE2,
    0x0FCE, 0x0FC0, 0x0FE1, 0x0FB0, 0x0FAB, 0x0FA4, 0x0FE0, 0x0F9F, 0x0F8B, 0x0FDE, 0x0F86, 0x0F81,
    0x0FDD, 0x0F7C, 0x0F6C, 0x0FDF, 0x0F4B, 0x0000, 0x0FCA, 0x0FEE, 0x0FE8, 0x0FC9, 0x0FDB, 0x0FD6,
    0x0FC8, 0x0FCE, 0x0FC0, 0x0FC7, 0x0FB0, 0x0FC6, 0x0FAB, 0x0FC4, 0x0FA4, 0x0F9F, 0x0FC3, 0x0F8B,
    0x0F86, 0x0FC2, 0x0F81, 0x0F7C, 0x0FC5, 0x0F4B, 0x0000, 0x0FBA, 0x0FEE, 0x0FB9, 0x0FE8, 0x0FDB,
    0x0FB8, 0x0FD6, 0x0FCE, 0x0FB7, 0x0FC0, 0x0FB0, 0x0FB6, 0x0FAB, 0x0FA4, 0x0FB4, 0x0F9F, 0x0F8B,
    0x0FB3, 0x0F86, 0x0F81, 0x0FB2, 0x0F7C, 0x0FB5, 0x0F4B, 0x0000, 0x0F9B, 0x0FEE, 0x0F9A, 0x0FE8,
    0x0F99, 0x0FDB, 0x0FD6, 0x0F98, 0x0FCE, 0x0FC0, 0x0F97, 0x0FB0, 0x0F95, 0x0FAB, 0x0FA4, 0x0F94,
    0x0F9F, 0x0F8B, 0x0F93, 0x0F86, 0x0F92, 0x0F81, 0x0F96, 0x0F4B, 0x0000, 0x0F76, 0x0FEE, 0x0F75,
    0x0FE8, 0x0FDB, 0x0F74, 0x0FD6, 0x0FCE, 0x0F73, 0x0FC0, 0x0FB0, 0x0F72, 0x0FAB, 0x0FA4, 0x0F70,
    0x0F9F, 0x0F8B, 0x0F6F, 0x0F86, 0x0F81, 0x0F6E, 0x0F7C, 0x0F71, 0x0F4B, 0x0000, 0x0FED, 0x0FE7,
    0x0FDA, 0x0FD5, 0x0FCD, 0x0FBF, 0x0FAF, 0x0FAA, 0x0FA3, 0x0F9E, 0x0F8F, 0x0F8A, 0x0F85, 0x0F80,
    0x0F7B, 0x0F6B, 0x0F65, 0x0F60, 0x0F5B, 0x0F55, 0x0F4E, 0x0F44, 0x0F48, 0x0F48, 0x0000, 0x0000,
};

/* The step handlers, indexed by the step staff_tick is on. */
static void (*staff_step_handlers[7])(void) = {
    staff_enter, staff_load_wait, staff_load_tick, staff_roll_tick,
    staff_exit_fade_begin, staff_exit_fade_wait, staff_exit_scene,
};

/* The staff roll's two per-frame handlers, run by staff_roll_tick: the script
 * and the scroll block. */
static void (*staff_script_fn)(void) = staff_script_init;
static void (*staff_scroll_fn)(void) = staff_scroll_init;

/* The steps of staff_load_tick, in order; STAFF_LOAD_DONE leaves the load. */
enum StaffLoadStep {
    STAFF_LOAD_START = 0,
    STAFF_LOAD_IMAGES = 10,
    STAFF_LOAD_SOUND = 11,
    STAFF_LOAD_SOUND_WAIT = 12,
    STAFF_LOAD_SCENE_IMAGE = 20,
    STAFF_LOAD_ROLL_FILE = 21,
    STAFF_LOAD_DONE = 0xFF,
};

/* Loading step: staff_load_tick switches on it, staff_load_wait clears it. */
static s16 staff_load_step = STAFF_LOAD_START;

static s32 staff_frame;             /* the roll's frame counter, wrapping at STAFF_ROLL_WRAP */
/* The fade the staff roll runs over its sprites: idle, or fading out or in. */
enum StaffFadeMode {
    STAFF_FADE_IDLE,
    STAFF_FADE_OUT,
    STAFF_FADE_IN,
};

/* The steps of staff_roll_tick, in order; STAFF_ROLL_DONE leaves the roll. */
enum StaffRollStep {
    STAFF_ROLL_FADE_IN = 0,
    STAFF_ROLL_FADE_IN_WAIT = 1,
    STAFF_ROLL_SCROLL = 2,
    STAFF_ROLL_FADE_OUT_WAIT = 3,
    STAFF_ROLL_DONE = 0xFF,
};

static s16 staff_roll_step;           /* staff-roll step: staff_roll_tick switches on it */
static s16 staff_fade_mode;           /* a StaffFadeMode (staff_fade_draw) */
static StaffEntry staff_fade_rect;    /* the rectangle the fade covers */
static StaffParticle staff_particles[STAFF_PARTICLE_COUNT];
static AnimState staff_sprite_anim;   /* registration state of the file at staff_sprite_file */
static u8 staff_sprite_file[0x670];    /* file loaded by staff_load_sprites */
static AnimState staff_roll_anim;       /* registration state of the file at staff_roll_file */
static u8 staff_roll_file[0xD1C];    /* the staff-roll file, loaded by staff_load_roll_file */
static s16 staff_fade_level;          /* fade level, 0..STAFF_FADE_MAX */
static POLY_F4 staff_fade_poly;       /* the fade rectangle */
static DR_MODE staff_fade_blend;       /* its blend mode */

void staff_tick(void) {
    s32 saved;

    saved = text_get_palette();
    text_set_palette(3);
    staff_step_handlers[game_state.progress.entryStep]();
    text_set_palette(saved);
}

static void staff_reset_state(void) {
    staff_script_fn = staff_script_init;
    staff_load_step = STAFF_LOAD_START;
    staff_roll_step = STAFF_ROLL_FADE_IN;
    staff_frame = 0;
    staff_script_pos = 0;
    staff_script_ticks = 0;
    staff_script_wait = 0;
    staff_scrolling = 1;
    staff_scroll_speed = STAFF_SCROLL_SPEED_DEFAULT;
    staff_ended = 0;
    staff_scroll_fn = staff_scroll_init;
    staff_line_count = 0;
    staff_line_index = 0;
    staff_block_on = 0;
    staff_line_step = STAFF_LINE_PICK;
    staff_line_text = 0;
    staff_line_x = 0;
    staff_line_y = 0;
}

static void staff_enter(void) {
    staff_reset_state();
    fade_start_out(0, 0, 2);
    staff_clear_screen();
    menu_reset();
    game_state.progress.entryStep++;
}

static void staff_load_wait(void) {
    if (staff_load_sprites()) {
        if (fade_is_settled() == 1) {
            staff_load_step = STAFF_LOAD_START;
            game_state.progress.entryStep++;
        }
    }
}

static void staff_load_tick(void) {
    switch (staff_load_step) {
    case STAFF_LOAD_START:
        staff_load_step = STAFF_LOAD_IMAGES;
        break;
    case STAFF_LOAD_IMAGES:
        staff_load_images();
        staff_load_step++;
        break;
    case STAFF_LOAD_SOUND:
        sound_load_tick();
        if (sound_fader_ready() == 0) return;
        sound_fade_in(10);
        staff_load_step++;
        break;
    case STAFF_LOAD_SOUND_WAIT:
        if (sound_fader_mode() == SOUND_FADER_IDLE) {
            staff_load_step = STAFF_LOAD_SCENE_IMAGE;
        }
        break;
    case STAFF_LOAD_SCENE_IMAGE:
        staff_image_request();
        staff_load_step++;
        break;
    case STAFF_LOAD_ROLL_FILE:
        if (staff_load_roll_file()) {
            staff_load_step = STAFF_LOAD_DONE;
        }
        break;
    case STAFF_LOAD_DONE:
        staff_load_step = STAFF_LOAD_START;
        sound_update();
        sound_seq_start_reverb(0, 0, 0x23, 127);
        game_state.progress.entryStep++;
        break;
    }
}

/* Run the roll's steps in order: fade in, scroll the script and the credit lines until
 * the script ends, then fade out. Each frame draws the roll's sprite. */
static void staff_roll_tick(void) {
    anim_draw_sprt_cursor(&current_frame->otTags[0][6], &staff_roll_anim, 0x1F, 0, 0, -7);
    prim_add_tile(&current_frame->otTags[0][5], 0, 0, HWD0, 140, 0, 1);

    staff_frame = (staff_frame + 1) % STAFF_ROLL_WRAP;

    switch (staff_roll_step) {
    case STAFF_ROLL_FADE_IN:
        fade_start_in(STAFF_FADE_FRAMES, 0, 2);
        staff_roll_step++;
        break;
    case STAFF_ROLL_FADE_IN_WAIT:
        if (fade_is_settled() == 1) {
            staff_roll_step++;
        }
        break;
    case STAFF_ROLL_SCROLL:
        staff_script_fn();
        staff_scroll_fn();
        if (staff_ended == 1) {
            fade_start_out(STAFF_FADE_FRAMES, 0, 2);
            staff_roll_step++;
        }
        break;
    case STAFF_ROLL_FADE_OUT_WAIT:
        if (fade_is_settled() == 1) {
            staff_roll_step = STAFF_ROLL_DONE;
        }
        break;
    case STAFF_ROLL_DONE:
        game_state.progress.entryStep++;
        break;
    }
}

/* The two steps that end a scene: start the screen and the music fading out, then wait until
 * both have settled, stop the music and release its bank, and move on. */
static void staff_exit_fade_begin(void) {
    fade_start_out(10, 0, 2);
    sound_fade_out(10);
    game_state.progress.entryStep++;
}

static void staff_exit_fade_wait(void) {
    u8 count = (fade_is_settled() == 1);

    if (sound_fader_mode() == SOUND_FADER_IDLE) {
        count = (count + 1) & 0xFF;
    }
    if (count == 2) {
        sound_seq_stop_first(0);
        sound_update();
        sound_release_from(2, 0);
        game_state.progress.entryStep++;
    }
}

/* Leave the staff roll: the title after the last route, otherwise back to the farm. */
static void staff_exit_scene(void) {
    game_state.progress.entryStep = 0;
    if (game_state.ending.route >= STAFF_ROUTE_CAP)
        main_request_scene(SCENE_TITLE);
    else
        main_request_scene(SCENE_FARM_STAFF_EXIT);
}

/* Reset the script interpreter, and the particles, on its first tick. */
static void staff_script_init(void) {
    staff_particles_clear();
    staff_script_fn = staff_script_run;
    staff_script_pos = 0;
    staff_script_ticks = 0;
    staff_script_wait = 0;
    staff_scrolling = 1;
    staff_scroll_speed = STAFF_SCROLL_SPEED_DEFAULT;
    staff_ended = 0;
}

/* The high nibble of an opcode, which picks its class in staff_script_run. */
enum StaffOpClass {
    STAFF_OP_COMMAND = 0xF0,            /* FADE, SCROLL_STOP, SCROLL_BLOCK_ON, END and SCROLL_BLOCK_OFF */
    STAFF_OP_WAIT_X30 = 0xE0,
    STAFF_OP_WAIT = 0xD0,
};

/* Run the script: one opcode per step until a wait is set, then sleep for its ticks. */
static void staff_script_run(void) {
    u8 code;
    s32 wait;

    staff_script_ticks++;
    staff_particles_draw();
    if (staff_scrolling == 1) {
        staff_particles_move();
    }
    if (staff_script_wait > 0) {
        if (staff_script_ticks < staff_script_wait) {
            return;
        }
        staff_script_wait = 0;
    }
next:
    code = staff_script[staff_script_pos];
    wait = STAFF_LINE_SPAN / staff_scroll_speed;
    staff_script_wait = wait;
    staff_script_ticks = 0;
    switch (code & 0xF0) {
    case STAFF_OP_COMMAND:
        if (code == END) {
            staff_ended = 1;
        } else if (code == SCROLL_BLOCK_OFF) {
            staff_block_on = 0;
        } else if (code == SCROLL_BLOCK_ON) {
            staff_block_on = 1;
        } else if (code == SCROLL_STOP) {
            staff_scrolling = 0;
        } else if (code == FADE) {
            sound_fade_out(60);
        }
        staff_script_pos++;
        goto next;
    case STAFF_OP_WAIT_X30:
        staff_script_wait = wait + (code & 0xF) * 30;
        break;
    case STAFF_OP_WAIT:
        staff_script_wait = wait + (STAFF_WAIT_SPAN / staff_scroll_speed) * (code & 0xF);
        break;
    default:
        staff_credit_start(code);
        break;
    }
    staff_script_pos++;
}

/* Start the staff roll: install its handler, reset the scroll block, and take
 * the entry count from game_state.ending.route (capped at ten). */
static void staff_scroll_init(void) {
    u16 count;

    staff_scroll_fn = staff_scroll_tick;
    staff_line_step = STAFF_LINE_PICK;
    staff_line_text = 0;
    staff_line_x = 0;
    staff_line_y = 0;
    staff_block_on = 0;
    count = game_state.ending.route;
    staff_line_index = 0;
    if (count >= STAFF_ROUTE_CAP) {
        staff_line_count = STAFF_ROUTE_CAP;
    } else {
        staff_line_count = count;
    }
}

/* Each credit line: pick the next credit, wait for its text, fade its panel out (revealing
 * the text), hold it under a tinted panel, then fade the panel back in. */
static void staff_scroll_tick(void) {
    u_long *ot = &current_frame->otTags[0][4];

    if (staff_block_on != 1) {
        return;
    }
    switch (staff_line_step) {
    case STAFF_LINE_PICK:
        if (staff_scrolling == 1) {
            staff_line_text = staff_scroll_table[staff_line_count * STAFF_CREDITS_PER_ROW + staff_line_index];
            staff_line_index++;
            staff_line_x = rand_range((u16)(HWD0 - STAFF_LINE_SIZE));
            staff_line_y = STAFF_LINE_START_Y;
            staff_line_step = STAFF_LINE_WAIT_TEXT;
        }
        break;
    case STAFF_LINE_WAIT_TEXT:
        if (text_bank_ready(staff_line_text) == 1) {
            staff_fade_out_begin(staff_line_x, staff_line_y);
            staff_line_step = STAFF_LINE_FADE_OUT;
        }
        break;
    case STAFF_LINE_FADE_OUT:
        staff_fade_draw();
        text_draw_face_by_code(ot, staff_line_text, staff_line_x, staff_line_y);
        if (staff_fade_idle()) {
            staff_line_step = STAFF_LINE_HOLD_FRAMES; /* not a step: the default arm counts this down */
        }
        break;
    case STAFF_LINE_FADE_IN:
        staff_fade_draw();
        text_draw_face_by_code(ot, staff_line_text, staff_line_x, staff_line_y);
        if (staff_fade_idle()) {
            staff_line_step = STAFF_LINE_PICK;
        }
        break;
    default:
        text_draw_face_by_code(ot, staff_line_text, staff_line_x, staff_line_y);
        staff_gouraud_rect(&current_frame->otTags[0][3], staff_line_x, staff_line_y, STAFF_LINE_SIZE,
                           STAFF_LINE_SIZE, STAFF_HOLD_COLOUR, STAFF_HOLD_COLOUR, STAFF_HOLD_COLOUR,
                           STAFF_HOLD_COLOUR, 1, 2);
        if (--staff_line_step == 0) {
            staff_fade_in_begin(staff_line_x, staff_line_y);
        }
        break;
    }
}

/* Empty the credit particles. */
static void staff_particles_clear(void) {
    StaffParticle *p = staff_particles;
    s32 value = STAFF_PARTICLE_START;
    s32 i;
    for (i = STAFF_PARTICLE_COUNT - 1; i >= 0; i--) {
        p->live = 0;
        p->code = 0;
        p->time = value;
        p++;
    }
}

/* Start a credit particle in the first free slot; nothing happens when all are live. */
static void staff_credit_start(s16 code) {
    s32 i;

    for (i = 0; i < STAFF_PARTICLE_COUNT; i++) {
        if (staff_particles[i].live == 0) {
            break;
        }
    }
    if (i != STAFF_PARTICLE_COUNT) {
        staff_particles[i].live = 1;
        staff_particles[i].code = code;
        staff_particles[i].time = STAFF_PARTICLE_START;
    }
}

/* Draw each live credit sprite, offset by its time. */
static void staff_particles_draw(void) {
    s32 i;

    for (i = 0; i < STAFF_PARTICLE_COUNT; i++) {
        if (staff_particles[i].live == 1) {
            anim_draw_sprt_cursor(&current_frame->otTags[0][2], &staff_sprite_anim, 0,
                          staff_particles[i].code, 0, staff_particles[i].time / FIXED_ONE);
        }
    }
}

/* Move the live credit particles on by the scroll speed; one whose time passes the end goes dead. */
static void staff_particles_move(void) {
    s32 one = 1;
    s32 delta = staff_scroll_speed;
    s32 limit = STAFF_PARTICLE_END;
    StaffParticle *p = staff_particles;
    s32 i = STAFF_PARTICLE_COUNT - 1;
    do {
        if (p->live == one) {
            p->time -= delta;
            if (p->time <= limit) p->live = 0;
        }
        i--;
        p++;
    } while (i >= 0);
}

/* Wait for the display, clear the ordering table, and start reading the sprite file. */
static void staff_clear_screen(void) {
    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0, STAFF_SPRITE_SECTOR, STAFF_SPRITE_SIZE, STAGING_BUFFER, 1, 1);
}

/* Read the sprite file; once it has arrived, load its textures and bind its animation.
 * Returns 1 when done. */
static s32 staff_load_sprites(void) {
    if (afile_read_bytes(0, STAFF_SPRITE_SECTOR, STAFF_SPRITE_SIZE, STAGING_BUFFER, 1, 0) == 0) return 0;

    texture_pack_load(region_base + STAFF_SPRITE_PACK_OFFSET);
    __builtin_memcpy(staff_sprite_file, STAGING_BUFFER, sizeof(staff_sprite_file));
    anim_bind_file(&staff_sprite_anim, (AnimFile *)staff_sprite_file);
    anim_select_range(&staff_sprite_anim, 0);
    return 1;
}

/* Reset the sound, then queue sequence 1 and song 0x23 for the roll. */
static void staff_load_images(void) {
    sound_set_seq_arg(0, STAGING_BUFFER);
    sound_request(SOUND_REQUEST_RESET, 0);
    sound_request(SOUND_REQUEST_SEQ_1, 0);
    sound_request(0x23, 0);
}

/* Where the scene's image starts in A_FILE member 0, and its size. */
#define SCENE_IMAGE_SECTOR 0x2000
#define SCENE_IMAGE_SIZE 0x23000

/* Wait for the display to finish, clear the ordering table and start reading the scene's
 * image into the staging buffer. The next step collects it. */
static void staff_image_request(void) {
    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    afile_read_bytes(0, SCENE_IMAGE_SECTOR, SCENE_IMAGE_SIZE, STAGING_BUFFER, 1, 1);
}

/* Collect the scene image that staff_image_request read, load the roll's textures from
 * its pack, copy the file in and bind its animation. Returns 1 when done. */
static s32 staff_load_roll_file(void) {
    if (afile_read_bytes(0, SCENE_IMAGE_SECTOR, SCENE_IMAGE_SIZE, STAGING_BUFFER, 1, 0) == 0) return 0;

    texture_pack_load(region_base + STAFF_ROLL_PACK_OFFSET);
    __builtin_memcpy(staff_roll_file, STAGING_BUFFER, sizeof(staff_roll_file));
    anim_bind_file(&staff_roll_anim, (AnimFile *)staff_roll_file);
    anim_select_range(&staff_roll_anim, 0x1F);
    return 1;
}

/* Queue a gouraud rectangle (four corner colours 0xRRGGBB) and the draw-mode
 * packet that sets its blend rate. */
static void staff_gouraud_rect(u_long *ot, s16 x, s16 y, s16 w, s16 h, s32 c0, s32 c1, s32 c2, s32 c3,
                   s16 semiTrans, u8 blend) {
    POLY_G4 *poly;
    DR_TPAGE *mode;

    poly = gs_work_alloc(0);
    setlen(poly, 8);
    poly->code = 0x38;
    poly->code = semiTrans ? poly->code | 2 : poly->code & ~2;
    poly->x0 = x;
    poly->y0 = y;
    poly->x1 = x + w;
    poly->y1 = y;
    poly->x2 = x;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    poly->r0 = c0 >> 16;
    poly->g0 = c0 >> 8;
    poly->b0 = c0;
    poly->r1 = c1 >> 16;
    poly->g1 = c1 >> 8;
    poly->b1 = c1;
    poly->r2 = c2 >> 16;
    poly->g2 = c2 >> 8;
    poly->b2 = c2;
    poly->r3 = c3 >> 16;
    poly->g3 = c3 >> 8;
    poly->b3 = c3;
    addPrim(ot, poly);

    poly++;
    mode = (DR_TPAGE *)poly;
    setlen(mode, 1);
    mode->code[0] = 0xE1000200 | ((blend & 3) << 5);
    addPrim(ot, mode);

    gs_work_release(mode + 1);
}

/* Step the staff-roll fade (StaffFadeMode) and draw the translucent rectangle
 * staff_fade_rect; stop once it is fully faded in or out. */
static void staff_fade_draw(void) {
    u_long *ot;

    if (staff_fade_mode == STAFF_FADE_IDLE) return;

    if (staff_fade_mode == STAFF_FADE_OUT) {
        staff_fade_level -= STAFF_FADE_STEP;
        if (staff_fade_level <= 0) staff_fade_level = 0;
    } else if (staff_fade_mode == STAFF_FADE_IN) {
        staff_fade_level += STAFF_FADE_STEP;
        if (staff_fade_level >= STAFF_FADE_MAX) staff_fade_level = STAFF_FADE_MAX;
    }

    SetPolyF4(&staff_fade_poly);
    staff_fade_poly.r0 = staff_fade_level * 180 / 255 + 75;
    staff_fade_poly.g0 = staff_fade_level * 155 / 255 + 100;
    staff_fade_poly.b0 = staff_fade_level * 105 / 255 - 106;
    staff_fade_poly.x0 = staff_fade_rect.x;
    staff_fade_poly.y0 = staff_fade_rect.y;
    staff_fade_poly.x1 = staff_fade_rect.x + staff_fade_rect.width;
    staff_fade_poly.y1 = staff_fade_rect.y;
    staff_fade_poly.x2 = staff_fade_rect.x;
    staff_fade_poly.y2 = staff_fade_rect.y + staff_fade_rect.height;
    staff_fade_poly.x3 = staff_fade_rect.x + staff_fade_rect.width;
    staff_fade_poly.y3 = staff_fade_rect.y + staff_fade_rect.height;
    SetSemiTrans(&staff_fade_poly, 1);

    ot = current_frame->otTags[0];
    addPrim(&ot[3], &staff_fade_poly);
    SetDrawMode(&staff_fade_blend, 0, 1, GetTPage(0, 2, 0, 0), 0);
    addPrim(&ot[3], &staff_fade_blend);

    if (staff_fade_level == 0 || staff_fade_level == STAFF_FADE_MAX) staff_fade_mode = STAFF_FADE_IDLE;
}

static void staff_fade_out_begin(s16 x, s16 y) {
    staff_fade_mode = STAFF_FADE_OUT;
    staff_fade_level = STAFF_FADE_MAX;
    staff_fade_rect.x = x;
    staff_fade_rect.y = y;
    staff_fade_rect.width = STAFF_LINE_SIZE;
    staff_fade_rect.height = STAFF_LINE_SIZE;
}

static void staff_fade_in_begin(s16 x, s16 y) {
    staff_fade_mode = STAFF_FADE_IN;
    staff_fade_level = 0;
    staff_fade_rect.x = x;
    staff_fade_rect.y = y;
    staff_fade_rect.width = STAFF_LINE_SIZE;
    staff_fade_rect.height = STAFF_LINE_SIZE;
}

static s32 staff_fade_idle(void) {
    return staff_fade_mode == STAFF_FADE_IDLE;
}
