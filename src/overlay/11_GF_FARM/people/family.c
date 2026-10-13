/* The family: the wife and child, the daily update of the family, its pregnancy and the child's growth. */
#include <memory.h>
#include "types.h"
#include "event_flags.h"
#include "family.h"
#include "farm_state.h"
#include "villager.h"

static void family_pregnancy_tick(FarmVillager *wife);
static void family_child_grow(FarmVillager *unused);
static void family_streak_count(FarmVillager *unused);
static void family_wife_tick(FarmVillager *wife);

/* The wife's pregnancy starts once her affection is past this. */
#define FAMILY_PREGNANCY_AFFECTION 64999
/* Ticks of the pregnancy before pregnancyDays starts to count. */
#define FAMILY_PREGNANCY_TICKS 30
/* The wife asks for things while her affection is at most this. */
#define FAMILY_WIFE_REQUEST_AFFECTION 15000
/* The wife's requests stop counting at this many. */
#define FAMILY_WIFE_REQUEST_MAX 3
/* The child's growth stops at this many days. */
#define FAMILY_CHILD_GROWTH_MAX 300
/* The child reaches level 1 and level 2 at these many days. */
#define FAMILY_CHILD_LEVEL_1_DAYS 60
#define FAMILY_CHILD_LEVEL_2_DAYS 120

/* Reset the family to no wife (wifeIndex -1) and set the single flag. The memset's
 * size and fill arguments are swapped, so it clears nothing. */
void family_reset(void) {
    memset(&game_state.family, sizeof(game_state.family), 0);
    game_state.family.wifeIndex = -1;
    event_flag_set(EVENT_FLAG_SINGLE, 1);
}

/* Record the bride's villager id and index (Karen, Ann, Elli and Mary map to 0..3,
 * anything else to 4), then clear the address mode and copy the date to selection. */
void family_set_bride(s16 mode) {
    game_state.family.wifeVillager = mode;
    if (mode == CHARACTER_KAREN) {
        game_state.family.wifeIndex = 0;
    } else if (mode == CHARACTER_ANN) {
        game_state.family.wifeIndex = 1;
    } else if (mode == CHARACTER_ELLI) {
        game_state.family.wifeIndex = 2;
    } else if (mode == CHARACTER_MARY) {
        game_state.family.wifeIndex = 3;
    } else {
        game_state.family.wifeIndex = 4;
    }
    game_state.family.addressMode = 0;
    game_state.family.selection = game_state.progress.clock.date;
}

/* The child is born today: her birthday is the clock's date, and the pregnancy ends. */
void family_child_born(void) {
    game_state.villagers[CHARACTER_CHILD].birthday.season = game_state.progress.clock.date.season;
    game_state.villagers[CHARACTER_CHILD].birthday.day = game_state.progress.clock.date.day;
    event_flag_set(EVENT_FLAG_PREGNANT, 0);
    game_state.family.pregnancyDays = 0;
}

/* The daily update, once married: the wife's streak, pregnancy and requests, and the
 * child's growth once she is born. */
void family_daily_update(void) {
    FarmVillager *item;

    if (event_flag_get(EVENT_FLAG_PLAYER_MARRIED) != 0) {
        item = &game_state.villagers[game_state.family.wifeVillager];
        family_streak_count(item);
        family_pregnancy_tick(item);
        family_wife_tick(item);
        if (event_flag_get(EVENT_FLAG_HAS_CHILD) != 0) {
            item = &game_state.villagers[CHARACTER_CHILD];
            family_child_grow(item);
        }
    }
}

/* Per wife index, an address in the overlay's data (family_table_lookup reads it). */
u32 family_tables[5] = {0x80127F98, 0x80127FA4, 0x80127FAC, 0x80127FB8, 0x80127FC4};

/* The family_tables entry of the current wife. */
s32 family_table_lookup(void) {
    return family_tables[game_state.family.wifeIndex];
}

/* Count the pregnancy: after FAMILY_PREGNANCY_TICKS ticks the days count up, and the
 * pregnancy starts once the wife's affection is past FAMILY_PREGNANCY_AFFECTION. */
static void family_pregnancy_tick(FarmVillager *wife) {
    if (event_flag_get(EVENT_FLAG_HAS_CHILD) == 1) return;
    if (game_state.family.pregnancyTicks < FAMILY_PREGNANCY_TICKS) {
        game_state.family.pregnancyTicks++;
        return;
    }
    if (event_flag_get(EVENT_FLAG_PREGNANT) == 1) {
        game_state.family.pregnancyDays++;
        return;
    }
    if (FAMILY_PREGNANCY_AFFECTION < wife->affection) {
        event_flag_set(EVENT_FLAG_PREGNANT, 1);
    }
}

/* Takes the villager for symmetry with its siblings and never reads it. The child
 * ages one day (up to FAMILY_CHILD_GROWTH_MAX) and her growth level flags follow. */
static void family_child_grow(FarmVillager *unused) {
    u16 count;
    if (game_state.family.childGrowthDays <= FAMILY_CHILD_GROWTH_MAX) {
        event_flag_set(EVENT_FLAG_CHILD_LEVEL_0, 0);
        event_flag_set(EVENT_FLAG_CHILD_LEVEL_1, 0);
        event_flag_set(EVENT_FLAG_CHILD_LEVEL_2, 0);
        if (game_state.family.childGrowthDays < FAMILY_CHILD_GROWTH_MAX) game_state.family.childGrowthDays++;
        count = game_state.family.childGrowthDays;
        if (count >= FAMILY_CHILD_LEVEL_2_DAYS) {
            game_state.family.childLevel = 2;
            event_flag_set(EVENT_FLAG_CHILD_LEVEL_2, 1);
        } else if (count >= FAMILY_CHILD_LEVEL_1_DAYS) {
            game_state.family.childLevel = 1;
            event_flag_set(EVENT_FLAG_CHILD_LEVEL_1, 1);
        } else {
            event_flag_set(EVENT_FLAG_CHILD_LEVEL_0, 1);
        }
    }
}

/* Takes the villager for symmetry with its siblings and never reads it. Counts the
 * days the wife has been ignored, and clears the day's talk flags. */
static void family_streak_count(FarmVillager *unused) {
    if (event_flag_get(EVENT_FLAG_WIFE_TALKED_TODAY) == 0) {
        game_state.family.wifeIgnoredDays++;
    } else {
        game_state.family.wifeIgnoredDays = 0;
    }
    event_flag_set(EVENT_FLAG_WIFE_TALKED_TODAY, 0);
    event_flag_set(EVENT_FLAG_4C, 0);
}

/* Once a day, while the wife's affection is at most FAMILY_WIFE_REQUEST_AFFECTION and
 * she has had no dated event today: raise her request and count it, up to the max. */
static void family_wife_tick(FarmVillager *wife) {
    u16 requests;

    if (wife->affection <= FAMILY_WIFE_REQUEST_AFFECTION) {
        if (event_flag_get(EVENT_FLAG_DATED_EVENT_TODAY) == 0) {
            event_flag_set(EVENT_FLAG_WIFE_REQUEST_PENDING, 1);
            event_flag_set(EVENT_FLAG_AF, 1);
            requests = game_state.family.wifeRequests;
            if (requests < FAMILY_WIFE_REQUEST_MAX) game_state.family.wifeRequests = requests + 1;
        }
    }
}
