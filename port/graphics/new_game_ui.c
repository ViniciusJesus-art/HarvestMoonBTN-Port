#include "new_game_ui.h"
#include "resource_file.h"
#include "text_resources.h"
#include "psx_data.h"
#include "window_frame.h"
#include "model_render.h"
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
    bool dim;
    char *root;
    unsigned portrait_bank;
};
void hmbtn_new_game_ui_cursor(const HmbtnNewGameUI *u, HmbtnWorldSprite *sprite,
    const int32_t position[3], unsigned variant)
{
    *sprite = (HmbtnWorldSprite){&u->icons,u->atlas,{24,(uint16_t)(variant&1u),0},{0},0,0};
    memcpy(sprite->position,position,sizeof(sprite->position));
}
bool hmbtn_new_game_ui_cue(const HmbtnNewGameUI *u,HmbtnWorldSprite *sprite,
    const int32_t position[3],unsigned icon,unsigned phase,int screen_y)
{
    /* cue_draw_frame: reverse frames, stepping once per ten countdown units. */
    if(!phase || icon>=u->icons.count[3]) return false;
    const unsigned frames=hmbtn_read_le16(u->icons.data+u->icons.section[3]+icon*4);
    if(!frames) return false;
    *sprite=(HmbtnWorldSprite){&u->icons,u->atlas,{(uint16_t)icon,(uint16_t)(frames-1-(phase/10)%frames),0},{0},screen_y,0};
    memcpy(sprite->position,position,sizeof(sprite->position));return true;
}
HmbtnNewGameUI *hmbtn_new_game_ui_create(const char *root, char *error, size_t error_size)
{
    HmbtnNewGameUI *u = calloc(1,sizeof(*u));
    uint8_t small[8192];
    if (!u) { snprintf(error,error_size,"Cannot allocate new-game UI"); return NULL; }
    size_t root_len = strlen(root) + 1;

    u->root = malloc(root_len);
    if (!u->root) {
        free(u);
        snprintf(error, error_size, "Cannot retain UI resource root");
        return NULL;
    }

    memcpy(u->root, root, root_len);
    u->portrait_bank = UINT32_MAX;
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
    if (u) { free(u->root);free(u->atlas); free(u); }
}
bool hmbtn_new_game_ui_load(HmbtnNewGameUI *u, const char *root, HmbtnNewGame *g,
    char *error, size_t error_size)
{
    if (!g->request || g->loaded == g->request) return true;
    if (!hmbtn_new_game_ui_message(u,root,"A_FILE/05_messege3.bin",g->request,&g->message,error,error_size)) return false;
    g->loaded = g->request;
    printf("New-game message: 0x%03x\n",g->loaded);
    fflush(stdout);
    return true;
}
bool hmbtn_new_game_ui_message(HmbtnNewGameUI *u, const char *root,
    const char *archive, unsigned id, HmbtnMessage *message, char *error, size_t error_size)
{
    uint8_t record[8192];
    if (id > UINT32_MAX/8192u) { snprintf(error,error_size,"Message index outside bounds"); return false; }
    return hmbtn_resource_read(root,archive,id*8192u,record,sizeof(record),error,error_size)
        && hmbtn_ui_message_record(u,record,sizeof(record),message,error,error_size);
}
bool hmbtn_ui_message_art(HmbtnNewGameUI *u,const uint8_t *record,size_t size,char *error,size_t error_size)
{
    if(!record || size!=8192) { snprintf(error,error_size,"Invalid message artwork record");return false; }
    const bool ok=hmbtn_read_le32(record)==UINT32_MAX
        || hmbtn_bank_apply(record+4,0x1800-4,u->atlas,error,error_size);
    if(ok && hmbtn_read_le32(record)!=UINT32_MAX) u->portrait_bank=hmbtn_read_le32(record);
    return ok;
}
bool hmbtn_ui_message_record(HmbtnNewGameUI *u,const uint8_t *record,size_t size,
    HmbtnMessage *message,char *error,size_t error_size)
{
    return hmbtn_message_open(message,record,size,error,error_size)
        && hmbtn_ui_message_art(u,record,size,error,error_size);
}
bool hmbtn_ui_message_portrait(HmbtnNewGameUI *u,const HmbtnMessage *m,char *error,size_t error_size)
{
    if(m->bank==UINT32_MAX || m->bank==u->portrait_bank) return true;
    uint8_t bank[HMBTN_PORTRAIT_BANK_BYTES];
    const HmbtnTextResourceId id={HMBTN_TEXT_PORTRAIT,m->bank,0};
    if(!hmbtn_text_resource_read(u->root,id,bank,sizeof(bank),error,error_size)
            || !hmbtn_bank_apply(bank,sizeof(bank),u->atlas,error,error_size)) return false;
    u->portrait_bank=m->bank;return true;
}
static bool glyph(HmbtnNewGameUI *u,uint8_t *p,uint16_t code,int x,int y,unsigned palette,
    char *error,size_t error_size)
{
    static const unsigned rows[6]={254,250,250,252,253,251};
    if(code==0xffd || code>=256) return true;
    if(u->dim) {
        uint8_t tile[8*12*4]={0};
        if(!hmbtn_texture_draw(u->atlas,896+(code&15)*2,(code>>4)*12,8,12,4,
                896,rows[palette],0,0,tile,8,12,error,error_size)) return false;
        for(int row=0;row<12;++row) for(int col=0;col<8;++col) {
            if(x+col<0 || x+col>=WIDTH || y+row<0 || y+row>=HEIGHT) continue;
            const uint8_t *src=tile+(row*8+col)*4;
            if(!src[3]) continue;
            uint8_t *dst=p+((y+row)*WIDTH+x+col)*4;
            for(unsigned channel=0;channel<3;++channel)
                dst[channel]=(uint8_t)((src[channel]+dst[channel])/2);
            dst[3]=255;
        }
        return true;
    }
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
            if(message && c<256) {
                uint8_t tile[8*12*4]={0};
                static const unsigned rows[6]={254,250,250,252,253,251};
                if(!hmbtn_texture_draw(u->atlas,896+(c&15)*2,(c>>4)*12,8,12,4,
                        896,rows[palette],0,0,tile,8,12,error,error_size)) return false;
                for(int row=0;row<12;++row) for(int col=0;col<8;++col) {
                    if(y+row<168 || y+row>=224 || x+col<0 || x+col>=WIDTH) continue;
                    const uint8_t *pixel=tile+(row*8+col)*4;
                    if(pixel[3]) memcpy(p+((y+row)*WIDTH+x+col)*4,pixel,4);
                }
            } else if(!message && !glyph(u,p,c,x,y,palette,error,error_size)) return false;
            x+=8+spacing;
        }
    }
    return true;
}
bool hmbtn_ui_text(HmbtnNewGameUI *u,uint8_t *p,const uint16_t *words,unsigned count,
    int x,int y,int spacing,unsigned palette,char *error,size_t error_size)
{
    if(palette>=6) return false;
    return text(u,p,words,count,x,y,spacing,palette,false,error,error_size);
}
bool hmbtn_ui_text_dim(HmbtnNewGameUI *u,uint8_t *p,const uint16_t *words,unsigned count,
    int x,int y,bool dim,char *error,size_t error_size)
{
    u->dim=dim;
    const bool ok=text(u,p,words,count,x,y,0,3,false,error,error_size);
    u->dim=false;
    return ok;
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
bool hmbtn_ui_icon(HmbtnNewGameUI *u,uint8_t *p,unsigned animation,unsigned sub,int x,int y,
    char *error,size_t error_size)
{
    return icon(u,p,animation,sub,x,y,error,error_size);
}
static bool summary(HmbtnNewGameUI *u,const HmbtnNewGame *g,uint8_t *p,char *error,size_t error_size)
{
    uint16_t expanded[128];unsigned count=0;
    for(unsigned i=0;i<sizeof(new_game_summary)/2 && count<127;++i) {
        const uint16_t c=new_game_summary[i];
        const uint16_t *insert=c==0x400a?g->registration->player_name:c==0x400c?g->registration->dog_name:c==0x400d?g->registration->farm_name:NULL;
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
    return icon(u,p,43,g->registration->birthday_season,172,72,error,error_size)
        && number(u,p,g->registration->birthday_day+1,188,72,error,error_size);
}
bool hmbtn_new_game_ui_draw(HmbtnNewGameUI *u,const HmbtnNewGame *g,
    unsigned tick,uint8_t *p,char *error,size_t error_size)
{
    if(g->message.phase!=HMBTN_MESSAGE_CLOSED && !hmbtn_ui_message_portrait(u,&g->message,error,error_size)) return false;
    const bool editor=hmbtn_new_game_name_editor(g);
    const bool birthday=g->step==18;
    if(editor || birthday) {
        /* Original name_entry_step() places a half-intensity black tile behind UI. */
        for(unsigned i=0;i<WIDTH*HEIGHT*4;++i) if(i%4!=3) p[i]=(uint8_t)((p[i]>>4)<<3);
        hmbtn_window_frame_draw(p,WIDTH,HEIGHT,16,16,120,20);
        const uint16_t *caption=birthday?your_birthday:g->step==101?name_label:g->step==21?farms_name:g->step==24?dogs_name:your_name;
        if(!text(u,p,caption,32,20,20,0,3,false,error,error_size)) return false;
        if(birthday) {
            hmbtn_window_frame_draw(p,WIDTH,HEIGHT,171,32,133,18);
            if(!text(u,p,birthday_label,sizeof(birthday_label)/2,173,35,0,3,false,error,error_size)) return false;
            if(g->birthday_phase>=2 && !icon(u,p,43,g->registration->birthday_season,253,35,error,error_size)) return false;
            if(g->birthday_phase>=3 && !number(u,p,g->registration->birthday_day+1,277,35,error,error_size)) return false;
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
    if(m->phase==HMBTN_MESSAGE_PRINTING || m->phase==HMBTN_MESSAGE_SCROLLING || m->phase==HMBTN_MESSAGE_WAIT_END || m->phase==HMBTN_MESSAGE_WAIT_PAGE) {
        if(!text(u,p,m->words+m->page,m->cursor-m->page,30,168-(int)m->scroll_y,0,3,true,error,error_size)) return false;
        if(m->face && !hmbtn_texture_draw(u->atlas,960,175,80,80,8,0,490,26,84,p,WIDTH,HEIGHT,error,error_size)) return false;
        if(!m->suppress_cursor && !editor && !birthday && g->step!=26 && !(g->step==100 && m->choices) && (m->phase==HMBTN_MESSAGE_WAIT_END || m->phase==HMBTN_MESSAGE_WAIT_PAGE)) {
            if(!hmbtn_animation_draw(&u->icons,u->atlas,&u->heart,285,217,p,WIDTH,HEIGHT,
                    WIDTH*HEIGHT*4,error,error_size)
                    || !hmbtn_animation_tick(&u->icons,&u->heart,error,error_size)) return false;
        }
        if((g->step==26 || (g->step==100 && m->choices)) && m->phase==HMBTN_MESSAGE_WAIT_END
                && !icon(u,p,11,1,36,174+(int)g->choice*14,error,error_size)) return false;
        if(((editor && g->confirming) || (birthday && g->birthday_phase==3)) && m->phase==HMBTN_MESSAGE_WAIT_END) {
            hmbtn_window_frame_draw(p,WIDTH,HEIGHT,145,76,40,30);
            if(!text(u,p,prompt_yes_no,sizeof(prompt_yes_no)/2,147,78,0,3,false,error,error_size)
                    || !icon(u,p,11,1,153,(int)g->choice*12+84,error,error_size)) return false;
        }
    }
    return true;
}
