#include "farm_clock.h"
#include "psx_data.h"
#include <stdio.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"Clock line %d: %s\n",__LINE__,#c);return 1; } } while(0)
int main(void)
{
    HmbtnFarmClock c;hmbtn_farm_clock_init(&c);
    CHECK(c.day==1 && c.weekday==1 && c.hour==6 && c.minute==0);
    for(unsigned i=0;i<14;++i) CHECK(hmbtn_farm_clock_tick(&c,false)==0);
    for(unsigned i=0;i<200;++i) CHECK(hmbtn_farm_clock_tick(&c,true)==0);
    CHECK(c.frames==14 && c.minute==0);
    CHECK(hmbtn_farm_clock_tick(&c,false)==HMBTN_CLOCK_MINUTE && c.minute==1 && c.frames==0);
    CHECK(hmbtn_farm_clock_advance(&c,59)==(HMBTN_CLOCK_MINUTE|HMBTN_CLOCK_HOUR) && c.hour==7);
    c.hour=23;c.minute=59;
    CHECK(!(hmbtn_farm_clock_advance(&c,1)&HMBTN_CLOCK_DAY) && c.hour==0 && c.day==1);
    CHECK(hmbtn_farm_clock_advance(&c,360)&HMBTN_CLOCK_DAY);
    CHECK(c.day==2 && c.hour==6 && c.weekday==2);
    c.day=29;c.hour=5;c.minute=59;c.weekday=6;
    CHECK(hmbtn_farm_clock_advance(&c,1)&HMBTN_CLOCK_SEASON);
    CHECK(c.day==0 && c.season==1 && c.weekday==0);
    c.year=98;c.season=3;c.day=29;c.hour=5;c.minute=59;
    CHECK(hmbtn_farm_clock_advance(&c,1)&HMBTN_CLOCK_YEAR);
    CHECK(c.year==98 && c.season==0 && c.day==0);
    c.hour=20;c.minute=37;
    CHECK(hmbtn_farm_clock_sleep_minutes(&c,0)==563);
    CHECK(hmbtn_farm_clock_sleep_minutes(&c,50)==683);
    CHECK(hmbtn_farm_clock_sleep_minutes(&c,70)==803);
    CHECK(hmbtn_farm_clock_sleep_minutes(&c,80)==923);
    CHECK(hmbtn_farm_clock_sleep_minutes(&c,100)==563);
    c.hour=2;c.minute=5;
    CHECK(hmbtn_farm_clock_sleep_minutes(&c,0)==235);
    hmbtn_farm_clock_advance(&c,235);CHECK(c.hour==6 && c.minute==0);
    uint8_t section[16]={0};
    CHECK(!hmbtn_farm_clock_place_held(section,sizeof(section)));
    hmbtn_write_le16(section+2,2);CHECK(hmbtn_farm_clock_place_held(section,sizeof(section)));
    CHECK(hmbtn_farm_clock_place_held(NULL,0));
    CHECK(hmbtn_farm_clock_period(5)==0 && hmbtn_farm_clock_period(6)==0);
    CHECK(hmbtn_farm_clock_period(7)==1 && hmbtn_farm_clock_period(14)==1);
    CHECK(hmbtn_farm_clock_period(15)==2 && hmbtn_farm_clock_period(16)==2);
    CHECK(hmbtn_farm_clock_period(17)==3 && hmbtn_farm_clock_period(4)==3);
    CHECK(hmbtn_farm_clock_fatigue(0)==3 && hmbtn_farm_clock_fatigue(2)==4 && hmbtn_farm_clock_fatigue(6)==10);
    HmbtnDaylight light;c.hour=14;hmbtn_daylight_select(&light,&c);
    CHECK(light.rgb[0]==0 && light.rgb[1]==0 && light.rgb[2]==0);
    c.hour=15;hmbtn_daylight_tick(&light,&c,true);
    CHECK(light.rgb[0]==0 && light.total==256 && light.taken==0);
    for(unsigned i=0;i<256;++i) hmbtn_daylight_tick(&light,&c,false);
    CHECK(light.rgb[0]==0x5000 && light.rgb[1]==0x1e000 && light.rgb[2]==0x28000);
    c.hour=17;hmbtn_daylight_select(&light,&c);
    uint8_t pixel[]={255,255,255,255};hmbtn_daylight_apply(&light,pixel,sizeof(pixel));
    CHECK(pixel[0]==173 && pixel[1]==189 && pixel[2]==214 && pixel[3]==255);
    puts("Original clock cadence, pauses, 06:00 rollover, seasons, sleep and subtractive light passed.");
    return 0;
}
