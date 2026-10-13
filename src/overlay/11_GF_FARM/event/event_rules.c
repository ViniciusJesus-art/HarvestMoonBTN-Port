/* The rules of the farm's event scripts: the conditions an event checks before it starts
 * (time, place, affection, the day's items) and the scripts that open the doors or put the
 * farm into a timed state. */

#include <abs.h>

#include "types.h"
#include "clock.h"
#include "doors.h"
#include "event_commands.h"
#include "event_flags.h"
#include "event_rules.h"
#include "farm_init.h"
#include "farm_state.h"
#include "items.h"
#include "player.h"
#include "villager.h"

/* One rule of a farm event script: two conditions, each a command with its value.
 * command0 and command1 are enum FarmRuleCommand values. If command1 is
 * RULE_CMD_NONE, the first condition alone decides. */
typedef struct FarmCommandPair {
    u8 command0;
    u8 command1;
    u16 operand0;
    u16 operand1;
    u16 result;
} FarmCommandPair;

/* A farm event script: the rules event_script_try_rules tries in turn for event `id`. */
typedef struct FarmScript {
    s16 id;
    s16 count;
    FarmCommandPair *rules;
} FarmScript;

/* The items day_lookup_has_item looks up on one day (x season, y day). */
typedef struct FarmLookupEntry {
    s16 x;
    s16 y;
    s32 count;
    s16 *values;
} FarmLookupEntry;

static s32 event_script_try_rules(FarmScript *script);
static s32 event_rule_eval(FarmCommandPair *pair);
static s32 event_command_eval(s32 command, s32 value);
static s32 villager_not_in_area(s32 idx, s32 val);
static s32 villager_affection_grade(s32 slot, u32 mode);
static s32 event_time_check(s32 command);
static s32 event_condition_test(s32 id);
static s32 hour_outside_range(s32 start, s32 stop);
static s32 event_day_item_check(s32 item);
static s32 day_lookup_has_item(s32 item, s16 x, s16 y);

/* The results a rule or script can name: the default result of a rule, and the one the
 * miracle potion's screen gives. */
enum EventRuleResult {
    EVENT_RESULT_DEFAULT = 0x1CD,
    EVENT_RESULT_MIRACLE_POTION = 0x209,
};

/* The command of a rule's condition: what its value is tested against. */
enum FarmRuleCommand {
    RULE_CMD_VILLAGER_NOT_IN_AREA = 0,
    RULE_CMD_AFFECTION_GRADE = 1,
    RULE_CMD_TIME_WINDOW = 2,
    RULE_CMD_EVENT_CONDITION = 3,
    RULE_CMD_NONE = 0xFF,
};

/* The time windows a RULE_CMD_TIME_WINDOW value picks, in hours. */
enum FarmTimeWindow {
    TIME_WINDOW_9_12 = 0,
    TIME_WINDOW_9_16 = 1,
    TIME_WINDOW_9_17 = 2,
    TIME_WINDOW_9_16_2 = 3,
    TIME_WINDOW_13_16 = 4,
    TIME_WINDOW_9_18 = 5,
    TIME_WINDOW_8_21 = 6,
    TIME_WINDOW_18_24 = 7,
    TIME_WINDOW_11_17 = 8,
};

/* The checks a villager affection rule makes (its mode, the low byte of the value). */
enum FarmAffectionCheck {
    AFFECTION_HOUR_BY_GRADE = 0,
    AFFECTION_AT_MOST_LOW = 1,
    AFFECTION_AT_MOST_HIGH = 2,
    AFFECTION_LOW_OR_OUTSIDE_18_24 = 3,
    AFFECTION_NEVER = 4,
};

/* The conditions a RULE_CMD_EVENT_CONDITION value tests. */
enum FarmRuleCondition {
    RULE_COND_POPURI_MARRIED_OR_KAI_MARRIED = 0,
    RULE_COND_KAREN_RICK_OR_POPURI_KAI_WEDDING_TODAY = 1,
    RULE_COND_FLAG_184_ALONE = 2,
    RULE_COND_ANN_CLIFF_WEDDING_TODAY = 3,
    RULE_COND_KAREN_RICK_WEDDING_TODAY = 4,
    RULE_COND_SUNDAY = 5,
    RULE_COND_POPURI_MARRIED = 6,
    RULE_COND_FALL_4 = 7,
    RULE_COND_DOG_PLACED_AND_HELD = 8,
    RULE_COND_WINTER_23_SINGLE_KAREN_RICK = 9,
    RULE_COND_WINTER_23_SINGLE_ANN_CLIFF = 10,
    RULE_COND_WINTER_23_SINGLE_ELLI_DOCTOR = 11,
    RULE_COND_WINTER_23_SINGLE_MARY_GRAY = 12,
    RULE_COND_WINTER_23_SINGLE_POPURI_KAI = 13,
    RULE_COND_FALL_2 = 14,
    RULE_COND_SPRING_0 = 15,
    RULE_COND_KAREN_MARRIED = 16,
    RULE_COND_ANN_MARRIED = 17,
    RULE_COND_MARY_MARRIED = 18,
    RULE_COND_ANN_BIRTHDAY_INVITATION = 19,
    RULE_COND_FLAG_19B_NOT_MON_WED = 20,
    RULE_COND_POPURI_KAI_WEDDING_TODAY = 21,
};

/* The hour Ann's birthday invitation is open until: 10:00 with an invitation held (or takeable),
 * 18:00 without. */
enum AnnBirthdayLimit {
    ANN_BIRTHDAY_LIMIT_WITH_INVITATION = 10,
    ANN_BIRTHDAY_LIMIT_WITHOUT_INVITATION = 18,
};

/* The condition id that was met last, or -1 when none is pending. */
static s32 event_pending_pair;

static FarmCommandPair event_script_6_rules[9] = {
    {3, 0x01, 0x000D, 0x1403, EVENT_RESULT_DEFAULT},
    {3, 0xFF, 0x0002, 0xFFFF, EVENT_RESULT_DEFAULT},
    {3, 0x00, 0x0000, 0x1231, EVENT_RESULT_DEFAULT},
    {3, 0x01, 0x0000, 0x1200, EVENT_RESULT_DEFAULT},
    {3, 0xFF, 0x0001, 0xFFFF, EVENT_RESULT_DEFAULT},
    {3, 0x00, 0x0005, 0x1331, EVENT_RESULT_DEFAULT},
    {3, 0x01, 0x0005, 0x1300, EVENT_RESULT_DEFAULT},
    {0, 0xFF, 0x1431, 0xFFFF, EVENT_RESULT_DEFAULT},
    {1, 0xFF, 0x1400, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_6 = {6, 9, event_script_6_rules};

static FarmCommandPair event_script_7_rules[4] = {
    {3, 0x01, 0x0015, 0x1202, 0x01D0},
    {3, 0x01, 0x0006, 0x1204, 0x01D0},
    {3, 0x01, 0x0005, 0x1202, 0x01CF},
    {1, 0xFF, 0x1202, 0xFFFF, 0x01CE},
};

static FarmScript event_script_7 = {7, 4, event_script_7_rules};

static FarmCommandPair event_script_8_rules[2] = {
    {0, 0xFF, 0x0F09, 0xFFFF, EVENT_RESULT_DEFAULT},
    {1, 0xFF, 0x0F00, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_8 = {8, 2, event_script_8_rules};

static FarmCommandPair event_script_10_rules[1] = {
    {1, 0xFF, 0x0F00, 0xFFFF, 0x01D1},
};

static FarmScript event_script_10 = {10, 1, event_script_10_rules};

static FarmCommandPair event_script_49_rules[1] = {
    {3, 0xFF, 0x0008, 0xFFFF, 0x01D7},
};

static FarmScript event_script_49 = {49, 1, event_script_49_rules};

static FarmCommandPair event_script_11_rules[2] = {
    {0, 0xFF, 0x0423, 0xFFFF, EVENT_RESULT_DEFAULT},
    {1, 0xFF, 0x0400, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_11 = {11, 2, event_script_11_rules};

static FarmCommandPair event_script_12_rules[1] = {
    {1, 0xFF, 0x0401, 0xFFFF, 0x01D2},
};

static FarmScript event_script_12 = {12, 1, event_script_12_rules};

static FarmCommandPair event_script_13_rules[3] = {
    {0, 0xFF, 0x1624, 0xFFFF, EVENT_RESULT_DEFAULT},
    {3, 0xFF, 0x0003, 0xFFFF, EVENT_RESULT_DEFAULT},
    {2, 0x01, 0x0000, 0x1600, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_13 = {13, 3, event_script_13_rules};

static FarmCommandPair event_script_16_17_rules[1] = {
    {1, 0xFF, 0x1602, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_16 = {16, 1, event_script_16_17_rules};

static FarmScript event_script_17 = {17, 1, event_script_16_17_rules};

static FarmCommandPair event_script_19_rules[3] = {
    {3, 0x01, 0x000C, 0x0E03, EVENT_RESULT_DEFAULT},
    {0, 0xFF, 0x0D16, 0xFFFF, EVENT_RESULT_DEFAULT},
    {1, 0xFF, 0x0D00, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_19 = {19, 3, event_script_19_rules};

static FarmCommandPair event_script_20_rules[2] = {
    {3, 0x01, 0x0012, 0x0C04, EVENT_RESULT_DEFAULT},
    {1, 0xFF, 0x0C02, 0xFFFF, 0x01D5},
};

static FarmScript event_script_20 = {20, 2, event_script_20_rules};

static FarmCommandPair event_script_21_22_rules[2] = {
    {0, 0xFF, 0x0E15, 0xFFFF, EVENT_RESULT_DEFAULT},
    {2, 0x01, 0x0001, 0x0E00, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_21 = {21, 2, event_script_21_22_rules};

static FarmScript event_script_22 = {22, 2, event_script_21_22_rules};

static FarmCommandPair event_script_48_rules[2] = {
    {3, 0x01, 0x0012, 0x0C04, EVENT_RESULT_DEFAULT},
    {1, 0xFF, 0x0C02, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_48 = {48, 2, event_script_48_rules};

static FarmCommandPair event_script_23_rules[2] = {
    {0, 0xFF, 0x1118, 0xFFFF, EVENT_RESULT_DEFAULT},
    {1, 0xFF, 0x1100, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_23 = {23, 2, event_script_23_rules};

static FarmCommandPair event_script_24_rules[3] = {
    {3, 0x01, 0x000B, 0x0903, EVENT_RESULT_DEFAULT},
    {0, 0xFF, 0x1719, 0xFFFF, EVENT_RESULT_DEFAULT},
    {1, 0xFF, 0x1700, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_24 = {24, 3, event_script_24_rules};

static FarmCommandPair event_script_25_rules[4] = {
    {3, 0x01, 0x0009, 0x0303, EVENT_RESULT_DEFAULT},
    {0, 0xFF, 0x011E, 0xFFFF, EVENT_RESULT_DEFAULT},
    {3, 0xFF, 0x0004, 0xFFFF, EVENT_RESULT_DEFAULT},
    {2, 0x01, 0x0002, 0x0100, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_25 = {25, 4, event_script_25_rules};

static FarmCommandPair event_script_26_rules[3] = {
    {3, 0x00, 0x0010, 0x021F, EVENT_RESULT_DEFAULT},
    {0, 0xFF, 0x021F, 0xFFFF, EVENT_RESULT_DEFAULT},
    {1, 0xFF, 0x0201, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_26 = {26, 3, event_script_26_rules};

static FarmCommandPair event_script_27_rules[2] = {
    {0, 0xFF, 0x0820, 0xFFFF, EVENT_RESULT_DEFAULT},
    {2, 0x01, 0x0003, 0x0800, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_27 = {27, 2, event_script_27_rules};

static FarmCommandPair event_script_28_rules[1] = {
    {1, 0xFF, 0x0801, 0xFFFF, 0x01D6},
};

static FarmScript event_script_28 = {28, 1, event_script_28_rules};

static FarmCommandPair event_script_29_rules[1] = {
    {1, 0xFF, 0x0801, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_29 = {29, 1, event_script_29_rules};

static FarmCommandPair event_script_30_rules[1] = {
    {1, 0xFF, 0x0902, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_30 = {30, 1, event_script_30_rules};

static FarmCommandPair event_script_31_rules[2] = {
    {3, 0x02, 0x000E, 0x0007, EVENT_RESULT_DEFAULT},
    {1, 0xFF, 0x0A00, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_31 = {31, 2, event_script_31_rules};

static FarmCommandPair event_script_32_rules[2] = {
    {3, 0xFF, 0x0014, 0xFFFF, EVENT_RESULT_DEFAULT},
    {2, 0xFF, 0x0004, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_32 = {32, 2, event_script_32_rules};

static FarmCommandPair event_script_33_rules[1] = {
    {2, 0xFF, 0x0005, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_33 = {33, 1, event_script_33_rules};

static FarmCommandPair event_script_34_rules[6] = {
    {3, 0x01, 0x000A, 0x0703, EVENT_RESULT_DEFAULT},
    {3, 0xFF, 0x0013, 0xFFFF, EVENT_RESULT_DEFAULT},
    {3, 0x00, 0x0007, 0x071B, EVENT_RESULT_DEFAULT},
    {3, 0x02, 0x0007, 0x0008, EVENT_RESULT_DEFAULT},
    {3, 0x02, 0x000F, 0x0007, EVENT_RESULT_DEFAULT},
    {2, 0x01, 0x0006, 0x0600, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_34 = {34, 6, event_script_34_rules};

static FarmCommandPair event_script_37_rules[3] = {
    {3, 0x01, 0x0011, 0x0604, EVENT_RESULT_DEFAULT},
    {3, 0xFF, 0x0007, 0xFFFF, 0x0223},
    {1, 0xFF, 0x0602, 0xFFFF, 0x01D3},
};

static FarmScript event_script_37 = {37, 3, event_script_37_rules};

static FarmCommandPair event_script_41_rules[2] = {
    {3, 0x01, 0x0011, 0x0604, EVENT_RESULT_DEFAULT},
    {1, 0xFF, 0x0602, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmCommandPair event_script_40_rules[2] = {
    {3, 0x01, 0x0011, 0x0604, EVENT_RESULT_DEFAULT},
    {1, 0xFF, 0x0702, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_40 = {40, 2, event_script_40_rules};

static FarmScript event_script_41 = {41, 2, event_script_41_rules};

static FarmCommandPair event_script_42_rules[2] = {
    {0, 0xFF, 0x1835, 0xFFFF, EVENT_RESULT_DEFAULT},
    {1, 0xFF, 0x1800, 0xFFFF, EVENT_RESULT_DEFAULT},
};

static FarmScript event_script_42 = {42, 2, event_script_42_rules};

static FarmScript *event_scripts[31] = {
    &event_script_6, &event_script_7, &event_script_8, &event_script_49, &event_script_10, &event_script_11,
    &event_script_12, &event_script_13, &event_script_16, &event_script_17, &event_script_19, &event_script_20,
    &event_script_21, &event_script_22, &event_script_23, &event_script_24, &event_script_25, &event_script_26,
    &event_script_27, &event_script_28, &event_script_29, &event_script_30, &event_script_31, &event_script_32,
    &event_script_33, &event_script_34, &event_script_37, &event_script_40, &event_script_41, &event_script_42,
    &event_script_48,
};

/* Look up the event script for `id`. A day item of that event gives the default result; otherwise
 * the script's rules decide, and -1 if none does. */
static s32 event_script_lookup_eval(s32 id) {
    u32 count;
    FarmScript *script;
    s32 result;

    event_pending_pair = -1;
    count = 0;
    while (count < 31) {
        script = event_scripts[count];
        if (script->id == id) {
            if (event_day_item_check(id) == 1) {
                return EVENT_RESULT_DEFAULT;
            }
            return event_script_try_rules(script);
        }
        count++;
    }
    result = -1;
    return result;
}

/* Try each rule of the script in turn; return the first result that is not -1,
 * or -1 if none. */
static s32 event_script_try_rules(FarmScript *script) {
    s32 i;
    s32 count = script->count;
    FarmCommandPair *rule = script->rules;

    for (i = 0; i < count; i++) {
        s32 result = event_rule_eval(rule);
        if (result != -1) {
            return result;
        }
        rule++;
    }
    return -1;
}

/* Evaluate one rule. Its first condition must hold, and when there is a second one it must hold
 * too; then the rule gives its result, else -1. A rule for another condition is skipped while one
 * is pending. */
static s32 event_rule_eval(FarmCommandPair *pair) {
    s32 firstResult;
    s32 secondResult;
    u8 command0;
    u8 command1;
    u16 operand0;
    u16 operand1;

    command0 = pair->command0;
    command1 = pair->command1;
    operand0 = pair->operand0;
    operand1 = pair->operand1;
    if (event_pending_pair != -1 && event_pending_pair != operand0) {
        return -1;
    }
    firstResult = event_command_eval(command0, operand0);
    secondResult = event_command_eval(command1, operand1);
    if (firstResult != 1) {
        return -1;
    }
    if (command1 == RULE_CMD_NONE) {
        return pair->result;
    }
    return secondResult == firstResult ? pair->result : -1;
}

/* Test one command against its value (the low 16 bits); an unknown command gives 0. */
static s32 event_command_eval(s32 command, s32 value) {
    value &= 0xFFFF;
    if (command == RULE_CMD_TIME_WINDOW) {
        goto command_two;
    }
    if (command < RULE_CMD_EVENT_CONDITION) {
        if (command == RULE_CMD_VILLAGER_NOT_IN_AREA) {
            goto command_zero;
        }
        if (command == RULE_CMD_AFFECTION_GRADE) {
            goto command_one;
        }
        goto command_done;
    }
    if (command == RULE_CMD_EVENT_CONDITION) {
        goto command_three;
    }
    goto command_done;
command_zero:
    return villager_not_in_area(value >> 8, value & 0xFF);
command_one:
    return villager_affection_grade(value >> 8, value & 0xFF);
command_two:
    return event_time_check(value);
command_three:
    return event_condition_test(value);
command_done:
    return 0;
}

/* Whether villager `idx` is anywhere but area `val`. */
static s32 villager_not_in_area(s32 idx, s32 val) {
    return (game_state.villagers[idx].place.location ^ val) != 0;
}

/* The hours an affection grade keeps a villager's check open: the lower the grade, the narrower the window. */
enum AffectionHourWindow {
    GRADE_LOW_START_HOUR = 10,
    GRADE_LOW_END_HOUR = 18,
    GRADE_MID_START_HOUR = 8,
    GRADE_MID_END_HOUR = 20,
    GRADE_HIGH_START_HOUR = 6,
    GRADE_HIGH_END_HOUR = 22,
};

/* The affection and friendship thresholds a villager's grade is set against. */
enum AffectionGradeThreshold {
    BACHELORETTE_AFFECTION_LOW = 10000,
    BACHELORETTE_AFFECTION_HIGH = 30000,
    FRIENDSHIP_LOW = 100,
    FRIENDSHIP_HIGH = 200,
};

/* Grade a villager against its low and high thresholds for the check `mode`: its affection if it
 * is a bachelorette, its friendship otherwise. */
static s32 villager_affection_grade(s32 slot, u32 mode) {
    s32 grade;
    s32 low;
    s32 high;
    s32 startHour;
    s32 endHour;

    if (villager_is_bachelorette(slot) == 1) {
        grade = game_state.villagers[slot].affection;
        low = BACHELORETTE_AFFECTION_LOW;
        high = BACHELORETTE_AFFECTION_HIGH;
    } else {
        grade = game_state.villagers[slot].friendship;
        low = FRIENDSHIP_LOW;
        high = FRIENDSHIP_HIGH;
    }
    switch (mode) {
    case AFFECTION_HOUR_BY_GRADE:
        if (grade <= low) {
            startHour = GRADE_LOW_START_HOUR;
            endHour = GRADE_LOW_END_HOUR;
        } else if (grade <= high) {
            startHour = GRADE_MID_START_HOUR;
            endHour = GRADE_MID_END_HOUR;
        } else {
            startHour = GRADE_HIGH_START_HOUR;
            endHour = GRADE_HIGH_END_HOUR;
        }
        return hour_outside_range(startHour, endHour);
    case AFFECTION_AT_MOST_LOW:
        return grade <= low;
    case AFFECTION_AT_MOST_HIGH:
        return grade <= high;
    case AFFECTION_LOW_OR_OUTSIDE_18_24:
        if (grade <= low) {
            return 1;
        }
        return hour_outside_range(18, 24);
    case AFFECTION_NEVER:
    default:
        return 0;
    }
}

/* Returns 1 when the current hour is outside the window for this check. */
static s32 event_time_check(s32 command) {
    s32 first;
    s32 second;

    switch (command) {
    case TIME_WINDOW_9_12:
        first = 9;
        second = 12;
        break;
    case TIME_WINDOW_9_16:
    case TIME_WINDOW_9_16_2:
        first = 9;
        second = 16;
        break;
    case TIME_WINDOW_9_17:
        first = 9;
        second = 17;
        break;
    case TIME_WINDOW_13_16:
        first = 13;
        second = 16;
        break;
    case TIME_WINDOW_9_18:
        first = 9;
        second = 18;
        break;
    case TIME_WINDOW_8_21:
        first = 8;
        second = 21;
        break;
    case TIME_WINDOW_18_24:
        first = 18;
        second = 24;
        break;
    case TIME_WINDOW_11_17:
        first = 11;
        second = 17;
        break;
    default:
        return 0;
    }
    return hour_outside_range(first, second);
}

/* Is condition `id` (0..21) of the farm's event script met? Records the id in
 * event_pending_pair when it is. */
static s32 event_condition_test(s32 id)
{
    /* Winter 23rd, while the player is single and the marriage flag `which` is not set. */
    s32 winter_23_single_check(s32 which)
    {
        if (game_state.progress.clock.date.season == SEASON_WINTER &&
            game_state.progress.clock.date.day == 23 &&
            event_flag_get(EVENT_FLAG_SINGLE) == 1 &&
            event_flag_get(EVENT_FLAG_KAREN_RICK_MARRIED + which) == 0) {
            return 1;
        }
        return 0;
    }

    s32 result;
    s32 limit;
    s16 value;

    switch (id) {
    case RULE_COND_POPURI_MARRIED_OR_KAI_MARRIED:
        if (event_flag_get(EVENT_FLAG_MARRIED_POPURI) == 0 && event_flag_get(EVENT_FLAG_POPURI_KAI_MARRIED) == 0) {
            result = 0;
        } else {
            result = 1;
        }
        break;
    case RULE_COND_KAREN_RICK_OR_POPURI_KAI_WEDDING_TODAY:
        if (event_flag_get(EVENT_FLAG_KAREN_RICK_WEDDING_TODAY) == 0 && event_flag_get(EVENT_FLAG_POPURI_KAI_WEDDING_TODAY) == 0) {
            result = 0;
        } else {
            result = 1;
        }
        break;
    case RULE_COND_FLAG_184_ALONE:
        result = 0;
        if (event_flag_get(EVENT_FLAG_184) == 1 && event_flag_get(EVENT_FLAG_185) == 0) {
            result = event_flag_get(EVENT_FLAG_186) == 0;
        }
        break;
    case RULE_COND_ANN_CLIFF_WEDDING_TODAY:
        result = event_flag_get(EVENT_FLAG_ANN_CLIFF_WEDDING_TODAY);
        break;
    case RULE_COND_KAREN_RICK_WEDDING_TODAY:
        result = event_flag_get(EVENT_FLAG_KAREN_RICK_WEDDING_TODAY);
        break;
    case RULE_COND_SUNDAY:
        result = game_state.progress.clock.date.weekday == WEEKDAY_SUNDAY;
        break;
    case RULE_COND_POPURI_MARRIED:
        result = event_flag_get(EVENT_FLAG_MARRIED_POPURI);
        break;
    case RULE_COND_FALL_4:
        result = 0;
        if (game_state.progress.clock.date.season == SEASON_FALL) {
            result = game_state.progress.clock.date.day == 4;
        }
        break;
    case RULE_COND_DOG_PLACED_AND_HELD:
        result = 0;
        if (event_flag_get(EVENT_FLAG_DOG_PLACED) == 1 && (u32)(game_state.player.held[1].id - ITEM_ADULT_DOG) < 2) {
            result = 1;
        }
        break;
    case RULE_COND_WINTER_23_SINGLE_KAREN_RICK:
    case RULE_COND_WINTER_23_SINGLE_ANN_CLIFF:
    case RULE_COND_WINTER_23_SINGLE_ELLI_DOCTOR:
    case RULE_COND_WINTER_23_SINGLE_MARY_GRAY:
    case RULE_COND_WINTER_23_SINGLE_POPURI_KAI:
        result = winter_23_single_check(id - RULE_COND_WINTER_23_SINGLE_KAREN_RICK);
        break;
    case RULE_COND_FALL_2:
        result = 0;
        if (game_state.progress.clock.date.season == SEASON_FALL) {
            result = game_state.progress.clock.date.day == 2;
        }
        break;
    case RULE_COND_SPRING_0:
        result = 0;
        if (game_state.progress.clock.date.season == SEASON_SPRING) {
            result = game_state.progress.clock.date.day == 0;
        }
        break;
    case RULE_COND_KAREN_MARRIED:
        result = event_flag_get(EVENT_FLAG_MARRIED_KAREN);
        break;
    case RULE_COND_ANN_MARRIED:
        result = event_flag_get(EVENT_FLAG_MARRIED_ANN);
        break;
    case RULE_COND_MARY_MARRIED:
        result = event_flag_get(EVENT_FLAG_MARRIED_MARY);
        break;
    case RULE_COND_ANN_BIRTHDAY_INVITATION:
        /* Ann's birthday is today. She can be invited until 10:00 if the player
         * holds an invitation (or can take one), and until 18:00 otherwise. */
        result = 0;
        if (game_state.villagers[CHARACTER_ANN].birthday.season == game_state.progress.clock.date.season
            && game_state.villagers[CHARACTER_ANN].birthday.day == game_state.progress.clock.date.day) {
            if (game_state.player.held[HELD_ITEM].id == ITEM_INVITATION || event_item_can_take(ITEM_INVITATION, 1, 1, 0) == 1) {
                limit = ANN_BIRTHDAY_LIMIT_WITH_INVITATION;
            } else {
                limit = ANN_BIRTHDAY_LIMIT_WITHOUT_INVITATION;
            }
            if (game_state.progress.clock.time.hm.hour < limit) {
                result = 1;
            }
        }
        break;
    case RULE_COND_FLAG_19B_NOT_MON_WED:
        result = 0;
        if (event_flag_get(EVENT_FLAG_19B) == 1) {
            value = game_state.progress.clock.date.weekday;
            if (value != WEEKDAY_MONDAY) {
                result = value != WEEKDAY_WEDNESDAY;
            }
        }
        break;
    case RULE_COND_POPURI_KAI_WEDDING_TODAY:
        result = event_flag_get(EVENT_FLAG_POPURI_KAI_WEDDING_TODAY);
        break;
    default:
        result = 0;
        break;
    }
    if (result == 1) {
        event_pending_pair = id;
    }
    return result;
}

/* Whether the current hour is outside [start, stop). */
static s32 hour_outside_range(s32 start, s32 stop) {
    s16 hour = game_state.progress.clock.time.hm.hour;
    if (hour >= start) {
        if (hour < stop) return 0;
    }
    return 1;
}

/* 0 when `item` is in the day's lookup group; otherwise 1 when the day has a clock date index. */
static s32 event_day_item_check(s32 item) {
    s16 season = game_state.progress.clock.date.season;
    s16 day = game_state.progress.clock.date.day;
    if (day_lookup_has_item(item, season, day) == 1) {
        return 0;
    }
    return clock_date_index(season, day) != 0;
}

static s16 day_lookup_group_0_items[4] = {ITEM_TURNIP_SEEDS, ITEM_STRAWBERRY_SEEDS, ITEM_CORN_SEEDS, ITEM_ONION_SEEDS};

static FarmLookupEntry day_lookup_group_0 = {0, 0, 4, day_lookup_group_0_items};

static s16 day_lookup_group_1_items[1] = {ITEM_COW_MIRACLE_POTION};

static FarmLookupEntry day_lookup_group_1 = {2, 2, 1, day_lookup_group_1_items};

static s16 day_lookup_group_2_items[11] = {
    ITEM_HOE, ITEM_COPPER_HOE, ITEM_GOLD_HAMMER, ITEM_MYSTRILE_HAMMER, ITEM_GOLD_WATERING_CAN, ITEM_MYSTRILE_WATERING_CAN,
    ITEM_MILKER, ITEM_TURNIP_SEEDS, ITEM_STRAWBERRY_SEEDS, ITEM_CORN_SEEDS, ITEM_ONION_SEEDS,
};

static FarmLookupEntry day_lookup_group_2 = {3, 23, 11, day_lookup_group_2_items};

static FarmLookupEntry *day_lookup_groups[3] = {&day_lookup_group_0, &day_lookup_group_1, &day_lookup_group_2};

/* Whether `item` is in the lookup group for the day (x season, y day). */
static s32 day_lookup_has_item(s32 item, s16 x, s16 y) {
    FarmLookupEntry *entry;
    u32 group;
    s32 index;
    for (group = 0; group < 3; group++) {
        entry = day_lookup_groups[group];
        if (entry->x == x && entry->y == y) {
            for (index = 0; index < entry->count; index++) {
                if (entry->values[index] == item) return 1;
            }
        }
    }
    return 0;
}

#define PENDING_RESULT_COUNT 5

/* Look `id` up in the pending pairs; either report its value through `result`
 * and return 0, or hand it to farm_zone_trigger. */
s32 event_pending_pair_lookup(s16 id, u16 zoneValue, s32 *result, s32 evaluate) {
    PendingResult *entry;
    s32 status;
    s32 foundId;
    s32 pendingValue;
    s32 i;

    status = -1;
    foundId = -1;
    pendingValue = 0;
    entry = game_state.pendingResults;
    for (i = 0; i < PENDING_RESULT_COUNT; i++) {
        if (entry->id == id && id != -1) {
            foundId = id;
            pendingValue = entry->value;
        }
        entry++;
    }
    if (foundId == -1) {
        if (evaluate == 0) {
            status = event_script_lookup_eval(id);
        }
        if (id == ITEM_SHEEP_MIRACLE_POTION && event_flag_get(EVENT_FLAG_RESULT_SCREEN) == 1) {
            *result = EVENT_RESULT_MIRACLE_POTION;
            return 0;
        }
        if (game_state.debugFlags & DEBUG_FLAG_DOOR_SCHEDULE) {
            status = -1;
        }
        if (status == -1) {
            return farm_zone_trigger(id, zoneValue);
        }
        if (result == 0) {
            return 0;
        }
        *result = status;
        if (door_entry_for_event(id) == 1) {
            return 0;
        }
        *result = -1;
        return 0;
    }
    if (evaluate != 0) {
        return farm_zone_trigger(id, zoneValue);
    }
    if (result != 0) {
        *result = pendingValue;
    }
    return 0;
}
