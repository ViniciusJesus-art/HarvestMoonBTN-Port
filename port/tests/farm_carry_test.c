#include "farm_carry.h"
#include "farm_menu.h"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Line %d: %s\n",__LINE__,#x);return 1; } } while(0)
int main(void)
{
    for(unsigned facing=0;facing<4;++facing) {
        int x,y;hmbtn_farm_carried_offset(facing,false,&x,&y);
        CHECK(y==(facing==0 || facing==3 ? -22 : -24));
        hmbtn_farm_carried_offset(facing,true,&x,&y);CHECK(x==-1 && y==-31);
    }
    HmbtnFarmField f;hmbtn_farm_field_init(&f,NULL,0);
    HmbtnHouseInteraction h;hmbtn_house_interaction_init(&h);
    HmbtnIntroActor p={0};p.position[0]=-4200;p.position[2]=1400;p.facing=3;
    HmbtnFarmCarry s={0};
    const unsigned ids[]={0x3a,0x3e,0x40};
    for(unsigned n=0;n<3;++n) {
        f.cells[1].value=(uint16_t)ids[n];
        CHECK(hmbtn_farm_carry_tick(&s,&h,&f,&p,HMBTN_PAD_CROSS));
        CHECK(h.held_item.id==ids[n] && f.cells[1].value==0x167 && s.changed);
        CHECK(p.sprite.animation==14);
        CHECK(hmbtn_farm_carry_tick(&s,&h,&f,&p,HMBTN_PAD_CROSS));CHECK(s.phase==1);
        p.animation_done=true;CHECK(hmbtn_farm_carry_tick(&s,&h,&f,&p,0));CHECK(!s.phase);
        CHECK(hmbtn_inventory_cycle(&h,false,false));CHECK(!h.held_item.id && h.items[0].id==ids[n]);
        CHECK(hmbtn_inventory_cycle(&h,false,false));CHECK(h.held_item.id==ids[n] && !h.items[0].id);
        CHECK(hmbtn_farm_carry_tick(&s,&h,&f,&p,HMBTN_PAD_CROSS));CHECK(s.phase>=2);
        p.animation_done=true;CHECK(hmbtn_farm_carry_tick(&s,&h,&f,&p,0));
        CHECK(!h.held_item.id && s.thrown[0].item.id==ids[n]);
        for(unsigned i=0;i<60;++i) hmbtn_farm_carry_flight(&s,&f,NULL,NULL,NULL);
        CHECK(!s.thrown[0].item.id);
        if(n<2) {
            bool restored=false;for(unsigned i=0;i<1125;++i) if(f.cells[i].value==ids[n]) { restored=true;f.cells[i].value=f.cells[i].ground; }
            CHECK(restored && s.discarded==0);
        } else CHECK(s.discarded==1);
    }
    f.cells[1].value=0x3b;CHECK(!hmbtn_farm_carry_tick(&s,&h,&f,&p,HMBTN_PAD_CROSS));
    CHECK(f.cells[1].value==0x3b && !h.held_item.id);
    h.held_item=(HmbtnStoredTool){21,0,73};
    CHECK(!hmbtn_farm_carry_tick(&s,&h,&f,&p,HMBTN_PAD_CROSS));CHECK(h.held_item.count==73);
    f.cells[0].value=0x3b;
    s.thrown[0]=(HmbtnThrownItem){{0x3a,0,1},{-4200,0,1400},0,0,0};
    hmbtn_farm_carry_flight(&s,&f,NULL,NULL,NULL);
    CHECK(f.cells[0].value==0x3b && s.discarded==2); /* Occupied soil is conserved. */
    s.area=112;h.held_item=(HmbtnStoredTool){0x122,0,1};
    CHECK(hmbtn_farm_carry_tick(&s,&h,NULL,&p,HMBTN_PAD_CROSS));
    p.animation_done=true;CHECK(hmbtn_farm_carry_tick(&s,&h,NULL,&p,0));
    CHECK(!h.held_item.id && s.thrown[0].area==112 && s.thrown[0].item.id==0x122);
    const int32_t x=s.thrown[0].position[0],y=s.thrown[0].position[1];
    s.area=0;hmbtn_farm_carry_flight(&s,&f,NULL,NULL,NULL);
    CHECK(s.thrown[0].position[0]==x && s.thrown[0].position[1]==y);
    s.area=112;for(unsigned i=0;i<60;++i) hmbtn_farm_carry_flight(&s,NULL,NULL,NULL,NULL);
    CHECK(!s.thrown[0].item.id && s.discarded==3);
    puts("Loose field pickups, bag roundtrip, timed throw, flight retirement and protected objects passed.");return 0;
}
