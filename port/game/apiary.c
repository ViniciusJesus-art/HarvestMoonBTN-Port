#include "apiary.h"
#include "inventory.h"
#include "psx_data.h"
static int half(const uint8_t *p)
{ unsigned n=hmbtn_read_le16(p);return n<32768 ? (int)n : (int)n-65536; }
bool hmbtn_apiary_tick(HmbtnApiary *a,HmbtnFarmFeed *feed,HmbtnInventory *inventory,
    HmbtnIntroActor *p,const uint8_t *section,size_t size,uint32_t pressed)
{
    if(!a || !feed || !inventory || !p || feed->phase || p->facing>3
            || !(pressed&HMBTN_PAD_CROSS) || !section || size<16+50*44) return false;
    static const int delta[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
    const int32_t x=p->position[0]+p->velocity[0]+delta[p->facing][0];
    const int32_t z=p->position[2]+p->velocity[2]+delta[p->facing][1];
    for(unsigned i=0;i<50;++i) {
        const uint8_t *q=section+16+i*44;
        if(!(hmbtn_read_le16(q)&0x4000u) || hmbtn_read_le16(q+2)!=47
                || x<=half(q+14) || x>=half(q+20) || z<=half(q+18) || z>=half(q+24)
                || p->position[1]<=half(q+16)-128 || p->position[1]>=half(q+22)+120) continue;
        /* A matched zone consumes the action even with occupied hands or no
         * honey. The original does not throw the held item at the tree. */
        if(a->owned && !a->collected && (p->facing==1 || p->facing==2)
                && hmbtn_inventory_hold(inventory,(HmbtnStoredTool){151,0,1})) {
            a->collected=true;feed->phase=1;feed->sound=7;feed->request=0;
            p->sprite=(HmbtnAnimationPlayer){(uint16_t)(11+p->facing),0,0};p->animation_done=false;
        } else feed->sound=-1;
        return true;
    }
    return false;
}
