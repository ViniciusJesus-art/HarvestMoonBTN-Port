#ifndef HMBTN_GAME_STATE_H
#define HMBTN_GAME_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "farm_clock.h"
#include "weather.h"

/* Runtime values, not a serialized PS1 layout. Reference: FarmStoredItem in
 * src/overlay/11_GF_FARM/menu/item_storage.h and FarmPlayer/estate stores in
 * src/main/game/farm_state.h (source revision 4402de2).
 * MA-01 only moves ownership from house_interaction.h; rules are unchanged. */
typedef struct HmbtnStoredTool { uint16_t id, modifier, count; } HmbtnStoredTool;
typedef struct HmbtnInventory {
    HmbtnStoredTool equipped, bag[8], toolbox[64];
    HmbtnStoredTool held_item, items[8];
    HmbtnStoredTool icebox[64],cabinet[64]; /* Original content capacity; one owner, no UI copies. */
    unsigned rucksack_level;
    unsigned rucksack_mornings_left; /* estate.rucksackTimer, second shop offer. */
    unsigned supplies[4]; /* Fodder, lumber, chicken feed, fish food. */
} HmbtnInventory;

/* src/overlay/11_GF_FARM/menu/shop_dialogues.h: FarmShipment;
 * src/overlay/11_GF_FARM/estate.h: FarmLedger;
 * src/overlay/11_GF_FARM/people/player.h: FarmPlayer.gold (purse)
 * (source revision 4402de2). MA-02 moves the current host fields here without
 * changing their algorithms or adopting the serialized PS1 layout. */
typedef struct HmbtnFarmShipping {
    uint32_t pending_value,fish_shipped;
    bool settled,item_shipped,honey_bonus;
} HmbtnFarmShipping;
typedef struct HmbtnEconomy {
    unsigned gold,shipments[25],ledger[4][2];
    unsigned gold_limit; /* 0: base-game default; native sessions may raise it. */
    HmbtnFarmShipping shipping;
} HmbtnEconomy;

/* src/overlay/11_GF_FARM/people/player.h: FarmPlayer.name/birthday;
 * estate.h: FarmEstate.farmName; animals/dog.h: FarmPet.actor.name
 * (same overlay, source revision 4402de2).
 * MA-03 separates the existing host registration values from the name editor.
 * Names retain eight glyphs plus terminator, birthday remains zero-based. */
enum { HMBTN_NAME_CHARS=8 };
typedef struct HmbtnRegistration {
    uint16_t player_name[HMBTN_NAME_CHARS+1],farm_name[HMBTN_NAME_CHARS+1],dog_name[HMBTN_NAME_CHARS+1];
    unsigned birthday_season,birthday_day;
} HmbtnRegistration;

/* HmbtnSession owns gameplay state since MA-04a. The title owns a provisional
 * instance until its one-time registration handoff to a new session. */
/* estate.statusFlags bit 23, independently of event done flags. */
typedef struct HmbtnGameProgress { bool moondrop_seeds; } HmbtnGameProgress;
enum { HMBTN_RECIPE_RECORDS=80 };
typedef struct HmbtnRecipeRecord { uint16_t id,quality,options,values[8]; } HmbtnRecipeRecord;
typedef struct HmbtnCooking { unsigned utensils;HmbtnRecipeRecord records[HMBTN_RECIPE_RECORDS]; } HmbtnCooking;
/* messages.c stores the nine TV Shopping offers in FarmEnding. The date is
 * deliberately a wrapping PS1 u16, independent of the native clock key. */
typedef struct HmbtnTvShopping { uint16_t date;uint8_t offers[9]; } HmbtnTvShopping;
/* FarmBasket plus estate record 18[0]; held basket refers to inventory.held_item. */
typedef struct HmbtnBasket {
    bool owned,placed;
    unsigned area,count,value,shipments[25];
    int32_t position[3];
} HmbtnBasket;
typedef struct HmbtnGameState {
    HmbtnInventory inventory;
    HmbtnEconomy economy;
    /* world/clock.h FarmClock + FarmSystem.clockFrames (farm_state.h). */
    HmbtnFarmClock clock;
    HmbtnRegistration registration;
    HmbtnWeather weather;
    HmbtnGameProgress progress;
    HmbtnCooking cooking;
    HmbtnTvShopping shopping;
    HmbtnBasket basket;
} HmbtnGameState;

/* New game only. Reopening a UI or loading an area must not call this. */
void hmbtn_game_state_init(HmbtnGameState *state);
void hmbtn_registration_init(HmbtnRegistration *registration);
HmbtnInventory *hmbtn_game_inventory(HmbtnGameState *state);
const HmbtnInventory *hmbtn_game_inventory_const(const HmbtnGameState *state);
HmbtnEconomy *hmbtn_game_economy(HmbtnGameState *state);
const HmbtnEconomy *hmbtn_game_economy_const(const HmbtnGameState *state);
HmbtnFarmClock *hmbtn_game_clock(HmbtnGameState *state);
const HmbtnFarmClock *hmbtn_game_clock_const(const HmbtnGameState *state);
HmbtnRegistration *hmbtn_game_registration(HmbtnGameState *state);
const HmbtnRegistration *hmbtn_game_registration_const(const HmbtnGameState *state);

/* Accessible bag capacity; zero for unsupported/invalid current storage levels.
 * The level mapping belongs to the base-game inventory, not the disk reader. */
unsigned hmbtn_inventory_capacity(const HmbtnInventory *inventory);
unsigned hmbtn_inventory_toolbox_capacity(const HmbtnInventory *inventory);

/* Existing farm-menu slot numbering: equipment 0, bag 1..8, hand 9,
 * goods 10..17. Inactive slots and invalid capacity levels return NULL. */
HmbtnStoredTool *hmbtn_inventory_slot(HmbtnInventory *inventory,unsigned index);
const HmbtnStoredTool *hmbtn_inventory_slot_const(const HmbtnInventory *inventory,unsigned index);

#endif
