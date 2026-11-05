#ifndef HMBTN_DOG_H
#define HMBTN_DOG_H
#include "farm_action.h"
/* animals/{animals,dog}.c: one pet record; the hand only references it.
 * Its name is owned by game.registration.dog_name, not copied here. */
typedef struct HmbtnDogBall {
    bool armed,placed; /* ESTATE_FLAG_TRIGGER_ARMED / stamp 18:1 enabled. */
    unsigned area;
    int32_t position[3];
} HmbtnDogBall;
typedef struct HmbtnDog {
    HmbtnIntroActor actor;
    unsigned area,affection,phase,step,timer,interrupt,walk_timer,walker_ticks;
    unsigned praise,call_ticks,carry_phase;
    uint32_t random;
    bool held,handled_today,called_today,approaching,whistling;
    bool adult,trained_today;
    unsigned training,fetch,fetch_time,fetch_step;
    int sound; /* Transient sound request, not a second audio state. */
    HmbtnDogBall ball;
} HmbtnDog;
typedef struct HmbtnDogWorld {
    unsigned area;
    const HmbtnModel *ground;
    const HmbtnFarmField *field;
    const HmbtnIntroActor *player;
    void *context;
    bool (*blocked)(const void *,const int32_t[3],const int32_t[3]);
} HmbtnDogWorld;
enum { HMBTN_ADULT_DOG=0x106,HMBTN_YOUNG_DOG=0x107,HMBTN_DOG_BALL=0x65 };
/* Shared farm_walker_move adaptation; each pet owns its actor and walker state. */
bool hmbtn_pet_walk(HmbtnIntroActor *,uint32_t *,bool *,unsigned *,unsigned *,
    const HmbtnDogWorld *,unsigned,const int32_t destination[3],bool stop);
unsigned hmbtn_dog_kind(const HmbtnDog *);
void hmbtn_dog_init(HmbtnDog *);
void hmbtn_dog_new_day(HmbtnDog *,unsigned weather);
void hmbtn_dog_morning(HmbtnDog *,unsigned weather,const HmbtnFarmClock *,HmbtnInventory *);
void hmbtn_dog_ball_place(HmbtnDog *,unsigned area,const int32_t position[3]);
bool hmbtn_dog_ball_disarm(HmbtnDog *);
bool hmbtn_dog_fetch(HmbtnDog *,unsigned area,unsigned hour);
void hmbtn_dog_tick(HmbtnDog *,const HmbtnDogWorld *,unsigned hour);
/* Returns sound id through sound, or -1 when no sound is requested. */
bool hmbtn_dog_carry_tick(HmbtnDog *,HmbtnInventory *,HmbtnIntroActor *,const HmbtnDogWorld *,uint32_t,int *sound);
bool hmbtn_dog_call_tick(HmbtnDog *,HmbtnIntroActor *,unsigned area,uint32_t,int *sound);
void hmbtn_dog_latch_place(HmbtnDog *,unsigned area);
void hmbtn_dog_release(HmbtnDog *,HmbtnInventory *,const HmbtnIntroActor *,unsigned area,const HmbtnModel *);
#endif
