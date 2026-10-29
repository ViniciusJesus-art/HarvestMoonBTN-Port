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
    HmbtnHouseInteraction h;HmbtnGameState h_state;hmbtn_game_state_init(&h_state);hmbtn_house_interaction_init(&h,&h_state);
    HmbtnIntroActor p={0};p.position[0]=-4200;p.position[2]=1400;p.facing=3;
    HmbtnFarmCarry s={0};
    const unsigned ids[]={0x3a,0x3e,0x40};
    for(unsigned n=0;n<3;++n) {
        f.cells[1].value=(uint16_t)ids[n];
        CHECK(hmbtn_farm_carry_tick(&s,&h,&f,&p,HMBTN_PAD_CROSS));
        CHECK(h.inventory->held_item.id==ids[n] && f.cells[1].value==0x167 && s.changed);
        CHECK(p.sprite.animation==14);
        CHECK(hmbtn_farm_carry_tick(&s,&h,&f,&p,HMBTN_PAD_CROSS));CHECK(s.phase==1);
        p.animation_done=true;CHECK(hmbtn_farm_carry_tick(&s,&h,&f,&p,0));CHECK(!s.phase);
        CHECK(hmbtn_inventory_cycle(h.inventory,false,false));CHECK(!h.inventory->held_item.id && h.inventory->items[0].id==ids[n]);
        CHECK(hmbtn_inventory_cycle(h.inventory,false,false));CHECK(h.inventory->held_item.id==ids[n] && !h.inventory->items[0].id);
        CHECK(hmbtn_farm_carry_tick(&s,&h,&f,&p,HMBTN_PAD_CROSS));CHECK(s.phase>=2);
        p.animation_done=true;CHECK(hmbtn_farm_carry_tick(&s,&h,&f,&p,0));
        CHECK(!h.inventory->held_item.id && s.thrown[0].item.id==ids[n]);
        for(unsigned i=0;i<60;++i) hmbtn_farm_carry_flight(&s,&f,NULL,NULL);
        CHECK(!s.thrown[0].item.id);
        unsigned restored=0;
        for(unsigned i=0;i<1125;++i) if(f.cells[i].value==ids[n]) { ++restored;f.cells[i].value=f.cells[i].ground; }
        CHECK(restored==(n<2 ? 1u : 0u) && s.discarded==(n<2 ? 0u : 1u));
    }
    f.cells[1].value=0x3b;CHECK(!hmbtn_farm_carry_tick(&s,&h,&f,&p,HMBTN_PAD_CROSS));
    CHECK(f.cells[1].value==0x3b && !h.inventory->held_item.id);
    h.inventory->held_item=(HmbtnStoredTool){21,0,73};
    CHECK(!hmbtn_farm_carry_tick(&s,&h,&f,&p,HMBTN_PAD_CROSS));CHECK(h.inventory->held_item.count==73);
    f.cells[0].value=0x3b;
    s.thrown[0]=(HmbtnThrownItem){.item={0x3a,0,1},.position={-4200,0,1400}};
    hmbtn_farm_carry_flight(&s,&f,NULL,NULL);
    CHECK(f.cells[0].value==0x3b && s.discarded==2 && s.thrown[0].phase==1);
    const HmbtnThrownItem landed=s.thrown[0];
    hmbtn_farm_carry_flight(&s,&f,NULL,NULL);
    CHECK(s.thrown[0].item.id==0x3a && s.thrown[0].phase==2 && s.thrown[0].timer==4);
    for(unsigned i=0;i<3;++i) {
        hmbtn_farm_carry_flight(&s,&f,NULL,NULL);
        CHECK(s.thrown[0].item.id==0x3a && s.thrown[0].position[0]==landed.position[0]
            && s.thrown[0].position[1]==landed.position[1]);
    }
    hmbtn_farm_carry_flight(&s,&f,NULL,NULL);CHECK(!s.thrown[0].item.id && s.discarded==2);
    s.area=112;h.inventory->held_item=(HmbtnStoredTool){0x122,0,1};
    CHECK(hmbtn_farm_carry_tick(&s,&h,NULL,&p,HMBTN_PAD_CROSS));
    p.animation_done=true;CHECK(hmbtn_farm_carry_tick(&s,&h,NULL,&p,0));
    CHECK(!h.inventory->held_item.id && s.thrown[0].area==112 && s.thrown[0].item.id==0x122);
    const int32_t x=s.thrown[0].position[0],y=s.thrown[0].position[1];
    s.area=0;hmbtn_farm_carry_flight(&s,&f,NULL,NULL);
    CHECK(s.thrown[0].position[0]==x && s.thrown[0].position[1]==y);
    s.area=112;for(unsigned i=0;i<60;++i) hmbtn_farm_carry_flight(&s,NULL,NULL,NULL);
    CHECK(!s.thrown[0].item.id && s.discarded==3);
    /* Restore catalog objects only on free active soil, in either layout.
     * Winter pictures normalize to the field's stable stone/branch ids. */
    for(unsigned indoor=0;indoor<2;++indoor) for(unsigned kind=0;kind<4;++kind) {
        if(indoor) hmbtn_farm_field_init_greenhouse(&f);else hmbtn_farm_field_init(&f,NULL,0);
        const unsigned ids[4]={0x3a,0x3e,0x13b,0x13c};
        const unsigned item=kind&1 ? 0x3e : 0x3a;
        HmbtnFarmCarry flight={0};
        CHECK(hmbtn_farm_field_position(&f,2,2,p.position));
        flight.thrown[0]=(HmbtnThrownItem){.item={(uint16_t)ids[kind],0,1},.position={p.position[0],1,p.position[2]}};
        const unsigned index=2*hmbtn_farm_field_columns(&f)+2;
        f.cells[index].value=f.cells[index].ground=0x169;
        hmbtn_farm_carry_flight(&flight,&f,NULL,NULL);
        CHECK(flight.changed && !flight.discarded && !flight.thrown[0].item.id
            && f.cells[index].value==item && f.cells[index].ground==0x169);
        f.cells[index].value=0x23;f.cells[index].kind=2;
        flight.thrown[0]=(HmbtnThrownItem){.item={0x3a,0,1},.position={p.position[0],1,p.position[2]}};
        hmbtn_farm_carry_flight(&flight,&f,NULL,NULL);
        CHECK(!flight.changed && flight.discarded==1 && f.cells[index].value==0x23 && f.cells[index].kind==2);
        f.cells[index].value=0x167;f.active[index]=false;
        flight.thrown[0]=(HmbtnThrownItem){.item={0x3e,0,1},.position={p.position[0],1,p.position[2]}};
        hmbtn_farm_carry_flight(&flight,&f,NULL,NULL);
        CHECK(!flight.changed && flight.discarded==2 && f.cells[index].value==0x167);
    }
    /* All four boundaries, both layouts: feet outside, target just inside.
     * Reject an out-of-range probe, never wrap to another row or column. */
    for(unsigned indoor=0;indoor<2;++indoor) for(unsigned face=0;face<4;++face) {
        if(indoor) hmbtn_farm_field_init_greenhouse(&f);else hmbtn_farm_field_init(&f,NULL,0);
        unsigned col=face==1 ? hmbtn_farm_field_columns(&f)-1 : 0,
            row=face==2 ? hmbtn_farm_field_rows(&f)-1 : 0;
        CHECK(hmbtn_farm_field_position(&f,col,row,p.position));p.facing=face;
        const int dx[4]={0,1,0,-1},dz[4]={1,0,-1,0};
        int c,r;
        for(unsigned n=0;n<500 && hmbtn_farm_field_world_cell(&f,p.position,&c,&r);++n) {
            p.position[0]+=dx[face];p.position[2]+=dz[face];
        }
        p.position[0]+=dx[face]*32;p.position[2]+=dz[face]*32;
        CHECK(!hmbtn_farm_field_world_cell(&f,p.position,&c,&r));
        const unsigned index=row*hmbtn_farm_field_columns(&f)+col;
        f.cells[index].value=0x3e;h.inventory->held_item=(HmbtnStoredTool){0};s.phase=0;
        CHECK(hmbtn_farm_carry_tick(&s,&h,&f,&p,HMBTN_PAD_CROSS));
        CHECK(h.inventory->held_item.id==0x3e && f.cells[index].value==0x167);
        p.animation_done=true;CHECK(hmbtn_farm_carry_tick(&s,&h,&f,&p,0));
        h.inventory->held_item=(HmbtnStoredTool){0};f.cells[index].value=0x3e;
        p.position[0]+=dx[face]*400;p.position[2]+=dz[face]*400;
        CHECK(!hmbtn_farm_carry_tick(&s,&h,&f,&p,HMBTN_PAD_CROSS));
        CHECK(!h.inventory->held_item.id && f.cells[index].value==0x3e);
    }
    puts("Loose field pickups, bag roundtrip, timed throw, flight retirement and protected objects passed.");return 0;
}
