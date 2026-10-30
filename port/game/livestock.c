/* Adapted from animals.c:livestock_find_free/place_row_spot/farm_message_request
 * and livestock.c:record_init/_b, growth_kind/_b, overnight_update/_alt,
 * pick_state/_b, walk cue, frame lookup and talk/cure. Host ground checks replace
 * PS1 actor callbacks. Full species cue sequencing remains partial. */
#include "livestock.h"
#include "psx_random.h"
#include "psx_data.h"
#include <stdlib.h>
#include <string.h>
typedef struct NativeFrame { uint16_t animation,frame; } NativeFrame;
typedef struct NativeFrameRow { NativeFrame entry[4]; } NativeFrameRow;
typedef struct NativeAnimalGate { int x_min,z_min,x_max,z_max; unsigned place,target; int x,z; } NativeAnimalGate;
typedef struct NativeGateRow { NativeAnimalGate zones[2]; } NativeGateRow;
#include "livestock_tables.inc"
unsigned hmbtn_livestock_kind(const HmbtnFarmAnimal *a)
{ return !a || !a->occupied ? 0 : a->sheep ? (a->growth<14 ? 0x164u : 0x165u) : a->growth<14 ? 0x160u : a->growth<28 ? 0x161u : 0x162u; }
unsigned hmbtn_livestock_pack(const HmbtnFarmAnimal *a)
{ return a->sheep ? (a->growth<14 ? 4u : a->days_since_shearing<LIVESTOCK_SHEAR_DAYS ? 8u : a->pregnancy ? 7u : 5u) : a->pregnancy ? 3u : a->growth<14 ? 0u : 1u; }
unsigned hmbtn_livestock_scale(const HmbtnFarmAnimal *a)
{ return hmbtn_livestock_kind(a)==0x161 ? LIVESTOCK_YOUNG_SCALE : 4096; }
unsigned hmbtn_livestock_capacity(const HmbtnLivestock *p)
{ return p && p->construction && p->construction->barn_level ? HMBTN_LIVESTOCK : 10u; }
unsigned hmbtn_livestock_free(const HmbtnLivestock *p)
{
    if(p) for(unsigned i=0;i<hmbtn_livestock_capacity(p);++i) if(!p->animals[i].occupied && !p->animals[i].reserved) return i;
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
/* shop_livestock_find(filter=0) / shop_barn_step: adult species only,
 * sickness and shorn wool do not block a sale. Dead records await funerals.
 * The source sale filter excludes pregnant cows; sheep have no such guard. */
unsigned hmbtn_livestock_sale_slot(const HmbtnLivestock *p,bool sheep,unsigned row)
{
    if(p) for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) {
        const HmbtnFarmAnimal *a=&p->animals[i];
        if(!a->dead && (sheep || !a->pregnancy) && hmbtn_livestock_kind(a)==(sheep ? 0x165u : 0x162u)) {
            if(!row) return i;
            --row;
        }
    }
    return HMBTN_LIVESTOCK;
}
unsigned hmbtn_livestock_sale_price(const HmbtnLivestock *p,unsigned slot)
{
    if(!p || slot>=HMBTN_LIVESTOCK) return 0;
    const HmbtnFarmAnimal *a=&p->animals[slot];
    if(!a->occupied || a->dead || (!a->sheep && a->pregnancy) || hmbtn_livestock_kind(a)!=(a->sheep ? 0x165u : 0x162u)) return 0;
    if(a->prize_winner) return a->sheep ? 6000u : 8000u;
    if(a->affection<101) return a->sheep ? 2000u : 3000u;
    if(a->affection<201) return a->sheep ? 3000u : 4500u;
    return a->sheep ? 3500u : 5000u;
}
static void release_birth(HmbtnLivestock *p,unsigned mother)
{
    for(unsigned i=0;i<2;++i) if(p->births[i].active && p->births[i].mother==mother) {
        HmbtnFarmAnimal *child=&p->animals[p->births[i].child];
        /* A claimed empty record is not another death. Preserve a child that
         * has already been born; normal naming cannot overlap a funeral. */
        if(!child->occupied) *child=(HmbtnFarmAnimal){0};
        else child->reserved=false;
        p->births[i]=(HmbtnLivestockBirth){0};
    }
}
unsigned hmbtn_livestock_sell(HmbtnLivestock *p,unsigned slot)
{
    const unsigned price=hmbtn_livestock_sale_price(p,slot);
    if(price) { release_birth(p,slot);p->animals[slot]=(HmbtnFarmAnimal){0}; }
    return price;
}
bool hmbtn_livestock_dead(const HmbtnLivestock *p,bool sheep,bool old_age)
{
    if(p) for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) {
        const HmbtnFarmAnimal *a=&p->animals[i];
        if(a->occupied && a->dead && a->sheep==sheep && a->died_of_age==old_age) return true;
    }
    return false;
}
unsigned hmbtn_livestock_clear_dead(HmbtnLivestock *p)
{
    unsigned count=0;
    if(p) for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) if(p->animals[i].occupied && p->animals[i].dead) {
        release_birth(p,i);p->animals[i]=(HmbtnFarmAnimal){0};++count;
    }
    return count;
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
/* livestock_start_pregnancy/_b and farm_mode_slot_find(1). A claimed
 * empty record consumes capacity without becoming a drawable/countable animal. */
bool hmbtn_livestock_breed(HmbtnLivestock *p,unsigned slot,bool sheep)
{
    if(!p || slot>=HMBTN_LIVESTOCK) return false;
    HmbtnFarmAnimal *a=&p->animals[slot];
    if(a->dead || a->pregnancy || a->days_sick || a->reserved || a->sheep!=sheep
            || hmbtn_livestock_kind(a)!=(sheep ? 0x165u : 0x162u)
            || (sheep && a->days_since_shearing<LIVESTOCK_SHEAR_DAYS)) return false;
    for(unsigned i=0;i<2;++i) if(p->births[i].active && p->births[i].mother==slot) return false;
    const unsigned child=hmbtn_livestock_free(p),limit=p->construction && p->construction->barn_level ? 2u : 1u;
    if(child==HMBTN_LIVESTOCK) return false;
    for(unsigned mode=0;mode<limit;++mode) if(!p->births[mode].active) {
        p->births[mode]=(HmbtnLivestockBirth){.active=true,.mother=slot,.child=child};
        p->animals[child].reserved=true;a->pregnancy=1;a->praise=45;a->timer=a->state=0;
        a->actor.sprite=(HmbtnAnimationPlayer){0,(uint16_t)a->actor.facing,0};return true;
    }
    return false;
}
bool hmbtn_livestock_birth_ready(const HmbtnLivestock *p,unsigned mode)
{ return p && mode<2 && p->births[mode].active && p->births[mode].ready; }
HmbtnFarmAnimal *hmbtn_livestock_newborn(HmbtnLivestock *p,unsigned mode)
{
    if(!hmbtn_livestock_birth_ready(p,mode)) return NULL;
    HmbtnFarmAnimal *a=&p->animals[p->births[mode].child];return a->occupied ? a : NULL;
}
void hmbtn_livestock_birth_begin(HmbtnLivestock *p,const HmbtnFarmClock *c)
{
    for(unsigned mode=0;mode<2;++mode) if(hmbtn_livestock_birth_ready(p,mode)) {
        const HmbtnFarmAnimal *mother=&p->animals[p->births[mode].mother];
        HmbtnFarmAnimal *a=&p->animals[p->births[mode].child];
        if(a->occupied) continue; /* Retrying resource loading never spawns twice. */
        *a=(HmbtnFarmAnimal){.occupied=true,.reserved=true,.sheep=mother->sheep,.affection=10,
            .area=mother->area,.season=c->season,.day=c->day,.days_since_shearing=LIVESTOCK_SHEAR_DAYS};
        for(unsigned j=0;j<=HMBTN_NAME_CHARS;++j) a->name[j]=0xffff;
        a->actor.visible=true;a->actor.facing=mother->actor.facing;
        memcpy(a->actor.position,mother->actor.position,sizeof(a->actor.position));
        a->actor.position[2]-=LIVESTOCK_OFFSPRING_Z_OFFSET;pose(a,0);
    }
}
void hmbtn_livestock_birth_end(HmbtnLivestock *p)
{
    for(unsigned mode=0;mode<2;++mode) {
        HmbtnFarmAnimal *a=hmbtn_livestock_newborn(p,mode);
        if(!a || a->name[0]==0xffff) continue;
        const unsigned slots[2]={p->births[mode].mother,p->births[mode].child};
        a->reserved=false;p->births[mode]=(HmbtnLivestockBirth){0};
        for(unsigned j=0;j<2;++j) {
            HmbtnFarmAnimal *item=&p->animals[slots[j]];const unsigned row=slots[j]/5;
            memcpy(item->actor.position,row_spots[row],sizeof(item->actor.position));
            item->area=96;item->actor.position[2]+=(int32_t)(slots[j]%5)*600;item->actor.facing=row&1 ? 1 : 3;pose(item,0);
        }
    }
}
bool hmbtn_livestock_valid_births(const HmbtnLivestock *p)
{
    bool children[HMBTN_LIVESTOCK]={0},mothers[HMBTN_LIVESTOCK]={0};
    for(unsigned mode=0;mode<2;++mode) {
        const HmbtnLivestockBirth *b=&p->births[mode];if(!b->active) { if(b->ready || b->mother || b->child) return false;continue; }
        if((mode==1 && (!p->construction || !p->construction->barn_level)) || b->mother>=hmbtn_livestock_capacity(p)
                || b->child>=hmbtn_livestock_capacity(p) || b->mother==b->child || mothers[b->mother] || children[b->child]) return false;
        const HmbtnFarmAnimal *m=&p->animals[b->mother],*a=&p->animals[b->child];
        if(!m->occupied || m->reserved || !a->reserved || a->occupied || (b->ready ? m->pregnancy!=0 : !m->pregnancy)) return false;
        mothers[b->mother]=children[b->child]=true;
    }
    for(unsigned i=0;i<HMBTN_LIVESTOCK;++i)
        if(p->animals[i].reserved!=children[i] || (p->animals[i].pregnancy && !mothers[i])) return false;
    return true;
}
void hmbtn_livestock_new_day(HmbtnLivestock *p,HmbtnFarmFeed *feed,const HmbtnFarmClock *c)
{
    /* Original farm_animals_overnight_update visits cows before sheep, even
     * when a sheep occupies an earlier shared slot. Consume before bin cleanup. */
    for(unsigned species=0;species<2;++species) for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) {
        HmbtnFarmAnimal *a=&p->animals[i];if(!a->occupied || a->sheep!=(species!=0) || a->dead || a->reserved) continue;
        if(a->area==96 && !a->fed_today) for(unsigned bin=0;bin<22;++bin) if(feed->barn[bin]) {
            a->fed_today=true;feed->barn[bin]=false;break;
        }
        int affection=(int)a->affection;
        if(a->talked_today) affection+=LIVESTOCK_TALK_AFFECTION;
        if(a->sheep && a->fed_yesterday && a->days_since_shearing<LIVESTOCK_SHEAR_DAYS) ++a->days_since_shearing;
        a->fed_yesterday=a->fed_today;
        affection+=a->fed_today ? LIVESTOCK_FED_AFFECTION : -LIVESTOCK_UNFED_AFFECTION;
        if(a->graze_count>=LIVESTOCK_FED_GRAZE) affection+=LIVESTOCK_GRAZE_AFFECTION;
        a->graze_count=0;
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
        if(a->pregnancy) {
            if(++a->pregnancy>=LIVESTOCK_GESTATION_DAYS) {
                a->pregnancy=0;
                for(unsigned mode=0;mode<2;++mode) if(p->births[mode].active && p->births[mode].mother==i) p->births[mode].ready=true;
            }
        }
        if(a->growth<LIVESTOCK_MAX_GROWTH_DAYS) ++a->growth;
        if(c->season==a->season && c->day==a->day) ++a->age;
        a->timer=hmbtn_psx_random_range(&p->random,300);a->state=a->praise=0;
        a->fed_today=a->talked_today=a->brushed_today=a->milked_today=false;a->actor.visible=!a->dead;
        a->actor.sprite=(HmbtnAnimationPlayer){0,(uint16_t)a->actor.facing,0};
    }
    /* farm_items_apply_kind(1): only residents of the barn return to their
     * ordinary row. Animals in the field keep their outdoor placement. */
    for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) {
        HmbtnFarmAnimal *a=&p->animals[i];if(!a->occupied || a->reserved || a->area!=96) continue;
        const unsigned row=i/5;memcpy(a->actor.position,row_spots[row],sizeof(a->actor.position));
        a->actor.position[2]+=(int32_t)(i%5)*600;a->actor.facing=row&1 ? 1 : 3;pose(a,0);
    }
    /* The two maternity slots use rows 4 and 5, including outdoor mothers. */
    for(unsigned mode=0;mode<2;++mode) if(p->births[mode].active) {
        HmbtnFarmAnimal *a=&p->animals[p->births[mode].mother];
        a->area=96;memcpy(a->actor.position,row_spots[4+mode],sizeof(a->actor.position));
        a->actor.facing=mode ? 1 : 3;pose(a,0);
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
/* Original livestock_set_bit2_cue/_b, livestock_cow_cue and
 * livestock_sheep_harvest (fcef049). Explicit records replace actor flags and
 * helper/player globals. Helpers are not integrated. Sickness
 * alone is not a production refusal in these source handlers. */
bool hmbtn_livestock_brush(HmbtnLivestock *p,unsigned slot)
{
    if(!p || slot>=HMBTN_LIVESTOCK) return false;
    HmbtnFarmAnimal *a=&p->animals[slot];if(!a->occupied || a->dead) return false;
    const bool first=!a->brushed_today;
    if(first && a->affection>=10) a->praise=45;
    a->brushed_today=true;return first;
}
unsigned hmbtn_livestock_harvest(HmbtnLivestock *p,unsigned slot,bool shear)
{
    if(!p || slot>=HMBTN_LIVESTOCK) return 0;
    HmbtnFarmAnimal *a=&p->animals[slot];
    if(!a->occupied || a->dead || a->pregnancy || a->reserved || a->sheep!=shear) return 0;
    if(shear) {
        if(hmbtn_livestock_kind(a)!=0x165 || a->days_since_shearing<LIVESTOCK_SHEAR_DAYS) return 0;
    } else {
        if(hmbtn_livestock_kind(a)!=0x162 || a->milked_today) return 0;
        if(!a->fed_yesterday) { a->milked_today=true;return 0; }
        if(a->affection<=1) return 0;
    }
    const unsigned product=a->affection>200 ? (a->prize_winner ? (shear ? 0xe7u : 0xe3u) : (shear ? 0x93u : 0x8du))
        : a->affection>100 ? (shear ? 0x92u : 0x8cu) : (shear ? 0x91u : 0x8bu);
    if(shear) { a->days_since_shearing=0;a->actor.sprite=(HmbtnAnimationPlayer){0,(uint16_t)a->actor.facing,0}; }
    else a->milked_today=true;
    if(a->affection>=10) a->praise=45;
    return product;
}
/* player_start_command_tool, PLAYER_MODE_USE_ANIMAL_TOOL and
 * player_apply_tool_target: latch a native slot, wait for original ANI once,
 * then hand the product to the shared player_show_item presentation. */
bool hmbtn_livestock_tool_tick(HmbtnLivestock *p,unsigned area,HmbtnFarmAction *a,HmbtnIntroActor *player,
    HmbtnHouseInteraction *h,HmbtnEventItem *shown,uint32_t pressed)
{
    a->sound=-1;a->changed=a->finished_bank=a->release_sound=false;a->broken_item=0;
    if(a->phase==HMBTN_FARM_ANIMAL_TOOL) {
        if(!player->animation_done) return true;
        unsigned product=0;
        if(!h->inventory->held_item.id && shown->phase==HMBTN_EVENT_ITEM_IDLE) {
            if(a->kind==0x1d) { hmbtn_livestock_bell(p,area,player);a->changed=true; }
            else if(a->kind==0x1c) a->changed=hmbtn_livestock_brush(p,a->target);
            else { product=hmbtn_livestock_harvest(p,a->target,a->kind==0x1b);a->changed=product!=0; }
        }
        hmbtn_farm_action_cost(a,h,a->kind==0x1d ? 0 : 1);
        a->finished_bank=true;a->bank=-1;a->phase=HMBTN_FARM_ACTION_FREE;
        player->sprite=(HmbtnAnimationPlayer){0,(uint16_t)player->facing,0};player->animation_done=false;
        if(product) (void)hmbtn_event_item_show(shown,player,h->inventory,product);
        return true;
    }
    const unsigned id=h->inventory->equipped.id;
    if(a->phase!=HMBTN_FARM_ACTION_FREE || !(pressed&HMBTN_PAD_SQUARE) || id<0x1a || id>0x1d
            || player->facing>3 || h->inventory->held_item.id || shown->phase!=HMBTN_EVENT_ITEM_IDLE || a->fatigue>=100) return false;
    a->target=hmbtn_livestock_target(p,area,player,128);a->kind=id;
    a->phase=HMBTN_FARM_ANIMAL_TOOL;a->bank=(int)(18+id-0x1a);a->bank_pending=true;a->sound=(int)(10+id-0x1a);
    player->sprite=(HmbtnAnimationPlayer){(uint16_t)player->facing,0,0};player->animation_done=false;return true;
}
unsigned hmbtn_livestock_talk(HmbtnLivestock *p,unsigned slot)
{
    if(!p || slot>=HMBTN_LIVESTOCK || !p->animals[slot].occupied || p->animals[slot].dead) return 0;
    HmbtnFarmAnimal *a=&p->animals[slot];if(!a->talked_today) a->praise=45;
    a->talked_today=true;p->talk_slot=slot;
    return a->days_sick ? (a->days_sick<4 ? LIVESTOCK_MSG_SICK_RECENT : LIVESTOCK_MSG_SICK_LONG)
        : a->pregnancy==1 ? LIVESTOCK_MSG_PREGNANT_START
        : a->pregnancy ? LIVESTOCK_MSG_PREGNANT
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
        } else if(c==0x4001) {
            const unsigned days=LIVESTOCK_GESTATION_DAYS-p->animals[p->talk_slot].pregnancy;
            if(n+2>HMBTN_MESSAGE_WORDS) return false;
            words[n++]=days>=10 ? (uint16_t)(0x40+days/10) : 0x0ffd;
            words[n++]=(uint16_t)(0x40+days%10);
        } else { if(n>=HMBTN_MESSAGE_WORDS) return false;words[n++]=(uint16_t)c; }
        if(c==0xffff) { for(unsigned j=0;j<n;++j) hmbtn_write_le16(record+0x1800+j*2,words[j]);return true; }
    }
    return false;
}
bool hmbtn_livestock_blocked(const HmbtnLivestock *p,unsigned area,const int32_t from[3],const int32_t to[3])
{
    if(p) for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) {
        const HmbtnFarmAnimal *a=&p->animals[i];const HmbtnFarmItem *item=hmbtn_farm_item(hmbtn_livestock_kind(a));
        if(!a->occupied || a->reserved || !item || a->dead || a->area!=area) continue;
        if(abs(to[0]-a->actor.position[0])<item->half_width+100 && abs(to[2]-a->actor.position[2])<item->half_depth+100
                && abs(to[0]-a->actor.position[0])+abs(to[2]-a->actor.position[2])
                    <abs(from[0]-a->actor.position[0])+abs(from[2]-a->actor.position[2])) return true;
    }
    return false;
}
/* farm_hourly_refresh -> livestock_action_timer/_b. The BIN compares farm
 * location 1 (native area 0), not the decoded enum's misleading zero. Sheep
 * specifically penalize weather 1; cows penalize every nonzero weather. */
void hmbtn_livestock_hour(HmbtnLivestock *p,HmbtnFarmField *field,const HmbtnFarmClock *clock,unsigned weather)
{
    if(!p || !field || !clock || clock->season==3) return;
    for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) {
        HmbtnFarmAnimal *a=&p->animals[i];
        if(!a->occupied || a->dead || a->reserved || a->area!=0) continue;
        if(a->sheep ? weather==1 : weather!=0) {
            a->affection=a->affection>LIVESTOCK_BAD_WEATHER_AFFECTION ? a->affection-LIVESTOCK_BAD_WEATHER_AFFECTION : 0;
        } else {
            a->graze_count=(a->graze_count+1)&0xffffu;
            if(a->graze_count>=LIVESTOCK_FED_GRAZE && !a->fed_today)
                a->fed_today=hmbtn_farm_field_graze(field,a->actor.position);
        }
    }
}
static void command(HmbtnFarmAnimal *a,const int32_t destination[3],unsigned state)
{
    memcpy(a->destination,destination,sizeof(a->destination));a->state=state;a->timer=300;
}
void hmbtn_livestock_bell(HmbtnLivestock *p,unsigned area,const HmbtnIntroActor *player)
{
    if(!p || !player) return;
    for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) {
        HmbtnFarmAnimal *a=&p->animals[i];
        if(a->occupied && !a->dead && !a->reserved && a->area==area
                && hmbtn_psx_random_range(&p->random,200)<a->affection) command(a,player->position,4);
    }
}
void hmbtn_livestock_push(HmbtnLivestock *p,unsigned area,const HmbtnIntroActor *player,bool pushing)
{
    const unsigned slot=pushing ? hmbtn_livestock_target(p,area,player,224) : HMBTN_LIVESTOCK;
    if(slot==HMBTN_LIVESTOCK) { p->push_frames=0;return; }
    if(p->push_slot!=slot || p->push_area!=area || p->push_facing!=player->facing) p->push_frames=0;
    p->push_slot=slot;p->push_area=area;p->push_facing=player->facing;
    if(p->push_frames<51) { ++p->push_frames;return; }
    HmbtnFarmAnimal *a=&p->animals[slot];
    int32_t goal[3];memcpy(goal,a->actor.position,sizeof(goal));
    const unsigned axis=player->facing&1u ? 0u : 2u;
    goal[axis]+=player->facing<2 ? -800 : 800;
    command(a,goal,5);a->actor.facing=player->facing;
}
static bool gate(HmbtnLivestock *p,HmbtnFarmAnimal *a,unsigned area,const uint8_t *section,size_t size,HmbtnAreaDoors *doors)
{
    /* farm_zone_rows[0] is the livestock door, independent of the player's
     * wider interaction rectangle. The source's place 1 maps to native 0. */
    const unsigned place=area==0 ? 1u : area==96 ? 5u : 0u;
    for(unsigned j=0;j<2;++j) {
        const NativeAnimalGate *g=&animal_gates[0].zones[j];
        if(place!=g->place || a->actor.position[0]<=g->x_min || a->actor.position[0]>=g->x_max
                || a->actor.position[2]<=g->z_min || a->actor.position[2]>=g->z_max) continue;
        const unsigned target=g->target==1 ? 0u : 96u;
        const int32_t arrival[3]={g->x,0,g->z};
        /* Do not place two animals in the same doorway. The first one must
         * clear the arrival before the next can cross. */
        for(unsigned k=0;k<HMBTN_LIVESTOCK;++k) {
            const HmbtnFarmAnimal *b=&p->animals[k];const HmbtnFarmItem *item=hmbtn_farm_item(hmbtn_livestock_kind(b));
            if(item && b->occupied && !b->dead && b->area==target
                    && abs(arrival[0]-b->actor.position[0])<item->half_width+100
                    && abs(arrival[2]-b->actor.position[2])<item->half_depth+100) return true;
        }
        if(area==0 && hmbtn_area_doors_request(doors,section,size,1,2)!=HMBTN_DOOR_READY) return true;
        memcpy(a->actor.position,arrival,sizeof(arrival));a->area=target;
        a->state=1;a->timer=90;pose(a,0);return true;
    }
    return false;
}
static bool step_clear(const HmbtnLivestock *p,const HmbtnFarmAnimal *a,unsigned area,const HmbtnModel *ground,
    const HmbtnFarmField *field,const HmbtnIntroActor *player,unsigned facing,int speed,int32_t target[3])
{
    static const int delta[4][2]={{0,-1},{-1,0},{0,1},{1,0}};
    memcpy(target,a->actor.position,3*sizeof(*target));
    target[0]+=delta[facing][0]*speed;target[2]+=delta[facing][1]*speed;
    const HmbtnFarmItem *item=hmbtn_farm_item(hmbtn_livestock_kind(a));
    int32_t ahead[3];memcpy(ahead,target,sizeof(ahead));
    ahead[0]+=delta[facing][0]*item->half_width;ahead[2]+=delta[facing][1]*item->half_depth;
    bool doorway=false;
    for(unsigned j=0;j<2;++j) {
        const NativeAnimalGate *g=&animal_gates[0].zones[j];
        const unsigned place=area==0 ? 1u : area==96 ? 5u : 0u;
        if(g->place==place && facing==(area==0 ? 2u : 0u) && target[0]>g->x_min && target[0]<g->x_max
                && target[2]>g->z_min-item->half_depth && target[2]<g->z_max+item->half_depth) doorway=true;
    }
    /* farm_walker_try_exits allows the footprint to straddle its own gate.
     * Testing a full half-depth beyond that plane would hit the map edge
     * before the actor's centre could ever enter the original exit rectangle. */
    return ground && (doorway || hmbtn_farm_settle(field,ground,ahead,0xE080,64))
        && (doorway || !hmbtn_farm_field_probe(field,ahead)) && !hmbtn_livestock_blocked(p,area,a->actor.position,target)
        && !(abs(target[0]-player->position[0])<item->half_width+100 && abs(target[2]-player->position[2])<item->half_depth+100)
        && hmbtn_farm_settle(field,ground,target,doorway ? 0xC000 : 0xE080,64);
}
void hmbtn_livestock_tick(HmbtnLivestock *p,unsigned area,const HmbtnModel *ground,const HmbtnFarmField *field,
    const uint8_t *section,size_t size,HmbtnAreaDoors *doors,const HmbtnIntroActor *player,unsigned hour)
{
    for(unsigned i=0;i<HMBTN_LIVESTOCK;++i) {
        HmbtnFarmAnimal *a=&p->animals[i];if(!a->occupied || a->dead || a->reserved || a->area!=area) continue;
        if(a->praise) --a->praise;
        if(gate(p,a,area,section,size,doors)) continue;
        const bool directed=a->state==4 || a->state==5;
        if(!directed && (hour<6 || hour>=21 || a->days_sick)) { pose(a,2);a->timer=0;continue; }
        if(!a->timer) {
            a->state=hmbtn_psx_random_range(&p->random,a->sheep ? 3 : 4);
            a->timer=120+hmbtn_psx_random_range(&p->random,61);a->actor.facing=hmbtn_psx_random_range(&p->random,4);
            a->actor.sprite=(HmbtnAnimationPlayer){0,(uint16_t)a->actor.facing,0};
        }
        --a->timer;
        if(a->state!=0 && a->state!=4 && a->state!=5) { pose(a,a->state==2 ? 2 : 0);continue; }
        unsigned facing=a->actor.facing;
        if(a->state==4 || a->state==5) {
            const int dx=a->destination[0]-a->actor.position[0],dz=a->destination[2]-a->actor.position[2];
            if(abs(dx)<600 && abs(dz)<600 && (abs(dx)<50 || abs(dz)<50)) {
                a->timer=300;a->state=1;pose(a,0);continue;
            }
            facing=abs(dx)>abs(dz) ? (dx<0 ? 1u : 3u) : (dz<0 ? 0u : 2u);
        }
        const int speed=a->pregnancy && !a->sheep ? LIVESTOCK_WALK_SPEED_PREGNANT : LIVESTOCK_WALK_SPEED;
        int32_t target[3];bool clear=step_clear(p,a,area,ground,field,player,facing,speed,target);
        if(!clear && (a->state==4 || a->state==5)) {
            /* Existing walker probes its two side headings around obstacles. */
            for(unsigned side=1;side<=3 && !clear;side+=2) {
                const unsigned turn=(facing+side)%4;
                clear=step_clear(p,a,area,ground,field,player,turn,speed,target);
                if(clear) facing=turn;
            }
        }
        if(clear) { a->actor.facing=facing;memcpy(a->actor.position,target,sizeof(target));pose(a,1); }
        else { if(a->state==0) a->timer=0;pose(a,0); }
    }
}
