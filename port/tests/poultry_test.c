#include "farm_menu.h"
#include "poultry.h"
#include <stdio.h>
#include <string.h>
#include "psx_data.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Poultry line %d: %s\n",__LINE__,#x);return 1; } } while(0)
int main(void)
{
    HmbtnPoultry p={0};HmbtnFarmFeed feed={0};HmbtnFarmClock clock={0,0,1,2,6,0,0};
    uint16_t name[9]={0,1,2,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};
    CHECK(hmbtn_poultry_room(&p) && !hmbtn_poultry_count(&p));
    CHECK(hmbtn_poultry_buy(&p,name,&clock));
    CHECK(p.hens[0].affection==10 && p.hens[0].actor.position[0]==-760);
    CHECK(memcmp(name,p.hens[0].name,sizeof(name))==0);
    feed.coop[3]=true;++clock.day;hmbtn_poultry_new_day(&p,&feed,&clock);
    CHECK(!p.eggs[0].item && !feed.coop[3] && p.hens[0].fed_yesterday && p.hens[0].affection==20);
    feed.coop[1]=feed.coop[4]=true;++clock.day;hmbtn_poultry_new_day(&p,&feed,&clock);
    CHECK(p.eggs[0].item==0x122 && !feed.coop[1] && feed.coop[4] && p.hens[0].affection==30);
    HmbtnHouseInteraction inventory;HmbtnGameState inventory_state;hmbtn_game_state_init(&inventory_state);hmbtn_house_interaction_init(&inventory,&inventory_state);HmbtnFarmCarry carry={0};HmbtnIntroActor player={0};
    memcpy(player.position,p.eggs[0].position,sizeof(player.position));player.position[2]+=224;
    CHECK(hmbtn_poultry_take_egg(&p,112,&inventory,&carry,&player));
    CHECK(inventory.inventory->held_item.id==0x122 && !p.eggs[0].item && carry.phase==1);
    CHECK(!hmbtn_poultry_take_egg(&p,112,&inventory,&carry,&player));
    player.animation_done=true;CHECK(hmbtn_farm_carry_tick(&carry,&inventory,NULL,&player,0));
    CHECK(!carry.phase && hmbtn_inventory_cycle(inventory.inventory,false,false) && !inventory.inventory->held_item.id);
    p.hens[0].affection=100;feed.coop[4]=false;++clock.day;hmbtn_poultry_new_day(&p,&feed,&clock);
    CHECK(p.eggs[0].item==0x123 && p.hens[0].affection==80 && !p.hens[0].fed_yesterday);
    ++clock.day;hmbtn_poultry_new_day(&p,&feed,&clock);CHECK(!p.eggs[0].item && p.hens[0].affection==60);
    p.hens[0].affection=200;p.hens[0].fed_yesterday=true;feed.coop[0]=true;
    ++clock.day;hmbtn_poultry_new_day(&p,&feed,&clock);CHECK(p.eggs[0].item==0x124);
    p.hens[0].prize=true;p.hens[0].affection=250;feed.coop[0]=true;
    ++clock.day;hmbtn_poultry_new_day(&p,&feed,&clock);CHECK(p.eggs[0].item==0xE8 && p.hens[0].affection==255);
    for(unsigned i=1;i<5;++i) CHECK(hmbtn_poultry_buy(&p,name,&clock));
    CHECK(!hmbtn_poultry_room(&p) && !hmbtn_poultry_buy(&p,name,&clock));
    p.upgraded=true;for(unsigned i=5;i<10;++i) CHECK(hmbtn_poultry_buy(&p,name,&clock));
    CHECK(!hmbtn_poultry_room(&p) && hmbtn_poultry_count(&p)==10);
    hmbtn_poultry_tick(&p,112,NULL,NULL,&player,21);CHECK(p.hens[0].actor.sprite.animation==5+p.hens[0].actor.facing);
    p.hens[0].age=5;++clock.day;hmbtn_poultry_new_day(&p,&feed,&clock);
    CHECK(p.hens[0].dead && !p.hens[0].actor.visible);
    CHECK(hmbtn_poultry_dead(&p,true) && hmbtn_poultry_clear_dead(&p)>=1 && hmbtn_poultry_room(&p));
    HmbtnPoultry q={0};feed=(HmbtnFarmFeed){0};
    CHECK(hmbtn_poultry_incubate(&q,0,0x122,&clock)==0 && q.hens[0].stage==HMBTN_HEN_EGG);
    CHECK(!hmbtn_poultry_count(&q) && q.hens[0].occupied && hmbtn_poultry_room(&q));
    CHECK(hmbtn_poultry_incubate(&q,0,0xE8,&clock)==0x1E6);
    CHECK(hmbtn_poultry_incubate(&q,1,0x122,&clock)==0x1E6);
    feed.coop[0]=true;
    for(unsigned day=1;day<=3;++day) { ++clock.day;hmbtn_poultry_new_day(&q,&feed,&clock);CHECK(q.hens[0].growth==day); }
    CHECK(feed.coop[0] && q.hens[0].stage==HMBTN_HEN_CHICK && q.hens[0].naming_pending && q.hens[0].affection==10);
    ++clock.day;hmbtn_poultry_new_day(&q,&feed,&clock);CHECK(q.hens[0].growth==3);
    memcpy(q.hens[0].name,name,sizeof(name));hmbtn_poultry_name_end(&q);
    CHECK(!q.hens[0].naming_pending && !q.hens[0].incubator && !q.hens[0].actor.position[1]);
    for(unsigned day=1;day<=7;++day) {
        ++clock.day;hmbtn_poultry_new_day(&q,&feed,&clock);CHECK(feed.coop[0] && !q.eggs[0].item);
        CHECK(q.hens[0].stage==(day==7 ? HMBTN_HEN_ADULT : HMBTN_HEN_CHICK));
    }
    ++clock.day;hmbtn_poultry_new_day(&q,&feed,&clock);
    CHECK(!feed.coop[0] && q.hens[0].fed_yesterday && q.hens[0].affection==20 && !q.eggs[0].item);
    feed.coop[0]=true;++clock.day;hmbtn_poultry_new_day(&q,&feed,&clock);CHECK(q.eggs[0].item==0x122);
    for(unsigned i=1;i<5;++i) CHECK(hmbtn_poultry_buy(&q,name,&clock));
    CHECK(hmbtn_poultry_incubate(&q,0,0x122,&clock)==0x1A1);
    q.upgraded=true;CHECK(!hmbtn_poultry_incubate(&q,1,0xE8,&clock) && q.hens[5].golden_egg && !q.hens[5].prize);
    /* Expanded death lists preserve every name and fit the message page renderer. */
    uint8_t record[8192]={0};for(unsigned i=0;i<10;++i) {
        q.hens[i].occupied=q.hens[i].dead=true;q.hens[i].died_of_age=true;
        for(unsigned j=0;j<8;++j) q.hens[i].name[j]=(uint16_t)i;
        q.hens[i].name[8]=0xFFFF;
    }
    hmbtn_write_le16(record+0x1800,0x401D);hmbtn_write_le16(record+0x1802,0xFFFF);
    CHECK(hmbtn_poultry_expand_dead(&q,record,sizeof(record)));
    HmbtnMessage message;char error[256];CHECK(hmbtn_message_open(&message,record,sizeof(record),error,sizeof(error)));
    unsigned names=0,pages=0;for(unsigned i=0;i<message.count;++i) { if(message.words[i]<10) ++names;if(message.words[i]==0xFFC) ++pages; }
    CHECK(names==80 && pages>=2 && hmbtn_poultry_clear_dead(&q)==10 && !hmbtn_poultry_count(&q));
    CHECK(!hmbtn_poultry_expand_dead(&q,record,32));
    HmbtnFarmAction action;hmbtn_farm_action_init(&action);
    HmbtnPoultry medicine={0};CHECK(hmbtn_poultry_buy(&medicine,name,&clock));
    CHECK(hmbtn_poultry_buy(&medicine,name,&clock));
    inventory_state.inventory=(HmbtnInventory){0};
    hmbtn_house_interaction_init(&inventory,&inventory_state);inventory_state.clock.hour=12;
    const int delta[4][2]={{0,-128},{-128,0},{0,128},{128,0}};
    for(unsigned facing=0;facing<4;++facing) {
        player=(HmbtnIntroActor){0};player.facing=facing;
        memcpy(player.position,medicine.hens[0].actor.position,sizeof(player.position));
        player.position[0]-=delta[facing][0];player.position[2]-=delta[facing][1];
        inventory.inventory->equipped=(HmbtnStoredTool){0x1E,0,2};medicine.hens[0].days_sick=2;medicine.hens[1].days_sick=3;
        CHECK(hmbtn_poultry_medicine_tick(&medicine,112,&action,&player,&inventory,HMBTN_PAD_SQUARE));
        CHECK(action.phase==HMBTN_FARM_MEDICINE && action.bank==22 && action.bank_pending
            && action.target==0 && player.sprite.animation==facing && inventory.inventory->equipped.count==2);
        CHECK(hmbtn_poultry_medicine_tick(&medicine,112,&action,&player,&inventory,HMBTN_PAD_SQUARE));
        CHECK(medicine.hens[0].days_sick==2 && inventory.inventory->equipped.count==2);
        player.animation_done=true;
        CHECK(hmbtn_poultry_medicine_tick(&medicine,112,&action,&player,&inventory,0));
        CHECK(!medicine.hens[0].days_sick && medicine.hens[1].days_sick==3 && medicine.hens[0].praise==45
            && action.finished_bank && action.changed && action.sound==17 && inventory.inventory->equipped.count==1);
        CHECK(action.stamina==100 && !action.fatigue && action.phase==HMBTN_FARM_ACTION_FREE);
    }
    /* A miss still consumes one dose. The target is latched, never reacquired. */
    player.position[0]=10000;inventory.inventory->equipped=(HmbtnStoredTool){0x1E,0,1};
    medicine.hens[0].days_sick=2;
    CHECK(hmbtn_poultry_medicine_tick(&medicine,112,&action,&player,&inventory,HMBTN_PAD_SQUARE) && action.target==HMBTN_HENS);
    player.position[0]=medicine.hens[0].actor.position[0];player.animation_done=true;
    CHECK(hmbtn_poultry_medicine_tick(&medicine,112,&action,&player,&inventory,0));
    CHECK(medicine.hens[0].days_sick==2 && !inventory.inventory->equipped.id && !action.changed);
    inventory.inventory->equipped=(HmbtnStoredTool){0x1E,0,1};inventory.inventory->held_item.id=0x122;
    CHECK(!hmbtn_poultry_medicine_tick(&medicine,112,&action,&player,&inventory,HMBTN_PAD_SQUARE));
    inventory.inventory->held_item.id=0;action.fatigue=100;
    CHECK(!hmbtn_poultry_medicine_tick(&medicine,112,&action,&player,&inventory,HMBTN_PAD_SQUARE));
    action.fatigue=0;inventory_state.clock.hour=23;inventory_state.weather.today=1;action.stamina=0;
    CHECK(hmbtn_poultry_medicine_tick(NULL,112,&action,&player,&inventory,HMBTN_PAD_SQUARE));
    player.animation_done=true;CHECK(hmbtn_poultry_medicine_tick(NULL,112,&action,&player,&inventory,0));
    CHECK(action.fatigue==4 && !inventory.inventory->equipped.id && medicine.hens[0].days_sick==2);
    puts("Coop feeding, eggs, lifecycle, death, medicine targeting, consumption, fatigue and message lists passed.");return 0;
}
