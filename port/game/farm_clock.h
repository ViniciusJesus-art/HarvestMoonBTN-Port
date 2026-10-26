#ifndef HMBTN_FARM_CLOCK_H
#define HMBTN_FARM_CLOCK_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
enum HmbtnClockChange {
    HMBTN_CLOCK_MINUTE=1,HMBTN_CLOCK_HOUR=2,HMBTN_CLOCK_DAY=4,
    HMBTN_CLOCK_SEASON=8,HMBTN_CLOCK_YEAR=16
};
typedef struct HmbtnFarmClock {
    unsigned year,season,day,weekday,hour,minute,frames;
} HmbtnFarmClock;
typedef struct HmbtnDaylight {
    int32_t rgb[3],step[3]; /* Original 20.12 subtractive quad. */
    unsigned period,taken,total;
} HmbtnDaylight;
unsigned hmbtn_farm_clock_date_index(unsigned season,unsigned day);
void hmbtn_farm_clock_init(HmbtnFarmClock *clock);
/* one_minute=30, divided by two in clock_advance_minute. Pauses retain frames. */
unsigned hmbtn_farm_clock_tick(HmbtnFarmClock *clock,bool held);
unsigned hmbtn_farm_clock_advance(HmbtnFarmClock *clock,unsigned minutes);
typedef void (*HmbtnClockObserver)(void *context,const HmbtnFarmClock *clock,unsigned changes);
/* Fast-forward preserves the order of minute events, including skipped days. */
unsigned hmbtn_farm_clock_advance_notify(HmbtnFarmClock *clock,unsigned minutes,
    HmbtnClockObserver observer,void *context);
unsigned hmbtn_farm_clock_sleep_minutes(const HmbtnFarmClock *clock,unsigned fatigue);
bool hmbtn_farm_clock_location_held(unsigned location);
bool hmbtn_farm_clock_place_held(const uint8_t *section,size_t size);
unsigned hmbtn_farm_clock_period(unsigned hour);
unsigned hmbtn_farm_clock_fatigue(unsigned hour);
void hmbtn_daylight_weather_select(HmbtnDaylight *light,const HmbtnFarmClock *clock,unsigned weather);
void hmbtn_daylight_weather_tick(HmbtnDaylight *light,const HmbtnFarmClock *clock,unsigned weather,bool held);
void hmbtn_daylight_select(HmbtnDaylight *light,const HmbtnFarmClock *clock);
void hmbtn_daylight_tick(HmbtnDaylight *light,const HmbtnFarmClock *clock,bool held);
/* Match GPU blend 2 in five-bit channels, before drawing UI. */
void hmbtn_daylight_apply(const HmbtnDaylight *light,uint8_t *rgba,size_t size);
#endif
