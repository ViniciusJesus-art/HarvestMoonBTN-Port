#ifndef HMBTN_HOUSE_SLEEP_H
#define HMBTN_HOUSE_SLEEP_H
#include "farm_intro.h"
/* player mode 0x4A and scene_link_handback. Stops before world overnight work. */
typedef struct HmbtnHouseSleep {
    int32_t from[3], camera[3],target[3];
    unsigned phase, ticks, travel, fade, link_state, link_step;
    bool active, sleeping, second_bed, boundary;
} HmbtnHouseSleep;
void hmbtn_house_sleep_start(HmbtnHouseSleep *sleep,HmbtnIntroActor *player,unsigned house_level);
void hmbtn_house_sleep_tick(HmbtnHouseSleep *sleep,HmbtnIntroActor *player);
#endif
