#include "farm_carry.h"
void hmbtn_farm_carried_offset(unsigned facing,bool running,int *x,int *y)
{
    /* player.c: mark_screen_offsets, groups CARRY_ITEM and POSE_4. */
    static const int offsets[4][2]={{-2,-22},{0,-24},{0,-24},{-2,-22}};
    *x=running ? -1 : offsets[facing&3][0];
    *y=running ? -31 : offsets[facing&3][1];
}
/* player_collect_target, farm_tile_occupant and thrown_item_update. */
void hmbtn_farm_carry_flight(HmbtnFarmCarry *s,HmbtnFarmField *field,
    const HmbtnModel *ground,HmbtnFarmShipping *shipping,HmbtnFarmStatistics *stats)
{
    static const int step[4][2]={{0,-44},{-44,0},{0,44},{44,0}};
    s->changed=false;s->sound=-1;
    for(unsigned i=0;i<3;++i) {
        HmbtnThrownItem *t=&s->thrown[i];
        if(!t->item.id || t->area!=s->area) continue;
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
            if(surface && (hit.flags&0x80u) && hmbtn_farm_shipping_add(shipping,stats,t->item.id)) {
                t->item=(HmbtnStoredTool){0};s->sound=0x13;continue;
            }
            int c,r;bool placed=false;
            const HmbtnFarmItem *info=hmbtn_farm_item(t->item.id);
            if(field && info && (info->class_flags&0x10u)
                    && hmbtn_farm_world_cell(t->position,&c,&r)) {
                unsigned at=(unsigned)(r*45+c);HmbtnFarmTile *tile=&field->cells[at];
                if(field->active[at] && tile->value>=0x167 && tile->value<=0x169) {
                    tile->value=t->item.id;tile->kind=0;tile->growth_flags=0;
                    s->changed=true;placed=true;
                }
            }
            t->item=(HmbtnStoredTool){0};if(!placed) ++s->discarded;
        }
    }
}
bool hmbtn_farm_carry_tick(HmbtnFarmCarry *s,HmbtnHouseInteraction *h,
    HmbtnFarmField *f,HmbtnIntroActor *p,uint32_t pressed)
{
    s->sound=-1;s->changed=false;
    if(s->phase) {
        if(!p->animation_done) return true;
        if(s->phase>=2) {
            unsigned i=s->phase-2;
            s->thrown[i]=(HmbtnThrownItem){h->held_item,
                {p->position[0],p->position[1]-300,p->position[2]},p->position[1],0,p->facing,s->area};
            h->held_item=(HmbtnStoredTool){0};s->sound=9;
        }
        s->phase=0;p->sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->facing,0};p->animation_done=false;
        return true;
    }
    if(!(pressed&HMBTN_PAD_CROSS) || p->facing>3) return false;
    if(h->held_item.id) {
        const HmbtnFarmItem *item=hmbtn_farm_item(h->held_item.id);
        /* Never discard tools, seeds, livestock or unported estate containers. */
        if(!item || !(item->flags&0x8000) || h->held_item.count>1) return false;
        for(unsigned i=0;i<3;++i) if(!s->thrown[i].item.id) {
            s->phase=i+2;p->sprite=(HmbtnAnimationPlayer){(uint16_t)(23+p->facing),0,0};
            p->animation_done=false;return true;
        }
        return true;
    }
    if(!f) return false;
    int c,r;
    if(!hmbtn_farm_world_cell(p->position,&c,&r)) return false;
    static const int step[4][2]={{0,1},{-1,0},{0,-1},{1,0}};
    c+=step[p->facing][0];r+=step[p->facing][1];
    if(c<0 || c>=45 || r<0 || r>=25) return false;
    unsigned index=(unsigned)(r*45+c);HmbtnFarmTile *tile=&f->cells[index];
    const HmbtnFarmItem *item=hmbtn_farm_item(tile->value);
    if(!f->active[index] || !item || (item->flags!=0x8000 && item->flags!=0x8010)) return false;
    h->held_item=(HmbtnStoredTool){tile->value,0,1};
    tile->value=tile->ground;tile->kind=0;tile->growth_flags=0;
    s->changed=true;s->sound=7;s->phase=1;
    p->sprite=(HmbtnAnimationPlayer){(uint16_t)(11+p->facing),0,0};p->animation_done=false;
    return true;
}
