#include "area_doors.h"
#include "village.h"
#include "house_control.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Area doors line %d: %s\n",__LINE__,#x);return 1; } } while(0)
static void zone(uint8_t *section,unsigned index,unsigned flags,unsigned event,unsigned model)
{
    uint8_t *z=section+16+index*44;
    hmbtn_write_le16(z,(uint16_t)flags);hmbtn_write_le16(z+10,(uint16_t)event);hmbtn_write_le16(z+12,(uint16_t)model);
}
static void cell(uint8_t *section,unsigned column,unsigned kind,unsigned param,int x)
{
    uint8_t *c=section+0x1000+column*12;
    hmbtn_write_le16(c,(uint16_t)kind);hmbtn_write_le16(c+2,(uint16_t)param);
    hmbtn_write_le16(c+4,3);hmbtn_write_le16(c+6,(uint16_t)x);
}
int main(void)
{
    uint8_t section[0x2000]={0};HmbtnSceneData scene={0};HmbtnAreaDoors d={0};
    scene.angle[0][1]=128;scene.angle[1][1]=256;
    zone(section,0,0x9000,25,0);zone(section,1,0x1000,26,2);
    hmbtn_write_le16(section+0x948,0x400);hmbtn_write_le16(section+0x950,1);
    for(unsigned face=0;face<4;++face) {
        d=(HmbtnAreaDoors){0};hmbtn_area_doors_bind(&d,&scene);
        CHECK(hmbtn_area_doors_request(&d,section,sizeof(section),25,face)==HMBTN_DOOR_WAIT);
        CHECK(d.models[0].opening_sound);d.models[0].opening_sound=false;
        CHECK(hmbtn_area_doors_request(&d,section,sizeof(section),25,face)==HMBTN_DOOR_WAIT && !d.models[0].opening_sound);
        HmbtnAreaDoors before=d;hmbtn_area_doors_tick(&d,true);CHECK(!memcmp(&before,&d,sizeof(d)));
        unsigned ticks=0;
        while(d.models[0].phase==HMBTN_DOOR_OPENING && ticks++<30) hmbtn_area_doors_tick(&d,false);
        CHECK(ticks==16 && d.models[0].phase==HMBTN_DOOR_HELD && d.models[0].hold==11);
        const int turn=(face==1 || face==2) ? -0x420 : 0x420;
        CHECK(d.models[0].turn==turn);hmbtn_area_doors_apply(&d,&scene);
        CHECK(scene.angle[0][1]==(uint16_t)(128+turn) && scene.angle[1][1]==(uint16_t)(256-turn));
        for(unsigned t=0;t<40;++t) {
            CHECK(hmbtn_area_doors_request(&d,section,sizeof(section),25,(face+2)%4)==HMBTN_DOOR_READY);
            hmbtn_area_doors_tick(&d,false);
        }
        CHECK(d.models[0].phase==HMBTN_DOOR_HELD);
        for(unsigned t=0;t<35;++t) hmbtn_area_doors_tick(&d,false);
        CHECK(d.models[0].phase==HMBTN_DOOR_CLOSED && !d.models[0].turn && d.models[0].closing_sound);
        hmbtn_area_doors_apply(&d,&scene);CHECK(scene.angle[0][1]==128 && scene.angle[1][1]==256);
        CHECK(hmbtn_area_doors_request(&d,section,sizeof(section),26,face)==HMBTN_DOOR_READY);
        CHECK(hmbtn_area_doors_request(&d,section,sizeof(section),99,face)==HMBTN_DOOR_UNHANDLED);
        CHECK(hmbtn_area_doors_request(&d,section,10,25,face)==HMBTN_DOOR_UNHANDLED);
    }
    /* Internal -2 door: opposite sides use one model, never an area travel. */
    d=(HmbtnAreaDoors){0};hmbtn_area_doors_bind(&d,&scene);
    zone(section,3,0x1000,35,4);hmbtn_write_le16(section+16+3*44+2,0xfffe);
    uint8_t *inside=section+16+3*44;
    hmbtn_write_le16(inside+14,(uint16_t)-100);hmbtn_write_le16(inside+18,(uint16_t)-100);
    hmbtn_write_le16(inside+20,100);hmbtn_write_le16(inside+24,100);
    HmbtnIntroActor player={0};HmbtnHouseExit exit;unsigned model;
    CHECK(hmbtn_world_door_at(section,sizeof(section),&player,&exit,&model) && exit.target==0xfffe && model==4);
    CHECK(hmbtn_area_doors_request(&d,section,sizeof(section),35,0)==HMBTN_DOOR_WAIT);
    for(unsigned i=0;i<16;++i) hmbtn_area_doors_tick(&d,false);
    CHECK(d.models[4].phase==HMBTN_DOOR_HELD);
    /* Multiple residents and multiple physical doors advance once per tick. */
    d=(HmbtnAreaDoors){0};hmbtn_area_doors_bind(&d,&scene);zone(section,2,0x9000,27,3);
    CHECK(hmbtn_area_doors_request(&d,section,sizeof(section),25,1)==HMBTN_DOOR_WAIT);
    CHECK(hmbtn_area_doors_request(&d,section,sizeof(section),27,0)==HMBTN_DOOR_WAIT);
    hmbtn_area_doors_tick(&d,false);hmbtn_area_doors_apply(&d,&scene);
    CHECK(d.models[0].turn==-8 && d.models[3].turn==8);
    hmbtn_area_doors_claim(&d,&scene,1); /* Player uses the twin leaf. */
    CHECK(d.models[0].phase==HMBTN_DOOR_CLOSED && scene.angle[0][1]==128 && scene.angle[1][1]==256 && d.models[3].turn==8);
    hmbtn_write_le16(section+0x950,99);
    CHECK(hmbtn_area_doors_request(&d,section,sizeof(section),25,0)==HMBTN_DOOR_UNHANDLED);
    hmbtn_write_le16(section+0x950,1);
    /* A resident waits at a closed door, crosses when open, then changes area.
     * Route state is authoritative; an absent renderer does not skip the wait. */
    d=(HmbtnAreaDoors){0};scene.angle[3][1]=0;hmbtn_area_doors_bind(&d,&scene);
    HmbtnVillage v;hmbtn_village_init(&v,NULL,0);v.doors=&d;v.minute=360;
    cell(section,0,2,0,0);cell(section,1,0x11,25,192);cell(section,2,4,0,192);
    HmbtnVillager *p=&v.people[15];p->location=0;p->schedule[0][0]=360;p->schedule[1][3]=1;
    for(unsigned t=0;t<16;++t) {
        hmbtn_village_tick(&v,section,sizeof(section),NULL,false);
        CHECK(p->column==1 && !p->actor.position[0]);hmbtn_area_doors_tick(&d,false);
    }
    for(unsigned t=0;t<5;++t) { hmbtn_village_tick(&v,section,sizeof(section),NULL,false);hmbtn_area_doors_tick(&d,false); }
    CHECK(p->location==1 && p->step==1 && !p->actor.visible && d.models[0].phase==HMBTN_DOOR_HELD);
    /* Pauses and historical route catch-up never request a new opening. */
    d=(HmbtnAreaDoors){0};hmbtn_area_doors_bind(&d,&scene);
    p->placed=false;p->location=0;p->step=0;v.minute=361;
    hmbtn_village_tick(&v,section,sizeof(section),NULL,true);
    CHECK(p->location==1 && d.models[0].phase==HMBTN_DOOR_CLOSED && !d.models[0].opening_sound);
    puts("Area doors: original swing/hold/twin, pause, shared requests, player ownership, bounds and resident passage/catch-up passed.");return 0;
}
