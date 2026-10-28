#ifndef HMBTN_HOUSE_LUMBER_H
#define HMBTN_HOUSE_LUMBER_H
#include "farm_action.h"
/* player_zone_lumber_stock / PLAYER_MODE_DELIVER_LUMBER. Stocks and held item
 * belong to inventory; the fire latch is village.flags[0x45], reset on entry. */
typedef struct HmbtnHouseLumber { unsigned phase,deliveries; } HmbtnHouseLumber;
bool hmbtn_house_lumber_tick(HmbtnHouseLumber *,HmbtnInventory *,HmbtnIntroActor *,
    HmbtnFarmAction *,bool *lit,const uint8_t *section,size_t size,uint32_t pressed,int *sound);
#endif
