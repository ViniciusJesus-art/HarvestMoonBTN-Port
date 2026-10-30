/* Vectors audited in shop_stock_step, livestock_cow_cue,
 * livestock_sheep_harvest, livestock_set_bit2_cue/_b and player.c. */
#include "save_fixture.h"
#include "inventory.h"
#include "farm_shipping.h"
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
static HmbtnSession *reload(HmbtnSession *s)
{
    char error[256]={0};uint8_t *data=NULL;size_t bytes=0;
    assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *next=hmbtn_save_decode(data,bytes,error,sizeof(error));
    if(!next) fprintf(stderr,"%s\n",error);
    assert(next);free(data);hmbtn_session_destroy(s);
    static const uint8_t schedules[1]={0};assert(hmbtn_session_bind_schedules(next,schedules,1));return next;
}
int main(void)
{
    HmbtnSession *s=save_fixture(0);
    memset(&s->game.inventory,0,sizeof(s->game.inventory));s->game.inventory.rucksack_level=2;
    s->game.economy.gold=10000;
    assert(hmbtn_blacksmith_shop_open(&s->feed_shop,&s->game.clock,&s->game.inventory));
    assert(!hmbtn_blacksmith_shop_open(&s->feed_shop,&s->game.clock,&s->game.inventory));
    const unsigned ids[]={0x1c,0x1a,0x1b},prices[]={800,2000,1800};
    for(unsigned i=0;i<3;++i) {
        const HmbtnSeedShopEntry *entry=hmbtn_blacksmith_tool_entry(&s->feed_shop,i);
        assert(entry && entry->item==ids[i] && entry->description==0xaa+i && entry->name[entry->name_words-1]==0xffff);
    }
    assert(hmbtn_blacksmith_tool_count(&s->feed_shop)==3 && !hmbtn_blacksmith_tool_entry(&s->feed_shop,3));
    until(s,HMBTN_SHOP_SELECT);input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x87);
    until(s,HMBTN_SHOP_SELECT);input(s,HMBTN_PAD_DOWN);input(s,HMBTN_PAD_DOWN);input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_TOOL_LIST);
    input(s,HMBTN_PAD_TRIANGLE);assert(s->feed_shop.next_request==0xaa);until(s,HMBTN_SHOP_TOOL_LIST);
    input(s,HMBTN_PAD_CIRCLE);until(s,HMBTN_SHOP_SELECT);assert(s->game.economy.gold==10000);
    input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_TOOL_LIST);
    s->game.inventory.equipped=(HmbtnStoredTool){6,0,1};
    for(unsigned i=0;i<8;++i) s->game.inventory.bag[i]=(HmbtnStoredTool){6,0,1};
    s->game.economy.gold=0;input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x54);
    until(s,HMBTN_SHOP_TOOL_LIST);s->game.inventory.bag[7]=(HmbtnStoredTool){0};
    input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x4f);
    until(s,HMBTN_SHOP_TOOL_LIST);
    memset(s->game.inventory.bag,0,sizeof(s->game.inventory.bag));s->game.inventory.equipped=(HmbtnStoredTool){0};
    s->game.economy.gold=10000;
    for(unsigned i=0;i<3;++i) {
        assert(s->feed_shop.item==ids[i]);const unsigned gold=s->game.economy.gold;
        input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0xae && s->game.economy.gold==gold);
        until(s,HMBTN_SHOP_SELECT);
        assert(s->game.economy.gold==gold-prices[i] && hmbtn_inventory_count(&s->game.inventory,(int)ids[i],0,2)==1);
        assert(hmbtn_blacksmith_tool_count(&s->feed_shop)==2-i);
        input(s,HMBTN_PAD_CROSS);
        if(i<2) until(s,HMBTN_SHOP_TOOL_LIST);
        else { assert(s->feed_shop.next_request==0xad);until(s,HMBTN_SHOP_SELECT); }
    }
    input(s,HMBTN_PAD_CIRCLE);until(s,HMBTN_SHOP_OFF);
    s=reload(s);assert(s->game.economy.gold==5400);
    assert(hmbtn_blacksmith_shop_open(&s->feed_shop,&s->game.clock,&s->game.inventory)
        && !hmbtn_blacksmith_tool_count(&s->feed_shop));
    s->feed_shop.phase=HMBTN_SHOP_OFF;s->feed_shop.closing=false;
    assert(hmbtn_livestock_buy(&s->livestock,0x161,name,&s->game.clock));
    assert(hmbtn_livestock_buy(&s->livestock,0x165,name,&s->game.clock));
    HmbtnFarmAnimal *cow=&s->livestock.animals[0],*sheep=&s->livestock.animals[1];
    assert(!hmbtn_livestock_harvest(&s->livestock,0,false));cow->growth=28;
    const unsigned affection[]={0,1,2,100,101,200,201,255};
    const unsigned milk[]={0,0,0x8b,0x8b,0x8c,0x8c,0x8d,0x8d};
    for(unsigned i=0;i<8;++i) {
        cow->affection=affection[i];cow->fed_yesterday=true;cow->milked_today=false;
        assert(hmbtn_livestock_harvest(&s->livestock,0,false)==milk[i]);
        if(milk[i]) assert(!hmbtn_livestock_harvest(&s->livestock,0,false));
        sheep->affection=affection[i];sheep->days_since_shearing=7;sheep->fed_yesterday=false;
        assert(hmbtn_livestock_harvest(&s->livestock,1,true)==(i<4 ? 0x91u : i<6 ? 0x92u : 0x93u));
        assert(!hmbtn_livestock_harvest(&s->livestock,1,true) && hmbtn_livestock_pack(sheep)==8);
    }
    cow->milked_today=false;cow->fed_yesterday=false;
    assert(!hmbtn_livestock_harvest(&s->livestock,0,false) && cow->milked_today);
    cow->milked_today=false;cow->fed_yesterday=true;cow->prize_winner=true;cow->days_sick=1;
    assert(hmbtn_livestock_harvest(&s->livestock,0,false)==0xe3); /* No sickness guard in source product cue. */
    cow->days_sick=0;sheep->days_since_shearing=7;sheep->prize_winner=true;
    assert(hmbtn_livestock_harvest(&s->livestock,1,true)==0xe7);
    assert(!hmbtn_livestock_harvest(&s->livestock,0,true) && !hmbtn_livestock_harvest(&s->livestock,1,false));
    const unsigned love=cow->affection;
    assert(hmbtn_livestock_brush(&s->livestock,0) && !hmbtn_livestock_brush(&s->livestock,0) && cow->affection==love);
    s=reload(s);cow=&s->livestock.animals[0];sheep=&s->livestock.animals[1];
    assert(cow->brushed_today && cow->milked_today && cow->prize_winner && !sheep->days_since_shearing);
    sheep->fed_yesterday=false;s->feed.barn[0]=s->feed.barn[1]=true;
    hmbtn_livestock_new_day(&s->livestock,&s->feed,&s->game.clock);
    assert(!sheep->days_since_shearing && sheep->fed_yesterday && !cow->brushed_today && !cow->milked_today);
    for(unsigned day=0;day<7;++day) {
        ++s->game.clock.day;s->daily_date=hmbtn_session_date(&s->game.clock);
        s->feed.barn[0]=s->feed.barn[1]=true;
        hmbtn_livestock_new_day(&s->livestock,&s->feed,&s->game.clock);
        assert(sheep->days_since_shearing==day+1 && hmbtn_livestock_pack(sheep)==(day==6 ? 5u : 8u));
        s=reload(s);cow=&s->livestock.animals[0];sheep=&s->livestock.animals[1];
    }
    s->game.inventory.held_item=(HmbtnStoredTool){0};s->game.inventory.equipped=(HmbtnStoredTool){0x1a,0,1};
    memcpy(s->intro.player.position,cow->actor.position,sizeof(cow->actor.position));
    s->intro.player.position[0]-=128;s->intro.player.facing=3;
    const unsigned stamina=s->farm_action.stamina;
    assert(hmbtn_livestock_tool_tick(&s->livestock,96,&s->farm_action,&s->intro.player,&s->house,&s->event_item,HMBTN_PAD_SQUARE));
    assert(!cow->milked_today && s->farm_action.bank==18);
    assert(hmbtn_livestock_tool_tick(&s->livestock,96,&s->farm_action,&s->intro.player,&s->house,&s->event_item,0) && !cow->milked_today);
    s->intro.player.animation_done=true;
    assert(hmbtn_livestock_tool_tick(&s->livestock,96,&s->farm_action,&s->intro.player,&s->house,&s->event_item,0));
    assert(cow->milked_today && s->event_item.shown==0xe3 && !s->game.inventory.held_item.id && s->farm_action.stamina==stamina-1);
    s->intro.player.animation_done=true;hmbtn_event_item_tick(&s->event_item,&s->intro.player,&s->game.inventory,&s->farm_action);
    assert(s->game.inventory.held_item.id==0xe3);
    assert(!hmbtn_livestock_tool_tick(&s->livestock,96,&s->farm_action,&s->intro.player,&s->house,&s->event_item,HMBTN_PAD_SQUARE));
    assert(hmbtn_farm_shipping_add(&s->game.economy,s->game.inventory.held_item.id));
    cow->dead=true;assert(!hmbtn_livestock_brush(&s->livestock,0) && !hmbtn_livestock_harvest(&s->livestock,0,false));
    hmbtn_session_destroy(s);
    puts("Animal tools: real catalog/space/funds/cancel/payment/ownership, product thresholds/daily latches, brush, fed wool interval, shorn art, save and ANI/item handoff passed.");return 0;
}
