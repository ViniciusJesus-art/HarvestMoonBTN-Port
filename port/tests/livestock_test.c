#include "save_fixture.h"
#include "livestock.h"
#include <stdio.h>
#include <stdlib.h>
static const uint16_t name[9]={0,1,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
static void input(HmbtnSession *s,uint32_t pad)
{
    HmbtnFeedShop *shop=&s->feed_shop;shop->loaded=shop->request;shop->message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_feed_shop_tick(shop,&s->house,&s->game.economy,&s->menu.stats,pad,pad);
}
static void until(HmbtnSession *s,HmbtnFeedShopPhase phase)
{
    for(unsigned i=0;i<16 && (s->feed_shop.phase!=phase || s->feed_shop.closing);++i) {
        s->feed_shop.loaded=s->feed_shop.request;s->feed_shop.message.phase=HMBTN_MESSAGE_CLOSED;
        hmbtn_feed_shop_tick(&s->feed_shop,&s->house,&s->game.economy,&s->menu.stats,0,0);
    }
    assert(s->feed_shop.phase==phase && !s->feed_shop.closing);
}
int main(void)
{
    char error[256]={0};HmbtnSession *s=save_fixture(0);HmbtnLivestock *p=&s->livestock;
    assert(p->construction==&s->construction && s->menu.livestock==p && s->feed_shop.livestock==p);
    assert(hmbtn_livestock_capacity(p)==10);
    for(unsigned i=0;i<10;++i) assert(hmbtn_livestock_buy(p,i&1 ? 0x161 : 0x165,name,&s->game.clock));
    assert(hmbtn_livestock_count(p,false)==5 && hmbtn_livestock_count(p,true)==5);
    assert(!hmbtn_livestock_buy(p,0x161,name,&s->game.clock));
    s->poultry.upgraded=true;s->construction.house_level=1;s->construction.barn_level=1;
    for(unsigned i=10;i<20;++i) assert(hmbtn_livestock_buy(p,0x161,name,&s->game.clock));
    assert(hmbtn_livestock_free(p)==20 && !hmbtn_livestock_buy(p,0x165,name,&s->game.clock));
    assert(hmbtn_farm_menu_animal_count(&s->menu,1)==15 && hmbtn_farm_menu_animal_count(&s->menu,2)==5);
    assert(p->animals[0].growth==14 && p->animals[0].affection==10 && p->animals[0].actor.position[0]==1980);
    assert(p->animals[19].actor.position[0]==-1980 && p->animals[19].actor.position[2]==1132);
    uint8_t *data=NULL;size_t bytes=0;assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(loaded);free(data);
    assert(loaded->livestock.construction==&loaded->construction && loaded->feed_shop.livestock==&loaded->livestock
        && loaded->menu.livestock==&loaded->livestock && hmbtn_livestock_count(&loaded->livestock,false)==15);
    hmbtn_session_destroy(loaded);
    p->animals[0].growth=32;assert(!hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));p->animals[0].growth=14;
    s->construction.barn_level=0;assert(!hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));s->construction.barn_level=1;
    assert(hmbtn_livestock_count(p,false)==15 && hmbtn_livestock_count(p,true)==5);
    /* Cow-before-sheep feed order follows the original two daily passes. */
    memset(p->animals,0,sizeof(p->animals));memset(s->feed.barn,0,sizeof(s->feed.barn));
    assert(hmbtn_livestock_buy(p,0x165,name,&s->game.clock) && hmbtn_livestock_buy(p,0x161,name,&s->game.clock));
    assert(hmbtn_livestock_talk(p,0)==0x6bd && hmbtn_livestock_talk(p,0)==0x6bd);
    s->feed.barn[0]=true;hmbtn_session_advance_minutes(s,1440);
    assert(p->animals[1].fed_yesterday && p->animals[1].affection==11);
    assert(!p->animals[0].fed_yesterday && p->animals[0].affection==3 && !p->animals[0].talked_today);
    assert(!s->feed.barn[0] && p->animals[0].growth==15);
    const unsigned growth=p->animals[0].growth;
    hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);assert(p->animals[0].growth==growth);
    p->animals[1].growth=27;s->feed.barn[19]=true;
    hmbtn_livestock_new_day(p,&s->feed,&s->game.clock);
    assert(p->animals[1].growth==28 && hmbtn_livestock_kind(&p->animals[1])==0x162 && !s->feed.barn[19]);
    /* Medicine uses the existing player action and consumes a single dose. */
    HmbtnFarmAnimal *cow=&p->animals[1];cow->days_sick=1;s->game.inventory.held_item=(HmbtnStoredTool){0};
    s->game.inventory.equipped=(HmbtnStoredTool){0x1e,0,2};s->intro.player.facing=3;
    memcpy(s->intro.player.position,cow->actor.position,sizeof(cow->actor.position));s->intro.player.position[0]-=128;
    assert(hmbtn_animal_medicine_tick(&s->poultry,p,96,&s->farm_action,&s->intro.player,&s->house,HMBTN_PAD_SQUARE));
    assert(cow->days_sick==1 && s->game.inventory.equipped.count==2);
    s->intro.player.animation_done=true;
    assert(hmbtn_animal_medicine_tick(&s->poultry,p,96,&s->farm_action,&s->intro.player,&s->house,0));
    assert(!cow->days_sick && s->game.inventory.equipped.count==1 && s->farm_action.changed);
    cow->days_sick=31;hmbtn_livestock_new_day(p,&s->feed,&s->game.clock);assert(cow->dead && !cow->actor.visible);
    assert(hmbtn_livestock_free(p)==2); /* Death is not a silent sale/removal. */
    /* Original Barley purchase checks and shared editor; no fake animal on cancel. */
    memset(p->animals,0,sizeof(p->animals));s->area=136;s->game.clock.hour=12;s->game.clock.weekday=3;
    s->intro.player=(HmbtnIntroActor){0};s->village.people[15].actor=(HmbtnIntroActor){.visible=true,.position={0,0,-224}};
    s->village.people[15].location=9;s->game.economy.gold=5999;
    assert(hmbtn_feed_shop_open(&s->feed_shop,&s->village,&s->intro.player,&s->game.clock,136,0));
    until(s,HMBTN_SHOP_SELECT);assert(s->feed_shop.request==0x46);
    input(s,HMBTN_PAD_DOWN);assert(s->feed_shop.selection==4 && hmbtn_feed_shop_cost(&s->feed_shop)==6000);
    input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x4f && !hmbtn_livestock_count(p,false));
    until(s,HMBTN_SHOP_SELECT);s->game.economy.gold=10000;input(s,HMBTN_PAD_CROSS);
    until(s,HMBTN_SHOP_NAME);s->feed_shop.editor.finished=true;s->feed_shop.editor.choice=1;
    s->feed_shop.editor.message.phase=HMBTN_MESSAGE_CLOSED;input(s,0);
    until(s,HMBTN_SHOP_SELECT);assert(s->game.economy.gold==10000 && !hmbtn_livestock_count(p,false));
    input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_NAME);
    memcpy(s->feed_shop.editor.name,name,sizeof(name));s->feed_shop.editor.finished=true;
    s->feed_shop.editor.message.phase=HMBTN_MESSAGE_CLOSED;input(s,0);
    assert(s->feed_shop.request==0x74 && s->game.economy.gold==4000 && hmbtn_livestock_count(p,false)==1);
    input(s,0);assert(s->game.economy.gold==4000 && hmbtn_livestock_count(p,false)==1);
    until(s,HMBTN_SHOP_SELECT);input(s,HMBTN_PAD_DOWN);assert(s->feed_shop.selection==5 && hmbtn_feed_shop_cost(&s->feed_shop)==4000);
    hmbtn_session_destroy(s);
    puts("Livestock: shared capacity/species, source placement/growth/food order, daily idempotence, medicine, live status, save owners and Barley atomic purchase/cancel passed.");return 0;
}
