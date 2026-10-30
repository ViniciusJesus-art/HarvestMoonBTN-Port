/* Original winery list, single purchase timing, repeat stock, carried records,
 * wine/juice consumption, recipe bonus and existing-state resume. */
#include "save_fixture.h"
#include "cooking.h"
#include "inventory.h"
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
    const HmbtnFeedShop loans=s->feed_shop;
    assert(hmbtn_wine_shop_open(&s->feed_shop,&s->game.clock,&s->game.inventory));
    assert(s->feed_shop.wine && s->feed_shop.merchant==22 && s->feed_shop.request==0xb0 && s->feed_shop.quantity==1);
    assert(s->feed_shop.economy==loans.economy && s->feed_shop.action==loans.action && s->feed_shop.random==loans.random && s->feed_shop.construction==loans.construction);
    assert(!s->feed_shop.clinic && !s->feed_shop.blacksmith && !s->feed_shop.clinic_shade);
    assert(!hmbtn_wine_shop_open(&s->feed_shop,&s->game.clock,&s->game.inventory));
    closed(s,HMBTN_SHOP_SELECT);assert(s->feed_shop.request==0x46);
}
int main(void)
{
    HmbtnSession *s=save_fixture(1);HmbtnInventory *h=&s->game.inventory;memset(h,0,sizeof(*h));h->rucksack_level=2;
    HmbtnFeedShop *shop=&s->feed_shop;HmbtnEconomy *e=&s->game.economy;
    s->game.clock.hour=10;s->daily_date=hmbtn_session_date(&s->game.clock);
    assert(!hmbtn_wine_shop_open(NULL,&s->game.clock,h) && !hmbtn_wine_shop_open(shop,NULL,h) && !hmbtn_wine_shop_open(shop,&s->game.clock,NULL));
    h->held_item=(HmbtnStoredTool){159,0,1};const HmbtnInventory held=*h;
    assert(!hmbtn_wine_shop_open(shop,&s->game.clock,h) && !memcmp(h,&held,sizeof(held)));h->held_item=(HmbtnStoredTool){0};
    assert(hmbtn_wine_shop_entry(0)->item==159 && hmbtn_wine_shop_entry(0)->description==0xb2);
    assert(hmbtn_wine_shop_entry(1)->item==160 && hmbtn_wine_shop_entry(1)->description==0xb3 && !hmbtn_wine_shop_entry(2));
    const unsigned ids[]={159,160},prices[]={300,200};
    for(unsigned row=0;row<2;++row) for(unsigned face=0;face<4;++face) {
        e->gold=1000;s->intro.player.facing=face;s->intro.player.animation_done=false;
        /* A full, valid goods bag does not block receipt into the empty hand. */
        for(unsigned i=0;i<8;++i) h->items[i]=(HmbtnStoredTool){153,0,1};
        const HmbtnInventory before=*h;const HmbtnEconomy money=*e;open_shop(s);
        tick(s,HMBTN_PAD_DOWN,HMBTN_MESSAGE_PRINTING);assert(shop->selection==0 && e->gold==1000);
        if(row) tick(s,HMBTN_PAD_DOWN,HMBTN_MESSAGE_WAIT_END);
        assert(shop->item==ids[row] && hmbtn_feed_shop_cost(shop)==prices[row]);
        tick(s,HMBTN_PAD_TRIANGLE,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_INFO);assert(shop->request==0xb2+row);
        closed(s,HMBTN_SHOP_SELECT);tick(s,HMBTN_PAD_CIRCLE,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_LEAVE);assert(shop->request==0xb1);
        closed(s,HMBTN_SHOP_OFF);assert(!memcmp(e,&money,sizeof(money)) && !memcmp(h,&before,sizeof(before)));
        e->gold=prices[row]-1;open_shop(s);if(row) tick(s,HMBTN_PAD_DOWN,HMBTN_MESSAGE_WAIT_END);
        tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);closed(s,HMBTN_SHOP_RESULT);assert(shop->request==0x4f);
        closed(s,HMBTN_SHOP_GREETING);assert(shop->request==0xb5);closed(s,HMBTN_SHOP_SELECT);
        assert(e->gold==prices[row]-1 && e->ledger[0][1]==money.ledger[0][1] && !memcmp(h,&before,sizeof(before)));
        e->gold=prices[row];tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_WAIT_END);
        assert(!e->gold && e->ledger[0][1]==money.ledger[0][1]+prices[row] && !h->held_item.id);
        closed(s,HMBTN_SHOP_RESULT);assert(shop->request==0xb4);
        for(unsigned n=0;n<10;++n) tick(s,0,HMBTN_MESSAGE_WAIT_END);
        assert(!h->held_item.id && e->ledger[0][1]==money.ledger[0][1]+prices[row]);closed(s,HMBTN_SHOP_RUCKSACK_CUE);
        assert(!hmbtn_save_available(s,(char[256]){0},256));
        assert(hmbtn_event_item_show(&s->event_item,&s->intro.player,h,ids[row]));shop->cue_started=true;
        assert(s->intro.player.sprite.animation==11+face);tick(s,HMBTN_PAD_CROSS,HMBTN_MESSAGE_CLOSED);assert(shop->phase==HMBTN_SHOP_RUCKSACK_CUE);
        s->intro.player.animation_done=true;hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);closed(s,HMBTN_SHOP_OFF);
        assert(h->held_item.id==ids[row] && h->held_item.count==1 && !h->held_item.modifier && !memcmp(h->items,before.items,sizeof(h->items)));
        assert(hmbtn_inventory_storage_accepts(2,ids[row]) && hmbtn_inventory_storage_swap(h,2,0,0,9,0) && !h->held_item.id);
        assert(hmbtn_inventory_storage_swap(h,2,9,0,0,0) && h->held_item.id==ids[row]);
        s->farm_action.stamina=50;s->farm_action.max_stamina=100;s->farm_action.fatigue=20;s->farm_action.happiness=10;
        const HmbtnFarmAction status=s->farm_action;
        assert(hmbtn_event_item_use(&s->event_item,&s->intro.player,h,&s->game.clock));
        assert(s->event_item.phase==HMBTN_EVENT_ITEM_USE_DRINK && s->event_item.sound==75);
        s->intro.player.animation_done=false;hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);
        assert(!memcmp(&status,&s->farm_action,sizeof(status)) && h->held_item.id==ids[row]);
        s->intro.player.animation_done=true;hmbtn_event_item_tick(&s->event_item,&s->intro.player,h,&s->farm_action);
        assert(!h->held_item.id && s->farm_action.stamina==(row ? 55u : 50u) && s->farm_action.fatigue==(row ? 18u : 15u) && s->farm_action.happiness==(row ? 10u : 11u));
    }
    const uint16_t ingredients[8]={89,160};HmbtnRecipeRecord recipe;
    assert(hmbtn_cooking_evaluate(8,ingredients,&recipe) && recipe.id==180 && recipe.quality==23);
    assert(hmbtn_farm_item(160)->stamina==5 && !(hmbtn_farm_item(160)->flags&0x800u) && (hmbtn_farm_item(180)->flags&0x800u));
    uint8_t *data=NULL;size_t bytes=0;char error[256];assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));
    assert(loaded && !memcmp(&loaded->game.inventory,h,sizeof(*h)) && !memcmp(&loaded->game.economy,e,sizeof(*e)) && loaded->farm_action.happiness==10);
    hmbtn_session_destroy(loaded);free(data);hmbtn_session_destroy(s);
    puts("Winery: source rows/prices, cancel/poor/retry, immediate unique debit, full bag/receipt/icebox, wine and grape-juice effects, recipe bonus and resume passed");return 0;
}
