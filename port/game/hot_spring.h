#ifndef HMBTN_HOT_SPRING_H
#define HMBTN_HOT_SPRING_H
#include "farm_action.h"
#include "ground.h"
/* Transient original player modes 3b/56/58/57/4b and zone walks. */
typedef enum HmbtnSpringPhase { HMBTN_SPRING_OFF,HMBTN_SPRING_APPROACH,
    HMBTN_SPRING_ENTER,HMBTN_SPRING_SPLASH,HMBTN_SPRING_SOAK,
    HMBTN_SPRING_REST,HMBTN_SPRING_LEAVE,HMBTN_SPRING_WALK } HmbtnSpringPhase;
typedef struct HmbtnHotSpring {
    HmbtnSpringPhase phase;
    unsigned ticks;
    unsigned walk_ticks;
    int32_t exit_box[4];
    int velocity,bank,sound;
    bool landed,finished,exit_known,exit_pending;
} HmbtnHotSpring;
/* Returns consumed also for occupied hands at the correctly faced entry. */
bool hmbtn_hot_spring_begin(HmbtnHotSpring *,HmbtnIntroActor *,bool,
    const uint8_t *,size_t,uint32_t);
void hmbtn_hot_spring_tick(HmbtnHotSpring *,HmbtnIntroActor *,const HmbtnModel *,uint32_t);
void hmbtn_hot_spring_minute(const HmbtnHotSpring *,HmbtnFarmAction *);
#endif
