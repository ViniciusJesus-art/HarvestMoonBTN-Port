#ifndef ESTATE_H
#define ESTATE_H

/* The estate: the buildings, the books and the farm's records. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "cue.h"
#include "farm_tables.h"
#include "house.h"
#include "item_storage.h"
#include "shop_dialogues.h"

typedef struct FarmGroundProbe
{
    s32 x;
    s32 y;
    s32 z;
} FarmGroundProbe;


struct FarmGroundProbe;

/* The screen position estate_point_project projects a point to: x, y and the depth
 * (the GTE's OTZ, an ordering-table index times four). */
typedef struct FarmScreenPoint {
    s32 x;
    s32 y;
    s32 depth;
} FarmScreenPoint;

/* The messages of text bank 2 posted when the player's child is born: the congratulations,
 * each a short letter signed by its sender. */
enum EstateMessage {
    ESTATE_MESSAGE_BIRTH_KAI_POPURI_PRESENT = 0x31, /* "...happy as if it were my own son! Popuri is happy too" */
    ESTATE_MESSAGE_BIRTH_KAI_POPURI_AWAY = 0x32,    /* "...Show him to me in the summer!" */
    /* Four letters, one per rival couple but Popuri-Kai, numbered from
     * ESTATE_MESSAGE_BIRTH_RIVAL_FIRST. The texts are signed Cliff & Ann (0x33),
     * Cliff & Karen (0x34, "May the Poultry Farm and the <farm> Farm live on"), Doctor & Elli
     * (0x35), Mary & Gray (0x36). */
    ESTATE_MESSAGE_BIRTH_RIVAL_FIRST = 0x33,
    ESTATE_MESSAGE_BIRTH_MAYOR = 0x37,              /* signed Thomas & Harris */
};

/* One season's books. Both are amounts: every reader compares or divides them
 * signed. */
typedef struct FarmLedger {
    s32 earnings;       /* max 9,999,999 */
    s32 expenses;
} FarmLedger;

/* FarmEstate.estateFlags. */
enum EstateFlag {
    ESTATE_FLAG_VASE_SET       = (1 << 1),
    ESTATE_FLAG_CATCH_ED       = (1 << 2),  /* the fishing specials, one per catch item */
    ESTATE_FLAG_CATCH_EE       = (1 << 3),
    ESTATE_FLAG_CATCH_EF       = (1 << 4),
    ESTATE_FLAG_CATCH_F0       = (1 << 5),
    ESTATE_FLAG_CATCH_F1       = (1 << 6),
    ESTATE_FLAG_CATCH_F2       = (1 << 7),
    ESTATE_FLAG_CATCHES        = (ESTATE_FLAG_CATCH_ED | ESTATE_FLAG_CATCH_EE | ESTATE_FLAG_CATCH_EF | ESTATE_FLAG_CATCH_F0 | ESTATE_FLAG_CATCH_F1 | ESTATE_FLAG_CATCH_F2),
    ESTATE_FLAG_BARN_UPGRADED  = (1 << 8),
    ESTATE_FLAG_BARN_FULL      = (1 << 9),
    ESTATE_FLAG_ITEM_83        = (1 << 10),
    ESTATE_FLAG_BASKET         = (1 << 11),
    ESTATE_FLAG_GREENHOUSE     = (1 << 13),
    ESTATE_FLAG_CHICKEN_NOTICE = (1 << 14),
    ESTATE_FLAG_VISITORS       = (1 << 15),
    ESTATE_FLAG_BIT16          = (1 << 16),
    ESTATE_FLAG_BIT17          = (1 << 17),
    ESTATE_FLAG_NOON_CHIME     = (1 << 18),
    ESTATE_FLAG_EVENING_CHIME  = (1 << 19),
    ESTATE_FLAG_COOP_BUILT     = (1 << 20),
    ESTATE_FLAG_BIT21          = (1 << 21),
    ESTATE_FLAG_ROW97_BONUS    = (1 << 22),
    ESTATE_FLAG_STRAY_DOGS     = (1 << 23),
    ESTATE_FLAG_DOG_WALK       = (1 << 24),
    ESTATE_FLAG_TRIGGER_ARMED  = (1 << 25),
    ESTATE_FLAG_TAKE_ONCE      = (1 << 26),
    ESTATE_FLAG_ANIMAL_AGED    = (1 << 27),
};

/* FarmEstate.statusFlags. */
enum EstateStatus {
    ESTATE_STATUS_SHIPPED_5D              = (1 << 0),
    ESTATE_STATUS_SHIPPED_5F              = (1 << 1),
    ESTATE_STATUS_GROUP_A_DONE            = (1 << 2),
    ESTATE_STATUS_GROUP_B_DONE            = (1 << 3),
    ESTATE_STATUS_GROUP_C_DONE            = (1 << 4),
    ESTATE_STATUS_NO_BASKET               = (1 << 5),
    ESTATE_STATUS_NO_RUCKSACK             = (1 << 6),
    ESTATE_STATUS_RUCKSACK_UPGRADE_OPEN   = (1 << 7),
    ESTATE_STATUS_AFFECTION_OVER_50000    = (1 << 8),
    ESTATE_STATUS_NO_COOP                 = (1 << 9),   /* the coop is not built */
    ESTATE_STATUS_NO_HOUSE                = (1 << 10),
    ESTATE_STATUS_NO_BARN                 = (1 << 11),
    ESTATE_STATUS_HOUSE_LEVEL_1           = (1 << 12),
    ESTATE_STATUS_NO_GREENHOUSE           = (1 << 13),
    ESTATE_STATUS_NO_ANIMAL_MEDICINE      = (1 << 14),
    ESTATE_STATUS_BARN_ORDER_OPEN         = (1 << 15),
    ESTATE_STATUS_COOP_ORDER_OPEN         = (1 << 16),
    ESTATE_STATUS_BARN_FULL_ORDER_OPEN    = (1 << 17),
    ESTATE_STATUS_NO_BRUSH                = (1 << 18),
    ESTATE_STATUS_NO_MILKER               = (1 << 19),
    ESTATE_STATUS_NO_CLIPPERS             = (1 << 20),
    ESTATE_STATUS_NO_COW_MIRACLE_POTION   = (1 << 21),
    ESTATE_STATUS_NO_SHEEP_MIRACLE_POTION = (1 << 22),
    ESTATE_STATUS_BIT23                   = (1 << 23),
};

/* The capacities of FarmEstate's arrays. The feed bins are the sweepers' loop
 * bounds; the cooking table has 80 recipe slots. */
#define BARN_FEED_BINS      22
#define COOP_FEED_BINS      10
#define ITEM_STORE_SLOTS    64
#define LEDGER_COUNT        4
#define MESSAGE_QUEUES      2
#define MESSAGE_QUEUE_LEN   20
#define RECIPE_SLOTS        80
#define ESTATE_STOCK_MAX    999

/* The farm itself: buildings, stock, stores, the books, the messages and records. */
typedef struct FarmEstate {
    u8   utensils;          /* a bitmask, one per kitchen tool */
    u8   discardCount;
    /* The building levels. */
    u16 houseLevel;         /* 0..2 */
    u16 barnLevel;          /* 0..3 */
    u16 coopLevel;          /* 0..2 */
    u16 greenhouseLevel;    /* 0..1 */
    u16 pendingRequest;
    s16 stampTimer;
    s16 orderTimer;
    s16 shipmentTotal;
    s16  lumberDeliveries;
    u16 farmName[9];        /* the farm's name, entered on the title screen */
    u8  pad026[0x02C - 0x026];
    u32 estateFlags;        /* ESTATE_FLAG_* */
    s32 noticesSeen;
    /* The stock: four counters clamped to 999, and the fish in the pond between them. */
    u16 lumber;
    u16 fodder;
    s16 pondFish;           /* 0..63 */
    u16 fishFood;
    u16 chickenFeed;
    FarmStoredItem iceboxItems[ITEM_STORE_SLOTS];     /* a run of 64 item records */
    FarmStoredItem toolboxItems[ITEM_STORE_SLOTS];     /* the toolboxes, 8 boxes x 8 slots;
                                                     * the new game's five tools go in box 1 */
    FarmStoredItem cabinetItems[ITEM_STORE_SLOTS];     /* a run of 64 item records */
    FarmShipment shipment;  /* what has been shipped */
    FarmLedger ledger[LEDGER_COUNT];    /* [0] the current season */
    FarmMessage queues[MESSAGE_QUEUES][MESSAGE_QUEUE_LEN];  /* two queues of twenty */
    s16 powerBerries;       /* the power berries eaten */
    u16 eventBits;
    FarmBasket basket;      /* the shipping basket */
    /* The feed bins, barn and coop. Each has a sweeper that scans it and clears
     * the first set entry. */
    u8  barnFeedBins[BARN_FEED_BINS];
    u8  coopFeedBins[COOP_FEED_BINS];
    FarmRecipe recipeRecords[RECIPE_SLOTS];
    u32 statusFlags;        /* ESTATE_STATUS_* */
    s16  rucksackTimer;
    s16 carpenterOrder;
} FarmEstate;

/* The estate's record runs. */
#define ESTATE_RUNS 19

/* The runs the events use: the placed items, and the stamps (the basket, then the dog ball). */
enum EstateRun {
    ESTATE_RUN_ITEMS = 17,
    ESTATE_RUN_STAMPS = 18,
};
enum EstateStampRecord {
    ESTATE_STAMP_BASKET = 0,
    ESTATE_STAMP_DOG_BALL = 1,
};

/* FarmEstateRecord.recordFlags. */
enum EstateRecordFlag {
    ESTATE_RECORD_ENABLED = 0x1,
    ESTATE_RECORD_BOUNCE = 0x100,
    ESTATE_RECORD_RUCKSACK = 0x8000,    /* the rucksack record: its item steps up with the upgrade */
    ESTATE_RECORD_CLAIMED = 0x4000,
};

typedef struct FarmRuns {
    FarmEstateRecord *records[ESTATE_RUNS];
    s16 counts[ESTATE_RUNS];
} FarmRuns;

s32 estate_gold_change(s32 amount, s32 reset);
void estate_pending_op_tick(void);
s32 estate_pending_op_start(void);
void estate_queues_clear(void);
void estate_timed_message_add(s16 kind, s16 message, s16 timer);
void estate_queues_daily(void);
/* The mode of a lumber add: where the lumber came from. */
enum LumberAddMode {
    LUMBER_ADD_CARRIED = 0,
    LUMBER_ADD_PICKED = 1,
    LUMBER_ADD_STUMP = 2,
    LUMBER_ADD_BOUGHT = 3,
};

/* The mode of a fodder add: where the fodder came from. */
enum FodderAddMode {
    FODDER_ADD_CARRIED = 0,
    FODDER_ADD_PICKED = 1,
    FODDER_ADD_BOUGHT = 2,
};

/* The mode of a fish food add: one unit, or the amount given. */
enum FishFoodAddMode {
    FISH_FOOD_ADD_ONE = 0,
    FISH_FOOD_ADD_AMOUNT = 1,
};

void estate_lumber_add(u16 mode, u16 amount);
s32 estate_lumber_take(u32 amount);
void estate_fodder_add(u16 mode, u16 amount);
s32 estate_fodder_take(u16 amount);
void estate_fish_food_add(u16 mode, u16 amount);
s32 estate_fish_food_take(u16 amount);
void estate_chicken_feed_add(u16 mode, u16 amount);
s32 estate_chicken_feed_take(u16 amount);
s32 estate_shipment_add(s32 itemId);
s16 estate_item_slot_index(s32 itemId);
void estate_feed_bins_clear(void);
s32 estate_shipment_settle(void);
s32 estate_shipment_count(s32 id);
s32 estate_basket_add(s32 itemId);
s32 estate_basket_merge(void);
s32 estate_flag_take_once(void);
void estate_items_refresh(void);
void estate_selection_refresh(s32 selection);
void estate_record_tables_refresh(void);
void estate_seasonal_refresh(void);
s32 estate_field_pickup(FarmEstateRecord *record, FarmStoredItem *result);
s16 estate_record_item_id(FarmEstateRecord *record);
void estate_result_window_clear(u16 value, s32 clear);
void estate_items_reset(FarmEstateRecord *p, s32 count);
void estate_marker_stamp(u16 kind, VECTOR position, u16 value, s32 index);
s32 estate_result_select(u16 kind, VECTOR *position, s16 *area, s32 index);
void estate_record_publish(s16 id, s32 publish);
s32 estate_record_submit(FarmEstateRecord *record);
s32 estate_record_submit_limited(FarmEstateRecord *record, s32 reach, s32 power);
s32 estate_record_submit_costed(FarmEstateRecord *record, s32 reach, s32 power);
FarmEstateRecord *estate_request_near_active(struct FarmGroundProbe *request);
void estate_slots_set_claimed(s32 mode);
s32 estate_slots_free_count(void);
s32 estate_request_begin(s32 request);
void estate_fatigue_tick(void);
void estate_request_clear(void);
void estate_trigger_stamp_second(void);
void estate_trigger_stamp_first(void);
void estate_basket_check(void);
void estate_trigger_fire(void);
s32 estate_point_on_screen(s32 x, s32 y);
s32 estate_point_project(SVECTOR *input, FarmScreenPoint *result);

/* One row of a result table: three words, then the key they answer. */
typedef struct FarmResultRow {
    s32 words[3];
    s32 key;
} FarmResultRow;

s32 item_code_to_picture(s32 code, s32 *variant, s32 *index);
s32 estate_code_value_4(s32 code, s32 *result, s32 *value);
s32 estate_code_value_9(s32 code, s32 *out1, s32 *out2);

extern u32 record18_bounce_step;

#endif /* ESTATE_H */
