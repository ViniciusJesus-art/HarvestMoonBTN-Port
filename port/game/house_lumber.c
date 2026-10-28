/* Adapted from player_zone_lumber_stock, player_act_zone_front's zone 18 and
 * player_run_action's PLAYER_MODE_DELIVER_LUMBER; no inventory copy. */
#include "house_lumber.h"
#include "psx_data.h"
#include "inventory.h"
#include <limits.h>
static int half(const uint8_t *p)
{
    unsigned n=hmbtn_read_le16(p);return n<32768 ? (int)n : (int)n-65536;
}
bool hmbtn_house_lumber_tick(HmbtnHouseLumber *l,HmbtnInventory *inventory,HmbtnIntroActor *p,
    HmbtnFarmAction *condition,bool *lit,const uint8_t *section,size_t size,uint32_t pressed,int *sound)
{
    *sound=-1;
    if(l->phase) {
        if(p->animation_done) {
            if(l->phase!=1) inventory->held_item=(HmbtnStoredTool){0};
            if(l->phase==3) {
                *lit=true;
                if(l->deliveries<UINT_MAX) ++l->deliveries;
                if(l->deliveries==1) condition->fatigue=condition->fatigue>5 ? condition->fatigue-5 : 0;
            }
            l->phase=0;p->animation_done=false;p->sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->facing,0};
        }
        return true;
    }
    if(!(pressed&HMBTN_PAD_CROSS) || !section || size<0x2000 || p->facing>3) return false;
    static const int delta[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
    const int32_t x=p->position[0]+delta[p->facing][0],z=p->position[2]+delta[p->facing][1];
    for(unsigned i=0;i<50;++i) {
        const uint8_t *zone=section+16+i*44;const unsigned kind=hmbtn_read_le16(zone+2);
        if(hmbtn_read_le16(zone)!=0x4000 || (kind!=13 && kind!=18)
                || x<=half(zone+14) || x>=half(zone+20) || z<=half(zone+18) || z>=half(zone+24)
                || p->position[1]<=half(zone+16)-128 || p->position[1]>=half(zone+22)+120) continue;
        const unsigned held=inventory->held_item.id;
        const bool wood=held==0x3f || held==0x13d;
        if(kind==18) {
            /* This zone consumes input even with another item or outside winter. */
            if(p->facing!=hmbtn_read_le16(zone+8) || !wood || hmbtn_read_le16(section+4)!=91) return true;
            l->phase=3;
        } else if(wood) {
            if(inventory->supplies[1]<hmbtn_inventory_lumber_limit()) ++inventory->supplies[1];
            l->phase=2;*sound=7;
        } else if(!held && inventory->supplies[1]) {
            --inventory->supplies[1];inventory->held_item=(HmbtnStoredTool){0x3f,0,1};
            l->phase=1;*sound=7;
        } else return true;
        p->sprite=(HmbtnAnimationPlayer){(uint16_t)((l->phase==1 ? 11 : 23)+p->facing),0,0};
        p->animation_done=false;return true;
    }
    return false;
}
