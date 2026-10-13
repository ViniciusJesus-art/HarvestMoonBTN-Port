/* Menus that open over the field: the window frame, the box animation, the menu state
 * machine and the menu's sound switch. */
#include "types.h"
#include "afile.h"
#include "anim.h"
#include "event_flags.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "heart.h"
#include "main.h"
#include "menu_window.h"
#include "name_entry.h"
#include "text.h"
#include "window_frame.h"

typedef s32 (*Handler)();

/* The menu box: where it opens and closes, and its size. */
#define MENU_BOX_X 26
#define MENU_BOX_Y 164
#define MENU_BOX_W 267
#define MENU_BOX_H 62

/* Draw the bevelled window frame at (x, y), w by h. */
void window_frame_draw_at(u_long *ot, s32 unused, s16 x, s16 y, s16 w, s16 h) {
    window_frame_draw(ot, x, y, w, h);
}

static s32 menu_begin_load(void);
static s32 menu_load_result(void);
static signed short menu_result_file(void);
static s32 menu_open_step(void);
static s32 menu_run_frame(void);
static s32 menu_close_step(void);
static s32 menu_finish(void);

s32 menu_data_index;
u32 menu_id;
u16 *menu_text;
/* The message window's status (TextWindowStatus) the menu runs on; menu_phase_is_result
 * tests for the closed and end-wait states. */
s32 menu_phase;
int menu_chosen;
s32 menu_event_bits;
s32 menu_saved_flags;
s32 menu_queued_id;
s32 menu_queued_arg;
s32 menu_box_frame;

AnimState menu_anim;

s32 menu_sound_enabled = 1;

int menu_choice(void) {
    return menu_chosen;
}

static const Handler menu_handlers[8] = {
    0, menu_begin_load, menu_load_result, menu_open_step, menu_run_frame, menu_close_step, menu_finish, 0,
};

int menu_run_state(void) {
    unsigned char index = game_state.system.menuState;
    if (index == MENU_STATE_IDLE) {
        return 1;
    }
    return menu_handlers[index](&game_state);
}

/* Open menu `id` with its data at `dataIndex`. Returns 0 when it starts, or when it is
 * queued behind the menu that is closing (menu_finish opens it); 1 when it is refused. */
s32 menu_request_open(s32 id, s32 dataIndex) {
    s32 result = 1;
    if (game_state.system.menuState == MENU_STATE_IDLE) {
        game_state.system.menuState = MENU_STATE_BEGIN_LOAD;
        menu_id = id;
        menu_data_index = dataIndex;
        menu_phase = TEXT_WINDOW_PRINTING;
        result = 0;
    }
    if ((u32)(game_state.system.menuState - MENU_STATE_CLOSE) < 2U) {
        if (menu_id != id || menu_data_index != dataIndex) {
            if (menu_queued_id == -1) {
                result = 0;
                menu_queued_id = id;
                menu_queued_arg = dataIndex;
            }
        }
    }
    return result;
}

enum TextWindowStatus menu_get_phase(void) {
    return menu_phase;
}

signed int menu_phase_is_result(void) {
    signed int isResult;

    isResult = 0;
    if ((menu_phase == TEXT_WINDOW_CLOSED) || (menu_phase == TEXT_WINDOW_END_WAIT)) {
        isResult = 1;
    }
    return isResult;
}

void menu_set_sound_enabled(signed int enabled) {
    menu_sound_enabled = enabled;
}

static signed int menu_get_sound_enabled(void) {
    return menu_sound_enabled;
}

/* Reset the menu state and the menu's saved flags; a pending request is cleared. */
void menu_reset(void) {
    menu_saved_flags = 0;
    menu_finish();
    game_state.system.menuState = MENU_STATE_IDLE;
    text_set_input(1);
    menu_set_sound_enabled(1);
    menu_phase = TEXT_WINDOW_CLOSED;
    menu_queued_id = -1;
}

/* Start closing the running menu: its box shrinks, and the window stops taking input. */
void menu_begin_close(void) {
    text_set_input(1);
    menu_set_sound_enabled(1);
    if (game_state.system.menuState == MENU_STATE_RUN) {
        game_state.system.menuState = MENU_STATE_CLOSE;
        menu_box_frame = 0;
        menu_phase = TEXT_WINDOW_DONE;
    }
}

static void menu_set_saved_flags(int flags) {
    menu_saved_flags = flags;
}

/* One frame of a box opening from (or, mode != 1, closing to) its centre over
 * `duration` + 6 frames, counted in *frame. The first 6 frames grow a 4-high
 * bar outwards to the full width; the rest grow it vertically to the full
 * height. Draws the current box (window_frame_draw_at) and returns 0 while running;
 * on the last frame it draws the full box (opening only), pins *frame at the
 * end and returns 1. */
s32 menu_box_animate(u_long *ot, s32 unused, s16 x, s16 y, s16 w, s16 h, s32 *frame, s32 duration, s32 mode) {
    s32 heightStep;
    s32 widthStep;
    s32 t;
    s32 elapsed;
    s32 end;
    s32 scaled;
    s32 boxX;
    s32 boxY;
    s32 boxW;
    s32 boxH;

loop:
    boxY = y + h / 2;
    boxX = x + w / 2;
    heightStep = (h << 11) / duration;
    widthStep = (w << 11) / 6;
    if (mode == 1) {
        t = *frame;
    } else {
        elapsed = *frame - 6;
        t = duration - elapsed;
    }
    if (t < 6) {
        scaled = widthStep * t;
        t = scaled / 4096;
        if (widthStep) scaled = 0;
        boxX -= t;
        boxH = 4;
        boxW = t * 2;
    } else {
        scaled = heightStep * (t - 6);
        boxX = x;
        boxY -= scaled / 4096;
        boxH = scaled / 4096 * 2;
        boxW = w;
        if (boxH < 3) {
            (*frame)++;
            goto loop;
        }
        if (boxH < 4) {
            boxH = 4;
        }
    }
    end = duration + 6;
    if (*frame >= end) {
        if (mode == 1) {
            window_frame_draw_at(ot, 0, x, y, w, h);
        }
        *frame = end;
        return 1;
    }
    window_frame_draw_at(ot, 0, boxX, boxY, boxW, boxH);
    (*frame)++;
    return 0;
}

/* Pause the game, hold the event flag, and read the menu's result file into the work
 * sector. */
static s32 menu_begin_load(void) {
    s32 flags;
    FarmState *state;
    menu_event_bits = event_flag_get(EVENT_FLAG_EVENT_ACTIVE);
    event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 1);
    state = &game_state;
    menu_phase = 14;
    flags = state->globalFlags;
    menu_saved_flags = flags;
    flags |= GLOBAL_FLAG_PAUSED;
    state->globalFlags = flags;
    afile_read_bytes(menu_result_file(), menu_data_index << 13, 0x2000, &state->work.sector[0], 0, 1);
    menu_box_frame = 0;
    state->system.menuState = MENU_STATE_LOAD_RESULT;
    return 0;
}

/* Wait for the result file to arrive, then load its text and move to OPEN, or straight
 * to RUN when the box has already finished opening. */
static s32 menu_load_result(void) {
    u8 *loaded;
    s32 result;
    s8 status;
    loaded = afile_read_bytes(menu_result_file(), menu_data_index << 13, 0x2000, game_state.work.sector, 0, 0);
    result = menu_box_animate(&current_frame->otTags[0][2], 0, MENU_BOX_X, MENU_BOX_Y, MENU_BOX_W, MENU_BOX_H, &menu_box_frame, 12, 1);
    if (loaded != 0) {
        text_load_record(loaded);
        menu_text = (u16 *)(game_state.work.sector + 0x1800);
        text_reset();
        status = MENU_STATE_OPEN;
        if (result == 1) status = MENU_STATE_RUN;
        game_state.system.menuState = status;
        anim_bind_file(&menu_anim, (AnimFile *)farm_area.stageData);
        farm_work_sector_release();
    }
    return 0;
}

/* The result file for each menu_id 0 to 4; ids 5 and up read file 0. */
signed short menu_window_values[6] = {0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0000};

static signed short menu_result_file(void) {
    unsigned int index;
    index = menu_id;
    if (index >= 5U) {
        index = 0;
    }
    return menu_window_values[index];
}

/* Grow the box; once it is full, the menu is RUN. */
static s32 menu_open_step(void) {
    s32 result;

    result = menu_box_animate(&current_frame->otTags[0][2], 0, MENU_BOX_X, MENU_BOX_Y, MENU_BOX_W, MENU_BOX_H, &menu_box_frame, 12, 1);
    if (result == 1) {
        game_state.system.menuState = MENU_STATE_RUN;
    }
    return 0;
}

/* Run one frame of the menu drawn at menu_text and act on its window status: CLOSED
 * closes it; the wait states (1, 3, 4, 5) show the confirm heart; CHOICE_0 to CHOICE_3
 * (10 to 13) close it recording choice 1 to 4 in menu_chosen. Redraws the window
 * unless the state is FINISH. */
static s32 menu_run_frame(void) {
    s32 confirm;
    s32 saved;

    text_set_spacing(0, 2);
    confirm = 0;
    saved = text_get_palette();
    text_set_palette(3);
    menu_phase = text_window_draw(&current_frame->otTags[0][2], menu_text, 30, 168, 263, 58);
    text_set_palette(saved);
    text_set_spacing(0, 0);
    menu_chosen = 0;
    switch (menu_phase) {
    case TEXT_WINDOW_PRINTING:
        break;
    case TEXT_WINDOW_CLOSED:
        game_state.system.menuState = MENU_STATE_CLOSE;
        menu_phase = TEXT_WINDOW_DONE;
        break;
    case TEXT_WINDOW_SCROLL_WAIT:
    case TEXT_WINDOW_PAGE_WAIT:
    case TEXT_WINDOW_RESERVED_4:
    case TEXT_WINDOW_END_WAIT:
        confirm = 1;
        break;
    case TEXT_WINDOW_CHOICE_0:
        game_state.system.menuState = MENU_STATE_CLOSE;
        menu_chosen = 1;
        menu_phase = TEXT_WINDOW_DONE;
        break;
    case TEXT_WINDOW_CHOICE_1:
        game_state.system.menuState = MENU_STATE_CLOSE;
        menu_chosen = 2;
        menu_phase = TEXT_WINDOW_DONE;
        break;
    case TEXT_WINDOW_CHOICE_2:
        game_state.system.menuState = MENU_STATE_CLOSE;
        menu_chosen = 3;
        menu_phase = TEXT_WINDOW_DONE;
        break;
    case TEXT_WINDOW_CHOICE_3:
        game_state.system.menuState = MENU_STATE_CLOSE;
        menu_chosen = 4;
        menu_phase = TEXT_WINDOW_DONE;
        break;
    }
    if (game_state.system.menuState != MENU_STATE_FINISH) {
        window_frame_draw_at(&current_frame->otTags[0][3], 0, MENU_BOX_X, MENU_BOX_Y, MENU_BOX_W, MENU_BOX_H);
        if (confirm && menu_sound_enabled == 1) {
            heart_icon_draw();
        }
    }
    if (game_state.system.menuState == MENU_STATE_CLOSE) {
        menu_box_frame = 0;
    }
    return 0;
}

/* Shrink the box; once it is gone, the menu is FINISH. */
static s32 menu_close_step(void) {
    s32 result;

    result = menu_box_animate(&current_frame->otTags[0][2], 0, MENU_BOX_X, MENU_BOX_Y, MENU_BOX_W, MENU_BOX_H, &menu_box_frame, 6, 0);
    if (result == 1) {
        game_state.system.menuState = MENU_STATE_FINISH;
        if (menu_queued_id == -1) {
            menu_phase = TEXT_WINDOW_CLOSED;
        }
    }
    return 0;
}

/* Restore the pause and event flags, return to IDLE, and open any queued request.
 * Returns 1. */
static s32 menu_finish(void) {
    s32 request;

    event_flag_set(EVENT_FLAG_EVENT_ACTIVE, *(u16 *)&menu_event_bits);
    if (menu_saved_flags & GLOBAL_FLAG_PAUSED) {
        game_state.globalFlags |= GLOBAL_FLAG_PAUSED;
    } else {
        game_state.globalFlags &= ~GLOBAL_FLAG_PAUSED;
    }
    game_state.system.menuState = MENU_STATE_IDLE;
    request = menu_queued_id;
    if (request != -1) {
        menu_request_open(request, menu_queued_arg);
        menu_queued_id = -1;
    } else {
        menu_phase = TEXT_WINDOW_CLOSED;
    }
    return 1;
}
