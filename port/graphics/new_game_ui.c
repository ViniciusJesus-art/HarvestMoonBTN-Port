#include "new_game_ui.h"
#include "resource_file.h"
#include "psx_data.h"
#include "window_frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "name_labels.inc"

enum { UI_BLOCK_SIZE = 0x5800, UI_ANI_SIZE = 0x13dc, WIDTH = 320, HEIGHT = 240 };
struct HmbtnNewGameUI {
    HmbtnImageAtlas *atlas;
    HmbtnAnimation icons;
    HmbtnAnimationPlayer heart;
    uint8_t icons_block[UI_BLOCK_SIZE];
};
HmbtnNewGameUI *hmbtn_new_game_ui_create(const char *root, char *error, size_t error_size)
{
    HmbtnNewGameUI *u = calloc(1,sizeof(*u));
    uint8_t small[8192];
    if (!u) { snprintf(error,error_size,"Cannot allocate new-game UI"); return NULL; }
    u->atlas = malloc(sizeof(*u->atlas));
    if (!u->atlas) { snprintf(error,error_size,"Cannot allocate UI image atlas"); goto failed; }
    if (!hmbtn_resource_read(root,"A_FILE/00_gf.bin",0,small,sizeof(small),error,error_size)
            || !hmbtn_bank_decode(small,sizeof(small),u->atlas,error,error_size)
            || !hmbtn_resource_read(root,"A_FILE/00_gf.bin",0x25000,u->icons_block,UI_BLOCK_SIZE,error,error_size)
            || !hmbtn_animation_open(&u->icons,u->icons_block,UI_ANI_SIZE,error,error_size)
            || !hmbtn_bank_apply(u->icons_block+UI_ANI_SIZE,UI_BLOCK_SIZE-UI_ANI_SIZE,u->atlas,error,error_size)) goto failed;
    u->heart.animation = 7;
    return u;
failed:
    hmbtn_new_game_ui_destroy(u); return NULL;
}
void hmbtn_new_game_ui_destroy(HmbtnNewGameUI *u)
{
    if (u) { free(u->atlas); free(u); }
}
bool hmbtn_new_game_ui_load(HmbtnNewGameUI *u, const char *root, HmbtnNewGame *g,
    char *error, size_t error_size)
{
    if (!g->request || g->loaded == g->request) return true;
    uint8_t record[8192];
    if (!hmbtn_resource_read(root,"A_FILE/05_messege3.bin",g->request*8192,record,sizeof(record),error,error_size)
            || !hmbtn_message_open(&g->message,record,sizeof(record),error,error_size)) return false;
    if (hmbtn_read_le32(record) != UINT32_MAX
            && !hmbtn_bank_apply(record+4,0x1800-4,u->atlas,error,error_size)) return false;
    g->loaded = g->request;
    printf("New-game message: 0x%03x\n",g->loaded);
    fflush(stdout);
    return true;
}
static bool glyph(HmbtnNewGameUI *u,uint8_t *p,uint16_t code,int x,int y,unsigned palette,
    char *error,size_t error_size)
{
    static const unsigned rows[6]={254,250,250,252,253,251};
    if(code==0xffd || code>=256) return true;
    return hmbtn_texture_draw(u->atlas,896+(code&15)*2,(code>>4)*12,8,12,4,
        896,rows[palette],x,y,p,WIDTH,HEIGHT,error,error_size);
}
static bool text(HmbtnNewGameUI *u,uint8_t *p,const uint16_t *words,unsigned count,
    int x,int y,int spacing,unsigned palette,bool message,char *error,size_t error_size)
{
    const int origin=x;
    for(unsigned i=0;i<count;++i) {
        const uint16_t c=words[i];
        if(c==0xffff || c==0xffc) break;
        if(c==0xfffe) { x=origin;y+=14; }
        else if(c<256 || c==0xffd) {
            if ((!message || y<222) && !glyph(u,p,c,x,y,palette,error,error_size)) return false;
            x+=8+spacing;
        }
    }
    return true;
}
static bool icon(HmbtnNewGameUI *u,uint8_t *p,unsigned animation,unsigned sub,int x,int y,
    char *error,size_t error_size)
{
    const HmbtnAnimationPlayer player={(uint16_t)animation,(uint16_t)sub,0};
    return hmbtn_animation_draw(&u->icons,u->atlas,&player,x,y,p,WIDTH,HEIGHT,WIDTH*HEIGHT*4,error,error_size);
}
static bool number(HmbtnNewGameUI *u,uint8_t *p,unsigned value,int x,int y,char *error,size_t error_size)
{
    const uint16_t digits[3]={value<10?0xffd:(uint16_t)(0x40+value/10),(uint16_t)(0x40+value%10),0xffff};
    return text(u,p,digits,3,x,y,0,3,false,error,error_size);
}
static bool summary(HmbtnNewGameUI *u,const HmbtnNewGame *g,uint8_t *p,char *error,size_t error_size)
{
    uint16_t expanded[128];unsigned count=0;
    for(unsigned i=0;i<sizeof(new_game_summary)/2 && count<127;++i) {
        const uint16_t c=new_game_summary[i];
        const uint16_t *insert=c==0x400a?g->player_name:c==0x400c?g->dog_name:c==0x400d?g->farm_name:NULL;
        if(insert) {
            for(unsigned j=0;j<HMBTN_NAME_CHARS && insert[j]!=0xffff && count<127;++j) expanded[count++]=insert[j];
        } else expanded[count++]=c;
    }
    expanded[count]=0xffff;
    hmbtn_window_frame_draw(p,WIDTH,HEIGHT,72,56,176,90);
    /* Original summary uses 12+1 line spacing; other message windows use 12+2. */
    int row_y=60;unsigned start=0;
    for(unsigned i=0;i<count;++i) {
        if(expanded[i]==0xfffe || expanded[i]==0xffff) {
            if(!text(u,p,expanded+start,i-start,76,row_y,0,3,false,error,error_size)) return false;
            start=i+1;row_y+=13;
        }
    }
    return icon(u,p,43,g->season,172,72,error,error_size)
        && number(u,p,g->day+1,188,72,error,error_size);
}
bool hmbtn_new_game_ui_draw(HmbtnNewGameUI *u,const HmbtnNewGame *g,
    unsigned tick,uint8_t *p,char *error,size_t error_size)
{
    const bool editor=hmbtn_new_game_name_editor(g);
    const bool birthday=g->step==18;
    if(editor || birthday) {
        /* Original name_entry_step() places a half-intensity black tile behind UI. */
        for(unsigned i=0;i<WIDTH*HEIGHT*4;++i) if(i%4!=3) p[i]=(uint8_t)((p[i]>>4)<<3);
        hmbtn_window_frame_draw(p,WIDTH,HEIGHT,16,16,120,20);
        const uint16_t *caption=birthday?your_birthday:g->step==21?farms_name:g->step==24?dogs_name:your_name;
        if(!text(u,p,caption,32,20,20,0,3,false,error,error_size)) return false;
        if(birthday) {
            hmbtn_window_frame_draw(p,WIDTH,HEIGHT,171,32,133,18);
            if(!text(u,p,birthday_label,sizeof(birthday_label)/2,173,35,0,3,false,error,error_size)) return false;
            if(g->birthday_phase>=2 && !icon(u,p,43,g->season,253,35,error,error_size)) return false;
            if(g->birthday_phase>=3 && !number(u,p,g->day+1,277,35,error,error_size)) return false;
            if(g->birthday_phase<3) {
                hmbtn_window_frame_draw(p,WIDTH,HEIGHT,20,56,279,103);
                for(unsigned i=0;i<4;++i) {
                    if(!icon(u,p,43,i,30+(int)i*64,60,error,error_size)
                            || !text(u,p,season_text_codes+i*6,6,42+(int)i*64,60,0,3,false,error,error_size)) return false;
                }
                for(unsigned i=0;i<30;++i)
                    if(!number(u,p,i+1,30+(int)(i%7)*39,88+(int)(i/7)*14,error,error_size)) return false;
                if(g->birthday_phase==1) {
                    if(!icon(u,p,11,1,36+(int)g->birthday_value*64,66,error,error_size)) return false;
                } else if(!icon(u,p,11,1,36+(int)(g->birthday_value%7)*39,
                        94+(int)(g->birthday_value/7)*14,error,error_size)) return false;
            }
        } else {
            hmbtn_window_frame_draw(p,WIDTH,HEIGHT,171,32,133,18);
            if(g->step==21) {
                if(!text(u,p,g->name,HMBTN_NAME_CHARS+1,173,35,0,3,false,error,error_size)
                        || !text(u,p,farm_suffix,sizeof(farm_suffix)/2,245,35,0,3,false,error,error_size)) return false;
            } else if(!text(u,p,name_label,sizeof(name_label)/2,173,35,0,3,false,error,error_size)
                    || !text(u,p,g->name,HMBTN_NAME_CHARS+1,221,35,0,3,false,error,error_size)) return false;
            const int name_x=g->step==21?173:221;
            if(tick%15==0 && g->length<HMBTN_NAME_CHARS
                    && !glyph(u,p,0x75,name_x+(int)g->length*8,35,3,error,error_size)) return false;
            if(!g->confirming) {
                hmbtn_window_frame_draw(p,WIDTH,HEIGHT,45,64,230,86);
                const uint16_t *grid=g->grid?symbol_grid:letter_grid;
                if(!text(u,p,grid,g->grid?sizeof(symbol_grid)/2:sizeof(letter_grid)/2,
                        47,66,1,3,false,error,error_size)) return false;
                int col=(int)(g->cell%14),row=(int)(g->cell/14),offset=col>=5;
                if(col>=10) ++offset;
                if(col>=13) offset+=2;
                if(!icon(u,p,11,1,(col+offset)*9+50,row*14+72,error,error_size)) return false;
            }
        }
    }
    if(g->step>=25 && g->step<=28 && !summary(u,g,p,error,error_size)) return false;
    const HmbtnMessage *m=&g->message;
    int x,y,w,h;
    if(hmbtn_message_box(m,&x,&y,&w,&h)) hmbtn_window_frame_draw(p,WIDTH,HEIGHT,x,y,w,h);
    if(m->phase==HMBTN_MESSAGE_PRINTING || m->phase==HMBTN_MESSAGE_WAIT_END || m->phase==HMBTN_MESSAGE_WAIT_PAGE) {
        if(!text(u,p,m->words+m->page,m->cursor-m->page,30,168,0,3,true,error,error_size)) return false;
        if(m->face && !hmbtn_texture_draw(u->atlas,960,175,80,80,8,0,490,26,84,p,WIDTH,HEIGHT,error,error_size)) return false;
        if(!editor && !birthday && g->step!=26 && (m->phase==HMBTN_MESSAGE_WAIT_END || m->phase==HMBTN_MESSAGE_WAIT_PAGE)) {
            if(!hmbtn_animation_draw(&u->icons,u->atlas,&u->heart,285,217,p,WIDTH,HEIGHT,
                    WIDTH*HEIGHT*4,error,error_size)
                    || !hmbtn_animation_tick(&u->icons,&u->heart,error,error_size)) return false;
        }
        if(g->step==26 && m->phase==HMBTN_MESSAGE_WAIT_END
                && !icon(u,p,11,1,36,174+(int)g->choice*14,error,error_size)) return false;
        if(((editor && g->confirming) || (birthday && g->birthday_phase==3)) && m->phase==HMBTN_MESSAGE_WAIT_END) {
            hmbtn_window_frame_draw(p,WIDTH,HEIGHT,145,76,40,30);
            if(!text(u,p,prompt_yes_no,sizeof(prompt_yes_no)/2,147,78,0,3,false,error,error_size)
                    || !icon(u,p,11,1,153,(int)g->choice*12+84,error,error_size)) return false;
        }
    }
    return true;
}
