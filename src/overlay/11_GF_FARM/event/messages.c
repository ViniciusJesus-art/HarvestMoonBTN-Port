/* Farm messages: the calendar and special-date messages, the message pairs and
 * their scrolling window, and the ending check. */

#include "types.h"
#include "clock.h"
#include "farm_load.h"
#include "farm_main.h"
#include "farm_state.h"
#include "main.h"
#include "menu_window.h"
#include "messages.h"
#include "pad.h"
#include "scene.h"
#include "text.h"
#include "texture_pack.h"

/* Message ids of the ending offers start here, one per offer slot. */
#define ENDING_OFFER_MESSAGE_BASE 0x258
#define ENDING_OFFER_SLOTS 9

/* The message window's position and its alternate position. */
typedef struct MessageWindowRect {
    s16 x;
    s16 y;
    s16 altX;
    s16 altY;
} MessageWindowRect;

/* One row of message_calendar_rows: the message of a weekday, by week. */
typedef struct FarmCalendarRow {
    s16 rows;
    s16 base;
    s16 result;
} FarmCalendarRow;

static void message_state_clear(void);
static void message_refresh_clock(void);
static s32 message_pick_hour_weather(s16 weather, s16 hour);
static int message_has_hour_pick(void);
static void message_pick_season(s16 period, s16 season);
static void message_pick_date(s16 a, s16 b);
static void message_pick_calendar_row(s16 year, s16 season, s16 day, s16 kind);
static s32 message_pick_special_date(s16 season, s16 day);
static s32 message_pick_ending(void);
static s16 calendar_row_index(s16 a, s16 b);
static void message_menu_input(void);
static s32 message_menu_pad_select(void);
static void message_show_result(void);
static s32 message_id_for_result(void);
static void message_show_selected(void);
static void message_window_setup(void);
static void message_scroll_begin(void);
static void message_scroll_tick(void);
static void message_scroll_end(void);
static void ending_offer_advance(void);
static void ending_offer_confirm(void);

/* Steps of message_phase: the message window's state machine. */
enum MessagePhase {
    MESSAGE_PHASE_IDLE,
    MESSAGE_PHASE_START,
    MESSAGE_PHASE_MENU_WAIT,
    MESSAGE_PHASE_OPEN_SCROLL,
    MESSAGE_PHASE_SCROLL_WAIT,
    MESSAGE_PHASE_END,
    MESSAGE_PHASE_CANCEL_WAIT,
};

/* The pair of message_pairs that holds the day's result: its b is the result code
 * (message_show_result), and its a the message the result opens. */
enum MessagePairSlot {
    MESSAGE_PAIR_RESULT = 2,
};

/* The message of the hour or the weather, when one applies (message_pick_hour_weather), and
 * the result code it is shown with. */
enum HourWeatherMessage {
    MESSAGE_LATE_HOUR = 0x26B,          /* the hour is before six in the morning */
    MESSAGE_BAD_WEATHER = 0x26A,        /* a storm or a blizzard today */
};

enum HourWeatherResult {
    MESSAGE_RESULT_LATE_HOUR = 0x10,
    MESSAGE_RESULT_BAD_WEATHER = 0x11,
};

/* Which of the four message pairs is shown. */
static s32 message_index;
/* Set once a message group has started, cleared when the state is reset. */
static s32 message_phase;
static MessagePair message_pairs[4];
static MessageWindowRect message_window;
/* The scrolling message's frame counter, and the group it was last taken for. */
static s32 message_scroll_counter;
static s32 message_scroll_group;

/* Clear the message state. */
void message_state_reset(void) {
    message_state_clear();
}

/* Start the message machine when it is idle. */
void message_machine_start(void) {
    if (message_phase == MESSAGE_PHASE_IDLE) {
        message_phase = MESSAGE_PHASE_START;
    }
}

/* Step the message machine while the farm is at PLACE_2: it opens the day's messages (the hour,
 * weather, season, date and calendar pairs) and runs the scroll that shows the chosen pair. */
void message_machine_step(void) {
    s32 result;

    if (farm_area.mode.data->head.place.location != PLACE_2) {
        return;
    }
    switch (message_phase) {
    case MESSAGE_PHASE_IDLE:
        break;
    case MESSAGE_PHASE_START:
        message_refresh_clock();
        message_window_setup();
        if (message_has_hour_pick() == 1) {
            message_phase = MESSAGE_PHASE_OPEN_SCROLL;
        } else {
            message_show_result();
            message_phase = MESSAGE_PHASE_MENU_WAIT;
        }
        break;
    case MESSAGE_PHASE_MENU_WAIT:
        result = menu_get_phase();
        if (result == TEXT_WINDOW_END_WAIT) {
            if (message_menu_pad_select() == 1) {
                menu_begin_close();
            }
        } else if (result == TEXT_WINDOW_CLOSED) {
            message_phase = MESSAGE_PHASE_OPEN_SCROLL;
        }
        break;
    case MESSAGE_PHASE_OPEN_SCROLL:
        menu_begin_close();
        message_scroll_begin();
        message_show_selected();
        message_phase = MESSAGE_PHASE_SCROLL_WAIT;
        break;
    case MESSAGE_PHASE_SCROLL_WAIT:
        result = menu_get_phase();
        if (result == TEXT_WINDOW_END_WAIT) {
            ending_offer_confirm();
        }
        if (result == TEXT_WINDOW_CLOSED) {
            message_phase = MESSAGE_PHASE_END;
        }
        if (result != 14) {
            message_menu_input();
        }
        message_scroll_tick();
        break;
    case MESSAGE_PHASE_END:
        message_scroll_end();
        message_state_clear();
        farm_unpause();
        break;
    case MESSAGE_PHASE_CANCEL_WAIT:
        if (menu_get_phase() == TEXT_WINDOW_CLOSED) {
            message_phase = MESSAGE_PHASE_END;
        }
        break;
    }
}

static void message_state_clear(void) {
    message_index = 0;
    message_phase = MESSAGE_PHASE_IDLE;
}

/* Pick the day's messages: the hour or weather message when one applies (it takes every pair),
 * else the season, date and calendar messages. */
static void message_refresh_clock(void) {
    FarmState *fs = &game_state;
    if (message_pick_hour_weather(fs->world.weather, fs->progress.clock.time.hm.hour) != 1) {
        message_pick_season(fs->world.weatherTomorrow, fs->progress.clock.date.season);
        message_pick_date(fs->progress.clock.date.season, fs->progress.clock.date.day);
        message_pick_calendar_row(fs->progress.clock.date.year, fs->progress.clock.date.season, fs->progress.clock.date.day, fs->progress.clock.date.weekday);
    }
}

/* Fill all four message_pairs with the hour or weather message when the hour is before six or the
 * weather is a storm or a blizzard. Returns 1 when one applies, else 0. */
static s32 message_pick_hour_weather(s16 weather, s16 hour) {
    s16 message;
    s16 kind;
    s32 i;

    message = 0;
    if ((u16)hour < 6) message = MESSAGE_LATE_HOUR;
    if (weather == WEATHER_STORM || weather == WEATHER_BLIZZARD) message = MESSAGE_BAD_WEATHER;
    if (message != 0) {
        kind = MESSAGE_RESULT_LATE_HOUR;
        if (message == MESSAGE_BAD_WEATHER) kind = MESSAGE_RESULT_BAD_WEATHER;
        for (i = 0; i < 4; i++) {
            message_pairs[i].a = message;
            message_pairs[i].b = kind;
        }
        return 1;
    }
    return 0;
}

/* True when the pairs hold the hour or the weather message. */
static int message_has_hour_pick(void) {
    return (u32)((u16)message_pairs[0].b - MESSAGE_RESULT_LATE_HOUR) < 2;
}

static short message_kind_ids[4] = {0x026C, 0x026E, 0x0271, 0x0273};

/* Pair 0: the season's message for tomorrow's weather. Sunny gives result 4, rain 5, snow 6,
 * a storm (0x270) 7 and a blizzard (0x275) 8. */
static void message_pick_season(s16 weather, s16 season) {
    s16 message;
    s16 result;

    if (weather >= WEATHER_STORM) {
        message = 0x275;
        if (weather == WEATHER_STORM) {
            message = 0x270;
            result = 7;
        } else {
            result = 8;
        }
    } else {
        message = message_kind_ids[season];
        result = 4;
        if (weather > WEATHER_SUNNY) {
            message = message + 1;
            result = 6;
            if (weather == WEATHER_RAIN) result = 5;
        }
    }
    message_pairs[0].a = message;
    message_pairs[0].b = result;
}

/* Pairs 1 and 3: the date's message, for the day index season * DAYS_PER_SEASON + day. */
static void message_pick_date(s16 a, s16 b) {
    s32 v = a * DAYS_PER_SEASON + b;
    message_pairs[1].a = v + 0x277;
    message_pairs[1].b = 0;
    message_pairs[3].a = v + 0x2ef;
    message_pairs[3].b = 1;
}

/* One row per weekday (the weekday is the kind): the message base, the number of weeks in its
 * cycle, and the result code. */
static FarmCalendarRow message_calendar_rows[7] = {
    {40, 560, 14},
    {38, 520, 15},
    {18, 500, 3},
    {20, 480, 12},
    {35, 440, 13},
    {38, 400, 11},
    {23, 371, 9},
};

/* Pick the calendar message for pair RESULT: special dates first (message_pick_special_date), else
 * a row-based message offset by the day-of-period. */
static void message_pick_calendar_row(s16 year, s16 season, s16 day, s16 kind) {
    const FarmCalendarRow *range;
    s16 value;
    s16 message;
    s16 result;

    if (message_pick_special_date(season, day) == 1) {
        return;
    }
    range = &message_calendar_rows[kind];
    value = (s16)(year * 113 - 5) + (day + season * DAYS_PER_SEASON);
    message = range->base + calendar_row_index(value, range->rows);
    result = range->result;
    switch (kind) {
    case WEEKDAY_FRIDAY:
        if (value >= 548) {
            message++;
        }
        break;
    case WEEKDAY_SATURDAY:
        if (message_pick_ending() == 1) {
            return;
        }
        if (year == 6 && season == SEASON_WINTER) {
            if (day == 15) {
                message = 0x182;
            }
            if (day == 22) {
                message = 0x178;
            }
        }
        break;
    }
    message_pairs[MESSAGE_PAIR_RESULT].a = message;
    message_pairs[MESSAGE_PAIR_RESULT].b = result;
}

/* Pick the message of the special dates: the last winter days (from 28) and the first spring days
 * (before 5) have their own messages, with result 10. Returns 1 when one applies. */
static s32 message_pick_special_date(s16 season, s16 day) {
    s16 message = 0;
    if (season == SEASON_WINTER) {
        if (day >= 28) message = day + 587;
    } else if (season == SEASON_SPRING) {
        if (day < 5) message = day + 610;
    }
    if (message != 0) {
        message_pairs[MESSAGE_PAIR_RESULT].a = message;
        message_pairs[MESSAGE_PAIR_RESULT].b = 10;
        return 1;
    }
    return 0;
}

/* Outside the four house-0 scenes, advance the ending offers and take the first offer that is
 * offered, accepted or paid as the result pair's message. Returns 1 when one is taken. */
static s32 message_pick_ending(void) {
    s32 slot;
    FarmZoneHeader *record;
    record = &farm_area.mode.data->head;
    if (record->scene - AREA_HOUSE0_SPRING < 4U) {
        return 0;
    }
    ending_offer_advance();
    for (slot = 0; slot < ENDING_OFFER_SLOTS; slot++) {
        if (game_state.ending.offers[slot] - ENDING_OFFER_OFFERED < 3U) {
            message_pairs[MESSAGE_PAIR_RESULT].a = slot + ENDING_OFFER_MESSAGE_BASE;
            message_pairs[MESSAGE_PAIR_RESULT].b = ENDING_OFFER_ACCEPTED;
            return 1;
        }
    }
    return 0;
}

/* The week of `a` in a cycle of `b` weeks, capped at the last row. */
static s16 calendar_row_index(s16 a, s16 b) {
    s16 q;
    q = a % (b * 7);
    q = q / 7;
    if (q >= b) q = b - 1;
    return q;
}

/* Option input while the message scrolls: Circle closes it, and a direction picks its pair (up 0,
 * right 1, left 2, down 3) and closes the message to open that one. */
static void message_menu_input(void) {
    s32 result;
    s32 selected;
    s32 current;

    result = 0x20;
    if ((input_pad1->pressed & (PAD_CIRCLE | PAD_ARROWS_MASK)) == 0) {
        return;
    }
    if ((input_pad1->pressed & (PAD_CIRCLE | PAD_ARROWS_MASK)) == PAD_CIRCLE) {
        message_phase = MESSAGE_PHASE_CANCEL_WAIT;
        menu_begin_close();
        return;
    }
    if ((input_pad1->pressed & (PAD_CIRCLE | PAD_ARROWS_MASK)) == PAD_UP) {
        selected = 0;
    } else if ((input_pad1->pressed & (PAD_CIRCLE | PAD_ARROWS_MASK)) == PAD_LEFT) {
        selected = 2;
    } else if ((input_pad1->pressed & (PAD_CIRCLE | PAD_ARROWS_MASK)) == PAD_RIGHT) {
        selected = 1;
    } else if ((input_pad1->pressed & (PAD_CIRCLE | PAD_ARROWS_MASK)) == PAD_DOWN) {
        selected = 3;
    } else {
        selected = message_index;
    }
    current = message_index;
    if (current != selected) {
        message_index = selected;
        menu_begin_close();
        message_phase = MESSAGE_PHASE_MENU_WAIT;
    }
}

/* Pick the message pair by the pressed direction (up 0, right 1, left 2, down 3). Returns 1 when
 * a direction was pressed. */
static s32 message_menu_pad_select(void) {
    s32 a = input_pad1->pressed & (PAD_CIRCLE | PAD_ARROWS_MASK);
    if (a == PAD_UP) {
        message_index = 0;
    } else if (a == PAD_LEFT) {
        message_index = 2;
    } else if (a == PAD_RIGHT) {
        message_index = 1;
    } else if (a == PAD_DOWN) {
        message_index = 3;
    } else {
        return 0;
    }
    return 1;
}

/* Open the message of the result code in the result pair. */
static void message_show_result(void) {
    s32 tmp = message_id_for_result();
    menu_begin_close();
    menu_request_open(3, tmp);
}

/* Map the farm result code in the result pair's b to a message id. */
static s32 message_id_for_result(void)
{
    switch (message_pairs[MESSAGE_PAIR_RESULT].b) {
    case 2:
    default:
        return 0x39D;
    case 3:
        return 0x399;
    case 9:
        return 0x39E;
    case 10:
        if (message_pairs[MESSAGE_PAIR_RESULT].a >= 0x267) {
            return 0x3A0;
        }
        return 0x3A1;
    case 11:
        return 0x39C;
    case 12:
        return 0x39A;
    case 13:
        return 0x39B;
    case 14:
        return 0x39F;
    case 15:
        return 0x398;
    case MESSAGE_RESULT_LATE_HOUR:
        return MESSAGE_LATE_HOUR;
    case MESSAGE_RESULT_BAD_WEATHER:
        return MESSAGE_BAD_WEATHER;
    }
}

/* Open the message of the pair the pad selected. */
static void message_show_selected(void) {
    menu_request_open(3, message_pairs[message_index].a);
}

/* Place the message window: the four house scenes from AREA_HOUSE2_SPRING get the lower position,
 * the others the top one. */
static void message_window_setup(void) {
    s32 mode;

    mode = farm_area.mode.data->head.scene;
    mode -= AREA_HOUSE2_SPRING;
    if ((u32)mode < 4) {
        message_window.x = 346;
        message_window.y = 96;
        message_window.altX = 320;
        message_window.altY = 143;
    } else {
        message_window.x = 320;
        message_window.y = 1;
        message_window.altX = 320;
        message_window.altY = -48;
    }
}

/* Start a scroll: reset its counter and load the sector group of the selected pair's b. */
static void message_scroll_begin(void) {
    message_scroll_counter = 0;
    message_scroll_group = -1;
    farm_load_sector_group(message_pairs[message_index].b);
}

/* Advance the scroll one frame: the window shows the text group of the frame (a new group every 16
 * frames, three in each 48-frame cycle), and the group is loaded when it changes. */
static void message_scroll_tick(void) {
    s32 adjusted;
    s32 counter;
    s32 group;
    s32 next_counter;
    counter = message_scroll_counter;
    adjusted = counter;
    if (counter < 0) {
        adjusted = counter + 15;
    }
    group = (adjusted >> 4) - ((counter / 48) * 3);
    if (message_scroll_group != group) {
        message_scroll_group = group;
        texture_pack_load_at((farm_area.sectorBuffer + (group << 11)), message_window.x, message_window.y,
                      message_window.altX, message_window.altY);
    }
    next_counter = message_scroll_counter + 1;
    message_scroll_counter = next_counter;
}

/* End a scroll: restore sector group 0x12 and the window's normal texture. */
static void message_scroll_end(void) {
    farm_load_sector_group(0x12);
    texture_pack_load_at(farm_area.sectorBuffer, message_window.x, message_window.y, message_window.altX, message_window.altY);
}

/* Once a day, release the offer that was accepted or paid and offer the next free slot. It runs
 * once per date (the date stamp is kept in ending.route). */
static void ending_offer_advance(void) {
    u8 *slots;
    u16 stamp;
    s32 selected;
    s32 i;
    s32 state;

    slots = game_state.ending.offers;
    stamp = game_state.progress.clock.date.day + (game_state.progress.clock.date.year * 120 + game_state.progress.clock.date.season * DAYS_PER_SEASON);
    if (stamp == game_state.ending.route) {
        return;
    }
    game_state.ending.route = stamp;

    selected = -1;
    for (i = 0; i < ENDING_OFFER_SLOTS; i++) {
        state = slots[i];
        switch (state) {
        case ENDING_OFFER_OFFERED:
        case ENDING_OFFER_ACCEPTED:
            selected = i;
            slots[i] = ENDING_OFFER_FREE;
            break;
        case ENDING_OFFER_PAID:
            selected = i;
            slots[i] = ENDING_OFFER_CLOSED;
            break;
        }
        if (selected != -1) {
            break;
        }
    }

    if (selected == -1) {
        selected = 0;
    } else {
        selected = (selected + 1) % ENDING_OFFER_SLOTS;
    }
    for (i = selected; i < ENDING_OFFER_SLOTS; i++) {
        if (slots[i] == ENDING_OFFER_FREE) {
            slots[i] = ENDING_OFFER_OFFERED;
            return;
        }
    }
    for (i = 0; i <= selected; i++) {
        if (slots[i] == ENDING_OFFER_FREE) {
            slots[i] = ENDING_OFFER_OFFERED;
            return;
        }
    }
}

/* When the scroll ends on an accepted offer, record it in its slot if the slot is still offered. */
static void ending_offer_confirm(void) {
    s16 value = message_pairs[MESSAGE_PAIR_RESULT].b;

    if (value == ENDING_OFFER_ACCEPTED) {
        u32 index = message_pairs[MESSAGE_PAIR_RESULT].a - ENDING_OFFER_MESSAGE_BASE;
        if (index < ENDING_OFFER_SLOTS) {
            if (game_state.ending.offers[index] == ENDING_OFFER_OFFERED) game_state.ending.offers[index] = value;
        }
    }
}
