#include "village.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Village line %d: %s; %s\n",__LINE__,#x,error);return 1; } } while(0)
static void route(uint8_t *s,unsigned col,unsigned kind,unsigned wait,unsigned pose,int x)
{
    uint8_t *p=s+0x1000+col*12;
    hmbtn_write_le16(p,(uint16_t)kind);hmbtn_write_le16(p+2,(uint16_t)wait);
    hmbtn_write_le16(p+4,(uint16_t)pose);hmbtn_write_le16(p+6,(uint16_t)x);
}
int main(void)
{
    char error[256]={0};uint8_t data[0xA800]={0};HmbtnVillage v;
    for(unsigned i=0;i<sizeof(data);i+=8) {
        hmbtn_write_le16(data+i,1500);hmbtn_write_le16(data+i+4,54);hmbtn_write_le16(data+i+6,54);
    }
    HmbtnFarmClock c={0,0,1,1,10,0,0};hmbtn_village_init(&v,data,sizeof(data));
    CHECK(hmbtn_village_sync(&v,&c,error,sizeof(error)));
    CHECK(v.people[3].friendship==0 && v.people[15].friendship==50);
    v.people[15].location=9;
    CHECK(hmbtn_village_door(&v,8,&c)==-1);
    c.hour=18;CHECK(hmbtn_village_door(&v,8,&c)==0x1CD);
    v.people[15].friendship=101;CHECK(hmbtn_village_door(&v,8,&c)==-1);
    c.hour=20;CHECK(hmbtn_village_door(&v,8,&c)==0x1CD);
    v.people[15].friendship=201;CHECK(hmbtn_village_door(&v,8,&c)==-1);
    v.people[15].location=54;CHECK(hmbtn_village_door(&v,8,&c)==0x1CD);
    c.hour=10;CHECK(hmbtn_village_door(&v,12,&c)==0x1D2);
    v.people[4].friendship=101;CHECK(hmbtn_village_door(&v,12,&c)==-1);
    v.people[22].location=36;c.hour=8;CHECK(hmbtn_village_door(&v,13,&c)==0x1CD);
    c.hour=9;CHECK(hmbtn_village_door(&v,13,&c)==-1);
    c.hour=12;CHECK(hmbtn_village_door(&v,13,&c)==-1);
    c.hour=18;CHECK(hmbtn_village_door(&v,13,&c)==0x1CD);
    v.people[22].friendship=201;CHECK(hmbtn_village_door(&v,13,&c)==-1);
    c.season=0;c.day=7;CHECK(hmbtn_village_sync(&v,&c,error,sizeof(error)));
    CHECK(hmbtn_village_door(&v,8,&c)==0x1CD && hmbtn_village_door(&v,0,&c)==-1);
    c.day=0;CHECK(hmbtn_village_sync(&v,&c,error,sizeof(error)));
    CHECK(hmbtn_village_door(&v,34,&c)==-1); /* Spring day 1 seed exception. */
    c.day=1;c.hour=6;CHECK(hmbtn_village_sync(&v,&c,error,sizeof(error)));
    uint8_t section[0x2000]={0};hmbtn_write_le16(section+2,9);
    route(section,0,2,0,3,0);route(section,1,1,2,2,144);route(section,2,0,0,0,144);
    HmbtnVillager *p=&v.people[15];p->location=9;p->row=0;p->placed=false;p->schedule[0][0]=360;
    p->step=0;v.minute=360;
    hmbtn_village_tick(&v,section,sizeof(section),NULL,false);
    CHECK(p->actor.visible && p->actor.position[0]==0 && p->wait==1);
    hmbtn_village_tick(&v,section,sizeof(section),NULL,true);CHECK(p->wait==1);
    for(unsigned i=0;i<5;++i) hmbtn_village_tick(&v,section,sizeof(section),NULL,false);
    CHECK(p->standing && p->actor.position[0]==144 && p->actor.facing==2);
    hmbtn_village_enter(&v);v.minute=361;hmbtn_village_tick(&v,section,sizeof(section),NULL,true);
    CHECK(p->standing && p->actor.position[0]==144); /* Resume elapsed walk, no replay at doorway. */
    route(section,2,4,0,0,144);p->schedule[1][3]=10;p->schedule[1][0]=1500;
    hmbtn_village_enter(&v);hmbtn_village_tick(&v,section,sizeof(section),NULL,false);
    CHECK(!p->actor.visible && p->location==10 && p->step==1);
    c.hour=6;c.minute=1;CHECK(hmbtn_village_sync(&v,&c,error,sizeof(error)));
    CHECK(p->step==1 && p->location==10); /* Route advance cannot be undone by clock sync. */
    /* NPC hit probes are strict in X/Z/Y and ignore absent actors. */
    HmbtnVillage solid;hmbtn_village_init(&solid,NULL,0);
    solid.people[18].actor.visible=true;
    const int32_t outside[3]={300,0,0},edge[3]={200,0,0},inside[3]={199,0,0};
    CHECK(!hmbtn_village_blocked(&solid,outside,edge));
    CHECK(hmbtn_village_blocked(&solid,outside,inside));
    const int32_t upper[3]={100,20,0},overlap[3]={100,0,0},away[3]={150,0,0},closer[3]={50,0,0};
    CHECK(!hmbtn_village_blocked(&solid,outside,upper));
    CHECK(!hmbtn_village_blocked(&solid,overlap,away));
    CHECK(hmbtn_village_blocked(&solid,overlap,closer));
    HmbtnIntroActor player={0};player.position[2]=300;player.facing=0;
    CHECK(hmbtn_village_target(&solid,&player)==18);
    player.facing=2;CHECK(!hmbtn_village_target(&solid,&player));
    solid.people[18].actor.visible=false;CHECK(!hmbtn_village_blocked(&solid,outside,inside));
    /* A walking villager stops before entering the player's 218-unit box,
     * then resumes when the player leaves. Arrival fast-forward is independent. */
    hmbtn_write_le16(section+2,9);
    route(section,0,2,0,3,0);route(section,1,1,0,3,400);route(section,2,0,0,0,400);
    HmbtnVillage walk;hmbtn_village_init(&walk,NULL,0);
    walk.people[15].location=9;walk.people[15].row=0;walk.people[15].schedule[0][0]=360;walk.minute=360;
    player=(HmbtnIntroActor){0};player.position[0]=300;walk.player=&player;
    hmbtn_village_tick(&walk,section,sizeof(section),NULL,false);
    CHECK(walk.people[15].actor.position[0]==48);
    for(unsigned i=0;i<20;++i) hmbtn_village_tick(&walk,section,sizeof(section),NULL,false);
    CHECK(walk.people[15].actor.position[0]==48);
    walk.player_running=true;hmbtn_village_tick(&walk,section,sizeof(section),NULL,false);
    CHECK(walk.people[15].actor.position[0]==96); /* Original POSE_4 exception. */
    walk.player_running=false;
    hmbtn_village_tick(&walk,section,sizeof(section),NULL,false);
    CHECK(walk.people[15].actor.position[0]==96); /* Walking restores blocking immediately. */
    player.position[0]=1000;hmbtn_village_tick(&walk,section,sizeof(section),NULL,false);
    CHECK(walk.people[15].actor.position[0]==144);
    /* A blocked route is released after 30 clock minutes, not 30 frames.
     * The villager remains selectable after its solid-box bit is released. */
    HmbtnVillage delayed;hmbtn_village_init(&delayed,NULL,0);
    delayed.people[15].location=9;delayed.people[15].row=0;
    delayed.people[15].schedule[0][0]=600;delayed.minute=600;
    player=(HmbtnIntroActor){0};player.position[0]=300;player.facing=1;delayed.player=&player;
    hmbtn_village_tick(&delayed,section,sizeof(section),NULL,false);
    hmbtn_village_tick(&delayed,section,sizeof(section),NULL,false);
    HmbtnVillager *blocked=&delayed.people[15];
    CHECK(blocked->blocked_by_player && blocked->actor.position[0]==48);
    c=(HmbtnFarmClock){0,0,2,2,10,0,0};
    for(unsigned i=0;i<200;++i) {
        hmbtn_village_tick(&delayed,section,sizeof(section),NULL,false);
        hmbtn_village_clock(&delayed,&c,0,0,false);
    }
    CHECK(blocked->delay==0);
    for(unsigned i=0;i<29;++i) {
        unsigned changes=hmbtn_farm_clock_advance(&c,1);
        hmbtn_village_clock(&delayed,&c,changes,0,false);
        hmbtn_village_tick(&delayed,section,sizeof(section),NULL,false);
    }
    const int32_t enter[3]={100,0,0};
    CHECK(blocked->delay==29 && blocked->actor.position[0]==48
        && hmbtn_village_blocked(&delayed,player.position,enter));
    unsigned changes=hmbtn_farm_clock_advance(&c,1);
    hmbtn_village_clock(&delayed,&c,changes,0,false);
    hmbtn_village_tick(&delayed,section,sizeof(section),NULL,false);
    CHECK(blocked->delay==30 && blocked->actor.position[0]==96
        && !hmbtn_village_blocked(&delayed,player.position,enter)
        && hmbtn_village_target(&delayed,&player)==15);
    /* Reloading a route subtracts the accrued delay from elapsed walking. */
    hmbtn_village_enter(&delayed);delayed.minute=630;
    hmbtn_village_tick(&delayed,section,sizeof(section),NULL,true);
    CHECK(blocked->actor.position[0]==0 && blocked->delay==30);
    hmbtn_village_enter(&delayed);delayed.minute=631;
    hmbtn_village_tick(&delayed,section,sizeof(section),NULL,true);
    CHECK(blocked->standing && blocked->actor.position[0]==400);
    /* Scheduled departures also include delay and cannot teleport early. */
    delayed.date_key=2;blocked->step=0;
    blocked->schedule[0][3]=9;blocked->schedule[1][0]=620;blocked->schedule[1][3]=10;
    blocked->schedule[2][0]=1500;
    c.minute=25;CHECK(hmbtn_village_sync(&delayed,&c,error,sizeof(error)));
    CHECK(!blocked->step && blocked->location==9);
    c.minute=50;CHECK(hmbtn_village_sync(&delayed,&c,error,sizeof(error)));
    CHECK(blocked->step==1 && blocked->location==10);
    blocked->step=0;blocked->location=9;blocked->standing=false;blocked->placed=true;
    blocked->column=1;blocked->cell_entered=false;blocked->actor.position[0]=96;
    c.hour=12;c.minute=0;hmbtn_village_clock(&delayed,&c,HMBTN_CLOCK_HOUR,0,false);
    CHECK(blocked->delay==0 && !blocked->blocked_by_player);
    hmbtn_village_tick(&delayed,section,sizeof(section),NULL,false);
    CHECK(blocked->actor.position[0]==96 && blocked->blocked_by_player);
    delayed.player_running=true;hmbtn_village_clock(&delayed,&c,HMBTN_CLOCK_MINUTE,0,false);
    CHECK(!blocked->delay);
    delayed.player_running=false;blocked->delay=12;c.hour=18;
    hmbtn_village_clock(&delayed,&c,HMBTN_CLOCK_HOUR,0,false);CHECK(!blocked->delay);
    blocked->delay=12;hmbtn_village_clock(&delayed,&c,HMBTN_CLOCK_DAY,0,false);CHECK(!blocked->delay);
    /* A route departure stores time minus delay and clears it only on
     * entering a clock-held place, as in ROUTE_CELL_SCHEDULE. */
    route(section,2,4,0,0,400);blocked->column=2;blocked->standing=false;
    blocked->placed=true;blocked->step=0;blocked->delay=5;blocked->location=9;
    blocked->schedule[1][3]=10;delayed.minute=700;
    hmbtn_village_tick(&delayed,section,sizeof(section),NULL,false);
    CHECK(blocked->step==1 && blocked->schedule[1][0]==695 && blocked->delay==0 && !blocked->actor.visible);
    blocked->column=2;blocked->standing=false;blocked->placed=true;
    blocked->step=0;blocked->delay=5;blocked->location=9;blocked->schedule[1][3]=11;
    hmbtn_village_tick(&delayed,section,sizeof(section),NULL,false);
    CHECK(blocked->step==1 && blocked->schedule[1][0]==695 && blocked->delay==5);
    hmbtn_village_init(&v,data,8);CHECK(!hmbtn_village_sync(&v,&c,error,sizeof(error)));
    puts("Original door hours, presence, affinity, festival exception, route waits, resume and schedule advance passed.");return 0;
}
