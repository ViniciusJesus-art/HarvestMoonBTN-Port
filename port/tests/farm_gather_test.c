#include "farm_gather.h"
#include "save_fixture.h"
#include <stdio.h>
#include <stdlib.h>
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
    HmbtnSession *s=save_fixture(1);char error[256]={0};
    s->game.clock.season=2;s->daily_date=hmbtn_session_date(&s->game.clock);
    s->game.inventory.held_item=(HmbtnStoredTool){0};
    for(unsigned i=1;i<=3;++i) {
        r=hmbtn_farm_gather_record(i);CHECK(r && r->item==87 && r->location==1 && r->flags==0x4a8);
        for(unsigned season=0;season<4;++season) CHECK(hmbtn_farm_gather_item(&s->gather,i,1,season)==(season==2 ? 87u : 0u));
        CHECK(!hmbtn_farm_gather_item(&s->gather,i,2,2) && !hmbtn_farm_gather_item(&s->gather,i,6,2));
        p=(HmbtnIntroActor){.facing=0};memcpy(p.position,r->position,sizeof(p.position));p.position[2]+=224;
        HmbtnInventory before=s->game.inventory;s->game.inventory.held_item=(HmbtnStoredTool){89,7,1};
        carry=(HmbtnFarmCarry){0};CHECK(!hmbtn_farm_gather_take(&s->gather,1,2,&s->house,&carry,&p));
        CHECK(s->game.inventory.held_item.id==89 && s->game.inventory.held_item.modifier==7 && !s->gather.taken[i]);
        s->game.inventory=before;
        p.position[1]+=200;CHECK(!hmbtn_farm_gather_take(&s->gather,1,2,&s->house,&carry,&p));p.position[1]-=200;
        CHECK(hmbtn_farm_gather_take(&s->gather,1,2,&s->house,&carry,&p));
        CHECK(s->gather.taken[i] && s->game.inventory.held_item.id==87 && s->game.inventory.held_item.count==1
            && !s->game.inventory.held_item.modifier && carry.phase==1 && carry.sound==7 && p.sprite.animation==11);
        s->game.inventory.held_item=(HmbtnStoredTool){0};carry.phase=0;
        CHECK(!hmbtn_farm_gather_take(&s->gather,1,2,&s->house,&carry,&p));
        CHECK(!memcmp(s->game.inventory.items,before.items,sizeof(before.items))); /* Full bag permits pickup. */
    }
    CHECK(hmbtn_estate_item_art(87)==18 && hmbtn_farm_item(87)->sell_price==50);
    CHECK(!hmbtn_farm_gather_item(&s->gather,0,1,2) && !hmbtn_farm_gather_item(&s->gather,4,2,2)
        && !hmbtn_farm_gather_item(&s->gather,197,1,2) && !hmbtn_farm_gather_item(NULL,1,1,2));
    uint8_t *data=NULL;size_t size=0;CHECK(hmbtn_save_encode(s,&data,&size,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,size,error,sizeof(error));CHECK(loaded);
    for(unsigned i=1;i<=3;++i) CHECK(loaded->gather.taken[i] && !hmbtn_farm_gather_item(&loaded->gather,i,1,2));
    free(data);hmbtn_session_destroy(loaded);
    hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);CHECK(s->gather.taken[1]);
    hmbtn_session_advance_minutes(s,1440);
    for(unsigned i=1;i<=3;++i) CHECK(!s->gather.taken[i] && hmbtn_farm_gather_item(&s->gather,i,1,2)==87);
    s->game.clock.day=29;s->daily_date=hmbtn_session_date(&s->game.clock);s->gather.taken[1]=true;
    hmbtn_session_advance_minutes(s,1440);CHECK(s->game.clock.season==3 && !s->gather.taken[1]);
    CHECK(!hmbtn_farm_gather_item(&s->gather,1,1,s->game.clock.season));
    hmbtn_session_advance_minutes(s,90u*1440);CHECK(s->game.clock.season==2 && s->game.clock.day==0);
    CHECK(hmbtn_farm_gather_item(&s->gather,1,1,2)==87);
    hmbtn_session_destroy(s);
    puts("Original forage and farm apples: three fall records, location/height/hands/full bag, unique pickup, persistence, daily idempotence, seasonal return and art/price passed.");return 0;
}
