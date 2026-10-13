#ifndef CLOCK_H
#define CLOCK_H

/* The farm clock and calendar: minutes, hours, days, seasons and years. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* FarmDate's season and weekday values, in the order the game prints them. */
enum Season {
    SEASON_SPRING,
    SEASON_SUMMER,
    SEASON_FALL,
    SEASON_WINTER
};

enum Weekday {
    WEEKDAY_SUNDAY,
    WEEKDAY_MONDAY,
    WEEKDAY_TUESDAY,
    WEEKDAY_WEDNESDAY,
    WEEKDAY_THURSDAY,
    WEEKDAY_FRIDAY,
    WEEKDAY_SATURDAY
};

/* The part of the day the clock is in: the drift table has one row per part. */
enum ClockPeriod {
    CLOCK_PERIOD_DAWN,      /* 5:00 to 6:59 */
    CLOCK_PERIOD_DAY,       /* 7:00 to 14:59 */
    CLOCK_PERIOD_DUSK,      /* 15:00 to 16:59 */
    CLOCK_PERIOD_NIGHT,     /* the rest of the day */
};

/* The farm calendar. season and weekday hold an enum Season and an enum Weekday;
 * the save file fixes their size at s16. */
typedef struct FarmDate {
    s16 year;           /* capped at 98 */
    s16 season;
    s16 day;            /* 0..29 */
    s16 weekday;
} FarmDate;

/* The clock's landmarks. The day starts at 6:00; noon and 18:00 trigger their
 * events. The minute forms are minuteOfDay stamps. */
enum ClockTime {
    CLOCK_DAY_START_HOUR    = 6,
    CLOCK_NOON_HOUR         = 12,
    CLOCK_DAY_START_MINUTE  = 360,    /* 6:00 */
    CLOCK_NOON_MINUTE       = 720,    /* 12:00 */
    CLOCK_EVENING_MINUTE    = 1080,    /* 18:00 */
    CLOCK_DAY_MINUTES       = 1440,    /* the minutes in a day */
};

/* The sizes of the calendar. */
enum ClockSize {
    MINUTES_PER_HOUR = 60,
    HOURS_PER_DAY = 24,
    DAYS_PER_WEEK = 7,
    DAYS_PER_SEASON = 30,
    SEASONS_PER_YEAR = 4,
    MAX_YEAR = 98,
    CLOCK_EVENT_COUNT = 10,
};

/* The time of day. */
typedef struct FarmTime {
    s16 hour;
    s16 minute;
} FarmTime;

/* FarmClock.time: the hour and minute, also read as one word, so that
 * `word == 6` tests 6:00 exactly. */
typedef union FarmTimeOfDay {
    FarmTime hm;
    s32 word;
} FarmTimeOfDay;

/* How long the day has run. */
typedef struct FarmElapsed {
    s16 hours;          /* hours since 6:00 */
    u16 minutes;        /* minutes into the hour */
} FarmElapsed;

/* The date and the clock. */
typedef struct FarmClock {
    FarmDate date;
    FarmTimeOfDay time;
    FarmElapsed elapsed;
    s16 unused10;
    s16 minuteOfDay;    /* minutes since midnight: 360 is 6:00 */
} FarmClock;

/* The day values the game keeps by meaning (FarmDayCounts.values). */
enum FarmDayValue {
    DAY_VALUE_INVITATION_YEAR = 3,  /* the invitation's year plus one, set when it is issued */
};

/* The day counters (0xFF = stopped, capped at 120). */
typedef struct FarmDayCounts {
    u8  count0[0x80];
    u8  count1[8];
    u8  values[0x80];
} FarmDayCounts;

/* A season and a day, with the value the calendar remaps them to (CLOCK_NO_REMAP: none). */
typedef struct ClockDateRow { s16 season; s16 day; s16 value; } ClockDateRow;

/* One entry of the calendar's event table: a date and a message. */
typedef struct FarmDateEntry {
    u8  season;
    u8  day;
    s16 message;
} FarmDateEntry;

void clock_frame_update(void);
s32 clock_is_pending(void);
enum ClockPeriod clock_time_period(s16 hour);
s32 clock_sleep_until_5am(s32 minutes);
void clock_fast_forward_unflagged(s32 frames);
void clock_reset(void);
void clock_reset_queue(void);
s32 clock_queue_busy(void);
void clock_set_end_flag(s32 flag);
void clock_advance_minute(void);
void clock_run_queue_head(void);
s32 clock_date_index(s16 x, s16 y);
s32 clock_find_date_entry(s16 a, s16 b);
s32 clock_remap_selection(s32 selection);

extern s32 (*const clock_callbacks[CLOCK_EVENT_COUNT])(void);

/* An hour of the day and the event that starts then. */
typedef struct {
    s16 value;
    s8 arg;
    u8 pad[1];
} FarmHourEvent;

void clock_tick_minute(void);

#endif /* CLOCK_H */
