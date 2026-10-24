#include "village.h"
#include "psx_random.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Affinity line %d: %s\n",__LINE__,#x);return 1; } } while(0)
static void protected_village(HmbtnVillage *v)
{
    hmbtn_village_init(v,NULL,0);
    for(unsigned i=0;i<HMBTN_VILLAGERS;++i) v->people[i].talk_flags=HMBTN_VILLAGER_TALKED_TODAY|0x10;
}
static void next_day(void *context,const HmbtnFarmClock *clock,unsigned changes)
{
    hmbtn_village_clock(context,clock,changes,0,false);
}
int main(void)
{
    /* Golden original LCG states and scaling; no platform rand()/long. */
    static const uint32_t states[]={0x3039,0xD3DC167E,0xA70427DF,0xD6651C2C,0x0DAA96F5,0xC21F1C8A,0x3EAD62FB,0xCD1DCF18};
    static const unsigned rolls[]={0,83,65,84,5,76,24,80};
    uint32_t seed=0;
    for(unsigned i=0;i<8;++i) { CHECK(hmbtn_psx_random_range(&seed,101)==rolls[i]);CHECK(seed==states[i]); }
    seed=0;CHECK(hmbtn_psx_random_range(&seed,0)==0 && seed==0x3039);
    seed=0;CHECK(hmbtn_psx_random_range(&seed,65536)==0 && seed==0x3039);
    seed=UINT32_MAX;CHECK(hmbtn_psx_random_range(&seed,UINT32_MAX)==48696 && seed==0xBE39E1CC);
    HmbtnVillage v;protected_village(&v);
    HmbtnFarmClock c={0,0,2,2,6,0,0};
    for(unsigned i=0;i<HMBTN_VILLAGERS;++i) v.people[i].talk_flags|=HMBTN_VILLAGER_DAILY_GAIN;
    v.people[3].affection=65500;v.people[7].affection=1499;
    v.people[9].affection=1300;v.people[14].affection=1301;
    v.people[18].friendship=254;v.people[15].friendship=255;
    next_day(&v,&c,HMBTN_CLOCK_DAY);
    CHECK(v.people[3].affection==65535 && v.people[7].affection==1699
        && v.people[9].affection==1500 && v.people[14].affection==1501 && v.people[20].affection==200);
    CHECK(v.people[18].friendship==255 && v.people[15].friendship==255 && v.people[35].friendship==51);
    CHECK(v.people[3].friendship==0 && v.people[18].affection==0);
    /* The original threshold is strictly greater than 1500, and is sticky. */
    CHECK(v.flags[0xE0] && v.flags[0xE1] && !v.flags[0xE2] && v.flags[0xE3] && !v.flags[0xE4]);
    CHECK(!v.random); /* No random draws for daily gain / talked-today flags. */
    for(unsigned i=0;i<HMBTN_VILLAGERS;++i) CHECK(v.people[i].talk_flags==0x10);
    const unsigned girls[]={3,7,9,14,20};
    protected_village(&v);
    for(unsigned i=0;i<5;++i) v.people[girls[i]].affection=1500;
    next_day(&v,&c,HMBTN_CLOCK_DAY);
    for(unsigned i=0;i<5;++i) CHECK(!v.flags[0xE0+i]);
    protected_village(&v);
    for(unsigned i=0;i<5;++i) v.people[girls[i]].affection=1501;
    next_day(&v,&c,HMBTN_CLOCK_DAY);
    for(unsigned i=0;i<5;++i) CHECK(v.flags[0xE0+i]);
    /* Talking without the gain flag protects affinity, but grants no points. */
    protected_village(&v);v.people[3].affection=2000;
    next_day(&v,&c,HMBTN_CLOCK_DAY);
    CHECK(v.people[3].affection==2000 && v.people[18].friendship==50 && !v.random);
    /* Exact 1-in-10 neglect roll, lower clamps, and six no-decay exceptions. */
    const unsigned ids[]={0,3,18,36};
    for(unsigned i=0;i<4;++i) {
        protected_village(&v);HmbtnVillager *p=&v.people[ids[i]];
        p->talk_flags=0x10;p->affection=199;
        next_day(&v,&c,HMBTN_CLOCK_DAY);
        CHECK(v.random==0x3039 && p->talk_flags==0x10);
        CHECK(ids[i]==3 ? p->affection==0 && p->friendship==0 : p->friendship==49 && p->affection==199);
    }
    protected_village(&v);v.people[18].friendship=0;v.people[18].talk_flags=0;
    next_day(&v,&c,HMBTN_CLOCK_DAY);CHECK(!v.people[18].friendship);
    protected_village(&v);v.people[18].talk_flags=0;v.random=0x3039;
    next_day(&v,&c,HMBTN_CLOCK_DAY);CHECK(v.people[18].friendship==50 && v.random==0xD3DC167E);
    protected_village(&v);
    const unsigned exceptions[]={25,29,30,31,32,35};
    for(unsigned i=0;i<6;++i) v.people[exceptions[i]].talk_flags=0;
    next_day(&v,&c,HMBTN_CLOCK_DAY);CHECK(!v.random);
    for(unsigned i=0;i<6;++i) CHECK(v.people[exceptions[i]].friendship==50);
    /* Clearing a daily flag never clears a story threshold already raised. */
    protected_village(&v);v.people[3].talk_flags=0;v.people[3].affection=1600;v.flags[0xE0]=true;
    next_day(&v,&c,HMBTN_CLOCK_DAY);CHECK(v.people[3].affection==1400 && v.flags[0xE0]);
    /* Agenda reloads/date selection must not consume yesterday's flags.
     * Only ordered day notifications own this update. */
    uint8_t schedule[0xA800]={0};char error[256]={0};
    hmbtn_village_init(&v,schedule,sizeof(schedule));v.people[18].talk_flags=0x610;
    CHECK(hmbtn_village_sync(&v,&c,error,sizeof(error)));
    ++c.day;CHECK(hmbtn_village_sync(&v,&c,error,sizeof(error)));
    CHECK(v.people[18].talk_flags==0x610 && v.people[18].friendship==50 && !v.random);
    /* Fast-forward across New Year consumes a gain once, not once per frame
     * or clock callback. Further days may only apply neglect. */
    protected_village(&v);v.people[18].talk_flags=0x610;
    c=(HmbtnFarmClock){0,3,29,6,23,59,0};
    hmbtn_farm_clock_advance_notify(&c,361,next_day,&v);
    CHECK(c.year==1 && c.day==0 && v.people[18].friendship==51 && v.people[18].talk_flags==0x10);
    for(unsigned i=0;i<37;++i) v.people[i].talk_flags|=HMBTN_VILLAGER_TALKED_TODAY;
    hmbtn_farm_clock_advance_notify(&c,1440,next_day,&v);
    CHECK(v.people[18].friendship==51 && c.day==1);
    HmbtnVillage before=v;
    for(unsigned i=0;i<100;++i) {
        CHECK(!hmbtn_farm_clock_tick(&c,true));next_day(&v,&c,0);
    }
    CHECK(!memcmp(&before,&v,sizeof(v)));
    puts("Original RNG vectors, daily friendship/affection, thresholds, neglect exceptions and ordered overnight updates passed.");return 0;
}
