#include "farm_menu.h"
#include "poultry.h"
#include "construction.h"
#include <string.h>
#include "storage_spots.inc"
const HmbtnStorageSpot *hmbtn_storage_spot(unsigned i) { return i<18 ? &storage_spots[i] : NULL; }
void hmbtn_farm_menu_init(HmbtnFarmMenu *m,const HmbtnEconomy *economy)
{
    memset(m,0,sizeof(*m));m->sound=-1;m->marked=255;m->economy=economy;
}
bool hmbtn_farm_menu_open(HmbtnFarmMenu *m,uint32_t pressed)
{
    if(m->mode!=HMBTN_MENU_NONE || !(pressed&(HMBTN_PAD_START|HMBTN_PAD_TRIANGLE))) return false;
    m->mode=pressed&HMBTN_PAD_START ? HMBTN_MENU_STATUS : HMBTN_MENU_RUCKSACK;
    m->page=m->cycle=m->cursor=m->repeat_frames=m->list_top=0;m->previous_arrows=0;
    m->marked=255;m->detail=m->closing=false;m->loaded=0;m->message=(HmbtnMessage){0};
    m->request=m->mode==HMBTN_MENU_RUCKSACK ? 0x3ac : 0;
    m->phase=m->mode==HMBTN_MENU_STATUS ? 1 : 3;m->fade=0;m->sound=3;return true;
}
void hmbtn_farm_menu_tick(HmbtnFarmMenu *m,HmbtnInventory *h,uint32_t pressed,uint32_t held)
{
    m->sound=-1;
    if(m->mode==HMBTN_MENU_NONE) return;
    if(m->mode==HMBTN_MENU_STATUS) {
        if(m->phase!=3) {
            if(++m->fade>=10) {
                m->fade=0;
                if(m->phase==5) { m->mode=HMBTN_MENU_NONE;return; }
                ++m->phase;
            }
            return;
        }
        if(pressed&(HMBTN_PAD_START|HMBTN_PAD_CIRCLE|HMBTN_PAD_CROSS)) { m->phase=4;m->fade=0;m->sound=3;return; }
        if(pressed&(HMBTN_PAD_RIGHT|HMBTN_PAD_LEFT)) {
            do { m->page=(m->page+((pressed&HMBTN_PAD_RIGHT) ? 1u : 7u))%8; }
            while(m->page>=2 && m->page<=4 && !m->stats.animals[m->page-2]);
            m->list_top=0;m->sound=5;
        }
        if(m->page==2 && m->poultry) {
            const unsigned count=hmbtn_poultry_count(m->poultry);
            if(count<=7) m->list_top=0;
            else {
                const unsigned last=count-7;
                if(m->list_top>last) m->list_top=0;
                if(pressed&HMBTN_PAD_UP) { m->list_top=m->list_top ? m->list_top-1 : last;m->sound=5; }
                else if(pressed&HMBTN_PAD_DOWN) { m->list_top=m->list_top<last ? m->list_top+1 : 0;m->sound=5; }
            }
        }
        m->cycle=(m->cycle+1)%120;return;
    }
    m->sound=hmbtn_message_tick(&m->message,m->detail ? pressed : 0,held);
    if(m->closing) {
        if(m->message.phase==HMBTN_MESSAGE_CLOSED) m->mode=HMBTN_MENU_NONE;
        return;
    }
    if(m->detail) {
        if(m->message.phase==HMBTN_MESSAGE_CLOSED) { m->detail=false;m->request=0x3ac;m->loaded=0; }
        return;
    }
    if(m->message.phase!=HMBTN_MESSAGE_WAIT_END) return;
    if(pressed&HMBTN_PAD_START || ((pressed&HMBTN_PAD_CIRCLE) && m->marked==255)) {
        m->closing=true;m->message.phase=HMBTN_MESSAGE_CLOSING;m->message.box_frame=0;m->sound=3;return;
    }
    if(pressed&HMBTN_PAD_CIRCLE) { m->marked=255;m->sound=1;return; }
    HmbtnStoredTool *item=hmbtn_inventory_slot(h,m->cursor);
    if(pressed&HMBTN_PAD_TRIANGLE) {
        if(item && item->id) { m->detail=true;m->request=item->id;m->loaded=0;m->sound=0; }
        else m->sound=2;
        return;
    }
    if(pressed&HMBTN_PAD_CROSS) {
        if(m->marked==255) { m->marked=m->cursor;m->sound=0; }
        else {
            const unsigned dest=m->marked==m->cursor ? (m->cursor<9 ? 0u : 9u) : m->cursor;
            if(hmbtn_inventory_swap(h,m->marked,dest)) { m->marked=255;m->sound=0; }
            else m->sound=2;
        }
        return;
    }
    uint32_t arrows=held&0xf000u;
    if(arrows!=m->previous_arrows) m->repeat_frames=0;
    else if(arrows && ++m->repeat_frames>=16 && (m->repeat_frames-16)%4==0) pressed|=arrows;
    m->previous_arrows=arrows;
    unsigned dir=4;
    if(pressed&HMBTN_PAD_UP) dir=0;else if(pressed&HMBTN_PAD_DOWN) dir=1;
    else if(pressed&HMBTN_PAD_LEFT) dir=2;else if(pressed&HMBTN_PAD_RIGHT) dir=3;
    if(dir<4) {
        if(m->marked!=255) dir+=4;
        unsigned next=storage_spots[m->cursor].next[h->rucksack_level+dir*3];
        if(hmbtn_inventory_slot(h,next)) { m->cursor=next;m->sound=5; }
    }
}

/* status_pages.c: represented building levels contribute twenty each.
 * Barn/greenhouse levels remain at their actual initial values. */
unsigned hmbtn_farm_menu_building_score(const HmbtnFarmMenu *m)
{
    return m ? ((m->poultry && m->poultry->upgraded ? 1u : 0u)+(m->construction ? m->construction->house_level : 0u))*20u : 0;
}
