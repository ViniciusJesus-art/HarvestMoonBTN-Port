#include "village_talk.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Tool grant line %d: %s; %s\n",__LINE__,#x,error);return 1; } } while(0)
static void command(uint8_t *p,unsigned type,unsigned yes,unsigned no,int item,int table,int quiet,int notify)
{
    p[0]=(uint8_t)type;p[1]=(uint8_t)yes;p[2]=(uint8_t)no;
    hmbtn_write_le16(p+4,(uint16_t)item);hmbtn_write_le16(p+6,(uint16_t)table);
    hmbtn_write_le32(p+8,(uint32_t)quiet);hmbtn_write_le32(p+12,(uint32_t)notify);
}
static size_t block(uint8_t bytes[140],int item,int table,int quiet,int notify,bool boundary)
{
    memset(bytes,0,140);hmbtn_write_le16(bytes+2,1);hmbtn_write_le16(bytes+4,0x4000);
    hmbtn_write_le32(bytes+56,80);
    command(bytes+60,32,1,2,item,table,quiet,notify);
    command(bytes+76,21,3,3,0,0,1,0);
    command(bytes+92,21,3,3,0,0,0,0);
    command(bytes+108,boundary ? 81 : 37,4,4,boundary ? 8 : 0,0,0,0);
    bytes[124]=255;return 140;
}
int main(void)
{
    char error[256]={0};uint8_t bytes[140];HmbtnVillageTalk t;HmbtnVillage v;
    HmbtnHouseInteraction h;HmbtnNewGame n;HmbtnIntroActor p={0};
    const HmbtnFarmClock c={0,0,2,2,7,0,0};
    hmbtn_village_init(&v,NULL,0);hmbtn_house_interaction_init(&h);hmbtn_new_game_init(&n);
    size_t size=block(bytes,56,0,1,1,false);
    CHECK(hmbtn_village_talk_open(&t,32,bytes,size,&v,&p,&c,&n,&h,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&t,error,sizeof(error)) && !t.active && !t.script.boundary && t.slots[0]==1);
    CHECK(h.equipped.id==56 && h.equipped.count==1 && !h.equipped.modifier && t.show_tool==56 && t.show_frames==11);
    for(unsigned level=0;level<3;++level) {
        const unsigned capacity=2u<<level;
        hmbtn_house_interaction_init(&h);h.rucksack_level=level;h.equipped=(HmbtnStoredTool){1,4,9};
        for(unsigned i=0;i<capacity;++i) h.bag[i]=(HmbtnStoredTool){6,2,7};
        h.bag[capacity-1]=(HmbtnStoredTool){0,9,9};
        CHECK(hmbtn_village_talk_open(&t,32,bytes,size,&v,&p,&c,&n,&h,error,sizeof(error)));
        CHECK(hmbtn_village_talk_tick(&t,error,sizeof(error)) && t.slots[0]==1 && t.show_tool==56);
        CHECK(h.bag[capacity-1].id==56 && !h.bag[capacity-1].modifier && h.bag[capacity-1].count==1);
        CHECK(h.equipped.id==1 && h.equipped.modifier==4 && h.equipped.count==9);
        HmbtnHouseInteraction before=h;
        CHECK(hmbtn_village_talk_open(&t,32,bytes,size,&v,&p,&c,&n,&h,error,sizeof(error)));
        CHECK(hmbtn_village_talk_tick(&t,error,sizeof(error)) && !t.slots[0] && !t.show_tool && !t.show_frames);
        CHECK(!memcmp(&h,&before,sizeof(h))); /* Toolbox space cannot bypass quiet=1. */
    }
    /* A non-quiet grant may spill to the first toolbox hole, without a pose. */
    size=block(bytes,56,0,0,1,false);h.toolbox[2]=(HmbtnStoredTool){0,9,9};
    CHECK(hmbtn_village_talk_open(&t,32,bytes,size,&v,&p,&c,&n,&h,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&t,error,sizeof(error)) && t.slots[0]==1 && !t.show_tool);
    CHECK(h.toolbox[2].id==56 && h.toolbox[2].count==1 && !h.toolbox[2].modifier);
    for(unsigned i=0;i<64;++i) h.toolbox[i]=(HmbtnStoredTool){6,2,7};
    HmbtnHouseInteraction before=h;
    CHECK(hmbtn_village_talk_open(&t,32,bytes,size,&v,&p,&c,&n,&h,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&t,error,sizeof(error)) && !t.slots[0] && !memcmp(&h,&before,sizeof(h)));
    /* Table mode reads the signed low halfword, just like script_vars[].half. */
    hmbtn_house_interaction_init(&h);size=block(bytes,7,1,1,2,false);
    CHECK(hmbtn_village_talk_open(&t,32,bytes,size,&v,&p,&c,&n,&h,error,sizeof(error)));t.slots[7]=0x10019;
    CHECK(hmbtn_village_talk_tick(&t,error,sizeof(error)) && h.equipped.id==25
        && h.equipped.modifier==4 && h.equipped.count==1 && t.show_tool==25 && t.show_frames==61);
    hmbtn_house_interaction_init(&h);size=block(bytes,56,0,1,0,false);
    CHECK(hmbtn_village_talk_open(&t,32,bytes,size,&v,&p,&c,&n,&h,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&t,error,sizeof(error)) && h.equipped.id==56 && !t.show_tool);
    /* A subsequent unsupported action restores all tool stores and the cue. */
    hmbtn_house_interaction_init(&h);before=h;size=block(bytes,56,0,1,1,true);
    CHECK(hmbtn_village_talk_open(&t,32,bytes,size,&v,&p,&c,&n,&h,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&t,error,sizeof(error)) && t.script.boundary && !t.show_tool
        && !t.show_frames && !memcmp(&h,&before,sizeof(h)));
    h.equipped.id=1;for(unsigned i=0;i<2;++i) h.bag[i].id=6;
    before=h;size=block(bytes,56,0,0,1,true);
    CHECK(hmbtn_village_talk_open(&t,32,bytes,size,&v,&p,&c,&n,&h,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&t,error,sizeof(error)) && t.script.boundary && !memcmp(&h,&before,sizeof(h)));
    size=block(bytes,380,0,1,1,false);
    CHECK(hmbtn_village_talk_open(&t,32,bytes,size,&v,&p,&c,&n,&h,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&t,error,sizeof(error)) && t.script.boundary && !memcmp(&h,&before,sizeof(h)));
    size=block(bytes,32,1,1,1,false);
    CHECK(hmbtn_village_talk_open(&t,32,bytes,size,&v,&p,&c,&n,&h,error,sizeof(error)));
    CHECK(!hmbtn_village_talk_tick(&t,error,sizeof(error)));
    puts("Original event tool storage order, capacities, modifiers, notifications, table halfword and rollback passed.");return 0;
}
