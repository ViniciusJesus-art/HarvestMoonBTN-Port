#include "save_fixture.h"
#include "item_reaction.h"
#include "psx_data.h"
#include <stdio.h>
#include <stdlib.h>
static void zone(uint8_t *s,unsigned slot,unsigned kind)
{
    uint8_t *z=s+16+slot*44;hmbtn_write_le16(z,0x4000);hmbtn_write_le16(z+2,(uint16_t)kind);
    for(unsigned a=0;a<3;++a) { hmbtn_write_le16(z+14+a*2,(uint16_t)-100);hmbtn_write_le16(z+20+a*2,100); }
}
int main(void)
{
    char error[256]={0};uint8_t b[88]={0},section[0x2000]={0};
    hmbtn_write_le32(b,0x42);hmbtn_write_le32(b+8,1);hmbtn_write_le32(b+12,28);hmbtn_write_le32(b+16,4);
    hmbtn_write_le32(b+28,60);hmbtn_write_le32(b+32,1);
    for(unsigned i=0;i<4;++i) {
        hmbtn_write_le16(b+40+i*8,(uint16_t)(i&1 ? 2000 : -2000));
        hmbtn_write_le16(b+44+i*8,(uint16_t)(i<2 ? -2000 : 2000));hmbtn_write_le16(b+80+i*2,(uint16_t)i);
    }
    hmbtn_write_le16(b+74,0x2901);HmbtnModel ground;assert(hmbtn_model_open(&ground,b,sizeof(b),error,sizeof(error)));
    zone(section,1,44);
    for(unsigned test=0;test<7;++test) {
        hmbtn_write_le16(b+72,test==0 ? 0 : 0x40);HmbtnFarmCarry c={.area=72};
        c.thrown[0]=(HmbtnThrownItem){.item={test==2 ? 164 : 65,65,1},.position={0,1,44},.area=72};
        if(test==3) zone(section,0,38);
        if(test==4) c.thrown[0].position[0]=100;
        if(test==5) c.thrown[0].area=0;
        hmbtn_farm_carry_flight(&c,NULL,&ground,NULL,section,sizeof(section));
        assert(c.offering==((test==1 || test==2 || test==6) ? 65u : 0u));
        c.offering=0;hmbtn_farm_carry_flight(&c,NULL,&ground,NULL,section,sizeof(section));assert(!c.offering);
        memset(section,0,sizeof(section));zone(section,1,44);
    }
    /* Real trigger shape, with the BIN's sunny-only gift mode and recipient. */
    uint8_t table[38];memset(table,255,sizeof(table));hmbtn_write_le16(table,(63u<<10)|182);table[12]=0x47;
    hmbtn_write_le32(table+8,0x3fffffff);hmbtn_write_le32(table+28,0x7ff88000);table[34]=4;table[35]=33;
    HmbtnFarmTriggers triggers;assert(hmbtn_farm_triggers_open(&triggers,table,sizeof(table),error,sizeof(error)));
    HmbtnSession *s=save_fixture(1);HmbtnVillage *v=&s->village;
    assert(hmbtn_item_villager_reaction(65,33)==0 && hmbtn_item_villager_reaction(64,33)!=0);
    assert(hmbtn_farm_event_trigger(&triggers,v,&s->game.clock,0,0,4,65,&s->game.inventory)==182);
    assert(!hmbtn_farm_event_trigger(&triggers,v,&s->game.clock,1,0,4,65,&s->game.inventory));
    assert(!hmbtn_farm_event_trigger(&triggers,v,&s->game.clock,0,0,4,64,&s->game.inventory));
    assert(!hmbtn_farm_event_trigger(&triggers,v,&s->game.clock,0,0,0,0,&s->game.inventory));
    s->visit.event=182;
    HmbtnEventCommand c={.type=101,.operand=9};
    for(unsigned i=0;i<HMBTN_VILLAGERS;++i) v->people[i].affection=0;
    v->people[14].affection=1234;
    assert(hmbtn_goddess_command(s,&c,error,sizeof(error))==HMBTN_EVENT_DONE && s->intro.slots[9]==14);
    const unsigned season=s->game.clock.season;
    v->people[7].affection=5000;v->flags[0x25]=true;s->game.clock.season=1;v->days.count0[2]=0;
    assert(hmbtn_goddess_command(s,&c,error,sizeof(error))==HMBTN_EVENT_DONE && s->intro.slots[9]==7);
    v->flags[0xc9]=true;
    assert(hmbtn_goddess_command(s,&c,error,sizeof(error))==HMBTN_EVENT_DONE && s->intro.slots[9]==14);
    c.command=1;
    assert(hmbtn_goddess_command(s,&c,error,sizeof(error))==HMBTN_EVENT_DONE && s->intro.slots[9]==7);
    c=(HmbtnEventCommand){.type=81,.command=6,.operand=256};
    assert(hmbtn_goddess_command(s,&c,error,sizeof(error))==HMBTN_EVENT_DONE && s->game.progress.goddess_berry);
    s->visit=(HmbtnFarmVisit){0};s->village.days.values[1]=21;s->game.clock.season=season;
    uint8_t *data=NULL;size_t bytes=0;assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));
    assert(loaded && loaded->game.progress.goddess_berry && loaded->village.days.values[1]==21);
    free(data);hmbtn_session_destroy(loaded);hmbtn_session_destroy(s);
    puts("Goddess: original target, water/first-zone/bounds, wrapped content, single landing, weather/reaction/mode, bride choice and persistent progress passed");return 0;
}
