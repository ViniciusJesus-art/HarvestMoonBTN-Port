/* messages.c:ending_offer_advance/confirm; shop_dialogues.c:ending_offer_buy.
 * Prices stay in the shared original item catalog, due flags in the village. */
#include "tv_shopping.h"
#include "farm_field.h"
#include "economy.h"
unsigned hmbtn_tv_shopping_program(HmbtnTvShopping *s,const HmbtnFarmClock *c,unsigned weather,unsigned house)
{
    if(!s || !c || !house || c->weekday!=6 || c->hour<6 || weather>=3
            || (c->season==0 && c->day<5) || (c->season==3 && c->day>=28)) return 9;
    const uint16_t date=(uint16_t)(c->day+c->year*120+c->season*30);
    if(s->date!=date) {
        s->date=date;unsigned selected=9;
        for(unsigned i=0;i<9;++i) {
            const unsigned state=s->offers[i];
            if(state>=HMBTN_TV_OFFERED && state<=HMBTN_TV_PAID) {
                selected=i;s->offers[i]=(uint8_t)(state==HMBTN_TV_PAID ? HMBTN_TV_CLOSED : HMBTN_TV_FREE);break;
            }
        }
        selected=selected==9 ? 0 : (selected+1)%9;
        for(unsigned i=0;i<9;++i) {
            const unsigned slot=(selected+i)%9;
            if(s->offers[slot]==HMBTN_TV_FREE) { s->offers[slot]=HMBTN_TV_OFFERED;break; }
        }
    }
    for(unsigned i=0;i<9;++i) if(s->offers[i]>=HMBTN_TV_OFFERED && s->offers[i]<=HMBTN_TV_PAID) return i;
    return 9;
}
void hmbtn_tv_shopping_watched(HmbtnTvShopping *s,unsigned offer)
{ if(s && offer<9 && s->offers[offer]==HMBTN_TV_OFFERED) s->offers[offer]=HMBTN_TV_WATCHED; }
unsigned hmbtn_tv_shopping_accepted(const HmbtnTvShopping *s)
{
    if(s) for(unsigned i=0;i<9;++i) if(s->offers[i]==HMBTN_TV_WATCHED) return i;
    return 9;
}
unsigned hmbtn_tv_shopping_item(unsigned offer) { return offer<8 ? 0x72+offer : offer==8 ? 0x67 : 0; }
bool hmbtn_tv_shopping_buy(HmbtnTvShopping *s,HmbtnEconomy *e,unsigned offer)
{
    const HmbtnFarmItem *item=hmbtn_farm_item(hmbtn_tv_shopping_item(offer));
    if(!s || !e || offer>=9 || s->offers[offer]!=HMBTN_TV_WATCHED || !item
            || item->buy_price<=0 || e->gold<(unsigned)item->buy_price) return false;
    hmbtn_economy_gold_change(e,-(int64_t)item->buy_price,true);
    s->offers[offer]=HMBTN_TV_PAID;return true;
}
bool hmbtn_tv_shopping_valid(const HmbtnTvShopping *s)
{
    if(!s) return false;
    unsigned active=0;
    for(unsigned i=0;i<9;++i) {
        const unsigned state=s->offers[i];
        if(state!=HMBTN_TV_FREE && state!=HMBTN_TV_CLOSED && (state<HMBTN_TV_OFFERED || state>HMBTN_TV_PAID)) return false;
        if(state>=HMBTN_TV_OFFERED && state<=HMBTN_TV_PAID) ++active;
    }
    return active<=1;
}
