#ifndef HMBTN_VILLAGE_H
#define HMBTN_VILLAGE_H
#include "farm_clock.h"
#include "farm_intro.h"
enum { HMBTN_VILLAGERS=37,HMBTN_VILLAGE_FLAGS=512 };
typedef struct HmbtnVillager {
    HmbtnIntroActor actor;
    unsigned friendship,affection,location,step,row,column,wait;
    uint16_t schedule[30][4];
    bool placed,standing,cell_entered;
} HmbtnVillager;
typedef struct HmbtnVillage {
    HmbtnVillager people[HMBTN_VILLAGERS];
    bool flags[HMBTN_VILLAGE_FLAGS];
    const uint8_t *schedule_data;size_t schedule_size;
    unsigned date_key,minute,ann_birthday_season,ann_birthday_day;
    bool invitation_available,dog_held;
} HmbtnVillage;
typedef struct HmbtnVillagePack { unsigned count;uint32_t offsets[32]; } HmbtnVillagePack;
void hmbtn_village_init(HmbtnVillage *v,const uint8_t *data,size_t size);
bool hmbtn_village_sync(HmbtnVillage *v,const HmbtnFarmClock *clock,char *error,size_t size);
void hmbtn_village_tick(HmbtnVillage *v,const uint8_t *section,size_t size,const HmbtnModel *ground,bool held);
void hmbtn_village_enter(HmbtnVillage *v);
/* -1 permits entry; otherwise the original bank-2 refusal message. */
int hmbtn_village_door(HmbtnVillage *v,unsigned event,const HmbtnFarmClock *clock);
bool hmbtn_village_pack(unsigned row,uint32_t *offset,size_t *size,const HmbtnVillagePack **toc);
#endif
