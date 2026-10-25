#include "village_talk.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Talk line %d: %s; %s\n",__LINE__,#x,error);return 1; } } while(0)
static size_t block(uint8_t *out,unsigned count)
{
    memset(out,0,512);hmbtn_write_le16(out+2,1);hmbtn_write_le16(out+4,0x4000);
    hmbtn_write_le32(out+56,count*16);out[60+(count-1)*16]=255;return 60+count*16;
}
static void step(uint8_t *out,unsigned at,unsigned type,unsigned yes,unsigned no,
    unsigned flags,int v,int element,int32_t x,int32_t y)
{
    uint8_t *p=out+60+at*16;
    p[0]=(uint8_t)type;p[1]=(uint8_t)yes;p[2]=(uint8_t)no;p[3]=(uint8_t)flags;
    hmbtn_write_le16(p+4,(uint16_t)v);hmbtn_write_le16(p+6,(uint16_t)element);
    hmbtn_write_le32(p+8,(uint32_t)x);hmbtn_write_le32(p+12,(uint32_t)y);
}
int main(void)
{
    char error[256]={0};uint8_t bytes[512];HmbtnVillageTalk talk;
    HmbtnVillage village;hmbtn_village_init(&village,NULL,0);
    HmbtnIntroActor player={0};HmbtnFarmClock clock={0,0,1,1,16,0,0};
    HmbtnHouseInteraction inventory;HmbtnGameState inventory_state;hmbtn_game_state_init(&inventory_state);hmbtn_house_interaction_init(&inventory,&inventory_state);
    HmbtnRegistration registration;hmbtn_registration_init(&registration);
    village.people[18].actor.facing=3;
    size_t length=block(bytes,7);
    /* Presenting a message must suspend this cursor, even when its original
     * other branch points at itself. Only the closed message may mark a visit. */
    step(bytes,0,13,4,1,0,46,18,4,1);
    step(bytes,1,2,2,1,1,0,123,0,0);
    step(bytes,2,19,3,3,0,46,18,1,0);
    step(bytes,3,37,6,3,1,0,0,0,0);
    step(bytes,4,2,5,4,1,0,124,0,0);
    step(bytes,5,37,6,5,1,0,0,0,0);
    CHECK(hmbtn_village_talk_open(&talk,18,bytes,length,&village,&player,&clock,&registration,&inventory,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && talk.pending && talk.request==123 && talk.script.pc==1);
    for(unsigned i=0;i<20;++i) CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && talk.script.pc==1);
    CHECK(!village.people[18].talk_flags);
    talk.message_done=true;talk.result=2;
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && talk.slots[0]==2);
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && !talk.active);
    CHECK(village.people[18].talk_flags==0x10 && village.people[18].actor.facing==3);
    CHECK(hmbtn_village_talk_open(&talk,18,bytes,length,&village,&player,&clock,&registration,&inventory,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && talk.request==124);
    /* An unsupported shop/story action must roll back preceding script flags. */
    length=block(bytes,6);
    step(bytes,0,19,255,255,0,100,0,1,0);
    step(bytes,1,19,255,255,0,46,18,0,0);
    step(bytes,2,19,255,255,0,44,18,1,0);
    step(bytes,3,19,255,255,0,45,18,1,0);
    step(bytes,4,81,5,5,0,8,0,0,0);
    CHECK(hmbtn_village_talk_open(&talk,18,bytes,length,&village,&player,&clock,&registration,&inventory,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && !talk.active && talk.script.boundary);
    CHECK(!village.flags[52] && village.people[18].talk_flags==0x10);
    /* The rolled-back daily gain must never be credited next morning. */
    village.people[18].talk_flags|=HMBTN_VILLAGER_TALKED_TODAY;
    hmbtn_village_clock(&village,&clock,HMBTN_CLOCK_DAY,0,false);
    CHECK(village.people[18].friendship==50 && village.people[18].talk_flags==0x10);
    /* Scripts consult and mutate actual byte-sized story state, not constants.
     * A boundary must roll these counters back with the other script state. */
    length=block(bytes,5);
    step(bytes,0,19,255,255,0,40,35,10,0);
    step(bytes,1,19,255,255,0,41,0,2,0);
    step(bytes,2,19,255,255,0,42,127,99,0);
    step(bytes,3,81,4,4,0,8,0,0,0);
    HmbtnEventDays before=village.days;
    CHECK(hmbtn_village_talk_open(&talk,18,bytes,length,&village,&player,&clock,&registration,&inventory,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && talk.script.boundary
        && !memcmp(&before,&village.days,sizeof(before)));
    length=block(bytes,7);
    step(bytes,0,19,255,255,0,40,35,10,0);
    step(bytes,1,19,255,255,0,41,0,2,0);
    step(bytes,2,19,255,255,0,42,127,999,0);
    step(bytes,3,22,255,255,0,40,35,3,0);
    step(bytes,4,22,255,255,0,41,0,4,0);
    step(bytes,5,37,6,5,1,0,0,0,0);
    CHECK(hmbtn_village_talk_open(&talk,18,bytes,length,&village,&player,&clock,&registration,&inventory,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && !talk.active
        && talk.slots[3]==10 && talk.slots[4]==2 && village.days.values[127]==255);
    hmbtn_event_days_advance(&village.days,&clock,0,false);
    step(bytes,0,22,6,6,0,40,35,0,0);
    CHECK(hmbtn_village_talk_open(&talk,18,bytes,length,&village,&player,&clock,&registration,&inventory,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && talk.slots[0]==11);
    step(bytes,0,19,6,6,0,40,128,1,0);
    CHECK(hmbtn_village_talk_open(&talk,18,bytes,length,&village,&player,&clock,&registration,&inventory,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && talk.script.boundary);
    /* Gift effects are transactional across all villagers and product stores. */
    length=block(bytes,9);inventory.inventory->held_item=(HmbtnStoredTool){161,0,7};inventory.event_count1=14;
    inventory.inventory->items[0]=(HmbtnStoredTool){162,2,3};
    const HmbtnStoredTool gift=inventory.inventory->held_item,bag_item=inventory.inventory->items[0];
    const unsigned lillia=village.people[18].friendship,louis=village.people[31].friendship;
    const uint16_t louis_flags=village.people[31].talk_flags;
    step(bytes,0,58,255,255,0,11,1,0,0);
    step(bytes,1,57,255,255,0,18,10,0,0);
    step(bytes,2,30,255,255,0,11,1,0,3);
    step(bytes,3,81,255,255,0,22,0,0,0);
    step(bytes,4,19,255,255,0,32,18,9,1);
    step(bytes,5,19,255,255,0,32,31,3,1);
    step(bytes,6,19,255,255,0,45,31,1,0);
    step(bytes,7,81,8,8,0,8,0,0,0);
    CHECK(hmbtn_village_talk_open(&talk,18,bytes,length,&village,&player,&clock,&registration,&inventory,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && talk.script.boundary && talk.slots[10]==1);
    CHECK(!memcmp(&inventory.inventory->held_item,&gift,sizeof(gift)) && inventory.event_count1==14
        && village.people[18].friendship==lillia && village.people[31].friendship==louis
        && village.people[31].talk_flags==louis_flags);
    step(bytes,7,37,8,8,1,0,0,0,0);
    CHECK(hmbtn_village_talk_open(&talk,18,bytes,length,&village,&player,&clock,&registration,&inventory,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && !talk.active && !talk.script.boundary);
    CHECK(!inventory.inventory->held_item.id && inventory.event_count1==15 && village.people[18].friendship==lillia+9
        && village.people[31].friendship==louis+3 && (village.people[31].talk_flags&HMBTN_VILLAGER_TALKED_TODAY)
        && !memcmp(&inventory.inventory->items[0],&bag_item,sizeof(bag_item)));
    inventory.inventory->held_item=gift;
    step(bytes,0,21,255,255,0,11,0,0x100A1,0); /* Indirect item uses only the signed low halfword. */
    CHECK(hmbtn_village_talk_open(&talk,18,bytes,length,&village,&player,&clock,&registration,&inventory,error,sizeof(error)));
    CHECK(hmbtn_village_talk_tick(&talk,error,sizeof(error)) && !talk.active && !inventory.inventory->held_item.id
        && inventory.event_count1==15);
    /* Invalid held item, malformed blocks and indirect return bounds. */
    inventory.inventory->held_item.id=380;
    CHECK(!hmbtn_village_talk_open(&talk,18,bytes,length,&village,&player,&clock,&registration,&inventory,error,sizeof(error)));
    inventory.inventory->held_item.id=0;
    CHECK(!hmbtn_village_talk_open(&talk,18,bytes,59,&village,&player,&clock,&registration,&inventory,error,sizeof(error)));
    length=block(bytes,2);step(bytes,0,80,1,1,0,0,0,0,0);
    CHECK(hmbtn_village_talk_open(&talk,18,bytes,length,&village,&player,&clock,&registration,&inventory,error,sizeof(error)));
    talk.slots[0]=206;CHECK(!hmbtn_village_talk_tick(&talk,error,sizeof(error)));
    puts("Conversation suspend/close, first/repeat flags, response slots, rollback and malformed operand bounds passed.");return 0;
}
