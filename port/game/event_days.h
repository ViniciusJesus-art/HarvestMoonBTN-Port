#ifndef HMBTN_EVENT_DAYS_H
#define HMBTN_EVENT_DAYS_H
#include "farm_clock.h"
/* PS1 byte-sized story counters; 255 means stopped, running timers cap at 120. */
typedef struct HmbtnEventDays {
    uint8_t count0[128],count1[8],values[128];
} HmbtnEventDays;
void hmbtn_event_days_init(HmbtnEventDays *days);
void hmbtn_event_days_advance(HmbtnEventDays *days,const HmbtnFarmClock *clock,
    unsigned weather,bool wedding_offer_pending);
#endif
