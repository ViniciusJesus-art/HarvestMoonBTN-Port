#include "save_fixture.h"
#include "tv_shopping.h"
#include "cooking.h"
#include "psx_data.h"
#include <stdlib.h>
static void answer(HmbtnHouseInteraction *h,uint32_t pad)
{ h->message.phase=HMBTN_MESSAGE_WAIT_END;hmbtn_house_interaction_tick(h,pad,0); }
int main(void)
{
    HmbtnTvShopping offers={0};HmbtnFarmClock c={0,0,5,6,6,0,0};
    HmbtnEconomy money={.gold=3000};
    assert(hmbtn_tv_shopping_program(&offers,&c,0,0)==9 && !offers.offers[0]);
    assert(hmbtn_tv_shopping_program(&offers,&c,3,1)==9);
    c.hour=5;assert(hmbtn_tv_shopping_program(&offers,&c,0,1)==9);c.hour=6;
    c.day=4;assert(hmbtn_tv_shopping_program(&offers,&c,0,1)==9);c.day=5;
    assert(hmbtn_tv_shopping_program(&offers,&c,0,1)==0);
    assert(!hmbtn_tv_shopping_buy(&offers,&money,0) && money.gold==3000);
    hmbtn_tv_shopping_watched(&offers,0);
    assert(hmbtn_tv_shopping_program(&offers,&c,0,1)==0 && offers.offers[0]==HMBTN_TV_WATCHED);
    money.gold=2999;assert(!hmbtn_tv_shopping_buy(&offers,&money,0) && money.gold==2999);
    money.gold=3000;assert(hmbtn_tv_shopping_buy(&offers,&money,0) && money.gold==0);
    assert(!hmbtn_tv_shopping_buy(&offers,&money,0));
    for(unsigned i=1;i<9;++i) {
        hmbtn_farm_clock_advance(&c,7*1440);
        assert(hmbtn_tv_shopping_program(&offers,&c,0,2)==i);
        hmbtn_tv_shopping_watched(&offers,i);money.gold=100000;
        assert(hmbtn_tv_shopping_buy(&offers,&money,i));
        assert(hmbtn_tv_shopping_valid(&offers));
    }
    hmbtn_farm_clock_advance(&c,7*1440);assert(hmbtn_tv_shopping_program(&offers,&c,0,2)==9);
    for(unsigned i=0;i<9;++i) assert(offers.offers[i]==HMBTN_TV_CLOSED);
    offers=(HmbtnTvShopping){0};c=(HmbtnFarmClock){0,0,5,6,6,0,0};
    assert(hmbtn_tv_shopping_program(&offers,&c,0,1)==0);
    c.day+=7;assert(hmbtn_tv_shopping_program(&offers,&c,0,1)==1 && !offers.offers[0]);
    c.weekday=5;c.day+=7;assert(hmbtn_tv_shopping_program(&offers,&c,0,1)==9 && offers.offers[1]==1);
    c=(HmbtnFarmClock){547,0,5,6,6,0,0};offers=(HmbtnTvShopping){0};
    assert(hmbtn_tv_shopping_program(&offers,&c,0,1)==0 && offers.date==(uint16_t)65645);

    HmbtnSession *s=save_fixture(1);HmbtnHouseInteraction *h=&s->house;
    s->game.clock=(HmbtnFarmClock){0,0,5,6,6,0,0};s->construction.house_level=1;s->poultry.upgraded=true;
    s->game.inventory.held_item=(HmbtnStoredTool){0};s->game.economy.gold=3010;
    assert(h->shopping==&s->game.shopping && h->village==&s->village);
    assert(hmbtn_tv_shopping_program(h->shopping,&s->game.clock,0,1)==0);
    hmbtn_tv_shopping_watched(h->shopping,0);
    uint8_t section[16+50*44]={0};uint8_t *z=section+16;
    hmbtn_write_le16(z,0x4000);hmbtn_write_le16(z+2,21);
    hmbtn_write_le16(z+14,(uint16_t)-100);hmbtn_write_le16(z+20,100);
    hmbtn_write_le16(z+18,(uint16_t)-300);hmbtn_write_le16(z+24,(uint16_t)-100);
    HmbtnIntroActor p={0};
    s->game.clock.weekday=0;
    assert(hmbtn_house_interact(h,section,sizeof(section),&p) && h->request==0x11a);
    h->mode=HMBTN_HOUSE_FREE;s->game.clock.weekday=6;
    assert(hmbtn_house_interact(h,section,sizeof(section),&p) && h->request==0x11b);
    answer(h,HMBTN_PAD_DOWN);answer(h,HMBTN_PAD_CROSS);
    assert(s->game.economy.gold==3010 && !s->village.flags[0x67]);
    h->mode=HMBTN_HOUSE_FREE;
    assert(hmbtn_house_interact(h,section,sizeof(section),&p));
    answer(h,HMBTN_PAD_CROSS);assert(h->phone_phase==1 && s->game.economy.gold==3000);
    h->message.phase=HMBTN_MESSAGE_CLOSED;hmbtn_house_interaction_tick(h,0,0);
    assert(h->request==0x11d && h->phone_phase==2);
    answer(h,HMBTN_PAD_CIRCLE);assert(h->request==0x127 && s->game.economy.gold==3000 && !s->village.flags[0x67]);
    h->mode=HMBTN_HOUSE_FREE;
    assert(hmbtn_house_interact(h,section,sizeof(section),&p));
    answer(h,HMBTN_PAD_CROSS);h->message.phase=HMBTN_MESSAGE_CLOSED;hmbtn_house_interaction_tick(h,0,0);
    answer(h,HMBTN_PAD_CROSS);assert(h->request==0x4f && s->game.economy.gold==2990 && !s->village.flags[0x67]);
    h->mode=HMBTN_HOUSE_FREE;s->game.economy.gold=9;
    assert(hmbtn_house_interact(h,section,sizeof(section),&p));
    answer(h,HMBTN_PAD_CROSS);assert(h->request==0x4f && s->game.economy.gold==9);
    h->mode=HMBTN_HOUSE_FREE;s->game.economy.gold=3010;
    assert(hmbtn_house_interact(h,section,sizeof(section),&p));
    answer(h,HMBTN_PAD_CROSS);h->message.phase=HMBTN_MESSAGE_CLOSED;hmbtn_house_interaction_tick(h,0,0);
    answer(h,HMBTN_PAD_CROSS);
    assert(h->request==0x126 && s->game.economy.gold==0 && s->village.flags[0x67] && !s->game.cooking.utensils);
    assert(hmbtn_tv_shopping_accepted(&s->game.shopping)==9);
    h->mode=HMBTN_HOUSE_FREE;h->message=(HmbtnMessage){0};h->closing=false;
    s->daily_date=hmbtn_session_date(&s->game.clock);
    uint8_t *save=NULL;size_t bytes=0;char error[256]={0};
    assert(hmbtn_save_encode(s,&save,&bytes,error,sizeof(error)));
    HmbtnSession *loaded=hmbtn_save_decode(save,bytes,error,sizeof(error));assert(loaded);
    assert(loaded->game.shopping.offers[0]==HMBTN_TV_PAID && loaded->village.flags[0x67]);
    assert(loaded->house.shopping==&loaded->game.shopping && loaded->house.village==&loaded->village);
    assert(!loaded->game.cooking.utensils);
    free(save);hmbtn_session_destroy(loaded);hmbtn_session_destroy(s);
    return 0;
}
