#ifndef SHOP_DIALOGUES_H
#define SHOP_DIALOGUES_H

/* The shops: their dialogues, pages and the shipping bin. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* The four-word block the shop count panels draw from: the two name pointers,
 * then the count and the price. */
typedef struct ShopMenuHeader {
    u16 *w0;
    u16 *w4;
    s16 h8;
    s16 ha;
} ShopMenuHeader;

/* A farm menu: the shop choice and confirm menus. */
typedef struct FarmMenu {
    u16 *text[10];
    s16 entryValue[10];
    u8 entryFlag[10];
    s16 label46[10];
    u8 columns[2];          /* the value field: its first and last column, in 8-pixel cells */
    s16 selected;
    u8 pad5E[2];
} FarmMenu;

/* A menu request: the menu and three message pairs, passed by value to the menu runners. */
typedef struct FarmMenuRequest {
    FarmMenu *menu;
    s16 first[2];
    s16 second[2];
    s16 third[2];
    s32 count;
} FarmMenuRequest;

/* The dialogues a shop request can start, by index. */
enum ShopDialogue {
    SHOP_DIALOGUE_SEEDS = 0xE,           /* "all the seeds you cannot find at the Supermarket" */
    SHOP_DIALOGUE_POULTRY = 0x10,        /* Chicken Lil's, hens and feed */
    SHOP_DIALOGUE_BARN = 0x11,           /* the poultry shop's twin for cows and sheep */
    SHOP_DIALOGUE_TOOLS = 0x12,          /* the tool shop */
    SHOP_DIALOGUE_PAY = 0x13,
    SHOP_DIALOGUE_DOCTOR = 0x14,         /* the doctor: an exam for 10, then the medicine */
    SHOP_DIALOGUE_15 = 0x15,             /* holds the request busy and nothing more */
    SHOP_DIALOGUE_ITEM_16 = 0x16,
    SHOP_DIALOGUE_ITEM_17 = 0x17,
    SHOP_DIALOGUE_LUMBER = 0x18,
    SHOP_DIALOGUE_GREENHOUSE = 0x19,     /* builds the greenhouse */
    SHOP_DIALOGUE_MEDAL_EXCHANGE = 0x1A, /* medals for a gift */
    SHOP_DIALOGUE_FEE = 0x1B,            /* pays 10 for the phone */
    SHOP_DIALOGUE_PLOT_SALE = 0x1C,      /* sells a plot for 2000 or 500 */
};

/* One runner's row of a race's bet table. */
typedef struct FarmBetRow {
    s16 runner;         /* the runner's name index; -1 when the row is empty (the player's horse) */
    s16 training;       /* the runner's training */
    s16 affection;      /* the runner's affection */
    s16 chance;         /* 0..127: the runner's roll when it leads; 127 minus this when it trails */
    s16 rivalChance;    /* 0..127: the roll against a close rival */
    s16 odds;           /* the payout per ticket */
    s16 tickets;        /* the tickets the player bought on it */
} FarmBetRow;

/* The race bets: three races of six runners, and the player's horse race. In
 * race 3 (the horse race) row [2][2] is the player's horse. */
#define BET_RACES   3
#define BET_RUNNERS 6
#define BET_RACE_HORSE 3

typedef struct FarmRaceBets {
    s16 race;           /* the race bet on: 0..2, or 3 for the horse race */
    FarmBetRow rows[BET_RACES][BET_RUNNERS];
    u8  pad0FE[0x100 - 0xFE];
    s32 horseWon;       /* 1 when the horse race was won */
    s16 medalTotal;     /* the capped medal total after the race */
    u8  pad106[0x108 - 0x106];
} FarmRaceBets;

/* The number of item kinds the shipment and basket count. */
#define SHIPPING_KINDS 25

/* The shipped counts, in the guides' order, and the value they made. */
typedef struct FarmShipment {
    u16 counts[SHIPPING_KINDS];
    s32 shippedValue;   /* the value of today's shipment */
} FarmShipment;

/* The shipping basket: the same tally kept for what has not been shipped yet. */
typedef struct FarmBasket {
    u16 count;          /* the items in the basket, at most 30 */
    u16 items[SHIPPING_KINDS];
    s32 contentsValue;  /* the value of its contents, cleared with them */
} FarmBasket;

/* The message pairs a buy dialogue shows for an item that is chosen, stored or
 * cancelled, then the menu it runs. */
typedef struct FarmBuyDialogue {
    s16 chosen[2];
    u8 pad04[4];
    s16 stored[2];
    u8 pad0C[4];
    s16 cancelled[2];
    u8 pad14[4];
    FarmMenuRequest menu;
} FarmBuyDialogue;

/* The shop dialogues an event can start (EVENT_CMD_SHOP_DIALOGUE's arg). */
enum EventShop {
    EVENT_SHOP_POULTRY = 0,
    EVENT_SHOP_BARN = 1,
    EVENT_SHOP_TOOLS = 2,
    EVENT_SHOP_PAY = 3,
    EVENT_SHOP_DELIVERY = 4,
    EVENT_SHOP_PLOT_UPGRADE = 5,
    EVENT_SHOP_6 = 6,
    EVENT_SHOP_ITEM_16 = 7,
    EVENT_SHOP_ITEM_17 = 8,
    EVENT_SHOP_LUMBER = 9,             /* x != 0 reads EVENT_FLAG_BUILD_IN_PROGRESS instead of starting it */
    EVENT_SHOP_SEEDS = 10,
    EVENT_SHOP_PLOT_SALE = 11,
};
s32 shop_medal_exchange_done(void);
s32 shop_record_list(s16 kind, s16 filter, s16 action);
s32 shop_menu_choose(s32 *step, s32 base, s32 next, struct FarmMenuRequest request,
                         ShopMenuHeader *header);
s32 shop_phase_run(void);
s32 shop_request_dispatch(s32 arg);
s32 shop_result_update(u_long *ot, s16 x, s16 y);
s32 shop_key_value_lookup(s16 key);
s32 shop_dialogue_current(void);
s16 shop_livestock_lookup(s16 mode, s16 filter, s16 wanted);
s32 shop_menu_choose_next(s32 *step, s32 base, s32 next, struct FarmMenuRequest request,
                         ShopMenuHeader *header);
s32 shop_buy_stored_item(s16 *chosen, s16 *stored, s16 *cancelled,
                         struct FarmMenuRequest request);


extern void (*const shop_screen_start_handlers[])(void);

#endif /* SHOP_DIALOGUES_H */
