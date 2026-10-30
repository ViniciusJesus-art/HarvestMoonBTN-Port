/* Kai shares shop_goods_run with Doug; replies and stock remain distinct. */
#include "save_fixture.h"
#include <stdio.h>
#include <stdlib.h>
static void tick(HmbtnSession *s,uint32_t pad,HmbtnMessagePhase phase)
{
    s->feed_shop.loaded=s->feed_shop.request;s->feed_shop.message.phase=phase;
    hmbtn_feed_shop_tick(&s->feed_shop,&s->house,&s->game.economy,&s->menu.stats,pad,pad);
}
static void closed(HmbtnSession *s,HmbtnFeedShopPhase phase)
{
    for(unsigned i=0;i<30 && (s->feed_shop.phase!=phase || s->feed_shop.closing);++i) tick(s,0,HMBTN_MESSAGE_CLOSED);
    assert(s->feed_shop.phase==phase && !s->feed_shop.closing);
}
static void open_shop(HmbtnSession *s)
{
    assert(hmbtn_kai_shop_open(&s->feed_shop,&s->game.clock,&s->game.inventory));
    assert(s->feed_shop.kai && !s->feed_shop.inn && s->feed_shop.merchant==25 && s->feed_shop.request==0x128);
    assert(!hmbtn_kai_shop_open(&s->feed_shop,&s->game.clock,&s->game.inventory));closed(s,HMBTN_SHOP_SELECT);
}
int main(void)
{
    HmbtnSession *s=save_fixture(1);HmbtnInventory *h=&s->game.inventory;HmbtnFeedShop *shop=&s->feed_shop;
    memset(h,0,sizeof(*h));h->rucksack_level=2;
    for(unsigned i=0;i<8;++i) h->items[i]=(HmbtnStoredTool){153,0,1};
    h->held_item=(HmbtnStoredTool){164,154,3};
    assert(!hmbtn_kai_shop_open(shop,&s->game.clock,h));memset(&h->held_item,0,sizeof(h->held_item));
    const HmbtnInventory inventory=*h;const HmbtnEconomy economy=s->game.economy;
    open_shop(s);assert(hmbtn_meal_shop_rows(shop)==5 && !hmbtn_meal_shop_entry(shop,5));
    tick(s,HMBTN_PAD_UP,HMBTN_MESSAGE_WAIT_END);assert(shop->selection==4 && shop->item==171);
    tick(s,HMBTN_PAD_DOWN,HMBTN_MESSAGE_WAIT_END);assert(shop->selection==0 && shop->item==172);
    tick(s,HMBTN_PAD_CIRCLE,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_LEAVE);assert(shop->request==0x12e);closed(s,HMBTN_SHOP_OFF);
    assert(!memcmp(h,&inventory,sizeof(*h)) && !memcmp(&economy,&s->game.economy,sizeof(economy)));
    /* Water followed by food, then water again: the special reply cannot stick. */
    const unsigned order[]={4,0,1,2,3,4},ids[]={172,309,310,311,171},prices[]={300,250,300,200,0},descriptions[]={0x12c,0x129,0x12a,0x12b,0x12d};
    const unsigned energy[]={0,10,20,15,1},recovery[]={10,1,1,1,0};
    for(unsigned n=0;n<6;++n) {
        const unsigned row=order[n],spent=s->game.economy.ledger[0][1];open_shop(s);
        for(unsigned i=0;i<row;++i) tick(s,HMBTN_PAD_DOWN,HMBTN_MESSAGE_WAIT_END);
        const HmbtnSeedShopEntry *entry=hmbtn_meal_shop_entry(shop,row);
        assert(entry && entry->item==ids[row] && entry->description==descriptions[row] && entry->name_words && hmbtn_feed_shop_cost(shop)==prices[row]);
        tick(s,HMBTN_PAD_TRIANGLE,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_INFO);assert(shop->request==descriptions[row]);closed(s,HMBTN_SHOP_SELECT);
        if(prices[row]) {
            s->game.economy.gold=prices[row]-1;tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_RESULT);assert(shop->request==0x4f);
            closed(s,HMBTN_SHOP_INFO);assert(shop->request==0x12f);closed(s,HMBTN_SHOP_SELECT);
            assert(s->game.economy.gold==prices[row]-1 && s->game.economy.ledger[0][1]==spent && !memcmp(h,&inventory,sizeof(*h)));
        }
        s->game.economy.gold=prices[row];tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);
        assert(!s->game.economy.gold && s->game.economy.ledger[0][1]==spent+prices[row]);closed(s,HMBTN_SHOP_RESULT);assert(shop->request==0x130);
        for(unsigned i=0;i<5;++i) tick(s,0,HMBTN_MESSAGE_WAIT_END);
        assert(!memcmp(h,&inventory,sizeof(*h)));tick(s,0,HMBTN_MESSAGE_CLOSED);assert(shop->request==(row==4 ? 0x200u : 0x131u));
        closed(s,HMBTN_SHOP_SERVE);assert(!shop->cue_started);
        for(unsigned facing=0;facing<4;++facing) {
            s->intro.player.facing=facing;s->farm_action.stamina=60;s->farm_action.max_stamina=100;s->farm_action.fatigue=30;s->farm_action.happiness=10;
            const HmbtnFarmAction status=s->farm_action;
            assert(hmbtn_event_item_receive(&s->event_item,&s->intro.player,h,&s->game.clock,entry->item));
            assert(s->event_item.phase==(row==4 ? HMBTN_EVENT_ITEM_SERVE_DRINK : HMBTN_EVENT_ITEM_SERVE_FOOD));
            assert(s->intro.player.sprite.animation==(row==4 ? 0 : 5) && h->held_item.id==ids[row]);
            assert(!hmbtn_event_item_receive(&s->event_item,&s->intro.player,h,&s->game.clock,entry->item));
            hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);assert(!memcmp(&status,&s->farm_action,sizeof(status)));
            s->intro.player.animation_done=true;hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);
            assert(!s->event_item.phase && s->farm_action.stamina==60+energy[row] && s->farm_action.fatigue==30-recovery[row] && s->farm_action.happiness==10);
            assert(!memcmp(h,&inventory,sizeof(*h)));hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);assert(s->farm_action.stamina==60+energy[row]);
        }
        shop->cue_started=true;closed(s,HMBTN_SHOP_OFF);assert(!s->game.economy.gold && s->game.economy.ledger[0][1]==spent+prices[row]);
    }
    puts("Kai: five original rows, prices/replies, water-food-water, poor/cancel, one payment and deferred food/drink effects passed");
    hmbtn_session_destroy(s);return 0;
}
