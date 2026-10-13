/* The name-entry screen: the letter and symbol grids, their cursor and markers, and the name display. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "actor.h"
#include "farm_state.h"
#include "main.h"
#include "menu_window.h"
#include "name_entry.h"
#include "pad.h"
#include "prim.h"
#include "sound.h"
#include "sound_request.h"
#include "text.h"
#include "text_bank.h"
#include "texture_pack.h"
#include "ui_icons.h"

/* The keyboard glyph at (col, row) in the name-entry grid. */
typedef u16 (*NameKeyCharHandler)(s32 col, s32 row);

/* Maps a cursor cell (col, row) and a step direction to the wrapped axis value. */
typedef s32 (*CursorFixup)(s32 col, s32 row, s32 forward);

static void name_sound_draw(void);
static void name_sound_draw_frame(void);
static void name_title_build_display(void);
static void name_sound_update(void);
static void name_sound_commit_value(void);
static void name_sound_commit_next(void);
static void name_sound_set_done(void);
static void name_sound_load_value(void);
static s32 name_sound_move_position(s16 position);
static s32 name_sound_move_index(s16 index);
static void name_sound_handle_input(void);
static void name_grid_step(void);
static void name_buffer_clear(void);
static void name_title_prepare(s32 from_name);
static void name_title_build_name(u16 *dest);
static void name_title_build_label(u16 *dest);
static void name_grid_draw_caption(void);
static void name_entry_key_input(void);
static void name_entry_confirm_input(void);
static void name_prompt_draw(s32 message, u16 row);
static u16 name_cursor_move(u16 cell, u16 button, u32 grid);
static signed int name_cursor_wrap_grid0(signed int col, signed int row, signed int forward);
static int name_cursor_wrap_vertical(int col, int row, int forward);
static signed int name_cursor_wrap_grid1(signed int col, signed int row, signed int forward);
static void name_marker_draw(u16 cell);
static u16 name_key_char(u16 cell, u32 grid);
static u16 name_key_cell_letters(s32 col, s32 row);
static unsigned short name_key_char_symbols(signed int col, signed int row);

static u16 name_display_text[16];

/* The steps of the voice and birthday screen (name_sound_state): pick the first
 * value (an icon), pick the day, confirm, then the way out: back, or done. */
enum NameSoundState {
    NAME_SOUND_IDLE = 0,
    NAME_SOUND_PICK_ICON = 1,
    NAME_SOUND_PICK_DAY = 2,
    NAME_SOUND_CONFIRM = 3,
    NAME_SOUND_BACK = 4,
    NAME_SOUND_DONE = 5,
};

u16 name_sound_state = 0;
s16 name_sound_value = 0;
u16 name_prompt_row = 0;
SoundSequence *name_sound_seq = 0;
/* The steps of the name entry screen (name_phase): idle, the letter grid, the
 * confirm prompt, and the two ways out of it: cancelled, or accepted. */
enum NameEntryPhase {
    NAME_PHASE_IDLE = 0,
    NAME_PHASE_GRID = 1,
    NAME_PHASE_CONFIRM = 2,
    NAME_PHASE_CANCEL = 3,
    NAME_PHASE_ACCEPT = 4,
};

u16 name_phase = NAME_PHASE_IDLE;
/* Whether the name entry can be cancelled: 1 for name_entry_start_cancellable, 0 for name_entry_start_no_cancel. */
s32 name_cancel_allowed = 1;
s16 name_entry_unused_8005E34C = 0;
u16 name_length = 0;
s32 name_dirty = 0;
u16 name_prompt_toggle = 0;
/* The keyboard grid: NAME_GRID_COUNT grids of NAME_GRID_COLS columns. */
#define NAME_GRID_COLS 14
#define NAME_GRID_COUNT 2
/* The keys by cell (row * NAME_GRID_COLS + col): shift, space and the end key. */
#define NAME_CELL_SHIFT 0xD
#define NAME_CELL_SPACE 0x1B
#define NAME_CELL_END 0x53
/* The end of the name buffer. */
#define NAME_END 0xFFFF
/* The birthday picker: 7 columns of days, 30 days in all, and 4 icons. */
#define BIRTHDAY_DAY_COUNT 30
#define BIRTHDAY_COLS 7
#define BIRTHDAY_ICON_COUNT 4

/* The name-entry cursor: a cell `row * NAME_GRID_COLS + col` of the keyboard grid. */
s16 name_cursor_cell = 0;
s16 name_shift = 0;
/* Title mode of the name entry: name_entry_reset clears it, name_entry_start sets 1. */
s32 name_title_mode = 0;
/* The name being entered: up to eight characters and a 0xFFFF terminator. */
u16 *name_buffer = 0;

/* Load a text record: its first word is the text bank (-1 for none), and the
 * texture pack follows it. */
void text_load_record(void *record) {
    signed int bank;

    bank = *(signed int *)record;
    if (bank != -1) {
        texture_pack_load(record + 4);
        text_set_bank(bank);
    }
}

void name_sound_reset(void) {
    name_sound_state = NAME_SOUND_IDLE;
    name_sound_value = 0;
    name_prompt_row = 0;
}

void name_sound_begin(SoundSequence *sequence) {
    name_sound_reset();
    name_sound_seq = sequence;
    name_sound_state = NAME_SOUND_PICK_ICON;
}

/* Run one step of the sound player. Returns NAME_STEP_CANCELLED when the player backs out,
 * NAME_STEP_ACCEPTED when it is done, NAME_STEP_EDITING while it runs, NAME_STEP_IDLE otherwise. */
s32 name_sound_step(void) {
    if (name_sound_state != 0) {
        prim_add_tile(&current_frame->otTags[0][27], 0, 0, HWD0, VWD0, 0, 1);
        name_sound_draw();
        switch (name_sound_state) {
        case NAME_SOUND_BACK:
            name_sound_state = NAME_SOUND_IDLE;
            return NAME_STEP_CANCELLED;
        case NAME_SOUND_DONE:
            name_sound_state = NAME_SOUND_IDLE;
            return NAME_STEP_ACCEPTED;
        }
        return NAME_STEP_EDITING;
    }
    return 0;
}

/* Reset the name entry state; the buffer itself is kept. */
void name_entry_reset(void) {
    name_phase = NAME_PHASE_IDLE;
    name_cancel_allowed = 1;
    name_prompt_toggle = 0;
    name_cursor_cell = 0;
    name_shift = 0;
    name_entry_unused_8005E34C = 0;
    name_length = 0;
    name_title_mode = 0;
    name_dirty = 0;
}

/* Start a name entry that can be cancelled, with the name shown in the title. */
void name_entry_start(u16 *name) {
    name_entry_reset();
    name_buffer = name;
    name_buffer_clear();
    name_cancel_allowed = 1;
    name_title_mode = 1;
    name_phase = NAME_PHASE_GRID;
}

/* Start a name entry that cannot be cancelled while the name is empty. */
void name_entry_start_no_cancel(u16 *name) {
    name_entry_reset();
    name_buffer = name;
    name_buffer_clear();
    name_cancel_allowed = 0;
    name_phase = NAME_PHASE_GRID;
}

/* Start a name entry that can be cancelled, with the localized label in the title. */
void name_entry_start_cancellable(u16 *name) {
    name_entry_reset();
    name_buffer = name;
    name_buffer_clear();
    name_cancel_allowed = 1;
    name_phase = NAME_PHASE_GRID;
}

s32 name_entry_step(void) {
    if (name_phase != NAME_PHASE_IDLE) {
        prim_add_tile(&current_frame->otTags[0][27], 0, 0, HWD0, VWD0, 0, 1);
        name_grid_step();
        switch (name_phase) {
        case NAME_PHASE_CANCEL:
            name_phase = NAME_PHASE_IDLE;
            return NAME_STEP_CANCELLED;
        case NAME_PHASE_ACCEPT:
            name_phase = NAME_PHASE_IDLE;
            return NAME_STEP_ACCEPTED;
        }
        return NAME_STEP_EDITING;
    }
    return NAME_STEP_IDLE;
}

/* Draw the sound prompt for the current step: the value and its controls. */
static void name_sound_draw(void) {
    u_long *sound_data;
    u16 step;
    s32 value;
    s16 quotient;
    s32 remainder;
    sound_data = &current_frame->otTags[0][24];
    name_sound_draw_frame();
    step = name_sound_state;
    if (step == NAME_SOUND_CONFIRM) {
        name_sound_handle_input();
        return;
    }
    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(3, 0x3A9);
    step = name_sound_state;
    if (step == NAME_SOUND_PICK_ICON) {
        ui_draw_icon_0b_1(sound_data, name_sound_value * 64 + 36, 66);
    } else if (step == NAME_SOUND_PICK_DAY) {
        value = name_sound_value;
        quotient = value / BIRTHDAY_COLS;
        remainder = value - quotient * BIRTHDAY_COLS;
        ui_draw_icon_0b_1(sound_data, remainder * 39 + 36, quotient * 14 + 94);
    }
    name_title_build_display();
    name_sound_update();
}

/* Draw the window frame and its localized caption; from PICK_DAY on also draw the
 * chosen icon, and from CONFIRM on the chosen day. */
static void name_sound_draw_frame(void) {
    u_long *base;
    u_long *window;

    base = current_frame->otTags[0];
    window = &base[25];
    window_frame_draw_at(&base[26], 1, 171, 32, 133, 18);
    text_draw_plain(window, LOCALIZED(text_bank.messages.birthday_label), 173, 35);
    if (name_sound_state >= NAME_SOUND_PICK_DAY) {
        ui_draw_icon_2b_sub(window, 253, 35, name_sound_seq->icon);
    }
    if (name_sound_state >= NAME_SOUND_CONFIRM) {
        text_draw_number(name_sound_seq->day + 1, 2, window, 277, 35);
    }
}

/* Build the birthday title: the four icons, and the thirty day numbers on their grid. */
static void name_title_build_display(void) {
    s32 index;
    s32 next_index;
    s32 cell_x;
    s32 y_pos;
    s32 x_pos;
    s32 terminator;
    u_long *base;
    u_long *display;
    s32 quotient;
    s32 value;
    s32 remainder;

    base = current_frame->otTags[0];
    display = &base[25];
    window_frame_draw_at(&base[26], 1, 20, 56, 279, 103);
    index = 0;
    x_pos = 0x2A0000;
    y_pos = 0x1E0000;
    terminator = -1;
    do {
        ui_draw_icon_2b_sub(display, y_pos >> 16, 60, index);
        *text_write_season(index, name_display_text) = terminator;
        text_draw_plain(display, name_display_text, x_pos >> 16, 60);
        x_pos += 0x400000;
        index++;
        y_pos += 0x400000;
    } while (index < BIRTHDAY_ICON_COUNT);
    index = 0;
    do {
        next_index = index + 1;
        quotient = index / BIRTHDAY_COLS;
        value = quotient * 14 + 88;
        remainder = index % BIRTHDAY_COLS;
        cell_x = remainder * 39 + 30;
        text_draw_number(next_index, 2, display, cell_x, value);
        index = next_index;
    } while (index < BIRTHDAY_DAY_COUNT);
}

/* Handle the icon and day pickers' pad input: cross commits the pick, circle goes
 * back, and the d-pad moves the pick. */
static void name_sound_update(void) {
    void (*reset)(void);
    void (*start)(void);
    s32 (*update)(s16);
    s32 state;
    s16 previous;
    s16 *saved_step;

    if (menu_phase_is_result() == 0) {
        return;
    }
    if (name_sound_state == NAME_SOUND_PICK_ICON) {
        reset = name_sound_commit_value;
        start = name_sound_set_done;
        update = name_sound_move_position;
    } else if (name_sound_state == NAME_SOUND_PICK_DAY) {
        reset = name_sound_commit_next;
        start = name_sound_load_value;
        update = name_sound_move_index;
    } else {
        return;
    }
    state = input_pad1->pressed;
    if (state == PAD_CROSS) {
        reset();
        sound_play(SOUND_MENU_CONFIRM);
        return;
    }
    if (state == PAD_CIRCLE) {
        start();
        sound_play(SOUND_MENU_CANCEL);
        return;
    }
    saved_step = &name_sound_value;
    previous = *saved_step;
    *saved_step = update(previous);
    if (*saved_step != previous) {
        sound_play(SOUND_MENU_CURSOR);
    }
}

static void name_sound_commit_value(void) {
    name_sound_seq->icon = name_sound_value;
    name_sound_value = 0;
    name_sound_state = NAME_SOUND_PICK_DAY;
}

static void name_sound_commit_next(void) {
    name_sound_seq->day = name_sound_value;
    name_sound_value = 0;
    name_prompt_row = 0;
    name_sound_state = NAME_SOUND_CONFIRM;
    menu_begin_close();
}

static void name_sound_set_done(void) {
    name_sound_state = NAME_SOUND_BACK;
    menu_begin_close();
}

static void name_sound_load_value(void) {
    name_sound_value = name_sound_seq->icon;
    name_sound_state = NAME_SOUND_PICK_ICON;
}

/* Move a 0..3 menu position one step left or right by a d-pad press, wrapping at both ends. */
static s32 name_sound_move_position(s16 position) {
    s32 mode;
    InputRecord *pad;

    pad = input_pad1;
    mode = pad->triggered;
    if (mode == PAD_RIGHT) {
        position = position + 1;
        if (position >= BIRTHDAY_ICON_COUNT) {
            position = 0;
        }
    } else if (mode == PAD_LEFT) {
        position = position - 1;
        if (position < 0) {
            position = BIRTHDAY_ICON_COUNT - 1;
        }
    }
    return position;
}

/* Move a day index by a d-pad press over the calendar: 7 columns, 30 days, wrapping at the edges. */
static s32 name_sound_move_index(s16 index) {
    s16 value;
    u32 flags;

    value = index;
    flags = input_pad1->triggered & PAD_ARROWS_MASK;
    switch (flags) {
    case PAD_UP:
        value -= 7;
        if (value < 0) {
            value += 28;
            if (value < 23) {
                value += 7;
            }
        }
        break;
    case PAD_DOWN:
        value += 7;
        if (value >= BIRTHDAY_DAY_COUNT) {
            value -= 28;
            if (value >= 7) {
                value -= 7;
            }
        }
        break;
    case PAD_LEFT:
        value -= 1;
        if (value < 0) {
            value += BIRTHDAY_DAY_COUNT;
        }
        break;
    case PAD_RIGHT:
        value += 1;
        if (value >= BIRTHDAY_DAY_COUNT) {
            value -= BIRTHDAY_DAY_COUNT;
        }
        break;
    }
    return value;
}

/* Day prompt input: cross finishes on row 0 and returns to the day picker otherwise, circle goes back, up and down move the row. */
static void name_sound_handle_input(void) {
    InputRecord *pad;
    s32 state;
    name_prompt_draw(0x3A7, name_prompt_row);
    if (menu_phase_is_result() != 0) {
        pad = input_pad1;
        state = pad->pressed;
        if (state == PAD_CROSS) {
            if (name_prompt_row == 0) {
                sound_play(SOUND_MENU_CONFIRM);
                name_sound_state = NAME_SOUND_DONE;
                menu_begin_close();
                return;
            }
        }
        if ((state == PAD_CROSS) || (state == PAD_CIRCLE)) {
            sound_play(SOUND_MENU_CANCEL);
            name_sound_state = NAME_SOUND_PICK_DAY;
            name_sound_value = name_sound_seq->day;
            menu_begin_close();
            return;
        }
        if ((pad->triggered & (PAD_UP | PAD_DOWN)) != 0) {
            sound_play(SOUND_MENU_CURSOR);
            name_prompt_row = (name_prompt_row + 1) & 1;
        }
    }
}

/* Run one frame of the letter grid: the caption, the title and cursor marker, then the
 * key input, or the confirm prompt once the name is being confirmed. */
static void name_grid_step(void) {
    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(3, 0x3A8);
    name_title_prepare(name_title_mode);
    if (name_phase == NAME_PHASE_CONFIRM) {
        name_entry_confirm_input();
        return;
    }
    name_marker_draw(name_cursor_cell);
    name_grid_draw_caption();
    name_entry_key_input();
}

/* Clear the name buffer to its end: NAME_END in all NAME_CHARS + 1 slots. */
static void name_buffer_clear(void) {
    unsigned int terminator;
    u16 *ptr;
    signed int i;

    terminator = NAME_END;
    ptr = name_buffer + NAME_CHARS;
    i = NAME_CHARS;
    do {
        *ptr = terminator;
        i--;
        ptr--;
    } while (i >= 0);
}

/* Fill the title buffer with spaces and its end, build the name or label into it, then
 * draw the title window and the text. */
static void name_title_prepare(s32 from_name) {
    s32 count;
    void (*setup)(u16 *dest);

    count = 0;
    do {
        name_display_text[count] = TEXT_CODE_SPACE;
        count++;
    } while (count < 16U);
    name_display_text[15] = 0xFFFF;
    if (from_name != 0) {
        setup = name_title_build_name;
    } else {
        setup = name_title_build_label;
    }
    setup(name_display_text);
    window_frame_draw_at(&current_frame->otTags[0][26], 1, 171, 32, 133, 18);
    text_draw_plain(&current_frame->otTags[0][25], name_display_text, 173, 35);
}

/* Copy the current command list (at most eight words, 0xFFFF-terminated) into
 * dest, replacing its terminator with 0x75 when FarmState.frameCount is a multiple
 * of 15 and 0xFFD otherwise; then append the localized list from dest[NAME_LENGTH]
 * on, without its terminator. */
static void name_title_build_name(u16 *dest) {
    u16 *src;
    u16 *text;
    s32 i;
    u16 word;

    src = name_buffer;
    i = 0;
    do {
        word = src[i];
        if (word == 0xFFFF) {
            dest[i] = (game_state.frameCount % 15 != 0) ? TEXT_CODE_SPACE : 0x75;
            break;
        }
        dest[i] = word;
        i++;
    } while (i < NAME_CHARS);

    text = LOCALIZED(text_bank.messages.farm_suffix);
    dest += NAME_LENGTH;
    if (*text != 0xFFFF) {
        i = 0;
        do {
            dest[i] = text[i];
            i++;
        } while (text[i] != 0xFFFF);
    }
}

/* Copy the localized name label into dest without its terminator, skip
 * one slot, then the current command list (at most eight words,
 * 0xFFFF-terminated), replacing its terminator with 0x75 when FarmState.frameCount
 * is a multiple of 15 and 0xFFD otherwise. The mirror of name_title_build_name. */
static void name_title_build_label(u16 *dest) {
    u16 *src;
    u16 *text;
    s32 i;
    u16 word;

    text = LOCALIZED(text_bank.messages.name_label);
    do {
        i = 0;
        if (text[0] != 0xFFFF) {
        copy:
            *dest++ = text[i++];
            if (text[i] != 0xFFFF) {
                goto copy;
            }
        }
    } while (0);
    dest++;
    if (dest == 0) word = 0;

    src = name_buffer;
    i = 0;
    do {
        word = src[i];
        if (word == 0xFFFF) {
            dest[i] = (game_state.frameCount % 15 != 0) ? TEXT_CODE_SPACE : 0x75;
            break;
        }
        dest[i] = word;
        i++;
    } while (i < NAME_CHARS);
}

/* Draw the window frame, then one of two localized texts in it depending on
 * name_shift. */
static void name_grid_draw_caption(void) {
    u16 *text;

    window_frame_draw_at(&current_frame->otTags[0][26], 1, 45, 64, 230, 86);
    if (name_shift == 0) {
        text = LOCALIZED(text_bank.messages.letter_grid);
    } else {
        text = LOCALIZED(text_bank.messages.symbol_grid);
    }
    text_set_spacing(1, 2);
    text_draw_plain(&current_frame->otTags[0][25], text, 47, 66);
    text_set_spacing(0, 0);
}

/* Handle the grid's pad input: start asks to confirm the name, circle erases a letter or
 * cancels, cross types the cell under the cursor (or shifts at NAME_CELL_SHIFT, or asks
 * to confirm at NAME_CELL_END), L1/R1 shift, and the d-pad moves the cursor. */
static void name_entry_key_input(void) {
    s16 sound;
    u16 cell;

    sound = -1;
    if (name_phase == NAME_PHASE_CONFIRM) {
        return;
    }
    if (menu_phase_is_result() == 0) {
        return;
    }
    switch (input_pad1->pressed) {
    case PAD_START:
        if (name_length == 0 || name_dirty == 0) {
            sound = SOUND_MENU_REFUSE;
        } else {
            menu_begin_close();
            sound = SOUND_MENU_CONFIRM;
            name_prompt_toggle = 0;
            name_phase = NAME_PHASE_CONFIRM;
        }
        break;
    case PAD_CIRCLE:
        if (name_length != 0) {
            sound = SOUND_MENU_CANCEL;
            name_length--;
            name_buffer[name_length] = NAME_END;
        } else if (name_cancel_allowed != 1) {
            sound = SOUND_MENU_REFUSE;
        } else {
            menu_begin_close();
            sound = SOUND_MENU_CANCEL;
            name_phase = NAME_PHASE_CANCEL;
        }
        break;
    case PAD_CROSS:
        if (name_cursor_cell == NAME_CELL_SHIFT) {
            name_shift = (name_shift + 1) & 1;
            sound = SOUND_MENU_CURSOR;
            break;
        }
        if (name_cursor_cell != NAME_CELL_END) {
            sound = SOUND_MENU_CONFIRM;
        } else {
            if (name_length == 0 || name_dirty == 0) {
                sound = SOUND_MENU_REFUSE;
            } else {
                menu_begin_close();
                sound = SOUND_MENU_CONFIRM;
                name_prompt_toggle = 0;
                name_buffer[name_length + 1] = NAME_END;
                name_phase = NAME_PHASE_CONFIRM;
            }
            break;
        }
        sound = SOUND_MENU_CONFIRM;
        if (name_length < NAME_CHARS) {
            if (name_cursor_cell == NAME_CELL_SPACE) {
                name_buffer[name_length] = TEXT_CODE_SPACE;
            } else {
                name_buffer[name_length] = name_key_char(name_cursor_cell, name_shift);
                name_dirty = 1;
            }
            name_length++;
        }
        break;
    case PAD_L1 | PAD_L2:
    case PAD_R1 | PAD_R2:
        name_shift = (name_shift + 1) & 1;
        sound = SOUND_MENU_CURSOR;
        break;
    default:
        cell = name_cursor_cell;
        name_cursor_cell = name_cursor_move(cell, input_pad1->triggered, name_shift);
        if (cell != name_cursor_cell) {
            sound = SOUND_MENU_CURSOR;
        }
        break;
    }
    if (sound >= 0) {
        sound_play(sound);
    }
}

/* Handle the confirm prompt's pad input. */
static void name_entry_confirm_input(void) {
    InputRecord *pad;
    s32 state;

    name_prompt_draw(0x3A6, name_prompt_toggle);
    if (menu_phase_is_result() == 0) return;
    pad = input_pad1;
    state = pad->pressed;
    if (state == PAD_CROSS) {
        if (name_prompt_toggle == 0) {
            menu_begin_close();
            sound_play(SOUND_MENU_CONFIRM);
            name_phase = NAME_PHASE_ACCEPT;
            return;
        }
        menu_begin_close();
        sound_play(SOUND_MENU_CANCEL);
        name_phase = NAME_PHASE_GRID;
        return;
    }
    if (state == PAD_CIRCLE) {
        menu_begin_close();
        sound_play(SOUND_MENU_CANCEL);
        name_phase = NAME_PHASE_GRID;
        return;
    }
    if ((pad->triggered & (PAD_UP | PAD_DOWN)) != 0) {
        sound_play(SOUND_MENU_CURSOR);
        name_prompt_toggle = (name_prompt_toggle + 1) & 1;
    }
}

/* Draw the yes/no prompt with message, its window, and the highlight on row. */
static void name_prompt_draw(s32 message, u16 row) {
    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(3, message);
    if (menu_phase_is_result() != 0) {
        text_draw_plain(&current_frame->otTags[0][22], LOCALIZED(text_bank.messages.prompt_yes_no), 147, 78);
        window_frame_draw_at(&current_frame->otTags[0][23], 1, 145, 76, 40, 30);
        ui_draw_icon_0b_1(&current_frame->otTags[0][21], 153, row * 12 + 84);
    }
}

static u8 name_grid0_row_last_cols[8] = {
    0x0D, 0x0D, 0x0C, 0x0A, 0x09, 0x0D, 0x00, 0x00,
};

static u8 name_grid1_row_last_cols[8] = {
    0x0D, 0x0D, 0x07, 0x00, 0x00, 0x0D, 0x00, 0x00,
};

/* Per cursor grid (name_cursor_move): the last column of each row, and the last
 * row of each column. */
static u8 *name_grid_row_last_cols[] = {name_grid0_row_last_cols, name_grid1_row_last_cols};

static u8 name_grid0_col_last_rows[16] = {
    0x05, 0x04, 0x04, 0x04, 0x04, 0x05, 0x04, 0x04, 0x04, 0x04, 0x03, 0x02, 0x02, 0x05,
    0x00, 0x00,
};

static u8 name_grid1_col_last_rows[16] = {
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x05,
    0x00, 0x00,
};

static u8 *name_grid_col_last_rows[] = {name_grid0_col_last_rows, name_grid1_col_last_rows};

/* The fixup applied after a horizontal move, per grid. */
static CursorFixup name_cursor_wrap_fixups[] = {name_cursor_wrap_grid0, name_cursor_wrap_grid1};

/* Move a cursor over a 14-column grid (`cell` = row * 14 + col) one step in
 * the direction of the d-pad bit `button`, wrapping at the edges of grid
 * `grid`, and return the new cell. Anything else leaves it where it is. */
static u16 name_cursor_move(u16 cell, u16 button, u32 grid) {
    s32 col;
    s32 row;
    s32 *axis;
    s32 last;
    s32 dir;
    s32 forward;
    CursorFixup fixup;

    /* bit 0: horizontal, bit 1: towards the higher index */
    if (button == PAD_UP) {
        dir = 0;
    } else if (button == PAD_DOWN) {
        dir = 2;
    } else if (button == PAD_LEFT) {
        dir = 1;
    } else if (button == PAD_RIGHT) {
        dir = 3;
    } else {
        return cell;
    }
    if (grid >= NAME_GRID_COUNT) {
        return cell;
    }

    forward = dir & 2;
    col = cell % NAME_GRID_COLS;
    row = cell / NAME_GRID_COLS;
    if (dir & 1) {
        last = name_grid_row_last_cols[grid][row];
        fixup = name_cursor_wrap_fixups[grid];
        axis = &col;
    } else {
        axis = &row;
        last = name_grid_col_last_rows[grid][col];
        fixup = name_cursor_wrap_vertical;
    }

    *axis += forward ? 1 : -1;
    if (*axis < 0) {
        *axis = last;
    }
    if (*axis > last) {
        *axis = 0;
    }
    *axis = fixup(col, row, forward);
    return col + row * NAME_GRID_COLS;
}

static signed int name_cursor_wrap_grid0(signed int col, signed int row, signed int forward) {
    if (row == 5) {
        if ((unsigned int)(col - 1) < 4U) return forward ? 5 : 0;
        if ((unsigned int)(col - 6) < 7U) return forward ? 13 : 5;
    }
    return col;
}

static int name_cursor_wrap_vertical(int col, int row, int forward) {
    if (col == 13 && (unsigned int)(row - 2) < 3) {
        return forward ? 5 : 1;
    }
    return row;
}

static signed int name_cursor_wrap_grid1(signed int col, signed int row, signed int forward) {
    if (row < 2) {
        if ((unsigned int)(col - 10) < 3U) {
            if (forward == 0) {
                return 9;
            }
            return 13;
        }
    }
    if (row == 5) {
        return 13;
    }
    return col;
}

/* Draw the cursor marker on a cell. The key columns are spaced unevenly on screen, so
 * the column moves one step further at 5, 10 and 13. */
static void name_marker_draw(u16 cell) {
    u16 row;
    s32 col;
    s32 offset;

    row = cell / NAME_GRID_COLS;
    col = cell % NAME_GRID_COLS;
    offset = col >= 5;
    if (col >= 10) {
        offset += 1;
    }
    if (col >= 13) {
        offset += 2;
    }
    col += offset;
    ui_draw_icon_0b_1(&current_frame->otTags[0][24], col * 9 + 50, row * 14 + 72);
}

static NameKeyCharHandler name_key_char_handlers[] = {name_key_cell_letters, name_key_char_symbols};

/* The character at keyboard cell `cell` (row * 14 + col) of grid `grid`;
 * 0xFFFF for a grid that does not exist. */
static u16 name_key_char(u16 cell, u32 grid) {
    u32 col;
    u32 row;

    if (grid >= NAME_GRID_COUNT) return 0xFFFF;
    row = cell / 14U;
    col = cell % 14U;
    return name_key_char_handlers[grid](col & 0xFFFF, row & 0xFFFF);
}

/* The letter at a cell of the letter grid: columns 0..4, 5..9 and 10..13 hold 5, 5 and 3
 * letters a row from 0x00, 0x20 and 0x40. */
static u16 name_key_cell_letters(s32 col, s32 row) {
    s32 base;
    s32 mul;

    if (col < 5) {
        base = 0;
        mul = 5;
    } else if (col < 10) {
        base = 0x20;
        col -= 5;
        mul = 5;
    } else {
        base = 0x40;
        col -= 10;
        mul = 3;
    }
    return (base + (row * mul + col)) & 0xFFFF;
}

static u16 name_symbol_chars[28] = {
    0x0063, 0x0062, 0x0064, 0x006C, 0x008D, 0x0083, 0x0084, 0x0085, 0x0082, 0x0086, 0x0068,
    0x0069, 0x008E, 0x006A, 0x006B, 0x006D, 0x006E, 0x0070, 0x006F, 0x0077, 0x0076, 0x0074,
    0x0071, 0x0066, 0x0067, 0x0080, 0x0081, 0x008B,
};

static unsigned short name_key_char_symbols(signed int col, signed int row) {
    return name_symbol_chars[row * 10 + col];
}
