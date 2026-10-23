#include "farm_menu.h"
#include "field_effects.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Line %d: %s\n",__LINE__,#x);return 1; } } while(0)
int main(void)
{
    HmbtnHouseInteraction h;hmbtn_house_interaction_init(&h);HmbtnFarmMenu m;hmbtn_farm_menu_init(&m);
    CHECK(m.stats.gold==500 && h.rucksack_level==0 && hmbtn_inventory_slot(&h,2) && !hmbtn_inventory_slot(&h,3));
    h.equipped=(HmbtnStoredTool){21,0,73};h.bag[0]=(HmbtnStoredTool){6,0,0};h.bag[1]=(HmbtnStoredTool){11,0,0};
    CHECK(hmbtn_inventory_cycle(&h,true,false));CHECK(h.equipped.id==6 && h.bag[1].id==21 && h.bag[1].count==73);
    CHECK(hmbtn_inventory_cycle(&h,true,true));CHECK(h.equipped.id==21 && h.equipped.count==73 && h.bag[0].id==6);
    CHECK(hmbtn_farm_menu_open(&m,HMBTN_PAD_TRIANGLE));m.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_farm_menu_tick(&m,&h,HMBTN_PAD_CROSS,0);CHECK(m.marked==0);
    hmbtn_farm_menu_tick(&m,&h,HMBTN_PAD_RIGHT,0);CHECK(m.cursor==1);
    hmbtn_farm_menu_tick(&m,&h,HMBTN_PAD_CROSS,0);CHECK(m.marked==255 && h.equipped.id==6 && h.bag[0].id==21 && h.bag[0].count==73);
    hmbtn_farm_menu_tick(&m,&h,HMBTN_PAD_TRIANGLE,0);CHECK(m.detail && m.request==21);
    m.message.phase=HMBTN_MESSAGE_CLOSED;hmbtn_farm_menu_tick(&m,&h,0,0);CHECK(!m.detail && m.request==0x3ac);
    m.message.phase=HMBTN_MESSAGE_WAIT_END;hmbtn_farm_menu_tick(&m,&h,HMBTN_PAD_START,0);
    for(unsigned i=0;i<12;++i) hmbtn_farm_menu_tick(&m,&h,0,0);
    CHECK(m.mode==HMBTN_MENU_NONE && m.message.phase==HMBTN_MESSAGE_CLOSED);
    CHECK(hmbtn_farm_menu_open(&m,HMBTN_PAD_START));
    for(unsigned i=0;i<20;++i) hmbtn_farm_menu_tick(&m,&h,0,0);
    CHECK(m.phase==3 && m.page==0);
    hmbtn_farm_menu_tick(&m,&h,HMBTN_PAD_RIGHT,0);CHECK(m.page==1);
    hmbtn_farm_menu_tick(&m,&h,HMBTN_PAD_RIGHT,0);CHECK(m.page==5); /* Empty livestock pages skipped. */
    hmbtn_farm_menu_tick(&m,&h,HMBTN_PAD_LEFT,0);CHECK(m.page==1);
    hmbtn_farm_menu_tick(&m,&h,HMBTN_PAD_START,0);
    for(unsigned i=0;i<20;++i) hmbtn_farm_menu_tick(&m,&h,0,0);
    CHECK(m.mode==HMBTN_MENU_NONE);
    h.items[0]=(HmbtnStoredTool){34,0,70};h.items[1]=(HmbtnStoredTool){34,0,50};
    CHECK(hmbtn_inventory_swap(&h,10,11));CHECK(h.items[0].count==21 && h.items[1].count==99);
    h.items[0]=(HmbtnStoredTool){0x3b,0,1};CHECK(!hmbtn_inventory_swap(&h,10,11));
    CHECK(!hmbtn_inventory_swap(&h,0,3));
    h.items[0]=(HmbtnStoredTool){64,0,1};h.items[1]=(HmbtnStoredTool){0};h.held_item=(HmbtnStoredTool){0};
    CHECK(hmbtn_inventory_cycle(&h,false,false));CHECK(h.held_item.id==64 && h.items[0].id==0);
    CHECK(hmbtn_inventory_cycle(&h,false,false));CHECK(!h.held_item.id && h.items[0].id==64);
    h.items[1]=(HmbtnStoredTool){58,0,1};h.held_item=(HmbtnStoredTool){62,0,1};
    CHECK(!hmbtn_inventory_cycle(&h,false,false));CHECK(h.held_item.id==62 && h.items[1].id==58);
    HmbtnFieldEffect effects[25]={0};HmbtnAnimation art[7]={0};uint8_t data[16]={0};
    hmbtn_write_le16(data+4,3);art[4].data=data;art[4].section[3]=0;art[4].count[3]=2;
    CHECK(hmbtn_field_effect_submit(effects,0x3e,3,4));CHECK(effects[0].art==4 && effects[0].position[0]==-3000 && effects[0].position[2]==-200);
    for(unsigned i=0;i<8;++i) { hmbtn_field_effect_tick(effects,art);CHECK(effects[0].active); }
    hmbtn_field_effect_tick(effects,art);CHECK(!effects[0].active);
    for(unsigned i=0;i<25;++i) CHECK(hmbtn_field_effect_submit(effects,0x3e,3,4));
    CHECK(!hmbtn_field_effect_submit(effects,0x3e,3,4));CHECK(!hmbtn_field_effect_submit(effects,0x167,3,4));
    puts("Original backpack navigation, swaps/stacks, slot limits, cycling, status fades/pages and bounded break effects passed.");return 0;
}
