/* 11_GF_FARM milestones: the farm's result counter, its timers and the
 * completion and status flags that gate each stage of the farm's progress. */

#include "types.h"
#include "animals.h"
#include "clock.h"
#include "clut.h"
#include "estate.h"
#include "event_commands.h"
#include "event_flags.h"
#include "farm_state.h"
#include "items.h"
#include "math_util.h"
#include "milestones.h"
#include "player.h"
#include "scene.h"
#include "villager.h"

typedef struct {
    s32 generated[3];
    s32 gap;
    u16 values[3];
} ResultFrame;

static s32 milestone_time_band(s16 hour, s16 seasonHour);
static s16 milestone_season_value(s16 season);
static void milestone_check_group_a(void);
static void milestone_check_group_b(void);
static void milestone_check_group_c(void);
static void milestone_check_blue_grass_shipped(void);
static void milestone_check_green_grass_shipped(void);
static void milestone_set_status_bits(void);
static void milestone_check_affection(void);
static void milestone_set_level_bits(void);
static void milestone_probe_animal_medicine(void);
static void milestone_set_coop_barn_bits(void);
static void milestone_probe_tool_items(void);
static void milestone_probe_miracle_potions(void);

static struct {
    ClutFade fade;
    u8 pad14[12];
} milestone_clut_states;
static u_long milestone_clut_colors[16];
static s32 milestone_timer;
static s32 milestone_result_frame;

#define MILESTONE_RESULT_FRAMES 1200
#define GROUP_SHIPMENT_THRESHOLD 101
#define GRASS_SHIPMENT_THRESHOLD 51
#define MILESTONE_DAWN_HOUR 5
#define MILESTONE_DAWN_HOURS 2
#define MILESTONE_DAY_HOUR 7
#define MILESTONE_BAND_HOURS 2
/* The season hour band's timer runs up to this and stops. */
#define MILESTONE_TIMER_END 113
#define MILESTONE_ORDER_COOP 0x13F
#define MILESTONE_ORDER_BARN 0x140
#define MILESTONE_ORDER_BARN_FULL 0x141
/* The tools' level orders: the five tools (sickle to watering can) each have five levels,
 * the orders run from ITEM_SICKLE up to ITEM_MYSTRILE_WATERING_CAN. */
#define MILESTONE_TOOL_ORDER_FIRST ITEM_SICKLE
#define MILESTONE_TOOL_ORDER_COUNT (ITEM_MYSTRILE_WATERING_CAN - ITEM_SICKLE + 1)
/* The palette fade's span in steps: the day bands run the full timer, the night band 32. */
#define MILESTONE_FADE_SPAN_DAY 113
#define MILESTONE_FADE_SPAN_NIGHT 32
/* The farm's CLUT fade: the palette rows it blends at x MILESTONE_CLUT_X, the day and night
 * rows (each blends into the row below it; the season hour uses the night pair) and the row
 * the blend lands on. */
#define MILESTONE_CLUT_X 320
#define MILESTONE_CLUT_DAY_ROW 0xDF
#define MILESTONE_CLUT_NIGHT_ROW 0xDE
#define MILESTONE_CLUT_DST_ROW 220

/* The band milestone_time_band places the hour in: DAWN is 5 and 6 o'clock, DAY runs
 * from 7 to the season's hour, SEASON_HOUR is that hour and the next, NIGHT is the rest. */
enum MilestoneTimeBand {
  TIME_BAND_DAWN = 0,
  TIME_BAND_DAY = 1,
  TIME_BAND_SEASON_HOUR = 2,
  TIME_BAND_NIGHT = 3,
};

/* Set the result screen's fade level and palette row for the time band, before it shows. */
void milestone_result_show(void)
{
  s16 seasonHour;
  s32 value;
  s32 clutRow;

  milestone_timer = 0;
  seasonHour = milestone_season_value(game_state.progress.clock.date.season);
  switch (milestone_time_band(game_state.progress.clock.time.hm.hour, seasonHour))
  {
  case TIME_BAND_SEASON_HOUR:
    milestone_timer = (game_state.progress.clock.time.hm.hour - seasonHour) * MINUTES_PER_HOUR + game_state.progress.clock.time.hm.minute;
    if (milestone_timer >= MILESTONE_TIMER_END)
      milestone_timer = MILESTONE_TIMER_END;
    value = MILESTONE_FADE_SPAN_DAY;
    clutRow = MILESTONE_CLUT_DAY_ROW;
    break;
  case TIME_BAND_DAWN:
  case TIME_BAND_DAY:
    value = MILESTONE_FADE_SPAN_DAY;
    clutRow = MILESTONE_CLUT_DAY_ROW;
    break;
  case TIME_BAND_NIGHT:
    value = MILESTONE_FADE_SPAN_NIGHT;
    clutRow = MILESTONE_CLUT_NIGHT_ROW;
    break;
  default:
    clutRow = 0;
    value = 0;
    break;
  }
  clut_fade_setup(&milestone_clut_states.fade, milestone_clut_colors, value, MILESTONE_CLUT_X, clutRow, MILESTONE_CLUT_X, clutRow - 1, MILESTONE_CLUT_X, MILESTONE_CLUT_DST_ROW);
}

/* Step the palette fade for the time band: hold it in the dawn and day, run it through
 * the season hour (which ends on the night pair), and scroll it through the night. */
void milestone_timer_tick(void) {
    s32 band;
    s16 seasonHour;
    s32 current;
    seasonHour = milestone_season_value(game_state.progress.clock.date.season);
    band = milestone_time_band(game_state.progress.clock.time.hm.hour, seasonHour);
    switch (band) {
    case TIME_BAND_DAWN:
    case TIME_BAND_DAY:
        clut_fade_step(&milestone_clut_states.fade, 0);
        break;
    case TIME_BAND_SEASON_HOUR:
        current = milestone_timer;
        if (current < MILESTONE_TIMER_END) {
            clut_fade_step(&milestone_clut_states.fade, current);
            milestone_timer = (game_state.progress.clock.time.hm.hour - seasonHour) * MINUTES_PER_HOUR + game_state.progress.clock.time.hm.minute;
            if (milestone_timer >= MILESTONE_TIMER_END) {
                milestone_timer = MILESTONE_TIMER_END;
            }
        } else if (current == MILESTONE_TIMER_END) {
            clut_fade_setup(&milestone_clut_states.fade, milestone_clut_colors, MILESTONE_FADE_SPAN_NIGHT, MILESTONE_CLUT_X, MILESTONE_CLUT_NIGHT_ROW, MILESTONE_CLUT_X, MILESTONE_CLUT_NIGHT_ROW - 1, MILESTONE_CLUT_X, MILESTONE_CLUT_DST_ROW);
            milestone_timer = milestone_timer + 1;
        }
        break;
    case TIME_BAND_NIGHT:
        clut_scroll_step(&milestone_clut_states.fade, 1, 1);
        break;
    }
}

static s32 milestone_time_band(s16 hour, s16 seasonHour) {
    s16 upper = seasonHour + MILESTONE_BAND_HOURS;
    s32 result = TIME_BAND_NIGHT;
    if ((u16)(hour - MILESTONE_DAWN_HOUR) < MILESTONE_DAWN_HOURS) result = TIME_BAND_DAWN;
    if (hour >= MILESTONE_DAY_HOUR && hour < seasonHour) result = TIME_BAND_DAY;
    if (hour >= seasonHour && hour < upper) result = TIME_BAND_SEASON_HOUR;
    return result;
}

/* The season's band hour (the SEASON_HOUR band starts there): 18, 19, 18 and 17 o'clock. */
static s16 milestone_season_hours[4] = {18, 19, 18, 17};

static s16 milestone_season_value(s16 season) {
    return milestone_season_hours[season];
}

static s32 milestone_result_start[3] = {-12418, 4187, 5739};

/* Start the result counter: its position goes to the first result values. */
void milestone_result_begin(void) {
    u16 values[3];
    values[0] = milestone_result_start[0];
    values[1] = milestone_result_start[1];
    values[2] = milestone_result_start[2];
    milestone_result_frame = 0;
    scene_entry_set_position(0, values);
}

static s32 milestone_result_end[3] = {
    -13424, -1077, 6688,
};

/* Advance the farm result counter and submit the next result values. */
void milestone_result_step(void) {
    ResultFrame frame;
    s32 count;

    if (event_flag_get(EVENT_FLAG_MILESTONE_RESULT_RUNNING) == 1) {
        math_lerp_vec3(milestone_result_frame, MILESTONE_RESULT_FRAMES, milestone_result_start, milestone_result_end, frame.generated);
        frame.values[0] = frame.generated[0];
        frame.values[1] = frame.generated[1];
        frame.values[2] = frame.generated[2];
        scene_entry_set_position(0, frame.values);
        count = milestone_result_frame;
        if (count >= MILESTONE_RESULT_FRAMES) {
            event_flag_set(EVENT_FLAG_MILESTONE_RESULT_RUNNING, 0);
        } else {
            milestone_result_frame = count + 1;
        }
    }
}

/* Clear the carpenter order, its timer and the build countdown. */
void milestone_reset_timers(void) {
    milestone_refresh();
    game_state.progress.estate.orderTimer = 0;
    game_state.progress.estate.carpenterOrder = 0;
    game_state.progress.estate.stampTimer = 0;
}

/* Post a carpenter order and start its timer: the coop and barn orders get 4, the
 * accessories and tools 2 (the rest none), and the milestone timer flag is set. */
void milestone_set_timer(s16 order) {
    s32 timer = ((u32)(order - MILESTONE_ORDER_COOP) < 3U) * 4;
    if ((u32)(order - ITEM_BRACELET) < 3U) timer = 2;
    if ((u32)(order - MILESTONE_TOOL_ORDER_FIRST) < MILESTONE_TOOL_ORDER_COUNT) timer = 2;
    if (timer == 0) game_state.progress.estate.orderTimer = 0;
    game_state.progress.estate.carpenterOrder = order;
    game_state.progress.estate.orderTimer = timer;
    event_flag_set(EVENT_FLAG_MILESTONE_TIMER, 1);
}

/* Start the build countdown: milestone_tick stamps the build when it runs out. */
void milestone_arm_countdown(void) {
    game_state.progress.estate.stampTimer = 3;
}

void milestone_refresh(void) {
    milestone_refresh_all();
}

/* Recompute every derived status bit from the estate, the villagers, the shipments
 * and the player's items. */
void milestone_refresh_all(void) {
    milestone_check_group_a();
    milestone_check_group_b();
    milestone_check_group_c();
    milestone_set_status_bits();
    milestone_check_affection();
    milestone_set_level_bits();
    milestone_probe_animal_medicine();
    milestone_check_blue_grass_shipped();
    milestone_check_green_grass_shipped();
    milestone_set_coop_barn_bits();
    milestone_probe_tool_items();
    milestone_probe_miracle_potions();
}

/* Per-frame farm timers. The order timer counts down and clears
 * EVENT_FLAG_MILESTONE_TIMER when it runs out; while EVENT_FLAG_BUILD_IN_PROGRESS
 * is set, the stamp timer counts down, and when it runs out the blocked animals go
 * back, the build is stamped and the next building level is set. The rucksack timer
 * counts down to zero. */
void milestone_tick(void)
{
    s32 i;

    if (game_state.progress.estate.orderTimer == 0 || --game_state.progress.estate.orderTimer < 0) {
        event_flag_set(EVENT_FLAG_MILESTONE_TIMER, 0);
        game_state.progress.estate.orderTimer = 0;
    }
    if (event_flag_get(EVENT_FLAG_BUILD_IN_PROGRESS) == 1 && --game_state.progress.estate.stampTimer < 0) {
        farm_put_back_blocked_animals();
        estate_trigger_stamp_first();
        estate_trigger_stamp_second();
        event_flag_set(EVENT_FLAG_BUILD_IN_PROGRESS, 0);
        game_state.progress.estate.stampTimer = 0;
        for (i = EVENT_FLAG_COOP_LEVEL_1; i < EVENT_FLAG_GREENHOUSE_LEVEL_1; i++) {
            if (event_flag_get(i) == 0) {
                break;
            }
        }
        switch (i) {
        case EVENT_FLAG_COOP_LEVEL_1:
            game_state.progress.estate.coopLevel = 1;
            break;
        case EVENT_FLAG_HOUSE_LEVEL_1:
            game_state.progress.estate.houseLevel = 1;
            break;
        case EVENT_FLAG_BARN_LEVEL_1:
            game_state.progress.estate.barnLevel = 1;
            break;
        case EVENT_FLAG_HOUSE_LEVEL_2:
            game_state.progress.estate.houseLevel = 2;
            break;
        case EVENT_FLAG_GREENHOUSE_LEVEL_1:
            game_state.progress.estate.greenhouseLevel = 1;
            game_state.progress.estate.estateFlags |= ESTATE_FLAG_GREENHOUSE;
            event_flag_set(EVENT_FLAG_GREENHOUSE_BUILT, 1);
            break;
        }
        event_flag_set(i, 1);
        if (game_state.animals.dog.actor.place.location == PLACE_2) {
            house_door_position(&game_state.animals.dog.actor.position);
        }
    }
    if (game_state.progress.estate.rucksackTimer != 0 && --game_state.progress.estate.rucksackTimer < 0) {
        game_state.progress.estate.rucksackTimer = 0;
    }
}

/* Report the carpenter order for `mode`: ORDER_STATUS_PENDING says whether one is waiting,
 * READ returns it, and APPLY builds it (coop, barn or full barn), refreshes the status
 * bits and clears the order. */
s32 milestone_status_query(s32 mode) {
    u16 result;

    result = 0;
    switch (mode) {
    case ORDER_STATUS_PENDING:
        return game_state.progress.estate.carpenterOrder != 0;
    case ORDER_STATUS_READ:
        result = game_state.progress.estate.carpenterOrder;
        break;
    case ORDER_STATUS_APPLY:
        result = game_state.progress.estate.carpenterOrder;
        switch (game_state.progress.estate.carpenterOrder) {
        case MILESTONE_ORDER_COOP:
            result = 0;
            game_state.progress.estate.coopLevel = mode;
            game_state.progress.estate.estateFlags = game_state.progress.estate.estateFlags | ESTATE_FLAG_COOP_BUILT;
            milestone_refresh_all();
            break;
        case MILESTONE_ORDER_BARN:
            result = 0;
            game_state.progress.estate.barnLevel = mode;
            game_state.progress.estate.estateFlags = game_state.progress.estate.estateFlags | ESTATE_FLAG_BARN_UPGRADED;
            milestone_refresh_all();
            break;
        case MILESTONE_ORDER_BARN_FULL:
            result = 0;
            game_state.progress.estate.barnLevel = 3;
            game_state.progress.estate.estateFlags = game_state.progress.estate.estateFlags | ESTATE_FLAG_BARN_FULL;
            milestone_refresh_all();
            break;
        }
        game_state.progress.estate.carpenterOrder = 0;
        break;
    }
    return result;
}

/* The shipment groups: each lists four item ids; a group is done when all four have
 * shipped at least GROUP_SHIPMENT_THRESHOLD. */
static s16 milestone_group_a_ids[4] = {ITEM_TURNIP, ITEM_POTATO, ITEM_CUCUMBER, ITEM_CABBAGE};

/* Clear the group A flag, test its four crops, and set the flag when all pass. */
static void milestone_check_group_a(void) {
    s32 index;
    s16 *value;
    u32 *flags;

    index = 0;
    value = milestone_group_a_ids;
    flags = &game_state.progress.estate.statusFlags;
    *flags &= ~ESTATE_STATUS_GROUP_A_DONE;
    while (index < 4 && estate_shipment_count(*value) >= GROUP_SHIPMENT_THRESHOLD) {
        value++;
        index++;
    }
    if (index >= 4) *flags |= ESTATE_STATUS_GROUP_A_DONE;
}

static s16 milestone_group_b_ids[4] = {ITEM_TOMATO, ITEM_CORN, ITEM_ONION, ITEM_PINEAPPLE};

/* Clear the group B flag, test its four crops, and set the flag when all pass. */
static void milestone_check_group_b(void) {
    s32 index;
    s16 *value;
    u32 *flags;

    index = 0;
    value = milestone_group_b_ids;
    flags = &game_state.progress.estate.statusFlags;
    *flags &= ~ESTATE_STATUS_GROUP_B_DONE;
    while (index < 4 && estate_shipment_count(*value) >= GROUP_SHIPMENT_THRESHOLD) {
        value++;
        index++;
    }
    if (index >= 4) *flags |= ESTATE_STATUS_GROUP_B_DONE;
}

/* Entries past the first four are unused. */
static s16 milestone_group_c_ids[34] = {
    75, 76, 77, 79, 10, 32, 49, 36, 45, -1, 0, 45, 45, -1, 4, 43, 43, 40, -1, 0, 12, 32, 49,
    56, -1, 0, 15, 46, 47, 52, 49, 40, -1, 0,
};

/* Clear the group C flag, test its first four ids, and set the flag when all pass. */
static void milestone_check_group_c(void) {
    s32 index;
    s16 *value;
    u32 *flags;

    index = 0;
    value = milestone_group_c_ids;
    flags = &game_state.progress.estate.statusFlags;
    *flags &= ~ESTATE_STATUS_GROUP_C_DONE;
    while (index < 4 && estate_shipment_count(*value) >= GROUP_SHIPMENT_THRESHOLD) {
        value++;
        index++;
    }
    if (index >= 4) *flags |= ESTATE_STATUS_GROUP_C_DONE;
}

static void milestone_check_blue_grass_shipped(void) {
    u32 *flags = &game_state.progress.estate.statusFlags;

    *flags &= ~ESTATE_STATUS_SHIPPED_5D;
    if (estate_shipment_count(ITEM_BLUE_GRASS) >= GRASS_SHIPMENT_THRESHOLD) *flags |= ESTATE_STATUS_SHIPPED_5D;
}

static void milestone_check_green_grass_shipped(void) {
    u32 *flags = &game_state.progress.estate.statusFlags;
    s32 value;

    *flags &= ~ESTATE_STATUS_SHIPPED_5F;
    value = estate_shipment_count(ITEM_GREEN_GRASS);
    if (value >= GRASS_SHIPMENT_THRESHOLD) {
        *flags |= ESTATE_STATUS_SHIPPED_5F;
    }
}

/* Set the basket and rucksack bits: no basket, no rucksack, or a rucksack upgrade that
 * can be bought (not yet level 2, and its timer has run out). */
static void milestone_set_status_bits(void) {
    u32 *p = &game_state.progress.estate.statusFlags;
    *p &= ~(ESTATE_STATUS_NO_BASKET | ESTATE_STATUS_NO_RUCKSACK | ESTATE_STATUS_RUCKSACK_UPGRADE_OPEN);
    if ((game_state.progress.estate.estateFlags & ESTATE_FLAG_BASKET) == 0) *p |= ESTATE_STATUS_NO_BASKET;
    if (game_state.player.rucksackLevel == 0) {
        *p |= ESTATE_STATUS_NO_RUCKSACK;
        return;
    }
    if (game_state.player.rucksackLevel != 2 && game_state.progress.estate.rucksackTimer <= 0) *p |= ESTATE_STATUS_RUCKSACK_UPGRADE_OPEN;
}

/* Set the overall affection bit when any villager's affection is over 50000. The check is
 * skipped while ESTATE_FLAG_BIT21 is set, and the bit then stays clear. */
static void milestone_check_affection(void) {
    u32 *f = &game_state.progress.estate.statusFlags;
    *f &= ~ESTATE_STATUS_AFFECTION_OVER_50000;
    if (!(game_state.progress.estate.estateFlags & ESTATE_FLAG_BIT21)) {
        FarmVillager *p = game_state.villagers;
        u16 limit = 50000;
        s32 n = 36;
        do {
            if (limit < p->affection) *f |= ESTATE_STATUS_AFFECTION_OVER_50000;
            p++;
            n--;
        } while (n >= 0);
    }
}

/* Set the building bits for the first building not yet in place: no coop, then no house,
 * then no barn; with those in place, house level 1, or no greenhouse. */
static void milestone_set_level_bits(void) {
    u32 *p = &game_state.progress.estate.statusFlags;
    u32 v = *p & ~(ESTATE_STATUS_NO_COOP | ESTATE_STATUS_NO_HOUSE | ESTATE_STATUS_NO_BARN | ESTATE_STATUS_HOUSE_LEVEL_1 | ESTATE_STATUS_NO_GREENHOUSE);
    *p = v;
    if (game_state.progress.estate.coopLevel == 0) {
        *p = v | ESTATE_STATUS_NO_COOP;
        return;
    }
    if (game_state.progress.estate.houseLevel == 0) {
        *p = v | ESTATE_STATUS_NO_HOUSE;
        return;
    }
    if (game_state.progress.estate.barnLevel == 0) {
        *p = v | ESTATE_STATUS_NO_BARN;
        return;
    }
    if (game_state.progress.estate.houseLevel == 1) {
        *p = v | ESTATE_STATUS_HOUSE_LEVEL_1;
        return;
    }
    if (game_state.progress.estate.greenhouseLevel == 0) {
        *p = v | ESTATE_STATUS_NO_GREENHOUSE;
    }
}

/* Set NO_ANIMAL_MEDICINE when the player has no room for the medicine. */
static void milestone_probe_animal_medicine(void) {
    u32 *flags = &game_state.progress.estate.statusFlags;

    *flags &= ~ESTATE_STATUS_NO_ANIMAL_MEDICINE;
    if (event_item_can_take(ITEM_ANIMAL_MEDICINE, 0, 2, 0) == 0) *flags |= ESTATE_STATUS_NO_ANIMAL_MEDICINE;
}

/* Set the order-open bits. The coop order opens once a coop is set but not built. Once the
 * coop is built, the barn upgrade order opens, then the full-barn order after the upgrade. */
static void milestone_set_coop_barn_bits(void) {
    u32 cleared;
    u32 flags;
    u32 *status;
    status = &game_state.progress.estate.statusFlags;
    cleared = (*status = (*status) & ~(ESTATE_STATUS_BARN_ORDER_OPEN | ESTATE_STATUS_COOP_ORDER_OPEN | ESTATE_STATUS_BARN_FULL_ORDER_OPEN));
    if (game_state.progress.estate.coopLevel != 0) {
        if ((game_state.progress.estate.estateFlags & ESTATE_FLAG_COOP_BUILT) == 0) {
            cleared |= ESTATE_STATUS_COOP_ORDER_OPEN;
            *status = cleared;
            return;
        }
    }
    if (game_state.progress.estate.barnLevel == 0) {
        return;
    }
    flags = game_state.progress.estate.estateFlags;
    if ((flags & ESTATE_FLAG_COOP_BUILT) == 0) {
        return;
    }
    if ((flags & ESTATE_FLAG_BARN_UPGRADED) == 0) {
        *status |= ESTATE_STATUS_BARN_ORDER_OPEN;
        return;
    }
    if ((flags & ESTATE_FLAG_BARN_FULL) != 0) {
        return;
    }
    *status |= ESTATE_STATUS_BARN_FULL_ORDER_OPEN;
}

/* Set NO_BRUSH, NO_MILKER and NO_CLIPPERS for each tool the player has no room for. */
static void milestone_probe_tool_items(void) {
    u32 *flags = &game_state.progress.estate.statusFlags;

    *flags &= ~(ESTATE_STATUS_NO_BRUSH | ESTATE_STATUS_NO_MILKER | ESTATE_STATUS_NO_CLIPPERS);
    if (event_item_can_take(ITEM_BRUSH, 0, 2, 0) == 0) {
        *flags |= ESTATE_STATUS_NO_BRUSH;
    }
    if (event_item_can_take(ITEM_MILKER, 0, 2, 0) == 0) {
        *flags |= ESTATE_STATUS_NO_MILKER;
    }
    if (event_item_can_take(ITEM_CLIPPERS, 0, 2, 0) == 0) {
        *flags |= ESTATE_STATUS_NO_CLIPPERS;
    }
}

/* Set NO_COW_MIRACLE_POTION and NO_SHEEP_MIRACLE_POTION when the player has no room for each. */
static void milestone_probe_miracle_potions(void) {
    u32 *flags = &game_state.progress.estate.statusFlags;

    *flags &= ~(ESTATE_STATUS_NO_COW_MIRACLE_POTION | ESTATE_STATUS_NO_SHEEP_MIRACLE_POTION);
    if (event_item_can_take(ITEM_COW_MIRACLE_POTION, 0, 2, 0) == 0) *flags |= ESTATE_STATUS_NO_COW_MIRACLE_POTION;
    if (event_item_can_take(ITEM_SHEEP_MIRACLE_POTION, 0, 2, 0) == 0) *flags |= ESTATE_STATUS_NO_SHEEP_MIRACLE_POTION;
}
