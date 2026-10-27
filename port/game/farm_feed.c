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
