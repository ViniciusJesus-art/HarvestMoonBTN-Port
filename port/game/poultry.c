/* Adult coop cycle from animals/chicken.c. Names retain original glyphs. */
#include "poultry.h"
#include <stdlib.h>
#include <string.h>
#include "psx_data.h"
static unsigned random_range(HmbtnPoultry *p,unsigned n)
{
    /* Host PRNG; distribution follows rand_range, not the PS1 global stream. */
    if(!p->random) p->random=0x42544E;
    p->random=p->random*1664525u+1013904223u;return (p->random>>8)%n;
}
unsigned hmbtn_poultry_count(const HmbtnPoultry *p)
{
    unsigned count=0;for(unsigned i=0;i<HMBTN_HENS;++i)
        if(p->hens[i].occupied && p->hens[i].stage!=HMBTN_HEN_EGG && !p->hens[i].naming_pending) ++count;
    return count;
}
unsigned hmbtn_poultry_status_slot(const HmbtnPoultry *p,unsigned row)
{
    if(p) for(unsigned i=0;i<HMBTN_HENS;++i) {
        const HmbtnHen *h=&p->hens[i];
        if(!h->occupied || h->stage==HMBTN_HEN_EGG || h->naming_pending) continue;
        if(!row) return i;
        --row;
    }
    return HMBTN_HENS;
}
bool hmbtn_poultry_room(const HmbtnPoultry *p)
{
    for(unsigned i=0;i<(p->upgraded ? 10u : 5u);++i) if(!p->hens[i].occupied) return true;
    return false;
}
static bool saleable(const HmbtnHen *h)
{
    return h->occupied && !h->dead && !h->held && !h->naming_pending && h->stage==HMBTN_HEN_ADULT;
}
unsigned hmbtn_poultry_sale_slot(const HmbtnPoultry *p,unsigned row)
{
    if(p) for(unsigned i=0;i<HMBTN_HENS;++i) if(saleable(&p->hens[i])) {
        if(!row) return i;
        --row;
    }
    return HMBTN_HENS;
}
unsigned hmbtn_poultry_sell(HmbtnPoultry *p,unsigned slot)
{
    if(!p || slot>=HMBTN_HENS || !saleable(&p->hens[slot])) return 0;
    const unsigned price=p->hens[slot].prize ? 2000 : 500;
    memset(&p->hens[slot],0,sizeof(p->hens[slot]));return price;
}
bool hmbtn_poultry_medicine_tick(HmbtnPoultry *p,unsigned area,HmbtnFarmAction *a,HmbtnIntroActor *player,
    HmbtnHouseInteraction *h,uint32_t pressed)
{
    a->sound=-1;a->changed=a->finished_bank=a->release_sound=false;a->broken_item=0;
    if(a->phase==HMBTN_FARM_MEDICINE) {
        if(!player->animation_done) return true;
        if(p && a->target<HMBTN_HENS) {
            HmbtnHen *hen=&p->hens[a->target];
            if(hen->occupied && !hen->dead && hen->stage==HMBTN_HEN_ADULT && hen->days_sick) {
                hen->days_sick=0;hen->timer=0;hen->praise=hen->affection>=10 ? 45 : 0;
                hen->actor.sprite=(HmbtnAnimationPlayer){0,(uint16_t)hen->actor.facing,0};a->changed=true;
            }
        }
        /* stored_item_release runs even when the target is absent or healthy. */
        if(h->equipped.id==0x1E && h->equipped.count && !--h->equipped.count) h->equipped=(HmbtnStoredTool){0};
        const unsigned fatigue=(h->weather!=0)+(h->hour>=22 ? 1u : h->hour<6 ? 2u : 0u)+(a->stamina==0 ? 2u : 0u);
        a->fatigue=a->fatigue+fatigue>100 ? 100 : a->fatigue+fatigue;
        a->sound=17;a->finished_bank=true;a->bank=-1;a->phase=HMBTN_FARM_ACTION_FREE;
        player->sprite=(HmbtnAnimationPlayer){0,(uint16_t)player->facing,0};player->animation_done=false;
        return true;
    }
    if(a->phase!=HMBTN_FARM_ACTION_FREE || !(pressed&HMBTN_PAD_SQUARE) || player->facing>3
            || h->equipped.id!=0x1E || !h->equipped.count || h->held_item.id || a->fatigue>=100) return false;
    /* player_probe_target(kind 0x18) probes 128 forward, against the original
     * chicken halfWidth/halfDepth (strict 200). It latches before the animation. */
    static const int step[4][2]={{0,-128},{-128,0},{0,128},{128,0}};
    a->target=HMBTN_HENS;
    if(p) for(unsigned i=0;i<HMBTN_HENS;++i) {
        const HmbtnHen *hen=&p->hens[i];
        if(!hen->occupied || hen->dead || hen->held || hen->area!=area || hen->stage==HMBTN_HEN_EGG || hen->naming_pending) continue;
        if(abs(hen->actor.position[0]-player->position[0]-step[player->facing][0])<200
                && abs(hen->actor.position[2]-player->position[2]-step[player->facing][1])<200) { a->target=i;break; }
    }
    a->phase=HMBTN_FARM_MEDICINE;a->kind=5;a->bank=22;a->bank_pending=true;
    player->sprite=(HmbtnAnimationPlayer){(uint16_t)player->facing,0,0};player->animation_done=false;return true;
}
static void place(HmbtnHen *h,unsigned i)
{
    h->area=112;
    h->actor.position[0]=(i<5 ? -760 : -750)+(int)(i%5)*400;
    h->actor.position[1]=0;h->actor.position[2]=i<5 ? -600 : 1600;
}
bool hmbtn_poultry_buy(HmbtnPoultry *p,const uint16_t name[HMBTN_NAME_CHARS+1],const HmbtnFarmClock *c)
{
    if(!hmbtn_poultry_room(p)) return false;
    for(unsigned i=0;i<(p->upgraded ? 10u : 5u);++i) if(!p->hens[i].occupied) {
        HmbtnHen *h=&p->hens[i];memset(h,0,sizeof(*h));h->occupied=true;h->affection=10;
        h->season=c->season;h->day=c->day;memcpy(h->name,name,sizeof(h->name));
        h->growth=10;place(h,i);h->actor.visible=true;
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
        h->praise=0; /* chicken_daily_tick resets transient cues. */
        if(h->stage!=HMBTN_HEN_ADULT) {
            if(h->naming_pending) continue;
            if(++h->growth==3) {
                h->stage=HMBTN_HEN_CHICK;h->naming_pending=true;h->affection=10;
                h->season=c->season;h->day=c->day;h->actor.visible=true;
                hmbtn_poultry_incubator_position(h->incubator-1,h->actor.position);
                h->actor.position[1]=-200;h->actor.facing=3;h->actor.sprite=(HmbtnAnimationPlayer){0,3,0};
            } else if(h->growth>=10) {
                h->stage=HMBTN_HEN_ADULT;h->timer=0;h->actor.sprite=(HmbtnAnimationPlayer){0,(uint16_t)h->actor.facing,0};
            }
            if(!h->naming_pending && h->area!=0 && h->area!=112) place(h,i);
            continue;
        }
        bool fed=h->fed_today;
        if(h->area==112 && !fed)
            for(unsigned j=0;j<10;++j) if(feed->coop[j]) { feed->coop[j]=false;fed=true;break; }
        if(h->area!=0 && h->area!=112) place(h,i); /* chicken_place after the feed check. */
        if(h->fed_yesterday) {
            HmbtnCoopEgg *e=&p->eggs[i];e->item=h->affection<100 ? 0x122 : h->affection<200 ? 0x123 : h->prize ? 0xE8 : 0x124;
            memcpy(e->position,h->actor.position,sizeof(e->position));
            e->area=h->area;
            /* farm_position_normalize keeps eggs clear of troughs/walls. */
            if(h->area==112) {
                if(e->position[0]<-1000 || e->position[0]>1200) e->position[0]=(int)random_range(p,2200)-1000;
                if(e->position[2]>=0) {
                    if(e->position[2]<400 || e->position[2]>1700) e->position[2]=400+(int)random_range(p,1300);
                } else if(e->position[2]<-2000 || e->position[2]>-500) e->position[2]=(int)random_range(p,1500)-2000;
            }
        }
        h->fed_today=false;h->grazing=0;
        h->fed_yesterday=fed;
        h->affection=fed ? (h->affection>245 ? 255 : h->affection+10) : h->affection>20 ? h->affection-20 : 0;
        if(h->days_sick) {
            if(h->days_sick<31) ++h->days_sick;
            if(h->days_sick>=3 && random_range(p,100)<=h->days_sick*10) h->dead=true;
        } else if(!h->affection && random_range(p,100)<11) h->days_sick=1;
        if(!h->dead && h->age>=3 && random_range(p,100)<=(h->age==3 ? 25u : h->age==4 ? 75u : 100u)) h->dead=h->died_of_age=true;
        if(h->growth<31) ++h->growth;
        if(c->season==h->season && c->day==h->day) ++h->age;
        h->actor.visible=!h->dead && !h->held;h->timer=0;
    }
}
void hmbtn_poultry_tick(HmbtnPoultry *p,unsigned area,const HmbtnModel *ground,
    const HmbtnFarmField *field,const HmbtnIntroActor *player,unsigned hour)
{
    static const int delta[4][2]={{0,-6},{-6,0},{0,6},{6,0}};
    for(unsigned i=0;i<HMBTN_HENS;++i) {
        HmbtnHen *h=&p->hens[i];if(!h->occupied || h->dead || h->held || h->area!=area
            || h->stage==HMBTN_HEN_EGG || h->naming_pending) continue;
        if(h->praise) --h->praise;
        HmbtnIntroActor *a=&h->actor;
        if(hour<6 || hour>=21 || h->days_sick) {
            const unsigned animation=h->days_sick ? 18 : 5+a->facing;
            if(a->sprite.animation!=animation) a->sprite=(HmbtnAnimationPlayer){(uint16_t)animation,(uint16_t)(h->days_sick ? a->facing : 0),0};
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
        if(field && hmbtn_farm_field_probe(field,ahead)) clear=false;
        if(abs(target[0]-player->position[0])<150 && abs(target[2]-player->position[2])<150) clear=false;
        for(unsigned j=0;j<HMBTN_HENS;++j) if(j!=i && p->hens[j].occupied && !p->hens[j].dead
                && !p->hens[j].held && p->hens[j].area==area && p->hens[j].stage!=HMBTN_HEN_EGG
                && abs(target[0]-p->hens[j].actor.position[0])<120 && abs(target[2]-p->hens[j].actor.position[2])<120) clear=false;
        if(clear && hmbtn_ground_find(ground,target,0xE080,64,&hit)) {
            target[1]=hit.height;memcpy(a->position,target,sizeof(target));
        } else { h->timer=0;a->sprite=(HmbtnAnimationPlayer){0,(uint16_t)a->facing,0}; }
    }
}
bool hmbtn_poultry_take_egg(HmbtnPoultry *p,unsigned area,HmbtnHouseInteraction *h,HmbtnFarmCarry *carry,HmbtnIntroActor *a)
{
    if(h->held_item.id || carry->phase || a->facing>3) return false;
    static const int delta[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
    for(unsigned i=0;i<HMBTN_HENS;++i) {
        HmbtnCoopEgg *e=&p->eggs[i];if(!e->item || e->area!=area) continue;
        if(abs(e->position[0]-a->position[0]-delta[a->facing][0])>=200
                || abs(e->position[2]-a->position[2]-delta[a->facing][1])>=200
                || abs(e->position[1]-a->position[1])>=200) continue;
        h->held_item=(HmbtnStoredTool){(uint16_t)e->item,0,1};e->item=0;
        carry->phase=1;carry->sound=7;a->animation_done=false;
        a->sprite=(HmbtnAnimationPlayer){(uint16_t)(11+a->facing),0,0};return true;
    }
    return false;
}
bool hmbtn_poultry_incubator_position(unsigned bed,int32_t position[3])
{
    if(bed>1 || !position) return false;
    position[0]=-1600;position[1]=-200;position[2]=bed ? 1300 : -1100;return true;
}
unsigned hmbtn_poultry_incubate(HmbtnPoultry *p,unsigned bed,unsigned item,const HmbtnFarmClock *clock)
{
    if(bed>1 || (bed==1 && !p->upgraded)) return 0x1E6;
    for(unsigned i=0;i<HMBTN_HENS;++i) if(p->hens[i].occupied && p->hens[i].incubator==bed+1) return 0x1E6;
    if(!hmbtn_poultry_room(p)) return 0x1A1;
    if(item!=0x122 && item!=0x123 && item!=0x124 && item!=0xE8) return 0x1E7;
    for(unsigned i=0;i<(p->upgraded ? 10u : 5u);++i) if(!p->hens[i].occupied) {
        HmbtnHen *h=&p->hens[i];memset(h,0,sizeof(*h));h->occupied=true;h->stage=HMBTN_HEN_EGG;
        h->area=112;h->incubator=bed+1;h->golden_egg=item==0xE8; /* A golden egg does not inherit contest wins. */
        h->season=clock->season;h->day=clock->day;
        for(unsigned j=0;j<=HMBTN_NAME_CHARS;++j) h->name[j]=0xFFFF;
        hmbtn_poultry_incubator_position(bed,h->actor.position);return 0;
    }
    return 0x1A1;
}
HmbtnHen *hmbtn_poultry_hatch_ready(HmbtnPoultry *p,unsigned bed)
{
    for(unsigned i=0;i<HMBTN_HENS;++i) if(p->hens[i].occupied && p->hens[i].incubator==bed+1 && p->hens[i].naming_pending) return &p->hens[i];
    return NULL;
}
void hmbtn_poultry_name_end(HmbtnPoultry *p)
{
    for(unsigned i=0;i<HMBTN_HENS;++i) if(p->hens[i].occupied && p->hens[i].naming_pending && p->hens[i].name[0]!=0xFFFF) {
        HmbtnHen *h=&p->hens[i];h->incubator=0;h->naming_pending=false;place(h,i);h->timer=0;
    }
}
bool hmbtn_poultry_dead(const HmbtnPoultry *p,bool old_age)
{
    for(unsigned i=0;i<HMBTN_HENS;++i) if(p->hens[i].occupied && p->hens[i].dead && p->hens[i].died_of_age==old_age) return true;
    return false;
}
unsigned hmbtn_poultry_clear_dead(HmbtnPoultry *p)
{
    unsigned count=0;
    for(unsigned i=0;i<HMBTN_HENS;++i) if(p->hens[i].occupied && p->hens[i].dead) {
        memset(&p->hens[i],0,sizeof(p->hens[i]));++count;
    }
    return count;
}
static int half(const uint8_t *p) { unsigned x=hmbtn_read_le16(p);return x<32768 ? (int)x : (int)x-65536; }
bool hmbtn_poultry_incubator_tick(HmbtnPoultry *p,HmbtnFarmFeed *action,HmbtnHouseInteraction *h,
    HmbtnIntroActor *a,const HmbtnFarmClock *clock,const uint8_t *section,size_t size,uint32_t pressed)
{
    if(!(pressed&HMBTN_PAD_CROSS) || action->phase || a->facing>3 || !section || size<16+50*44) return false;
    static const int delta[4][2]={{0,-224},{-224,0},{0,224},{224,0}};
    const int32_t x=a->position[0]+delta[a->facing][0],z=a->position[2]+delta[a->facing][1];
    for(unsigned i=0;i<50;++i) {
        const uint8_t *zone=section+16+i*44;
        if(hmbtn_read_le16(zone)!=0x4000 || hmbtn_read_le16(zone+2)!=34 || hmbtn_read_le16(zone+8)!=a->facing
                || x<=half(zone+14) || x>=half(zone+20) || z<=half(zone+18) || z>=half(zone+24)
                || abs(a->position[1]-half(zone+16))>=128) continue;
        action->sound=-1;action->request=0;
        if(h->held_item.id && h->held_item.id!=0x122 && h->held_item.id!=0x123 && h->held_item.id!=0x124 && h->held_item.id!=0xE8) return true;
        if(!h->held_item.id) { action->request=0x1E7;return true; }
        action->request=hmbtn_poultry_incubate(p,hmbtn_read_le16(zone+12),h->held_item.id,clock);
        if(action->request) return true;
        action->phase=2;action->sound=7;a->animation_done=false;
        a->sprite=(HmbtnAnimationPlayer){(uint16_t)(23+a->facing),0,0};return true;
    }
    return false;
}
bool hmbtn_poultry_expand_dead(const HmbtnPoultry *p,uint8_t *record,size_t size)
{
    if(!record || size!=8192) return false;
    uint16_t words[HMBTN_MESSAGE_WORDS];unsigned count=0;
    for(unsigned i=0;i<HMBTN_MESSAGE_WORDS;++i) {
        uint16_t c=hmbtn_read_le16(record+0x1800+i*2);
        if(c==0x401D || c==0x4020) {
            for(unsigned j=0;j<HMBTN_HENS;++j) {
                const HmbtnHen *h=&p->hens[j];
                if(!h->occupied || !h->dead || h->died_of_age!=(c==0x401D)) continue;
                for(unsigned k=0;k<8 && h->name[k]!=0xFFFF;++k) {
                    if(count>=HMBTN_MESSAGE_WORDS) return false;
                    words[count++]=h->name[k];
                }
                if(count+2>=HMBTN_MESSAGE_WORDS) return false;
                words[count++]=0x51;words[count++]=0xFFFE; /* text_died_suffix */
            }
        } else { if(count>=HMBTN_MESSAGE_WORDS) return false;words[count++]=c; }
        if(c==0xFFFF) break;
    }
    if(!count || words[count-1]!=0xFFFF) return false;
    /* Lists can span ten names. Page complete lines through the existing native
     * message renderer, retaining the source's explicit page boundaries. */
    unsigned out=0,lines=0;bool leading=false;
    for(unsigned i=0;i<count;++i) {
        uint16_t c=words[i];
        if(c==0xFFFE) {
            if(leading) leading=false;
            else if(++lines>=4) {
                if(out+3>=HMBTN_MESSAGE_WORDS) return false;
                hmbtn_write_le16(record+0x1800+out++*2,0xFFC);
                hmbtn_write_le16(record+0x1800+out++*2,0xFFB);lines=0;
            }
        } else if(c==0xFFC || c==0xFFB) { lines=0;leading=true; }
        else if(c<256 || c==0xFFD) leading=false;
        if(out>=HMBTN_MESSAGE_WORDS) return false;
        hmbtn_write_le16(record+0x1800+out++*2,c);
    }
    return true;
}
