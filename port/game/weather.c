/* Adapted from world/weather.c: weather_roll_tomorrow/step_today/date_next_day,
 * event_commands.c: item_places, screen_fade.c: rand_range (d3c0f38).
 * Native owned state; original seasonal rules, no GPU, packed records or ABI.
 * Separate persistent weather stream: drawing and NPC movement cannot reroll it. */
#include "weather.h"
#include "game_state.h"
#include "psx_random.h"
#include "bride_birthdays.inc"
bool hmbtn_bride_birthday(const HmbtnRegistration *r,unsigned id,unsigned *season,unsigned *day)
{
    for(unsigned i=0;i<5;++i) if((unsigned)bride_birthdays[0][i][0]==id) {
        const unsigned alternate=r && r->birthday_season==(unsigned)bride_birthdays[0][i][1]
            && r->birthday_day==(unsigned)bride_birthdays[0][i][2];
        *season=(unsigned)bride_birthdays[alternate][i][1];*day=(unsigned)bride_birthdays[alternate][i][2];return true;
    }
    return false;
}
static void next(unsigned *season,unsigned *day)
{
    if(++*day==30) { *day=0;*season=(*season+1)%4; }
}
static bool birthday(const HmbtnRegistration *r,unsigned season,unsigned day)
{
    for(unsigned i=0;i<5;++i) {
        unsigned s,d;hmbtn_bride_birthday(r,(unsigned)bride_birthdays[0][i][0],&s,&d);
        if(s==season && d==day) return true;
    }
    return false;
}
unsigned hmbtn_weather_forecast(HmbtnWeather *w,const HmbtnFarmClock *c,
    const HmbtnRegistration *r,const HmbtnWeatherWedding *wedding)
{
    unsigned season=c->season,day=c->day;next(&season,&day);
    unsigned after_season=season,after_day=day;next(&after_season,&after_day);
    w->tomorrow=HMBTN_WEATHER_SUNNY;
    if(hmbtn_farm_clock_date_index(season,day) || birthday(r,season,day)
        || (wedding && wedding->season==season && wedding->day==day)) return w->tomorrow;
    unsigned roll;
    switch(season) {
    case 0:roll=hmbtn_psx_random_range(&w->random,10)+1;if(roll%3==0) w->tomorrow=HMBTN_WEATHER_RAIN;break;
    case 1:roll=hmbtn_psx_random_range(&w->random,20);if(roll==14) w->tomorrow=HMBTN_WEATHER_STORM;
        else if(roll==3) w->tomorrow=HMBTN_WEATHER_RAIN;
        break;
    case 2:if(day!=12 && hmbtn_psx_random_range(&w->random,5)==3) w->tomorrow=HMBTN_WEATHER_RAIN;break;
    case 3:roll=hmbtn_psx_random_range(&w->random,10);if(roll==7) w->tomorrow=HMBTN_WEATHER_BLIZZARD;
        else if(roll&1) w->tomorrow=HMBTN_WEATHER_SNOW;
        break;
    }
    if(hmbtn_farm_clock_date_index(after_season,after_day) && w->tomorrow>=HMBTN_WEATHER_STORM) w->tomorrow-=2;
    return w->tomorrow;
}
void hmbtn_weather_new_day(HmbtnWeather *w,const HmbtnFarmClock *c,
    const HmbtnRegistration *r,const HmbtnWeatherWedding *wedding)
{
    unsigned season=c->season,day=c->day;next(&season,&day);
    w->today=w->tomorrow;
    if(hmbtn_farm_clock_date_index(c->season,c->day)) w->today=HMBTN_WEATHER_SUNNY;
    if(hmbtn_farm_clock_date_index(season,day) && w->today>=HMBTN_WEATHER_STORM) w->today-=2;
    if((wedding && wedding->today) || birthday(r,c->season,c->day)) w->today=HMBTN_WEATHER_SUNNY;
    hmbtn_weather_forecast(w,c,r,wedding);
}
void hmbtn_weather_init(HmbtnWeather *w,const HmbtnFarmClock *c,const HmbtnRegistration *r)
{
    *w=(HmbtnWeather){.random=1};hmbtn_weather_forecast(w,c,r,NULL);
}
