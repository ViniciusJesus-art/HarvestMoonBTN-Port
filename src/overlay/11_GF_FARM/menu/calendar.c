/* The farm calendar: the month grid, its cursor, the day marks and the event messages. */

#include "types.h"
#include "anim.h"
#include "calendar.h"
#include "clock.h"
#include "farm_main.h"
#include "farm_state.h"
#include "main.h"
#include "menu_window.h"
#include "menus.h"
#include "pad.h"
#include "screen_fade.h"
#include "sound_request.h"
#include "text.h"
#include "text_bank.h"
#include "ui_icons.h"

/* A 4-byte display entry: two byte selectors and a signed x nudge. */
typedef struct {
    u8 anim;     /* animation range */
    u8 frame;    /* animation frame */
    u16 offsetX; /* added to the projected x */
} FarmDisplayEntry;

static void farm_calendar_tick(void);
static void farm_calendar_handle_buttons(void);
static void farm_calendar_move_cursor(void);
static void farm_calendar_build_geometry(FarmDate *date);
static void farm_calendar_mark_events(u_long *ot, s16 season, s16 offset);
static void farm_calendar_mark_wedding(u_long *ot, s16 year, s16 season, s16 day);
static void farm_calendar_select_message(u_long *unused0, u_long *unused1, s16 year, s16 season, s16 day);
static void farm_calendar_draw_cursor(u_long *ot, s16 day, s16 offset);
static void farm_calendar_draw_today(u_long *ot, s16 x, s16 y);
static void farm_calendar_draw_event(u_long *ot, s16 a, s16 b);
static void farm_calendar_draw_wedding(u_long *ot, s16 x, s16 y);
static void farm_calendar_draw_entry(s32 index, u_long *ot, s16 x, s16 y);
static void farm_calendar_draw_result(u_long *ot, s16 year, s16 season);
static void farm_calendar_draw_days(u_long *object, s16 start);
static void farm_calendar_draw_day_cell(u_long *ot, s16 slot, s16 startOffset, s16 mode_arg);
static void farm_calendar_cell_position(s16 index, s16 offset, s16 *x, s16 *y);
static void farm_calendar_draw_weekday_header(u_long *frame_ot, u_long *text_ot);

#define CALENDAR_CHOICE_NONE 0xFF
#define CALENDAR_EVENT_COUNT 18
#define CALENDAR_MESSAGE_DEFAULT 0x3DC
#define CALENDAR_MESSAGE_WEDDING 0x3DD
#define CALENDAR_CELL_W 41
#define CALENDAR_CELL_H 14
#define CALENDAR_ORIGIN_X 30
#define CALENDAR_ORIGIN_Y 80
/* The calendar's icon animation range: today, an event, a wedding. */
#define CALENDAR_ICON_ANIM 0x1E
/* The weekday header's frame (seven cells wide) and the first weekday label. */
#define CALENDAR_HEADER_X 16
#define CALENDAR_HEADER_Y 48
#define CALENDAR_HEADER_W 287
#define CALENDAR_HEADER_H 116
#define CALENDAR_WEEKDAY_X 22
#define CALENDAR_WEEKDAY_Y 66
/* The grey the screen dims to behind the calendar. */
#define CALENDAR_DIM_GREY 0x40000
/* The text palettes of the Sunday and Saturday labels and day numbers. */
enum CalendarPalette {
    CALENDAR_PALETTE_SUNDAY = 4,
    CALENDAR_PALETTE_SATURDAY = 5,
};

/* The OT tags the calendar draws into: the cursor, the today and event marks, the text
 * (the year line, the day numbers) and the weekday header's frame. */
enum CalendarOt {
    CALENDAR_OT_CURSOR = 20,
    CALENDAR_OT_MARKS = 21,
    CALENDAR_OT_TEXT = 24,
    CALENDAR_OT_HEADER_FRAME = 25,
};

/* The calendar's states: closed, open, or closing after a choice. */
enum CalendarState {
    CALENDAR_CLOSED = 0,
    CALENDAR_OPEN = 1,
    CALENDAR_CLOSING = 2,
};

/* Scratch buffer for the number and season text. */
static u16 calendar_scratch[12];

static s32 calendar_state = CALENDAR_CLOSED;

u32 calendar_choice = CALENDAR_CHOICE_NONE;

static s32 calendar_cursor_day = 0;

s32 calendar_close_hook_active = 0;

static void farm_calendar_reset(void) {
    calendar_state = CALENDAR_CLOSED;
    calendar_cursor_day = 0;
    calendar_close_hook_active = 1;
}

/* Open the calendar with the cursor on today. */
void farm_calendar_open(void) {
    farm_calendar_reset();
    calendar_close_hook_active = calendar_close_hook_enabled();
    calendar_open_hook();
    sound_play(SOUND_MENU_OPEN_CLOSE);
    calendar_state = CALENDAR_OPEN;
    calendar_cursor_day = game_state.progress.clock.date.day;
}

/* Draw the calendar while it is open: the dimmed screen, then its tick, or the close
 * once it is closing. */
void farm_calendar_frame(void) {
    s32 state;
    s32 saved;

    if (calendar_state != CALENDAR_CLOSED) {
        screen_fill_quad(CALENDAR_DIM_GREY, CALENDAR_DIM_GREY, CALENDAR_DIM_GREY);
        state = calendar_state;
        if (state == CALENDAR_CLOSING) {
            if (menu_get_phase() == state) {
                menu_reset();
                calendar_state = CALENDAR_CLOSED;
                farm_unpause();
            }
        } else {
            saved = text_get_palette();
            text_set_palette(3);
            farm_calendar_tick();
            text_set_palette(saved);
        }
    }
}

/* Farm calendar screen tick: START closes it. Otherwise the cursor moves, and either
 * the chosen day's message is shown or, with no choice pending, the calendar waits
 * for one. */
static void farm_calendar_tick(void) {
    u_long *ot;

    if (menu_phase_is_result() != 0 && (input_pad1->pressed & PAD_START)) {
        if (calendar_close_hook_active == 1) {
            calendar_close_hook();
        }
        menu_begin_close();
        calendar_state = CALENDAR_CLOSING;
        sound_play(SOUND_MENU_OPEN_CLOSE);
        return;
    }
    farm_calendar_build_geometry(&game_state.progress.clock.date);
    farm_calendar_move_cursor();
    if (calendar_choice == CALENDAR_CHOICE_NONE) {
        menu_set_sound_enabled(0);
        text_set_input(0);
        menu_request_open(3, 0x3B4);
    } else {
        ot = current_frame->otTags[0];
        farm_calendar_select_message(&ot[22], &ot[23], game_state.progress.clock.date.year,
                      game_state.progress.clock.date.season, calendar_cursor_day);
        return;
    }
    farm_calendar_handle_buttons();
}

/* Triangle or cross chooses the cursor's day; circle closes the calendar. */
static void farm_calendar_handle_buttons(void) {
    s32 flags;

    if (menu_phase_is_result() != 0) {
        flags = input_pad1->pressed;
        if ((flags & (PAD_TRIANGLE | PAD_CROSS)) != 0) {
            menu_begin_close();
            sound_play(SOUND_MENU_CONFIRM);
            calendar_choice = 0;
        } else if ((flags & PAD_CIRCLE) != 0) {
            if (calendar_close_hook_active == 1) {
                calendar_close_hook();
            }
            menu_begin_close();
            calendar_state = CALENDAR_CLOSING;
            sound_play(SOUND_MENU_OPEN_CLOSE);
        }
    }
}

/* Move the farm calendar cursor (30 days, 7 per row) from pad input, then redraw it. */
static void farm_calendar_move_cursor(void) {
    s32 day;
    s16 weekday;
    s16 offset;

    if (calendar_choice == CALENDAR_CHOICE_NONE && menu_phase_is_result() != 0 && (input_pad1->triggered & PAD_ARROWS_MASK)) {
        sound_play(SOUND_MENU_CURSOR);
        if (input_pad1->triggered & PAD_UP) {
            day = calendar_cursor_day;
            calendar_cursor_day = day - 7;
            if (day - 7 < 0) {
                calendar_cursor_day = day + 21;
                if (day + 21 < 23) {
                    calendar_cursor_day = day + 28;
                }
            }
        } else if (input_pad1->triggered & PAD_DOWN) {
            day = calendar_cursor_day;
            calendar_cursor_day = day + 7;
            if (day + 7 >= DAYS_PER_SEASON) {
                calendar_cursor_day = day - 21;
                if (day - 21 >= 7) {
                    calendar_cursor_day = day - 28;
                }
            }
        } else if (input_pad1->triggered & PAD_LEFT) {
            day = calendar_cursor_day;
            calendar_cursor_day = day - 1;
            if (day - 1 < 0) {
                calendar_cursor_day = day + 29;
            }
        } else {
            day = calendar_cursor_day;
            calendar_cursor_day = day + 1;
            if (day + 1 >= DAYS_PER_SEASON) {
                calendar_cursor_day = day - 29;
            }
        }
    }
    weekday = game_state.progress.clock.date.day;
    offset = (game_state.progress.clock.date.weekday + 7 - (weekday % 7)) % 7;
    farm_calendar_draw_cursor(&current_frame->otTags[0][CALENDAR_OT_CURSOR], calendar_cursor_day, offset);
}

/* Draw the calendar for a date: the weekday header, the year and season line, the day
 * cells, and the marks for today, the events and the wedding. */
static void farm_calendar_build_geometry(FarmDate *date) {
    s16 adjusted;
    s16 angle;
    u_long *ot;
    u_long *otRow;
    u_long *otColumn;

    angle = date->day;
    adjusted = (date->weekday + 7 - (angle % 7)) % 7;
    otRow = &current_frame->otTags[0][CALENDAR_OT_TEXT];
    otColumn = &current_frame->otTags[0][CALENDAR_OT_MARKS];
    text_set_spacing(1, 2);
    ot = current_frame->otTags[0];
    farm_calendar_draw_weekday_header(&ot[CALENDAR_OT_HEADER_FRAME], &ot[CALENDAR_OT_TEXT]);
    farm_calendar_draw_result(otRow, date->year, date->season);
    farm_calendar_draw_days(otRow, adjusted);
    farm_calendar_draw_today(otColumn, date->day, adjusted);
    farm_calendar_mark_events(otColumn, date->season, adjusted);
    farm_calendar_mark_wedding(otColumn, date->year, date->season, adjusted);
    text_set_spacing(0, 0);
}

static FarmDateEntry calendar_events[CALENDAR_EVENT_COUNT] = {
    {0x00, 0x00, 0x3CA}, {0x00, 0x07, 0x3CB}, {0x00, 0x0D, 0x3CC}, {0x00, 0x11, 0x3CD},
    {0x00, 0x15, 0x3CE}, {0x01, 0x00, 0x3CF}, {0x01, 0x06, 0x3D0}, {0x01, 0x0B, 0x3D1},
    {0x01, 0x13, 0x3D2}, {0x01, 0x17, 0x3D3}, {0x02, 0x02, 0x3D4}, {0x02, 0x08, 0x3D5},
    {0x02, 0x0C, 0x3D6}, {0x02, 0x14, 0x3D7}, {0x03, 0x09, 0x3D8}, {0x03, 0x0D, 0x3D9},
    {0x03, 0x17, 0x3DA}, {0x03, 0x1D, 0x3DB},
};

/* Mark the event days of the season on the calendar. */
static void farm_calendar_mark_events(u_long *ot, s16 season, s16 offset) {
    s32 i;
    for (i = 0; i < CALENDAR_EVENT_COUNT; i++) {
        if (calendar_events[i].season == season) farm_calendar_draw_event(ot, calendar_events[i].day, offset);
    }
}

/* Mark the wedding day on the calendar, when it falls in this year and season. */
static void farm_calendar_mark_wedding(u_long *ot, s16 year, s16 season, s16 day) {
    if (game_state.player.wedding.bride != -1 &&
        game_state.player.wedding.date.year == year && game_state.player.wedding.date.season == season) {
        farm_calendar_draw_wedding(ot, game_state.player.wedding.date.day, day);
    }
}

/* Open the message for the chosen day: its event's, the wedding's, or the default one.
 * Once the message window has closed, the choice is cleared and the calendar closes. */
static void farm_calendar_select_message(u_long *unused0, u_long *unused1, s16 year, s16 season, s16 day) {
    s32 message;
    s32 i;
    if (calendar_choice != CALENDAR_CHOICE_NONE) {
        message = CALENDAR_MESSAGE_DEFAULT;
        for (i = 0; i < CALENDAR_EVENT_COUNT; i++) {
            if (calendar_events[i].season == season && calendar_events[i].day == day) {
                message = calendar_events[i].message;
            }
        }
        if (game_state.player.wedding.date.year == year && game_state.player.wedding.date.season == season &&
            game_state.player.wedding.date.day == day) {
            message = CALENDAR_MESSAGE_WEDDING;
        }
        menu_request_open(3, message);
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            calendar_choice = CALENDAR_CHOICE_NONE;
            menu_begin_close();
        }
    }
}

static void farm_calendar_draw_cursor(u_long *ot, s16 day, s16 offset) {
    u16 x;
    u16 y;
    farm_calendar_cell_position(day, offset, &x, &y);
    ui_draw_icon_0b_1(ot, x + 12, y + 9);
}

static void farm_calendar_draw_today(u_long *ot, s16 x, s16 y) {
    farm_calendar_draw_entry(0, ot, x, y);
}

static void farm_calendar_draw_event(u_long *ot, s16 a, s16 b) {
    farm_calendar_draw_entry(1, ot, a, b);
}

static void farm_calendar_draw_wedding(u_long *ot, s16 x, s16 y) {
    farm_calendar_draw_entry(2, ot, x, y);
}

static FarmDisplayEntry calendar_display_entries[3] = {
    {CALENDAR_ICON_ANIM, 0x00, -4}, {CALENDAR_ICON_ANIM, 0x01, -4}, {CALENDAR_ICON_ANIM, 0x02, 0},
};

/* Draw the calendar icon `index` (0 today, 1 an event, 2 the wedding) at a day cell. */
static void farm_calendar_draw_entry(s32 index, u_long *ot, s16 x, s16 y) {
    FarmDisplayEntry *entry;
    s16 pos_x;
    s16 pos_y;

    entry = &calendar_display_entries[index];
    anim_select_range(&ui_icon_anim, entry->anim);
    anim_set_frame(&ui_icon_anim, entry->frame);
    farm_calendar_cell_position(x, y, &pos_x, &pos_y);
    pos_x += entry->offsetX;
    anim_draw_sprt_cursor(ot, &ui_icon_anim, ui_icon_anim.animation, ui_icon_anim.frame, pos_x, pos_y);
}

/* Draw the year and the season above the calendar. */
static void farm_calendar_draw_result(u_long *ot, s16 year, s16 season) {
    *text_format_number(year + 1, calendar_scratch, 2) = 0xFFFF;
    text_draw_plain(ot, calendar_scratch, 31, 52);
    text_draw_plain(ot, LOCALIZED(text_bank.messages.year), 57, 52);
    season = season < 0 ? 0 : season >= SEASONS_PER_YEAR ? SEASONS_PER_YEAR - 1 : season;
    ui_draw_icon_2b_sub(ot, 142, 52, season);
    *text_write_season(season, calendar_scratch) = 0xFFFF;
    text_draw_plain(ot, calendar_scratch, 158, 52);
}

/* Draw the thirty day cells, from the starting phase. */
static void farm_calendar_draw_days(u_long *object, s16 start) {
    s32 slot;
    s32 quotient;
    s32 remainder;
    s32 mode;
    s32 value;

    for (slot = 0; slot < DAYS_PER_SEASON; slot++) {
        value = start + slot;
        quotient = value / DAYS_PER_WEEK;
        remainder = value - quotient * DAYS_PER_WEEK;
        mode = 2;
        if (remainder != 0) {
            mode = remainder == 6;
        }
        farm_calendar_draw_day_cell(object, slot, start, mode);
    }
}

/* Draw the day number in a cell: Saturday and Sunday in their palettes, the rest in the
 * current one. */
static void farm_calendar_draw_day_cell(u_long *ot, s16 slot, s16 startOffset, s16 mode_arg) {
    s16 x;
    s16 y;
    s32 mode;
    s32 current;

    *text_format_number(slot + 1, calendar_scratch, 2) = 0xFFFF;
    farm_calendar_cell_position(slot, startOffset, &x, &y);
    current = text_get_palette();
    mode = CALENDAR_PALETTE_SATURDAY;
    if (mode_arg != 1) {
        mode = current;
        if (mode_arg == 2) {
            mode = CALENDAR_PALETTE_SUNDAY;
        }
    }
    text_set_palette(mode);
    text_draw_plain(ot, calendar_scratch, x, y);
    text_set_palette(current);
}

/* The cell's top-left corner: a day slot counted from the month's first cell. */
static void farm_calendar_cell_position(s16 index, s16 offset, s16 *x, s16 *y) {
    s32 sum = index + offset;
    s32 row = sum / DAYS_PER_WEEK;
    s32 col = sum - row * DAYS_PER_WEEK;
    *x = col * CALENDAR_CELL_W + CALENDAR_ORIGIN_X;
    *y = row * CALENDAR_CELL_H + CALENDAR_ORIGIN_Y;
}

/* Draw the weekday header row and the frame around it. */
static void farm_calendar_draw_weekday_header(u_long *frame_ot, u_long *text_ot) {
    s32 normalPalette;
    s32 palette;
    s32 i;
    s16 x;

    window_frame_draw_at(frame_ot, 1, CALENDAR_HEADER_X, CALENDAR_HEADER_Y, CALENDAR_HEADER_W, CALENDAR_HEADER_H);
    i = 0;
    normalPalette = text_get_palette();
    x = CALENDAR_WEEKDAY_X;
    do {
        *text_write_weekday(i, calendar_scratch) = 0xFFFF;
        if (i != 0) {
            palette = CALENDAR_PALETTE_SATURDAY;
            if (i < 6) {
                palette = normalPalette;
            }
        } else {
            palette = CALENDAR_PALETTE_SUNDAY;
        }
        text_set_palette(palette);
        text_draw_plain(text_ot, calendar_scratch, x, CALENDAR_WEEKDAY_Y);
        i++;
        x += CALENDAR_CELL_W;
    } while (i < DAYS_PER_WEEK);
    text_set_palette(normalPalette);
}
