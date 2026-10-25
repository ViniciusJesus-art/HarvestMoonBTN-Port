#include "farm_gather.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Gather line %d: %s\n",__LINE__,#x);return 1; } } while(0)
int main(void)
{
    HmbtnFarmGather g={0};const HmbtnGatherRecord *r=hmbtn_farm_gather_record(146);CHECK(r);
    unsigned bamboo=hmbtn_farm_gather_item(&g,146,r->location,0);
    CHECK(bamboo && hmbtn_farm_item(bamboo)->sell_price==50 && hmbtn_estate_item_art(bamboo)==11);
    CHECK(!hmbtn_farm_gather_item(&g,146,r->location,1));
    CHECK(!hmbtn_farm_gather_item(&g,146,999,0));
    HmbtnHouseInteraction h;HmbtnGameState h_state;hmbtn_game_state_init(&h_state);hmbtn_house_interaction_init(&h,&h_state);HmbtnFarmCarry carry={0};HmbtnIntroActor p={0};
    memcpy(p.position,r->position,sizeof(p.position));p.position[2]+=224;
    p.facing=2;CHECK(!hmbtn_farm_gather_take(&g,r->location,0,&h,&carry,&p));p.facing=0;
    CHECK(hmbtn_farm_gather_take(&g,r->location,0,&h,&carry,&p));
    CHECK(g.taken[146] && h.inventory->held_item.id==bamboo && carry.phase==1 && p.sprite.animation==11);
    CHECK(!hmbtn_farm_gather_take(&g,r->location,0,&h,&carry,&p));
    CHECK(!hmbtn_farm_gather_item(&g,146,r->location,0));
    hmbtn_farm_gather_new_day(&g);CHECK(hmbtn_farm_gather_item(&g,146,r->location,0)==bamboo);
    for(unsigned season=0;season<4;++season) for(unsigned i=125;i<=172;++i) {
        r=hmbtn_farm_gather_record(i);unsigned id=hmbtn_farm_gather_item(&g,i,r->location,season);
        if(id) CHECK(hmbtn_estate_item_art(id)>=5 && hmbtn_estate_item_art(id)<=19);
        if(season==3) CHECK(!id);
    }
    puts("Original forage positions, seasonal substitutions, daily return, facing and single pickup passed.");return 0;
}
