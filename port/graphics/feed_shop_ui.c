/* shop_draw_currency_panel, shop_menu_draw and shop_draw_count_total. */
#include "feed_shop_ui.h"
#include "window_frame.h"
#include <stdio.h>
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
static bool sale_list(const HmbtnFeedShop *s,const HmbtnRegistration *g,HmbtnNewGameUI *ui,
    uint8_t *p,unsigned tick,char *error,size_t size)
{
    /* shop_record_list: four rows, original name/date/affection icons. */
    hmbtn_window_frame_draw(p,320,240,24,32,272,116);
    hmbtn_window_frame_draw(p,320,240,80,16,160,32);
    uint16_t title[20];unsigned n=0;
    while(n<HMBTN_NAME_CHARS && g->farm_name[n]!=0xFFFF) { title[n]=g->farm_name[n];++n; }
    for(unsigned i=0;i<WORDS(shop_farm);++i) title[n++]=shop_farm[i];
    if(!hmbtn_ui_text(ui,p,title,n,82,26,0,3,error,size)
            || !hmbtn_ui_icon(ui,p,39,0,176,18,error,size)
            || !hmbtn_ui_text(ui,p,shop_name_panel_title,WORDS(shop_name_panel_title),200,26,0,3,error,size)
            || !hmbtn_ui_icon(ui,p,11,1,32,60+(int)(s->sale_cursor-s->sale_top)*24,error,size)) return false;
    if(s->sale_top && !hmbtn_ui_icon(ui,p,41,(tick/15)%2,288,48,error,size)) return false;
    if(hmbtn_poultry_sale_slot(s->poultry,s->sale_top+4)<HMBTN_HENS
            && !hmbtn_ui_icon(ui,p,40,(tick/15)%2,288,128,error,size)) return false;
    for(unsigned row=0;row<4;++row) {
        unsigned slot=hmbtn_poultry_sale_slot(s->poultry,s->sale_top+row);
        if(slot==HMBTN_HENS) break;
        const HmbtnHen *h=&s->poultry->hens[slot];const int y=48+(int)row*24;
        uint16_t date[14]={h->prize ? 0x90 : 0xFFD,0xFFD,0xFFD,0xFFD,0x71,
            (uint16_t)(0x40+(h->day+1)/10),(uint16_t)(0x40+(h->day+1)%10),0xFFD,0,0x26,0x24,
            (uint16_t)(0x40+(h->age%100)/10),(uint16_t)(0x40+h->age%10),0xFFFF};
        if(!number(ui,p,s->sale_top+row+1,2,NULL,32,y+6,error,size)
                || !hmbtn_ui_icon(ui,p,39,0,64,y,error,size)
                || !hmbtn_ui_text(ui,p,h->name,HMBTN_NAME_CHARS+1,90,y,0,3,error,size)
                || !hmbtn_ui_text(ui,p,date,WORDS(date),176,y,0,3,error,size)
                || !hmbtn_ui_icon(ui,p,43,h->season,192,y,error,size)) return false;
        for(unsigned heart=0;heart<h->affection/25 && heart<10;++heart)
            if(!hmbtn_ui_icon(ui,p,42,0,160-(int)heart*8,y+12,error,size)) return false;
    }
    return true;
}
bool hmbtn_feed_shop_ui_draw(const HmbtnFeedShop *s,const HmbtnEconomy *economy,const HmbtnRegistration *registration,
    HmbtnNewGameUI *ui,uint8_t *p,unsigned tick,char *error,size_t size)
{
    if(s->phase==HMBTN_SHOP_OFF || s->phase==HMBTN_SHOP_NAME) return true;
    if(s->seeds) {
        /* shop_gold_panel_step / shop_gold_choice_step / shop_panel_submit.
         * Seed entries have flag=1: names only; price belongs to quantity total. */
        hmbtn_window_frame_draw(p,320,240,227,16,79,34);
        if(!hmbtn_ui_text(ui,p,shop_money_title,WORDS(shop_money_title),231,20,0,3,error,size)
                || !number(ui,p,economy->gold,8,shop_gold_unit,231,34,error,size)) return false;
        if(s->phase==HMBTN_SHOP_GREETING || s->phase==HMBTN_SHOP_LEAVE) return true;
        const unsigned rows=hmbtn_seed_shop_count(s);
        if(!rows || s->selection>=rows) { snprintf(error,size,"Unsupported seed shop season/selection");return false; }
        /* Original setup_c reserves wider columns than setup_a/b. Fit the
         * imported labels to this renderer's eight-pixel glyph advance, keeping
         * the existing cursor/text padding and names-only list. */
        unsigned width=127;
        for(unsigned i=0;i<rows;++i) {
            const HmbtnSeedShopEntry *entry=hmbtn_seed_shop_choice(s,i);
            const unsigned label_width=15+(entry->name_words-1)*8;
            if(label_width>width) width=label_width;
        }
        hmbtn_window_frame_draw(p,320,240,16,16,width,rows*14+6);
        for(unsigned i=0;i<rows;++i) {
            const HmbtnSeedShopEntry *entry=hmbtn_seed_shop_choice(s,i);
            if(!hmbtn_ui_text(ui,p,entry->name,entry->name_words,28,20+(int)i*14,0,3,error,size)) return false;
        }
        if(!hmbtn_ui_icon(ui,p,11,1,20,20+(int)s->selection*14,error,size)) return false;
        const bool quantity=s->phase==HMBTN_SHOP_COUNT
            || (s->phase==HMBTN_SHOP_INFO && s->after_result==HMBTN_SHOP_COUNT)
            || (s->phase==HMBTN_SHOP_RESULT && s->after_result==HMBTN_SHOP_COUNT);
        if(!quantity) return true;
        /* Keep the native panel below all names; the original selected-row
         * placement overlaps later labels in this software renderer. */
        const int y=20+(int)rows*14;
        hmbtn_window_frame_draw(p,320,240,16,y,116,34);
        return number(ui,p,s->quantity,3,shop_units,48,y+4,error,size)
            && hmbtn_ui_icon(ui,p,13,(tick/15)%2,20,y+4,error,size)
            && hmbtn_ui_icon(ui,p,12,(tick/15)%2,112,y+4,error,size)
            && number(ui,p,hmbtn_feed_shop_cost(s),5,shop_gold_unit,32,y+18,error,size);
    }
    if(s->phase==HMBTN_SHOP_SELL_LIST || s->phase==HMBTN_SHOP_SELL_CONFIRM) {
        if(!sale_list(s,registration,ui,p,tick,error,size)) return false;
        if(s->phase!=HMBTN_SHOP_SELL_CONFIRM || s->message.phase!=HMBTN_MESSAGE_WAIT_END || s->closing) return true;
        hmbtn_window_frame_draw(p,320,240,200,128,40,36);
        return hmbtn_ui_text(ui,p,shop_yes,WORDS(shop_yes),212,132,0,3,error,size)
            && hmbtn_ui_text(ui,p,shop_no,WORDS(shop_no),212,146,0,3,error,size)
            && hmbtn_ui_icon(ui,p,11,1,212,138+(int)s->answer*14,error,size);
    }
    const uint16_t *name=s->merchant==15 ? shop_fodder : shop_chicken_feed;
    const unsigned length=s->merchant==15 ? WORDS(shop_fodder) : WORDS(shop_chicken_feed);
    hmbtn_window_frame_draw(p,320,240,227,16,79,34);
    if(!hmbtn_ui_text(ui,p,shop_money_title,WORDS(shop_money_title),231,20,0,3,error,size)
            || !number(ui,p,economy->gold,8,shop_gold_unit,231,34,error,size)) return false;
    if(s->phase==HMBTN_SHOP_GREETING || s->phase==HMBTN_SHOP_LEAVE) return true;
    const bool hens=s->merchant==18 && s->poultry;
    const unsigned rows=(hens ? 3u : 1u)+s->medicine_available;
    const unsigned selected=s->selection==3 ? rows-1 : s->selection;
    hmbtn_window_frame_draw(p,320,240,16,16,208,8+rows*14);
    if(!hmbtn_ui_text(ui,p,name,length,34,20,0,3,error,size)
            || !hmbtn_ui_icon(ui,p,11,1,23,19+(int)selected*14,error,size)) return false;
    if(hens && !hmbtn_ui_text(ui,p,shop_buy_chicken,WORDS(shop_buy_chicken),34,34,0,3,error,size)) return false;
    if(hens && !hmbtn_ui_text(ui,p,shop_sell_chicken,WORDS(shop_sell_chicken),34,48,0,3,error,size)) return false;
    if(s->medicine_available && !hmbtn_ui_text(ui,p,shop_animal_medicine,WORDS(shop_animal_medicine),34,
            hens ? 62 : 34,0,3,error,size)) return false;
    /* shop_menu_draw prints each item's unit price beside its label. */
    const HmbtnFarmItem *feed=hmbtn_farm_item(s->merchant==15 ? 0x68 : 0x69);
    const HmbtnFarmItem *chicken=hmbtn_farm_item(0x109),*medicine=hmbtn_farm_item(0x1E);
    if(!feed || !chicken || !medicine
            || !number(ui,p,(unsigned)feed->buy_price,5,shop_gold_unit,172,20,error,size)) return false;
    if(hens && !number(ui,p,(unsigned)chicken->buy_price,5,shop_gold_unit,172,34,error,size)) return false;
    if(s->medicine_available && !number(ui,p,(unsigned)medicine->buy_price,5,shop_gold_unit,
            172,hens ? 62 : 34,error,size)) return false;
    if(s->selection) return true;
    const bool quantity=s->phase==HMBTN_SHOP_COUNT || s->phase==HMBTN_SHOP_OFFER
        || (s->phase==HMBTN_SHOP_RESULT && s->after_result==HMBTN_SHOP_COUNT);
    if(!quantity) return true;
    const int y=28+(int)rows*14;
    hmbtn_window_frame_draw(p,320,240,16,y,116,48);
    return hmbtn_ui_text(ui,p,name,length,20,y+4,0,3,error,size)
        && number(ui,p,s->quantity,3,shop_units,48,y+18,error,size)
        && hmbtn_ui_icon(ui,p,13,(tick/15)%2,20,y+18,error,size)
        && hmbtn_ui_icon(ui,p,12,(tick/15)%2,112,y+18,error,size)
        && number(ui,p,hmbtn_feed_shop_cost(s),5,shop_gold_unit,32,y+32,error,size);
}
