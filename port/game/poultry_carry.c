/* player_collect_target, player_use_held_item and farm_place_held.
 * The hand references the one reserved animal record; it never creates a copy. */
#include "poultry.h"
#include <stdlib.h>
#include <string.h>

unsigned hmbtn_poultry_held_slot(const HmbtnPoultry *p)
{
    for(unsigned i=0;i<HMBTN_HENS;++i) if(p->hens[i].occupied && p->hens[i].held) return i;
    return HMBTN_HENS;
}
static bool on_ground(const HmbtnHen *h,unsigned area)
{
    return h->occupied && !h->dead && !h->held && !h->naming_pending
        && h->stage!=HMBTN_HEN_EGG && h->area==area;
}
bool hmbtn_poultry_occupies(const HmbtnPoultry *p,unsigned area,const int32_t point[3])
{
    for(unsigned i=0;i<HMBTN_HENS;++i) {
        const HmbtnHen *h=&p->hens[i];
        if(on_ground(h,area) && abs(h->actor.position[0]-point[0])<200
                && abs(h->actor.position[2]-point[2])<200) return true;
    }
    return false;
}
static unsigned item(const HmbtnHen *h) { return h->stage==HMBTN_HEN_CHICK ? 0x10A : 0x109; }
static void front(const HmbtnIntroActor *a,int32_t point[3])
{
    /* player_facing_point(mode 2), facing_steps[4..7]. */
    static const int step[4][2]={{0,-300},{-300,0},{0,300},{300,0}};
    memcpy(point,a->position,sizeof(a->position));
    point[0]+=step[a->facing&3u][0];point[2]+=step[a->facing&3u][1];
}
static bool drop_point(const HmbtnPoultry *p,unsigned area,const HmbtnIntroActor *a,
    const HmbtnModel *ground,const HmbtnFarmField *field,int32_t point[3])
{
    front(a,point);
    /* Water, shipping surfaces, blocked terrain/field occupants and other animals. */
    return !hmbtn_poultry_occupies(p,area,point)
        && !hmbtn_farm_field_probe(field,point) && hmbtn_farm_settle(field,ground,point,0xC087,200);
}
static void put_down(HmbtnHen *h,HmbtnHouseInteraction *inventory,unsigned area,const int32_t point[3])
{
    h->held=false;h->area=area;h->actor.visible=true;h->timer=0;
    memcpy(h->actor.position,point,sizeof(h->actor.position));
    h->actor.sprite=(HmbtnAnimationPlayer){0,(uint16_t)h->actor.facing,0};
    inventory->held_item=(HmbtnStoredTool){0};
}
void hmbtn_poultry_carry_sync(HmbtnPoultry *p,HmbtnHouseInteraction *inventory)
{
    const unsigned slot=hmbtn_poultry_held_slot(p);
    if(slot==HMBTN_HENS) return;
    HmbtnHen *h=&p->hens[slot];
    if(h->dead) {
        h->held=false;inventory->held_item=(HmbtnStoredTool){0};p->carry_phase=0;return;
    }
    inventory->held_item=(HmbtnStoredTool){(uint16_t)item(h),0,1};
}
void hmbtn_poultry_release(HmbtnPoultry *p,HmbtnHouseInteraction *inventory,const HmbtnIntroActor *a,
    unsigned area,const HmbtnModel *ground)
{
    const unsigned slot=hmbtn_poultry_held_slot(p);if(slot==HMBTN_HENS) return;
    int32_t point[3];front(a,point);HmbtnGroundHit hit;
    if(ground && hmbtn_ground_find(ground,point,0,200,&hit)) point[1]=hit.height;
    else memcpy(point,a->position,sizeof(point));
    put_down(&p->hens[slot],inventory,area,point);p->carry_phase=0;
}
bool hmbtn_poultry_carry_tick(HmbtnPoultry *p,HmbtnHouseInteraction *inventory,HmbtnIntroActor *a,
    unsigned area,const HmbtnModel *ground,const HmbtnFarmField *field,uint32_t pressed)
{
    p->sound=-1;const unsigned slot=hmbtn_poultry_held_slot(p);
    if(p->carry_phase) {
        if(!a->animation_done) return true;
        int32_t point[3];
        if(p->carry_phase==2 && slot<HMBTN_HENS && drop_point(p,area,a,ground,field,point)) {
            put_down(&p->hens[slot],inventory,area,point);p->sound=9;
        }
        p->carry_phase=0;
        a->sprite=(HmbtnAnimationPlayer){(uint16_t)(inventory->held_item.id ? 41 : 0),(uint16_t)a->facing,0};
        a->animation_done=false;return true;
    }
    if(!(pressed&HMBTN_PAD_CROSS) || a->facing>3) return false;
    if(slot<HMBTN_HENS) {
        int32_t point[3];
        if(drop_point(p,area,a,ground,field,point)) {
            p->carry_phase=2;a->sprite=(HmbtnAnimationPlayer){(uint16_t)(23+a->facing),0,0};a->animation_done=false;
        }
        return true; /* A refused drop conserves both the hand and its animal. */
    }
    if(inventory->held_item.id) return false;
    static const int step[4][2]={{0,-128},{-128,0},{0,128},{128,0}};
    for(unsigned i=0;i<HMBTN_HENS;++i) {
        HmbtnHen *h=&p->hens[i];
        if(!on_ground(h,area) || abs(h->actor.position[0]-a->position[0]-step[a->facing][0])>=200
                || abs(h->actor.position[2]-a->position[2]-step[a->facing][1])>=200) continue;
        h->held=true;h->actor.visible=false;h->timer=0;h->praise=0;
        inventory->held_item=(HmbtnStoredTool){(uint16_t)item(h),0,1};
        p->carry_phase=1;p->sound=7;
        a->sprite=(HmbtnAnimationPlayer){(uint16_t)(37+a->facing),0,0};a->animation_done=false;return true;
    }
    return false;
}
void hmbtn_poultry_hour(HmbtnPoultry *p,const HmbtnFarmClock *clock,unsigned weather)
{
    for(unsigned i=0;i<HMBTN_HENS;++i) {
        HmbtnHen *h=&p->hens[i];
        if(!h->occupied || h->dead || h->stage!=HMBTN_HEN_ADULT || h->area!=0 || clock->season==3) continue;
        /* PLACE_FARM is 1. The source compares weather against that place;
         * preserve this predicate rather than inventing new weather rules. */
        if(weather!=1) { if(h->grazing<5) ++h->grazing;if(h->grazing>=5) h->fed_today=true; }
        else h->affection=h->affection>5 ? h->affection-5 : 0;
    }
}
