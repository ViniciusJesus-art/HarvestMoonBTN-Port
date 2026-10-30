/* House branches of player_act_zone_front, messages.c, calendar.c, house.c and
 * item_storage.c. State contains native values, never serialized PS1 pointers. */
#include "house_interaction.h"
#include "inventory.h"
#include "farm_field.h"
#include "psx_data.h"
#include "tv_shopping.h"
#include "economy.h"
#include "construction.h"
#include "village.h"
#include <string.h>
#include "storage_links.inc"

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
    h->progress=&state->progress;h->cooking=&state->cooking;
    h->shopping=&state->shopping;h->economy=&state->economy;
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
        const unsigned offer=hmbtn_tv_shopping_program(h->shopping,h->clock,h->weather->today,
            h->construction ? h->construction->house_level : 0);
        if(offer<9) { *message=0x258+offer;*group=2;return; }
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
        if(half(p+8)!=(int32_t)player->facing && !(id==16 && half(p+8)==5) && id!=21) return false;
        h->closing=h->detail=false; h->sound=3; h->steps=0; h->message_bank=3;
        switch(id) {
        case 1:
            h->mode=HMBTN_HOUSE_OPTIONS;
            h->options.page=h->options.row=h->options.column=h->options.opening_cursor=0;
            h->options.manual_row=h->options.manual_column=0;
            request(h,0x3b0); break;
        case 2: h->mode=HMBTN_HOUSE_BED; h->bed_choice=0; request(h,0x1b7); break;
        case 3:
            /* Original refresh prepares all four programs even when the
             * player selects weather instead of the entertainment channel. */
            (void)hmbtn_tv_shopping_program(h->shopping,h->clock,h->weather->today,
                h->construction ? h->construction->house_level : 0);
            h->mode=HMBTN_HOUSE_TV_SELECT; request(h,0x3a1); break;
        case 21:
            if(!h->village || !h->economy || !h->shopping) return false;
            h->mode=HMBTN_HOUSE_PHONE;h->message_bank=2;h->phone_answer=0;
            h->phone_offer=hmbtn_tv_shopping_accepted(h->shopping);
            h->phone_phase=h->clock->weekday==6 && h->phone_offer<9 ? 0u : 3u;
            request(h,h->phone_phase==0 ? 0x11b : 0x11a);break;
        case 8: h->mode=HMBTN_HOUSE_CALENDAR; h->calendar_day=h->clock->day; request(h,0x3b4); break;
        case 22: h->storage=0;h->mode=HMBTN_HOUSE_TOOLBOX_OPEN; h->pan=0; h->cursor=h->page=0; h->picked=255; h->sound=-1; break;
        case 5:case 24:
            if(h->inventory->held_item.id) { h->sound=-1;return false; }
            h->mode=HMBTN_HOUSE_COOKING;h->kitchen=(HmbtnKitchenMenu){.phase=id==5 ? HMBTN_KITCHEN_OPEN : HMBTN_KITCHEN_RECORDS,.record=HMBTN_RECIPE_RECORDS};
            request(h,id==5 ? 0x3b5 : 0x3b8);break;
        case 4:case 23:
            h->storage=id==4 ? 2u : 1u;h->mode=HMBTN_HOUSE_TOOLBOX;
            h->pan=0;h->cursor=h->page=0;h->picked=255;h->detail=true;
            request(h,h->storage==2 ? 0x379 : 0x37a);break;
        case 16: h->mode=HMBTN_HOUSE_BOOK; h->message_bank=2; request(h,(unsigned)half(p+4)); break;
        default: h->sound=-1; return false;
        }
        return true;
    }
    return false;
}
static HmbtnStoredTool *tool(HmbtnHouseInteraction *h,unsigned cell,unsigned page)
{
    return hmbtn_inventory_storage_slot(h->inventory,h->storage,cell,page);
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
/* shop_fee_step: first confirm the 10 G call, then the catalog purchase.
 * Phase 1 is the connecting message; phase 3 is a final message. */
static void phone_tick(HmbtnHouseInteraction *h,uint32_t pressed,uint32_t held)
{
    const bool ready=h->message.phase==HMBTN_MESSAGE_WAIT_END;
    const bool question=h->phone_phase==0 || h->phone_phase==2;
    h->sound=hmbtn_message_tick(&h->message,question && ready && !h->closing ? 0 : pressed,held);
    if(h->closing) { if(h->message.phase==HMBTN_MESSAGE_CLOSED) h->mode=HMBTN_HOUSE_FREE;return; }
    if(question) {
        if(!ready) return;
        if(pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) { h->phone_answer^=1;h->sound=5; }
        if(!(pressed&(HMBTN_PAD_CROSS|HMBTN_PAD_CIRCLE))) return;
        const bool yes=!h->phone_answer && !(pressed&HMBTN_PAD_CIRCLE);
        h->sound=yes ? 0 : 1;
        if(h->phone_phase==0) {
            if(!yes) { close_menu(h);return; }
            if(h->economy->gold<10) { h->phone_phase=3;request(h,0x4f);return; }
            hmbtn_economy_gold_change(h->economy,-10,true);
            h->phone_phase=1;request(h,0x11c);
        } else {
            unsigned message=0x127;
            if(yes) {
                const bool bought=hmbtn_tv_shopping_buy(h->shopping,h->economy,h->phone_offer);
                if(bought) hmbtn_village_flag_set(h->village,0x67+h->phone_offer,true);
                message=bought ? 0x126 : 0x4f;
            }
            h->phone_phase=3;request(h,message);
        }
        h->phone_answer=0;
    } else if(h->message.phase==HMBTN_MESSAGE_CLOSED) {
        if(h->phone_phase==1) { h->phone_phase=2;request(h,0x11d+h->phone_offer); }
        else close_menu(h);
    }
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
static unsigned kitchen_message(HmbtnKitchenPhase phase)
{
    static const unsigned ids[]={0x3b5,0x3b6,0x3b7,0x3b8,0x3bc,0x3bd,0x3be,0x3bf,0x3c0,0,0x3b9,0x3bb};
    return ids[phase];
}
static void kitchen_page(HmbtnHouseInteraction *h,HmbtnKitchenPhase phase)
{
    h->kitchen.phase=phase;h->kitchen.choice=0;
    h->message=(HmbtnMessage){0};request(h,kitchen_message(phase));
}
static unsigned kitchen_known_slot(const HmbtnCooking *c,unsigned row)
{
    for(unsigned i=0;i<HMBTN_RECIPE_RECORDS;++i) if(c->records[i].id) { if(!row) return i;--row; }
    return HMBTN_RECIPE_RECORDS;
}
static void kitchen_cook(HmbtnHouseInteraction *h)
{
    HmbtnKitchenMenu *k=&h->kitchen;
    if(!hmbtn_cooking_available(h->cooking,h->inventory,k->options,k->values)) {
        h->sound=2;kitchen_page(h,HMBTN_KITCHEN_ITEMS);return;
    }
    /* The original commits only after the confirmation text closes and the
     * screen fade settles. No inventory/recipe mutation on this button. */
    k->return_phase=k->phase;k->phase=HMBTN_KITCHEN_CLOSE_CONFIRM;k->fade=0;
    k->audio_started=k->audio_finished=false;
    h->message.phase=HMBTN_MESSAGE_CLOSING;h->message.box_frame=0;
    h->request=h->loaded=0;
}
static void kitchen_apply_result(HmbtnHouseInteraction *h)
{
    HmbtnKitchenMenu *k=&h->kitchen;
    if(!hmbtn_cooking_make(h->cooking,h->inventory,k->options,k->values,&k->result)) {
        h->sound=2;kitchen_page(h,HMBTN_KITCHEN_ITEMS);return;
    }
    k->prior_record=hmbtn_cooking_record_slot(h->cooking,k->result.id);
    unsigned message=0x3c2;
    if(k->record<HMBTN_RECIPE_RECORDS) {
        k->result=h->cooking->records[k->record];h->inventory->held_item.modifier=k->result.quality;
        message=0x3c0;
    } else if(k->result.id!=0xe2) {
        message=k->prior_record==HMBTN_RECIPE_RECORDS ? 0x3c1
            : k->result.quality==h->cooking->records[k->prior_record].quality ? 0x3c0
            : k->result.quality>h->cooking->records[k->prior_record].quality ? 0x3c3 : 0x3c4;
    }
    k->result_message=message;k->phase=HMBTN_KITCHEN_TRACK;
    h->message=(HmbtnMessage){0};h->request=h->loaded=0;
}
unsigned hmbtn_house_kitchen_shade(const HmbtnHouseInteraction *h)
{
    if(!h || h->mode!=HMBTN_HOUSE_COOKING) return 0;
    const HmbtnKitchenMenu *k=&h->kitchen;
    if(k->phase==HMBTN_KITCHEN_TRACK) return 255;
    if(k->phase!=HMBTN_KITCHEN_FADE_OUT && k->phase!=HMBTN_KITCHEN_FADE_IN) return 0;
    const unsigned length=k->phase==HMBTN_KITCHEN_FADE_OUT ? 11u : 10u;
    const unsigned shade=(255u*4096u/length)*k->fade/4096u;
    return shade>255 ? 255 : shade;
}
/* Same original questions, utensil/seasoning and ingredient grids, recipe records
 * and result/save decisions; message ownership stays in the existing house modal.
 * A key revealing/advancing text never also commits a selection. */
static void kitchen_tick(HmbtnHouseInteraction *h,uint32_t pressed,uint32_t held)
{
    HmbtnKitchenMenu *k=&h->kitchen;
    if(k->phase==HMBTN_KITCHEN_CLOSE_CONFIRM) {
        h->sound=hmbtn_message_tick(&h->message,0,0);
        if(h->message.phase==HMBTN_MESSAGE_CLOSED) k->phase=HMBTN_KITCHEN_FADE_OUT;
        return;
    }
    if(k->phase==HMBTN_KITCHEN_FADE_OUT) {
        if(++k->fade>=12) kitchen_apply_result(h);
        return;
    }
    if(k->phase==HMBTN_KITCHEN_TRACK) {
        if(k->audio_finished) { k->phase=HMBTN_KITCHEN_FADE_IN;k->fade=9; }
        return;
    }
    if(k->phase==HMBTN_KITCHEN_FADE_IN) {
        if(k->fade) --k->fade;
        if(!k->fade) { kitchen_page(h,HMBTN_KITCHEN_RESULT);request(h,k->result_message); }
        return;
    }
    const bool ready=h->message.phase==HMBTN_MESSAGE_WAIT_END;
    const bool plain=k->phase==HMBTN_KITCHEN_OPEN || k->phase==HMBTN_KITCHEN_DESCRIPTION
        || (k->phase==HMBTN_KITCHEN_RESULT && h->request<0x3c3);
    h->sound=hmbtn_message_tick(&h->message,(!ready || plain || h->closing) ? pressed : 0,held);
    if(h->closing) {
        if(h->message.phase==HMBTN_MESSAGE_CLOSED) {
            if(k->record_pending) { (void)hmbtn_cooking_remember(h->cooking,&k->result);k->record_pending=false; }
            h->mode=HMBTN_HOUSE_FREE;
        }
        return;
    }
    if(plain) {
        if(h->message.phase!=HMBTN_MESSAGE_CLOSED) return;
        if(k->phase==HMBTN_KITCHEN_OPEN) kitchen_page(h,HMBTN_KITCHEN_FIRST);
        else if(k->phase==HMBTN_KITCHEN_DESCRIPTION) kitchen_page(h,k->return_phase);
        else {
            if(h->request==0x3c1) (void)hmbtn_cooking_remember(h->cooking,&k->result);
            close_menu(h);
        }
        return;
    }
    if(!ready) return;
    if(k->phase==HMBTN_KITCHEN_FIRST || k->phase==HMBTN_KITCHEN_SECOND || k->phase==HMBTN_KITCHEN_CONFIRM
            || k->phase==HMBTN_KITCHEN_RESULT || k->phase==HMBTN_KITCHEN_RECIPE_CONFIRM) {
        if(pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) { const unsigned count=k->phase==HMBTN_KITCHEN_SECOND ? 3u : k->phase==HMBTN_KITCHEN_RECIPE_CONFIRM ? 4u : 2u;
            k->choice=(k->choice+((pressed&HMBTN_PAD_DOWN) ? 1u : count-1))%count;h->sound=5; }
        else if(pressed&HMBTN_PAD_CIRCLE) {
            if(k->phase==HMBTN_KITCHEN_CONFIRM) kitchen_page(h,HMBTN_KITCHEN_ITEMS);
            else close_menu(h);
        } else if(pressed&HMBTN_PAD_CROSS) {
            h->sound=0;
            if(k->phase==HMBTN_KITCHEN_RECIPE_CONFIRM) {
                if(!k->choice) kitchen_cook(h);
                else if(k->choice==1) { k->record=HMBTN_RECIPE_RECORDS;k->cursor=0;kitchen_page(h,HMBTN_KITCHEN_TOOLS); }
                else if(k->choice==2) kitchen_page(h,HMBTN_KITCHEN_RECORDS);
                else close_menu(h);
            } else if(k->phase==HMBTN_KITCHEN_FIRST) { if(k->choice) close_menu(h);else kitchen_page(h,HMBTN_KITCHEN_SECOND); }
            else if(k->phase==HMBTN_KITCHEN_SECOND) {
                if(k->choice==2) { close_menu(h);return; }
                k->cursor=k->page=0;k->record=HMBTN_RECIPE_RECORDS;
                kitchen_page(h,k->choice ? HMBTN_KITCHEN_TOOLS : HMBTN_KITCHEN_RECORDS);
            } else if(k->phase==HMBTN_KITCHEN_CONFIRM) {
                if(k->choice) kitchen_page(h,HMBTN_KITCHEN_ITEMS);else kitchen_cook(h);
            } else {
                k->record_pending=!k->choice;
                close_menu(h);
            }
        }
        return;
    }
    if(k->phase==HMBTN_KITCHEN_RECORD_VIEW) {
        if(pressed&HMBTN_PAD_START) close_menu(h);
        else if(pressed&(HMBTN_PAD_CROSS|HMBTN_PAD_CIRCLE|HMBTN_PAD_TRIANGLE)) kitchen_page(h,HMBTN_KITCHEN_RECORDS);
        return;
    }
    if(k->phase==HMBTN_KITCHEN_RECORDS) {
        const unsigned count=hmbtn_cooking_record_count(h->cooking);
        if(pressed&HMBTN_PAD_START) close_menu(h);
        else if(pressed&HMBTN_PAD_CIRCLE) kitchen_page(h,HMBTN_KITCHEN_SECOND);
        else if(count && (pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN|HMBTN_PAD_LEFT|HMBTN_PAD_RIGHT))) {
            k->cursor=(k->cursor+((pressed&HMBTN_PAD_DOWN) ? 1u : (pressed&HMBTN_PAD_UP) ? count-1
                : (pressed&HMBTN_PAD_RIGHT) ? 10u : count-(10u%count)))%count;h->sound=5;
        } else if(count && (pressed&(HMBTN_PAD_CROSS|HMBTN_PAD_TRIANGLE))) {
            k->record=kitchen_known_slot(h->cooking,k->cursor);
            const HmbtnRecipeRecord *r=&h->cooking->records[k->record];
            k->options=r->options;memcpy(k->values,r->values,sizeof(k->values));
            if(pressed&HMBTN_PAD_TRIANGLE) kitchen_page(h,HMBTN_KITCHEN_RECORD_VIEW);
            else if(hmbtn_cooking_available(h->cooking,h->inventory,k->options,k->values)) kitchen_page(h,HMBTN_KITCHEN_RECIPE_CONFIRM);
            else { k->return_phase=HMBTN_KITCHEN_RECORDS;kitchen_page(h,HMBTN_KITCHEN_DESCRIPTION);request(h,0x3ba); }
        }
        return;
    }
    if(k->phase==HMBTN_KITCHEN_TOOLS || k->phase==HMBTN_KITCHEN_SEASONING) {
        const bool seasoning=k->phase==HMBTN_KITCHEN_SEASONING;
        const unsigned count=seasoning ? 5 : 8;
        if(seasoning && (pressed&(HMBTN_PAD_START|HMBTN_PAD_CIRCLE))) {
            k->cursor=7;kitchen_page(h,HMBTN_KITCHEN_TOOLS);return;
        }
        if(pressed&HMBTN_PAD_START) { k->cursor=0;kitchen_page(h,HMBTN_KITCHEN_ITEMS); }
        else if(pressed&HMBTN_PAD_CIRCLE) {
            if(k->options) k->options=0;
            else kitchen_page(h,HMBTN_KITCHEN_SECOND);
        } else if(pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN|HMBTN_PAD_LEFT|HMBTN_PAD_RIGHT)) {
            if(seasoning) k->cursor=(k->cursor+((pressed&(HMBTN_PAD_LEFT|HMBTN_PAD_UP)) ? count-1 : 1))%count;
            else if(pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) k->cursor^=4u;
            else k->cursor=k->cursor/4*4+(k->cursor+((pressed&HMBTN_PAD_LEFT) ? 3u : 1u))%4;
            h->sound=5;
        } else if(!seasoning && (pressed&HMBTN_PAD_TRIANGLE)) {
            if(h->cooking->utensils&(1u<<k->cursor)) {
                k->return_phase=HMBTN_KITCHEN_TOOLS;kitchen_page(h,HMBTN_KITCHEN_DESCRIPTION);
                request(h,k->cursor<7 ? hmbtn_cooking_option_item(k->cursor) : 0x79);h->sound=0;
            } else h->sound=2;
        } else if(pressed&HMBTN_PAD_CROSS) {
            if(!seasoning && k->cursor==7) {
                if(h->cooking->utensils&0x80) { k->cursor=0;kitchen_page(h,HMBTN_KITCHEN_SEASONING); } else h->sound=2;
            } else {
                const unsigned bit=1u<<(k->cursor+(seasoning ? 7u : 0));
                if(hmbtn_cooking_allowed_options(h->cooking)&bit) { k->options^=bit;h->sound=0; } else h->sound=2;
            }
        }
        return;
    }
    const HmbtnStoredTool *item=hmbtn_cooking_source(h->inventory,k->page,k->cursor);
    if(pressed&HMBTN_PAD_START) {
        for(unsigned i=0;i<8;++i) if(k->values[i]) { kitchen_page(h,HMBTN_KITCHEN_CONFIRM);return; }
        h->sound=2;
    } else if(pressed&HMBTN_PAD_CIRCLE) {
        bool any=false;for(unsigned i=0;i<8;++i) any|=k->values[i]!=0;
        if(any) memset(k->values,0,sizeof(k->values));else { k->cursor=0;kitchen_page(h,HMBTN_KITCHEN_TOOLS); }
    } else if(pressed&HMBTN_PAD_TRIANGLE) {
        if(item && item->id) {
            k->return_phase=HMBTN_KITCHEN_ITEMS;kitchen_page(h,HMBTN_KITCHEN_DESCRIPTION);
            request(h,hmbtn_inventory_item_message(item));
        }
    } else if(pressed&HMBTN_PAD_CROSS) {
        if(!item || !item->id) { h->sound=2;return; }
        unsigned i=0;for(;i<8;++i) if(k->values[i]==item->id) { k->values[i]=0;h->sound=1;return; }
        const HmbtnFarmItem *info=hmbtn_farm_item(item->id);
        if(!info || info->category==1) { h->sound=2;return; }
        for(i=0;i<8;++i) if(!k->values[i]) { k->values[i]=item->id;h->sound=0;return; }
        h->sound=2;
    } else if(pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) { k->cursor^=4;h->sound=5; }
    else if(pressed&HMBTN_PAD_LEFT) {
        if(!(k->cursor%4)) k->page=(k->page+8)%9;
        k->cursor=(k->cursor/4)*4+(k->cursor+3)%4;h->sound=5;
    } else if(pressed&HMBTN_PAD_RIGHT) {
        if(k->cursor%4==3) k->page=(k->page+1)%9;
        k->cursor=(k->cursor/4)*4+(k->cursor+1)%4;h->sound=5;
    } else if(pressed&(HMBTN_PAD_L1|HMBTN_PAD_L2)) { k->page=(k->page+8)%9;h->sound=5; }
    else if(pressed&(HMBTN_PAD_R1|HMBTN_PAD_R2)) { k->page=(k->page+1)%9;h->sound=5; }
}

void hmbtn_house_interaction_tick(HmbtnHouseInteraction *h,uint32_t pressed,uint32_t held)
{
    h->sound=-1;
    if(h->mode==HMBTN_HOUSE_PHONE) { phone_tick(h,pressed,held);return; }
    if(h->mode==HMBTN_HOUSE_COOKING) { kitchen_tick(h,pressed,held);return; }
    if(h->mode==HMBTN_HOUSE_FREE || h->mode==HMBTN_HOUSE_SLEEP) return;
    if(h->mode==HMBTN_HOUSE_TOOLBOX_OPEN) {
        if(h->pan<640) { h->pan+=32; return; }
        h->mode=HMBTN_HOUSE_TOOLBOX; request(h,0x378); h->detail=true; h->sound=3; return;
    }
    const bool ready=h->message.phase==HMBTN_MESSAGE_WAIT_END;
    const bool dialogue=h->mode==HMBTN_HOUSE_TV || h->mode==HMBTN_HOUSE_BOOK
        || ((h->mode==HMBTN_HOUSE_CALENDAR || h->mode==HMBTN_HOUSE_OPTIONS) && h->detail);
    h->sound=hmbtn_message_tick(&h->message,dialogue ? pressed : 0,held);
    if(h->mode==HMBTN_HOUSE_TV && h->tv_group==2 && h->loaded==h->request
            && (ready || h->message.phase==HMBTN_MESSAGE_WAIT_END) && !h->closing
            && h->request>=0x258 && h->request<=0x260)
        hmbtn_tv_shopping_watched(h->shopping,h->request-0x258);
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
                h->mode=HMBTN_HOUSE_TOOLBOX; h->detail=false; request(h,0x3ad+(h->storage ? 3-h->storage : 0));
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
        if(h->detail) { h->detail=false; request(h,0x3ad+(h->storage ? 3-h->storage : 0)); return; }
        if(pressed&HMBTN_PAD_CROSS) {
            h->sound=0;
            if(h->picked==255) { h->picked=h->cursor; h->picked_page=h->page; }
            else {
                if(h->storage) {
                    const unsigned destination=h->picked==h->cursor && (h->cursor<9 || h->picked_page==h->page) ? 0 : h->cursor;
                    if(hmbtn_inventory_storage_swap(h->inventory,h->storage,h->picked,h->picked_page,destination,h->page)) h->picked=255;
                    else h->sound=2;
                } else {
                    HmbtnStoredTool *a=tool(h,h->picked,h->picked_page), *b=tool(h,h->cursor,h->page);
                    const HmbtnStoredTool swap=*a; *a=*b; *b=swap;h->picked=255;
                }
            }
        } else if(pressed&HMBTN_PAD_SELECT) {
            if(h->picked!=255) { h->sound=2; return; }
            hmbtn_inventory_compact(h->storage==0 ? h->inventory->toolbox : h->storage==1 ? h->inventory->cabinet : h->inventory->icebox,hmbtn_inventory_toolbox_capacity(h->inventory));
            h->sound=0;
        } else if(pressed&HMBTN_PAD_TRIANGLE) {
            const HmbtnStoredTool *item=tool(h,h->cursor,h->page);
            if(item && item->id) { h->mode=HMBTN_HOUSE_BOOK; h->detail=true; request(h,hmbtn_inventory_item_message(item)); h->sound=0; }
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
                h->cursor=storage_links[h->cursor][(unsigned)direction*3+h->inventory->rucksack_level]; h->sound=5;
            }
            if(pressed&(HMBTN_PAD_L1|HMBTN_PAD_L2)) { h->page=(h->page+7)%8; h->sound=5; }
            if(pressed&(HMBTN_PAD_R1|HMBTN_PAD_R2)) { h->page=(h->page+1)%8; h->sound=5; }
        }
    }
    if(h->mode==HMBTN_HOUSE_CALENDAR && h->detail && h->message.phase==HMBTN_MESSAGE_CLOSED) {
        h->detail=false; request(h,0x3b4);
    }
}
