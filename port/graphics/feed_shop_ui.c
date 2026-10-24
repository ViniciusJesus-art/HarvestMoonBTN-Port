/* shop_draw_currency_panel, shop_menu_draw and shop_draw_count_total. */
#include "feed_shop_ui.h"
#include "window_frame.h"
#include "feed_shop_labels.inc"
#define WORDS(a) (unsigned)(sizeof(a)/sizeof(*(a)))
static bool number(HmbtnNewGameUI *ui,uint8_t *pixels,unsigned value,unsigned width,
    const uint16_t *suffix,int x,int y,char *error,size_t size)
{
    uint16_t text[20];unsigned n=width;
    if(width>8) return false;
    for(unsigned i=0;i<width;++i) { text[width-i-1]=(uint16_t)(0x40+value%10);value/=10; }
    for(unsigned i=0;i+1<width && text[i]==0x40;++i) text[i]=0xFFD;
    if(suffix) for(unsigned i=0;suffix[i]!=0xFFFF && n<19;++i) text[n++]=suffix[i];
    text[n++]=0xFFFF;
    return hmbtn_ui_text(ui,pixels,text,n,x,y,0,3,error,size);
}
bool hmbtn_feed_shop_ui_draw(const HmbtnFeedShop *s,const HmbtnFarmStatistics *stats,
    HmbtnNewGameUI *ui,uint8_t *p,unsigned tick,char *error,size_t size)
{
    if(s->phase==HMBTN_SHOP_OFF || s->phase==HMBTN_SHOP_NAME) return true;
    const uint16_t *name=s->merchant==15 ? shop_fodder : shop_chicken_feed;
    const unsigned length=s->merchant==15 ? WORDS(shop_fodder) : WORDS(shop_chicken_feed);
    hmbtn_window_frame_draw(p,320,240,227,16,79,34);
    if(!hmbtn_ui_text(ui,p,shop_money_title,WORDS(shop_money_title),231,20,0,3,error,size)
            || !number(ui,p,stats->gold,8,shop_gold_unit,231,34,error,size)) return false;
    if(s->phase==HMBTN_SHOP_GREETING || s->phase==HMBTN_SHOP_LEAVE) return true;
    const bool hens=s->merchant==18 && s->poultry;
    hmbtn_window_frame_draw(p,320,240,16,16,174,hens ? 36 : 22);
    if(!hmbtn_ui_text(ui,p,name,length,34,20,0,3,error,size)
            || !hmbtn_ui_icon(ui,p,11,1,23,29+(int)s->selection*14,error,size)) return false;
    if(hens && !hmbtn_ui_text(ui,p,shop_buy_chicken,WORDS(shop_buy_chicken),34,34,0,3,error,size)) return false;
    if(s->selection) return true;
    const bool quantity=s->phase==HMBTN_SHOP_COUNT || s->phase==HMBTN_SHOP_OFFER
        || (s->phase==HMBTN_SHOP_RESULT && s->after_result==HMBTN_SHOP_COUNT);
    if(!quantity) return true;
    const int y=hens ? 56 : 42;
    hmbtn_window_frame_draw(p,320,240,16,y,116,48);
    return hmbtn_ui_text(ui,p,name,length,20,y+4,0,3,error,size)
        && number(ui,p,s->quantity,3,shop_units,48,y+18,error,size)
        && hmbtn_ui_icon(ui,p,13,(tick/15)%2,20,y+18,error,size)
        && hmbtn_ui_icon(ui,p,12,(tick/15)%2,112,y+18,error,size)
        && number(ui,p,hmbtn_feed_shop_cost(s),5,shop_gold_unit,32,y+32,error,size);
}
