#ifndef HMBTN_LIVESTOCK_H
#define HMBTN_LIVESTOCK_H
#include "farm_feed.h"
#include "construction.h"
#include "event_item.h"
enum { HMBTN_LIVESTOCK=20 };
/* One record per cow/sheep; stage, art, counts and capacity are derived.
 * Original content capacity, not a serialized PS1 layout or hardware ceiling. */
typedef struct HmbtnFarmAnimal {
    bool occupied,sheep,dead,died_of_age,fed_today,fed_yesterday,talked_today;
    bool brushed_today,milked_today,prize_winner;
    unsigned growth,affection,age,season,day,days_sick,days_since_shearing;
    unsigned area,timer,state,praise;
    uint16_t name[HMBTN_NAME_CHARS+1];
    HmbtnIntroActor actor;
} HmbtnFarmAnimal;
typedef struct HmbtnLivestock {
    HmbtnFarmAnimal animals[HMBTN_LIVESTOCK];
    uint32_t random;
    const HmbtnConstruction *construction; /* Borrowed, no duplicate barn level. */
    unsigned talk_slot; /* Transient message source, never another animal record. */
} HmbtnLivestock;
unsigned hmbtn_livestock_kind(const HmbtnFarmAnimal *);
unsigned hmbtn_livestock_pack(const HmbtnFarmAnimal *);
unsigned hmbtn_livestock_scale(const HmbtnFarmAnimal *);
unsigned hmbtn_livestock_capacity(const HmbtnLivestock *);
unsigned hmbtn_livestock_free(const HmbtnLivestock *);
unsigned hmbtn_livestock_count(const HmbtnLivestock *,bool sheep);
unsigned hmbtn_livestock_slot(const HmbtnLivestock *,bool sheep,unsigned row);
unsigned hmbtn_livestock_sale_slot(const HmbtnLivestock *,bool sheep,unsigned row);
unsigned hmbtn_livestock_sale_price(const HmbtnLivestock *,unsigned slot);
unsigned hmbtn_livestock_sell(HmbtnLivestock *,unsigned slot);
bool hmbtn_livestock_buy(HmbtnLivestock *,unsigned item,const uint16_t *name,const HmbtnFarmClock *);
void hmbtn_livestock_new_day(HmbtnLivestock *,HmbtnFarmFeed *,const HmbtnFarmClock *);
void hmbtn_livestock_tick(HmbtnLivestock *,unsigned area,const HmbtnModel *,const HmbtnIntroActor *,unsigned hour);
unsigned hmbtn_livestock_target(const HmbtnLivestock *,unsigned area,const HmbtnIntroActor *,unsigned reach);
unsigned hmbtn_livestock_talk(HmbtnLivestock *,unsigned slot);
bool hmbtn_livestock_expand(const HmbtnLivestock *,uint8_t *,size_t);
bool hmbtn_livestock_cure(HmbtnLivestock *,unsigned slot);
bool hmbtn_livestock_brush(HmbtnLivestock *,unsigned slot);
unsigned hmbtn_livestock_harvest(HmbtnLivestock *,unsigned slot,bool shear);
bool hmbtn_livestock_tool_tick(HmbtnLivestock *,unsigned area,HmbtnFarmAction *,HmbtnIntroActor *,
    HmbtnHouseInteraction *,HmbtnEventItem *,uint32_t pressed);
bool hmbtn_livestock_blocked(const HmbtnLivestock *,unsigned area,const int32_t from[3],const int32_t to[3]);
#endif
