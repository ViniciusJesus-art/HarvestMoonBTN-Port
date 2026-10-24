#include "poultry.h"
#include <stdio.h>
#include <string.h>
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
    HmbtnHouseInteraction inventory;hmbtn_house_interaction_init(&inventory);HmbtnFarmCarry carry={0};HmbtnIntroActor player={0};
    memcpy(player.position,p.eggs[0].position,sizeof(player.position));player.position[2]+=224;
    CHECK(hmbtn_poultry_take_egg(&p,&inventory,&carry,&player));
    CHECK(inventory.held_item.id==0x122 && !p.eggs[0].item && carry.phase==1);
    CHECK(!hmbtn_poultry_take_egg(&p,&inventory,&carry,&player));
    player.animation_done=true;CHECK(hmbtn_farm_carry_tick(&carry,&inventory,NULL,&player,0));
    CHECK(!carry.phase && hmbtn_inventory_cycle(&inventory,false,false) && !inventory.held_item.id);
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
    hmbtn_poultry_tick(&p,NULL,&player,21);CHECK(p.hens[0].actor.sprite.animation==18);
    p.hens[0].age=5;++clock.day;hmbtn_poultry_new_day(&p,&feed,&clock);
    CHECK(p.hens[0].dead && !p.hens[0].actor.visible);
    puts("Coop capacity, names, FIFO feeding, prior-day eggs, grades, affection, pickup and age passed.");return 0;
}
