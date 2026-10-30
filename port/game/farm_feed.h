#ifndef HMBTN_FARM_FEED_H
#define HMBTN_FARM_FEED_H
#include "house_interaction.h"
#include "makers.h"
typedef struct HmbtnFarmFeed {
    bool barn[22],coop[10];
    unsigned phase,request;
    int sound;
} HmbtnFarmFeed;
/* Shared estate stocks and held item; no parallel inventory. */
bool hmbtn_farm_feed_tick(HmbtnFarmFeed *feed,HmbtnHouseInteraction *inventory,
    HmbtnIntroActor *player,const uint8_t *section,size_t size,uint32_t pressed);
bool hmbtn_farm_feed_makers_tick(HmbtnFarmFeed *,const HmbtnMakers *,HmbtnHouseInteraction *,
    HmbtnIntroActor *,unsigned area,const uint8_t *,size_t,uint32_t pressed);
void hmbtn_farm_feed_new_day(HmbtnFarmFeed *feed);
bool hmbtn_farm_feed_position(bool barn,unsigned bin,int32_t position[3]);
/* Active troughs come from the selected original section, including maternity
 * stalls. They are not an animal count or a second stored building level. */
uint32_t hmbtn_farm_feed_bins(const uint8_t *section,size_t size,bool barn);
#endif
