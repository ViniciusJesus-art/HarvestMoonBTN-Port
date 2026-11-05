#ifndef HMBTN_HOT_SPRING_H
#define HMBTN_HOT_SPRING_H
#include "farm_action.h"
#include "ground.h"
#include "area_doors.h"
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
    bool door_known;
    unsigned door_model;
    int32_t door_xmin,door_xmax,door_z,door_y,door_front;
} HmbtnHotSpring;
/* Returns consumed also for occupied hands at the correctly faced entry. */
bool hmbtn_hot_spring_begin(HmbtnHotSpring *,HmbtnIntroActor *,bool,
    const uint8_t *,size_t,uint32_t);
void hmbtn_hot_spring_tick(HmbtnHotSpring *,HmbtnIntroActor *,const HmbtnModel *,uint32_t);
void hmbtn_hot_spring_minute(const HmbtnHotSpring *,HmbtnFarmAction *);
void hmbtn_hot_spring_bind(HmbtnHotSpring *,const uint8_t *,size_t);
bool hmbtn_hot_spring_blocked(const HmbtnHotSpring *,const HmbtnAreaDoors *,const int32_t[3],const int32_t[3]);
#endif
