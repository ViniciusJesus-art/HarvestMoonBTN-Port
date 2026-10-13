/* The farm's option menu: the settings (the sound, the status and the menu's own
 * pages) and the cursors that drive them. */

#include "types.h"
#include "farm_main.h"
#include "farm_state.h"
#include "main.h"
#include "menu_window.h"
#include "options_menu.h"
#include "pad.h"
#include "pad_vibration.h"
#include "screen_fade.h"
#include "sound.h"
#include "sound_request.h"
#include "status_pages.h"
#include "text.h"
#include "text_bank.h"
#include "ui_icons.h"

/* Draw one option label: dimmed unless its option is set to it. */
#define OPTION_LABEL(text, x, y, dimmed) \
    text_draw_semi(window, LOCALIZED(text), x, y, dimmed)
static void options_menu_tick(void);
static void options_main_menu_tick(void);
static void options_main_menu_prompt(s16 row, s16 choice);
static void options_settings_tick(void);
static void options_settings_prompt(s16 raw_first, s16 raw_second);

/* The settings the menu edits. They are copied from the saved options on entry and
 * back on cancel. */
static FarmSettings options_settings;

/* The menu's state: the page on screen (main menu, settings), its closing step, or
 * the opening and closing of the window. */
enum OptionsMenuState {
    OPTIONS_CLOSED = 0,
    OPTIONS_OPENING = 1,
    OPTIONS_MAIN = 0x10,
    OPTIONS_MAIN_CLOSING = 0x11,
    OPTIONS_SETTINGS = 0x20,
    OPTIONS_SETTINGS_CLOSING = 0x21,
    OPTIONS_CLOSING = 0x30
};

/* The rows of the settings menu. */
enum OptionsSettingsRow {
    OPTIONS_ROW_TYPE = 0,
    OPTIONS_ROW_VIBRATION = 1,
    OPTIONS_ROW_SOUND = 2,
    OPTIONS_ROW_STATUS_LATCH = 3,
};

/* The ot layers the window draws in: the page's cursor, text and frame, and the same three
 * for the opening page. */
enum OptionsLayer {
    OPTIONS_LAYER_CURSOR = 20,
    OPTIONS_LAYER_TEXT = 21,
    OPTIONS_LAYER_FRAME = 22,
    OPTIONS_LAYER_START_CURSOR = 23,
    OPTIONS_LAYER_START_TEXT = 24,
    OPTIONS_LAYER_START_FRAME = 25,
};

/* The first message of the main menu's descriptions (one per entry: column 0's five rows,
 * then column 1's), and the bases of the settings' type and row messages. */
#define OPTIONS_MSG_MAIN_BASE 0x367
#define OPTIONS_MSG_TYPE_BASE 0x37B
#define OPTIONS_MSG_ROW_BASE 0x37E

static enum OptionsMenuState options_menu_state = OPTIONS_CLOSED;

static s16 options_main_index = 0;

static s16 options_column = 0;

static s16 options_row = 0;

static s16 options_settings_column = 0;

static s16 options_settings_row = 0;

static void options_state_reset(void) {
    options_menu_state = OPTIONS_CLOSED;
    options_settings_row = OPTIONS_ROW_TYPE;
    options_settings_column = 0;
    options_row = 0;
    options_column = 0;
    options_main_index = 0;
}

/* Open the menu from closed: copy the saved options into options_settings and start the
 * opening page. */
void options_menu_open(void) {
    FarmSettings *destination;
    FarmState *base;

    if (options_menu_state == OPTIONS_CLOSED) {
        options_state_reset();
        options_menu_state = OPTIONS_OPENING;
        destination = &options_settings;
        base = &game_state;
        *destination = base->world.options;
        sound_play(3);
    }
}

/* Close the menu: fill the screen with a dark quad and tick the menu once more. */
void options_menu_close(void) {
    if (options_menu_state != OPTIONS_CLOSED) {
        screen_fill_quad(0xA000, 0xA000, 0xA000);
        text_set_spacing(0, 2);
        options_menu_tick();
        text_set_spacing(0, 0);
    }
}

/* The menu's page handlers, indexed by state / 16 - 1. */
static void (*options_state_handlers[2])(void) = {options_main_menu_tick, options_settings_tick};

/* Per-frame tick of the window. Start closes it from any page (and the window closes
 * once its menu is gone). On the opening page the frame and the prompt are drawn, circle
 * closes and cross opens the page under the cursor (the buttons page, or the settings).
 * Any other state runs its page's tick. */
static void options_menu_tick(void) {
    s32 result;
    u32 mode;
    s32 flags;
    u16 value;
    s32 index;
    s32 handler_index;
    void (**handler)(void);
    FarmState *base;
    if (options_menu_state == OPTIONS_CLOSING) {
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            base = &game_state;
            base->world.options = options_settings;
            menu_reset();
            options_menu_state = OPTIONS_CLOSED;
            farm_unpause();
            game_state.globalFlags |= GLOBAL_FLAG_TRANSITION_END;
            options_apply_settings();
        }
    } else {
        result = menu_phase_is_result();
        if ((result == 1) && (input_pad1->pressed & PAD_START)) {
            options_menu_state = OPTIONS_CLOSING;
            sound_play(3);
            menu_begin_close();
            return;
        }
        mode = options_menu_state;
        if (mode != OPTIONS_OPENING) {
            handler = options_state_handlers;
            handler_index = (mode >> 4) - 1;
            handler[handler_index]();
            return;
        }
        window_frame_draw_at(&current_frame->otTags[0][OPTIONS_LAYER_START_FRAME], 1, 100, 74, 130, 34);
        text_draw_plain(&current_frame->otTags[0][OPTIONS_LAYER_START_TEXT], LOCALIZED(text_bank.messages.how_to_operate_configuration), 112, 78);
        menu_set_sound_enabled(0);
        text_set_input(0);
        menu_request_open(3, 0x3B0);
        index = options_main_index;
        ui_draw_icon_0b_1(&current_frame->otTags[0][OPTIONS_LAYER_START_CURSOR], 108, (index * 14) + 84);
        if (result == mode) {
            flags = input_pad1->pressed;
            if (flags & PAD_CIRCLE) {
                options_menu_state = OPTIONS_CLOSING;
                sound_play(3);
                menu_begin_close();
                return;
            }
            if (flags & PAD_CROSS) {
                menu_begin_close();
                sound_play(0);
                options_menu_state = (options_main_index + 1) * 0x10;
                return;
            }
            flags = input_pad1->triggered;
            index = flags & PAD_DOWN;
            if (index) {
                sound_play(5);
                options_main_index = options_main_index + 1;
            } else if (flags & PAD_UP) {
                options_main_index = options_main_index + 1;
                sound_play(5);
            }
            value = options_main_index;
            options_main_index =
                (value - ((((s32)(((s16)value) + (((u32)(value << 16)) >> 31))) >> 1) * 2));
        }
    }
}

/* The buttons page: a two-column menu, the cursor a column (0 or 1) and a row. Column 0
 * has five rows and column 1 six. While the page is open and the phase is ready, left and
 * right swap the column, up and down step the row with wrap-around, cross closes the page
 * (state 0x11, which then returns to the opening page), and circle backs out to the
 * opening page. Then draw the frame, the text and the cursor. */
static void options_main_menu_tick(void)
{
    s32 rows;
    s32 row;
    s32 next;
    s16 *cursor;

    window_frame_draw_at(&current_frame->otTags[0][OPTIONS_LAYER_FRAME], 1, 20, 55, 280, 105);
    text_set_spacing(0, 2);
    text_draw_plain(&current_frame->otTags[0][OPTIONS_LAYER_TEXT], LOCALIZED(text_bank.messages.how_to_operate_buttons), 32, 59);
    text_set_spacing(0, 0);
    if (options_menu_state == OPTIONS_MAIN && menu_phase_is_result() == 1) {
        if (input_pad1->triggered & (PAD_RIGHT | PAD_LEFT)) {
            options_column = (options_column + 1) % 2;
            sound_play(5);
        }
        rows = 6;
        if (options_column == 0) {
            rows = 5;
            if (options_row == 5) {
                options_row--;
            }
        }
        if (input_pad1->triggered & (PAD_UP | PAD_DOWN)) {
            cursor = &options_row;
            row = *cursor;
            if (!(input_pad1->triggered & PAD_DOWN)) {
                next = row - 1;
                next += rows;
            } else {
                next = row + 1;
            }
            *cursor = next;
            sound_play(5);
        }
        options_row = options_row % rows;
        if (input_pad1->pressed & PAD_CIRCLE) {
            sound_play(1);
            menu_begin_close();
            options_menu_state = OPTIONS_OPENING;
        } else if (input_pad1->pressed & PAD_CROSS) {
            sound_play(0);
            if (options_menu_state == OPTIONS_MAIN) {
                menu_begin_close();
                options_menu_state = OPTIONS_MAIN_CLOSING;
            } else {
                options_menu_state = OPTIONS_MAIN;
            }
        }
    }
    ui_draw_icon_0b_1(&current_frame->otTags[0][OPTIONS_LAYER_CURSOR], options_column * 150 + 28,
                  (options_row + 1) * 14 + 65);
    options_main_menu_prompt(options_column, options_row);
}

/* Show the main menu's prompt for the cursor: while the page is closing, the description
 * of the entry at `column` and `row` (column 0 has five rows, so column 1 starts at +5),
 * then the page's own prompt while it is open. */
static void options_main_menu_prompt(s16 column, s16 row) {
    s32 scaled;

    if (options_menu_state == OPTIONS_MAIN_CLOSING) {
        scaled = column * 5;
        scaled += OPTIONS_MSG_MAIN_BASE;
        menu_request_open(3, row + scaled);
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            menu_begin_close();
            options_menu_state = OPTIONS_MAIN;
        }
    }
    if (options_menu_state == OPTIONS_MAIN) {
        menu_set_sound_enabled(0);
        text_set_input(0);
        menu_request_open(3, 0x3B2);
    }
}

/* The settings menu's handlers for row 2, by column (options_settings_tick). */
static void (*options_row2_handlers[2])(void) = {sound_set_stereo, sound_set_mono};

/* Row 3's handlers, by column: the first sets the status latch, the second resets it. */
static void (*options_row3_handlers[2])(void) = {status_latch_set, status_latch_reset};

/* The settings menu: the cursor is a row (0-3) and a column (three choices in row 0,
 * two in the others). While the menu is in state 0x20 and the phase is ready, up and down
 * step the row, left and right the column, cross applies the column to the row's option
 * (rows 2 and 3 also call that column's handler), triangle leaves the page (state 0x21,
 * which returns to the prompt) and circle backs out to the opening page. Then draw the
 * cursor and the nine labels. */
static void options_settings_tick(void)
{
    s32 ready;
    s32 count;
    s32 columns;
    s32 column;
    s32 flags;
    u_long *window;
    s16 *rowCursor;
    s32 row;
    s16 *columnCursor;
    s32 col;
    s32 next;

    window_frame_draw_at(&current_frame->otTags[0][OPTIONS_LAYER_FRAME], 1, 58, 30, 202, 132);
    text_set_spacing(0, 2);
    text_draw_plain(&current_frame->otTags[0][OPTIONS_LAYER_TEXT], LOCALIZED(text_bank.messages.configuration_screen), 62, 34);
    text_set_spacing(0, 0);
    if (options_menu_state == OPTIONS_SETTINGS && (ready = menu_phase_is_result()) == 1) {
        if (input_pad1->triggered & (PAD_UP | PAD_DOWN)) {
            if (options_settings_column >= 2 && options_settings_row == OPTIONS_ROW_TYPE) {
                options_settings_column = ready;
            }
            rowCursor = &options_settings_row;
            row = *rowCursor;
            count = input_pad1->triggered & PAD_DOWN;
            if (count) {
                row += 1;
            } else {
                row += 3;
            }
            *rowCursor = row;
            sound_play(5);
        }
        options_settings_row = options_settings_row % 4;
        count = 2;
        if (options_settings_row == OPTIONS_ROW_TYPE) {
            count = 3;
        }
        flags = input_pad1->triggered;
        columns = count;
        if (flags & (PAD_LEFT | PAD_RIGHT)) {
            columnCursor = &options_settings_column;
            col = *columnCursor;
            if (!(input_pad1->triggered & PAD_RIGHT)) {
                next = col - 1;
                next += columns;
            } else {
                next = col + 1;
            }
            *columnCursor = next;
            sound_play(5);
        }
        column = options_settings_column % columns;
        options_settings_column = column;
        flags = input_pad1->pressed;
        if (flags & PAD_CIRCLE) {
            menu_begin_close();
            sound_play(1);
            options_menu_state = OPTIONS_OPENING;
        } else if (flags & PAD_CROSS) {
            switch (options_settings_row) {
            case OPTIONS_ROW_TYPE:
                options_settings.messageType = column;
                break;
            case OPTIONS_ROW_VIBRATION:
                if (column == 0 && options_settings.vibration == 0) {
                    pad_set_started(1);
                    pad_vibration_start(0, 0);
                }
                options_settings.vibration = options_settings_column != 1;
                break;
            case OPTIONS_ROW_SOUND:
                options_row2_handlers[column]();
                options_settings.stereo = options_settings_column != 1;
                break;
            case OPTIONS_ROW_STATUS_LATCH:
                options_row3_handlers[column]();
                options_settings.statusLatch = options_settings_column != 1;
                break;
            }
            sound_play(0);
        } else if (flags & PAD_TRIANGLE) {
            sound_play(0);
            if (options_menu_state == OPTIONS_SETTINGS) {
                menu_begin_close();
                options_menu_state = OPTIONS_SETTINGS_CLOSING;
            } else {
                options_menu_state = OPTIONS_SETTINGS;
            }
        }
    }
    ui_draw_icon_0b_1(&current_frame->otTags[0][OPTIONS_LAYER_CURSOR], options_settings_column * 65 + 66,
                  (options_settings_row + 1) * 28 + 40);
    window = &current_frame->otTags[0][OPTIONS_LAYER_TEXT];
    OPTION_LABEL(text_bank.messages.type_1, 70, 62, options_settings.messageType != 0);
    OPTION_LABEL(text_bank.messages.type_2, 135, 62, options_settings.messageType != 1);
    OPTION_LABEL(text_bank.messages.type_3, 200, 62, options_settings.messageType != 2);
    OPTION_LABEL(text_bank.messages.on, 70, 90, options_settings.vibration != 1);
    OPTION_LABEL(text_bank.messages.off, 135, 90, options_settings.vibration != 0);
    OPTION_LABEL(text_bank.messages.stereo, 70, 118, options_settings.stereo != 1);
    OPTION_LABEL(text_bank.messages.mono, 135, 118, options_settings.stereo != 0);
    OPTION_LABEL(text_bank.messages.yes, 70, 146, options_settings.statusLatch != 1);
    OPTION_LABEL(text_bank.messages.no, 135, 146, options_settings.statusLatch != 0);
    options_settings_prompt(options_settings_column, options_settings_row);
}

static void options_settings_prompt(s16 column, s16 row) {
    s32 request;

    if (options_menu_state == OPTIONS_SETTINGS_CLOSING) {
        if (row == 0) {
            request = column + OPTIONS_MSG_TYPE_BASE;
        } else {
            request = row + OPTIONS_MSG_ROW_BASE;
        }
        menu_request_open(3, request);
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            menu_begin_close();
            options_menu_state = OPTIONS_SETTINGS;
        }
    }
    if (options_menu_state == OPTIONS_SETTINGS) {
        menu_set_sound_enabled(0);
        text_set_input(0);
        menu_request_open(3, 0x3B3);
    }
}

/* Apply the saved options: vibration, stereo or mono sound and the status latch. */
void options_apply_settings(void) {
    if (game_state.world.options.vibration != 0) {
        pad_set_started(1);
    } else {
        pad_set_started(0);
    }
    if (game_state.world.options.stereo != 0) {
        sound_set_stereo();
    } else {
        sound_set_mono();
    }
    if (game_state.world.options.statusLatch != 0) {
        status_latch_set();
    } else {
        status_latch_reset();
    }
}
