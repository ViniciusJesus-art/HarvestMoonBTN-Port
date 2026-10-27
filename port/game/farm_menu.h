#ifndef HMBTN_FARM_MENU_H
#define HMBTN_FARM_MENU_H
#include "farm_action.h"
#include "inventory.h"
typedef enum HmbtnFarmMenuMode { HMBTN_MENU_NONE,HMBTN_MENU_STATUS,HMBTN_MENU_RUCKSACK } HmbtnFarmMenuMode;
typedef struct HmbtnStorageSpot { int x,y;uint8_t next[24]; } HmbtnStorageSpot;
typedef struct HmbtnFarmStatistics {
    unsigned animals[3],fish,discarded,power_berries;
    unsigned dog_affection,helper_affection[7];
} HmbtnFarmStatistics;
typedef struct HmbtnFarmMenu {
    HmbtnFarmMenuMode mode;
    unsigned phase,fade,page,cycle,cursor,marked,repeat_frames;
    unsigned list_top;
    uint32_t previous_arrows;
    bool detail,closing;
    unsigned request,loaded;
    HmbtnMessage message;
    int sound;
    HmbtnFarmStatistics stats;
    const HmbtnEconomy *economy; /* Borrowed read-only view; no financial snapshot. */
    const HmbtnCooking *cooking;
    const struct HmbtnConstruction *construction;
    const struct HmbtnPoultry *poultry; /* Borrow the session records; never a serialized address. */
} HmbtnFarmMenu;
/* Bind the game's live economy; does not initialize or reset financial data. */
void hmbtn_farm_menu_init(HmbtnFarmMenu *menu,const HmbtnEconomy *economy);
bool hmbtn_farm_menu_open(HmbtnFarmMenu *menu,uint32_t pressed);
void hmbtn_farm_menu_tick(HmbtnFarmMenu *menu,HmbtnInventory *inventory,uint32_t pressed,uint32_t held);
/* status_pages.c: building contribution is derived, never a cached level. */
unsigned hmbtn_farm_menu_building_score(const HmbtnFarmMenu *menu);
const HmbtnStorageSpot *hmbtn_storage_spot(unsigned index);
#endif
