/* Initial-house branches of player_act_zone_front, messages.c, calendar.c and
 * item_storage.c. State contains native values, never serialized PS1 pointers. */
#include "house_interaction.h"
#include "inventory.h"
#include "psx_data.h"
#include <string.h>

static int32_t half(const uint8_t *p)
{
    const uint16_t v=hmbtn_read_le16(p); return v<32768 ? v : (int32_t)v-65536;
}
void hmbtn_house_interaction_init(HmbtnHouseInteraction *h,HmbtnGameState *state)
{
    memset(h,0,sizeof(*h));
    h->picked=255; h->sound=-1; h->tv_group=18;
    h->message_bank=3;
    h->options.vibration=h->options.stereo=h->options.clock=true;
    h->inventory=hmbtn_game_inventory(state);
    h->clock=hmbtn_game_clock_const(state);
    h->registration=hmbtn_game_registration_const(state);
    h->weather=&state->weather;
}
bool hmbtn_house_expand_reading(const HmbtnHouseInteraction *h, uint8_t *record,
    size_t size)
{
    return hmbtn_house_expand_item_reading(h,NULL,record,size);
}
bool hmbtn_house_expand_item_reading(const HmbtnHouseInteraction *h,
    const HmbtnStoredTool *selected,uint8_t *record,size_t size)
{
    /* text_insert_source: dog/farm names and estate stocks in bank-2 signs.
     * Unsupported inserts stay intact for the message parser to reject. */
    if(!h || !record || size!=8192 || !h->registration) return false;
    uint16_t words[HMBTN_MESSAGE_WORDS]; unsigned count=0;
    for(unsigned i=0;i<HMBTN_MESSAGE_WORDS;++i) {
        const uint16_t c=hmbtn_read_le16(record+0x1800+i*2);
        uint16_t insert[HMBTN_NAME_CHARS+1]; unsigned n=0;
        if(c==0x400c || c==0x400d) {
            const uint16_t *name=c==0x400c ? h->registration->dog_name : h->registration->farm_name;
            while(n<HMBTN_NAME_CHARS && name[n]!=0xffff) {
                insert[n]=name[n]; ++n;
            }
        } else if(c==0x4010 || (c>=0x4012 && c<=0x4015)) {
            if(c==0x4010 && !selected && (h->cursor>16 || h->page>=8)) return false;
            unsigned value=c==0x4010 ? (selected ? selected->count : h->cursor==0 ? h->inventory->equipped.count : h->cursor<9 ?
                h->inventory->bag[h->cursor-1].count : h->inventory->toolbox[h->page*8+h->cursor-9].count) : h->inventory->supplies[c-0x4012];
            unsigned divisor=1000;
            if(value>9999) return false;
            while(divisor>1 && value<divisor) divisor/=10;
            do {
                insert[n++]=(uint16_t)(0x40+value/divisor);
                value%=divisor; divisor/=10;
            } while(divisor);
        } else insert[n++]=c;
        if(n>HMBTN_MESSAGE_WORDS-count) return false;
        memcpy(words+count,insert,n*sizeof(*insert)); count+=n;
        if(c==0xffff) {
            for(unsigned j=0;j<count;++j) hmbtn_write_le16(record+0x1800+j*2,words[j]);
            return true;
        }
    }
    return false;
}
unsigned hmbtn_house_calendar_message(unsigned season, unsigned day)
{
    static const unsigned events[18][3]={
        {0,0,0x3ca},{0,7,0x3cb},{0,13,0x3cc},{0,17,0x3cd},{0,21,0x3ce},
        {1,0,0x3cf},{1,6,0x3d0},{1,11,0x3d1},{1,19,0x3d2},{1,23,0x3d3},
        {2,2,0x3d4},{2,8,0x3d5},{2,12,0x3d6},{2,20,0x3d7},
        {3,9,0x3d8},{3,13,0x3d9},{3,23,0x3da},{3,29,0x3db}};
    for(unsigned i=0;i<18;++i) if(events[i][0]==season && events[i][1]==day) return events[i][2];
    return 0x3dc;
}
void hmbtn_house_tv_program(const HmbtnHouseInteraction *h, unsigned channel,
    unsigned *message, unsigned *group)
{
    if(h->clock->hour<6 || h->weather->today>=3) {
        *message=h->weather->today>=3 ? 0x26a : 0x26b;
        *group=h->weather->today>=3 ? 17u : 16u; return;
    }
    if(channel==0) {
        static const unsigned base[4]={0x26c,0x26e,0x271,0x273};
        *message=h->weather->tomorrow>=3 ? (h->weather->tomorrow==3 ? 0x270u : 0x275u)
            : base[h->clock->season]+(h->weather->tomorrow!=0);
        *group=h->weather->tomorrow>=3 ? h->weather->tomorrow+4 : h->weather->tomorrow==1 ? 5u : h->weather->tomorrow==2 ? 6u : 4u;
    } else if(channel==1 || channel==3) {
        *message=(channel==1 ? 0x277u : 0x2efu)+h->clock->season*30+h->clock->day;
        *group=channel==1 ? 0u : 1u;
    } else if((h->clock->season==0 && h->clock->day<5) || (h->clock->season==3 && h->clock->day>=28)) {
        *message=h->clock->day+(h->clock->season==0 ? 610 : 587); *group=10;
    } else {
        static const unsigned rows[7][3]={{40,560,14},{38,520,15},{18,500,3},
            {20,480,12},{35,440,13},{38,400,11},{23,371,9}};
        const unsigned *r=rows[h->clock->weekday];
        const int value=(int)(h->clock->year*113+h->clock->season*30+h->clock->day)-5;
        const int row=value%((int)r[0]*7)/7;
        *message=(unsigned)((int)r[1]+row); *group=r[2];
        if(h->clock->weekday==5 && value>=548) ++*message;
    }
}
static void request(HmbtnHouseInteraction *h,unsigned message)
{
    h->request=message; h->loaded=0;
}
bool hmbtn_house_interact(HmbtnHouseInteraction *h,const uint8_t *section,
    size_t size,const HmbtnIntroActor *player)
{
    if(!section || size<16+50*44 || h->mode!=HMBTN_HOUSE_FREE || player->facing>3) return false;
    static const int16_t reach[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
    const int32_t x=player->position[0]+reach[player->facing][0],
        y=player->position[1], z=player->position[2]+reach[player->facing][1];
    for(unsigned i=0;i<50;++i) {
        const uint8_t *p=section+16+i*44;
        if(!(hmbtn_read_le16(p)&0x4000) || x<=half(p+14) || x>=half(p+20)
                || z<=half(p+18) || z>=half(p+24) || y<=half(p+16)-128 || y>=half(p+22)+120) continue;
        const unsigned id=hmbtn_read_le16(p+2);
        if(half(p+8)!=(int32_t)player->facing && !(id==16 && half(p+8)==5)) return false;
        h->closing=h->detail=false; h->sound=3; h->steps=0; h->message_bank=3;
        switch(id) {
        case 1:
            h->mode=HMBTN_HOUSE_OPTIONS;
            h->options.page=h->options.row=h->options.column=h->options.opening_cursor=0;
            h->options.manual_row=h->options.manual_column=0;
            request(h,0x3b0); break;
        case 2: h->mode=HMBTN_HOUSE_BED; h->bed_choice=0; request(h,0x1b7); break;
        case 3: h->mode=HMBTN_HOUSE_TV_SELECT; request(h,0x3a1); break;
        case 8: h->mode=HMBTN_HOUSE_CALENDAR; h->calendar_day=h->clock->day; request(h,0x3b4); break;
        case 22: h->mode=HMBTN_HOUSE_TOOLBOX_OPEN; h->pan=0; h->cursor=h->page=0; h->picked=255; h->sound=-1; break;
        case 16: h->mode=HMBTN_HOUSE_BOOK; h->message_bank=2; request(h,(unsigned)half(p+4)); break;
        default: h->sound=-1; return false;
        }
        return true;
    }
    return false;
}
static HmbtnStoredTool *tool(HmbtnHouseInteraction *h,unsigned cell,unsigned page)
{
    return cell==0 ? &h->inventory->equipped : cell<9 ? &h->inventory->bag[cell-1] : &h->inventory->toolbox[page*8+cell-9];
}
static void close_menu(HmbtnHouseInteraction *h)
{
    h->closing=true;
    /* The text engine may have already completed its closing animation.
     * Starting it again flashes a full-size box for one more cycle. */
    if(h->message.phase!=HMBTN_MESSAGE_CLOSED && h->message.phase!=HMBTN_MESSAGE_CLOSING) {
        h->message.phase=HMBTN_MESSAGE_CLOSING; h->message.box_frame=0; h->sound=3;
    }
    if(h->message.phase==HMBTN_MESSAGE_CLOSED && !h->pan) h->mode=HMBTN_HOUSE_FREE;
    if(h->mode==HMBTN_HOUSE_TV || h->mode==HMBTN_HOUSE_TV_SELECT) { h->tv_group=18; h->tv_dirty=true; }
}
uint32_t hmbtn_house_world_buttons(const HmbtnHouseOptions *o,uint32_t held)
{
    if(o->layout==0 || (o->layout==2 && !o->rotated)) return held;
    uint32_t result=held&~(uint32_t)(HMBTN_PAD_UP|HMBTN_PAD_DOWN|HMBTN_PAD_LEFT|HMBTN_PAD_RIGHT);
    if(held&HMBTN_PAD_UP) result|=HMBTN_PAD_LEFT;
    if(held&HMBTN_PAD_DOWN) result|=HMBTN_PAD_RIGHT;
    if(held&HMBTN_PAD_LEFT) result|=HMBTN_PAD_DOWN;
    if(held&HMBTN_PAD_RIGHT) result|=HMBTN_PAD_UP;
    return result;
}
void hmbtn_house_options_idle(HmbtnHouseOptions *o,bool moving,unsigned facing)
{
    if(moving) o->idle_frames=0;
    else if(o->idle_frames==16) { o->idle_frames=0; o->rotated=facing==1 || facing==3; }
    else ++o->idle_frames;
}
static void options_tick(HmbtnHouseInteraction *h,uint32_t pressed)
{
    HmbtnHouseOptions *o=&h->options;
    if(pressed&HMBTN_PAD_START) { close_menu(h); return; }
    if(pressed&HMBTN_PAD_CIRCLE) {
        if(o->page==0) close_menu(h);
        else { o->page=0; request(h,0x3b0); h->sound=1; }
        return;
    }
    if(o->page==0) {
        if(pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) { o->opening_cursor^=1; h->sound=5; }
        if(pressed&HMBTN_PAD_CROSS) {
            o->page=o->opening_cursor+1;
            request(h,o->page==1 ? 0x3b2 : 0x3b3); h->sound=0;
        }
        return;
    }
    if(o->page==1) {
        if(pressed&(HMBTN_PAD_LEFT|HMBTN_PAD_RIGHT)) {
            o->manual_column^=1;
            if(!o->manual_column && o->manual_row==5) o->manual_row=4;
            h->sound=5;
        }
        const unsigned rows=o->manual_column ? 6u : 5u;
        if(pressed&HMBTN_PAD_UP) { o->manual_row=(o->manual_row+rows-1)%rows; h->sound=5; }
        else if(pressed&HMBTN_PAD_DOWN) { o->manual_row=(o->manual_row+1)%rows; h->sound=5; }
        if(pressed&HMBTN_PAD_CROSS) {
            h->detail=true; request(h,0x367+o->manual_column*5+o->manual_row); h->sound=0;
        }
    } else {
        if(pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) {
            if(o->row==0 && o->column>=2) o->column=1;
            o->row=(o->row+((pressed&HMBTN_PAD_DOWN) ? 1u : 3u))%4; h->sound=5;
        }
        const unsigned columns=o->row==0 ? 3u : 2u;
        if(pressed&HMBTN_PAD_LEFT) { o->column=(o->column+columns-1)%columns; h->sound=5; }
        else if(pressed&HMBTN_PAD_RIGHT) { o->column=(o->column+1)%columns; h->sound=5; }
        if(pressed&HMBTN_PAD_CROSS) {
            if(o->row==0) o->layout=o->column;
            else if(o->row==1) {
                if(!o->vibration && !o->column) o->rumble=true;
                o->vibration=o->column==0;
            } else if(o->row==2) o->stereo=o->column==0;
            else o->clock=o->column==0;
            h->sound=0;
        } else if(pressed&HMBTN_PAD_TRIANGLE) {
            h->detail=true; request(h,o->row==0 ? 0x37b + o->column : 0x37e + o->row); h->sound=0;
        }
    }
}
void hmbtn_house_interaction_tick(HmbtnHouseInteraction *h,uint32_t pressed,uint32_t held)
{
    h->sound=-1;
    if(h->mode==HMBTN_HOUSE_FREE || h->mode==HMBTN_HOUSE_SLEEP) return;
    if(h->mode==HMBTN_HOUSE_TOOLBOX_OPEN) {
        if(h->pan<640) { h->pan+=32; return; }
        h->mode=HMBTN_HOUSE_TOOLBOX; request(h,0x378); h->detail=true; h->sound=3; return;
    }
    const bool ready=h->message.phase==HMBTN_MESSAGE_WAIT_END;
    const bool dialogue=h->mode==HMBTN_HOUSE_TV || h->mode==HMBTN_HOUSE_BOOK
        || ((h->mode==HMBTN_HOUSE_CALENDAR || h->mode==HMBTN_HOUSE_OPTIONS) && h->detail);
    h->sound=hmbtn_message_tick(&h->message,dialogue ? pressed : 0,held);
    if(h->closing) {
        if(h->pan) { h->pan-=32; return; }
        if(h->message.phase==HMBTN_MESSAGE_CLOSED) h->mode=HMBTN_HOUSE_FREE;
        return;
    }
    if(h->mode==HMBTN_HOUSE_OPTIONS) {
        if(ready && (pressed&HMBTN_PAD_START)) { h->detail=false; close_menu(h); return; }
        if(h->detail) {
            if(h->message.phase==HMBTN_MESSAGE_CLOSED) {
                h->detail=false; request(h,h->options.page==1 ? 0x3b2 : 0x3b3);
            }
        } else if(ready) options_tick(h,pressed);
        return;
    }
    if(h->mode==HMBTN_HOUSE_BED) {
        if(!ready) return;
        if(pressed&(HMBTN_PAD_START|HMBTN_PAD_CIRCLE)) { close_menu(h); h->sound=1; return; }
        if(pressed&HMBTN_PAD_UP) { h->bed_choice=(h->bed_choice+3)%4; h->sound=5; }
        else if(pressed&HMBTN_PAD_DOWN) { h->bed_choice=(h->bed_choice+1)%4; h->sound=5; }
        if(pressed&HMBTN_PAD_CROSS) {
            if(h->bed_choice<2) {
                h->save_requested=h->bed_choice==1; h->mode=HMBTN_HOUSE_SLEEP;
                h->message=(HmbtnMessage){0}; h->request=h->loaded=0; h->sound=0;
            } else { h->boundary=h->bed_choice; h->sound=2; }
        }
        return;
    }
    if(h->mode==HMBTN_HOUSE_TV) {
        ++h->tv_ticks;
        if(pressed&HMBTN_PAD_CIRCLE) { close_menu(h); return; }
    }
    if((h->mode==HMBTN_HOUSE_TV_SELECT && ready) || h->mode==HMBTN_HOUSE_TV) {
        unsigned channel=4;
        if(pressed&HMBTN_PAD_UP) channel=0;
        else if(pressed&HMBTN_PAD_LEFT) channel=2;
        else if(pressed&HMBTN_PAD_RIGHT) channel=1;
        else if(pressed&HMBTN_PAD_DOWN) channel=3;
        if(channel<4) {
            unsigned message; hmbtn_house_tv_program(h,channel,&message,&h->tv_group);
            h->mode=HMBTN_HOUSE_TV; h->tv_channel=channel; h->tv_ticks=0; h->tv_dirty=true;
            request(h,message); return;
        }
    }
    if(h->mode==HMBTN_HOUSE_BOOK || h->mode==HMBTN_HOUSE_TV) {
        if(h->message.phase==HMBTN_MESSAGE_CLOSED) {
            if(h->mode==HMBTN_HOUSE_BOOK && h->detail) {
                h->mode=HMBTN_HOUSE_TOOLBOX; h->detail=false; request(h,0x3ad);
            } else close_menu(h);
        }
        return;
    }
    if(h->mode==HMBTN_HOUSE_CALENDAR && h->detail && h->message.phase==HMBTN_MESSAGE_CLOSED) {
        h->detail=false; request(h,0x3b4); return;
    }
    if(!ready) return;
    if(pressed&(HMBTN_PAD_START|HMBTN_PAD_CIRCLE)) {
        if(h->mode==HMBTN_HOUSE_TOOLBOX && h->picked!=255 && !(pressed&HMBTN_PAD_START)) {
            h->picked=255; h->sound=1;
        } else close_menu(h);
        return;
    }
    if(h->mode==HMBTN_HOUSE_CALENDAR) {
        if(h->detail) return;
        int day=(int)h->calendar_day;
        if(pressed&HMBTN_PAD_UP) { day-=7; if(day<0) { day+=28; if(day<23) day+=7; } }
        else if(pressed&HMBTN_PAD_DOWN) { day+=7; if(day>=30) { day-=28; if(day>=7) day-=7; } }
        else if(pressed&HMBTN_PAD_LEFT) day=(day+29)%30;
        else if(pressed&HMBTN_PAD_RIGHT) day=(day+1)%30;
        if((unsigned)day!=h->calendar_day) { h->calendar_day=(unsigned)day; h->sound=5; }
        if(pressed&(HMBTN_PAD_CROSS|HMBTN_PAD_TRIANGLE)) {
            h->detail=true; request(h,hmbtn_house_calendar_message(h->clock->season,h->calendar_day)); h->sound=0;
        }
    } else if(h->mode==HMBTN_HOUSE_TOOLBOX) {
        if(h->detail) { h->detail=false; request(h,0x3ad); return; }
        /* Original marker table, initial rucksack level (two unlocked tool slots). */
        static const uint8_t next[17][4]={
            {13,9,2,1},{13,9,0,2},{14,10,1,0},{0,0,2,0},{0,0,0,0},
            {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
            {1,13,12,10},{2,14,9,11},{2,15,10,12},{2,16,11,9},
            {9,1,16,14},{10,2,13,15},{11,2,14,16},{12,2,15,13}};
        if(pressed&HMBTN_PAD_CROSS) {
            h->sound=0;
            if(h->picked==255) { h->picked=h->cursor; h->picked_page=h->page; }
            else {
                HmbtnStoredTool *a=tool(h,h->picked,h->picked_page), *b=tool(h,h->cursor,h->page);
                const HmbtnStoredTool swap=*a; *a=*b; *b=swap; h->picked=255;
            }
        } else if(pressed&HMBTN_PAD_SELECT) {
            if(h->picked!=255) { h->sound=2; return; }
            hmbtn_inventory_compact(h->inventory->toolbox,hmbtn_inventory_toolbox_capacity(h->inventory));
            h->sound=0;
        } else if(pressed&HMBTN_PAD_TRIANGLE) {
            const unsigned id=tool(h,h->cursor,h->page)->id;
            if(id) { h->mode=HMBTN_HOUSE_BOOK; h->detail=true; request(h,id); h->sound=0; }
            else h->sound=2;
        } else {
            int direction=-1;
            if(pressed&HMBTN_PAD_UP) direction=0;
            else if(pressed&HMBTN_PAD_DOWN) direction=1;
            else if(pressed&HMBTN_PAD_LEFT) direction=2;
            else if(pressed&HMBTN_PAD_RIGHT) direction=3;
            if(direction>=0) {
                if(direction==2 && (h->cursor==9 || h->cursor==13)) h->page=(h->page+7)%8;
                if(direction==3 && (h->cursor==12 || h->cursor==16)) h->page=(h->page+1)%8;
                h->cursor=next[h->cursor][direction]; h->sound=5;
            }
            if(pressed&(HMBTN_PAD_L1|HMBTN_PAD_L2)) { h->page=(h->page+7)%8; h->sound=5; }
            if(pressed&(HMBTN_PAD_R1|HMBTN_PAD_R2)) { h->page=(h->page+1)%8; h->sound=5; }
        }
    }
    if(h->mode==HMBTN_HOUSE_CALENDAR && h->detail && h->message.phase==HMBTN_MESSAGE_CLOSED) {
        h->detail=false; request(h,0x3b4);
    }
}
