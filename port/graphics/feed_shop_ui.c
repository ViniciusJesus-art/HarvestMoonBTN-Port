/* shop_draw_currency_panel, shop_menu_draw and shop_draw_count_total. */
#include "feed_shop_ui.h"
#include "window_frame.h"
#include <stdio.h>
#include "feed_shop_labels.inc"
#define WORDS(a) (unsigned)(sizeof(a)/sizeof(*(a)))
static bool text_cursor(HmbtnNewGameUI *ui,uint8_t *pixels,int x,int y,char *error,size_t size)
{
    /* text.c:text_draw_cursor adds 6 to the text row's Y. These native
     * layouts already supply the icon's horizontal anchor, including inset. */
    return hmbtn_ui_icon(ui,pixels,11,1,x,y+6,error,size);
}
static bool blacksmith_answer(const HmbtnFeedShop *s,HmbtnNewGameUI *ui,uint8_t *p,char *error,size_t size)
{
    if((s->phase!=HMBTN_SHOP_MAKER_CONFIRM && s->phase!=HMBTN_SHOP_MAKER_AGAIN)
            || s->closing || s->message.phase!=HMBTN_MESSAGE_WAIT_END) return true;
    const int x=s->phase==HMBTN_SHOP_MAKER_CONFIRM ? 200 : 224;
    hmbtn_window_frame_draw(p,320,240,x,128,48,36);
    return hmbtn_ui_text(ui,p,shop_yes,WORDS(shop_yes),x+16,132,0,3,error,size)
        && hmbtn_ui_text(ui,p,shop_no,WORDS(shop_no),x+16,146,0,3,error,size)
        && text_cursor(ui,p,x+8,132+(int)s->answer*14,error,size);
}
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
    const bool barn=s->merchant==15,sheep=barn && s->selection==7;
    const unsigned species_icon=barn ? (sheep ? 1u : 2u) : 0;
    hmbtn_window_frame_draw(p,320,240,24,32,272,116);
    hmbtn_window_frame_draw(p,320,240,80,16,160,32);
    uint16_t title[20];unsigned n=0;
    while(n<HMBTN_NAME_CHARS && g->farm_name[n]!=0xFFFF) { title[n]=g->farm_name[n];++n; }
    for(unsigned i=0;i<WORDS(shop_farm);++i) title[n++]=shop_farm[i];
    if(!hmbtn_ui_text(ui,p,title,n,82,26,0,3,error,size)
            || !hmbtn_ui_icon(ui,p,39,species_icon,176,18,error,size)
            || !hmbtn_ui_text(ui,p,shop_name_panel_title,WORDS(shop_name_panel_title),200,26,0,3,error,size)
            || !hmbtn_ui_icon(ui,p,11,1,32,60+(int)(s->sale_cursor-s->sale_top)*24,error,size)) return false;
    if(s->sale_top && !hmbtn_ui_icon(ui,p,41,(tick/15)%2,288,48,error,size)) return false;
    const bool more=barn ? hmbtn_livestock_sale_slot(s->livestock,sheep,s->sale_top+4)<HMBTN_LIVESTOCK
        : hmbtn_poultry_sale_slot(s->poultry,s->sale_top+4)<HMBTN_HENS;
    if(more
            && !hmbtn_ui_icon(ui,p,40,(tick/15)%2,288,128,error,size)) return false;
    for(unsigned row=0;row<4;++row) {
        unsigned slot=barn ? hmbtn_livestock_sale_slot(s->livestock,sheep,s->sale_top+row)
            : hmbtn_poultry_sale_slot(s->poultry,s->sale_top+row);
        if(slot==(barn ? HMBTN_LIVESTOCK : HMBTN_HENS)) break;
        const HmbtnFarmAnimal *a=barn ? &s->livestock->animals[slot] : NULL;
        const HmbtnHen *h=barn ? NULL : &s->poultry->hens[slot];const int y=48+(int)row*24;
        const unsigned day=barn ? a->day : h->day,age=barn ? a->age : h->age,
            season=barn ? a->season : h->season,affection=barn ? a->affection : h->affection;
        const bool prize=barn ? a->prize_winner : h->prize;
        const unsigned icon=sheep && a->days_since_shearing<7 ? 3u : species_icon;
        uint16_t date[14]={prize ? 0x90 : 0xFFD,0xFFD,0xFFD,0xFFD,0x71,
            (uint16_t)(0x40+(day+1)/10),(uint16_t)(0x40+(day+1)%10),0xFFD,0,0x26,0x24,
            (uint16_t)(0x40+(age%100)/10),(uint16_t)(0x40+age%10),0xFFFF};
        if(!number(ui,p,s->sale_top+row+1,2,NULL,32,y+6,error,size)
                || !hmbtn_ui_icon(ui,p,39,icon,64,y,error,size)
                || !hmbtn_ui_text(ui,p,barn ? a->name : h->name,HMBTN_NAME_CHARS+1,90,y,0,3,error,size)
                || !hmbtn_ui_text(ui,p,date,WORDS(date),176,y,0,3,error,size)
                || !hmbtn_ui_icon(ui,p,43,season,192,y,error,size)) return false;
        for(unsigned heart=0;heart<affection/25 && heart<10;++heart)
            if(!hmbtn_ui_icon(ui,p,42,0,160-(int)heart*8,y+12,error,size)) return false;
    }
    return true;
}
bool hmbtn_feed_shop_ui_draw(const HmbtnFeedShop *s,const HmbtnEconomy *economy,const HmbtnRegistration *registration,
    HmbtnNewGameUI *ui,uint8_t *p,unsigned tick,char *error,size_t size)
{
    if(s->phase==HMBTN_SHOP_OFF || s->phase==HMBTN_SHOP_NAME || s->phase==HMBTN_SHOP_MAKER_CUE
            || s->phase==HMBTN_SHOP_RUCKSACK_CUE || s->phase==HMBTN_SHOP_RUCKSACK_UPGRADE) return true;
    if(s->blacksmith || s->rucksack || s->basket) {
        hmbtn_window_frame_draw(p,320,240,227,16,79,34);
        if(!hmbtn_ui_text(ui,p,shop_money_title,WORDS(shop_money_title),231,20,0,3,error,size)
                || !number(ui,p,economy->gold,8,shop_gold_unit,231,34,error,size)) return false;
        if(s->rucksack || s->basket) return blacksmith_answer(s,ui,p,error,size);
        if(s->phase==HMBTN_SHOP_GREETING || s->phase==HMBTN_SHOP_LEAVE) return true;
        if(s->upgrade) return blacksmith_answer(s,ui,p,error,size);
        const bool tools=s->phase==HMBTN_SHOP_TOOL_LIST
            || ((s->phase==HMBTN_SHOP_INFO || s->phase==HMBTN_SHOP_RESULT) && s->after_result==HMBTN_SHOP_TOOL_LIST);
        const unsigned rows=tools ? hmbtn_blacksmith_tool_count(s) : hmbtn_blacksmith_rows(s);
        hmbtn_window_frame_draw(p,320,240,16,16,208,8+rows*14);
        if(!tools) {
            unsigned selected=0;
            for(unsigned i=0;i<rows;++i) {
                const unsigned choice=hmbtn_blacksmith_row(s,i);
                const uint16_t *labels[]={shop_improve_tool,shop_buy_tool,shop_mayonnaise_maker,shop_cheese_maker,shop_yarn_maker,shop_girls_present};
                const unsigned words[]={WORDS(shop_improve_tool),WORDS(shop_buy_tool),WORDS(shop_mayonnaise_maker),WORDS(shop_cheese_maker),WORDS(shop_yarn_maker),WORDS(shop_girls_present)};
                if(choice==s->selection) selected=i;
                if(!hmbtn_ui_text(ui,p,labels[choice],words[choice],30,20+(int)i*14,0,3,error,size)) return false;
            }
            if(!text_cursor(ui,p,20,20+(int)selected*14,error,size)) return false;
            return blacksmith_answer(s,ui,p,error,size);
        }
        for(unsigned i=0;i<rows;++i) {
            const HmbtnSeedShopEntry *entry=hmbtn_blacksmith_tool_entry(s,i);
            if(!hmbtn_ui_text(ui,p,entry->name,entry->name_words,30,20+(int)i*14,0,3,error,size)
                    || !number(ui,p,(unsigned)hmbtn_farm_item(entry->item)->buy_price,5,shop_gold_unit,172,20+(int)i*14,error,size)) return false;
        }
        return !rows || text_cursor(ui,p,20,20+(int)s->sale_cursor*14,error,size);
    }
    if(s->carpenter) {
        hmbtn_window_frame_draw(p,320,240,227,16,79,34);
        if(!hmbtn_ui_text(ui,p,shop_money_title,WORDS(shop_money_title),231,20,0,3,error,size)
                || !number(ui,p,economy->gold,8,shop_gold_unit,231,34,error,size)) return false;
        if(s->phase==HMBTN_SHOP_GREETING || s->phase==HMBTN_SHOP_LEAVE) return true;
        const unsigned rows=hmbtn_carpenter_shop_rows(s);
        hmbtn_window_frame_draw(p,320,240,16,16,208,8+rows*14);
        if(!hmbtn_ui_text(ui,p,shop_buy_lumber,WORDS(shop_buy_lumber),30,20,0,3,error,size)
                || !text_cursor(ui,p,20,20+(int)s->selection*14,error,size)) return false;
        const unsigned stage=hmbtn_construction_next(s->construction,s->poultry);
        const uint16_t *label=stage==4 ? shop_hothouse : stage==2 ? shop_enlarge_barn : stage==1 || stage==3 ? shop_enlarge_own_house : shop_enlarge_hen_house;
        const unsigned words=stage==4 ? WORDS(shop_hothouse) : stage==2 ? WORDS(shop_enlarge_barn) : stage==1 || stage==3 ? WORDS(shop_enlarge_own_house) : WORDS(shop_enlarge_hen_house);
        if(rows>1 && !hmbtn_ui_text(ui,p,label,words,30,34,0,3,error,size)) return false;
        if(s->phase==HMBTN_SHOP_BUILD_CONFIRM && !s->closing && s->message.phase==HMBTN_MESSAGE_WAIT_END) {
            hmbtn_window_frame_draw(p,320,240,200,112,48,36);
            return hmbtn_ui_text(ui,p,shop_yes,WORDS(shop_yes),216,116,0,3,error,size)
                && hmbtn_ui_text(ui,p,shop_no,WORDS(shop_no),216,130,0,3,error,size)
                && text_cursor(ui,p,208,116+(int)s->answer*14,error,size);
        }
        const bool quantity=s->phase==HMBTN_SHOP_COUNT
            || ((s->phase==HMBTN_SHOP_INFO || s->phase==HMBTN_SHOP_RESULT) && s->after_result==HMBTN_SHOP_COUNT);
        if(!quantity) return true;
        hmbtn_window_frame_draw(p,320,240,16,56,116,48);
        return hmbtn_ui_text(ui,p,shop_lumber,WORDS(shop_lumber),20,60,0,3,error,size)
            && number(ui,p,s->quantity,3,shop_units,48,74,error,size)
            && hmbtn_ui_icon(ui,p,13,(tick/15)%2,20,74,error,size)
            && hmbtn_ui_icon(ui,p,12,(tick/15)%2,112,74,error,size)
            && number(ui,p,hmbtn_feed_shop_cost(s),5,shop_gold_unit,32,88,error,size);
    }
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
        if(!text_cursor(ui,p,20,20+(int)s->selection*14,error,size)) return false;
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
            && text_cursor(ui,p,204,132+(int)s->answer*14,error,size);
    }
    const uint16_t *name=s->merchant==15 ? shop_fodder : shop_chicken_feed;
    const unsigned length=s->merchant==15 ? WORDS(shop_fodder) : WORDS(shop_chicken_feed);
    hmbtn_window_frame_draw(p,320,240,227,16,79,34);
    if(!hmbtn_ui_text(ui,p,shop_money_title,WORDS(shop_money_title),231,20,0,3,error,size)
            || !number(ui,p,economy->gold,8,shop_gold_unit,231,34,error,size)) return false;
    if(s->phase==HMBTN_SHOP_GREETING || s->phase==HMBTN_SHOP_LEAVE) return true;
    const bool hens=s->merchant==18 && s->poultry;
    const bool livestock=s->merchant==15 && s->livestock;
    const unsigned rows=livestock ? hmbtn_barn_shop_rows(s) : (hens ? 3u : 1u)+s->medicine_available;
    unsigned selected=s->selection==3 ? rows-1 : s->selection;
    if(livestock) { selected=0;while(selected<rows && hmbtn_barn_shop_row(s,selected)!=s->selection) ++selected; }
    hmbtn_window_frame_draw(p,320,240,16,16,208,8+rows*14);
    if(!hmbtn_ui_text(ui,p,name,length,34,20,0,3,error,size)
            || !text_cursor(ui,p,23,20+(int)selected*14,error,size)) return false;
    if(hens && !hmbtn_ui_text(ui,p,shop_buy_chicken,WORDS(shop_buy_chicken),34,34,0,3,error,size)) return false;
    if(hens && !hmbtn_ui_text(ui,p,shop_sell_chicken,WORDS(shop_sell_chicken),34,48,0,3,error,size)) return false;
    if(livestock) for(unsigned row=1;row<rows;++row) {
        const unsigned choice=hmbtn_barn_shop_row(s,row);const int y=20+(int)row*14;
        const uint16_t *label=NULL;unsigned words=0,item=0;
        switch(choice) {
        case 8:label=shop_cm_potion;words=WORDS(shop_cm_potion);item=0x1f;break;
        case 10:label=shop_bell;words=WORDS(shop_bell);item=0x1d;break;
        case 9:label=shop_sm_potion;words=WORDS(shop_sm_potion);item=0x20;break;
        case 4:label=shop_buy_cow;words=WORDS(shop_buy_cow);item=0x161;break;
        case 5:label=shop_buy_sheep;words=WORDS(shop_buy_sheep);item=0x165;break;
        case 6:label=shop_sell_cow;words=WORDS(shop_sell_cow);break;
        case 7:label=shop_sell_sheep;words=WORDS(shop_sell_sheep);break;
        case 3:label=shop_animal_medicine;words=WORDS(shop_animal_medicine);item=0x1e;break;
        }
        if(!label || !hmbtn_ui_text(ui,p,label,words,34,y,0,3,error,size)
                || (item && !number(ui,p,(unsigned)hmbtn_farm_item(item)->buy_price,5,shop_gold_unit,172,y,error,size))) return false;
    }
    if(!livestock && s->medicine_available && !hmbtn_ui_text(ui,p,shop_animal_medicine,WORDS(shop_animal_medicine),34,
            hens ? 62 : 34,0,3,error,size)) return false;
    /* shop_menu_draw prints each item's unit price beside its label. */
    const HmbtnFarmItem *feed=hmbtn_farm_item(s->merchant==15 ? 0x68 : 0x69);
    const HmbtnFarmItem *chicken=hmbtn_farm_item(0x109),*medicine=hmbtn_farm_item(0x1E);
    if(!feed || !chicken || !medicine
            || !number(ui,p,(unsigned)feed->buy_price,5,shop_gold_unit,172,20,error,size)) return false;
    if(hens && !number(ui,p,(unsigned)chicken->buy_price,5,shop_gold_unit,172,34,error,size)) return false;
    if(!livestock && s->medicine_available && !number(ui,p,(unsigned)medicine->buy_price,5,shop_gold_unit,
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
