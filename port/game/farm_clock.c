/* Native clock.c/weather.c subset: cadence, 06:00 date rollover, calendar,
 * sleep duration and the original subtractive light, without world callbacks. */
#include "farm_clock.h"
#include "psx_data.h"
#include <string.h>
#include "clock_tables.inc"
void hmbtn_farm_clock_init(HmbtnFarmClock *c)
{
    *c=(HmbtnFarmClock){0,0,1,1,6,0,0};
}
static unsigned minute(HmbtnFarmClock *c)
{
    unsigned changes=HMBTN_CLOCK_MINUTE;
    if(++c->minute<60) return changes;
    c->minute=0;changes|=HMBTN_CLOCK_HOUR;
    c->hour=(c->hour+1)%24;
    if(c->hour!=6) return changes;
    changes|=HMBTN_CLOCK_DAY;c->weekday=(c->weekday+1)%7;
    if(++c->day<30) return changes;
    c->day=0;changes|=HMBTN_CLOCK_SEASON;
    if(++c->season<4) return changes;
    c->season=0;if(c->year<98) ++c->year;
    return changes|HMBTN_CLOCK_YEAR;
}
unsigned hmbtn_farm_clock_tick(HmbtnFarmClock *c,bool held)
{
    if(held || ++c->frames<15) return 0;
    c->frames=0;return minute(c);
}
unsigned hmbtn_farm_clock_advance(HmbtnFarmClock *c,unsigned minutes)
{
    return hmbtn_farm_clock_advance_notify(c,minutes,NULL,NULL);
}
unsigned hmbtn_farm_clock_advance_notify(HmbtnFarmClock *c,unsigned minutes,HmbtnClockObserver observer,void *context)
{
    unsigned changes=0;
    while(minutes--) {
        const unsigned change=minute(c);changes|=change;
        if(observer) observer(context,c,change);
    }
    c->frames=0;return changes;
}
unsigned hmbtn_farm_clock_sleep_minutes(const HmbtnFarmClock *c,unsigned fatigue)
{
    const unsigned extra=fatigue<50 ? 0 : fatigue<70 ? 120 : fatigue<80 ? 240 : fatigue<100 ? 360 : 0;
    return ((5+24-c->hour)%24)*60+60-c->minute+extra;
}
bool hmbtn_farm_clock_place_held(const uint8_t *section,size_t size)
{
    if(!section || size<16) return true;
    const unsigned place=hmbtn_read_le16(section+2);
    return place>=78 || clock_places[place]==0x1000;
}
unsigned hmbtn_farm_clock_period(unsigned hour)
{
    return hour>=5 && hour<7 ? 0 : hour>=7 && hour<15 ? 1 : hour>=15 && hour<17 ? 2 : 3;
}
unsigned hmbtn_farm_clock_fatigue(unsigned hour)
{
    for(unsigned i=0;i<4;++i) if(hour==(unsigned)hour_fatigue[i][0]) return (unsigned)hour_fatigue[i][1];
    return 0;
}
void hmbtn_daylight_select(HmbtnDaylight *l,const HmbtnFarmClock *c)
{
    memset(l,0,sizeof(*l));l->period=hmbtn_farm_clock_period(c->hour);
    memcpy(l->rgb,drift_by_hour[l->period],sizeof(l->rgb));
}
void hmbtn_daylight_tick(HmbtnDaylight *l,const HmbtnFarmClock *c,bool held)
{
    const unsigned period=hmbtn_farm_clock_period(c->hour);
    if(period!=l->period) {
        l->period=period;l->taken=0;l->total=256;
        for(unsigned i=0;i<3;++i) l->step[i]=(drift_by_hour[period][i]-l->rgb[i])/256;
    }
    if(!held && l->taken<l->total) {
        ++l->taken;for(unsigned i=0;i<3;++i) l->rgb[i]+=l->step[i];
    }
}
void hmbtn_daylight_apply(const HmbtnDaylight *l,uint8_t *rgba,size_t size)
{
    if(!(l->rgb[0]|l->rgb[1]|l->rgb[2])) return;
    for(size_t i=0;i+3<size;i+=4) for(unsigned ch=0;ch<3;++ch) {
        int color=(rgba[i+ch]>>3)-(l->rgb[ch]>>15);
        if(color<0) color=0;
        rgba[i+ch]=(uint8_t)((color<<3)|(color>>2));
    }
}
