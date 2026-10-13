/* The farm clock: the calendar and time of day, the minute tick, the queue of
 * timed events, and the date flags that follow the calendar. */

#include <memory.h>
#include "types.h"
#include "animals.h"
#include "clock.h"
#include "clock_events.h"
#include "cue_history.h"
#include "estate.h"
#include "event_commands.h"
#include "event_flags.h"
#include "family.h"
#include "farm_data.h"
#include "farm_state.h"
#include "player.h"
#include "screen_fade.h"
#include "villager.h"
#include "weather.h"

#define CLOCK_ANY (-1)
#define CLOCK_NO_REMAP (-1)
#define CLOCK_QUEUE_LENGTH 50
#define CLOCK_QUEUE_EMPTY (-1)
#define CLOCK_WAKE_HOUR 5
#define CLOCK_HISPEED_DIVISOR 8
#define CLOCK_WEEKDAY_FLAG_BASE 0xB
#define CLOCK_SEASON_FLAG_BASE 0x17
#define CLOCK_DAY_FLAG_BASE 0x78
/* The clock periods of clock_time_period: dawn, the day, and the dusk hours. */
#define CLOCK_DAWN_HOUR 5
#define CLOCK_DAWN_HOURS 2
#define CLOCK_DAY_PERIOD_HOUR 7
#define CLOCK_DAY_PERIOD_HOURS 8
#define CLOCK_DUSK_HOUR 15
#define CLOCK_DUSK_HOURS 2
/* clock_advance_minute's rollover bits: the minute ticked, the hour rolled over. */
#define CLOCK_ROLLED_MINUTE 0x8000
#define CLOCK_ROLLED_HOUR 0x4000
/* The place_entries value of a marked place (clock_place_is_marked). */
#define CLOCK_PLACE_MARK 0x1000

/* A clock event: when it is due (each field CLOCK_ANY for any) and what to run. */
typedef struct FarmClockEvent {
    s16 year;
    s16 season;
    s16 day;
    s16 pad6;
    s16 hour;
    s16 minute;
    s16 event;      /* handed to clock_queue_event */
} FarmClockEvent;

static void clock_fast_forward(s32 frames);
/* The fields clock_check_event_due checks, in order: each one falls through to the next. */
enum ClockEventField {
    CLOCK_EVENT_FIELD_MINUTE,
    CLOCK_EVENT_FIELD_HOUR,
    CLOCK_EVENT_FIELD_DAY,
    CLOCK_EVENT_FIELD_SEASON,
    CLOCK_EVENT_FIELD_YEAR,
};

static void clock_check_event_due(FarmClockEvent *args, s32 start);
static void clock_queue_event(s32 event);
static s32 clock_place_is_marked(void);
static void clock_update_date_flags(s16 weekday);
static void clock_set_season_flag(s16 season);

void clock_tick_minute(void) {
    clock_advance_minute();
    clock_run_queue_head();
}

static s32 clock_queue[CLOCK_QUEUE_LENGTH];
static s32 clock_pending;
static u32 clock_stepping;
static s32 clock_fast_forward_left;
static s32 clock_end_flag;

/* Finish a clock fast-forward: tick through the remaining minutes, then reset the
 * schedule (before 6:00) or pick up the day's schedule, and run the overnight finish
 * when it was requested. */
void clock_frame_update(void) {
    s32 count;
    FarmSystem *system;
    if (clock_pending != 0) {
        clock_stepping = 0;
        if (clock_queue_busy() == 0) {
            system = &game_state.system;
            while (clock_fast_forward_left > 0) {
                system->clockFrames = one_minute;
                clock_stepping = 1;
                clock_tick_minute();
                clock_stepping = 0;
                clock_fast_forward_left -= 1;
                if (clock_queue_busy() != 0) {
                    return;
                }
            }

            system->clockFrames = 0;
            clock_pending = 0;
            clock_stepping = 1;
            if (game_state.progress.clock.time.hm.hour < CLOCK_DAY_START_HOUR) {
                count = 0;
                do {
                    event_cmd_reset_schedule(count, 0, 0, 0);
                    count += 1;
                } while (count < 37);
            } else {
                item_select_schedule_step(game_state.progress.clock.minuteOfDay);
                farm_item_history_resume();
            }
            if (clock_end_flag == 1) {
                farm_overnight_finish();
                clock_end_flag = 0;
            }
            drift_select_hour();
        }
    }
}

s32 clock_is_pending(void) {
    return clock_pending;
}

enum ClockPeriod clock_time_period(s16 hour) {
    enum ClockPeriod result = CLOCK_PERIOD_NIGHT;
    if ((u32)(hour - CLOCK_DAWN_HOUR) < CLOCK_DAWN_HOURS) result = CLOCK_PERIOD_DAWN;
    if ((u32)(hour - CLOCK_DAY_PERIOD_HOUR) < CLOCK_DAY_PERIOD_HOURS) result = CLOCK_PERIOD_DAY;
    if ((u32)(hour - CLOCK_DUSK_HOUR) < CLOCK_DUSK_HOURS) result = CLOCK_PERIOD_DUSK;
    return result;
}

/* Sleep until the next 5:00 am: fast-forward the clock by the minutes to get
 * there plus `minutes`, and return the total. */
s32 clock_sleep_until_5am(s32 minutes) {
    s32 hours;
    s32 total;

    hours = CLOCK_WAKE_HOUR - game_state.progress.clock.time.hm.hour;
    if (hours < 0) {
        hours += HOURS_PER_DAY;
    }
    clock_end_flag = 0;
    total = MINUTES_PER_HOUR - game_state.progress.clock.time.hm.minute;
    total += hours * MINUTES_PER_HOUR + minutes;
    clock_fast_forward(total);
    return total;
}

void clock_fast_forward_unflagged(s32 frames) {
    clock_end_flag = 0;
    clock_fast_forward(frames);
}

/* Start the calendar at spring of year 0, day 1, a Monday at 6:00, with the event
 * queue and the fast-forward cleared and the date flags set for that day. */
void clock_reset(void) {
    FarmState *farm;
    farm = &game_state;
    farm->progress.clock.date.day = 1;
    farm->progress.clock.date.weekday = WEEKDAY_MONDAY;
    farm->progress.clock.date.year = 0;
    farm->progress.clock.date.season = SEASON_SPRING;
    farm->progress.clock.time.hm.minute = 0;
    farm->progress.clock.unused10 = 0;
    farm->progress.clock.minuteOfDay = CLOCK_DAY_START_MINUTE;
    farm->system.clockFrames = 0;
    farm->progress.clock.time.hm.hour = CLOCK_DAY_START_HOUR;
    memset(clock_queue, -1, sizeof(clock_queue));
    farm->system.overnightStep = OVERNIGHT_FADE_OUT;
    clock_stepping = 0;
    clock_pending = 0;
    clock_fast_forward_left = 0;
    clock_update_date_flags(farm->progress.clock.date.weekday);
    clock_set_season_flag(farm->progress.clock.date.season);
    if (clock_date_index(farm->progress.clock.date.season, farm->progress.clock.date.day) != 0) {
        event_flag_set(EVENT_FLAG_DATED_EVENT_TODAY, 1);
    }
    event_flag_set(EVENT_FLAG_FIRST_YEAR, 1);
}

/* Empty the event queue and clear the fast-forward state; the calendar and time stay. */
void clock_reset_queue(void) {
    game_state.system.clockFrames = 0;
    memset(clock_queue, -1, sizeof(clock_queue));
    game_state.system.overnightStep = OVERNIGHT_FADE_OUT;
    clock_stepping = 0;
    clock_pending = 0;
    clock_fast_forward_left = 0;
}

/* True while a queued event is waiting to run. */
s32 clock_queue_busy(void) {
    return clock_queue[0] != CLOCK_QUEUE_EMPTY;
}

void clock_set_end_flag(s32 flag) {
    clock_end_flag = flag;
}

/* The timed events, checked every minute: each is due when its minute, hour, day,
 * season and year match (CLOCK_ANY for any), and `event` indexes clock_callbacks. */
static FarmClockEvent clock_events[CLOCK_EVENT_COUNT] = {
    {CLOCK_ANY, CLOCK_ANY, CLOCK_ANY, 0, CLOCK_ANY, 0, 4},
    {CLOCK_ANY, CLOCK_ANY, CLOCK_ANY, 0, 17, 58, 5},
    {CLOCK_ANY, CLOCK_ANY, CLOCK_ANY, 0, 5, 59, 0},
    {CLOCK_ANY, CLOCK_ANY, CLOCK_ANY, 0, 6, 0, 2},
    {CLOCK_ANY, CLOCK_ANY, 29, 0, 5, 59, 1},
    {CLOCK_ANY, CLOCK_ANY, 0, 0, 6, 0, 3},
    {CLOCK_ANY, 3, 10, 0, 6, 0, 6},
    {CLOCK_ANY, 1, 20, 0, 6, 0, 7},
    {CLOCK_ANY, 2, 21, 0, 6, 0, 8},
    {CLOCK_ANY, 0, 18, 0, 6, 0, 9},
};

/* Advance the farm clock one game minute once enough frames have passed, rolling over
 * hours, days, seasons and years, then refresh the date state. Nothing moves while a
 * fade, an event, the pause or a marked place holds the clock (unless a fast-forward
 * is stepping it). */
void clock_advance_minute(void) {
    FarmDate *date;
    FarmTime *time;
    FarmElapsed *totals;
    FarmSystem *system;
    s32 limit;
    u32 changed;
    u32 flags;
    u32 i;

    game_state.globalFlags &= ~(GLOBAL_FLAG_CLOCK_TICKED | GLOBAL_FLAG_HOUR_PASSED | GLOBAL_FLAG_MINUTE_PASSED);
    if (clock_pending == 0) {
        if (fade_is_settled() == 0) {
            return;
        }
        if (event_flag_get(EVENT_FLAG_EVENT_ACTIVE) == 1) {
            return;
        }
        if (game_state.globalFlags & GLOBAL_FLAG_PAUSED) {
            return;
        }
        if (clock_place_is_marked() == 1) {
            return;
        }
    } else if (clock_stepping == 0) {
        return;
    }
    system = &game_state.system;
    date = &game_state.progress.clock.date;
    time = &game_state.progress.clock.time.hm;
    totals = &game_state.progress.clock.elapsed;
    limit = one_minute;
    game_state.globalFlags |= GLOBAL_FLAG_CLOCK_TICKED;
    system->clockFrames++;
    if (game_state.debugFlags & DEBUG_FLAG_HISPEED_TIME) {
        limit = limit / CLOCK_HISPEED_DIVISOR + 1;
    }
    if (system->clockFrames < limit / 2) {
        return;
    }
    changed = CLOCK_ROLLED_MINUTE;
    system->clockFrames = 0;
    time->minute++;
    totals->minutes++;
    game_state.progress.clock.minuteOfDay++;
    game_state.globalFlags |= GLOBAL_FLAG_MINUTE_PASSED;
    if (clock_pending == 0) {
        player_count_idle(&game_state.player);
    }
    flags = game_state.globalFlags & ~GLOBAL_FLAG_HOUR_PASSED;
    game_state.globalFlags = flags;
    if (time->minute >= MINUTES_PER_HOUR) {
        changed = CLOCK_ROLLED_MINUTE | CLOCK_ROLLED_HOUR;
        time->minute = 0;
        totals->minutes = 0;
        game_state.globalFlags = flags | GLOBAL_FLAG_HOUR_PASSED;
        time->hour++;
        totals->hours++;
    }
    if (time->hour >= HOURS_PER_DAY) {
        game_state.progress.clock.minuteOfDay = 0;
        time->hour = 0;
    }
    if (time->hour == CLOCK_DAY_START_HOUR && (changed & CLOCK_ROLLED_HOUR)) {
        totals->hours = 0;
        date->day++;
        date->weekday++;
        if (date->weekday >= DAYS_PER_WEEK) {
            date->weekday = WEEKDAY_SUNDAY;
        }
    }
    if (date->day >= DAYS_PER_SEASON) {
        date->day = 0;
        date->season++;
        farm_actions_clear(1);
    }
    if (date->season >= SEASONS_PER_YEAR) {
        date->season = SEASON_SPRING;
        date->year++;
        event_flag_set(EVENT_FLAG_FIRST_YEAR, 0);
        if (date->year > MAX_YEAR) {
            date->year = MAX_YEAR;
        }
    }
    clock_update_date_flags(game_state.progress.clock.date.weekday);
    event_flag_set(EVENT_FLAG_DATED_EVENT_TODAY, 0);
    event_flag_set(EVENT_FLAG_WEDDING_TODAY, 0);
    clock_set_season_flag(date->season);
    if (clock_date_index(game_state.progress.clock.date.season, game_state.progress.clock.date.day) != 0) {
        event_flag_set(EVENT_FLAG_DATED_EVENT_TODAY, 1);
    }
    for (i = 0; i < CLOCK_EVENT_COUNT; i++) {
        clock_check_event_due(&clock_events[i], CLOCK_EVENT_FIELD_MINUTE);
    }
    event_trigger_mode_set(1);
}

static void clock_fast_forward(s32 frames) {
    clock_pending = 1;
    clock_fast_forward_left = frames;
}

/* The callbacks for queued clock events, indexed by event id. */
s32 (*const clock_callbacks[CLOCK_EVENT_COUNT])(void) = {
    clock_callback_nothing, estate_ledger_roll, scene_overnight_step, estate_season_refresh, farm_hourly_refresh,
    estate_shipment_settle, clock_show_object_4, clock_show_object_1, clock_show_object_2, clock_show_object_0,
};

/* Run the event `args` when it is due: from field `start` on (0 = all five), each
 * of minute, hour, day, season and year is either -1 (any) or must equal the farm
 * clock's; when every checked field agrees, hand args->event to clock_queue_event. */
static void clock_check_event_due(FarmClockEvent *args, s32 start) {
    switch (start) {
    case CLOCK_EVENT_FIELD_MINUTE:
        if (args->minute != CLOCK_ANY && args->minute != game_state.progress.clock.time.hm.minute) {
            return;
        }
    case CLOCK_EVENT_FIELD_HOUR:
        if (args->hour != CLOCK_ANY && args->hour != game_state.progress.clock.time.hm.hour) {
            return;
        }
    case CLOCK_EVENT_FIELD_DAY:
        if (args->day != CLOCK_ANY && args->day != game_state.progress.clock.date.day) {
            return;
        }
    case CLOCK_EVENT_FIELD_SEASON:
        if (args->season != CLOCK_ANY && args->season != game_state.progress.clock.date.season) {
            return;
        }
    case CLOCK_EVENT_FIELD_YEAR:
        if (args->year != CLOCK_ANY && args->year != game_state.progress.clock.date.year) {
            return;
        }
    }
    clock_queue_event(args->event);
}

/* Put `event` in the first free slot of the queue. */
static void clock_queue_event(s32 event) {
    s32 i;
    for (i = 0; i < CLOCK_QUEUE_LENGTH; i++) {
        if (clock_queue[i] == CLOCK_QUEUE_EMPTY) {
            clock_queue[i] = event;
            break;
        }
    }
}

/* Run the queued farm callback; once it reports done, shift the queue down. */
void clock_run_queue_head(void) {
    s32 i;

    if (clock_queue[0] == CLOCK_QUEUE_EMPTY) {
        return;
    }
    if (clock_callbacks[clock_queue[0]]() == 0) {
        return;
    }
    game_state.system.overnightStep = OVERNIGHT_FADE_OUT;
    for (i = 1; i < CLOCK_QUEUE_LENGTH; i++) {
        clock_queue[i - 1] = clock_queue[i];
    }
    clock_queue[CLOCK_QUEUE_LENGTH - 1] = CLOCK_QUEUE_EMPTY;
}

/* True when the current place's place_entries value is the marked one. */
static s32 clock_place_is_marked(void) {
    s16 place = farm_area.mode.data->head.place.location;
    s16 marked = place_entries[place];
    return marked == CLOCK_PLACE_MARK;
}

/* Set the weekday, day, birthday, family-selection and child-birthday flags for today.
 * The family flags are skipped once the player is single, the child's once one is born. */
static void clock_update_date_flags(s16 weekday) {
    u32 i;
    FarmBirthday *childBirthday;
    FarmBirthday *wifeBirthday;
    i = 0;
    do {
        event_flag_set((i + CLOCK_WEEKDAY_FLAG_BASE) & 0xFFFF, 0);
        i++;
    } while (i < DAYS_PER_WEEK);
    event_flag_set((weekday + CLOCK_WEEKDAY_FLAG_BASE) & 0xFFFF, 1);
    i = CLOCK_DAY_FLAG_BASE;
    do {
        event_flag_set(i & 0xFFFF, 0);
        i++;
    } while (i < CLOCK_DAY_FLAG_BASE + DAYS_PER_SEASON);
    event_flag_set((game_state.progress.clock.date.day + CLOCK_DAY_FLAG_BASE) & 0xFFFF, 1);
    event_flag_set(EVENT_FLAG_PLAYER_BIRTHDAY, 0);
    if ((game_state.progress.clock.date.season == game_state.player.birthday.season) &&
        (game_state.progress.clock.date.day == game_state.player.birthday.day))
        event_flag_set(EVENT_FLAG_PLAYER_BIRTHDAY, 1);
    if (event_flag_get(EVENT_FLAG_SINGLE) != 1) {
        event_flag_set(EVENT_FLAG_FAMILY_SELECTION_TODAY, 0);
        if ((game_state.progress.clock.date.season == game_state.family.selection.season) &&
            (game_state.progress.clock.date.day == game_state.family.selection.day))
            event_flag_set(EVENT_FLAG_FAMILY_SELECTION_TODAY, 1);
        event_flag_set(EVENT_FLAG_WIFE_BIRTHDAY, 0);
        wifeBirthday = &game_state.villagers[game_state.family.wifeVillager].birthday;
        if ((game_state.progress.clock.date.season == wifeBirthday->season) &&
            (game_state.progress.clock.date.day == wifeBirthday->day)) {
            event_flag_set(EVENT_FLAG_WIFE_BIRTHDAY, 1);
        }
        if (event_flag_get(EVENT_FLAG_HAS_CHILD) != 1) {
            event_flag_set(EVENT_FLAG_CHILD_BIRTHDAY, 0);
            childBirthday = &game_state.villagers[CHARACTER_CHILD].birthday;
            if ((game_state.progress.clock.date.season == childBirthday->season) &&
                (game_state.progress.clock.date.day == childBirthday->day)) {
                event_flag_set(EVENT_FLAG_CHILD_BIRTHDAY, 1);
            }
        }
    }
}

static void clock_set_season_flag(s16 season) {
    u32 i = 0;
    do {
        event_flag_set(i + CLOCK_SEASON_FLAG_BASE, 0);
        i++;
    } while (i < SEASONS_PER_YEAR);
    event_flag_set(season + CLOCK_SEASON_FLAG_BASE, 1);
}

/* The calendar's special dates: a season and a day, and the scene selection it remaps
 * to (CLOCK_NO_REMAP for none). clock_date_index finds a row by its date. */
static ClockDateRow clock_date_table[18] = {
    {0, 0, CLOCK_NO_REMAP},
    {0, 7, CLOCK_NO_REMAP},
    {0, 17, 222},
    {0, 21, 223},
    {1, 6, 224},
    {1, 11, 225},
    {2, 8, 226},
    {3, 9, 227},
    {1, 0, CLOCK_NO_REMAP},
    {1, 19, CLOCK_NO_REMAP},
    {2, 20, CLOCK_NO_REMAP},
    {1, 23, CLOCK_NO_REMAP},
    {2, 2, CLOCK_NO_REMAP},
    {3, 23, CLOCK_NO_REMAP},
    {3, 29, CLOCK_NO_REMAP},
    {0, 13, CLOCK_NO_REMAP},
    {2, 12, CLOCK_NO_REMAP},
    {3, 13, CLOCK_NO_REMAP},
};

/* The 1-based row of clock_date_table (among its first 14) for the date (season x, day y),
 * or 0 when there is none. */
s32 clock_date_index(s16 x, s16 y) {
    s32 i;
    for (i = 0; i < 14; i++) {
        if (x != clock_date_table[i].season || y != clock_date_table[i].day) continue;
        return i + 1;
    }
    return 0;
}

/* As clock_date_index, over the whole table. */
s32 clock_find_date_entry(s16 x, s16 y) {
    u32 i = 0;
    while (i < sizeof(clock_date_table) / sizeof(clock_date_table[0])) {
        if (x == clock_date_table[i].season && y == clock_date_table[i].day) return i + 1;
        i++;
    }
    return 0;
}

/* Set the wedding-today flag when today is one of the five rival wedding dates
 * or the player's own wedding date, and keep each couple's married flag. */
static void clock_update_event_dates(void) {
    u32 i;

    for (i = 0; i < RIVAL_COUPLE_COUNT; i++) {
        event_flag_set(EVENT_FLAG_KAREN_RICK_WEDDING_TODAY + i, 0);
        if (game_state.progress.clock.date.year == game_state.world.rivalWeddings[i].date.year &&
            game_state.progress.clock.date.season == game_state.world.rivalWeddings[i].date.season &&
            game_state.progress.clock.date.day == game_state.world.rivalWeddings[i].date.day) {
            event_flag_set(EVENT_FLAG_WEDDING_TODAY, 1);
            event_flag_set(EVENT_FLAG_KAREN_RICK_MARRIED + i, 1);
            event_flag_set(EVENT_FLAG_KAREN_RICK_WEDDING_TODAY + i, 1);
        }
    }
    if (game_state.progress.clock.date.year == game_state.player.wedding.date.year) {
        if (game_state.progress.clock.date.season == game_state.player.wedding.date.season) {
            if (game_state.progress.clock.date.day == game_state.player.wedding.date.day) {
                event_flag_set(EVENT_FLAG_WEDDING_TODAY, 1);
            }
        }
    }
}

/* Remap a scene selection in 0x34..0x37 to the calendar's value for today, when today
 * is a special date with one. Other selections pass through. */
s32 clock_remap_selection(s32 selection) {
    s16 selected;
    s32 result;
    s32 table_index;
    s16 *table;
    s16 table_value;

    selected = selection;
    if ((u32)(selected - 0x34) < 4U) {
        result = clock_date_index(game_state.progress.clock.date.season, game_state.progress.clock.date.day);
        if (result != 0) {
            table = &clock_date_table[0].season;
            table_index = (result - 1) * 3;
            table_index += 2;
            table_value = table[table_index];
            if (table_value != CLOCK_NO_REMAP) {
                selected = table_value;
            }
        }
    }
    return selected;
}
