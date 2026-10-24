#include "poultry.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Poultry carry line %d: %s\n",__LINE__,#x);return 1; } } while(0)
static void floor_bytes(uint8_t b[88],unsigned flags)
{
    memset(b,0,88);hmbtn_write_le32(b,0x42);hmbtn_write_le32(b+8,1);
    hmbtn_write_le32(b+12,28);hmbtn_write_le32(b+16,4);
    hmbtn_write_le32(b+28,60);hmbtn_write_le32(b+32,1);
    for(unsigned i=0;i<4;++i) {
        hmbtn_write_le16(b+40+i*8,(uint16_t)(i&1 ? 2000 : -2000));
        hmbtn_write_le16(b+44+i*8,(uint16_t)(i<2 ? -2000 : 2000));
    }
    hmbtn_write_le16(b+72,(uint16_t)flags);hmbtn_write_le16(b+74,0x2901);
    for(unsigned i=0;i<4;++i) hmbtn_write_le16(b+80+i*2,(uint16_t)i);
}
int main(void)
{
    uint8_t bytes[88];floor_bytes(bytes,0);HmbtnModel ground;char error[256]={0};
    CHECK(hmbtn_model_open(&ground,bytes,sizeof(bytes),error,sizeof(error)));
    HmbtnPoultry p={0};HmbtnFarmClock date={0,0,1,1,12,0,0};HmbtnHouseInteraction h={0};
    const uint16_t name[9]={0,1,2,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};
    CHECK(hmbtn_poultry_buy(&p,name,&date));
    const int forward[4][2]={{0,-128},{-128,0},{0,128},{128,0}};
    for(unsigned facing=0;facing<4;++facing) {
        HmbtnHen *hen=&p.hens[0];hen->area=112;memset(hen->actor.position,0,sizeof(hen->actor.position));
        HmbtnIntroActor player={0};player.facing=facing;
        player.position[0]=-forward[facing][0];player.position[2]=-forward[facing][1];
        CHECK(hmbtn_poultry_occupies(&p,112,hen->actor.position));
        CHECK(hmbtn_poultry_carry_tick(&p,&h,&player,112,&ground,NULL,HMBTN_PAD_CROSS));
        CHECK(p.carry_phase==1 && hen->held && !hen->actor.visible && h.held_item.id==0x109
            && player.sprite.animation==37+facing && hmbtn_poultry_count(&p)==1 && hen->affection==10);
        CHECK(!hmbtn_poultry_occupies(&p,112,hen->actor.position) && !hmbtn_poultry_sell(&p,0));
        CHECK(!hmbtn_inventory_cycle(&h,false,false) && !hmbtn_inventory_swap(&h,9,0) && !hmbtn_inventory_swap(&h,9,10));
        CHECK(hmbtn_poultry_carry_tick(&p,&h,&player,112,&ground,NULL,HMBTN_PAD_CROSS) && p.carry_phase==1);
        player.animation_done=true;
        CHECK(hmbtn_poultry_carry_tick(&p,&h,&player,112,&ground,NULL,0) && !p.carry_phase && player.sprite.animation==41);
        CHECK(hmbtn_poultry_carry_tick(&p,&h,&player,0,&ground,NULL,HMBTN_PAD_CROSS) && p.carry_phase==2);
        CHECK(hen->held && h.held_item.id==0x109);player.animation_done=true;
        CHECK(hmbtn_poultry_carry_tick(&p,&h,&player,0,&ground,NULL,0));
        CHECK(!hen->held && hen->area==0 && hen->actor.visible && !h.held_item.id && !p.carry_phase
            && hmbtn_poultry_count(&p)==1 && hmbtn_poultry_occupies(&p,0,hen->actor.position)
            && !hmbtn_poultry_occupies(&p,112,hen->actor.position));
        CHECK(hen->actor.position[0]==forward[facing][0]/128*172 && hen->actor.position[2]==forward[facing][1]/128*172);
    }
    /* farm_object_contains reports 0x2008, not a solid 0x4000 hit. The source
     * selects chickens without applying the player's hard movement stop. */
    p.hens[0].area=112;memset(p.hens[0].actor.position,0,sizeof(p.hens[0].actor.position));
    HmbtnIntroActor player={0};player.position[2]=600;
    HmbtnHouseControl control={0};control.enabled=true;
    for(unsigned i=0;i<25;++i) hmbtn_farm_control_tick(&control,&player,&ground,NULL,HMBTN_PAD_DOWN);
    CHECK(player.position[2]<0);
    player.position[2]=300;
    CHECK(hmbtn_poultry_carry_tick(&p,&h,&player,112,&ground,NULL,HMBTN_PAD_CROSS));
    player.animation_done=true;CHECK(hmbtn_poultry_carry_tick(&p,&h,&player,112,&ground,NULL,0));
    control.carrying=control.carrying_big=true;
    for(unsigned i=0;i<20;++i) hmbtn_farm_control_tick(&control,&player,&ground,NULL,HMBTN_PAD_DOWN);
    CHECK(player.position[2]<0 && player.sprite.animation==42);
    /* Water, missing floor and another animal refuse a drop without clearing the hand. */
    player.position[2]=300;player.facing=0;
    floor_bytes(bytes,1);CHECK(hmbtn_model_open(&ground,bytes,sizeof(bytes),error,sizeof(error)));
    CHECK(hmbtn_poultry_carry_tick(&p,&h,&player,112,&ground,NULL,HMBTN_PAD_CROSS) && !p.carry_phase && p.hens[0].held);
    player.position[0]=10000;
    CHECK(hmbtn_poultry_carry_tick(&p,&h,&player,112,&ground,NULL,HMBTN_PAD_CROSS) && !p.carry_phase && h.held_item.id==0x109);
    player.position[0]=0;floor_bytes(bytes,0);CHECK(hmbtn_model_open(&ground,bytes,sizeof(bytes),error,sizeof(error)));
    CHECK(hmbtn_poultry_buy(&p,name,&date));memset(p.hens[1].actor.position,0,sizeof(p.hens[1].actor.position));
    CHECK(hmbtn_poultry_carry_tick(&p,&h,&player,112,&ground,NULL,HMBTN_PAD_CROSS) && !p.carry_phase && p.hens[0].held);
    p.hens[1].occupied=false;
    /* Chicks keep identity when carried through growth; deaths never leave a ghost hand. */
    p.hens[0].stage=HMBTN_HEN_CHICK;p.hens[0].growth=9;hmbtn_poultry_carry_sync(&p,&h);
    CHECK(h.held_item.id==0x10A);HmbtnFarmFeed feed={0};
    ++date.day;hmbtn_poultry_new_day(&p,&feed,&date);hmbtn_poultry_carry_sync(&p,&h);
    CHECK(p.hens[0].stage==HMBTN_HEN_ADULT && h.held_item.id==0x109 && p.hens[0].held);
    p.hens[0].age=5;++date.day;hmbtn_poultry_new_day(&p,&feed,&date);hmbtn_poultry_carry_sync(&p,&h);
    CHECK(p.hens[0].dead && !p.hens[0].held && !h.held_item.id && hmbtn_poultry_clear_dead(&p)==1);
    /* Five hourly outdoor updates feed an adult; coops/eggs stay tied to their areas. */
    CHECK(hmbtn_poultry_buy(&p,name,&date));HmbtnHen *hen=&p.hens[0];hen->area=0;hen->affection=100;
    hen->actor.position[0]=1000;hen->actor.position[2]=1300;feed.coop[0]=true;
    for(unsigned i=0;i<4;++i) hmbtn_poultry_hour(&p,&date,0);
    CHECK(!hen->fed_today);hmbtn_poultry_hour(&p,&date,0);CHECK(hen->fed_today && hen->grazing==5);
    ++date.day;hmbtn_poultry_new_day(&p,&feed,&date);
    CHECK(hen->fed_yesterday && hen->affection==110 && feed.coop[0] && !p.eggs[0].item && !hen->fed_today);
    ++date.day;hmbtn_poultry_new_day(&p,&feed,&date);
    CHECK(p.eggs[0].item==0x123 && p.eggs[0].area==0 && p.eggs[0].position[0]==1000 && p.eggs[0].position[2]==1300);
    player.position[0]=1000;player.position[2]=1524;player.facing=0;HmbtnFarmCarry carry={0};
    CHECK(!hmbtn_poultry_take_egg(&p,112,&h,&carry,&player));
    CHECK(hmbtn_poultry_take_egg(&p,0,&h,&carry,&player) && h.held_item.id==0x123);
    date.season=3;hmbtn_poultry_hour(&p,&date,0);CHECK(!hen->grazing && !hen->fed_today);
    date.season=0;unsigned affection=hen->affection;hmbtn_poultry_hour(&p,&date,1);
    CHECK(hen->affection==affection-5 && !hen->grazing);
    hen->area=80;++date.day;hmbtn_poultry_new_day(&p,&feed,&date);CHECK(hen->area==112);
    puts("Poultry carry: facings, reserved records, drops/refusals, interaction footprints, growth/death, outdoor feed and egg areas passed.");return 0;
}
