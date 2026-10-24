#include "event_days.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Event days line %d: %s\n",__LINE__,#x);return 1; } } while(0)
static void clock_event(void *context,const HmbtnFarmClock *clock,unsigned changes)
{
    if(changes&HMBTN_CLOCK_DAY) hmbtn_event_days_advance(context,clock,0,false);
}
int main(void)
{
    HmbtnEventDays days;hmbtn_event_days_init(&days);
    CHECK(days.count0[0]==255 && days.count0[1]==1 && days.count0[6]==30
        && days.count0[12]==31 && days.count0[16]==0 && days.count0[21]==28
        && days.count0[22]==0 && days.count0[24]==118 && days.count0[25]==28 && days.count0[44]==0);
    for(unsigned i=0;i<8;++i) CHECK(days.count1[i]==255);
    for(unsigned i=0;i<128;++i) CHECK(!days.values[i]);
    HmbtnFarmClock c={0,0,2,2,6,0,0};days.count1[0]=0;days.values[10]=254;
    hmbtn_event_days_advance(&days,&c,0,false);
    CHECK(days.count0[0]==255 && days.count0[1]==2 && days.count0[24]==119 && days.count1[0]==1 && days.values[10]==254);
    for(unsigned i=0;i<200;++i) hmbtn_event_days_advance(&days,&c,0,false);
    CHECK(days.count0[24]==120 && days.count0[1]==120 && days.count1[0]==120 && days.count1[1]==255);
    /* Bank 0 advances even when bank 1 is held by date, weather or wedding. */
    hmbtn_event_days_init(&days);days.count1[0]=10;
    c.season=0;c.day=0;CHECK(hmbtn_farm_clock_date_index(c.season,c.day)==1);
    hmbtn_event_days_advance(&days,&c,0,false);CHECK(days.count0[1]==2 && days.count1[0]==10);
    c.day=7;hmbtn_event_days_advance(&days,&c,0,false);CHECK(days.count0[1]==3 && days.count1[0]==10);
    c.day=2;
    hmbtn_event_days_advance(&days,&c,3,false);CHECK(days.count0[1]==4 && days.count1[0]==10);
    hmbtn_event_days_advance(&days,&c,4,false);CHECK(days.count0[1]==5 && days.count1[0]==10);
    hmbtn_event_days_advance(&days,&c,0,true);CHECK(days.count0[1]==6 && days.count1[0]==10);
    hmbtn_event_days_advance(&days,&c,1,false);CHECK(days.count0[1]==7 && days.count1[0]==11);
    /* Only the first 14 dates are used, not all scene-remapping dates. */
    CHECK(hmbtn_farm_clock_date_index(3,23)==14 && !hmbtn_farm_clock_date_index(3,29)
        && !hmbtn_farm_clock_date_index(0,13) && !hmbtn_farm_clock_date_index(4,0));
    /* Real ordered fast-forward, including the spring day-1 bank-1 pause. */
    hmbtn_event_days_init(&days);days.count1[0]=0;
    c=(HmbtnFarmClock){0,3,29,6,23,59,0};
    hmbtn_farm_clock_advance_notify(&c,361,clock_event,&days);
    CHECK(c.year==1 && c.season==0 && c.day==0 && c.hour==6 && days.count0[1]==2 && days.count1[0]==0);
    hmbtn_farm_clock_advance_notify(&c,2880,clock_event,&days);
    CHECK(c.day==2 && days.count0[1]==4 && days.count1[0]==2);
    HmbtnEventDays paused=days;
    for(unsigned i=0;i<200;++i) CHECK(!hmbtn_farm_clock_tick(&c,true));
    CHECK(!memcmp(&paused,&days,sizeof(days)));
    puts("Original counter defaults, 120/255 semantics, date/weather/wedding holds and ordered overnight advancement passed.");return 0;
}
