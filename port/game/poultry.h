#ifndef HMBTN_POULTRY_H
#define HMBTN_POULTRY_H
#include "farm_feed.h"
#include "farm_carry.h"
#include "farm_clock.h"
enum { HMBTN_HENS=10 };
typedef struct HmbtnHen {
    bool occupied,fed_yesterday,prize,dead;
    unsigned affection,age,season,day,days_sick,timer;
    uint16_t name[HMBTN_NAME_CHARS+1];
    HmbtnIntroActor actor;
} HmbtnHen;
typedef struct HmbtnCoopEgg { unsigned item;int32_t position[3]; } HmbtnCoopEgg;
typedef struct HmbtnPoultry {
    HmbtnHen hens[HMBTN_HENS];
    HmbtnCoopEgg eggs[HMBTN_HENS];
    uint32_t random;
    bool upgraded;
} HmbtnPoultry;
unsigned hmbtn_poultry_count(const HmbtnPoultry *p);
bool hmbtn_poultry_room(const HmbtnPoultry *p);
bool hmbtn_poultry_buy(HmbtnPoultry *p,const uint16_t name[HMBTN_NAME_CHARS+1],const HmbtnFarmClock *clock);
void hmbtn_poultry_new_day(HmbtnPoultry *p,HmbtnFarmFeed *feed,const HmbtnFarmClock *clock);
void hmbtn_poultry_tick(HmbtnPoultry *p,const HmbtnModel *ground,const HmbtnIntroActor *player,unsigned hour);
bool hmbtn_poultry_take_egg(HmbtnPoultry *p,HmbtnHouseInteraction *inventory,HmbtnFarmCarry *carry,HmbtnIntroActor *player);
#endif
