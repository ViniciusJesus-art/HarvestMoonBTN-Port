/* Source-derived forge transactions, shared queue/RNG/inventory and stable state. */
#include "save_fixture.h"
#include "inventory.h"
#include "psx_random.h"
#include <stdio.h>
#include <stdlib.h>
static void input(HmbtnSession *s,uint32_t pad)
{
    s->feed_shop.loaded=s->feed_shop.request;s->feed_shop.message.phase=HMBTN_MESSAGE_WAIT_END;
    hmbtn_feed_shop_tick(&s->feed_shop,&s->house,&s->game.economy,&s->menu.stats,pad,pad);
}
static void until(HmbtnSession *s,HmbtnFeedShopPhase phase)
{
    for(unsigned i=0;i<30 && (s->feed_shop.phase!=phase || s->feed_shop.closing);++i) {
        s->feed_shop.loaded=s->feed_shop.request;s->feed_shop.message.phase=HMBTN_MESSAGE_CLOSED;
        hmbtn_feed_shop_tick(&s->feed_shop,&s->house,&s->game.economy,&s->menu.stats,0,0);
    }
    assert(s->feed_shop.phase==phase && !s->feed_shop.closing);
}
static HmbtnSession *reload(HmbtnSession *s)
{
    uint8_t *bytes=NULL;size_t n=0;char error[256];assert(hmbtn_save_encode(s,&bytes,&n,error,sizeof(error)));
    HmbtnSession *next=hmbtn_save_decode(bytes,n,error,sizeof(error));assert(next);
    assert(next->feed_shop.random==&next->village.random && next->feed_shop.makers==&next->makers);
    free(bytes);hmbtn_session_destroy(s);static const uint8_t schedules[1]={0};
    assert(hmbtn_session_bind_schedules(next,schedules,1));return next;
}
int main(void)
{
    HmbtnSession *s=save_fixture(1);memset(&s->game.inventory,0,sizeof(s->game.inventory));
    s->game.inventory.rucksack_level=2;s->game.economy.gold=10000;
    const unsigned seeds[]={0,2,3};
    for(unsigned i=0;i<3;++i) {
        const unsigned item=0xf7+i;const HmbtnFarmItem *info=hmbtn_farm_item(item);
        assert(info && info->buy_price==1000 && info->sell_price==2000 && info->picture==(int)(142+i) && !info->category);
        s->village.random=seeds[i];uint32_t expected=seeds[i];
        assert(hmbtn_psx_random_range(&expected,3)==i);
        assert(hmbtn_blacksmith_shop_open(&s->feed_shop,&s->game.clock,&s->game.inventory));
        assert(s->feed_shop.gift==item && s->village.random==expected);
        until(s,HMBTN_SHOP_SELECT);assert(hmbtn_blacksmith_row(&s->feed_shop,1)==5);
        input(s,HMBTN_PAD_DOWN);assert(s->feed_shop.selection==5);
        input(s,HMBTN_PAD_TRIANGLE);assert(s->feed_shop.next_request==0x88);until(s,HMBTN_SHOP_SELECT);
        input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0xa3);until(s,HMBTN_SHOP_SELECT);
        s->game.inventory.icebox[0]=(HmbtnStoredTool){0x61,0,1};
        assert(!hmbtn_saibara_accessory_ready(&s->makers,&s->game.inventory,&s->game.economy,item));
        s->game.inventory.icebox[0]=(HmbtnStoredTool){0};
        s->game.inventory.items[0]=(HmbtnStoredTool){0x61,7,1};
        s->game.inventory.items[1]=(HmbtnStoredTool){0x61,9,1};
        s->game.economy.gold=999;input(s,HMBTN_PAD_CROSS);
        assert(s->feed_shop.next_request==0xa3);until(s,HMBTN_SHOP_SELECT);s->game.economy.gold=10000;
        s->game.inventory.held_item=(HmbtnStoredTool){0x62,0,1};
        assert(!hmbtn_saibara_accessory_order(&s->makers,&s->game.inventory,&s->game.economy,item));
        s->game.inventory.held_item=(HmbtnStoredTool){0};
        for(unsigned cancel=0;cancel<2;++cancel) {
            input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_MAKER_CONFIRM);assert(s->feed_shop.request==0x1e1);
            if(cancel) { input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_MAKER_AGAIN);assert(s->feed_shop.request==0x9e); }
            input(s,HMBTN_PAD_CIRCLE);assert(s->feed_shop.next_request==0x93);until(s,HMBTN_SHOP_SELECT);
            assert(s->game.economy.gold==10000 && !s->makers.pending && s->village.random==expected);
        }
        input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_MAKER_CONFIRM);
        input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_MAKER_AGAIN);input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_MAKER_CUE);
        assert(s->makers.pending==item && s->makers.mornings_left==3 && s->game.economy.gold==9000);
        assert(!s->game.inventory.items[0].id && s->game.inventory.items[1].modifier==9);
        assert(!hmbtn_saibara_accessory_order(&s->makers,&s->game.inventory,&s->game.economy,item));
        assert(!hmbtn_makers_deliver(&s->makers));
        s->feed_shop.cue_started=true;s->game.inventory.held_item=(HmbtnStoredTool){0x61,0,1};
        until(s,HMBTN_SHOP_OFF);s->feed_shop.departure=false;
        assert(!s->game.inventory.held_item.id && !hmbtn_blacksmith_upgrade_open(&s->feed_shop,&s->game.clock,&s->game.inventory));
        for(unsigned day=0;day<3;++day) {
            assert(hmbtn_village_flag(&s->village,0x37));s=reload(s);
            hmbtn_session_advance_minutes(s,1440);assert(s->makers.mornings_left==2-day);
            hmbtn_session_clock_events(s,&s->game.clock,HMBTN_CLOCK_DAY);assert(s->makers.mornings_left==2-day);
        }
        assert(!hmbtn_village_flag(&s->village,0x37) && hmbtn_makers_deliver(&s->makers) && !hmbtn_makers_deliver(&s->makers));
        assert(!s->makers.owned);s->game.inventory.items[1]=(HmbtnStoredTool){0};
    }
    s->makers.pending=0xf7;s->makers.mornings_left=4;assert(!hmbtn_makers_valid(&s->makers,&s->construction,&s->poultry));
    s->makers.pending=0xfa;s->makers.mornings_left=3;assert(!hmbtn_makers_valid(&s->makers,&s->construction,&s->poultry));
    assert(!hmbtn_saibara_accessory(0x13f));hmbtn_session_destroy(s);
    puts("Saibara accessories: random offers, refusals, confirmations, unique shared orders, deadlines and resume passed");return 0;
}
