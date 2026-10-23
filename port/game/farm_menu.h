#ifndef HMBTN_FARM_MENU_H
#define HMBTN_FARM_MENU_H
#include "farm_action.h"
typedef enum HmbtnFarmMenuMode { HMBTN_MENU_NONE,HMBTN_MENU_STATUS,HMBTN_MENU_RUCKSACK } HmbtnFarmMenuMode;
typedef struct HmbtnStorageSpot { int x,y;uint8_t next[24]; } HmbtnStorageSpot;
typedef struct HmbtnFarmStatistics {
    unsigned gold,shipments[25],animals[3],fish,discarded,power_berries;
    unsigned building_score,recipes,dog_affection,helper_affection[7];
    unsigned ledger[4][2];
} HmbtnFarmStatistics;
typedef struct HmbtnFarmMenu {
    HmbtnFarmMenuMode mode;
    unsigned phase,fade,page,cycle,cursor,marked,repeat_frames;
    uint32_t previous_arrows;
    bool detail,closing;
    unsigned request,loaded;
    HmbtnMessage message;
    int sound;
    HmbtnFarmStatistics stats;
} HmbtnFarmMenu;
void hmbtn_farm_menu_init(HmbtnFarmMenu *menu);
bool hmbtn_farm_menu_open(HmbtnFarmMenu *menu,uint32_t pressed);
void hmbtn_farm_menu_tick(HmbtnFarmMenu *menu,HmbtnHouseInteraction *inventory,uint32_t pressed,uint32_t held);
const HmbtnStorageSpot *hmbtn_storage_spot(unsigned index);
HmbtnStoredTool *hmbtn_inventory_slot(HmbtnHouseInteraction *inventory,unsigned index);
bool hmbtn_inventory_swap(HmbtnHouseInteraction *inventory,unsigned from,unsigned to);
bool hmbtn_inventory_cycle(HmbtnHouseInteraction *inventory,bool tools,bool reverse);
#endif
