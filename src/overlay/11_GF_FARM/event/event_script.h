#ifndef EVENT_SCRIPT_H
#define EVENT_SCRIPT_H

/* The farm event scripts: their steps, handlers and triggers. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "anim.h"

/* A farm event's flag word, also read a byte at a time. */
typedef union FarmEventFlags {
    s32 word;
    u8 bytes[4];        /* bit 7 of bytes[1] is passed on when the event is requested */
} FarmEventFlags;

/* The conditions an event's `availability` byte can name (0xFF = none). NOT_ negates
 * the test; a _2 or _3 suffix repeats the test of its unsuffixed twin. */
enum EventCondition {
    EVENT_COND_NO_HORSE_ON_MAP_2A_30 = 0,
    EVENT_COND_HORSE_ADULT = 1,
    EVENT_COND_HORSE_ADULT_2 = 2,
    EVENT_COND_CAN_TAKE_BANDAGE = 3,
    EVENT_COND_PLOT_FREE = 4,
    EVENT_COND_POPURI_CHICK_NAMED = 5,
    EVENT_COND_POPURI_CHICK_NOT_NAMED = 6,
    EVENT_COND_MOONDROP_SEEDS_PLANTABLE = 7,
    EVENT_COND_CAN_TAKE_WOODCUTTER_BOOK = 8,
    EVENT_COND_INVITATION_DUE = 9,
    EVENT_COND_POPURI_LOVE_BELOW_1 = 10,
    EVENT_COND_POPURI_LOVE_BELOW_2 = 11,
    EVENT_COND_POPURI_LOVE_BELOW_3 = 12,
    EVENT_COND_POPURI_LOVE_UNDER_50000 = 13,
    EVENT_COND_ANN_LOVE_BELOW_1 = 14,
    EVENT_COND_ANN_LOVE_BELOW_2 = 15,
    EVENT_COND_ANN_LOVE_BELOW_3 = 16,
    EVENT_COND_ANN_LOVE_UNDER_60000 = 17,
    EVENT_COND_FLAG_E6_CLEAR = 18,
    EVENT_COND_KAREN_LOVE_BELOW_1 = 19,
    EVENT_COND_KAREN_LOVE_BELOW_2 = 20,
    EVENT_COND_KAREN_LOVE_WEDDING_GATE = 21,
    EVENT_COND_POPURI_MARRIED_OR_KAREN_LOVE = 22,
    EVENT_COND_KAREN_OFFER_UNDER_60000 = 23,
    EVENT_COND_ELLI_LOVE_BELOW_1 = 24,
    EVENT_COND_ELLI_LOVE_BELOW_2 = 25,
    EVENT_COND_ELLI_LOVE_UNDER_60000 = 26,
    EVENT_COND_MARY_LOVE_BELOW_1 = 27,
    EVENT_COND_MARY_LOVE_BELOW_2_DAYTIME = 28,
    EVENT_COND_MARY_LOVE_BELOW_3 = 29,
    EVENT_COND_MARY_LOVE_UNDER_60000 = 30,
    EVENT_COND_FIELD_TILES_200 = 31,
    EVENT_COND_STORM_TODAY = 32,
    EVENT_COND_CORN_OR_DOG_ADULT = 33,
    EVENT_COND_DOG_ADULT = 34,
    EVENT_COND_HELPERS_AT_28_AFFECTION_80 = 35,
    EVENT_COND_NOT_ANN_MARRIED = 36,
    EVENT_COND_FLOWER_TILES_90 = 37,
    EVENT_COND_ALWAYS_38 = 38,
    EVENT_COND_CAN_TAKE_APPLE_PIE = 39,
    EVENT_COND_FLAG_E8_SET = 40,
    EVENT_COND_FREE_CHICKEN_PLOTS_5 = 41,
    EVENT_COND_CONTEST_ENTRANTS_AFFECTION_50 = 42,
    EVENT_COND_ALWAYS_43 = 43,
    EVENT_COND_ALWAYS_44 = 44,
    EVENT_COND_ALWAYS_45 = 45,
    EVENT_COND_ALWAYS_46 = 46,
    EVENT_COND_NOT_MAP_24_25 = 47,
    EVENT_COND_CHICKENS_3 = 48,
    EVENT_COND_EGGS_3 = 49,
    EVENT_COND_POPURI_OFFER_YES = 50,
    EVENT_COND_ANN_OFFER_YES = 51,
    EVENT_COND_KAREN_OFFER_YES = 52,
    EVENT_COND_ELLI_OFFER_YES = 53,
    EVENT_COND_MARY_OFFER_YES = 54,
    EVENT_COND_DAYS_COUNT2_NOT_0X59 = 55,
    EVENT_COND_DAYS_COUNT0X30_SET = 56,
    EVENT_COND_ANIMAL_DIED = 57,
    EVENT_COND_FISH_50 = 61,
    EVENT_COND_CATCHES_ON_MAP_C_22 = 62,
    EVENT_COND_KAREN_NOT_PREGNANT_OR_PENDING = 63,
    EVENT_COND_NOT_WIFE_REQUEST_PENDING = 64,
    EVENT_COND_FAMILY_SELECTION_TODAY = 65,
    EVENT_COND_BRIDE_BIRTHDAY_TODAY = 66,
    EVENT_COND_CHILD_BIRTHDAY_TODAY = 67,
    EVENT_COND_PLAYER_BIRTHDAY_TODAY = 68,
    EVENT_COND_PREGNANT_1_TO_7_DAYS = 69,
    EVENT_COND_PREGNANT_60_NOT_BIRTHDAY = 70,
    EVENT_COND_WIFE_REQUESTS_1_2 = 71,
    EVENT_COND_WIFE_REQUESTS_3_UP = 72,
    EVENT_COND_WIFE_IGNORED_30_DAYS = 73,
    EVENT_COND_CHILD_GROWTH_10_DAYS = 74,
    EVENT_COND_SPAWN_GATE_COUNT0X17 = 75,
    EVENT_COND_FLOWER_SEEDS_PLANTABLE = 76,
    EVENT_COND_ALWAYS_77 = 77,
    EVENT_COND_GOLD_500 = 79,
    EVENT_COND_GOLD_30000_AFFECTION_50000 = 80,
    EVENT_COND_ADULT_DOG_GOLD_100 = 81,
    EVENT_COND_HOUSE_LEVEL_2_GOLD_5000 = 82,
    EVENT_COND_YEAR_CYCLE_ITEMS_3 = 83,
    EVENT_COND_UTENSIL_DUE_KNIFE = 84,
    EVENT_COND_UTENSIL_DUE_FRYING_PAN = 85,
    EVENT_COND_UTENSIL_DUE_POT = 86,
    EVENT_COND_UTENSIL_DUE_MIXER = 87,
    EVENT_COND_UTENSIL_DUE_WHISK = 88,
    EVENT_COND_UTENSIL_DUE_ROLLING_PIN = 89,
    EVENT_COND_UTENSIL_DUE_OVEN = 90,
    EVENT_COND_UTENSIL_DUE_SEASONING_SET = 91,
    EVENT_COND_OFFER_9_DUE = 92,
    EVENT_COND_ALWAYS_93 = 93,
    EVENT_COND_ALWAYS_94 = 94,
    EVENT_COND_DOG_ADULT_2 = 95,
    EVENT_COND_ALWAYS_96 = 96,
    EVENT_COND_NEWBORN_READY_FOR_NAMING = 97,
    EVENT_COND_POPURI_KAI_WEDDING_TODAY = 98,
    EVENT_COND_POPURI_KAI_WEDDING_TODAY_2 = 99,
    EVENT_COND_ANN_CLIFF_WEDDING_TODAY = 100,
    EVENT_COND_KAREN_RICK_WEDDING_TODAY = 101,
    EVENT_COND_ELLI_DOCTOR_WEDDING_TODAY = 102,
    EVENT_COND_MARY_GRAY_WEDDING_TODAY = 103,
    EVENT_COND_NOT_BUILD_IN_PROGRESS = 104,
    EVENT_COND_FLAG_15F_OR_163_OR_164 = 105,
    EVENT_COND_NO_WEDDING_TODAY = 106,
    EVENT_COND_ANN_OFFER_UNDER_60000 = 107,
    EVENT_COND_OFFER_4_YES = 108,
    EVENT_COND_MARY_OFFER_UNDER_60000 = 109,
    EVENT_COND_OFFER_0_YES = 110,
    EVENT_COND_ELLI_OFFER_UNDER_60000 = 111,
    EVENT_COND_HORSE_OWNED = 112,
    EVENT_COND_PLACE_SEASON_DAY = 113,
    EVENT_COND_WIFE_REQUEST_DAY = 114,
    EVENT_COND_GOLD_500_ROD_NOT_TAKEABLE = 115,
    EVENT_COND_TALK_VILLAGER_MARRIED = 116,
    EVENT_COND_NOT_SINGLE = 117,
};

/* One farm event from the trigger table: the id and every condition
 * the trigger check tests before firing it. */
typedef struct FarmEventEntry {
    u16 id;             /* low 10 bits: the event id */
    u8 varId;           /* the farm variable (EventVar) ... */
    u8 varElement;      /* ...and its element; either 0xFF = no test */
    u16 rangeLow;       /* the value must be at least this (0xFF = no lower bound) */
    u16 rangeHigh;      /* ...and at most this (0xFF = no upper bound) */
    s32 window;         /* a packed date and time window: six halfwords once unpacked */
    u8 roleCondition;   /* EventRoleCondition */
    u8 bondVillager;    /* the villager (or EventBond) whose friendship / affection ... */
    u16 bondAmount;     /* ...must reach this */
    s32 needFlags;      /* three packed 10-bit event flag ids that must all be set */
    s32 needClear;      /* three that must all be clear */
    s32 prerequisites;  /* two 10-bit ids: a story flag that must be set, one that must be clear */
    FarmEventFlags triggerFlags;    /* EventTriggerFlag bits */
    u8 pad20;
    u8 availability;    /* an availability condition, 0xFF = none */
    u8 triggerMode;     /* which trigger mode fires it */
    u8 triggerValue;    /* the villager (mode 3) or item (mode 4) that trigger wants */
} FarmEventEntry;

/* One step of a farm event script. */
typedef struct FarmScriptStep {
    u8 type;      /* index into the handler table; 0xFF ends the script */
    u8 nextDone;  /* step to jump to when the handler returns 1; 0xFF = the next step */
    u8 nextOther; /* step to jump to otherwise; 0xFF = the next step */
    u8 flags;     /* bit 0: stop after this step */
    s16 command;
    s16 operand;
    s32 x;
    s32 y;
} FarmScriptStep;

typedef s32 (*FarmScriptHandler)(s16, s16, s32, s32);

/* The farm variables the scripts read and write. The villager rows (27 to 39, and 44 to
 * 46) step through the villager records; 43 and 47 step through the helper records; the
 * rest are single values. */
enum EventVar {
    EVENT_VAR_GOLD = 0,                /* player.gold */
    EVENT_VAR_STAMINA = 1,
    EVENT_VAR_MAX_STAMINA = 2,
    EVENT_VAR_BIRTH_SEASON = 3,
    EVENT_VAR_BIRTH_DAY = 4,
    EVENT_VAR_LUMBER = 5,
    EVENT_VAR_FODDER = 6,
    EVENT_VAR_POND_FISH = 7,
    EVENT_VAR_FISH_FOOD = 8,
    EVENT_VAR_CHICKEN_FEED = 9,
    EVENT_VAR_HOUR = 10,
    EVENT_VAR_WEEKDAY = 11,
    EVENT_VAR_YEAR = 12,               /* reported plus one */
    EVENT_VAR_SEASON = 13,
    EVENT_VAR_DAY = 14,
    EVENT_VAR_PLAYER_LOCATION = 15,
    EVENT_VAR_WIFE_REQUESTS = 16,
    EVENT_VAR_PREGNANCY_DAYS = 17,
    EVENT_VAR_CHILD_GROWTH_DAYS = 18,
    EVENT_VAR_WEATHER = 19,
    EVENT_VAR_WEDDING_BRIDE = 20,      /* player.wedding.bride: 0 to 50 */
    EVENT_VAR_PLAYER_X = 21,
    EVENT_VAR_PLAYER_Y = 22,
    EVENT_VAR_PLAYER_Z = 23,
    EVENT_VAR_SHIPPED_VALUE = 24,
    EVENT_VAR_PLAYER_FACING = 25,
    EVENT_VAR_MINUTE = 26,
    EVENT_VAR_VILLAGER_FACING = 27,
    EVENT_VAR_VILLAGER_ACTION = 28,
    EVENT_VAR_VILLAGER_POSITION = 29,
    EVENT_VAR_VILLAGER_SPEED = 30,
    EVENT_VAR_VILLAGER_AFFECTION = 31,
    EVENT_VAR_VILLAGER_FRIENDSHIP = 32,
    EVENT_VAR_VILLAGER_SPRITE80 = 33,
    EVENT_VAR_VILLAGER_EVENT_POSITION = 34,
    EVENT_VAR_VILLAGER_EVENT_SPEED = 35,
    EVENT_VAR_VILLAGER_EVENT_SPRITE = 36,
    EVENT_VAR_VILLAGER_FLAGS = 37,
    EVENT_VAR_VILLAGER_DESTINATION = 38,
    EVENT_VAR_VILLAGER_PREV_DESTINATION = 39,
    EVENT_VAR_DAY_COUNT0 = 40,
    EVENT_VAR_DAY_COUNT1 = 41,
    EVENT_VAR_DAY_VALUE = 42,
    EVENT_VAR_HELPER_AFFECTION = 43,
    EVENT_VAR_VILLAGER_FLAG_0400 = 44, /* one bit of the villager's flags: the row's mask is the value */
    EVENT_VAR_VILLAGER_FLAG_0200 = 45,
    EVENT_VAR_VILLAGER_FLAG_0010 = 46, /* set on the villager of the five gift scenes */
    EVENT_VAR_HELPER_FLAG_0002 = 47,
    EVENT_VAR_FLAGS_FIRST = 0x31,      /* from here on, the event flags themselves: flag = var - EVENT_VAR_FLAG_BIAS */
    EVENT_VAR_FLAG_BIAS = 0x30,
};

/* The bits of FarmEventEntry.triggerFlags: when the entry may fire, besides its other conditions. */
enum EventTriggerFlag {
    EVENT_TRIGGER_BLIZZARD = 0x00000800,
    EVENT_TRIGGER_STORM = 0x00001000,
    EVENT_TRIGGER_RAIN = 0x00002000,
    EVENT_TRIGGER_INVERT_RANGE = 0x00000400, /* the range test is negated */
    EVENT_TRIGGER_FREE_HAND = 0x00010000,    /* needs an empty item hand */
    EVENT_TRIGGER_TOOL_ROOM = 0x00020000,    /* needs a free slot among the rucksack's tools */
    EVENT_TRIGGER_SNOW = 0x00040000,
    EVENT_TRIGGER_SUNNY = 0x00080000,
    EVENT_TRIGGER_WEATHER_ANY = 0x000C3800,  /* all five weather bits: always passes */
    EVENT_TRIGGER_SATURDAY = 0x00100000,
    EVENT_TRIGGER_FRIDAY = 0x00200000,
    EVENT_TRIGGER_THURSDAY = 0x00400000,
    EVENT_TRIGGER_WEDNESDAY = 0x00800000,
    EVENT_TRIGGER_TUESDAY = 0x01000000,
    EVENT_TRIGGER_MONDAY = 0x02000000,
    EVENT_TRIGGER_SUNDAY = 0x04000000,
    EVENT_TRIGGER_WINTER = 0x08000000,
    EVENT_TRIGGER_FALL = 0x10000000,
    EVENT_TRIGGER_SUMMER = 0x20000000,
    EVENT_TRIGGER_SPRING = 0x40000000,
};

/* The fields of the trigger table entries (FarmEventEntry) that are not plain numbers. */
enum EventTriggerField {
    EVENT_TRIGGER_ID_MASK = 0x3FF,   /* the low 10 bits of FarmEventEntry.id; the entry's event number */
    EVENT_TRIGGER_NO_ID = 0x3FF,     /* a packed 10-bit event id of all ones is "none" */
    EVENT_TRIGGER_BOND_BELOW = 0x4000, /* the bond must be under the amount, not over */
    EVENT_WINDOW_ANY = 0x1F,         /* a 5-bit field of the date/time window that matches everything */
    EVENT_STORY_FLAG_BASE = 0xC8,    /* the required story flags are 0xC8 + id (EVENT_FLAG_C9 is id 1) */
    EVENT_DONE_FLAG_BASE = 0xFB,     /* event n's own flag is 0xFB + n */
};

/* Bonds other than the villagers'. */
enum EventBond {
    EVENT_BOND_HORSE = 0x32,
    EVENT_BOND_DOG = 0x33,
};

/* The condition byte of a wedding-role trigger entry. */
enum EventRoleCondition {
    EVENT_ROLE_KIND_MASK = 7,        /* which bride (7 matches any) */
    EVENT_ROLE_ANY_KIND = 7,
    EVENT_ROLE_WITHOUT_ROLE_OK = 0x10, /* passes if the villager does not have VILLAGER_FLAG_WEDDING_ROLE */
    EVENT_ROLE_WITH_ROLE_OK = 0x20,    /* passes if it does */
    EVENT_ROLE_WHEN_SINGLE = 0x40,     /* the entry is enabled while the player has no bride... */
    EVENT_ROLE_WHEN_ENGAGED = 0x80,    /* ...or while he has one */
};

s32 event_state_step(void);
FarmScriptStep *event_result_cursor(void);
void farm_apply_pending_mode(void);
void event_request_start(void);
void event_map_upload(u8 *dst);
s32 event_current_script_id(void);

/* The trigger modes: FarmEventEntry.triggerMode, and the mode a trigger is set to. */
enum EventTriggerMode {
    EVENT_TRIGGER_MODE_ALWAYS = 1,     /* fires with no test of its own */
    EVENT_TRIGGER_MODE_2 = 2,
    EVENT_TRIGGER_MODE_TALK = 3,       /* the villager named by the trigger value is being talked to */
    EVENT_TRIGGER_MODE_GIFT = 4,       /* the villager named by the trigger value is given a gift */
    EVENT_TRIGGER_MODE_5 = 5,
    EVENT_TRIGGER_MODE_6 = 6,
    EVENT_TRIGGER_MODE_7 = 7,
    EVENT_TRIGGER_MODE_8 = 8,
};

/* One script variable cell. The same cell is read at three widths, so it is a
 * union: the word, the low halfword, or the low byte. */
typedef union ScriptVar {
    s32 value;
    s16 half;
    u8  byte;
} ScriptVar;

u8 *event_chunk_read_back(u8 *buffer, s16 x, s16 y, u32 chunk);
void event_sources_reset(void);
void event_enter(void);
u8 *rle_load_cached(u8 *dst, s32 index);
void rle_expand(u8 *dst, u8 *src, s32 count);
void villager_sprites_copy(void);
void farm_pairs_reset(void);
void villagers_clear_flags(void);
void event_exit(void);
void farm_image_queue(void *ot, s16 x, s16 y);
void event_actor_slot_bind(void *file, s32 result);

/* The farm's checksummed upload block. vram_chunk_offsets holds each of its 50
 * chunks' start offsets, vram_stash_rect is the framebuffer rectangle its start
 * is uploaded to. */
extern s32 vram_chunk_offsets[];
/* Checksums of the uploaded block's 50 chunks, and the address of the block's tail copy. */
extern s32 chunk_checksums[50];
extern RECT vram_stash_rect;
extern u32 vram_stash_tail;
/* The ten staging sprites of the event actors. */
extern struct AnimState event_staging_sprites[10];
/* Sprite offsets for the event's sprites. */
extern s16 event_sprite_offset_x;
extern s16 event_sprite_offset_y;
/* Status of the current event request. */
extern s16 event_request_status[];

#endif /* EVENT_SCRIPT_H */
