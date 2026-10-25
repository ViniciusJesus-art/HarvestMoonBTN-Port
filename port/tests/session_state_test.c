#include "session.h"
#include "farm_menu.h"
#include "inventory_checks.h"
#include "economy_checks.h"
#include "calendar_checks.h"
#include <stdio.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Session state line %d: %s\n",__LINE__,#x);goto fail; } } while(0)
static bool game_equal(const HmbtnGameState *a,const HmbtnGameState *b)
{
    return inventory_equal(&a->inventory,&b->inventory) && economy_equal(&a->economy,&b->economy)
        && clock_equal(&a->clock,&b->clock) && registration_equal(&a->registration,&b->registration);
}
int main(void)
{
    uint16_t layout[2]={0x168,0x169};
    HmbtnSession *s=hmbtn_session_create(layout,2),*other=hmbtn_session_create(NULL,0);
    CHECK(s && other && s!=other);
    CHECK(s->field.cells[0].value==0x168 && s->field.cells[1].value==0x169);
    layout[0]=0;CHECK(s->field.cells[0].value==0x168); /* No retained layout pointer. */
    CHECK(other->field.cells[0].value==0x167 && !hmbtn_poultry_count(&other->poultry));
    CHECK(!s->village_ready && !s->village_initialized && !s->village.schedule_data);
    uint8_t first[64]={0},replacement[64]={0};
    CHECK(!hmbtn_session_bind_schedules(NULL,first,sizeof(first)));
    CHECK(!hmbtn_session_bind_schedules(s,NULL,0) && !s->village_initialized);
    CHECK(hmbtn_session_bind_schedules(s,first,sizeof(first)));
    CHECK(s->village_ready && s->village_initialized && s->village.schedule_data==first);
    CHECK(s->village.people[18].friendship==50 && s->village.date_key==UINT32_MAX);
    s->game.inventory.bag[7]=(HmbtnStoredTool){21,0,73};s->game.inventory.supplies[1]=42;
    s->game.economy.gold=4321;s->game.economy.ledger[2][1]=91;s->game.economy.shipping.pending_value=60;
    s->game.clock=(HmbtnFarmClock){2,1,14,3,12,59,7};s->game.registration.farm_name[0]=0x20;
    s->field.cells[400]=(HmbtnFarmTile){0x169,0x168,2,0x1234};
    s->field.season=1;
    s->village.people[18].friendship=210;s->village.people[3].affection=3210;
    s->village.people[18].actor.position[0]=123;s->village.people[18].delay=17;
    s->village.people[18].schedule[2][3]=45;s->village.days.values[127]=53;s->village.flags[381]=true;
    s->village.random=77;s->village.date_key=65;
    const uint16_t name[9]={0,1,2,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
    CHECK(hmbtn_poultry_buy(&s->poultry,name,&s->game.clock));
    HmbtnHen *hen=&s->poultry.hens[0];hen->affection=133;hen->days_sick=1;
    s->poultry.random=19;s->poultry.eggs[0]=(HmbtnCoopEgg){0x122,{10,0,20},112};
    const HmbtnGameState expected=s->game;
    /* Two replacement sets of real UI controllers borrow the same domain. */
    HmbtnHouseInteraction house,second;HmbtnFarmMenu menu;
    hmbtn_house_interaction_init(&house,&s->game);hmbtn_house_interaction_init(&second,&s->game);
    hmbtn_farm_menu_init(&menu,hmbtn_game_economy_const(&s->game));menu.poultry=&s->poultry;
    CHECK(house.inventory==second.inventory && house.inventory==&s->game.inventory);
    CHECK(house.clock==&s->game.clock && house.registration==&s->game.registration);
    CHECK(menu.economy==&s->game.economy && menu.poultry->hens==s->poultry.hens);
    hmbtn_house_interaction_init(&house,&s->game);
    hmbtn_farm_menu_init(&menu,hmbtn_game_economy_const(&s->game));menu.poultry=&s->poultry;
    CHECK(game_equal(&s->game,&expected) && &s->poultry.hens[0]==hen);
    /* Rebinding identical resource storage must never reinitialize villagers. */
    HmbtnIntroActor host_player={0},event_actor={0};
    s->village.player=&host_player;s->village.event_actor=&event_actor;s->village.event_actor_id=17;
    s->village.event_actors[9]=&event_actor;
    hmbtn_session_unbind_resources(s);
    CHECK(!s->village_ready && s->village_initialized && !s->village.schedule_data);
    CHECK(!s->village.player && !s->village.event_actor && !s->village.event_actors[9]);
    first[0]=0xaa; /* Storage is caller-owned and can now be retired. */
    CHECK(hmbtn_session_bind_schedules(s,replacement,sizeof(replacement)));
    CHECK(s->village_ready && s->village.schedule_data==replacement);
    CHECK(s->village.people[18].friendship==210 && s->village.people[3].affection==3210);
    CHECK(s->village.people[18].actor.position[0]==123 && s->village.people[18].delay==17);
    CHECK(s->village.people[18].schedule[2][3]==45 && s->village.days.values[127]==53 && s->village.flags[381]);
    CHECK(s->village.random==77 && s->village.date_key==65);
    CHECK(s->field.cells[400].value==0x169 && s->field.cells[400].ground==0x168
        && s->field.cells[400].kind==2 && s->field.cells[400].growth_flags==0x1234 && s->field.season==1);
    CHECK(hmbtn_poultry_count(&s->poultry)==1 && hen->affection==133 && hen->days_sick==1
        && hen->name[2]==2 && s->poultry.random==19 && s->poultry.eggs[0].item==0x122);
    CHECK(game_equal(&s->game,&expected));
    CHECK(other->game.economy.gold==500 && !other->village_initialized && !hmbtn_poultry_count(&other->poultry));
    hmbtn_session_destroy(other);other=NULL;
    CHECK(hen->affection==133 && game_equal(&s->game,&expected));
    hmbtn_session_destroy(s);s=NULL;
    CHECK(replacement[0]==0 && first[0]==0xaa); /* Session did not free borrowed buffers. */
    s=hmbtn_session_create(NULL,0);CHECK(s && s->game.economy.gold==500 && !s->village_initialized);
    hmbtn_session_destroy(s);hmbtn_session_destroy(NULL);hmbtn_session_unbind_resources(NULL);
    puts("Headless session lifecycle, independent worlds, UI recreation, owned layout values and schedule loans passed.");return 0;
fail:
    hmbtn_session_destroy(other);hmbtn_session_destroy(s);return 1;
}
