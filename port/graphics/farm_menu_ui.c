/* Native status_pages.c and storage_swap.c presentation using original packs. */
#include "farm_menu_ui.h"
#include "cooking.h"
#include "resource_file.h"
#include "window_frame.h"
#include "poultry.h"
#include "livestock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "farm_menu_labels.inc"
#define WORDS(a) (unsigned)(sizeof(a)/sizeof(*(a)))
struct HmbtnFarmMenuUI { uint8_t *pack;HmbtnImageAtlas *atlas;HmbtnAnimation art[5]; };
HmbtnFarmMenuUI *hmbtn_farm_menu_ui_create(void) { return calloc(1,sizeof(HmbtnFarmMenuUI)); }
void hmbtn_farm_menu_ui_destroy(HmbtnFarmMenuUI *u) { if(u) { free(u->pack);free(u->atlas);free(u); } }
static bool load(HmbtnFarmMenuUI *u,const char *root,char *error,size_t size)
{
    if(u->pack) return true;
    uint8_t *pack=malloc(0x8e90);HmbtnImageAtlas *atlas=malloc(sizeof(*atlas));
    if(!pack || !atlas) { free(pack);free(atlas);snprintf(error,size,"Cannot allocate original status pack");return false; }
    static const unsigned offsets[6]={0,0xa88,0x1500,0x1f64,0x2bb0,0x2d74};
    bool ok=hmbtn_resource_read(root,"A_FILE/14_Status.Bin",0,pack,0x8e90,error,size)
        && hmbtn_bank_decode(pack+0x2d74,0x8e90-0x2d74,atlas,error,size);
    for(unsigned i=0;i<5 && ok;++i) ok=hmbtn_animation_open(&u->art[i],pack+offsets[i],offsets[i+1]-offsets[i],error,size);
    if(!ok) { free(pack);free(atlas);return false; }
    u->pack=pack;u->atlas=atlas;return true;
}
static bool sprite(HmbtnFarmMenuUI *u,unsigned art,unsigned anim,unsigned frame,int x,int y,uint8_t *p,char *error,size_t size)
{
    const HmbtnAnimationPlayer player={(uint16_t)anim,(uint16_t)frame,0};
    return hmbtn_animation_draw(&u->art[art],u->atlas,&player,x,y,p,320,240,320*240*4,error,size);
}
static bool number(HmbtnNewGameUI *shared,uint8_t *p,unsigned value,unsigned width,int suffix,int x,int y,char *error,size_t size)
{
    uint16_t text[16];if(width>12) return false;
    for(unsigned i=0;i<width;++i) { text[width-1-i]=(uint16_t)(0x40+value%10);value/=10; }
    for(unsigned i=0;i+1<width && text[i]==0x40;++i) text[i]=0xffd;
    unsigned count=width;if(suffix>=0) text[count++]=(uint16_t)suffix;
    text[count++]=0xffff;
    return hmbtn_ui_text(shared,p,text,count,x,y,0,3,error,size);
}
static void rect(uint8_t *p,int x,int y,unsigned w,unsigned h,unsigned rgb)
{
    for(unsigned dy=0;dy<h;++dy) for(unsigned dx=0;dx<w;++dx) {
        int px=x+(int)dx,py=y+(int)dy;if(px<0 || px>=320 || py<0 || py>=240) continue;
        uint8_t *out=p+(py*320+px)*4;
        out[0]=(uint8_t)rgb;out[1]=(uint8_t)(rgb>>8);out[2]=(uint8_t)(rgb>>16);out[3]=255;
    }
}
static bool background(HmbtnNewGameUI *shared,unsigned tick,uint8_t *p,char *error,size_t size)
{
    static const unsigned order[4]={0,2,1,3};unsigned frame=0;
    for(int y=0;y<264;y+=24) for(int x=-12;x<344;x+=24) {
        frame=(frame+1)&3u;
        if(!hmbtn_ui_icon(shared,p,29,order[frame],x-(int)((tick/4)%24),y-(int)((tick/4)%24),error,size)) return false;
    }
    return true;
}
static bool summary(HmbtnFarmMenuUI *u,HmbtnNewGameUI *shared,const HmbtnFarmMenu *m,
    const HmbtnHouseInteraction *h,const HmbtnRegistration *g,uint8_t *p,char *error,size_t size)
{
    const HmbtnFarmStatistics *s=&m->stats;
    if(!sprite(u,0,0,0,24,48,p,error,size)
        || !hmbtn_ui_text(shared,p,g->player_name,HMBTN_NAME_CHARS+1,48,50,0,3,error,size)) return false;
    const unsigned icons[3]={11,12,10},values[3]={hmbtn_farm_menu_building_score(m),hmbtn_cooking_record_count(m->cooking),m->economy->gold};
    const int xs[3]={40,40,140},ys[3]={72,96,48};
    for(unsigned i=0;i<3;++i) if(!sprite(u,3,icons[i],0,xs[i],ys[i],p,error,size)
        || !number(shared,p,values[i],i==2 ? 8 : 3,i==0 ? farm_glyph_8f[0] : i==1 ? farm_pieces[0] : farm_gold[0],xs[i]+24,ys[i]+4,error,size)) return false;
    for(unsigned i=0;i<s->power_berries && i<10;++i) if(!sprite(u,3,5,0,120-(int)i*8,60,p,error,size)) return false;
    if(g->dog_name[0]!=0xffff) {
        if(!sprite(u,1,0,0,24,168,p,error,size)
            || !hmbtn_ui_text(shared,p,g->dog_name,HMBTN_NAME_CHARS+1,48,170,0,3,error,size)) return false;
        for(unsigned i=0;i<s->dog_affection/25 && i<10;++i) if(!sprite(u,3,4,0,120-(int)i*8,184,p,error,size)) return false;
    }
    const unsigned list_art[5]={1,1,1,1,3},list_anim[5]={4,2,3,5,22};
    const unsigned list_count[5]={hmbtn_farm_menu_animal_count(m,0),hmbtn_farm_menu_animal_count(m,1),hmbtn_farm_menu_animal_count(m,2),s->fish,s->discarded};
    for(unsigned i=0;i<5;++i) if(!sprite(u,list_art[i],list_anim[i],0,140,72+(int)i*24,p,error,size)
        || !number(shared,p,list_count[i]>99 ? 99 : list_count[i],2,-1,164,76+(int)i*24,error,size)) return false;
    const unsigned feed_anim[4]={8,7,6,9},stock[4]={h->inventory->supplies[2],h->inventory->supplies[0],h->inventory->supplies[3],h->inventory->supplies[1]};
    const int feed_y[4]={72,96,144,168};
    for(unsigned i=0;i<4;++i) if(!sprite(u,3,feed_anim[i],0,212,feed_y[i],p,error,size)
        || !number(shared,p,stock[i]>999 ? 999 : stock[i],3,-1,236,feed_y[i]+4,error,size)) return false;
    const unsigned score=hmbtn_farm_menu_building_score(m)/10+hmbtn_cooking_record_count(m->cooking)/8+s->dog_affection/25+hmbtn_farm_menu_animal_count(m,0)+hmbtn_farm_menu_animal_count(m,1)+hmbtn_farm_menu_animal_count(m,2);
    return number(shared,p,score,3,farm_glyph_8f[0],186,26,error,size);
}
static bool chickens(HmbtnFarmMenuUI *u,HmbtnNewGameUI *shared,const HmbtnFarmMenu *m,
    uint8_t *p,char *error,size_t size)
{
    if(!m->poultry) { snprintf(error,size,"Chicken status has no session records");return false; }
    if(!sprite(u,1,15,0,186,20,p,error,size)
            || !hmbtn_ui_text(shared,p,farm_name_panel_title,WORDS(farm_name_panel_title),210,26,0,3,error,size)) return false;
    if(m->list_top && !sprite(u,3,1,m->cycle/30,288,48,p,error,size)) return false;
    if(hmbtn_poultry_status_slot(m->poultry,m->list_top+7)<HMBTN_HENS
            && !sprite(u,3,3,m->cycle/30,288,200,p,error,size)) return false;
    for(unsigned row=0;row<7;++row) {
        const unsigned slot=hmbtn_poultry_status_slot(m->poultry,m->list_top+row);
        if(slot==HMBTN_HENS) break;
        const HmbtnHen *h=&m->poultry->hens[slot];const bool chick=h->stage==HMBTN_HEN_CHICK;
        const int y=48+(int)row*24;const unsigned ordinal=m->list_top+row+1;
        /* status_draw_item_values uses the stored day directly (the shop's
         * similar formatter adds one). Retain the source's distinction. */
        const uint16_t date[14]={h->prize ? 0x90 : 0xFFD,0xFFD,0xFFD,0xFFD,0x71,
            h->day<10 ? 0xFFD : (uint16_t)(0x40+(h->day%100)/10),(uint16_t)(0x40+h->day%10),0xFFD,0,0x26,0x24,
            h->age<10 ? 0xFFD : (uint16_t)(0x40+(h->age%100)/10),(uint16_t)(0x40+h->age%10),0xFFFF};
        if(!number(shared,p,ordinal,ordinal<10 ? 1 : 2,-1,24,y+4,error,size)
                || !sprite(u,1,chick ? 14 : 15,!chick && h->days_sick ? 1 : 0,48,y,p,error,size)
                || !hmbtn_ui_text(shared,p,h->name,HMBTN_NAME_CHARS+1,72,y+2,0,3,error,size)
                || !hmbtn_ui_text(shared,p,date,WORDS(date),176,y+4,0,3,error,size)
                || !hmbtn_ui_icon(shared,p,43,h->season,192,y+4,error,size)) return false;
        for(unsigned i=0;i<h->affection/25 && i<10;++i)
            if(!sprite(u,3,4,0,144-(int)i*8,y+16,p,error,size)) return false;
    }
    return true;
}
static bool livestock(HmbtnFarmMenuUI *u,HmbtnNewGameUI *shared,const HmbtnFarmMenu *m,
    uint8_t *p,char *error,size_t size)
{
    const bool sheep=m->page==4;
    if(!sprite(u,1,sheep ? 12 : 8,0,186,20,p,error,size)
            || !hmbtn_ui_text(shared,p,farm_name_panel_title,WORDS(farm_name_panel_title),210,26,0,3,error,size)) return false;
    if(m->list_top && !sprite(u,3,1,m->cycle/30,288,48,p,error,size)) return false;
    if(hmbtn_livestock_slot(m->livestock,sheep,m->list_top+7)<HMBTN_LIVESTOCK
            && !sprite(u,3,3,m->cycle/30,288,200,p,error,size)) return false;
    for(unsigned row=0;row<7;++row) {
        const unsigned slot=hmbtn_livestock_slot(m->livestock,sheep,m->list_top+row);if(slot==HMBTN_LIVESTOCK) break;
        const HmbtnFarmAnimal *a=&m->livestock->animals[slot];const unsigned kind=hmbtn_livestock_kind(a);
        const unsigned icon=sheep ? (kind==0x164 ? 10 : 12) : kind==0x160 ? 6 : kind==0x161 ? 7 : 8;
        const int y=48+(int)row*24;const unsigned day=a->day+(sheep ? 1u : 0u);
        const uint16_t date[]={0xffd,0xffd,0xffd,0xffd,0x71,day<10 ? 0xffd : (uint16_t)(0x40+day/10),
            (uint16_t)(0x40+day%10),0xffd,0,0x26,0x24,a->age<10 ? 0xffd : (uint16_t)(0x40+(a->age%100)/10),(uint16_t)(0x40+a->age%10),0xffff};
        if(!number(shared,p,m->list_top+row+1,2,-1,24,y+4,error,size)
                || !sprite(u,1,icon,0,48,y,p,error,size)
                || !hmbtn_ui_text(shared,p,a->name,HMBTN_NAME_CHARS+1,72,y+2,0,3,error,size)
                || !hmbtn_ui_text(shared,p,date,WORDS(date),176,y+4,0,3,error,size)
                || !hmbtn_ui_icon(shared,p,43,a->season,192,y+4,error,size)) return false;
        for(unsigned i=0;i<a->affection/25 && i<10;++i) if(!sprite(u,3,4,0,144-(int)i*8,y+16,p,error,size)) return false;
    }
    return true;
}
static bool status(HmbtnFarmMenuUI *u,HmbtnNewGameUI *shared,const HmbtnFarmMenu *m,
    const HmbtnHouseInteraction *h,const HmbtnFarmAction *a,const HmbtnRegistration *g,uint8_t *p,char *error,size_t size)
{
    hmbtn_window_frame_draw(p,320,240,16,32,288,192);hmbtn_window_frame_draw(p,320,240,60,16,200,32);
    uint16_t title[16];unsigned n=0;
    while(n<HMBTN_NAME_CHARS && g->farm_name[n]!=0xffff) { title[n]=g->farm_name[n];++n; }
    title[n++]=0xffd;
    for(unsigned i=0;i<WORDS(farm_farm) && n<16;++i) title[n++]=farm_farm[i];
    if(!hmbtn_ui_text(shared,p,title,n,62,26,0,3,error,size)
        || !sprite(u,3,0,m->cycle/30,32,24,p,error,size)
        || !sprite(u,3,2,m->cycle/30,264,24,p,error,size)) return false;
    if(m->page==0) return summary(u,shared,m,h,g,p,error,size);
    if(m->page==1) {
        static const unsigned icons[21]={1,0,8,12,5,15,17,6,3,7,10,14,18,16,4,9,2,11,13,20,19};
        if(!hmbtn_ui_text(shared,p,farm_amount_shipped,WORDS(farm_amount_shipped),186,20,0,3,error,size)) return false;
        for(unsigned i=0;i<21;++i) {
            const int x=36+(int)(i/7)*88,y=50+(int)(i%7)*24;
            if(!sprite(u,2,icons[i],0,x,y,p,error,size)
                || !number(shared,p,m->economy->shipments[i]>999 ? 999 : m->economy->shipments[i],3,-1,x+30,y+6,error,size)) return false;
        }
    } else if(m->page==2) {
        return chickens(u,shared,m,p,error,size);
    } else if(m->page==3 || m->page==4) {
        return livestock(u,shared,m,p,error,size);
    } else if(m->page==5) {
        const uint16_t *names[7]={farm_chef,farm_nappy,farm_timid,farm_hoggy,farm_staid,farm_bold,farm_aqua};
        const unsigned lengths[7]={WORDS(farm_chef),WORDS(farm_nappy),WORDS(farm_timid),WORDS(farm_hoggy),WORDS(farm_staid),WORDS(farm_bold),WORDS(farm_aqua)};
        if(!hmbtn_ui_text(shared,p,farm_help,WORDS(farm_help),186,26,0,3,error,size)) return false;
        for(unsigned i=0;i<7;++i) {
            const int y=48+(int)i*24;
            if(!number(shared,p,i+1,1,-1,24,y+4,error,size) || !sprite(u,0,7,i,40,y,p,error,size)
                || !hmbtn_ui_text(shared,p,names[i],lengths[i],64,y+2,0,3,error,size)
                || !hmbtn_ui_text(shared,p,farm_status_free,WORDS(farm_status_free),198,y+4,0,3,error,size)) return false;
            for(unsigned j=0;j<m->stats.helper_affection[i]/25 && j<10;++j)
                if(!sprite(u,3,4,0,136-(int)j*8,y+16,p,error,size)) return false;
        }
    } else if(m->page==6) {
        if(!hmbtn_ui_text(shared,p,farm_earnings_expenses,WORDS(farm_earnings_expenses),178,20,0,3,error,size)
            || !hmbtn_ui_text(shared,p,farm_this_months_accounts,WORDS(farm_this_months_accounts),32,64,0,3,error,size)
            || !number(shared,p,m->economy->ledger[0][0],7,farm_gold[0],186,64,error,size)
            || !number(shared,p,m->economy->ledger[0][1],7,farm_gold[0],186,78,error,size)) return false;
        for(unsigned i=0;i<4;++i) {
            const int y=104+(int)i*32;
            if(!hmbtn_ui_icon(shared,p,43,(h->clock->season+4-i)%4,24,y,error,size)) return false;
            rect(p,40,y+2,129,9,0xe0b888);rect(p,41,y+3,127,7,0xc8a060);
            const unsigned income=m->economy->ledger[i][0]%10000*64/10000,expense=m->economy->ledger[i][1]%10000*64/10000;
            rect(p,105-(int)expense,y+3,expense,7,0xd14070);rect(p,105,y+3,income,7,0x0090c8);
            const uint16_t *grades[9]={farm_go_getter,farm_spend_too_much,farm_idler,farm_satisfactory,
                farm_you_should_save,farm_play_too_much,farm_hard_worker,farm_keep_working,farm_try_harder};
            const unsigned lengths[9]={WORDS(farm_go_getter),WORDS(farm_spend_too_much),WORDS(farm_idler),WORDS(farm_satisfactory),
                WORDS(farm_you_should_save),WORDS(farm_play_too_much),WORDS(farm_hard_worker),WORDS(farm_keep_working),WORDS(farm_try_harder)};
            const unsigned income_grade=m->economy->ledger[i][0]>=5000 ? 0u : m->economy->ledger[i][0]>=2000 ? 1u : 2u,
                expense_grade=m->economy->ledger[i][1]>=5000 ? 0u : m->economy->ledger[i][1]>=2000 ? 1u : 2u,grade=expense_grade*3+income_grade;
            const bool empty=!m->economy->ledger[i][0] && !m->economy->ledger[i][1];
            if(!hmbtn_ui_text(shared,p,empty ? farm_nothing : grades[grade],empty ? WORDS(farm_nothing) : lengths[grade],172,y,0,3,error,size)) return false;
        }
    } else if(m->page==7) {
        static const unsigned icons[5]={5,4,6,7,8};
        static const unsigned threshold[5]={0,6000,18000,36000,65535};
        if(!hmbtn_ui_text(shared,p,farm_level,WORDS(farm_level),186,26,0,3,error,size)) return false;
        for(unsigned i=0;i<5;++i) {
            const int y=48+(int)i*34;unsigned band=1;
            while(band<4 && a->experience[i]>=threshold[band]) ++band;
            const unsigned percent=(a->experience[i]-threshold[band-1])*100/(threshold[band]-threshold[band-1])+(band-1)*100;
            if(!sprite(u,4,icons[i],0,40,y,p,error,size)
                || !number(shared,p,percent,4,farm_glyph_8f[0],215,y+12,error,size)) return false;
            rect(p,80,y+8,132,9,0xe0b888);rect(p,81,y+9,130,7,0xc8a060);
            /* Initial tools are level 1; XP beyond 6000 caps their gauge. */
            rect(p,81,y+9,a->experience[i]>=6000 ? 130 : a->experience[i]*130/6000,7,0xb89000);
            for(unsigned j=1;j<5 && a->experience[i]>=threshold[j];++j)
                if(!sprite(u,4,j-1,0,72+(int)(j-1)*8,y+20,p,error,size)) return false;
        }
    }
    return true;
}
bool hmbtn_farm_menu_ui_draw(HmbtnFarmMenuUI *u,HmbtnHouseUI *items,HmbtnNewGameUI *shared,
    const HmbtnFarmMenu *m,const HmbtnHouseInteraction *h,const HmbtnFarmAction *a,
    const HmbtnRegistration *g,const char *root,unsigned tick,uint8_t *p,char *error,size_t size)
{
    if(m->mode==HMBTN_MENU_NONE) return true;
    if(m->mode==HMBTN_MENU_STATUS) {
        if(m->phase>=2 && m->phase<=4) {
            if(!load(u,root,error,size) || !background(shared,tick,p,error,size)
                || !status(u,shared,m,h,a,g,p,error,size)) return false;
        }
        unsigned brightness=m->phase==1 || m->phase==4 ? 10-m->fade : m->phase==2 || m->phase==5 ? m->fade : 10;
        for(unsigned i=0;i<320*240*4;++i) if(i%4!=3) p[i]=(uint8_t)(p[i]*brightness/10);
        return true;
    }
    for(unsigned i=0;i<320*240*4;++i) if(i%4!=3) p[i]=(uint8_t)((p[i]>>4)<<3);
    hmbtn_window_frame_draw(p,320,240,25,16,96,74);hmbtn_window_frame_draw(p,320,240,121,16,174,74);
    hmbtn_window_frame_draw(p,320,240,25,90,96,74);hmbtn_window_frame_draw(p,320,240,121,90,174,74);
    if(!hmbtn_ui_text(shared,p,farm_swap_equip,WORDS(farm_swap_equip),29,20,0,3,error,size)
        || !hmbtn_ui_text(shared,p,farm_swap_rucksack,WORDS(farm_swap_rucksack),29,94,0,3,error,size)) return false;
    for(unsigned i=0;i<18;++i) {
        const HmbtnStoredTool *item=hmbtn_inventory_slot_const(h->inventory,i);
        if(!item) continue;
        const HmbtnStorageSpot *spot=hmbtn_storage_spot(i);
        if(!hmbtn_house_ui_item(items,shared,root,p,item->id,spot->x,spot->y,error,size)) return false;
        const HmbtnFarmItem *info=hmbtn_farm_item(item->id);
        if(item->id && info && info->category==2 && !number(shared,p,item->count,2,-1,spot->x+12,spot->y+20,error,size)) return false;
    }
    const HmbtnStorageSpot *spot=hmbtn_storage_spot(m->cursor);
    if(m->marked!=255) {
        const HmbtnStorageSpot *picked=hmbtn_storage_spot(m->marked);
        if(!hmbtn_ui_icon(shared,p,11,0,picked->x,picked->y,error,size)) return false;
    }
    return hmbtn_ui_icon(shared,p,11,1,spot->x+12,spot->y+24,error,size);
}
