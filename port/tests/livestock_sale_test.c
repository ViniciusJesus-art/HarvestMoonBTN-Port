/* shop_barn_step / shop_livestock_find(filter=0), independent source vectors. */
#include "save_fixture.h"
#include <stdlib.h>
#include <stdio.h>
static const uint16_t name[9]={0x10,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
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
    HmbtnSession *s=save_fixture(136);HmbtnLivestock *p=&s->livestock;
    const unsigned affection[]={0,100,101,200,201,255};
    const unsigned prices[2][6]={{3000,3000,4500,4500,5000,5000},{2000,2000,3000,3000,3500,3500}};
    assert(!hmbtn_livestock_sale_price(NULL,0) && !hmbtn_livestock_sell(p,HMBTN_LIVESTOCK));
    for(unsigned species=0;species<2;++species) {
        assert(hmbtn_livestock_buy(p,species ? 0x165 : 0x161,name,&s->game.clock));
        HmbtnFarmAnimal *a=&p->animals[species];
        if(!species) { assert(!hmbtn_livestock_sale_price(p,species));a->growth=28; }
        for(unsigned i=0;i<6;++i) {
            a->affection=affection[i];a->days_sick=2;a->days_since_shearing=0;
            assert(hmbtn_livestock_sale_price(p,species)==prices[species][i]);
        }
        a->prize_winner=true;assert(hmbtn_livestock_sale_price(p,species)==(species ? 6000u : 8000u));
        a->dead=true;assert(!hmbtn_livestock_sale_price(p,species) && !hmbtn_livestock_sell(p,species));
        assert(a->occupied && hmbtn_livestock_sale_slot(p,species,0)==HMBTN_LIVESTOCK);a->dead=false;
    }
    /* A mixed herd, holes and a reserved death: list indexes refer to live
     * records, never a copy/compaction of the authoritative collection. */
    for(unsigned i=2;i<10;++i) {
        assert(hmbtn_livestock_buy(p,i==9 ? 0x165 : 0x161,name,&s->game.clock));
        p->animals[i].growth=28;p->animals[i].affection=120;
    }
    p->animals[8].dead=true;p->animals[9].growth=0;
    assert(hmbtn_livestock_sale_slot(p,false,6)==7 && hmbtn_livestock_sale_slot(p,false,7)==HMBTN_LIVESTOCK);
    assert(hmbtn_livestock_sale_slot(p,true,0)==1 && hmbtn_livestock_sale_slot(p,true,1)==HMBTN_LIVESTOCK);
    s->feed_shop.phase=HMBTN_SHOP_SELECT;s->feed_shop.merchant=15;s->feed_shop.request=0x46;
    s->feed.barn[3]=true;s->game.economy.gold=100;
    input(s,HMBTN_PAD_DOWN);input(s,HMBTN_PAD_DOWN);input(s,HMBTN_PAD_DOWN);
    assert(s->feed_shop.selection==6 && hmbtn_barn_shop_rows(&s->feed_shop)==6);
    input(s,HMBTN_PAD_TRIANGLE);assert(s->feed_shop.next_request==0x69);until(s,HMBTN_SHOP_SELECT);
    input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x75);until(s,HMBTN_SHOP_SELL_LIST);
    input(s,HMBTN_PAD_UP);assert(s->feed_shop.sale_cursor==6 && s->feed_shop.sale_top==3);
    input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.sale_slot==7 && s->feed_shop.next_request==0x7f);
    until(s,HMBTN_SHOP_SELL_CONFIRM);
    input(s,HMBTN_PAD_CIRCLE);until(s,HMBTN_SHOP_SELECT);
    assert(p->animals[7].occupied && s->game.economy.gold==100);
    input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_SELL_LIST);
    input(s,HMBTN_PAD_CIRCLE);until(s,HMBTN_SHOP_SELECT);assert(s->game.economy.gold==100);
    input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_SELL_LIST);
    for(unsigned i=0;i<6;++i) input(s,HMBTN_PAD_DOWN);
    input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_SELL_CONFIRM);
    const HmbtnFarmAnimal neighbor=p->animals[6],dead=p->animals[8];
    const HmbtnFarmFeed feed=s->feed;
    input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x83 && s->game.economy.gold==4600);
    for(unsigned i=0;i<4;++i) input(s,HMBTN_PAD_CROSS);
    assert(s->game.economy.gold==4600 && !p->animals[7].occupied && hmbtn_livestock_free(p)==7);
    assert(!memcmp(&neighbor,&p->animals[6],sizeof(neighbor)) && !memcmp(&dead,&p->animals[8],sizeof(dead))
        && !memcmp(&feed,&s->feed,sizeof(feed)));
    until(s,HMBTN_SHOP_SELECT);input(s,HMBTN_PAD_DOWN);assert(s->feed_shop.selection==7);
    input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_SELL_LIST);input(s,HMBTN_PAD_CROSS);
    assert(s->feed_shop.next_request==0x81);until(s,HMBTN_SHOP_SELL_CONFIRM);
    input(s,HMBTN_PAD_CROSS);until(s,HMBTN_SHOP_SELECT);assert(s->game.economy.gold==10600 && !p->animals[1].occupied);
    input(s,HMBTN_PAD_CROSS);assert(s->feed_shop.next_request==0x78);until(s,HMBTN_SHOP_SELECT);
    input(s,HMBTN_PAD_CIRCLE);until(s,HMBTN_SHOP_OFF);
    assert(hmbtn_livestock_buy(p,0x165,name,&s->game.clock) && p->animals[1].occupied);
    uint8_t *data=NULL;size_t bytes=0;char error[256]={0};
    assert(hmbtn_save_encode(s,&data,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(data,bytes,error,sizeof(error));assert(loaded);
    assert(loaded->game.economy.gold==10600 && !loaded->livestock.animals[7].occupied
        && loaded->livestock.animals[8].occupied && loaded->livestock.animals[8].dead && loaded->feed.barn[3]);
    free(data);hmbtn_session_destroy(loaded);hmbtn_session_destroy(s);
    puts("Livestock sales: source price thresholds, eligibility, species/pagination, cancellation, single payment, stable slots/feed and saves passed.");return 0;
}
