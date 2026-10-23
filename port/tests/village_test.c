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
    hmbtn_village_init(&v,data,8);CHECK(!hmbtn_village_sync(&v,&c,error,sizeof(error)));
    puts("Original door hours, presence, affinity, festival exception, route waits, resume and schedule advance passed.");return 0;
}
