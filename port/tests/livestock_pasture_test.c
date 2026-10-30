/* Source/BIN vectors: hourly grazing, sheep's weather branch, Barley bell,
 * delayed push, source goals, outdoor persistence and maternity placement. */
#include "save_fixture.h"
#include "inventory.h"
#include <stdio.h>
#include <stdlib.h>
static const uint16_t name[9]={0x100,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
static void input(HmbtnSession *s,uint32_t pad)
{
    HmbtnFeedShop *shop=&s->feed_shop;shop->loaded=shop->request;shop->message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_feed_shop_tick(shop,&s->house,&s->game.economy,&s->menu.stats,pad,pad);
}
static void until(HmbtnSession *s,HmbtnFeedShopPhase phase)
{
    for(unsigned i=0;i<20 && (s->feed_shop.phase!=phase || s->feed_shop.closing);++i) {
        s->feed_shop.loaded=s->feed_shop.request;s->feed_shop.message.phase=HMBTN_MESSAGE_CLOSED;
        hmbtn_feed_shop_tick(&s->feed_shop,&s->house,&s->game.economy,&s->menu.stats,0,0);
    }
    assert(s->feed_shop.phase==phase && !s->feed_shop.closing);
}
int main(void)
{
    HmbtnSession *s=save_fixture(0);HmbtnLivestock *p=&s->livestock;
    assert(hmbtn_livestock_buy(p,0x161,name,&s->game.clock) && hmbtn_livestock_buy(p,0x165,name,&s->game.clock));
    HmbtnFarmAnimal *cow=&p->animals[0],*sheep=&p->animals[1];cow->area=sheep->area=0;
    cow->affection=sheep->affection=100;hmbtn_farm_field_clear(&s->field);
    hmbtn_farm_field_position(&s->field,10,10,cow->actor.position);
    memcpy(sheep->actor.position,cow->actor.position,sizeof(cow->actor.position));
    HmbtnFarmTile *near=&s->field.cells[10*45+10],*far=&s->field.cells[24*45+44];
    *near=(HmbtnFarmTile){.value=0x36,.ground=0x168,.kind=3};*far=*near;
    const unsigned stock=s->game.inventory.supplies[0];
    for(unsigned hour=0;hour<4;++hour) hmbtn_livestock_hour(p,&s->field,&s->game.clock,0);
    assert(!cow->fed_today && !sheep->fed_today && near->kind==3 && far->kind==3);
    hmbtn_livestock_hour(p,&s->field,&s->game.clock,0);
    assert(cow->fed_today && sheep->fed_today && cow->graze_count==5 && sheep->graze_count==5);
    assert(near->kind==1 && far->kind==1 && (near->growth_flags&0x8000) && !near->watered_days);
    assert(s->game.inventory.supplies[0]==stock); /* Consuming pasture does not produce silo fodder. */
    near->kind=3;hmbtn_livestock_hour(p,&s->field,&s->game.clock,0);assert(near->kind==3);
    cow->fed_today=sheep->fed_today=false;near->kind=2;
    hmbtn_livestock_hour(p,&s->field,&s->game.clock,0);assert(!cow->fed_today && !sheep->fed_today);
    near->value=0x22;near->kind=3;
    hmbtn_livestock_hour(p,&s->field,&s->game.clock,0);assert(!cow->fed_today && !sheep->fed_today);
    for(unsigned weather=1;weather<=3;++weather) {
        const unsigned ca=cow->affection,sa=sheep->affection,cg=cow->graze_count,sg=sheep->graze_count;
        hmbtn_livestock_hour(p,&s->field,&s->game.clock,weather);
        assert(cow->affection==ca-5 && cow->graze_count==cg);
        assert(sheep->affection==sa-(weather==1 ? 5u : 0u) && sheep->graze_count==sg+(weather!=1));
    }
    s->game.clock.season=3;const unsigned cg=cow->graze_count,ca=cow->affection;
    hmbtn_livestock_hour(p,&s->field,&s->game.clock,1);assert(cow->graze_count==cg && cow->affection==ca);
    s->game.clock.season=0;cow->area=96;
    hmbtn_livestock_hour(p,&s->field,&s->game.clock,1);assert(cow->graze_count==cg && cow->affection==ca);
    cow->area=0;cow->affection=2;hmbtn_livestock_hour(p,&s->field,&s->game.clock,1);assert(!cow->affection);
    /* Bell addresses all live same-map animals with affection/200 chance. */
    cow->affection=200;sheep->affection=0;s->intro.player.position[0]=1000;
    hmbtn_livestock_bell(p,0,&s->intro.player);assert(cow->state==4 && sheep->state!=4 && cow->destination[0]==1000);
    cow->state=1;cow->area=96;hmbtn_livestock_bell(p,0,&s->intro.player);assert(cow->state==1);
    cow->area=0;cow->dead=true;hmbtn_livestock_bell(p,0,&s->intro.player);assert(cow->state==1);cow->dead=false;
    s->intro.player.facing=3;memcpy(s->intro.player.position,cow->actor.position,sizeof(cow->actor.position));
    s->intro.player.position[0]-=128;
    for(unsigned i=0;i<51;++i) hmbtn_livestock_push(p,0,&s->intro.player,true);
    assert(cow->state==1);hmbtn_livestock_push(p,0,&s->intro.player,true);
    assert(cow->state==5 && cow->destination[0]==cow->actor.position[0]+800);
    hmbtn_livestock_push(p,0,&s->intro.player,false);assert(!p->push_frames);
    /* State needed to resume a cow in the field is encoded in the same record. */
    char error[256]={0};uint8_t *data=NULL;size_t bytes=0;
    assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(loaded);free(data);
    assert(loaded->livestock.animals[0].area==0 && loaded->livestock.animals[0].state==5
        && loaded->livestock.animals[0].graze_count==cow->graze_count
        && !memcmp(loaded->livestock.animals[0].destination,cow->destination,sizeof(cow->destination)));
    hmbtn_session_destroy(loaded);
    /* The shared clock still feeds the off-screen herd while the player is
     * in another area; rendering and current-map movement are irrelevant. */
    cow->fed_today=sheep->fed_today=false;s->game.weather.today=0;
    *near=(HmbtnFarmTile){.value=0x36,.ground=0x168,.kind=3};*far=*near;
    assert(s->area==80);hmbtn_session_advance_minutes(s,60);
    assert(cow->fed_today && sheep->fed_today && near->kind==1 && far->kind==1);
    /* Outdoor animals never eat barn fodder; the graze bonus is separate. */
    cow->affection=100;cow->fed_today=false;cow->graze_count=5;
    sheep->fed_today=true;sheep->graze_count=5;s->feed.barn[0]=true;
    hmbtn_livestock_new_day(p,&s->feed,&s->game.clock);
    assert(!cow->fed_yesterday && cow->affection==91 && !cow->graze_count && s->feed.barn[0]);
    assert(sheep->fed_yesterday && !sheep->graze_count);
    cow->growth=28;assert(hmbtn_livestock_breed(p,0,false));
    hmbtn_livestock_new_day(p,&s->feed,&s->game.clock);assert(cow->area==96 && cow->actor.position[0]==2400);
    /* Actual shop transaction: funds and space checked before charging. */
    memset(&s->game.inventory,0,sizeof(s->game.inventory));s->game.inventory.rucksack_level=2;
    s->area=136;s->game.clock.hour=12;s->game.clock.weekday=3;s->intro.player=(HmbtnIntroActor){0};
    s->village.people[15].actor=(HmbtnIntroActor){.visible=true,.position={0,0,-224}};s->village.people[15].location=9;
    assert(hmbtn_feed_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,136,0));until(s,HMBTN_SHOP_SELECT);
    assert(hmbtn_barn_shop_rows(&s->feed_shop)==9);input(s,HMBTN_PAD_UP);assert(s->feed_shop.selection==10 && s->feed_shop.item==0x1d);
    input(s,HMBTN_PAD_TRIANGLE);assert(s->feed_shop.next_request==0x281);until(s,HMBTN_SHOP_SELECT);
    s->game.economy.gold=499;input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x4f);until(s,HMBTN_SHOP_SELECT);
    s->game.economy.gold=500;s->game.inventory.equipped=(HmbtnStoredTool){1,0,1};
    for(unsigned i=0;i<8;++i) s->game.inventory.bag[i]=(HmbtnStoredTool){1,0,1};
    input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x54 && s->game.economy.gold==500);until(s,HMBTN_SHOP_SELECT);
    s->game.inventory.bag[0]=(HmbtnStoredTool){0};input(s,HMBTN_PAD_CROSS);
    assert(s->feed_shop.next_request==0x84 && !s->game.economy.gold && hmbtn_inventory_count(&s->game.inventory,0x1d,0,2)==1);
    until(s,HMBTN_SHOP_SELECT);assert(hmbtn_barn_shop_rows(&s->feed_shop)==8 && !s->feed_shop.bell_available);
    s->game.inventory.equipped=(HmbtnStoredTool){0x1d,0,1};s->game.inventory.bag[0]=(HmbtnStoredTool){0};
    s->farm_action.phase=HMBTN_FARM_ACTION_FREE;s->farm_action.fatigue=0;s->farm_action.stamina=50;
    cow->area=96;cow->affection=200;cow->state=1;
    assert(hmbtn_livestock_tool_tick(p,96,&s->farm_action,&s->intro.player,&s->house,&s->event_item,HMBTN_PAD_SQUARE));
    assert(s->farm_action.bank==21 && s->farm_action.sound==13 && cow->state==1);
    s->intro.player.animation_done=true;
    assert(hmbtn_livestock_tool_tick(p,96,&s->farm_action,&s->intro.player,&s->house,&s->event_item,0));
    assert(cow->state==4 && s->farm_action.stamina==50 && s->game.inventory.equipped.count==1);
    hmbtn_session_destroy(s);
    puts("Pasture: consumed grass per animal/hour, original weather branches, winter/barn/no-grass guards, push delay/goals, bell purchase/action/chance, outdoor persistence and maternity passed.");return 0;
}
