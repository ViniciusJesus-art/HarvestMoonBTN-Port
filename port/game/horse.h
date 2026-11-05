#ifndef HMBTN_HORSE_H
#define HMBTN_HORSE_H
#include "dog.h"
#include "house_control.h"
/* animals/horse.c: one optional pet, created by the installed Barley script. */
typedef struct HmbtnHorse {
    HmbtnIntroActor actor;
    uint16_t name[HMBTN_NAME_CHARS+1];
    unsigned area,affection,training,year,season,day;
    unsigned phase,step,timer,interrupt,walk_timer,walker_ticks,call_ticks,praise;
    uint32_t random;
    bool owned,adult,riding,in_event,called_today,brushed_today,trained_today,approaching,whistling;
    unsigned ride_phase; /* Mount/dismount ANI, transient. Stable riding is persistent. */
    unsigned track; /* Transient original CD-XA request. */
} HmbtnHorse;
bool hmbtn_horse_spawn(HmbtnHorse *,const HmbtnFarmClock *);
void hmbtn_horse_new_day(HmbtnHorse *,unsigned weather);
void hmbtn_horse_morning(HmbtnHorse *,unsigned weather,const HmbtnFarmClock *);
void hmbtn_horse_clear(HmbtnHorse *);
void hmbtn_horse_stop_riding(HmbtnHorse *);
bool hmbtn_horse_ride_action(HmbtnHorse *,HmbtnIntroActor *,HmbtnHouseControl *,const HmbtnDogWorld *,uint32_t pressed,int *sound);
void hmbtn_horse_rider_sync(HmbtnHorse *,HmbtnIntroActor *,const HmbtnHouseControl *,unsigned area);
void hmbtn_horse_tick(HmbtnHorse *,const HmbtnDogWorld *,unsigned hour);
bool hmbtn_horse_target(const HmbtnHorse *,unsigned area,const HmbtnIntroActor *,unsigned reach);
bool hmbtn_horse_blocked(const HmbtnHorse *,unsigned area,const int32_t from[3],const int32_t to[3]);
bool hmbtn_horse_call_tick(HmbtnHorse *,HmbtnIntroActor *,unsigned area,uint32_t pressed);
unsigned hmbtn_horse_talk(HmbtnHorse *);
bool hmbtn_horse_brush(HmbtnHorse *);
#endif
