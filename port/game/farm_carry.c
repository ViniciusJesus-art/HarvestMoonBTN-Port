#include "farm_carry.h"
#include "inventory.h"
#include "basket.h"
#include "psx_data.h"
#include <stdio.h>
void hmbtn_farm_carried_offset(unsigned facing,bool running,int *x,int *y)
{
    /* player.c: mark_screen_offsets, groups CARRY_ITEM and POSE_4. */
    static const int offsets[4][2]={{-2,-22},{0,-24},{0,-24},{-2,-22}};
    *x=running ? -1 : offsets[facing&3][0];
    *y=running ? -31 : offsets[facing&3][1];
}
/* player_collect_target, farm_tile_occupant and thrown_item_update. */
void hmbtn_farm_carry_flight(HmbtnFarmCarry *s,HmbtnFarmField *field,
    const HmbtnModel *ground,HmbtnEconomy *economy)
{
    static const int step[4][2]={{0,-44},{-44,0},{0,44},{44,0}};
    s->changed=s->small_fish_placed=false;s->sound=-1;s->request=0;
    for(unsigned i=0;i<3;++i) {
        HmbtnThrownItem *t=&s->thrown[i];
        if(!t->item.id || t->area!=s->area) continue;
        if(t->phase) {
            if(t->phase==3) continue; /* Tick the decoded splash ANI separately. */
            if(t->timer && !--t->timer) {
                if(t->phase==1) { t->phase=2;t->timer=4; }
                else t->item=(HmbtnStoredTool){0};
            }
            continue;
        }
        t->position[0]+=step[t->facing][0];t->position[2]+=step[t->facing][1];
        t->position[1]+=t->velocity;t->velocity+=9;
        if(t->velocity>45) t->velocity=45;
        HmbtnGroundHit hit={0};bool surface=false;
        if(ground) {
            /* player_settle_ground_tolerance, mode 0 includes shipping bit 0x80. */
            static const unsigned tolerances[]={4,64,320,960,1720};
            const int32_t probe[3]={t->position[0],t->ground,t->position[2]};
            for(unsigned j=0;j<5 && !surface;++j)
                surface=hmbtn_ground_find(ground,probe,0,tolerances[j],&hit);
            t->ground=surface ? hit.height : 5001;
        }
        if(t->position[1]>=t->ground) {
            int32_t landing[3]={t->position[0],t->ground,t->position[2]};
            /* farm_status_kind gives pond bit 4 priority over other waters.
             * The landing consumes even a refused fish, as in the original. */
            if(surface && (hit.flags&0x46u)) {
                if((hit.flags&4u) && s->pond) {
                    if(t->item.id>=161 && t->item.id<=163 && !hmbtn_pond_add(s->pond,t->item.id)) s->request=0x382;
                    if(t->item.id==132) s->pond->fed=true;
                } else if(t->area==56 && t->item.id==163 && (hit.flags&2u) && !(hit.flags&0x40u)) s->small_fish_placed=true;
                t->position[1]=t->ground;t->velocity=0;t->phase=3;t->splash=(HmbtnAnimationPlayer){23,0,0};s->sound=16;
                continue;
            }
            if(hmbtn_basket_near(s->basket,s->area,landing) && hmbtn_farm_shipping_price(economy,t->item.id)) {
                if(!hmbtn_basket_add(s->basket,economy,t->item.id)) s->request=0x383;
                t->item=(HmbtnStoredTool){0};s->sound=9;continue;
            }
            if(t->area==56 && t->item.id==0xA3 && surface && (hit.flags&2u) && !(hit.flags&0x44u)) s->small_fish_placed=true;
            if(surface && (hit.flags&0x80u) && hmbtn_farm_shipping_add(economy,t->item.id)) {
                t->item=(HmbtnStoredTool){0};s->sound=0x13;continue;
            }
            /* Reuse the field/cell/catalog placement path: only carryable
             * field objects (class bit 0x10) can return to empty soil. */
            unsigned item=t->item.id;
            if(item>=0x13b && item<=0x13d) item=item==0x13b ? 0x3a : item==0x13c ? 0x3e : 0x3f;
            const HmbtnFarmItem *info=hmbtn_farm_item(item);int c,r;
            if(field && info && (info->class_flags&0x10u) && (!surface || !(hit.flags&0x87u))
                    && hmbtn_farm_field_world_cell(field,t->position,&c,&r)) {
                const unsigned index=(unsigned)r*hmbtn_farm_field_columns(field)+(unsigned)c;
                HmbtnFarmTile *tile=&field->cells[index];
                if(field->active[index] && tile->value>=0x167 && tile->value<=0x169) {
                    tile->value=(uint16_t)item;tile->kind=0;tile->growth_flags=0;tile->watered_days=0;
                    t->item=(HmbtnStoredTool){0};s->changed=true;continue;
                }
            }
            t->position[1]=t->ground;t->velocity=0;t->phase=1;t->timer=1;
            ++s->discarded;
        }
    }
}
bool hmbtn_farm_carry_splashes(HmbtnFarmCarry *s,const HmbtnAnimation *a,char *error,size_t size)
{
    for(unsigned i=0;i<3;++i) {
        HmbtnThrownItem *t=&s->thrown[i];if(!t->item.id || t->area!=s->area || t->phase!=3) continue;
        if(!a || !a->data || t->splash.animation>=a->count[3]) { snprintf(error,size,"Missing original water landing ANI");return false; }
        const uint8_t *range=a->data+a->section[3]+t->splash.animation*4;
        const unsigned count=hmbtn_read_le16(range);
        if(t->splash.frame>=count) { snprintf(error,size,"Water landing frame outside range");return false; }
        const uint8_t *frame=a->data+a->section[4]+hmbtn_read_le16(range+2)+t->splash.frame*10;
        const bool ended=t->splash.ticks+1u>=((unsigned)frame[6]+1)/2 && (frame[7]!=0 || t->splash.frame+1u>=count);
        if(!hmbtn_animation_tick(a,&t->splash,error,size)) return false;
        if(ended) t->item=(HmbtnStoredTool){0};
    }
    return true;
}
bool hmbtn_farm_carry_tick(HmbtnFarmCarry *s,HmbtnHouseInteraction *h,
    HmbtnFarmField *f,HmbtnIntroActor *p,uint32_t pressed)
{
    s->sound=-1;s->changed=false;
    if(s->phase) {
        if(!p->animation_done) return true;
        if(s->phase>=2) {
            unsigned i=s->phase-2;
            s->thrown[i]=(HmbtnThrownItem){.item=h->inventory->held_item,
                .position={p->position[0],p->position[1]-300,p->position[2]},
                .ground=p->position[1],.facing=p->facing,.area=s->area};
            h->inventory->held_item=(HmbtnStoredTool){0};s->sound=9;
        }
        s->phase=0;p->sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->facing,0};p->animation_done=false;
        return true;
    }
    if(!(pressed&HMBTN_PAD_CROSS) || p->facing>3) return false;
    if(h->inventory->held_item.id) {
        const HmbtnFarmItem *item=hmbtn_farm_item(h->inventory->held_item.id);
        /* Never discard tools, seeds, livestock or unported estate containers. */
        if(!item || !(item->flags&0x8000) || h->inventory->held_item.count>1) return false;
        for(unsigned i=0;i<3;++i) if(!s->thrown[i].item.id) {
            s->phase=i+2;p->sprite=(HmbtnAnimationPlayer){(uint16_t)(23+p->facing),0,0};
            p->animation_done=false;return true;
        }
        return true;
    }
    if(!f) return false;
    int c,r;
    /* player_probe_target/facing_reach: classify the point in front, even
     * when the player's feet are outside the cultivated rectangle. */
    static const int step[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
    const int32_t probe[3]={p->position[0]+step[p->facing][0],p->position[1],p->position[2]+step[p->facing][1]};
    if(!hmbtn_farm_field_world_cell(f,probe,&c,&r)) return false;
    unsigned index=(unsigned)(r*(int)hmbtn_farm_field_columns(f)+c);HmbtnFarmTile *tile=&f->cells[index];
    const unsigned crop=hmbtn_farm_field_harvest_item(f,index);
    if(crop) {
        if(!hmbtn_inventory_hold(h->inventory,(HmbtnStoredTool){(uint16_t)crop,0,1})) return false;
        hmbtn_farm_field_harvest(f,index);
    } else {
    const HmbtnFarmItem *item=hmbtn_farm_item(tile->value);
    if(!f->active[index] || !item || (item->flags!=0x8000 && item->flags!=0x8010)) return false;
    if(!hmbtn_inventory_hold(h->inventory,(HmbtnStoredTool){tile->value,0,1})) return false;
    tile->value=tile->ground;tile->kind=0;tile->growth_flags=0;tile->watered_days=0;
    }
    s->changed=true;s->sound=7;s->phase=1;
    p->sprite=(HmbtnAnimationPlayer){(uint16_t)(11+p->facing),0,0};p->animation_done=false;
    return true;
}
