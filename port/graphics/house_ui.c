#include "house_ui.h"
#include "inventory.h"
#include "resource_file.h"
#include "window_frame.h"
#include "psx_data.h"
#include <stdlib.h>
#include <stdio.h>
#include "house_labels.inc"
typedef struct ToolArt {
    uint8_t bytes[2048];
    HmbtnAnimation animation;
    HmbtnImageAtlas *atlas;
    unsigned id;
    uint64_t used;
} ToolArt;
struct HmbtnHouseUI { ToolArt tools[18];uint64_t serial; };
HmbtnHouseUI *hmbtn_house_ui_create(void) { return calloc(1,sizeof(HmbtnHouseUI)); }
void hmbtn_house_ui_destroy(HmbtnHouseUI *u)
{
    if(u) { for(unsigned i=0;i<18;++i) free(u->tools[i].atlas); free(u); }
}
static bool number(HmbtnNewGameUI *shared,uint8_t *p,unsigned value,int x,int y,
    unsigned palette,char *error,size_t error_size)
{
    const uint16_t words[3]={value<10 ? 0xffd : (uint16_t)(0x40+value/10),
        (uint16_t)(0x40+value%10),0xffff};
    return hmbtn_ui_text(shared,p,words,3,x,y,1,palette,error,error_size);
}
/* Share the original item ANI/cache with world attachments and menus. */
static ToolArt *picture_art(HmbtnHouseUI *u,const char *root,unsigned id,char *error,size_t error_size)
{
    if(!id) { snprintf(error,error_size,"Missing item picture id");return NULL; }
    if(id>=380) { snprintf(error,error_size,"Item picture outside original table");return NULL; }
    unsigned i=0; while(i<18 && (!u->tools[i].atlas || u->tools[i].id!=id)) ++i;
    if(i==18) {
        i=0;for(unsigned j=0;j<18;++j) {
            if(!u->tools[j].atlas) { i=j;break; }
            if(u->tools[j].used<u->tools[i].used) i=j;
        }
        free(u->tools[i].atlas);u->tools[i]=(ToolArt){0};
    }
    ToolArt *a=&u->tools[i];
    if(!a->atlas) {
        a->atlas=malloc(sizeof(*a->atlas));
        if(!a->atlas) { snprintf(error,error_size,"Cannot allocate tool picture atlas"); return NULL; }
        if(!hmbtn_resource_read(root,"A_FILE/00_gf.bin",0x3C19000+id*0x800,a->bytes,2048,error,error_size)
                || !hmbtn_animation_open(&a->animation,a->bytes,256,error,error_size)
                || !hmbtn_bank_decode(a->bytes+256,1792,a->atlas,error,error_size)) {
            free(a->atlas);a->atlas=NULL;return NULL;
        }
        a->id=id;
    }
    a->used=++u->serial;
    return a;
}
bool hmbtn_house_ui_picture_art(HmbtnHouseUI *u,const char *root,unsigned id,
    const HmbtnAnimation **animation,const HmbtnImageAtlas **atlas,char *error,size_t error_size)
{
    ToolArt *a=picture_art(u,root,id,error,error_size);
    if(!a) return false;
    *animation=&a->animation;*atlas=a->atlas;return true;
}
static bool picture(HmbtnHouseUI *u,const char *root,uint8_t *p,unsigned id,int x,int y,bool centre,char *error,size_t error_size);
bool hmbtn_house_ui_item(HmbtnHouseUI *u,HmbtnNewGameUI *shared,const char *root,uint8_t *p,
    unsigned id,int x,int y,char *error,size_t error_size)
{
    if(!hmbtn_ui_icon(shared,p,11,2,x,y,error,error_size)) return false;
    return picture(u,root,p,id,x,y,true,error,error_size);
}
bool hmbtn_house_ui_picture(HmbtnHouseUI *u,const char *root,uint8_t *p,
    unsigned id,int x,int y,char *error,size_t error_size)
{
    return picture(u,root,p,id,x,y,false,error,error_size);
}
static bool picture(HmbtnHouseUI *u,const char *root,uint8_t *p,unsigned id,int x,int y,
    bool centre,char *error,size_t error_size)
{
    if(!id) return true;
    ToolArt *a=picture_art(u,root,id,error,error_size);
    if(!a) return false;
    const HmbtnAnimationPlayer frame={0};
    if(centre) {
        const HmbtnAnimation *ani=&a->animation;
        const uint8_t *f=ani->data+ani->section[4];
        if(hmbtn_read_le16(f)>=ani->count[1]) return true;
        const uint8_t *list=ani->data+ani->section[1]+hmbtn_read_le16(f)*4;
        if(!hmbtn_read_le16(list)) return true;
        const uint8_t *part=ani->data+ani->section[2]+hmbtn_read_le16(list+2);
        const uint8_t *img=ani->data+ani->section[0]+hmbtn_read_le16(part)*8;
        const int width=hmbtn_read_le16(img+4)*((part[8]&0x80) ? 2 : 4),height=hmbtn_read_le16(img+6);
        const unsigned rawx=(hmbtn_read_le16(part+4)+hmbtn_read_le16(f+2))&65535u,
            rawy=(hmbtn_read_le16(part+6)+hmbtn_read_le16(f+4))&65535u;
        x+=(32-width)/2-(rawx<32768 ? (int)rawx : (int)rawx-65536);
        y+=(32-height)/2-(rawy<32768 ? (int)rawy : (int)rawy-65536);
    }
    return hmbtn_animation_draw(&a->animation,a->atlas,&frame,x,y,p,320,240,320*240*4,error,error_size);
}
static void position(unsigned cell,int *x,int *y)
{
    *x=cell==0 ? 57 : 141+(int)((cell-1)%4)*34;
    *y=cell==0 ? 34 : cell<9 ? 20+(int)((cell-1)/4)*34 : 94+(int)((cell-9)/4)*34;
}
#define WORDS(a) (unsigned)(sizeof(a)/sizeof((a)[0]))
static bool clock_draw(HmbtnNewGameUI *shared,const HmbtnHouseInteraction *h,uint8_t *p,
    char *error,size_t error_size)
{
    const unsigned hour=h->clock->hour%12 ? h->clock->hour%12 : 12;
    const uint16_t time[]={h->clock->hour<12 ? 0 : 15,12,0xffd,
        hour<10 ? 0xffd : (uint16_t)(0x40+hour/10),(uint16_t)(0x40+hour%10),
        0xffd,(uint16_t)(0x40+h->clock->minute/10),0x40,0xffff};
    const uint16_t colon[]={0x66,0xffff};
    hmbtn_window_frame_draw(p,320,240,228,16,78,32);
    return hmbtn_ui_icon(shared,p,43,h->clock->season,232,19,error,error_size)
        && number(shared,p,h->clock->day+1,252,19,3,error,error_size)
        && hmbtn_ui_text(shared,p,weekday_text_codes+h->clock->weekday*3,3,276,19,0,
            h->clock->weekday==0 ? 4u : h->clock->weekday==6 ? 5u : 3u,error,error_size)
        && hmbtn_ui_text(shared,p,time,WORDS(time),232,33,0,3,error,error_size)
        && hmbtn_ui_text_dim(shared,p,colon,WORDS(colon),272,33,h->clock->minute%10>=5,error,error_size);
}
static bool options_draw(HmbtnNewGameUI *shared,const HmbtnHouseOptions *o,uint8_t *p,
    char *error,size_t error_size)
{
    /* Opening window remains behind the selected page (OT layers 23..25). */
    hmbtn_window_frame_draw(p,320,240,100,74,130,34);
    if(!hmbtn_ui_text(shared,p,house_how_to_operate_configuration,WORDS(house_how_to_operate_configuration),
            112,78,0,3,error,error_size)
            || !hmbtn_ui_icon(shared,p,11,1,108,84+(int)o->opening_cursor*14,error,error_size)) return false;
    if(o->page==1) {
        hmbtn_window_frame_draw(p,320,240,20,55,280,105);
        return hmbtn_ui_text(shared,p,house_how_to_operate_buttons,WORDS(house_how_to_operate_buttons),
                32,59,0,3,error,error_size)
            && hmbtn_ui_icon(shared,p,11,1,28+(int)o->manual_column*150,
                79+(int)o->manual_row*14,error,error_size);
    }
    if(o->page==2) {
        hmbtn_window_frame_draw(p,320,240,58,30,202,132);
        if(!hmbtn_ui_text(shared,p,house_configuration_screen,WORDS(house_configuration_screen),
                62,34,0,3,error,error_size)) return false;
        const uint16_t *labels[9]={house_type_1,house_type_2,house_type_3,
            house_on,house_off,house_stereo,house_mono,house_yes,house_no};
        const unsigned lengths[9]={WORDS(house_type_1),WORDS(house_type_2),WORDS(house_type_3),
            WORDS(house_on),WORDS(house_off),WORDS(house_stereo),WORDS(house_mono),WORDS(house_yes),WORDS(house_no)};
        for(unsigned i=0;i<9;++i) {
            const unsigned row=i<3 ? 0u : 1+(i-3)/2, col=i<3 ? i : (i-3)%2;
            const unsigned selected=row==0 ? o->layout : row==1 ? !o->vibration : row==2 ? !o->stereo : !o->clock;
            if(!hmbtn_ui_text_dim(shared,p,labels[i],lengths[i],70+(int)col*65,62+(int)row*28,
                    col!=selected,error,error_size)) return false;
        }
        return hmbtn_ui_icon(shared,p,11,1,66+(int)o->column*65,68+(int)o->row*28,error,error_size);
    }
    return true;
}
static bool menu_background(HmbtnNewGameUI *shared,uint8_t *p,unsigned tick,char *error,size_t size)
{
    const unsigned order[4]={0,2,1,3};const int scroll=(int)((tick/4)%24);unsigned frame=0;
    for(int y=0;y<264;y+=24) for(int x=-12;x<344;x+=24) {
        frame=(frame+1)&3u;
        if(!hmbtn_ui_icon(shared,p,29,order[frame],x-scroll,y-scroll,error,size)) return false;
    }
    return true;
}

/* The same original item art/font/window resources used by the storage screen.
 * Rules, selections and recipe records are read-only here. */
static bool kitchen_draw(HmbtnHouseUI *u,HmbtnNewGameUI *shared,const HmbtnHouseInteraction *h,
    const char *root,unsigned tick,uint8_t *p,char *error,size_t size)
{
    if(!menu_background(shared,p,tick,error,size)) return false;
    const HmbtnKitchenMenu *k=&h->kitchen;
    if(k->phase==HMBTN_KITCHEN_RECORDS) {
        hmbtn_window_frame_draw(p,320,240,8,16,304,148);
        const unsigned count=hmbtn_cooking_record_count(h->cooking),top=k->cursor/20*20;
        unsigned row=0;
        for(unsigned i=0;i<HMBTN_RECIPE_RECORDS;++i) if(h->cooking->records[i].id) {
            const unsigned n=row++;if(n<top || n>=top+20) continue;
            const uint16_t *name=hmbtn_cooking_dish_name(h->cooking->records[i].id);
            unsigned length=0;while(name && name[length]!=0xffff && length<16) ++length;
            const unsigned visible=n-top;
            if(name && !hmbtn_ui_text(shared,p,name,length,20+(int)(visible/10)*148,20+(int)(visible%10)*14,0,3,error,size)) return false;
        }
        return !count || hmbtn_ui_icon(shared,p,11,1,10+(int)((k->cursor-top)/10)*148,22+(int)((k->cursor-top)%10)*14,error,size);
    }
    if(k->phase==HMBTN_KITCHEN_RECORD_VIEW || k->phase==HMBTN_KITCHEN_RECIPE_CONFIRM
            || (k->phase==HMBTN_KITCHEN_DESCRIPTION && k->return_phase==HMBTN_KITCHEN_RECORDS)) {
        hmbtn_window_frame_draw(p,320,240,22,16,270,148);
        const uint16_t *name=k->record<HMBTN_RECIPE_RECORDS ? hmbtn_cooking_dish_name(h->cooking->records[k->record].id) : NULL;
        unsigned count=0;while(name && name[count]!=0xffff) ++count;
        if(name && !hmbtn_ui_text(shared,p,name,count,32,22,0,3,error,size)) return false;
        for(unsigned i=0;i<8;++i) if(!hmbtn_house_ui_item(u,shared,root,p,k->values[i],110+(int)(i%4)*38,64+(int)(i/4)*40,error,size)) return false;
        return true;
    }
    if(k->phase==HMBTN_KITCHEN_TOOLS || k->phase==HMBTN_KITCHEN_SEASONING) {
        const bool spice=k->phase==HMBTN_KITCHEN_SEASONING;
        hmbtn_window_frame_draw(p,320,240,22,16,270,148);
        if(!hmbtn_ui_text(shared,p,spice ? house_use : house_utensils,
                spice ? WORDS(house_use) : WORDS(house_utensils),30,22,0,3,error,size)) return false;
        if(spice) {
            const uint16_t *labels[5]={house_sugar,house_salt,house_vinegar,house_soy_sauce,house_miso_paste};
            const unsigned lengths[5]={WORDS(house_sugar),WORDS(house_salt),WORDS(house_vinegar),WORDS(house_soy_sauce),WORDS(house_miso_paste)};
            for(unsigned i=0;i<5;++i) if(!hmbtn_ui_text_dim(shared,p,labels[i],lengths[i],96,48+(int)i*20,
                    !(k->options&(1u<<(7+i))),error,size)) return false;
            return hmbtn_ui_icon(shared,p,11,1,80,50+(int)k->cursor*20,error,size);
        }
        for(unsigned i=0;i<(spice ? 5u : 8u);++i) {
            const bool owned=spice ? (h->cooking->utensils&0x80)!=0 : (h->cooking->utensils&(1u<<i))!=0;
            const unsigned id=spice ? hmbtn_cooking_option_item(7+i) : i<7 ? hmbtn_cooking_option_item(i) : 0x79;
            const int x=72+(int)(i%4)*44,y=46+(int)(i/4)*48;
            hmbtn_window_frame_draw(p,320,240,x-2,y-2,36,36);
            if(owned && !hmbtn_house_ui_item(u,shared,root,p,id,x,y,error,size)) return false;
            const bool marked=spice ? (k->options&(1u<<(7+i)))!=0 : i<7 ? (k->options&(1u<<i))!=0 : (k->options&0xf80)!=0;
            if(marked && !hmbtn_ui_icon(shared,p,11,0,x,y,error,size)) return false;
        }
        return hmbtn_ui_icon(shared,p,11,1,80+(int)(k->cursor%4)*44,70+(int)(k->cursor/4)*48,error,size);
    }
    if(k->phase==HMBTN_KITCHEN_ITEMS || k->phase==HMBTN_KITCHEN_CONFIRM
            || (k->phase==HMBTN_KITCHEN_DESCRIPTION && k->return_phase==HMBTN_KITCHEN_ITEMS)) {
        hmbtn_window_frame_draw(p,320,240,22,16,270,74);
        hmbtn_window_frame_draw(p,320,240,22,90,270,74);
        if(!hmbtn_ui_text(shared,p,house_ingredient_title,WORDS(house_ingredient_title),30,24,0,3,error,size)
                || !hmbtn_ui_text(shared,p,k->page ? house_ingredient_icebox : house_ingredient_rucksack,
                    k->page ? WORDS(house_ingredient_icebox) : WORDS(house_ingredient_rucksack),30,102,0,3,error,size)) return false;
        if(k->page && !number(shared,p,k->page,48,120,3,error,size)) return false;
        for(unsigned i=0;i<8;++i) {
            const int x=134+(int)(i%4)*34;
            if(!hmbtn_house_ui_item(u,shared,root,p,k->values[i],x,22+(int)(i/4)*34,error,size)) return false;
            const HmbtnStoredTool *item=hmbtn_cooking_source(h->inventory,k->page,i);
            if(item && !hmbtn_house_ui_item(u,shared,root,p,item->id,x,96+(int)(i/4)*34,error,size)) return false;
            if(item && item->id) for(unsigned j=0;j<8;++j) if(k->values[j]==item->id
                    && !hmbtn_ui_icon(shared,p,11,0,x,96+(int)(i/4)*34,error,size)) return false;
        }
        return hmbtn_ui_icon(shared,p,11,1,135+(int)(k->cursor%4)*34,103+(int)(k->cursor/4)*34,error,size);
    }
    if(k->phase==HMBTN_KITCHEN_RESULT && k->result.id) {
        const uint16_t *name=hmbtn_cooking_dish_name(k->result.id);unsigned n=0;
        while(name && name[n]!=0xffff) ++n;
        hmbtn_window_frame_draw(p,320,240,32,40,256,112);
        return hmbtn_house_ui_item(u,shared,root,p,k->result.id,144,88,error,size)
            && (!name || hmbtn_ui_text(shared,p,name,n,48,52,0,3,error,size));
    }
    return true; /* Questions are rendered by the existing original message panel. */
}

bool hmbtn_house_ui_draw(HmbtnHouseUI *u,HmbtnNewGameUI *shared,
    const HmbtnHouseInteraction *h,const char *root,unsigned tick,uint8_t *p,
    char *error,size_t error_size)
{
    if(h->mode==HMBTN_HOUSE_SLEEP) return true;
    if(h->options.clock && !clock_draw(shared,h,p,error,error_size)) return false;
    if(h->mode==HMBTN_HOUSE_FREE || h->mode==HMBTN_HOUSE_TOOLBOX_OPEN) return true;
    if(h->mode==HMBTN_HOUSE_CALENDAR || h->mode==HMBTN_HOUSE_TOOLBOX
            || h->mode==HMBTN_HOUSE_OPTIONS
            || (h->mode==HMBTN_HOUSE_BOOK && h->detail)) {
        for(unsigned i=0;i<320*240*4;++i) if(i%4!=3) p[i]=(uint8_t)((p[i]>>4)<<3);
    }
    if(h->mode==HMBTN_HOUSE_COOKING) return kitchen_draw(u,shared,h,root,tick,p,error,error_size);
    if(h->mode==HMBTN_HOUSE_OPTIONS) return options_draw(shared,&h->options,p,error,error_size);
    if(h->mode==HMBTN_HOUSE_BED) {
        static const unsigned rows[4]={0,2,4,5};
        hmbtn_window_frame_draw(p,320,240,16,16,172,88);
        return hmbtn_ui_text(shared,p,house_sleep_menu,WORDS(house_sleep_menu),18,18,0,3,error,error_size)
            && hmbtn_ui_icon(shared,p,11,1,22,24+(int)rows[h->bed_choice]*14,error,error_size);
    }
    if(h->mode==HMBTN_HOUSE_CALENDAR) {
        hmbtn_window_frame_draw(p,320,240,16,48,287,116);
        const unsigned offset=(h->clock->weekday+7-h->clock->day%7)%7;
        if(!number(shared,p,h->clock->year+1,31,52,3,error,error_size)
                || !hmbtn_ui_text(shared,p,house_year,5,57,52,1,3,error,error_size)
                || !hmbtn_ui_icon(shared,p,43,h->clock->season,142,52,error,error_size)
                || !hmbtn_ui_text(shared,p,season_text_codes+h->clock->season*6,6,158,52,1,3,error,error_size)) return false;
        for(unsigned i=0;i<7;++i) {
            if(!hmbtn_ui_text(shared,p,weekday_text_codes+i*3,3,22+(int)i*41,66,1,
                    i==0 ? 4u : i==6 ? 5u : 3u,error,error_size)) return false;
        }
        for(unsigned i=0;i<30;++i) {
            const unsigned cell=i+offset, col=cell%7;
            const int x=30+(int)col*41,y=80+(int)(cell/7)*14;
            if(!number(shared,p,i+1,x,y,col==0 ? 4u : col==6 ? 5u : 3u,error,error_size)) return false;
            if(hmbtn_house_calendar_message(h->clock->season,i)!=0x3dc
                    && !hmbtn_ui_icon(shared,p,30,1,x-4,y,error,error_size)) return false;
            if(i==h->clock->day && !hmbtn_ui_icon(shared,p,30,0,x-4,y,error,error_size)) return false;
        }
        const unsigned cell=h->calendar_day+offset;
        if(!hmbtn_ui_icon(shared,p,11,1,42+(int)(cell%7)*41,89+(int)(cell/7)*14,error,error_size)) return false;
    } else if(h->mode==HMBTN_HOUSE_TOOLBOX || (h->mode==HMBTN_HOUSE_BOOK && h->detail)) {
        if(!menu_background(shared,p,tick,error,error_size)) return false;
        hmbtn_window_frame_draw(p,320,240,25,16,105,74);
        hmbtn_window_frame_draw(p,320,240,121,16,174,74);
        hmbtn_window_frame_draw(p,320,240,25,90,270,74);
        if(!hmbtn_ui_text(shared,p,h->storage ? house_storage_rucksack : house_storage_equip,h->storage ? sizeof(house_storage_rucksack)/sizeof(*house_storage_rucksack) : 6,29,20,0,3,error,error_size)
                || !hmbtn_ui_text(shared,p,h->storage==0 ? house_storage_toolbox : h->storage==1 ? house_storage_cabinet : house_storage_icebox,h->storage==2 ? 7u : 8u,29,94,0,3,error,error_size)
                || !number(shared,p,h->page+1,40,110,3,error,error_size)
                || !hmbtn_ui_text(shared,p,house_slash,2,53,110,0,3,error,error_size)
                || !number(shared,p,8,66,110,3,error,error_size)) return false;
        for(unsigned cell=0;cell<17;++cell) {
            if(cell>hmbtn_inventory_capacity(h->inventory) && cell<9) continue;
            int x,y; position(cell,&x,&y);
            const HmbtnStoredTool *item=hmbtn_inventory_storage_slot_const(h->inventory,h->storage,cell,h->page);
            const unsigned id=item ? item->id : 0;
            if(!hmbtn_house_ui_item(u,shared,root,p,id,x,y,error,error_size)) return false;
            if(item && item->id && cell>=9 && h->storage && !number(shared,p,item->count,x+12,y+24,3,error,error_size)) return false;
        }
        int x,y;
        if(h->picked!=255 && (h->picked<9 || h->picked_page==h->page)) {
            position(h->picked,&x,&y);
            if(!hmbtn_ui_icon(shared,p,11,0,x,y,error,error_size)) return false;
        }
        position(h->cursor,&x,&y);
        if(!hmbtn_ui_icon(shared,p,11,1,x+12,y+24,error,error_size)
                || !hmbtn_ui_icon(shared,p,13,(tick/9)%4,125,144,error,error_size)
                || !hmbtn_ui_icon(shared,p,12,(tick/9)%4,277,144,error,error_size)) return false;
    }
    return true;
}
