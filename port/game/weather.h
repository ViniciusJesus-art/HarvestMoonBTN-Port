#ifndef HMBTN_WEATHER_H
#define HMBTN_WEATHER_H
#include "farm_clock.h"
struct HmbtnRegistration;
enum HmbtnWeatherKind { HMBTN_WEATHER_SUNNY,HMBTN_WEATHER_RAIN,HMBTN_WEATHER_SNOW,
    HMBTN_WEATHER_STORM,HMBTN_WEATHER_BLIZZARD,HMBTN_WEATHER_KINDS };
typedef struct HmbtnWeather { unsigned today,tomorrow;uint32_t random; } HmbtnWeather;
/* No wedding domain exists yet. NULL means no scheduled wedding; callers with
 * that domain can provide its date without embedding family state here. */
typedef struct HmbtnWeatherWedding { unsigned season,day;bool today; } HmbtnWeatherWedding;
unsigned hmbtn_weather_forecast(HmbtnWeather *weather,const HmbtnFarmClock *clock,
    const struct HmbtnRegistration *registration,const HmbtnWeatherWedding *wedding);
void hmbtn_weather_new_day(HmbtnWeather *weather,const HmbtnFarmClock *clock,
    const struct HmbtnRegistration *registration,const HmbtnWeatherWedding *wedding);
void hmbtn_weather_init(HmbtnWeather *weather,const HmbtnFarmClock *clock,
    const struct HmbtnRegistration *registration);
/* Shared with conversations; alternate birthday when it matches the player. */
bool hmbtn_bride_birthday(const struct HmbtnRegistration *registration,unsigned id,
    unsigned *season,unsigned *day);
#endif
