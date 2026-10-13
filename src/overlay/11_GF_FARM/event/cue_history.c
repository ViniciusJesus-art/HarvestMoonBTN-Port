/* The farm's cue-history scripts: the cue each farm item plays for a day, by
 * the flags set. */

#include "types.h"
#include "cue.h"
#include "cue_history.h"
#include "clock.h"
#include "event_flags.h"
#include "farm_state.h"
#include "scene.h"
#include "villager.h"

/* The words of a cue-history script (cue_history_select): a clause opens with CUE_IF
 * and lists flag tests (event_flag_get's flag, CUE_NOT for "not set"); when they all
 * hold, the word after CUE_THEN is the result, an offset into the item's cue file,
 * or CUE_NONE for none. CUE_ELSE gives the result when no clause held. */
#define CUE_IF 0xFFFD
#define CUE_THEN 0xFFFE
#define CUE_ELSE 0xFFFC
#define CUE_NONE 0xFFFB
#define CUE_NOT_BIT 0x8000
#define CUE_NOT(flag) (CUE_NOT_BIT | (flag))

static u16 *cue_history_select(u16 *script, u8 *base);

static u16 cue_history_item_1[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x02D0,
    CUE_IF,
    EVENT_FLAG_KAREN_RICK_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    11,
    CUE_THEN,
    0x01E0,
    CUE_IF,
    13,
    CUE_THEN,
    0x00F0,
    CUE_ELSE,
    0x0000,
};

static u16 cue_history_item_2[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x0690,
    CUE_IF,
    EVENT_FLAG_KAREN_RICK_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    19,
    CUE_THEN,
    0x04B0,
    CUE_IF,
    20,
    CUE_THEN,
    0x04B0,
    CUE_IF,
    13,
    CUE_THEN,
    0x05A0,
    CUE_ELSE,
    0x03C0,
};

static u16 cue_history_item_3[] = {
    CUE_IF,
    23,
    127,
    EVENT_FLAG_MARRIED_KAREN,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    EVENT_FLAG_KAREN_RICK_MARRIED,
    CUE_THEN,
    0x12C0,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    EVENT_FLAG_MARRIED_KAREN,
    CUE_THEN,
    0x11D0,
    CUE_IF,
    EVENT_FLAG_MARRIED_KAREN,
    EVENT_FLAG_WIFE_REQUEST_PENDING,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x10E0,
    CUE_IF,
    EVENT_FLAG_KAREN_RICK_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_KAREN_RICK_MARRIED,
    11,
    CUE_THEN,
    0x0FF0,
    CUE_IF,
    EVENT_FLAG_KAREN_RICK_MARRIED,
    CUE_THEN,
    0x0F00,
    CUE_IF,
    EVENT_FLAG_MARRIED_KAREN,
    EVENT_FLAG_CHILD_LEVEL_2,
    CUE_THEN,
    0x0E10,
    CUE_IF,
    EVENT_FLAG_MARRIED_KAREN,
    EVENT_FLAG_CHILD_LEVEL_1,
    CUE_THEN,
    0x0E10,
    CUE_IF,
    EVENT_FLAG_MARRIED_KAREN,
    EVENT_FLAG_CHILD_LEVEL_0,
    CUE_THEN,
    0x0D20,
    CUE_IF,
    EVENT_FLAG_MARRIED_KAREN,
    CUE_THEN,
    0x0C30,
    CUE_IF,
    13,
    19,
    CUE_THEN,
    0x0B40,
    CUE_IF,
    13,
    20,
    CUE_THEN,
    0x0B40,
    CUE_IF,
    13,
    CUE_THEN,
    0x0A50,
    CUE_IF,
    11,
    CUE_THEN,
    0x0960,
    CUE_IF,
    19,
    CUE_THEN,
    0x0870,
    CUE_IF,
    20,
    CUE_THEN,
    0x0870,
    CUE_ELSE,
    0x0780,
};

static u16 cue_history_item_4[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x1680,
    CUE_IF,
    EVENT_FLAG_MARY_GRAY_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_MILESTONE_TIMER,
    CUE_THEN,
    0x1770,
    CUE_IF,
    15,
    19,
    CUE_THEN,
    0x1590,
    CUE_IF,
    15,
    20,
    CUE_THEN,
    0x1590,
    CUE_IF,
    15,
    CUE_THEN,
    0x14A0,
    CUE_ELSE,
    0x13B0,
};

static u16 cue_history_item_5[] = {
    CUE_IF,
    325,
    CUE_NOT(327),
    EVENT_FLAG_SINGLE,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    EVENT_FLAG_MARY_GRAY_MARRIED,
    CUE_THEN,
    0x1FE0,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x1EF0,
    CUE_IF,
    EVENT_FLAG_MARY_GRAY_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_MARY_GRAY_MARRIED,
    15,
    19,
    CUE_THEN,
    0x1E00,
    CUE_IF,
    EVENT_FLAG_MARY_GRAY_MARRIED,
    15,
    20,
    CUE_THEN,
    0x1E00,
    CUE_IF,
    EVENT_FLAG_MARY_GRAY_MARRIED,
    15,
    CUE_THEN,
    0x1D10,
    CUE_IF,
    EVENT_FLAG_MARY_GRAY_MARRIED,
    CUE_THEN,
    0x1C20,
    CUE_IF,
    312,
    15,
    CUE_THEN,
    0x1B30,
    CUE_IF,
    312,
    12,
    CUE_THEN,
    0x1A40,
    CUE_IF,
    312,
    CUE_THEN,
    0x1950,
    CUE_ELSE,
    0x1860,
};

static u16 cue_history_item_6[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x22B0,
    CUE_IF,
    EVENT_FLAG_ANN_CLIFF_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    25,
    124,
    CUE_THEN,
    CUE_NONE,
    CUE_ELSE,
    0x21C0,
};

static u16 cue_history_item_7[] = {
    CUE_IF,
    23,
    127,
    EVENT_FLAG_MARRIED_ANN,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    EVENT_FLAG_MARRIED_ANN,
    CUE_THEN,
    0x2940,
    CUE_IF,
    EVENT_FLAG_MARRIED_ANN,
    EVENT_FLAG_WIFE_REQUEST_PENDING,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x2850,
    CUE_IF,
    EVENT_FLAG_ANN_CLIFF_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_MARRIED_ANN,
    EVENT_FLAG_CHILD_LEVEL_2,
    CUE_THEN,
    0x2760,
    CUE_IF,
    EVENT_FLAG_MARRIED_ANN,
    EVENT_FLAG_CHILD_LEVEL_1,
    CUE_THEN,
    0x2760,
    CUE_IF,
    EVENT_FLAG_MARRIED_ANN,
    EVENT_FLAG_CHILD_LEVEL_0,
    CUE_THEN,
    0x2670,
    CUE_IF,
    EVENT_FLAG_MARRIED_ANN,
    CUE_THEN,
    0x2580,
    CUE_IF,
    19,
    CUE_THEN,
    0x2490,
    CUE_IF,
    20,
    CUE_THEN,
    0x2490,
    CUE_ELSE,
    0x23A0,
};

static u16 cue_history_item_8[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x2D00,
    CUE_IF,
    EVENT_FLAG_ELLI_DOCTOR_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    14,
    19,
    CUE_THEN,
    0x2C10,
    CUE_IF,
    14,
    20,
    CUE_THEN,
    0x2C10,
    CUE_IF,
    14,
    CUE_THEN,
    0x2B20,
    CUE_ELSE,
    0x2A30,
};

static u16 cue_history_item_9[] = {
    CUE_IF,
    23,
    127,
    EVENT_FLAG_MARRIED_ELLI,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    EVENT_FLAG_MARRIED_ELLI,
    CUE_THEN,
    0x3390,
    CUE_IF,
    EVENT_FLAG_MARRIED_ELLI,
    EVENT_FLAG_WIFE_REQUEST_PENDING,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x32A0,
    CUE_IF,
    EVENT_FLAG_ELLI_DOCTOR_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_MARRIED_ELLI,
    EVENT_FLAG_CHILD_LEVEL_2,
    CUE_THEN,
    0x31B0,
    CUE_IF,
    EVENT_FLAG_MARRIED_ELLI,
    EVENT_FLAG_CHILD_LEVEL_1,
    CUE_THEN,
    0x31B0,
    CUE_IF,
    EVENT_FLAG_MARRIED_ELLI,
    EVENT_FLAG_CHILD_LEVEL_0,
    CUE_THEN,
    0x30C0,
    CUE_IF,
    EVENT_FLAG_MARRIED_ELLI,
    CUE_THEN,
    0x2FD0,
    CUE_IF,
    14,
    CUE_THEN,
    0x2EE0,
    CUE_ELSE,
    0x2DF0,
};

static u16 cue_history_item_10[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x3750,
    CUE_IF,
    EVENT_FLAG_ANN_CLIFF_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    411,
    12,
    CUE_THEN,
    0x3660,
    CUE_IF,
    411,
    14,
    CUE_THEN,
    0x3660,
    CUE_IF,
    411,
    CUE_THEN,
    0x3570,
    CUE_ELSE,
    0x3480,
};

static u16 cue_history_item_11[] = {
    CUE_IF,
    CUE_NOT(230),
    CUE_NOT(EVENT_FLAG_FIRST_YEAR),
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x3B10,
    CUE_IF,
    EVENT_FLAG_ANN_CLIFF_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    402,
    13,
    CUE_THEN,
    0x3A20,
    CUE_IF,
    402,
    15,
    CUE_THEN,
    0x3A20,
    CUE_IF,
    402,
    17,
    CUE_THEN,
    0x3A20,
    CUE_IF,
    402,
    CUE_THEN,
    0x3930,
    CUE_IF,
    EVENT_FLAG_FIRST_YEAR,
    230,
    CUE_NOT(402),
    CUE_THEN,
    CUE_NONE,
    CUE_ELSE,
    0x3840,
};

static u16 cue_history_item_12[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x3ED0,
    CUE_IF,
    EVENT_FLAG_MARY_GRAY_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    12,
    19,
    CUE_THEN,
    0x3DE0,
    CUE_IF,
    12,
    20,
    CUE_THEN,
    0x3DE0,
    CUE_IF,
    12,
    CUE_THEN,
    0x3CF0,
    CUE_ELSE,
    0x3C00,
};

static u16 cue_history_item_13[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x4380,
    CUE_IF,
    EVENT_FLAG_MARY_GRAY_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    12,
    19,
    CUE_THEN,
    0x4290,
    CUE_IF,
    12,
    20,
    CUE_THEN,
    0x4290,
    CUE_IF,
    12,
    CUE_THEN,
    0x41A0,
    CUE_IF,
    19,
    CUE_THEN,
    0x40B0,
    CUE_IF,
    20,
    CUE_THEN,
    0x40B0,
    CUE_ELSE,
    0x3FC0,
};

static u16 cue_history_item_14[] = {
    CUE_IF,
    23,
    127,
    EVENT_FLAG_MARRIED_MARY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    EVENT_FLAG_MARY_GRAY_MARRIED,
    CUE_THEN,
    0x4FB0,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    EVENT_FLAG_MARRIED_MARY,
    CUE_THEN,
    0x4EC0,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_WIFE_REQUEST_PENDING,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x4DD0,
    CUE_IF,
    EVENT_FLAG_MARY_GRAY_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_MARY_GRAY_MARRIED,
    15,
    19,
    CUE_THEN,
    0x4CE0,
    CUE_IF,
    EVENT_FLAG_MARY_GRAY_MARRIED,
    15,
    20,
    CUE_THEN,
    0x4CE0,
    CUE_IF,
    EVENT_FLAG_MARY_GRAY_MARRIED,
    15,
    CUE_THEN,
    0x4BF0,
    CUE_IF,
    EVENT_FLAG_MARY_GRAY_MARRIED,
    CUE_THEN,
    0x4B00,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_PLAYER_BIRTHDAY,
    EVENT_FLAG_CHILD_LEVEL_2,
    CUE_THEN,
    0x4920,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_WIFE_BIRTHDAY,
    EVENT_FLAG_CHILD_LEVEL_2,
    CUE_THEN,
    0x4920,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_FAMILY_SELECTION_TODAY,
    EVENT_FLAG_CHILD_LEVEL_2,
    CUE_THEN,
    0x4920,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_CHILD_BIRTHDAY,
    EVENT_FLAG_CHILD_LEVEL_2,
    CUE_THEN,
    0x4920,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_PLAYER_BIRTHDAY,
    EVENT_FLAG_CHILD_LEVEL_1,
    CUE_THEN,
    0x4920,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_WIFE_BIRTHDAY,
    EVENT_FLAG_CHILD_LEVEL_1,
    CUE_THEN,
    0x4920,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_FAMILY_SELECTION_TODAY,
    EVENT_FLAG_CHILD_LEVEL_1,
    CUE_THEN,
    0x4920,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_CHILD_BIRTHDAY,
    EVENT_FLAG_CHILD_LEVEL_1,
    CUE_THEN,
    0x4920,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_PLAYER_BIRTHDAY,
    EVENT_FLAG_CHILD_LEVEL_0,
    CUE_THEN,
    0x4830,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_WIFE_BIRTHDAY,
    EVENT_FLAG_CHILD_LEVEL_0,
    CUE_THEN,
    0x4830,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_FAMILY_SELECTION_TODAY,
    EVENT_FLAG_CHILD_LEVEL_0,
    CUE_THEN,
    0x4830,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_CHILD_BIRTHDAY,
    EVENT_FLAG_CHILD_LEVEL_0,
    CUE_THEN,
    0x4830,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_PLAYER_BIRTHDAY,
    CUE_THEN,
    0x4740,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_WIFE_BIRTHDAY,
    CUE_THEN,
    0x4740,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_FAMILY_SELECTION_TODAY,
    CUE_THEN,
    0x4740,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_CHILD_BIRTHDAY,
    CUE_THEN,
    0x4740,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    11,
    CUE_THEN,
    0x4A10,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_CHILD_LEVEL_2,
    CUE_THEN,
    0x4920,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_CHILD_LEVEL_1,
    CUE_THEN,
    0x4920,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_CHILD_LEVEL_0,
    CUE_THEN,
    0x4830,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_WEATHER_STORM,
    CUE_THEN,
    0x4740,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    EVENT_FLAG_WEATHER_BLIZZARD,
    CUE_THEN,
    0x4740,
    CUE_IF,
    EVENT_FLAG_MARRIED_MARY,
    CUE_THEN,
    0x4740,
    CUE_IF,
    12,
    19,
    CUE_THEN,
    0x4650,
    CUE_IF,
    12,
    20,
    CUE_THEN,
    0x4650,
    CUE_IF,
    12,
    CUE_THEN,
    0x4560,
    CUE_ELSE,
    0x4470,
};

static u16 cue_history_item_15[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x5550,
    CUE_IF,
    12,
    19,
    CUE_THEN,
    0x5460,
    CUE_IF,
    12,
    20,
    CUE_THEN,
    0x5460,
    CUE_IF,
    12,
    CUE_THEN,
    0x5370,
    CUE_ELSE,
    0x5280,
};

static u16 cue_history_item_16[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x5BE0,
    CUE_IF,
    19,
    CUE_THEN,
    0x5910,
    CUE_IF,
    20,
    CUE_THEN,
    0x5910,
    CUE_IF,
    12,
    CUE_THEN,
    0x5A00,
    CUE_IF,
    411,
    14,
    CUE_THEN,
    0x5AF0,
    CUE_IF,
    411,
    CUE_THEN,
    0x5820,
    CUE_ELSE,
    0x5730,
};

static u16 cue_history_item_17[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x6270,
    CUE_IF,
    11,
    19,
    CUE_THEN,
    0x6180,
    CUE_IF,
    11,
    20,
    CUE_THEN,
    0x6180,
    CUE_IF,
    11,
    CUE_THEN,
    0x6090,
    CUE_IF,
    17,
    19,
    CUE_THEN,
    0x5FA0,
    CUE_IF,
    17,
    20,
    CUE_THEN,
    0x5FA0,
    CUE_IF,
    17,
    CUE_THEN,
    0x5EB0,
    CUE_ELSE,
    0x5DC0,
};

static u16 cue_history_item_18[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x6540,
    CUE_IF,
    EVENT_FLAG_KAREN_RICK_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_POPURI_KAI_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    388,
    CUE_NOT(389),
    CUE_NOT(390),
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    11,
    CUE_THEN,
    0x6450,
    CUE_ELSE,
    0x6360,
};

static u16 cue_history_item_19[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x6900,
    CUE_IF,
    EVENT_FLAG_KAREN_RICK_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_POPURI_KAI_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    388,
    CUE_NOT(389),
    CUE_NOT(390),
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    19,
    CUE_THEN,
    0x6720,
    CUE_IF,
    20,
    CUE_THEN,
    0x6720,
    CUE_IF,
    13,
    CUE_THEN,
    0x6810,
    CUE_ELSE,
    0x6630,
};

static u16 cue_history_item_20[] = {
    CUE_IF,
    23,
    127,
    EVENT_FLAG_MARRIED_POPURI,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    EVENT_FLAG_POPURI_KAI_MARRIED,
    CUE_THEN,
    0x7530,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    EVENT_FLAG_MARRIED_POPURI,
    CUE_THEN,
    0x7440,
    CUE_IF,
    EVENT_FLAG_MARRIED_POPURI,
    EVENT_FLAG_WIFE_REQUEST_PENDING,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x7350,
    CUE_IF,
    EVENT_FLAG_KAREN_RICK_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_POPURI_KAI_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    388,
    CUE_NOT(389),
    CUE_NOT(390),
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_POPURI_KAI_MARRIED,
    24,
    19,
    CUE_THEN,
    0x7260,
    CUE_IF,
    EVENT_FLAG_POPURI_KAI_MARRIED,
    24,
    CUE_THEN,
    0x7170,
    CUE_IF,
    EVENT_FLAG_POPURI_KAI_MARRIED,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_MARRIED_POPURI,
    EVENT_FLAG_CHILD_LEVEL_2,
    CUE_THEN,
    0x7080,
    CUE_IF,
    EVENT_FLAG_MARRIED_POPURI,
    EVENT_FLAG_CHILD_LEVEL_1,
    CUE_THEN,
    0x7080,
    CUE_IF,
    EVENT_FLAG_MARRIED_POPURI,
    EVENT_FLAG_CHILD_LEVEL_0,
    CUE_THEN,
    0x6F90,
    CUE_IF,
    EVENT_FLAG_MARRIED_POPURI,
    CUE_THEN,
    0x6EA0,
    CUE_IF,
    24,
    11,
    19,
    CUE_THEN,
    0x7620,
    CUE_IF,
    24,
    11,
    18,
    CUE_THEN,
    0x6DB0,
    CUE_IF,
    11,
    19,
    CUE_THEN,
    0x6CC0,
    CUE_IF,
    11,
    20,
    CUE_THEN,
    0x6CC0,
    CUE_IF,
    11,
    CUE_THEN,
    0x6BD0,
    CUE_IF,
    19,
    CUE_THEN,
    0x6AE0,
    CUE_IF,
    20,
    CUE_THEN,
    0x6AE0,
    CUE_ELSE,
    0x69F0,
};

static u16 cue_history_item_21[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x7AD0,
    CUE_IF,
    EVENT_FLAG_ANN_CLIFF_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_FIRST_YEAR,
    25,
    134,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_FIRST_YEAR,
    25,
    135,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_FIRST_YEAR,
    25,
    136,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_FIRST_YEAR,
    25,
    137,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_FIRST_YEAR,
    25,
    138,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    EVENT_FLAG_FIRST_YEAR,
    25,
    139,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    402,
    17,
    CUE_THEN,
    0x79E0,
    CUE_IF,
    402,
    CUE_THEN,
    0x78F0,
    CUE_IF,
    17,
    CUE_THEN,
    0x7800,
    CUE_ELSE,
    0x7710,
};

static u16 cue_history_item_22[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x8070,
    CUE_IF,
    EVENT_FLAG_ANN_CLIFF_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    402,
    17,
    CUE_THEN,
    0x7F80,
    CUE_IF,
    19,
    CUE_THEN,
    0x7DA0,
    CUE_IF,
    20,
    CUE_THEN,
    0x7DA0,
    CUE_IF,
    EVENT_FLAG_FIRST_YEAR,
    25,
    134,
    CUE_THEN,
    0x7CB0,
    CUE_IF,
    EVENT_FLAG_FIRST_YEAR,
    25,
    135,
    CUE_THEN,
    0x7CB0,
    CUE_IF,
    EVENT_FLAG_FIRST_YEAR,
    25,
    136,
    CUE_THEN,
    0x7CB0,
    CUE_IF,
    EVENT_FLAG_FIRST_YEAR,
    25,
    137,
    CUE_THEN,
    0x7CB0,
    CUE_IF,
    EVENT_FLAG_FIRST_YEAR,
    25,
    138,
    CUE_THEN,
    0x7CB0,
    CUE_IF,
    EVENT_FLAG_FIRST_YEAR,
    25,
    139,
    CUE_THEN,
    0x7CB0,
    CUE_IF,
    17,
    CUE_THEN,
    0x7E90,
    CUE_ELSE,
    0x7CB0,
};

static u16 cue_history_item_23[] = {
    CUE_IF,
    EVENT_FLAG_ELLI_DOCTOR_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_ELSE,
    0x8160,
};

static u16 cue_history_item_24[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x8610,
    CUE_IF,
    EVENT_FLAG_BUILD_IN_PROGRESS,
    CUE_THEN,
    0x8700,
    CUE_IF,
    11,
    CUE_THEN,
    0x8520,
    CUE_IF,
    17,
    CUE_THEN,
    0x8430,
    CUE_IF,
    15,
    CUE_THEN,
    0x8340,
    CUE_IF,
    16,
    CUE_THEN,
    0x8340,
    CUE_ELSE,
    0x8250,
};

static u16 cue_history_item_25[] = {
    CUE_IF,
    24,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x8E80,
    CUE_IF,
    24,
    11,
    19,
    CUE_THEN,
    0x8D90,
    CUE_IF,
    24,
    11,
    CUE_THEN,
    0x8CA0,
    CUE_IF,
    24,
    CUE_THEN,
    0x8BB0,
    CUE_ELSE,
    CUE_NONE,
};

static u16 cue_history_item_26[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x9060,
    CUE_ELSE,
    0x8F70,
};

static u16 cue_history_item_28[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x9240,
    CUE_ELSE,
    0x9150,
};

static u16 cue_history_item_29[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x9420,
    CUE_ELSE,
    0x9330,
};

static u16 cue_history_item_31[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x96F0,
    CUE_IF,
    16,
    CUE_THEN,
    0x9600,
    CUE_IF,
    17,
    CUE_THEN,
    0x9600,
    CUE_ELSE,
    0x9510,
};

static u16 cue_history_item_32[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x99C0,
    CUE_IF,
    16,
    CUE_THEN,
    0x98D0,
    CUE_IF,
    17,
    CUE_THEN,
    0x98D0,
    CUE_IF,
    11,
    CUE_THEN,
    0x98D0,
    CUE_ELSE,
    0x97E0,
};

static u16 cue_history_item_34[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0x9F60,
    CUE_IF,
    EVENT_FLAG_ELLI_DOCTOR_WEDDING_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_IF,
    411,
    14,
    CUE_THEN,
    0x9E70,
    CUE_IF,
    12,
    CUE_THEN,
    0x9D80,
    CUE_IF,
    19,
    CUE_THEN,
    0x9BA0,
    CUE_IF,
    20,
    CUE_THEN,
    0x9BA0,
    CUE_IF,
    411,
    CUE_THEN,
    0x9C90,
    CUE_ELSE,
    0x9AB0,
};

static u16 cue_history_item_35[] = {
    CUE_IF,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    CUE_NONE,
    CUE_ELSE,
    0xA050,
};

static u16 cue_history_item_36[] = {
    CUE_IF,
    EVENT_FLAG_CHILD_LEVEL_0,
    23,
    127,
    CUE_THEN,
    0xA410,
    CUE_IF,
    EVENT_FLAG_CHILD_LEVEL_0,
    EVENT_FLAG_DATED_EVENT_TODAY,
    CUE_THEN,
    0xA140,
    CUE_IF,
    EVENT_FLAG_CHILD_LEVEL_0,
    EVENT_FLAG_WIFE_REQUEST_PENDING,
    CUE_THEN,
    0xA410,
    CUE_IF,
    EVENT_FLAG_CHILD_LEVEL_0,
    CUE_THEN,
    0xA140,
    CUE_IF,
    EVENT_FLAG_CHILD_LEVEL_1,
    CUE_THEN,
    0xA230,
    CUE_IF,
    EVENT_FLAG_CHILD_LEVEL_2,
    CUE_THEN,
    0xA320,
    CUE_ELSE,
    CUE_NONE,
};

/* Each farm item's cue-history script (cue_history_reset, by item number). */
static u16 *cue_history_scripts[40] = {
    0,
    cue_history_item_1,
    cue_history_item_2,
    cue_history_item_3,
    cue_history_item_4,
    cue_history_item_5,
    cue_history_item_6,
    cue_history_item_7,
    cue_history_item_8,
    cue_history_item_9,
    cue_history_item_10,
    cue_history_item_11,
    cue_history_item_12,
    cue_history_item_13,
    cue_history_item_14,
    cue_history_item_15,
    cue_history_item_16,
    cue_history_item_17,
    cue_history_item_18,
    cue_history_item_19,
    cue_history_item_20,
    cue_history_item_21,
    cue_history_item_22,
    cue_history_item_23,
    cue_history_item_24,
    cue_history_item_25,
    cue_history_item_26,
    0,
    cue_history_item_28,
    cue_history_item_29,
    0,
    cue_history_item_31,
    cue_history_item_32,
    0,
    cue_history_item_34,
    cue_history_item_35,
    cue_history_item_36,
    0,
    0,
    0,
};

/* Reset each farm item record, then import the schedule that its cue-history script selects
 * (VILLAGER_SCHEDULE_STEPS steps read from the cue file at the chosen offset). The record
 * starts at the first step's place. With nothing selected the schedule stays at the reset
 * values: every step in the event cell. */
void cue_history_reset(u8 *base)
{
    FarmVillager *item;
    u32 index;
    u32 count;
    u16 *source;
    u16 *entry;
    s16 eventCell;
    item = game_state.villagers;
    index = 0;
    eventCell = PLACE_EVENT_CELL;
    do
    {
        item->id = index;
        item->place.value = eventCell;
        item->place.location = eventCell;
        item->lockCount = 0;
        item->delay = 0;
        item->column = 0;
        item->row = 0;
        item->wait = 0;
        item->destination = 0;
        item->prevDestination = 0;
        item->unused18 = 0;
        item->action = VILLAGER_ACTION_NONE;
        item->prevAction = VILLAGER_ACTION_NONE;
        item->step = 0;
        item->scriptOffset = 0;
        item->flags = (item->flags | VILLAGER_FLAG_JUST_RESET) & VILLAGER_FLAG_RESET_KEEP;
        item->placeFlags = item->placeFlags & ~VILLAGER_PLACE_KEPT_PLACE;
        cue_reset(&item->cue);
        count = 0;
        do
        {
            item->schedule[count].time = 0;
            item->schedule[count].route = 0;
            item->schedule[count].place.value = eventCell;
            item->schedule[count].place.location = eventCell;
            count++;
        } while (count < VILLAGER_SCHEDULE_STEPS);
        source = cue_history_scripts[index];
        if (source != 0)
        {
            source = cue_history_select(source, base);
            if (source == 0)
                item->scriptOffset = 0;
            else
                item->scriptOffset = (s32)source - (s32)base;
            if (source != 0)
            {
                count = 0;
                entry = source;
                do
                {
                    item->schedule[count].time = entry[0];
                    item->schedule[count].route = entry[1];
                    item->schedule[count].place.value = entry[2];
                    item->schedule[count].place.location = entry[3];
                    count++;
                    entry += 4;
                } while (count < VILLAGER_SCHEDULE_STEPS);
            }
            item->place.value = item->schedule[0].place.value;
            item->place.location = item->schedule[0].place.location;
            item->destination = item->schedule[0].route >> 8;
            item->prevDestination = item->schedule[0].route >> 8;
        }
        index++;
        item++;
    } while (index < CHARACTER_COUNT);
}

/* Walk a u16 condition script and return the entry it selects: `base` plus the selected
 * word's offset (low two bits masked off), or NULL when nothing is selected (CUE_NONE).
 *
 * The script is a list of clauses. Each clause opens with CUE_IF, which is skipped, and is
 * a run of flag tests (event_flag_get's flag, CUE_NOT for "not set"). A passing test may be
 * followed by CUE_THEN and a result word, taken only if no earlier test in the clause
 * failed. A failing test skips the rest of its clause: its CUE_THEN and result, or a
 * CUE_NONE. A CUE_ELSE in place of a clause is an unconditional pick: the next word is the
 * result. */
static u16 *cue_history_select(u16 *script, u8 *base)
{
    s32 more;
    s32 failed;
    s32 result;

    while (1)
    {
        more = 1;
        if (*script == CUE_ELSE)
        {
            script++;
            if (*script == CUE_NONE)
            {
                return 0;
            }
            return (u16 *)(base + (*script & ~3));
        }
        failed = 0;
        script++;
        do
        {
            result = event_flag_get(*script & 0x7FFF);
            if (*script & CUE_NOT_BIT)
            {
                if (result != 0)
                {
                    result = 0;
                }
                else
                {
                    result = 1;
                }
            }
            if (result == 1)
            {
                script++;
                if (*script == CUE_THEN)
                {
                    script++;
                    if (!failed)
                    {
                        if (*script == CUE_NONE)
                        {
                            return 0;
                        }
                        return (u16 *)(base + (*script & 0xFFFC));
                    }
                    script++;
                    more = 0;
                }
            }
            else
            {
                script++;
                failed = 1;
                if (*script == CUE_THEN)
                {
                    script += 2;
                    more = 0;
                }
                if (*script == CUE_NONE)
                {
                    script++;
                    more = 0;
                }
            }
        } while (more);
    }
}

/* Point each farm item at its first schedule step not earlier than `arg` (a minute of the
 * day) and refresh its row and place from that step. Items with no such step are left
 * as they are. Nothing runs on the day-start minute, CLOCK_DAY_START_MINUTE. */
void item_select_schedule_step(s16 arg)
{
    FarmVillager *item;
    VillagerStep *entry;
    s16 current;
    s32 index;
    s32 selected;
    s32 slot;
    s32 next_slot;
    FarmVillager *next_item;
    if (arg != CLOCK_DAY_START_MINUTE)
    {
        item = game_state.villagers;
        slot = 0;
        do
        {
            entry = item->schedule;
            index = 0;
            next_slot = slot + 1;
            next_item = item + 1;
            do
            {
                current = entry->time;

                if (current >= arg)
                {
                    if (arg < current)
                    {
                        selected = index;
                        if (selected < 0)
                        {
                            selected = 0;
                        }
                        item->step = selected;
                    }
                    else
                    {
                        item->step = index;
                    }
                    item->column = 0;
                    item->row = item->schedule[item->step].route & 0xFF;
                    item->place.value = item->schedule[item->step].place.value;
                    item->place.location = item->schedule[item->step].place.location;
                    break;
                }
                index++;
                entry++;
            } while (index < VILLAGER_SCHEDULE_STEPS);
            slot = next_slot;
            item = next_item;
        } while (slot < CHARACTER_COUNT);
    }
}
