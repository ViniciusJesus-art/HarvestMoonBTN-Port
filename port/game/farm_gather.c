/* estate.c's seasonal substitutions, collectible records and per-day reset. */
#include "farm_gather.h"
#include "inventory.h"
#include <stdlib.h>
#include "gather_tables.inc"
const HmbtnGatherRecord *hmbtn_farm_gather_record(unsigned i) { return i<197 ? &gather_records[i] : NULL; }
static bool supported(unsigned i)
{ return (i>=1 && i<=3) || (i>=125 && i<=172); }
unsigned hmbtn_farm_gather_item(const HmbtnFarmGather *s,unsigned i,unsigned location,unsigned season)
{
    /* Farm apples (run 0) share pickup/state with mountain/woodcutter forage.
     * Other fixture records retain their own interaction controllers. */
    if(!s || !supported(i) || season>3 || s->taken[i]) return 0;
    const HmbtnGatherRecord *r=&gather_records[i];
    if(r->location!=location || !(r->flags&0x80) || !(r->flags&(2u<<season))) return 0;
    unsigned id=r->flags&0x2000 ? gather_kinds[season][0] : r->flags&0x800 ? gather_kinds[season][1]
        : r->flags&0x1000 ? gather_kinds[season][2] : r->item;
    const HmbtnFarmItem *item=hmbtn_farm_item(id);
    return item && id && (item->flags&0x8000) ? id : 0;
}
void hmbtn_farm_gather_new_day(HmbtnFarmGather *s)
{
    /* estate_record_table_enable: the three apples have daily bit 0x20.
     * Season eligibility is checked separately, including on Fall 1. */
    for(unsigned i=1;i<=3;++i) if(gather_records[i].flags&0x20) s->taken[i]=false;
    for(unsigned i=125;i<=172;++i) if(gather_records[i].flags&0x400) s->taken[i]=false;
}
bool hmbtn_farm_gather_take(HmbtnFarmGather *s,unsigned location,unsigned season,
    HmbtnHouseInteraction *h,HmbtnFarmCarry *carry,HmbtnIntroActor *p)
{
    if(h->inventory->held_item.id || carry->phase || p->facing>3) return false;
    static const int step[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
    for(unsigned i=0;i<HMBTN_GATHER_RECORDS;++i) {
        const unsigned id=hmbtn_farm_gather_item(s,i,location,season);
        if(!id) continue;
        const HmbtnGatherRecord *r=&gather_records[i];
        if(abs(r->position[0]-p->position[0]-step[p->facing][0])>=200
                || abs(r->position[2]-p->position[2]-step[p->facing][1])>=200
                || abs(r->position[1]-p->position[1])>=200) continue;
        if(!hmbtn_inventory_hold(h->inventory,(HmbtnStoredTool){(uint16_t)id,0,1})) return false;
        s->taken[i]=true;
        carry->phase=1;carry->sound=7;p->animation_done=false;
        p->sprite=(HmbtnAnimationPlayer){(uint16_t)(11+p->facing),0,0};return true;
    }
    return false;
}
int hmbtn_estate_item_art(unsigned item)
{
    /* estate_action_status and estate_code_value. */
    switch(item) {
    case 0x57:return 0x12; /* estate_code_lookup(APPLE), fall farm sprite. */
    case 0x122:case 0x123:case 0x124:return 5;case 0xE8:return 6;
    case 0x68:return 4;case 0x69:return 7;
    default:break;
    }
    for(unsigned i=0;i<sizeof(gather_art)/sizeof(*gather_art);++i)
        if(gather_art[i].item==item) return (int)gather_art[i].art;
    return -1;
}
