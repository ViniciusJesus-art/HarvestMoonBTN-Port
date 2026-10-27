#ifndef HMBTN_VILLAGE_H
#define HMBTN_VILLAGE_H
#include "farm_clock.h"
#include "weather.h"
#include "area_doors.h"
#include "event_days.h"
#include "farm_intro.h"
enum { HMBTN_VILLAGERS=37,HMBTN_VILLAGE_FLAGS=37*32 }; /* Original eventFlags[37]. */
enum { HMBTN_VILLAGER_TALKED_TODAY=0x200,HMBTN_VILLAGER_DAILY_GAIN=0x400 };
typedef struct HmbtnVillager {
    HmbtnIntroActor actor;
    unsigned friendship,affection,location,step,row,column,wait,delay;
    uint16_t talk_flags;
    uint16_t schedule[30][4];
    bool placed,standing,cell_entered,blocked_by_player;
} HmbtnVillager;
struct HmbtnPoultry;
struct HmbtnConstruction;
struct HmbtnCooking;
typedef struct HmbtnVillage {
    HmbtnVillager people[HMBTN_VILLAGERS];
    HmbtnEventDays days;
    bool flags[HMBTN_VILLAGE_FLAGS];
    const uint8_t *schedule_data;size_t schedule_size;
    /* Original generator, local to villager updates until global RNG is ported. */
    uint32_t random;
    unsigned date_key,minute,ann_birthday_season,ann_birthday_day;
    bool invitation_available,dog_held,player_running;
    const struct HmbtnPoultry *poultry;
    const struct HmbtnConstruction *construction;
    const struct HmbtnCooking *cooking;
    HmbtnAreaDoors *doors; /* Borrowed session controller, never another route owner. */
    const HmbtnWeather *weather; /* Shared session domain; NULL in standalone fixtures. */
    const HmbtnIntroActor *player;
    const HmbtnIntroActor *event_actor;
    unsigned event_actor_id;
    /* Event copies override routes only while their owning script is active. */
    const HmbtnIntroActor *event_actors[HMBTN_VILLAGERS];
} HmbtnVillage;
typedef struct HmbtnVillagePack { unsigned count;uint32_t offsets[32]; } HmbtnVillagePack;
void hmbtn_village_init(HmbtnVillage *v,const uint8_t *data,size_t size);
/* Construction flags are derived from session owners, never copied into flags[].
 * Managed flags are read-only through this interface. Standalone fixtures retain
 * the raw original flags when no domain loan is bound. */
bool hmbtn_village_flag(const HmbtnVillage *v,unsigned id);
bool hmbtn_village_flag_set(HmbtnVillage *v,unsigned id,bool value);
/* Derived script flags, never another weather owner. */
void hmbtn_village_refresh_weather(HmbtnVillage *v);
bool hmbtn_village_sync(HmbtnVillage *v,const HmbtnFarmClock *clock,char *error,size_t size);
void hmbtn_village_tick(HmbtnVillage *v,const uint8_t *section,size_t size,const HmbtnModel *ground,bool held);
/* One active transform per resident. Event aliases point into the session;
 * the persistent route record remains dormant until the binding is released. */
const HmbtnIntroActor *hmbtn_village_event_actor(const HmbtnVillage *v,unsigned id);
const HmbtnIntroActor *hmbtn_village_actor(const HmbtnVillage *v,unsigned id);
/* Strict original NPC hit box. Overlapping entry positions may move apart. */
bool hmbtn_village_blocked(const void *context,const int32_t from[3],const int32_t to[3]);
unsigned hmbtn_village_target(const HmbtnVillage *v,const HmbtnIntroActor *player);
unsigned hmbtn_village_zone_target(const HmbtnVillage *v,const uint8_t *section,size_t size,
    const HmbtnIntroActor *player);
/* Ordered minute/day notifications, including sleep and script fast-forward. */
void hmbtn_village_clock(HmbtnVillage *v,const HmbtnFarmClock *clock,unsigned changes,
    unsigned weather,bool wedding_offer_pending);
void hmbtn_village_enter(HmbtnVillage *v);
/* -1 permits entry; otherwise the original bank-2 refusal message. */
int hmbtn_village_door(HmbtnVillage *v,unsigned event,const HmbtnFarmClock *clock);
bool hmbtn_village_pack(unsigned row,uint32_t *offset,size_t *size,const HmbtnVillagePack **toc);
#endif
