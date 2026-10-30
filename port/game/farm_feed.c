/* player_zone_*_exchange/place_* and estate_feed_bins_clear. */
#include "farm_feed.h"
#include "psx_data.h"
#include <string.h>
#include "feed_positions.inc"
static int half(const uint8_t *p)
{
    unsigned n=hmbtn_read_le16(p);return n<32768 ? (int)n : (int)n-65536;
}
void hmbtn_farm_feed_new_day(HmbtnFarmFeed *f)
{
    memset(f->barn,0,sizeof(f->barn));memset(f->coop,0,sizeof(f->coop));
}
bool hmbtn_farm_feed_position(bool barn,unsigned bin,int32_t position[3])
{
    if(bin>=(barn ? 22u : 10u) || !position) return false;
    memcpy(position,feed_positions[bin+(barn ? 0 : 22)],3*sizeof(*position));return true;
}
uint32_t hmbtn_farm_feed_bins(const uint8_t *section,size_t size,bool barn)
{
    if(!section || size<16+50*44) return 0;
    uint32_t mask=0;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *zone=section+16+i*44;
        const unsigned index=hmbtn_read_le16(zone+4);
        if(hmbtn_read_le16(zone)==0x4000 && hmbtn_read_le16(zone+2)==(barn ? 41u : 40u)
                && index<(barn ? 22u : 10u)) mask|=1u<<index;
    }
    return mask;
}
bool hmbtn_farm_feed_tick(HmbtnFarmFeed *f,HmbtnHouseInteraction *h,HmbtnIntroActor *p,
    const uint8_t *section,size_t size,uint32_t pressed)
{
    f->sound=-1;f->request=0;
    if(f->phase) {
        if(p->animation_done) {
            if(f->phase==2) h->inventory->held_item=(HmbtnStoredTool){0};
            f->phase=0;p->sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->facing,0};p->animation_done=false;
        }
        return true;
    }
    if(!(pressed&HMBTN_PAD_CROSS) || !section || size<16+50*44 || p->facing>3) return false;
    static const int delta[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
    const int32_t x=p->position[0]+delta[p->facing][0],z=p->position[2]+delta[p->facing][1];
    for(unsigned i=0;i<50;++i) {
        const uint8_t *zone=section+16+i*44;const unsigned kind=hmbtn_read_le16(zone+2);
        if(hmbtn_read_le16(zone)!=0x4000 || (kind!=9 && kind!=26 && kind!=40 && kind!=41)
                || x<=half(zone+14) || x>=half(zone+20) || z<=half(zone+18) || z>=half(zone+24)
                || p->position[1]<=half(zone+16)-128 || p->position[1]>=half(zone+22)+120) continue;
        const unsigned facing=hmbtn_read_le16(zone+8);
        const bool bin=kind==40 || kind==41,barn=kind==9 || kind==41;
        const unsigned item=barn ? 0x68 : 0x69,stock=barn ? 0 : 2;
        if(bin && h->inventory->held_item.id!=item) { f->request=barn ? 0x1E4 : 0x1E5;return true; }
        if(kind!=9 && p->facing!=facing
                && !(bin && ((facing==6 && (p->facing&1)) || (facing==7 && !(p->facing&1))))) return true;
        if(bin) {
            const unsigned index=hmbtn_read_le16(zone+4);
            if(index>=(barn ? 22u : 10u)) return true;
            /* The original consumes another unit even when the bin is full. */
            if(barn) f->barn[index]=true;else f->coop[index]=true;
            f->phase=2;
        } else if(!h->inventory->held_item.id && h->inventory->supplies[stock]) {
            --h->inventory->supplies[stock];h->inventory->held_item=(HmbtnStoredTool){(uint16_t)item,0,1};f->phase=1;
        } else if(h->inventory->held_item.id==item) {
            if(h->inventory->supplies[stock]<999) ++h->inventory->supplies[stock];
            f->phase=2;
        } else { f->request=barn ? 0x19C : 0x19F;return true; }
        p->sprite=(HmbtnAnimationPlayer){(uint16_t)((f->phase==1 ? 11 : 23)+p->facing),0,0};
        p->animation_done=false;f->sound=7;return true;
    }
    return false;
}

/* player_act_zone_front: ZONE_POND 45 and ZONE_FISH_FOOD_STORE 10.
 * Reuse the feed pickup phase/ANI; no separate interaction controller. */
bool hmbtn_farm_feed_pond_tick(HmbtnFarmFeed *f,HmbtnPond *pond,HmbtnHouseInteraction *h,
    HmbtnIntroActor *p,unsigned season,const uint8_t *section,size_t size,uint32_t pressed)
{
    if(f->phase || !pond || !(pressed&HMBTN_PAD_CROSS) || !section || size<16+50*44 || p->facing>3) return false;
    static const int delta[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
    const int32_t x=p->position[0]+p->velocity[0]+delta[p->facing][0],z=p->position[2]+p->velocity[2]+delta[p->facing][1];
    for(unsigned i=0;i<50;++i) {
        const uint8_t *q=section+16+i*44;const unsigned kind=hmbtn_read_le16(q+2);
        if(!(hmbtn_read_le16(q)&0x4000u) || (kind!=10 && kind!=45) || p->facing!=hmbtn_read_le16(q+8)
                || x<=half(q+14) || x>=half(q+20) || z<=half(q+18) || z>=half(q+24)
                || p->position[1]<=half(q+16)-128 || p->position[1]>=half(q+22)+120) continue;
        if(h->inventory->held_item.id) return kind==10;
        unsigned item=0;
        if(kind==45) { if(season==3) return false;item=hmbtn_pond_take(pond); }
        else if(h->inventory->supplies[3]) { --h->inventory->supplies[3];item=132; }
        if(!item) return false;
        h->inventory->held_item=(HmbtnStoredTool){(uint16_t)item,0,1};f->phase=1;f->sound=7;f->request=0;
        p->sprite=(HmbtnAnimationPlayer){(uint16_t)(11+p->facing),0,0};p->animation_done=false;return true;
    }
    return false;
}

/* Machine submission shares the feed modal lock/base ANI. Phases 3..5 retain
 * the authoritative held ingredient until the throw animation completes;
 * phase 1 then shows its replacement, with no rucksack-space requirement. */
bool hmbtn_farm_feed_makers_tick(HmbtnFarmFeed *f,const HmbtnMakers *m,HmbtnHouseInteraction *h,
    HmbtnIntroActor *p,unsigned area,const uint8_t *section,size_t size,uint32_t pressed)
{
    f->sound=-1;f->request=0;
    static const unsigned kinds[3]={14,33,15};
    if(f->phase>=3 && f->phase<=5) {
        if(p->animation_done) {
            const unsigned output=hmbtn_makers_product(kinds[f->phase-3],h->inventory->held_item.id);
            if(output) {
                h->inventory->held_item=(HmbtnStoredTool){(uint16_t)output,0,1};
                p->sprite=(HmbtnAnimationPlayer){(uint16_t)(11+p->facing),0,0};f->phase=1;f->sound=7;
            } else { f->phase=0;p->sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->facing,0}; }
            p->animation_done=false;
        }
        return true;
    }
    if(f->phase || !m || !(pressed&HMBTN_PAD_CROSS) || !section || size<16+50*44 || p->facing>3) return false;
    static const int delta[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
    const int32_t x=p->position[0]+delta[p->facing][0],z=p->position[2]+delta[p->facing][1];
    for(unsigned i=0;i<50;++i) {
        const uint8_t *q=section+16+i*44;const unsigned kind=hmbtn_read_le16(q+2);
        const unsigned type=kind==14 ? 0 : kind==33 ? 1 : kind==15 ? 2 : 3;
        if(hmbtn_read_le16(q)!=0x4000 || type==3 || !(m->owned&(1u<<type))
                || area!=(type ? 96u : 112u) || x<=half(q+14) || x>=half(q+20)
                || z<=half(q+18) || z>=half(q+24) || p->position[1]<=half(q+16)-128
                || p->position[1]>=half(q+22)+120) continue;
        if(!hmbtn_makers_product(kind,h->inventory->held_item.id)) { f->request=0x1a2+type;return true; }
        if(p->facing!=hmbtn_read_le16(q+8)) return true;
        f->phase=3+type;p->animation_done=false;
        p->sprite=(HmbtnAnimationPlayer){(uint16_t)(23+p->facing),0,0};return true;
    }
    return false;
}
