#include "save_fixture.h"
#include "event_variables.h"
#include "world_map.h"
#include <stdio.h>
#include <stdlib.h>
static void input(HmbtnSession *s,uint32_t pad)
{
    HmbtnFeedShop *shop=&s->feed_shop;shop->loaded=shop->request;shop->message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_feed_shop_tick(shop,&s->house,&s->game.economy,&s->menu.stats,pad,pad);
}
static void until(HmbtnSession *s,HmbtnFeedShopPhase phase)
{
    HmbtnFeedShop *shop=&s->feed_shop;
    for(unsigned i=0;i<12 && (shop->phase!=phase || shop->closing);++i) {
        shop->loaded=shop->request;shop->message.phase=HMBTN_MESSAGE_CLOSED;
        hmbtn_feed_shop_tick(shop,&s->house,&s->game.economy,&s->menu.stats,0,0);
    }
    assert(shop->phase==phase && !shop->closing);
}
static void open(HmbtnSession *s)
{
    assert(hmbtn_carpenter_shop_open(&s->feed_shop,&s->game.clock));
    assert(s->feed_shop.request==0x132 && s->feed_shop.merchant==24 && s->feed_shop.item==0x3f);
    assert(!hmbtn_carpenter_shop_open(&s->feed_shop,&s->game.clock));
    until(s,HMBTN_SHOP_SELECT);assert(s->feed_shop.request==0x139);
}
static void confirm(HmbtnSession *s)
{
    input(s,HMBTN_PAD_DOWN);input(s,HMBTN_PAD_CROSS);
    assert(s->feed_shop.next_request==0x13c);until(s,HMBTN_SHOP_BUILD_CONFIRM);
    assert(s->feed_shop.request==0x5f);
}
int main(void)
{
    char error[256]={0};HmbtnSession *s=save_fixture(1);
    HmbtnFeedShop *shop=&s->feed_shop;HmbtnInventory *inv=&s->game.inventory;
    const HmbtnConstructionOffer *offer=hmbtn_construction_offer(0);
    assert(offer->gold==5000 && offer->lumber==420 && hmbtn_construction_mornings()==4);
    s->game.economy.gold=20000;inv->supplies[1]=400;
    open(s);assert(hmbtn_carpenter_shop_rows(shop)==2);
    input(s,HMBTN_PAD_TRIANGLE);assert(shop->next_request==0x133);until(s,HMBTN_SHOP_SELECT);
    input(s,HMBTN_PAD_CROSS);assert(shop->next_request==0x13b);until(s,HMBTN_SHOP_COUNT);
    input(s,HMBTN_PAD_LEFT);assert(shop->quantity==99);input(s,HMBTN_PAD_RIGHT);assert(shop->quantity==1);
    input(s,HMBTN_PAD_UP);assert(shop->quantity==11);input(s,HMBTN_PAD_UP);assert(shop->quantity==21);
    input(s,HMBTN_PAD_TRIANGLE);assert(shop->next_request==0x133);until(s,HMBTN_SHOP_COUNT);
    input(s,HMBTN_PAD_CIRCLE);until(s,HMBTN_SHOP_SELECT);
    assert(inv->supplies[1]==400 && s->game.economy.gold==20000);
    input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_COUNT);
    input(s,HMBTN_PAD_UP);input(s,HMBTN_PAD_UP);input(s,HMBTN_PAD_LEFT);assert(shop->quantity==20);
    const unsigned cost=hmbtn_feed_shop_cost(shop);assert(cost==1000);
    input(s,HMBTN_PAD_CROSS);assert(shop->next_request==0x143 && inv->supplies[1]==420 && s->game.economy.gold==19000);
    until(s,HMBTN_SHOP_OFF);
    open(s);confirm(s);input(s,HMBTN_PAD_CIRCLE);until(s,HMBTN_SHOP_SELECT);
    assert(!s->construction.mornings_left && inv->supplies[1]==420);
    /* Exact failure ordering and no partial debit. */
    input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_BUILD_CONFIRM);s->game.economy.gold=4999;
    input(s,HMBTN_PAD_CROSS);assert(shop->next_request==0x4f && inv->supplies[1]==420 && !s->construction.mornings_left);
    until(s,HMBTN_SHOP_SELECT);input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_BUILD_CONFIRM);
    s->game.economy.gold=5000;inv->supplies[1]=419;input(s,HMBTN_PAD_CROSS);
    assert(shop->next_request==0x145 && s->game.economy.gold==5000 && inv->supplies[1]==419);
    until(s,HMBTN_SHOP_SELECT);input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_BUILD_CONFIRM);
    inv->supplies[1]=420;input(s,HMBTN_PAD_CROSS);
    assert(shop->next_request==0x144 && s->construction.mornings_left==4 && !s->poultry.upgraded && !inv->supplies[1] && !s->game.economy.gold);
    until(s,HMBTN_SHOP_OFF);
    assert(shop->construction==&s->construction && hmbtn_village_flag(&s->village,0x38) && !hmbtn_village_flag(&s->village,0x3a));
    assert(!s->village.flags[0x38] && !s->village.flags[0x3a]);
    HmbtnEventVariables vars={.village=&s->village};int32_t result=0;
    s->village.people[24].schedule[0][1]=0x600;s->village.people[24].schedule[0][2]=51;
    s->village.people[24].step=0;
    assert(hmbtn_event_variable_get(&vars,38,24,&result) && result==6);
    assert(hmbtn_event_variable_get(&vars,48+0x38,0,&result) && result==1);
    assert(!hmbtn_event_variable_update(&vars,48+0x38,0,0,0));
    open(s);input(s,HMBTN_PAD_DOWN);input(s,HMBTN_PAD_CROSS);
    assert(shop->next_request==0x144 && !s->game.economy.gold && !inv->supplies[1]);
    until(s,HMBTN_SHOP_SELECT);input(s,HMBTN_PAD_CIRCLE);until(s,HMBTN_SHOP_OFF);
    /* Pending work survives loading; day notifications are idempotent. */
    uint8_t *data=NULL;size_t bytes=0;
    assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(loaded);free(data);
    hmbtn_session_destroy(s);s=loaded;shop=&s->feed_shop;inv=&s->game.inventory;
    static const uint8_t schedules[1]={0};assert(hmbtn_session_bind_schedules(s,schedules,1));
    assert(s->construction.mornings_left==4 && s->village.construction==&s->construction && s->village.poultry==&s->poultry);
    for(unsigned day=0;day<4;++day) {
        hmbtn_session_advance_minutes(s,1440);assert(s->construction.mornings_left==3-day && s->poultry.upgraded==(day==3));
        hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);
        assert(s->construction.mornings_left==3-day);
    }
    assert(hmbtn_farm_menu_building_score(&s->menu)==20);
    assert(!hmbtn_village_flag(&s->village,0x38) && hmbtn_village_flag(&s->village,0x3a));
    assert(hmbtn_construction_next(&s->construction,&s->poultry)==1);
    assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(loaded && loaded->poultry.upgraded && !loaded->construction.mornings_left);
    free(data);hmbtn_session_destroy(loaded);
    for(unsigned season=0;season<4;++season) {
        unsigned logical,resource;
        assert(hmbtn_world_area_coop(0,season,true,&logical,&resource) && !logical && resource==4+season);
        assert(hmbtn_world_area_coop(112,season,true,&logical,&resource) && logical==112 && resource==116+season);
        assert(hmbtn_world_area_coop(80,season,true,&logical,&resource) && logical==80 && resource==80+season);
    }
    /* The same poultry algorithms now allow ten hens and the second incubator. */
    const uint16_t name[9]={0x100,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
    memset(s->poultry.hens,0,sizeof(s->poultry.hens));
    for(unsigned i=0;i<10;++i) assert(hmbtn_poultry_buy(&s->poultry,name,&s->game.clock));
    assert(!hmbtn_poultry_buy(&s->poultry,name,&s->game.clock));
    s->construction.house_level=1;s->construction.barn_level=1;open(s);assert(hmbtn_carpenter_shop_rows(shop)==1);input(s,HMBTN_PAD_DOWN);assert(!shop->selection);
    inv->supplies[1]=999;input(s,HMBTN_PAD_CROSS);assert(shop->next_request==0x13a);until(s,HMBTN_SHOP_SELECT);
    inv->supplies[1]=998;s->game.economy.gold=1000;
    input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_COUNT);input(s,HMBTN_PAD_RIGHT);input(s,HMBTN_PAD_CROSS);
    assert(shop->next_request==0x142 && inv->supplies[1]==998 && s->game.economy.gold==1000);until(s,HMBTN_SHOP_COUNT);
    s->game.economy.gold=49;shop->quantity=1;input(s,HMBTN_PAD_CROSS);
    assert(shop->next_request==0x4f && inv->supplies[1]==998);until(s,HMBTN_SHOP_COUNT);
    s->game.economy.gold=50;input(s,HMBTN_PAD_CROSS);
    assert(shop->next_request==0x143 && inv->supplies[1]==999 && !s->game.economy.gold);until(s,HMBTN_SHOP_OFF);
    hmbtn_session_destroy(s);
    puts("Gotz: shared wood/purse, original costs/dialogues, cancel/failure/busy, four mornings, saves, derived flags and upgraded resources/capacity passed.");return 0;
}
