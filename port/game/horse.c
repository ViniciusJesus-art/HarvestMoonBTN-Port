/* horse_spawn/update_bond/frame_table, player mount/dismount and rider sync. */
#include "horse.h"
#include "psx_random.h"
#include "pad_buttons.h"
#include <stdlib.h>
#include <string.h>
static void pose(HmbtnHorse *h,unsigned action)
{
    const unsigned animation=action ? 1+(action-1)*4+h->actor.facing : 0;
    if(h->actor.sprite.animation!=animation) {
        h->actor.sprite=(HmbtnAnimationPlayer){(uint16_t)animation,0,0};h->actor.animation_done=false;
    }
    if(!action) h->actor.sprite.frame=(uint16_t)h->actor.facing;
}
static void phase(HmbtnHorse *h,unsigned next)
{ h->phase=next;h->step=h->timer=h->interrupt=0;memset(h->actor.velocity,0,sizeof(h->actor.velocity)); }
bool hmbtn_horse_spawn(HmbtnHorse *h,const HmbtnFarmClock *c)
{
    if(h->owned) return false;
    *h=(HmbtnHorse){.owned=true,.area=92,.affection=10,.year=c->year,.season=c->season,.day=c->day,.random=1};
    for(unsigned i=0;i<=HMBTN_NAME_CHARS;++i) h->name[i]=0xffff;
    h->actor.visible=true;h->actor.position[0]=460;h->actor.position[2]=600;return true;
}
void hmbtn_horse_new_day(HmbtnHorse *h,unsigned weather)
{
    if(!h->owned || h->in_event) return;
    unsigned value=h->affection+(h->called_today ? 2u : 0u);if(value>255) value=255;
    value+=h->brushed_today ? 2u : 0u;if(value>255) value=255;
    if(!h->called_today && !h->brushed_today && !h->trained_today && value) --value;
    if(h->trained_today) { h->training+=value<100 ? 1u : value<200 ? 2u : 3u;if(h->training>255) h->training=255; }
    if(h->area==0 && weather) value=value>5 ? value-5 : 0;
    h->affection=value;h->called_today=h->brushed_today=h->trained_today=false;h->praise=0;
}
void hmbtn_horse_clear(HmbtnHorse *h)
{ *h=(HmbtnHorse){0};for(unsigned i=0;i<=HMBTN_NAME_CHARS;++i) h->name[i]=0xffff; }
void hmbtn_horse_morning(HmbtnHorse *h,unsigned weather,const HmbtnFarmClock *c)
{
    hmbtn_horse_new_day(h,weather);
    if(h->owned && !h->in_event && !h->adult && h->year!=c->year && h->season==c->season && h->day<=c->day) {
        h->adult=true;pose(h,0);h->actor.animation_done=false;
    }
}
static void pick(HmbtnHorse *h,unsigned hour)
{
    const unsigned roll=hour>=6 && hour<21 && h->phase!=15 ? hmbtn_psx_random_range(&h->random,h->approaching ? 7 : 2) : 0;
    const unsigned next=hour<6 || hour>=21 ? 2 : h->phase==15 ? 1 : roll==1 ? 2 : h->approaching ? 17 : 18;
    if(h->phase!=next) phase(h,next);
}
void hmbtn_horse_tick(HmbtnHorse *h,const HmbtnDogWorld *w,unsigned hour)
{
    if(!h->owned || h->riding || h->in_event || h->area!=w->area) return;
    if(h->praise) --h->praise;
    if(h->phase==17 || h->phase==18) {
        const bool moving=hmbtn_pet_walk(&h->actor,&h->random,&h->approaching,&h->walk_timer,&h->walker_ticks,
            w,h->phase==17 ? 24 : 12,w->player->position,h->call_ticks!=0);
        pose(h,moving ? (h->phase==17 ? 3u : 1u) : 0u);
        if(h->call_ticks && ++h->call_ticks>=300) { h->call_ticks=0;h->approaching=false;phase(h,0); }
    } else {
        pose(h,h->phase==2 && h->step ? 2u : 0u);
        if(++h->timer>=(h->phase==0 ? 30u : 10u) && h->actor.animation_done) {
            h->timer=0;if(h->phase==2) h->step=1;
        }
    }
    if(++h->interrupt>=300) { pick(h,hour);h->interrupt=0; }
}
bool hmbtn_horse_target(const HmbtnHorse *h,unsigned area,const HmbtnIntroActor *p,unsigned reach)
{
    static const int delta[4][2]={{0,-1},{-1,0},{0,1},{1,0}};
    return h->owned && !h->in_event && h->area==area && p->facing<4 && abs(p->position[1]-h->actor.position[1])<128
        && abs(h->actor.position[0]-p->position[0]-delta[p->facing][0]*(int)reach)<300
        && abs(h->actor.position[2]-p->position[2]-delta[p->facing][1]*(int)reach)<300;
}
bool hmbtn_horse_blocked(const HmbtnHorse *h,unsigned area,const int32_t from[3],const int32_t to[3])
{
    if(!h->owned || h->riding || h->in_event || h->area!=area) return false;
    const int32_t *p=h->actor.position;
    return abs(to[0]-p[0])<400 && abs(to[2]-p[2])<400
        && abs(to[0]-p[0])+abs(to[2]-p[2])<abs(from[0]-p[0])+abs(from[2]-p[2]);
}
bool hmbtn_horse_call_tick(HmbtnHorse *h,HmbtnIntroActor *p,unsigned area,uint32_t pressed)
{
    if(h->whistling) {
        if(!p->animation_done) return true;
        h->whistling=false;p->sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->facing,0};p->animation_done=false;
        if(h->owned && h->area==area && hmbtn_psx_random_range(&h->random,200)<h->affection+100) {
            h->called_today=true;h->approaching=true;h->call_ticks=1;h->walk_timer=100;phase(h,17);h->track=14;
        }
        return true;
    }
    if(!(pressed&HMBTN_PAD_L2)) return false;
    h->whistling=true;p->sprite=(HmbtnAnimationPlayer){(uint16_t)(67+p->facing),0,0};p->animation_done=false;return true;
}
unsigned hmbtn_horse_talk(HmbtnHorse *h)
{ h->track=13;return h->affection>=201 ? 0x259 : 0x258; }
bool hmbtn_horse_brush(HmbtnHorse *h)
{
    if(!h->owned) return false;
    const bool first=!h->brushed_today;if(first) h->praise=45;
    h->brushed_today=true;h->track=13;return first;
}
void hmbtn_horse_rider_sync(HmbtnHorse *h,HmbtnIntroActor *p,const HmbtnHouseControl *control,unsigned area)
{
    if(!h->riding) return;
    h->area=area;memcpy(h->actor.position,p->position,sizeof(h->actor.position));
    h->actor.facing=p->facing;
    pose(h,control->jumping ? 4u : control->mode==4 ? 3u : control->mode==2 ? 1u : 0u);
    if(!h->ride_phase) {
        const unsigned animation=control->jumping ? 63+p->facing : control->mode ? 59+p->facing : 54;
        if(p->sprite.animation!=animation) {
            p->sprite=(HmbtnAnimationPlayer){(uint16_t)animation,(uint16_t)(control->mode || control->jumping ? 0 : p->facing),0};
            p->animation_done=false;
        }
    }
}
void hmbtn_horse_stop_riding(HmbtnHorse *h)
{
    if(!h->riding) return;
    h->riding=false;h->ride_phase=0;phase(h,0);pose(h,0);h->actor.animation_done=false;
}
bool hmbtn_horse_ride_action(HmbtnHorse *h,HmbtnIntroActor *p,HmbtnHouseControl *control,
    const HmbtnDogWorld *world,uint32_t pressed,int *sound)
{
    *sound=-1;
    if(h->ride_phase) {
        if(!p->animation_done) return true;
        control->mode=0;
        if(h->ride_phase==2) {
            /* player_calc_first_valid tests north, west, south, east. The horse
             * stays at the rider's old position, including when none is free. */
            static const int step[4][2]={{0,-300},{-300,0},{0,300},{300,0}};
            memcpy(h->actor.position,p->position,sizeof(h->actor.position));h->area=world->area;
            for(unsigned face=0;face<4;++face) {
                int32_t at[3]={p->position[0]+step[face][0],p->position[1],p->position[2]+step[face][1]};
                if(!hmbtn_farm_settle(world->field,world->ground,at,0xc080,200)
                        || hmbtn_farm_field_probe(world->field,at)
                        || (world->blocked && world->blocked(world->context,p->position,at))) continue;
                memcpy(p->position,at,sizeof(at));p->facing=face;break;
            }
            hmbtn_horse_stop_riding(h);
        }
        h->ride_phase=0;p->animation_done=false;
        p->sprite=(HmbtnAnimationPlayer){(uint16_t)(h->riding ? 54 : 0),(uint16_t)p->facing,0};
        hmbtn_horse_rider_sync(h,p,control,world->area);return true;
    }
    if(!(pressed&HMBTN_PAD_CROSS)) return false;
    if(h->riding) {
        if(control->jumping || control->exit_pending) return true;
        h->ride_phase=2;p->sprite=(HmbtnAnimationPlayer){176,0,0};
    } else {
        if(!h->adult || !hmbtn_horse_target(h,world->area,p,224)) return false;
        h->riding=h->trained_today=true;h->ride_phase=1;phase(h,0);
        p->facing=h->actor.facing;p->sprite=(HmbtnAnimationPlayer){(uint16_t)(50+p->facing),0,0};*sound=6;
    }
    control->mode=0;p->animation_done=false;
    hmbtn_horse_rider_sync(h,p,control,world->area);return true;
}
