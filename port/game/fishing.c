/* fishing.c and the fishing/received-item branches of people/player.c. */
#include "fishing.h"
#include "psx_random.h"
#include "fishing_tables.inc"
static void pose(HmbtnIntroActor *p,unsigned animation,unsigned frame)
{
    p->sprite=(HmbtnAnimationPlayer){(uint16_t)animation,(uint16_t)frame,0};p->animation_done=false;
}
void hmbtn_fishing_cancel(HmbtnFishing *f)
{
    f->phase=HMBTN_FISHING_FREE;f->shown=0;f->marker_visible=false;f->using_action=false;f->in_flight=false;
    f->bank_pending=false;f->finished_bank=true;
}
unsigned hmbtn_fishing_wait(HmbtnFishing *f,unsigned minute)
{
    if(f->kind>=5) return 0;
    for(unsigned i=0;i<4;++i) if(minute>=bite_ranges[i][0] && minute<=bite_ranges[i][1]) {
        f->pull=hmbtn_psx_random_range(&f->random,fishing_bite_amounts[f->kind])+5;
        return bite_ranges[i][2+f->kind];
    }
    return 0;
}
unsigned hmbtn_fishing_roll(HmbtnFishing *f,const HmbtnFarmClock *c,bool flags[512])
{
    if(f->kind>=5 || c->season>=4 || !flags) return 0;
    const uint16_t *table=fishing_bite_amounts+season_catches[c->season];
    switch(f->kind) {
    case 1:table=fishing_spot_catches_1;break;case 2:table=fishing_spot_catches_2;break;
    case 3:table=fishing_spot_catches_3;break;case 4:table=fishing_spot_catches_4;break;
    }
    const unsigned count=*table++,roll=hmbtn_psx_random_range(&f->random,100);unsigned sum=0;
    for(unsigned i=0;i<count;++i,table+=2) {
        sum+=table[0];if(roll>=sum) continue;
        const unsigned item=table[1];unsigned flag=0;
        switch(item) {
        case ITEM_FRIES_RECIPE:flag=EVENT_FLAG_FISHED_0EA;break;
        case ITEM_POWER_BERRY:flag=EVENT_FLAG_FISHED_POWER_BERRY;break;
        case ITEM_CARP:
            if(!flags[EVENT_FLAG_FISHED_0EE] || !flags[EVENT_FLAG_FISHED_CATFISH]
                    || !flags[EVENT_FLAG_FISHED_0F0] || !flags[EVENT_FLAG_FISHED_0F1] || !flags[EVENT_FLAG_FISHED_0F2]) continue;
            flag=EVENT_FLAG_FISHED_0ED;break;
        case ITEM_SQUID:if(!flags[EVENT_FLAG_SMALL_FISH_PLACED]) continue;flag=EVENT_FLAG_FISHED_0F0;break;
        case ITEM_ANGLER: {
            const unsigned minute=c->hour*60+c->minute;
            if(minute>=480 && minute<1321) continue;
            flag=EVENT_FLAG_FISHED_0F1;break;
        }
        case ITEM_SEA_BREAM:if(!flags[EVENT_FLAG_FISH_SHIPPED_200]) continue;flag=EVENT_FLAG_FISHED_0F2;break;
        case ITEM_CATFISH:flag=EVENT_FLAG_FISHED_CATFISH;break;
        case ITEM_CHAR:
            if(!flags[EVENT_FLAG_COOKED_SASHIMI] || !flags[EVENT_FLAG_COOKED_GRILLED_FISH] || !flags[EVENT_FLAG_COOKED_SUSHI]) continue;
            flag=EVENT_FLAG_FISHED_0EE;break;
        default:
            if(item>=ITEM_LARGE_FISH && item<=ITEM_SMALL_FISH) f->streak=0;
            else if(f->streak<11) ++f->streak;
            else { f->streak=0;return fishing_fallback_catch[f->kind]; }
        }
        if(flag) {
            if(flags[flag]) continue;
            flags[flag]=true;
            if(item>=ITEM_CARP && item<=ITEM_SEA_BREAM) f->catch_flags |= 1u<<(item-ITEM_CARP+2);
        }
        return item;
    }
    return 0;
}
static bool surface(const HmbtnModel *ground,const int32_t position[3],HmbtnGroundHit *hit)
{
    static const unsigned tolerance[]={4,64,320,960,1720};
    for(unsigned i=0;i<5;++i) if(hmbtn_ground_find(ground,position,0,tolerance[i],hit)) return true;
    return false;
}
static void launch(HmbtnFishing *f,const HmbtnIntroActor *p,bool marker)
{
    f->marker[0]=p->position[0];f->marker[1]=p->position[1]-300;f->marker[2]=p->position[2];
    f->ground=p->position[1];f->velocity=marker ? -78 : 0;
    f->direction=p->facing;
    const int speed=marker ? 29 : 44;
    f->dx=p->facing&1u ? (p->facing==1 ? -speed : speed) : 0;
    f->dz=p->facing&1u ? 0 : (p->facing==0 ? -speed : speed);
    f->marker_visible=true;f->water=false;f->in_flight=true;
}
static bool flight(HmbtnFishing *f,const HmbtnModel *ground)
{
    f->marker[0]+=f->dx;f->marker[2]+=f->dz;f->marker[1]+=f->velocity;
    f->velocity+=9;if(f->velocity>45) f->velocity=45;
    const int32_t probe[3]={f->marker[0]+f->dx,f->marker[1],f->marker[2]+f->dz};
    HmbtnGroundHit hit;const bool found=surface(ground,probe,&hit);
    if(f->marker[1]<=5000 && (!found || hit.height<f->marker[1]-100)) {
        f->direction=(f->direction+2)&3u;
        f->marker[f->direction&1u ? 0 : 2]+=f->direction<2 ? -88 : 88;
        f->dx=f->dz=0;return false;
    }
    if(f->marker[1]<=f->ground && f->marker[1]<=5000) {
        const int32_t below[3]={f->marker[0],f->ground,f->marker[2]};
        if(surface(ground,below,&hit)) f->ground=hit.height;
        return false;
    }
    f->water=found && (hit.flags&2u) && !(hit.flags&0x44u);
    f->marker[1]-=10;return true;
}
static void finish(HmbtnFishing *f,HmbtnIntroActor *p)
{
    hmbtn_fishing_cancel(f);pose(p,0,p->facing);
}
bool hmbtn_fishing_tick(HmbtnFishing *f,HmbtnIntroActor *p,HmbtnHouseInteraction *h,
    HmbtnFarmAction *status,const HmbtnModel *ground,const HmbtnFarmClock *c,
    unsigned location,bool flags[512],uint32_t pressed,uint32_t held)
{
    f->sound=-1;f->finished_bank=f->berry_eaten=false;
    switch(f->phase) {
    case HMBTN_FISHING_FREE:
        if(!(pressed&HMBTN_PAD_SQUARE) || (h->inventory->equipped.id!=ITEM_FISHING_ROD && h->inventory->equipped.id!=ITEM_FISHING_POLE)
                || h->inventory->held_item.id || p->facing>3 || status->fatigue>=100) return false;
        f->phase=HMBTN_FISHING_CAST;f->using_action=true;f->bank_pending=true;f->caught=f->shown=0;
        f->bob_frame=0;pose(p,p->facing,0);launch(f,p,true);f->sound=15;return true;
    case HMBTN_FISHING_CAST:
        if(f->in_flight && flight(f,ground)) { f->in_flight=false;if(f->water) f->sound=16; }
        if(f->in_flight || !p->animation_done) return true;
        f->kind=location==44 ? 0 : location==13 ? 1 : location==12 ? 2 : location==66 ? 3 : 4;
        status->fatigue+=(h->weather->today!=0)+(h->clock->hour>=22 ? 1u : h->clock->hour<6 ? 2u : 0u)+(status->stamina==0 ? 2u : 0u);
        if(status->fatigue>100) status->fatigue=100;
        if(status->stamina) --status->stamina;
        f->wait=hmbtn_fishing_wait(f,c->hour*60+c->minute);
        f->phase=f->water ? HMBTN_FISHING_WAIT : HMBTN_FISHING_REEL;
        if(f->water) pose(p,8,p->facing);else { pose(p,p->facing+4,0);f->marker_visible=false;f->sound=15; }
        return true;
    case HMBTN_FISHING_WAIT:
        if(!(held&HMBTN_PAD_SQUARE)) { f->phase=HMBTN_FISHING_REEL;pose(p,p->facing+4,0);f->marker_visible=false;f->sound=15;return true; }
        if(!f->wait) {
            if(hmbtn_psx_random_range(&f->random,100)<(h->inventory->equipped.id==ITEM_FISHING_POLE ? 40u : 20u)) {
                f->phase=HMBTN_FISHING_HOOKED;pose(p,p->facing+9,0);return true;
            }
            f->wait=hmbtn_fishing_wait(f,c->hour*60+c->minute);
        } else --f->wait;
        pose(p,8,p->facing);f->bob_phase=(f->bob_phase+22)%4096;return true;
    case HMBTN_FISHING_HOOKED:
        if(!(held&HMBTN_PAD_SQUARE)) { f->phase=HMBTN_FISHING_CATCH;pose(p,p->facing+4,0);f->marker_visible=false;f->sound=15; }
        else if(!f->pull) { f->phase=HMBTN_FISHING_WAIT;f->wait=hmbtn_fishing_wait(f,c->hour*60+c->minute); }
        else { --f->pull;++f->bob_frame; }
        return true;
    case HMBTN_FISHING_REEL:if(p->animation_done) finish(f,p);return true;
    case HMBTN_FISHING_CATCH:
        if(!p->animation_done) return true;
        f->caught=hmbtn_fishing_roll(f,c,flags);f->using_action=false;f->finished_bank=true;
        if(!f->caught) { finish(f,p);return true; }
        f->shown=f->caught;f->phase=HMBTN_FISHING_RECEIVE;pose(p,11+p->facing,0);return true;
    case HMBTN_FISHING_RECEIVE:
        if(!p->animation_done) return true;
        h->inventory->held_item=(HmbtnStoredTool){(uint16_t)f->caught,0,1};f->shown=0;
        if(f->caught==ITEM_POWER_BERRY) { f->phase=HMBTN_FISHING_EAT;f->sound=74;pose(p,5,0); }
        else if(f->caught>=ITEM_CARP && f->caught<=ITEM_SEA_BREAM) { f->phase=HMBTN_FISHING_RELEASE;pose(p,23+p->facing,0); }
        else finish(f,p);
        return true;
    case HMBTN_FISHING_EAT:
        if(p->animation_done) {
            status->max_stamina=status->max_stamina+10>255 ? 255 : status->max_stamina+10;
            status->stamina=status->max_stamina;if(status->fatigue) --status->fatigue;
            f->berry_eaten=true;
            h->inventory->held_item=(HmbtnStoredTool){0};finish(f,p);
        }
        return true;
    case HMBTN_FISHING_RELEASE:
        if(p->animation_done) {
            launch(f,p,false);f->phase=HMBTN_FISHING_RELEASE_FLIGHT;h->inventory->held_item=(HmbtnStoredTool){0};f->sound=9;pose(p,0,p->facing);
        }
        return true;
    case HMBTN_FISHING_RELEASE_FLIGHT:
        if(flight(f,ground)) {
            f->marker_visible=false;f->shown=f->caught-ITEM_CARP+ITEM_CARP_PRINT;
            f->phase=HMBTN_FISHING_PRINT;pose(p,11+p->facing,0);
        }
        return true;
    case HMBTN_FISHING_PRINT:
        if(p->animation_done) { f->phase=HMBTN_FISHING_PRINT_HOLD;f->hold=31;pose(p,27,0); }
        return true;
    case HMBTN_FISHING_PRINT_HOLD:
        if(!--f->hold) { f->phase=HMBTN_FISHING_PRINT_STORE;f->shown=0;pose(p,29+p->facing,0); }
        return true;
    case HMBTN_FISHING_PRINT_STORE:if(p->animation_done) finish(f,p);return true;
    }
    return false;
}
