/* Shop page: the row being edited, the gold display, and the item name table. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "farm_main.h"
#include "farm_state.h"
#include "farm_tables.h"
#include "horse_names.h"
#include "house.h"
#include "main.h"
#include "menu_window.h"
#include "pad.h"
#include "player.h"
#include "prim.h"
#include "shop_page.h"
#include "sound_request.h"
#include "text.h"
#include "text_bank.h"
#include "ui_icons.h"

static void shop_menu_step(void);
static void shop_edit_end(void);
static s32 shop_value_input(void);
static s32 shop_mode_input(void);
static void shop_draw_gold(void);
static void shop_draw_header(void);
static void shop_draw_message(void);
static void shop_draw_row(s16 slot);
static s32 shop_rows_unchanged(void);
static void shop_adjust_value(s16 delta, s16 *value);

/* The race bet on the horse race (the player's horse is its row 2). */
#define SHOP_RACE_HORSE 3

/* The six values of the row being edited, and the one under the cursor. */
static s16 shop_slot_values[6];
static s16 shop_slot_current;
/* The gold on hand, and the gold when the value edit began (restored if it is cancelled). */
static s32 shop_gold;
static s32 shop_gold_pending;
/* Scratch buffer for the gold display's number text. */
static u16 shop_number_text[12];

/* Close the shop page: unpause the farm, play the close sound and hand control back to the player. */
void shop_page_close(void) {
    u32 value;
    cooking_menu_step = 0;
    game_state.globalFlags |= GLOBAL_FLAG_TRANSITION_END;
    farm_unpause();
    sound_play(3);
    value = game_state.player.stateFlags;
    value |= PLAYER_FLAG_REFRESH_HELD;
    game_state.player.stateFlags = value;
    menu_reset();
}

/* True when the six bet rows of the selected race hold no tickets. */
s32 shop_row_all_zero(void) {
    s32 sum;
    u16 index;
    s32 i;

    sum = 0;
    index = 2;
    if (game_state.raceBets.race != SHOP_RACE_HORSE) {
        index = game_state.raceBets.race;
    }
    for (i = 0; i < 6; i++) {
        sum += game_state.raceBets.rows[index][i].tickets;
    }
    return sum == 0;
}

/* The editor's states (shop_menu_state). */
enum ShopEditState {
    SHOP_EDIT_OFF = 0,
    SHOP_EDIT_BROWSE = 1,       /* the cursor moves over the rows and OK */
    SHOP_EDIT_VALUE = 2,        /* the tickets of the row under the cursor are changed */
    SHOP_EDIT_BOOKED = 3,       /* closing with the bets booked */
    SHOP_EDIT_DECLINED = 4,     /* closing without booking */
    SHOP_EDIT_CLOSING = 5,      /* waiting for the message window to close */
};

/* The editor state, see enum ShopEditState. */
static u32 shop_menu_state = SHOP_EDIT_OFF;

/* True when the editor is not open. */
int shop_menu_idle(void) {
    return shop_menu_state < 1;
}

/* The row under the cursor: 0 to 5 for the bet rows, SHOP_ROW_OK for the OK row. */
static s32 shop_cursor = 0;

/* The rows are the six race-bet rows, then the OK row. */
enum ShopRow {
    SHOP_ROW_OK = 6,
    SHOP_ROW_COUNT = 7,
};

/* A ticket costs 50 gold; a row holds at most 99 tickets. */
#define SHOP_TICKET_PRICE 50
#define SHOP_TICKET_MAX 99
#define SHOP_TICKET_LIMIT 100

/* The rows of the race-bet list start at y 50 and go down 14 a row. */
#define SHOP_ROW_TOP 50
#define SHOP_ROW_PITCH 14
#define SHOP_ROW_X 43

/* The message sectors the editor opens: the bet list itself, and the two
 * sectors that close it, declined or booked. */
enum ShopMessage {
    SHOP_MSG_BET_LIST = 0x764,
    SHOP_MSG_LEAVE_DECLINED = 0x447,
    SHOP_MSG_BETS_BOOKED = 0x448,
};

/* The pointer icons beside the message: the row under the cursor, the OK row,
 * and the value being edited. */
enum ShopPointerIcon {
    SHOP_ICON_ROW = 0x22,
    SHOP_ICON_OK = 0xD6,
    SHOP_ICON_VALUE = 0xC4,
};

/* The glyph runs of the row numbers: rows 1 and 2, then rows 3 to 6. */
#define SHOP_GLYPH_ROW_FIRST 0x79
#define SHOP_GLYPH_ROW_LATER 0x8F

/* Digits drawn for the gold, and for a row's odds and tickets. */
#define SHOP_GOLD_DIGITS 7
#define SHOP_VALUE_DIGITS 2

/* Horizontal advances between the columns of a race-bet row: the row number,
 * the horse's name, the odds label, the odds and the tickets. */
#define SHOP_NUMBER_ADVANCE 18
#define SHOP_NAME_ADVANCE 81
#define SHOP_ODDS_LABEL_ADVANCE 36
#define SHOP_ODDS_ADVANCE 27
#define SHOP_TICKETS_LABEL_ADVANCE 18

/* Clear the editor and its six values. */
static void shop_reset_editor(void) {
    s32 i;
    i = 5;
    shop_menu_state = SHOP_EDIT_OFF;
    shop_cursor = 0;
    do {
        shop_slot_values[i] = 0;
        i--;
    } while (i >= 0);
    shop_gold_pending = 0;
    shop_slot_current = 0;
}

/* Open the editor on the selected race's bet rows: copy their tickets and the gold. */
void shop_edit_begin(void) {
    FarmState *state;
    s32 selected;
    s32 i;
    state = &game_state;
    selected = 2;
    if (state->raceBets.race != SHOP_RACE_HORSE) {
        selected = state->raceBets.race;
    }
    shop_reset_editor();
    shop_menu_state = SHOP_EDIT_BROWSE;
    sound_play(3);
    i = 0;
    shop_gold = state->player.gold;
    do {
        shop_slot_values[i] = state->raceBets.rows[selected][i].tickets;
        i++;
    } while (i < 6);
}

/* Run one frame of the editor while it is open, over a full-screen tile. */
void shop_menu_tick(void) {
    if (shop_menu_state != SHOP_EDIT_OFF) {
        text_set_spacing(0, 2);
        prim_add_tile(&current_frame->otTags[0][24], 0, 0, HWD0, VWD0, 0, 1);
        shop_menu_step();
        text_set_spacing(0, 0);
    }
}

/* Run one frame of the editor (shop_menu_state): draw the gold, the header and the message, take
 * the input of the current state, and open the bet list message. */
static void shop_menu_step(void)
{
    u32 mode;
    s32 result;

    shop_draw_gold();
    shop_draw_header();
    shop_draw_message();

    mode = shop_menu_state;
    if (mode == SHOP_EDIT_BROWSE) {
        result = shop_mode_input();
        if (result == -1) {
            menu_begin_close();
            shop_menu_state = SHOP_EDIT_DECLINED;
        } else if (result == 1) {
            shop_menu_state = SHOP_EDIT_BOOKED;
        } else if (result == 2) {
            shop_menu_state = SHOP_EDIT_VALUE;
        }
    } else if (mode == SHOP_EDIT_VALUE) {
        result = shop_value_input();
        if (result == -1) {
            shop_menu_state = SHOP_EDIT_BROWSE;
            shop_slot_values[shop_cursor] = shop_slot_current;
            shop_gold = shop_gold_pending;
        } else if (result == -2) {
            menu_begin_close();
            shop_menu_state = SHOP_EDIT_DECLINED;
        } else if (result == 1) {
            shop_menu_state = SHOP_EDIT_BROWSE;
        } else if (result == 2) {
            menu_begin_close();
            shop_menu_state = SHOP_EDIT_BOOKED;
        }
    } else if (mode == SHOP_EDIT_BOOKED) {
        menu_begin_close();
        menu_request_open(1, SHOP_MSG_BETS_BOOKED);
        shop_edit_end();
        shop_menu_state = SHOP_EDIT_CLOSING;
        return;
    } else if (mode == SHOP_EDIT_DECLINED) {
        menu_begin_close();
        menu_request_open(1, SHOP_MSG_LEAVE_DECLINED);
        shop_edit_end();
        shop_menu_state = SHOP_EDIT_CLOSING;
        return;
    } else if (mode == SHOP_EDIT_CLOSING) {
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            farm_unpause();
            sound_play(3);
            shop_menu_state = SHOP_EDIT_OFF;
        }
        return;
    }
    menu_set_sound_enabled(0);
    text_set_input(0);
    menu_request_open(1, SHOP_MSG_BET_LIST);
}

/* Leave the editor with the bets booked (state SHOP_EDIT_BOOKED): book the gold difference
 * against the expense tally, restore the gold, and write the six edited values back into the
 * selected race's rows. The reverse of shop_edit_begin. */
static void shop_edit_end(void) {
    s32 selected;
    s32 delta;
    s32 i;

    if (game_state.raceBets.race == SHOP_RACE_HORSE) {
        selected = 2;
    } else {
        selected = game_state.raceBets.race;
    }
    if (shop_menu_state == SHOP_EDIT_BOOKED) {
        delta = game_state.player.gold - shop_gold;
        if (delta > 0) {
            game_state.progress.estate.ledger[0].expenses += delta;
        } else if (delta < 0) {
            game_state.progress.estate.ledger[0].expenses += delta;
        }
        i = 0;
        game_state.player.gold = shop_gold;
        do {
            game_state.raceBets.rows[selected][i].tickets = shop_slot_values[i];
            i++;
        } while (i < 6);
    }
}

/* Input while one row's tickets are edited: up, down, right and left change the value (10, -10,
 * 1, -1). Start leaves the page, Circle cancels the value, Cross confirms it. Returns -2 to leave
 * with no change, 2 to book, -1 to cancel the value, 1 to confirm it, 0 to stay. */
static s32 shop_value_input(void) {
    s32 direction;
    s16 *row;
    s32 flags;
    if (menu_phase_is_result() == 0) {
        return 0;
    }
    direction = 0;
    row = &shop_slot_values[shop_cursor];
    if (input_pad1->triggered & PAD_UP) {
        direction = 10;
    } else if (input_pad1->triggered & PAD_DOWN) {
        direction = -10;
    } else if (input_pad1->triggered & PAD_RIGHT) {
        direction = 1;
    } else if (input_pad1->triggered & PAD_LEFT) {
        direction = -1;
    }
    if (direction != 0) {
        sound_play(5);
        shop_adjust_value(direction, row);
    }
    flags = input_pad1->pressed;
    if (flags & PAD_START) {
        if (shop_rows_unchanged() == 1) {
            sound_play(1);
            return -2;
        }
        sound_play(0);
        return 2;
    }
    if (flags & PAD_CIRCLE) {
        sound_play(1);
        return -1;
    }
    if (flags & PAD_CROSS) {
        sound_play(0);
        return 1;
    }
    return 0;
}

/* Input on the row list: up and down move the cursor, wrapping through the OK row. Cross edits
 * the row under the cursor, or on the OK row confirms. Start books, Circle cancels. Returns the
 * transition: -1 to leave with no change, 1 to book, 2 to edit the row, 0 to stay. */
static s32 shop_mode_input(void) {
    s32 direction;
    s32 flags;
    s32 mode;
    if (menu_phase_is_result() == 0) return 0;
    direction = 0;
    flags = input_pad1->triggered;
    if (flags & PAD_UP)
        direction = -1;
    else if (flags & PAD_DOWN)
        direction = 1;
    if (direction != 0) {
        sound_play(5);
        mode = shop_cursor + direction;
        shop_cursor = mode;
        if (mode < 0) shop_cursor = SHOP_ROW_OK;
        if (shop_cursor >= SHOP_ROW_COUNT) shop_cursor = 0;
    }
    flags = input_pad1->pressed;
    if (flags & PAD_START) {
        if (shop_rows_unchanged() == 1) {
            sound_play(1);
            return -1;
        }
        sound_play(0);
        return 1;
    }
    if (flags & PAD_CIRCLE) {
        sound_play(1);
        return -1;
    }
    if (flags & PAD_CROSS) {
        mode = shop_cursor;
        if (mode != SHOP_ROW_OK) {
            shop_slot_current = shop_slot_values[mode];
            shop_gold_pending = shop_gold;
            sound_play(0);
            return 2;
        }
        if (shop_rows_unchanged() == 1) {
            sound_play(1);
            return -1;
        }
        sound_play(0);
        return 1;
    }
    return 0;
}

/* Draw the money panel: its title, then the gold and its unit. */
static void shop_draw_gold(void) {
    u_long *display;
    u_long *line;
    u_long *source;
    display = current_frame->otTags[0];
    line = &display[22];
    window_frame_draw_at(&display[23], 0, 227, 16, 71, 30);
    source = line;
    text_draw_plain(source, LOCALIZED_NAME(text_bank.names.money_title), 229, 18);
    text_copy_string(text_format_number(shop_gold, shop_number_text, SHOP_GOLD_DIGITS),
                  LOCALIZED_NAME(text_bank.names.gold_unit));
    text_draw_plain(source, shop_number_text, 229, 32);
}

/* Draw the race-bet panel: its frame, the race number, the OK label, then the six rows. */
static void shop_draw_header(void) {
    u_long *display;
    u16 index;
    s32 slot;
    FrameState *state;
    state = current_frame;
    window_frame_draw_at((display = &state->otTags[0][22]) + 1, 0, 32, 48, 257, 114);
    index = 2;
    if (game_state.raceBets.race != SHOP_RACE_HORSE) {
        index = game_state.raceBets.race;
    }
    *text_format_number(index + 1, shop_number_text, 1) = TEXT_END;
    text_set_source(0, shop_number_text);
    text_draw_plain(display, LOCALIZED(text_bank.messages.race_bets), 34, 50);
    text_draw_plain(display, LOCALIZED(text_bank.messages.ok), 223, 148);
    slot = 0;
    do {
        shop_draw_row(slot);
        slot++;
    } while (slot < 6);
}

/* Draw the pointer icon beside the row under the cursor: the row icon or the OK icon while
 * browsing, the value icon while a value is edited. */
static void shop_draw_message(void) {
    s32 frame;
    s32 mode;
    s32 message;
    s16 timing;

    frame = shop_cursor;
    timing = (frame + 1) * SHOP_ROW_PITCH + SHOP_ROW_TOP;
    mode = shop_menu_state;
    message = -1;
    if (mode == SHOP_EDIT_BROWSE) {
        if (frame != SHOP_ROW_OK) {
            message = SHOP_ICON_ROW;
        } else {
            message = SHOP_ICON_OK;
        }
    } else if (mode == SHOP_EDIT_VALUE) {
        message = SHOP_ICON_VALUE;
    }
    if (message != -1) {
        ui_draw_icon_0b_1(&current_frame->otTags[0][21], message + 6, timing + 6);
    }
}

/* Each item's name, as an offset into the name table; the second halfword is
 * always 0. */
static s16 shop_item_name_offsets[40][2] = {
    {0x000, 0}, {0x00E, 0}, {0x01C, 0}, {0x026, 0}, {0x034, 0}, {0x040, 0},
    {0x04E, 0}, {0x05C, 0}, {0x068, 0}, {0x074, 0}, {0x082, 0}, {0x090, 0},
    {0x09C, 0}, {0x0A8, 0}, {0x0B4, 0}, {0x0C2, 0}, {0x0D0, 0}, {0x0DE, 0},
    {0x0EA, 0}, {0x0F6, 0}, {0x104, 0}, {0x112, 0}, {0x120, 0}, {0x12A, 0},
    {0x138, 0}, {0x144, 0}, {0x152, 0}, {0x160, 0}, {0x16C, 0}, {0x17A, 0},
    {0x186, 0}, {0x192, 0}, {0x19C, 0}, {0x1AA, 0}, {0x1B4, 0}, {0x1C2, 0},
    {0x1CC, 0}, {0x1DA, 0}, {0x1E6, 0}, {0x1F0, 0},
};

/* Draw row `slot` (0..5) of the shop page: its number glyph, the item's name,
 * a label, the row's odds and the tickets bought, column by column from x 43.
 * The horse race (race 3) reads the third race's rows, and its row 2 shows the
 * player's horse. */
static void shop_draw_row(s16 slot)
{
    u_long *ot;
    s32 race;
    s32 rowSet;
    s32 item;
    s32 odds;
    s32 tickets;
    s16 x;
    u16 *text;
    s16 y;
    u16 *name;

    ot = &current_frame->otTags[0][22];
    race = game_state.raceBets.race;
    rowSet = 2;
    if (race != SHOP_RACE_HORSE) {
        rowSet = race;
    }
    item = game_state.raceBets.rows[rowSet][slot].runner;
    odds = game_state.raceBets.rows[rowSet][slot].odds;
    tickets = shop_slot_values[slot];
    y = (slot + 1) * SHOP_ROW_PITCH + SHOP_ROW_TOP;
    if (race == SHOP_RACE_HORSE && slot == 2) {
        item = -1;
    }
    x = SHOP_ROW_X;
    shop_number_text[0] = slot < 2 ? slot + SHOP_GLYPH_ROW_FIRST : slot + SHOP_GLYPH_ROW_LATER;
    text = shop_number_text;
    text[1] = TEXT_END;
    text_draw_plain(ot, text, x, y);
    x += SHOP_NUMBER_ADVANCE;
    if (item != -1) {
        name = HORSE_NAME(shop_item_name_offsets[item][0]);
    } else {
        name = game_state.animals.horse.actor.name;
    }
    text_draw_plain(ot, name, x, y);
    x += SHOP_NAME_ADVANCE;
    text_draw_plain(ot, LOCALIZED(text_bank.messages.odds), x, y);
    x += SHOP_ODDS_LABEL_ADVANCE;
    *text_format_number(odds, shop_number_text, SHOP_VALUE_DIGITS) = TEXT_END;
    text_draw_plain(ot, shop_number_text, x, y);
    x += SHOP_ODDS_ADVANCE;
    *text_format_number(tickets, shop_number_text, SHOP_VALUE_DIGITS) = TEXT_END;
    text_draw_plain(ot, shop_number_text, x, y);
    text_draw_plain(ot, LOCALIZED(text_bank.messages.tickets), x + SHOP_TICKETS_LABEL_ADVANCE, y);
}

/* Compare the tickets of the selected race's rows with the edited values. */
static s32 shop_rows_unchanged(void) {
    FarmState *state;
    s16 mode;
    s16 selected;
    s32 result;
    s32 i;

    state = &game_state;
    mode = state->raceBets.race;
    selected = 2;
    if (mode != SHOP_RACE_HORSE) {
        selected = mode;
    }
    result = 1;
    i = 0;
    do {
        if (shop_slot_values[i] != state->raceBets.rows[selected][i].tickets) {
            result = 0;
        }
        i++;
    } while (i < 6);
    return result;
}

/* Move a row's tickets by delta. Past the top, the value goes to the most the gold allows, or to 0
 * when it is already there. Below 0 it goes to 0, or to that most when it is already 0. The gold
 * moves by the price of the change. */
static void shop_adjust_value(s16 delta, s16 *value) {
    s16 current;
    s32 maxTickets;
    s32 wanted;

    current = *value;
    maxTickets = current + shop_gold / SHOP_TICKET_PRICE;
    if (maxTickets >= SHOP_TICKET_LIMIT) {
        maxTickets = SHOP_TICKET_MAX;
    }
    if (maxTickets < 0) {
        maxTickets = 0;
    }
    wanted = current + delta;
    if (wanted < 0) {
        if (current == 0) {
            *value = maxTickets;
        } else {
            *value = 0;
        }
    } else if (maxTickets < wanted) {
        if (current == maxTickets) {
            *value = 0;
        } else {
            *value = maxTickets;
        }
    } else {
        *value = wanted;
    }
    shop_gold += current * SHOP_TICKET_PRICE;
    shop_gold -= *value * SHOP_TICKET_PRICE;
}
