#ifndef HMBTN_POULTRY_H
#define HMBTN_POULTRY_H
#include "farm_action.h"
#include "farm_feed.h"
#include "farm_carry.h"
#include "farm_clock.h"
enum { HMBTN_HENS=10 };
typedef enum HmbtnHenStage { HMBTN_HEN_ADULT,HMBTN_HEN_EGG,HMBTN_HEN_CHICK } HmbtnHenStage;
typedef struct HmbtnHen {
    bool occupied,fed_yesterday,prize,dead,held,fed_today;
    bool naming_pending,died_of_age,golden_egg;
    HmbtnHenStage stage;
    unsigned growth,incubator; /* Original pregnancy counter; bed 1/2, zero when released. */
    unsigned affection,age,season,day,days_sick,timer,praise;
    unsigned area,grazing; /* Logical host area (112 coop, 0 farm), not a PS1 pointer/place. */
    uint16_t name[HMBTN_NAME_CHARS+1];
    HmbtnIntroActor actor;
} HmbtnHen;
typedef struct HmbtnCoopEgg { unsigned item;int32_t position[3];unsigned area; } HmbtnCoopEgg;
typedef struct HmbtnPoultry {
    HmbtnHen hens[HMBTN_HENS];
    HmbtnCoopEgg eggs[HMBTN_HENS];
    uint32_t random;
    bool upgraded;
    unsigned carry_phase; /* 1 pickup, 2 put down; the animal record remains reserved. */
    int sound;
} HmbtnPoultry;
unsigned hmbtn_poultry_count(const HmbtnPoultry *p);
/* Zero-based status row: named chick/adult, including held records; HMBTN_HENS when absent. */
unsigned hmbtn_poultry_status_slot(const HmbtnPoultry *p,unsigned row);
bool hmbtn_poultry_room(const HmbtnPoultry *p);
bool hmbtn_poultry_buy(HmbtnPoultry *p,const uint16_t name[HMBTN_NAME_CHARS+1],const HmbtnFarmClock *clock);
/* Zero-based adult sale list; HMBTN_HENS means no record at that row. */
unsigned hmbtn_poultry_sale_slot(const HmbtnPoultry *p,unsigned row);
/* Clear an eligible record and return its original price, or zero on refusal. */
unsigned hmbtn_poultry_sell(HmbtnPoultry *p,unsigned slot);
/* Player mode 0x51: latch a target in the current area, play bank 22, then
 * consume a dose. A missed use still consumes medicine. */
struct HmbtnLivestock;
bool hmbtn_animal_medicine_tick(HmbtnPoultry *,struct HmbtnLivestock *,unsigned area,HmbtnFarmAction *,
    HmbtnIntroActor *,HmbtnHouseInteraction *,uint32_t pressed);
bool hmbtn_poultry_medicine_tick(HmbtnPoultry *p,unsigned area,HmbtnFarmAction *action,
    HmbtnIntroActor *player,HmbtnHouseInteraction *inventory,uint32_t pressed);
void hmbtn_poultry_new_day(HmbtnPoultry *p,HmbtnFarmFeed *feed,const HmbtnFarmClock *clock);
void hmbtn_poultry_tick(HmbtnPoultry *p,unsigned area,const HmbtnModel *ground,
    const HmbtnFarmField *field,const HmbtnIntroActor *player,unsigned hour);
bool hmbtn_poultry_take_egg(HmbtnPoultry *p,unsigned area,HmbtnHouseInteraction *inventory,HmbtnFarmCarry *carry,HmbtnIntroActor *player);
unsigned hmbtn_poultry_held_slot(const HmbtnPoultry *p);
bool hmbtn_poultry_occupies(const HmbtnPoultry *p,unsigned area,const int32_t point[3]);
bool hmbtn_poultry_carry_tick(HmbtnPoultry *p,HmbtnHouseInteraction *inventory,HmbtnIntroActor *player,
    unsigned area,const HmbtnModel *ground,const HmbtnFarmField *field,uint32_t pressed);
/* Forced placement before sleep; unlike Cross it does not require an empty forward footprint. */
void hmbtn_poultry_release(HmbtnPoultry *p,HmbtnHouseInteraction *inventory,const HmbtnIntroActor *player,
    unsigned area,const HmbtnModel *ground);
void hmbtn_poultry_carry_sync(HmbtnPoultry *p,HmbtnHouseInteraction *inventory);
void hmbtn_poultry_hour(HmbtnPoultry *p,const HmbtnFarmClock *clock,unsigned weather);
/* Returns the original bank-2 refusal, or zero after reserving a free animal record. */
unsigned hmbtn_poultry_incubate(HmbtnPoultry *p,unsigned bed,unsigned item,const HmbtnFarmClock *clock);
bool hmbtn_poultry_incubator_tick(HmbtnPoultry *p,HmbtnFarmFeed *action,HmbtnHouseInteraction *inventory,
    HmbtnIntroActor *player,const HmbtnFarmClock *clock,const uint8_t *section,size_t size,uint32_t pressed);
HmbtnHen *hmbtn_poultry_hatch_ready(HmbtnPoultry *p,unsigned bed);
void hmbtn_poultry_name_end(HmbtnPoultry *p);
bool hmbtn_poultry_dead(const HmbtnPoultry *p,bool old_age);
unsigned hmbtn_poultry_clear_dead(HmbtnPoultry *p);
bool hmbtn_poultry_incubator_position(unsigned bed,int32_t position[3]);
/* text_insert_source 0x1b..0x20: lists by species and cause. */
bool hmbtn_animal_death_expand(const HmbtnPoultry *,const struct HmbtnLivestock *,uint8_t *,size_t);
bool hmbtn_poultry_expand_dead(const HmbtnPoultry *p,uint8_t *record,size_t size);
#endif
