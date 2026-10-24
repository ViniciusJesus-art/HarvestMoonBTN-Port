/* Adult coop cycle from animals/chicken.c. Names retain original glyphs. */
#include "poultry.h"
#include <stdlib.h>
#include <string.h>
static unsigned random_range(HmbtnPoultry *p,unsigned n)
{
    /* Host PRNG; distribution follows rand_range, not the PS1 global stream. */
    if(!p->random) p->random=0x42544E;
    p->random=p->random*1664525u+1013904223u;return (p->random>>8)%n;
}
unsigned hmbtn_poultry_count(const HmbtnPoultry *p)
{
    unsigned count=0;for(unsigned i=0;i<HMBTN_HENS;++i) if(p->hens[i].occupied) ++count;return count;
}
bool hmbtn_poultry_room(const HmbtnPoultry *p) { return hmbtn_poultry_count(p)<(p->upgraded ? 10u : 5u); }
bool hmbtn_poultry_buy(HmbtnPoultry *p,const uint16_t name[HMBTN_NAME_CHARS+1],const HmbtnFarmClock *c)
{
    if(!hmbtn_poultry_room(p)) return false;
    for(unsigned i=0;i<HMBTN_HENS;++i) if(!p->hens[i].occupied) {
        HmbtnHen *h=&p->hens[i];memset(h,0,sizeof(*h));h->occupied=true;h->affection=10;
        h->season=c->season;h->day=c->day;memcpy(h->name,name,sizeof(h->name));
        h->actor.position[0]=(i<5 ? -760 : -750)+(int)(i%5)*400;
        h->actor.position[2]=i<5 ? -600 : 1600;h->actor.visible=true;
        h->actor.facing=random_range(p,4);h->actor.sprite.frame=(uint16_t)h->actor.facing;
        return true;
    }
    return false;
}
void hmbtn_poultry_new_day(HmbtnPoultry *p,HmbtnFarmFeed *feed,const HmbtnFarmClock *c)
{
    memset(p->eggs,0,sizeof(p->eggs)); /* estate run 16 is replaced daily. */
    for(unsigned i=0;i<HMBTN_HENS;++i) {
        HmbtnHen *h=&p->hens[i];if(!h->occupied || h->dead) continue;
        bool fed=false;
        for(unsigned j=0;j<10;++j) if(feed->coop[j]) { feed->coop[j]=false;fed=true;break; }
        if(h->fed_yesterday) {
            HmbtnCoopEgg *e=&p->eggs[i];e->item=h->affection<100 ? 0x122 : h->affection<200 ? 0x123 : h->prize ? 0xE8 : 0x124;
            memcpy(e->position,h->actor.position,sizeof(e->position));
            /* farm_position_normalize keeps eggs clear of troughs/walls. */
            if(e->position[0]<-1000 || e->position[0]>1200) e->position[0]=(int)random_range(p,2200)-1000;
            if(e->position[2]>=0) {
                if(e->position[2]<400 || e->position[2]>1700) e->position[2]=400+(int)random_range(p,1300);
            } else if(e->position[2]<-2000 || e->position[2]>-500) e->position[2]=(int)random_range(p,1500)-2000;
        }
        h->fed_yesterday=fed;
        h->affection=fed ? (h->affection>245 ? 255 : h->affection+10) : h->affection>20 ? h->affection-20 : 0;
        if(h->days_sick) {
            if(h->days_sick<31) ++h->days_sick;
            if(h->days_sick>=3 && random_range(p,100)<=h->days_sick*10) h->dead=true;
        } else if(!h->affection && random_range(p,100)<11) h->days_sick=1;
        if(!h->dead && h->age>=3 && random_range(p,100)<=(h->age==3 ? 25u : h->age==4 ? 75u : 100u)) h->dead=true;
        if(c->season==h->season && c->day==h->day) ++h->age;
        h->actor.visible=!h->dead;h->timer=0;
    }
}
void hmbtn_poultry_tick(HmbtnPoultry *p,const HmbtnModel *ground,const HmbtnIntroActor *player,unsigned hour)
{
    static const int delta[4][2]={{0,-6},{-6,0},{0,6},{6,0}};
    for(unsigned i=0;i<HMBTN_HENS;++i) {
        HmbtnHen *h=&p->hens[i];if(!h->occupied || h->dead) continue;
        HmbtnIntroActor *a=&h->actor;
        if(hour<6 || hour>=21 || h->days_sick) {
            if(a->sprite.animation!=18) a->sprite=(HmbtnAnimationPlayer){18,(uint16_t)a->facing,0};
            h->timer=0;continue;
        }
        if(!h->timer) {
            h->timer=120+random_range(p,61);a->facing=random_range(p,4);
            a->sprite=random_range(p,4)<2 ? (HmbtnAnimationPlayer){(uint16_t)(1+a->facing),0,0}
                : (HmbtnAnimationPlayer){0,(uint16_t)a->facing,0};
        }
        --h->timer;
        if(a->sprite.animation<1 || a->sprite.animation>4) continue;
        int32_t target[3]={a->position[0]+delta[a->facing][0],a->position[1],a->position[2]+delta[a->facing][1]};
        int32_t ahead[3]={target[0]+delta[a->facing][0]*20,target[1],target[2]+delta[a->facing][1]*20};
        HmbtnGroundHit hit;bool clear=ground && hmbtn_ground_find(ground,ahead,0xE080,64,&hit);
        if(abs(target[0]-player->position[0])<150 && abs(target[2]-player->position[2])<150) clear=false;
        for(unsigned j=0;j<HMBTN_HENS;++j) if(j!=i && p->hens[j].occupied && !p->hens[j].dead
                && abs(target[0]-p->hens[j].actor.position[0])<120 && abs(target[2]-p->hens[j].actor.position[2])<120) clear=false;
        if(clear && hmbtn_ground_find(ground,target,0xE080,64,&hit)) {
            target[1]=hit.height;memcpy(a->position,target,sizeof(target));
        } else { h->timer=0;a->sprite=(HmbtnAnimationPlayer){0,(uint16_t)a->facing,0}; }
    }
}
bool hmbtn_poultry_take_egg(HmbtnPoultry *p,HmbtnHouseInteraction *h,HmbtnFarmCarry *carry,HmbtnIntroActor *a)
{
    if(h->held_item.id || carry->phase || a->facing>3) return false;
    static const int delta[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
    for(unsigned i=0;i<HMBTN_HENS;++i) {
        HmbtnCoopEgg *e=&p->eggs[i];if(!e->item) continue;
        if(abs(e->position[0]-a->position[0]-delta[a->facing][0])>=200
                || abs(e->position[2]-a->position[2]-delta[a->facing][1])>=200
                || abs(e->position[1]-a->position[1])>=200) continue;
        h->held_item=(HmbtnStoredTool){(uint16_t)e->item,0,1};e->item=0;
        carry->phase=1;carry->sound=7;a->animation_done=false;
        a->sprite=(HmbtnAnimationPlayer){(uint16_t)(11+a->facing),0,0};return true;
    }
    return false;
}
