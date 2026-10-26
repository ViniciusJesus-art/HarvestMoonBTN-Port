#include "farm_menu.h"
#include "poultry.h"
#include "field_effects.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Line %d: %s\n",__LINE__,#x);return 1; } } while(0)
int main(void)
{
    HmbtnHouseInteraction h;HmbtnGameState h_state;hmbtn_game_state_init(&h_state);hmbtn_house_interaction_init(&h,&h_state);HmbtnFarmMenu m;hmbtn_farm_menu_init(&m,hmbtn_game_economy_const(&h_state));
    CHECK(m.economy->gold==500 && h.inventory->rucksack_level==0 && hmbtn_inventory_slot(h.inventory,2) && !hmbtn_inventory_slot(h.inventory,3));
    h.inventory->equipped=(HmbtnStoredTool){21,0,73};h.inventory->bag[0]=(HmbtnStoredTool){6,0,0};h.inventory->bag[1]=(HmbtnStoredTool){11,0,0};
    CHECK(hmbtn_inventory_cycle(h.inventory,true,false));CHECK(h.inventory->equipped.id==6 && h.inventory->bag[1].id==21 && h.inventory->bag[1].count==73);
    CHECK(hmbtn_inventory_cycle(h.inventory,true,true));CHECK(h.inventory->equipped.id==21 && h.inventory->equipped.count==73 && h.inventory->bag[0].id==6);
    CHECK(hmbtn_farm_menu_open(&m,HMBTN_PAD_TRIANGLE));m.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_CROSS,0);CHECK(m.marked==0);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_RIGHT,0);CHECK(m.cursor==1);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_CROSS,0);CHECK(m.marked==255 && h.inventory->equipped.id==6 && h.inventory->bag[0].id==21 && h.inventory->bag[0].count==73);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_TRIANGLE,0);CHECK(m.detail && m.request==21);
    m.message.phase=HMBTN_MESSAGE_CLOSED;hmbtn_farm_menu_tick(&m,h.inventory,0,0);CHECK(!m.detail && m.request==0x3ac);
    m.message.phase=HMBTN_MESSAGE_WAIT_END;hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_START,0);
    for(unsigned i=0;i<12;++i) hmbtn_farm_menu_tick(&m,h.inventory,0,0);
    CHECK(m.mode==HMBTN_MENU_NONE && m.message.phase==HMBTN_MESSAGE_CLOSED);
    CHECK(hmbtn_farm_menu_open(&m,HMBTN_PAD_START));
    for(unsigned i=0;i<20;++i) hmbtn_farm_menu_tick(&m,h.inventory,0,0);
    CHECK(m.phase==3 && m.page==0);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_RIGHT,0);CHECK(m.page==1);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_RIGHT,0);CHECK(m.page==5); /* Empty livestock pages skipped. */
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_LEFT,0);CHECK(m.page==1);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_START,0);
    for(unsigned i=0;i<20;++i) hmbtn_farm_menu_tick(&m,h.inventory,0,0);
    CHECK(m.mode==HMBTN_MENU_NONE);
    HmbtnPoultry poultry={0};poultry.upgraded=true;HmbtnFarmClock date={0,0,2,2,12,0,0};
    const uint16_t name[9]={0,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};
    for(unsigned i=0;i<10;++i) CHECK(hmbtn_poultry_buy(&poultry,name,&date));
    poultry.hens[0].held=true;poultry.hens[1].stage=HMBTN_HEN_EGG;
    poultry.hens[2].stage=HMBTN_HEN_CHICK;poultry.hens[3].stage=HMBTN_HEN_CHICK;poultry.hens[3].naming_pending=true;
    CHECK(hmbtn_poultry_count(&poultry)==8 && !hmbtn_poultry_room(&poultry));
    CHECK(hmbtn_poultry_status_slot(&poultry,0)==0 && hmbtn_poultry_status_slot(&poultry,1)==2
        && hmbtn_poultry_status_slot(&poultry,7)==9 && hmbtn_poultry_status_slot(&poultry,8)==HMBTN_HENS);
    m.poultry=&poultry;m.stats.animals[0]=8;
    CHECK(hmbtn_farm_menu_open(&m,HMBTN_PAD_START));
    for(unsigned i=0;i<20;++i) hmbtn_farm_menu_tick(&m,h.inventory,0,0);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_RIGHT,0);hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_RIGHT,0);
    CHECK(m.page==2 && !m.list_top);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_UP,0);CHECK(m.list_top==1);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_DOWN,0);CHECK(!m.list_top);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_DOWN,0);CHECK(m.list_top==1);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_LEFT,0);CHECK(m.page==1 && !m.list_top);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_RIGHT,0);CHECK(m.page==2 && !m.list_top);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_DOWN,0);CHECK(m.list_top==1);
    poultry.hens[9].occupied=false;m.stats.animals[0]=7;
    hmbtn_farm_menu_tick(&m,h.inventory,0,0);CHECK(!m.list_top);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_UP,0);CHECK(!m.list_top);
    hmbtn_farm_menu_tick(&m,h.inventory,HMBTN_PAD_CIRCLE,0);
    for(unsigned i=0;i<20;++i) hmbtn_farm_menu_tick(&m,h.inventory,0,0);
    CHECK(m.mode==HMBTN_MENU_NONE && m.poultry==&poultry);
    h.inventory->items[0]=(HmbtnStoredTool){34,0,70};h.inventory->items[1]=(HmbtnStoredTool){34,0,50};
    CHECK(hmbtn_inventory_swap(h.inventory,10,11));CHECK(h.inventory->items[0].count==21 && h.inventory->items[1].count==99);
    h.inventory->items[0]=(HmbtnStoredTool){0x3b,0,1};CHECK(!hmbtn_inventory_swap(h.inventory,10,11));
    CHECK(!hmbtn_inventory_swap(h.inventory,0,3));
    h.inventory->items[0]=(HmbtnStoredTool){64,0,1};h.inventory->items[1]=(HmbtnStoredTool){0};h.inventory->held_item=(HmbtnStoredTool){0};
    CHECK(hmbtn_inventory_cycle(h.inventory,false,false));CHECK(h.inventory->held_item.id==64 && h.inventory->items[0].id==0);
    CHECK(hmbtn_inventory_cycle(h.inventory,false,false));CHECK(!h.inventory->held_item.id && h.inventory->items[0].id==64);
    h.inventory->items[1]=(HmbtnStoredTool){58,0,1};h.inventory->held_item=(HmbtnStoredTool){62,0,1};
    CHECK(!hmbtn_inventory_cycle(h.inventory,false,false));CHECK(h.inventory->held_item.id==62 && h.inventory->items[1].id==58);
    HmbtnFieldEffect effects[25]={0};HmbtnAnimation art[7]={0};uint8_t data[16]={0};
    hmbtn_write_le16(data+4,3);art[4].data=data;art[4].section[3]=0;art[4].count[3]=2;
    CHECK(hmbtn_field_effect_submit(effects,0x3e,3,4));CHECK(effects[0].art==4 && effects[0].position[0]==-3000 && effects[0].position[2]==-200);
    for(unsigned i=0;i<8;++i) { hmbtn_field_effect_tick(effects,art,NULL);CHECK(effects[0].active); }
    hmbtn_field_effect_tick(effects,art,NULL);CHECK(!effects[0].active);
    for(unsigned i=0;i<25;++i) CHECK(hmbtn_field_effect_submit(effects,0x3e,3,4));
    CHECK(!hmbtn_field_effect_submit(effects,0x3e,3,4));CHECK(!hmbtn_field_effect_submit(effects,0x167,3,4));
    puts("Original backpack navigation, swaps/stacks, slot limits, cycling, status fades/pages and bounded break effects passed.");return 0;
}
