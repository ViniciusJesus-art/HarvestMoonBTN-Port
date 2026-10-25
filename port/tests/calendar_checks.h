#ifndef HMBTN_TEST_CALENDAR_CHECKS_H
#define HMBTN_TEST_CALENDAR_CHECKS_H
#include "game_state.h"
static inline bool clock_equal(const HmbtnFarmClock *a,const HmbtnFarmClock *b)
{
    return a->year==b->year && a->season==b->season && a->day==b->day
        && a->weekday==b->weekday && a->hour==b->hour && a->minute==b->minute && a->frames==b->frames;
}
static inline bool registration_equal(const HmbtnRegistration *a,const HmbtnRegistration *b)
{
    if(a->birthday_season!=b->birthday_season || a->birthday_day!=b->birthday_day) return false;
    for(unsigned i=0;i<=HMBTN_NAME_CHARS;++i)
        if(a->player_name[i]!=b->player_name[i] || a->farm_name[i]!=b->farm_name[i] || a->dog_name[i]!=b->dog_name[i]) return false;
    return true;
}
#endif
