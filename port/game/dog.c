/* Native adaptation of animals/dog.c (phase cues, care, call) and
 * animals.c: farm_walker_{move,pick_heading,pick_turn}, farm_place_held.
 * The original packed resources remain installed data, never compiled assets. */
#include "dog.h"
#include "psx_random.h"
#include <stdlib.h>
#include <string.h>
static const int delta[4][2]={{0,-1},{-1,0},{0,1},{1,0}};
static unsigned random_range(HmbtnDog *d,unsigned n)
{ return hmbtn_psx_random_range(&d->random,n); }
static void pose(HmbtnDog *d,unsigned action)
{
    /* cue_anim_table: adult-only ball poses 18..21 are in the same ANI. */
    static const unsigned base[]={0,1,5,9,13,17,21,25,29,33,34,38,42,46,44,54,58,62,42,46,50,66};
    const unsigned facing=d->actor.facing;
    const bool fixed=action==0 || action==9 || action==21;
    const unsigned animation=fixed ? base[action]
        : action>=12 && action<=14 ? base[action]+(facing>=2) : base[action]+facing;
    const unsigned frame=fixed ? facing : 0;
    if(d->actor.sprite.animation!=animation) {
        d->actor.sprite=(HmbtnAnimationPlayer){(uint16_t)animation,(uint16_t)frame,0};d->actor.animation_done=false;
    } else if(fixed) d->actor.sprite.frame=(uint16_t)frame;
}
static void phase(HmbtnDog *d,unsigned next)
{ d->phase=next;d->step=d->timer=d->interrupt=0;memset(d->actor.velocity,0,sizeof(d->actor.velocity)); }
void hmbtn_dog_init(HmbtnDog *d)
{
    *d=(HmbtnDog){.affection=10,.random=1,.sound=-1};
    d->actor.position[0]=-1800;d->actor.position[2]=3100;d->actor.visible=true;
}
void hmbtn_dog_new_day(HmbtnDog *d,unsigned weather)
{
    unsigned value=d->affection+(d->called_today ? 2u : 0u);
    if(value>255) value=255;
    if(d->handled_today) { value+=2;if(value>255) value=255; }
    if(!d->called_today && !d->handled_today && !d->trained_today && value) --value;
    if(d->trained_today) {
        d->training+=value<10 ? 1u : value<20 ? 2u : 3u;
        if(d->training>255) d->training=255;
    }
    /* dog_tick's outdoor-place list, expressed as native logical area ids. */
    static const unsigned outside[]={0,28,72,68,64,60,36,48,40,32,44,52,56,24,76};
    for(unsigned i=0;i<sizeof(outside)/sizeof(*outside);++i) if(d->area==outside[i] && weather) {
        value=value>5 ? value-5 : 0;break;
    }
    d->affection=value;d->handled_today=d->called_today=d->trained_today=false;d->praise=0;
    if(d->fetch>=2) { hmbtn_dog_ball_place(d,d->area,d->actor.position);d->fetch=0;phase(d,0); }
}
unsigned hmbtn_dog_kind(const HmbtnDog *d) { return d->adult ? HMBTN_ADULT_DOG : HMBTN_YOUNG_DOG; }
void hmbtn_dog_morning(HmbtnDog *d,unsigned weather,const HmbtnFarmClock *c,HmbtnInventory *h)
{
    hmbtn_dog_new_day(d,weather);
    if(!d->adult && c->year==0 && c->season==2) {
        d->adult=true;d->actor.sprite=(HmbtnAnimationPlayer){0,(uint16_t)d->actor.facing,0};d->actor.animation_done=false;
        if(d->held && h->held_item.id==HMBTN_YOUNG_DOG) h->held_item.id=HMBTN_ADULT_DOG;
    }
}
void hmbtn_dog_ball_place(HmbtnDog *d,unsigned area,const int32_t position[3])
{ d->ball.placed=true;d->ball.area=area;memcpy(d->ball.position,position,sizeof(d->ball.position)); }
bool hmbtn_dog_ball_disarm(HmbtnDog *d)
{
    const bool armed=d->ball.armed;d->ball.armed=d->ball.placed=false;
    d->fetch=d->fetch_time=d->fetch_step=0;d->approaching=false;phase(d,0);return armed;
}
bool hmbtn_dog_fetch(HmbtnDog *d,unsigned area,unsigned hour)
{
    if(!d->adult || d->held || d->fetch || d->area!=area || !d->ball.placed || d->ball.area!=area
            || hour<6 || hour>=21 || abs(d->ball.position[0]-d->actor.position[0])>=2000
            || abs(d->ball.position[2]-d->actor.position[2])>=2000) return false;
    d->trained_today=true;d->fetch=1;d->fetch_time=d->fetch_step=0;d->call_ticks=0;d->approaching=true;
    d->walk_timer=100;phase(d,7);return true;
}
static bool clear(const HmbtnDogWorld *w,const int32_t from[3],int32_t point[3])
{
    return w->ground && !hmbtn_farm_field_probe(w->field,point)
        && hmbtn_farm_settle(w->field,w->ground,point,0xE087,64)
        && (!w->blocked || !w->blocked(w->context,from,point));
}
/* Original weighted turns, including the destination bias for a called dog. */
#include "dog_turns.inc"
static unsigned bias(const HmbtnIntroActor *actor,const int32_t position[3])
{
    static const unsigned heading[9]={0,5,7,6,1,2,8,4,3};
    const int dx=position[0]-actor->position[0],dz=position[2]-actor->position[2];
    const unsigned x=dx==0 ? 0 : dx>0 ? 1 : 2,z=abs(dz)*3<abs(dx) ? 0 : dz==0 ? 0 : dz>0 ? 1 : 2;
    return heading[x*3+z];
}
static unsigned turn(HmbtnIntroActor *actor,uint32_t *random,unsigned row,unsigned destination)
{
    const unsigned roll=hmbtn_psx_random_range(random,101);unsigned total=0;
    for(unsigned i=0;i<4;++i) {
        const unsigned weight=dog_turn_weights[actor->facing][row][destination][i];
        if(weight) { total+=weight;if(total>=roll) return i; }
    }
    return 4;
}
static bool probe(const HmbtnIntroActor *actor,const HmbtnDogWorld *w,unsigned facing,int distance,int32_t out[3])
{
    memcpy(out,actor->position,sizeof(actor->position));
    out[0]+=delta[facing][0]*distance;out[2]+=delta[facing][1]*distance;
    return clear(w,actor->position,out)
        && !(abs(out[0]-w->player->position[0])<300 && abs(out[2]-w->player->position[2])<300);
}
bool hmbtn_pet_walk(HmbtnIntroActor *actor,uint32_t *random,bool *approaching,unsigned *walk_timer,unsigned *walker_ticks,
    const HmbtnDogWorld *w,unsigned speed,const int32_t destination[3],bool stop)
{
    const unsigned limit=(((*walker_ticks)/150)&1u ? 1080u : 720u)/speed;
    ++(*walker_ticks);if((*walker_ticks)>=300) (*walker_ticks)=0;
    int32_t point[3];
    if(++(*walk_timer)>=limit) {
        (*walk_timer)=0;
        const unsigned facing=actor->facing,left=(facing+3)%4,right=(facing+1)%4;
        /* The original mask marks FREE directions despite its name 'blocked'. */
        const unsigned mask=(probe(actor,w,facing,(int)speed,point) ? 1u : 0u)
            |(probe(actor,w,left,400,point) ? 4u : 0u)|(probe(actor,w,right,400,point) ? 2u : 0u);
        const unsigned direction=(*approaching) ? bias(actor,destination) : 0;
        unsigned next=4;
        switch(mask) {
        case 7:next=turn(actor,random,0,direction);break;
        case 5:next=turn(actor,random,1,direction);break;
        case 3:next=turn(actor,random,2,direction);break;
        case 1:next=facing;break;
        case 6:next=turn(actor,random,3,direction);break;
        case 4:next=left;break;
        case 2:next=right;break;
        default:if(probe(actor,w,(facing+2)%4,400,point)) next=(facing+2)%4;break;
        }
        if(next!=4) actor->facing=next;
    }
    const int dx=abs(destination[0]-actor->position[0]),dz=abs(destination[2]-actor->position[2]);
    if((*approaching) && dx<600 && dz<600 && (dx<50 || dz<50)) (*approaching)=false;
    bool moving=(!stop || (*approaching)) && probe(actor,w,actor->facing,(int)speed+200,point);
    if(moving && probe(actor,w,actor->facing,(int)speed,point)) memcpy(actor->position,point,sizeof(point));
    else { moving=false;(*walk_timer)+=2; }
    return moving;
}
static void walk(HmbtnDog *d,const HmbtnDogWorld *w,unsigned speed,const int32_t destination[3],bool mouth)
{
    const bool moving=hmbtn_pet_walk(&d->actor,&d->random,&d->approaching,&d->walk_timer,&d->walker_ticks,
        w,speed,destination,d->call_ticks || d->fetch);
    pose(d,moving ? (d->phase==18 ? 1u : mouth ? 19u : 4u) : mouth ? 18u : 0u);
}
typedef struct DogCue { unsigned pose,frames; } DogCue;
static const DogCue cues[19][4]={
    [0]={{0,30}},[1]={{7,10},{0,10}},[2]={{0,10},{7,10},{2,0}},
    [8]={{5,90}},[9]={{5,90}},[10]={{6,10}},[11]={{17,60}},[12]={{7,10},{8,60}},
    [13]={{9,30},{10,90},{9,30}},[14]={{7,30},{14,90},{7,30}},
    [15]={{0,30},{12,90},{0,30}},[16]={{7,30},{13,90},{7,30}}
};
static const unsigned steps[19]={[0]=1,[1]=2,[2]=3,[8]=1,[9]=1,[10]=1,[11]=1,[12]=2,[13]=3,[14]=3,[15]=3,[16]=3};
static void pick(HmbtnDog *d,unsigned hour)
{
    static const unsigned modes[8]={18,8,10,12,13,14,15,16};
    const unsigned roll=hour<6 || hour>=21 || d->phase==15 ? 0 : random_range(d,d->adult ? 6 : 8);
    const unsigned next=hour<6 || hour>=21 ? 2 : d->phase==15 ? 1 : d->adult && roll==5 ? 11 : modes[roll];
    if(next!=d->phase) phase(d,next);
}
void hmbtn_dog_tick(HmbtnDog *d,const HmbtnDogWorld *w,unsigned hour)
{
    d->sound=-1;
    if(d->held || d->area!=w->area) return;
    if(d->praise) --d->praise;
    if(d->fetch) {
        ++d->fetch_time;
        switch(d->fetch) {
        case 1: /* dog_approach_script: seek the live marker, up to 300 ticks. */
            if(!d->ball.placed || d->ball.area!=d->area) { d->fetch=0;d->approaching=false;phase(d,0);break; }
            walk(d,w,36,d->ball.position,false);
            if(!d->approaching) { d->ball.placed=false;d->fetch=2;d->fetch_time=d->fetch_step=0; }
            else if(d->fetch_time>=300 && d->actor.animation_done) { d->fetch=0;d->approaching=false;phase(d,0); }
            break;
        case 2: /* claim stamp 18:1; two ANI ends precede the return walk. */
            d->ball.placed=false;pose(d,18);
            if(d->actor.animation_done && ++d->fetch_step>=2) {
                d->fetch=3;d->fetch_time=0;d->approaching=true;d->walk_timer=100;
            }
            break;
        case 3:
            walk(d,w,36,w->player->position,true);
            if(!d->approaching) { d->fetch=4;d->fetch_time=0;d->sound=25;pose(d,21); }
            else if(d->fetch_time>=300 && d->actor.animation_done) { hmbtn_dog_ball_place(d,d->area,d->actor.position);d->fetch=0;d->approaching=false;phase(d,0); }
            break;
        case 4:
            pose(d,21);
            if(d->fetch_time>=90 && d->actor.animation_done) { hmbtn_dog_ball_place(d,d->area,d->actor.position);d->fetch=0;phase(d,0); }
            break;
        }
        return;
    }
    if(d->phase==17 || d->phase==18) {
        walk(d,w,d->phase==17 ? (d->adult ? 36 : 24) : 12,w->player->position,false);
        if(d->call_ticks && ++d->call_ticks>=300) { d->call_ticks=0;d->approaching=false;phase(d,0); }
        if(++d->interrupt>=300 && !d->approaching) { pick(d,hour);d->interrupt=0; }
        return;
    }
    const DogCue *cue=&cues[d->phase][d->step];pose(d,cue->pose);
    ++d->timer;
    if(d->actor.animation_done && d->timer>=cue->frames) {
        if(d->phase==2 && d->step==2) { /* Night's final pose waits for dawn. */
            if(hour>=6 && hour<21) pick(d,hour);
            return;
        }
        d->timer=0;
        if(++d->step>=steps[d->phase]) { d->step=0;d->interrupt=300; }
    }
    if(++d->interrupt>=300 && (d->phase!=2 || (hour>=6 && hour<21))) { pick(d,hour);d->interrupt=0; }
}
static void front(const HmbtnIntroActor *a,int32_t point[3])
{ memcpy(point,a->position,sizeof(a->position));point[0]+=delta[a->facing&3][0]*300;point[2]+=delta[a->facing&3][1]*300; }
static void put_down(HmbtnDog *d,HmbtnInventory *h,unsigned area,const int32_t point[3])
{
    d->held=false;d->area=area;d->actor.visible=true;memcpy(d->actor.position,point,sizeof(d->actor.position));
    if(!d->handled_today) d->praise=45;
    d->handled_today=true;d->approaching=false;d->call_ticks=0;phase(d,9);pose(d,5);
    h->held_item=(HmbtnStoredTool){0};
}
bool hmbtn_dog_carry_tick(HmbtnDog *d,HmbtnInventory *h,HmbtnIntroActor *a,const HmbtnDogWorld *w,uint32_t pressed,int *sound)
{
    *sound=-1;
    if(d->carry_phase) {
        if(!a->animation_done) return true;
        int32_t point[3];front(a,point);
        if(d->carry_phase==2 && clear(w,a->position,point)) { put_down(d,h,w->area,point);*sound=9; }
        d->carry_phase=0;a->sprite=(HmbtnAnimationPlayer){(uint16_t)(d->held ? 41 : h->held_item.id ? 27 : 0),(uint16_t)a->facing,0};a->animation_done=false;return true;
    }
    if(!(pressed&HMBTN_PAD_CROSS) || a->facing>3) return false;
    if(d->held) {
        int32_t point[3];front(a,point);
        if(clear(w,a->position,point)) { d->carry_phase=2;a->sprite=(HmbtnAnimationPlayer){(uint16_t)(23+a->facing),0,0};a->animation_done=false; }
        return true;
    }
    if(h->held_item.id) return false;
    const int32_t *ball=d->ball.placed && d->ball.area==w->area ? d->ball.position
        : d->fetch>=2 && d->area==w->area ? d->actor.position : NULL;
    if(ball && abs(ball[1]-a->position[1])<200
            && abs(ball[0]-a->position[0]-delta[a->facing][0]*224)<200
            && abs(ball[2]-a->position[2]-delta[a->facing][1]*224)<200) {
        d->ball.placed=false;d->fetch=d->fetch_time=d->fetch_step=0;d->approaching=false;phase(d,18);
        h->held_item=(HmbtnStoredTool){HMBTN_DOG_BALL,0,1};d->carry_phase=3;*sound=7;
        a->sprite=(HmbtnAnimationPlayer){(uint16_t)(11+a->facing),0,0};a->animation_done=false;return true;
    }
    if(d->area!=w->area || abs(a->position[1]-d->actor.position[1])>=200
        || abs(d->actor.position[0]-a->position[0]-delta[a->facing][0]*128)>=200
        || abs(d->actor.position[2]-a->position[2]-delta[a->facing][1]*128)>=200) return false;
    d->held=true;d->actor.visible=false;d->carry_phase=1;d->praise=0;d->approaching=false;d->call_ticks=0;
    h->held_item=(HmbtnStoredTool){(uint16_t)hmbtn_dog_kind(d),0,1};*sound=7;
    a->sprite=(HmbtnAnimationPlayer){(uint16_t)(37+a->facing),0,0};a->animation_done=false;return true;
}
bool hmbtn_dog_call_tick(HmbtnDog *d,HmbtnIntroActor *a,unsigned area,uint32_t pressed,int *sound)
{
    *sound=-1;
    if(d->whistling) {
        if(!a->animation_done) return true;
        d->whistling=false;a->sprite=(HmbtnAnimationPlayer){0,(uint16_t)a->facing,0};a->animation_done=false;
        /* farm_mode_check_poll waits until the player can act again. */
        if(!d->held && !d->fetch && d->area==area && random_range(d,200)<d->affection+50) {
            d->called_today=true;d->approaching=true;d->call_ticks=1;phase(d,17);*sound=25;
        }
        return true;
    }
    if(!(pressed&HMBTN_PAD_L1)) return false;
    d->whistling=true;a->sprite=(HmbtnAnimationPlayer){(uint16_t)(67+a->facing),0,0};a->animation_done=false;*sound=18;
    return true;
}
void hmbtn_dog_latch_place(HmbtnDog *d,unsigned area) { if(d->held) d->area=area; }
void hmbtn_dog_release(HmbtnDog *d,HmbtnInventory *h,const HmbtnIntroActor *a,unsigned area,const HmbtnModel *ground)
{
    if(!d->held) return;
    int32_t point[3];front(a,point);HmbtnGroundHit hit;
    if(ground && hmbtn_ground_find(ground,point,0,200,&hit)) point[1]=hit.height;else memcpy(point,a->position,sizeof(point));
    put_down(d,h,area,point);d->carry_phase=0;
}
