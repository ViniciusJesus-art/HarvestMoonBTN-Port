/* Adapted from animals.c:livestock_find_free/place_row_spot/farm_message_request
 * and livestock.c:record_init/_b, growth_kind/_b, overnight_update/_alt,
 * pick_state/_b, walk cue, frame lookup and talk/cure. Host ground checks replace
 * PS1 actor callbacks. Detailed cue sequencing, pasture/breeding/production and
 * funeral events are outside this acquisition/indoor-care slice. */
#include "livestock.h"
#include "psx_random.h"
#include "psx_data.h"
#include <stdlib.h>
#include <string.h>
typedef struct NativeFrame { uint16_t animation,frame; } NativeFrame;
typedef struct NativeFrameRow { NativeFrame entry[4]; } NativeFrameRow;
#include "livestock_tables.inc"
unsigned hmbtn_livestock_kind(const HmbtnFarmAnimal *a)
{ return !a || !a->occupied ? 0 : a->sheep ? (a->growth<14 ? 0x164u : 0x165u) : a->growth<14 ? 0x160u : a->growth<28 ? 0x161u : 0x162u; }
unsigned hmbtn_livestock_pack(const HmbtnFarmAnimal *a)
{ return a->sheep ? (a->growth<14 ? 4u : a->days_since_shearing<7 ? 8u : 5u) : a->growth<14 ? 0u : 1u; }
unsigned hmbtn_livestock_scale(const HmbtnFarmAnimal *a)
{ return hmbtn_livestock_kind(a)==0x161 ? LIVESTOCK_YOUNG_SCALE : 4096; }
unsigned hmbtn_livestock_capacity(const HmbtnLivestock *p)
{ return p && p->construction && p->construction->barn_level ? HMBTN_LIVESTOCK : 10u; }
unsigned hmbtn_livestock_free(const HmbtnLivestock *p)
{
    if(p) for(unsigned i=0;i<hmbtn_livestock_capacity(p);++i) if(!p->animals[i].occupied) return i;
    return HMBTN_LIVESTOCK;
}
unsigned hmbtn_livestock_count(const HmbtnLivestock *p,bool sheep)
{
    unsigned n=0;if(p) for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) n+=p->animals[i].occupied && p->animals[i].sheep==sheep;
    return n;
}
unsigned hmbtn_livestock_slot(const HmbtnLivestock *p,bool sheep,unsigned row)
{
    if(p) for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) if(p->animals[i].occupied && p->animals[i].sheep==sheep) { if(!row) return i;--row; }
    return HMBTN_LIVESTOCK;
}
static void pose(HmbtnFarmAnimal *a,unsigned action)
{
    const NativeFrame frame=a->sheep ? sheep_frames[action][a->actor.facing] : cow_frames[action].entry[a->actor.facing];
    if(a->actor.sprite.animation!=frame.animation) a->actor.sprite=(HmbtnAnimationPlayer){frame.animation,frame.frame,0};
    else if(!frame.animation) a->actor.sprite.frame=frame.frame;
}
bool hmbtn_livestock_buy(HmbtnLivestock *p,unsigned item,const uint16_t *name,const HmbtnFarmClock *c)
{
    if(!p || !name || !c || (item!=0x161 && item!=0x165)) return false;
    const unsigned slot=hmbtn_livestock_free(p);if(slot==HMBTN_LIVESTOCK) return false;
    HmbtnFarmAnimal *a=&p->animals[slot];*a=(HmbtnFarmAnimal){0};
    a->occupied=true;a->sheep=item==0x165;a->growth=LIVESTOCK_YOUNG_DAYS;a->affection=10;
    a->season=c->season;a->day=c->day;a->days_since_shearing=LIVESTOCK_SHEAR_DAYS;
    a->area=96;a->actor.visible=true;memcpy(a->name,name,sizeof(a->name));
    const unsigned row=slot/5;memcpy(a->actor.position,row_spots[row],sizeof(a->actor.position));
    a->actor.position[2]+=(int32_t)(slot%5)*600;a->actor.facing=row&1 ? 1 : 3;
    pose(a,0);return true;
}
void hmbtn_livestock_new_day(HmbtnLivestock *p,HmbtnFarmFeed *feed,const HmbtnFarmClock *c)
{
    /* Original farm_animals_overnight_update visits cows before sheep, even
     * when a sheep occupies an earlier shared slot. Consume before bin cleanup. */
    for(unsigned species=0;species<2;++species) for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) {
        HmbtnFarmAnimal *a=&p->animals[i];if(!a->occupied || a->sheep!=(species!=0) || a->dead) continue;
        if(a->area==96 && !a->fed_today) for(unsigned bin=0;bin<22;++bin) if(feed->barn[bin]) {
            a->fed_today=true;feed->barn[bin]=false;break;
        }
        int affection=(int)a->affection;
        if(a->talked_today) affection+=LIVESTOCK_TALK_AFFECTION;
        if(a->sheep && a->fed_yesterday && a->days_since_shearing<LIVESTOCK_SHEAR_DAYS) ++a->days_since_shearing;
        a->fed_yesterday=a->fed_today;
        affection+=a->fed_today ? LIVESTOCK_FED_AFFECTION : -LIVESTOCK_UNFED_AFFECTION;
        a->affection=(unsigned)(affection<0 ? 0 : affection>LIVESTOCK_MAX_AFFECTION ? LIVESTOCK_MAX_AFFECTION : affection);
        if(a->days_sick) {
            if(a->days_sick<LIVESTOCK_MAX_DAYS_SICK) ++a->days_sick;
            if(a->days_sick>=LIVESTOCK_SICK_DEATH_FROM
                    && hmbtn_psx_random_range(&p->random,100)<=a->days_sick*LIVESTOCK_SICK_DEATH_PER_DAY) a->dead=true;
        } else if(!a->affection && hmbtn_psx_random_range(&p->random,100)<LIVESTOCK_SICK_CHANCE) ++a->days_sick;
        if(!a->dead) {
            const unsigned old=a->sheep ? LIVESTOCK_SHEEP_OLD_YEAR : LIVESTOCK_COW_OLD_YEAR;
            if((a->age==old && hmbtn_psx_random_range(&p->random,100)<LIVESTOCK_OLD_CHANCE_FIRST)
                    || (a->age==old+1 && hmbtn_psx_random_range(&p->random,100)<LIVESTOCK_OLD_CHANCE_SECOND) || a->age>=old+2)
                a->dead=a->died_of_age=true;
        }
        if(a->growth<LIVESTOCK_MAX_GROWTH_DAYS) ++a->growth;
        if(c->season==a->season && c->day==a->day) ++a->age;
        a->timer=hmbtn_psx_random_range(&p->random,300);a->state=a->praise=0;
        a->fed_today=a->talked_today=false;a->actor.visible=!a->dead;
        a->actor.sprite=(HmbtnAnimationPlayer){0,(uint16_t)a->actor.facing,0};
    }
}
unsigned hmbtn_livestock_target(const HmbtnLivestock *p,unsigned area,const HmbtnIntroActor *player,unsigned reach)
{
    if(!p || !player || player->facing>3) return HMBTN_LIVESTOCK;
    static const int delta[4][2]={{0,-1},{-1,0},{0,1},{1,0}};
    for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) {
        const HmbtnFarmAnimal *a=&p->animals[i];const HmbtnFarmItem *item=hmbtn_farm_item(hmbtn_livestock_kind(a));
        if(a->occupied && !a->dead && a->area==area && item
                && abs(a->actor.position[0]-player->position[0]-delta[player->facing][0]*(int)reach)<item->half_width
                && abs(a->actor.position[2]-player->position[2]-delta[player->facing][1]*(int)reach)<item->half_depth
                && abs(a->actor.position[1]-player->position[1])<128) return i;
    }
    return HMBTN_LIVESTOCK;
}
bool hmbtn_livestock_cure(HmbtnLivestock *p,unsigned slot)
{
    if(!p || slot>=HMBTN_LIVESTOCK) return false;
    HmbtnFarmAnimal *a=&p->animals[slot];if(!a->occupied || a->dead || !a->days_sick) return false;
    a->days_sick=0;a->praise=45;a->timer=a->state=0;pose(a,0);return true;
}
unsigned hmbtn_livestock_talk(HmbtnLivestock *p,unsigned slot)
{
    if(!p || slot>=HMBTN_LIVESTOCK || !p->animals[slot].occupied || p->animals[slot].dead) return 0;
    HmbtnFarmAnimal *a=&p->animals[slot];if(!a->talked_today) a->praise=45;
    a->talked_today=true;p->talk_slot=slot;
    return a->days_sick ? (a->days_sick<4 ? LIVESTOCK_MSG_SICK_RECENT : LIVESTOCK_MSG_SICK_LONG)
        : a->affection>=LIVESTOCK_DEVOTED_AFFECTION ? LIVESTOCK_MSG_DEVOTED
        : a->affection>=LIVESTOCK_FOND_AFFECTION ? LIVESTOCK_MSG_FOND : LIVESTOCK_MSG_HEALTHY;
}
bool hmbtn_livestock_expand(const HmbtnLivestock *p,uint8_t *record,size_t bytes)
{
    if(!p || !record || bytes!=8192 || p->talk_slot>=HMBTN_LIVESTOCK) return false;
    uint16_t words[HMBTN_MESSAGE_WORDS];unsigned n=0;
    for(unsigned i=0;i<HMBTN_MESSAGE_WORDS;++i) {
        const unsigned c=hmbtn_read_le16(record+0x1800+i*2);
        if(c==0x4000) {
            const uint16_t *name=p->animals[p->talk_slot].name;
            for(unsigned j=0;j<HMBTN_NAME_CHARS && name[j]!=0xffff;++j) { if(n>=HMBTN_MESSAGE_WORDS) return false;words[n++]=name[j]; }
        } else { if(n>=HMBTN_MESSAGE_WORDS) return false;words[n++]=(uint16_t)c; }
        if(c==0xffff) { for(unsigned j=0;j<n;++j) hmbtn_write_le16(record+0x1800+j*2,words[j]);return true; }
    }
    return false;
}
bool hmbtn_livestock_blocked(const HmbtnLivestock *p,unsigned area,const int32_t from[3],const int32_t to[3])
{
    if(p) for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) {
        const HmbtnFarmAnimal *a=&p->animals[i];const HmbtnFarmItem *item=hmbtn_farm_item(hmbtn_livestock_kind(a));
        if(!item || a->dead || a->area!=area) continue;
        if(abs(to[0]-a->actor.position[0])<item->half_width+100 && abs(to[2]-a->actor.position[2])<item->half_depth+100
                && abs(to[0]-a->actor.position[0])+abs(to[2]-a->actor.position[2])
                    <abs(from[0]-a->actor.position[0])+abs(from[2]-a->actor.position[2])) return true;
    }
    return false;
}
void hmbtn_livestock_tick(HmbtnLivestock *p,unsigned area,const HmbtnModel *ground,const HmbtnIntroActor *player,unsigned hour)
{
    static const int delta[4][2]={{0,-LIVESTOCK_WALK_SPEED},{-LIVESTOCK_WALK_SPEED,0},{0,LIVESTOCK_WALK_SPEED},{LIVESTOCK_WALK_SPEED,0}};
    for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) {
        HmbtnFarmAnimal *a=&p->animals[i];if(!a->occupied || a->dead || a->area!=area) continue;
        if(a->praise) --a->praise;
        if(hour<6 || hour>=21 || a->days_sick) { pose(a,2);a->timer=0;continue; }
        if(!a->timer) {
            /* Same species-specific idle/walk/bed choices. Host ground probes
             * replace the original shared walker callbacks; cue timing remains partial. */
            a->state=hmbtn_psx_random_range(&p->random,a->sheep ? 3 : 4);
            a->timer=120+hmbtn_psx_random_range(&p->random,61);a->actor.facing=hmbtn_psx_random_range(&p->random,4);
            a->actor.sprite=(HmbtnAnimationPlayer){0,(uint16_t)a->actor.facing,0};
        }
        --a->timer;pose(a,a->state==0 ? 1 : a->state==2 ? 2 : 0);
        if(a->state) continue;
        const HmbtnFarmItem *item=hmbtn_farm_item(hmbtn_livestock_kind(a));
        int32_t target[3];memcpy(target,a->actor.position,sizeof(target));
        target[0]+=delta[a->actor.facing][0];target[2]+=delta[a->actor.facing][1];
        int32_t ahead[3];memcpy(ahead,target,sizeof(ahead));
        ahead[0]+=delta[a->actor.facing][0]*(item->half_width/6);ahead[2]+=delta[a->actor.facing][1]*(item->half_depth/6);
        HmbtnGroundHit hit;
        bool clear=ground && hmbtn_ground_find(ground,ahead,0xE080,64,&hit)
            && !hmbtn_livestock_blocked(p,area,a->actor.position,target)
            && !(abs(target[0]-player->position[0])<item->half_width+100 && abs(target[2]-player->position[2])<item->half_depth+100);
        if(clear && hmbtn_ground_find(ground,target,0xE080,64,&hit)) { target[1]=hit.height;memcpy(a->actor.position,target,sizeof(target)); }
        else { a->timer=0;pose(a,0); }
    }
}
