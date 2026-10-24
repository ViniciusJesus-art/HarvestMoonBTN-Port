/* Native views of cue_history.c, event_rules.c and villager.c. */
#include "village.h"
#include "psx_data.h"
#include "psx_random.h"
#include "ground.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct HmbtnHistory { const uint16_t *words;size_t count; } HmbtnHistory;
typedef struct HmbtnDoorRule { unsigned a,b,x,y,result; } HmbtnDoorRule;
typedef struct HmbtnDoorScript { unsigned id,count;const HmbtnDoorRule *rules; } HmbtnDoorScript;
#include "village_tables.inc"
static bool girl(unsigned id) { return id==3 || id==7 || id==9 || id==14 || id==20; }
static bool flag(const HmbtnVillage *v,unsigned id) { return id<HMBTN_VILLAGE_FLAGS && v->flags[id]; }
static int half(const uint8_t *p) { const unsigned x=hmbtn_read_le16(p);return x<32768 ? (int)x : (int)x-65536; }
void hmbtn_village_init(HmbtnVillage *v,const uint8_t *data,size_t size)
{
    memset(v,0,sizeof(*v));v->schedule_data=data;v->schedule_size=size;v->date_key=UINT32_MAX;
    hmbtn_event_days_init(&v->days);
    v->flags[EVENT_FLAG_SINGLE]=true;v->ann_birthday_season=1;v->ann_birthday_day=16;
    for(unsigned i=0;i<HMBTN_VILLAGERS;++i) { v->people[i].friendship=girl(i) ? 0 : 50;v->people[i].location=54; }
}
static int history(const HmbtnVillage *v,unsigned id)
{
    const HmbtnHistory *h=&histories[id];size_t i=0;
    while(i<h->count) {
        unsigned op=h->words[i++];
        if(op==65532) return i<h->count && h->words[i]!=65531 ? (int)(h->words[i]&~3u) : -1;
        if(op!=65533) return -1;
        bool passed=true;
        while(i<h->count && h->words[i]!=65534 && h->words[i]!=65531) {
            unsigned word=h->words[i++];bool value=flag(v,word&0x7fffu);
            passed &= word&0x8000u ? !value : value;
        }
        if(i==h->count) return -1;
        if(h->words[i++]==65531) continue;
        if(i==h->count) return -1;
        unsigned selected=h->words[i++];
        if(passed) return selected==65531 ? -1 : (int)(selected&~3u);
    }
    return -1;
}
bool hmbtn_village_sync(HmbtnVillage *v,const HmbtnFarmClock *c,char *error,size_t size)
{
    for(unsigned i=0;i<7;++i) v->flags[11+i]=i==c->weekday;
    for(unsigned i=0;i<4;++i) v->flags[23+i]=i==c->season;
    for(unsigned i=0;i<30;++i) v->flags[120+i]=i==c->day;
    v->flags[EVENT_FLAG_WEATHER_SUNNY]=true;
    v->flags[EVENT_FLAG_DATED_EVENT_TODAY]=hmbtn_farm_clock_date_index(c->season,c->day)!=0;
    const unsigned key=(c->year*4+c->season)*30+c->day;
    if(key!=v->date_key) {
        for(unsigned id=0;id<HMBTN_VILLAGERS;++id) {
            HmbtnVillager *p=&v->people[id];int at=history(v,id);
            p->step=0;p->placed=false;p->actor.visible=false;
            p->delay=0;p->blocked_by_player=false;
            for(unsigned i=0;i<30;++i) {
                p->schedule[i][0]=p->schedule[i][1]=0;p->schedule[i][2]=p->schedule[i][3]=54;
                if(at>=0) {
                    if((size_t)at>v->schedule_size || 240>v->schedule_size-(size_t)at) {
                        snprintf(error,size,"Villager %u schedule outside imported block",id);return false;
                    }
                    for(unsigned a=0;a<4;++a) p->schedule[i][a]=hmbtn_read_le16(v->schedule_data+at+i*8+a*2);
                }
            }
        }
        v->date_key=key;
    }
    const unsigned now=c->hour*60+c->minute+(c->hour<6 ? 1440 : 0);
    v->minute=now;
    for(unsigned id=0;id<HMBTN_VILLAGERS;++id) {
        HmbtnVillager *p=&v->people[id];unsigned step=p->step;
        for(unsigned i=step+1;i<30;++i) {
            unsigned time=p->schedule[i][0];if(time<360) time+=1440;
            time+=p->delay;
            if(now<time) break;
            step=i;
        }
        if(p->step!=step) { p->placed=false;p->step=step; }
        p->location=p->schedule[step][3];p->row=p->schedule[step][1]&255u;
    }
    return true;
}
static bool inside(const int32_t p[3],const int32_t centre[3],int radius)
{
    return llabs((int64_t)p[0]-centre[0])<radius && llabs((int64_t)p[2]-centre[2])<radius
        && llabs((int64_t)p[1]-centre[1])<20;
}
static bool approaching(const int32_t from[3],const int32_t to[3],const int32_t centre[3],int radius)
{
    if(!inside(to,centre,radius)) return false;
    if(!inside(from,centre,radius)) return true;
    const int64_t dx=(int64_t)from[0]-centre[0],dz=(int64_t)from[2]-centre[2];
    const int64_t nx=(int64_t)to[0]-centre[0],nz=(int64_t)to[2]-centre[2];
    return nx*nx+nz*nz<=dx*dx+dz*dz;
}
bool hmbtn_village_blocked(const void *context,const int32_t from[3],const int32_t to[3])
{
    const HmbtnVillage *v=context;
    if(!v) return false;
    for(unsigned id=1;id<HMBTN_VILLAGERS;++id) {
        const bool single=v->event_actor && id==v->event_actor_id;
        const bool scripted=single || v->event_actors[id];
        const HmbtnIntroActor *actor=single ? v->event_actor : v->event_actors[id] ? v->event_actors[id] : &v->people[id].actor;
        if(actor->visible && (scripted || v->people[id].delay<30) && approaching(from,to,actor->position,200)) return true;
    }
    return false;
}
unsigned hmbtn_village_target(const HmbtnVillage *v,const HmbtnIntroActor *p)
{
    if(!v || !p || p->facing>3) return 0;
    int32_t ahead[3];memcpy(ahead,p->position,sizeof(ahead));
    ahead[p->facing&1u ? 0 : 2]+=p->facing<2 ? -128 : 128;
    for(unsigned id=1;id<HMBTN_VILLAGERS;++id) {
        const HmbtnIntroActor *actor=v->event_actor && id==v->event_actor_id ? v->event_actor
            : v->event_actors[id] ? v->event_actors[id] : &v->people[id].actor;
        if(id!=27 && id!=30 && id!=33 && actor->visible && inside(ahead,actor->position,200)) return id;
    }
    return 0;
}
unsigned hmbtn_village_zone_target(const HmbtnVillage *v,const uint8_t *section,size_t size,const HmbtnIntroActor *player)
{
    if(!v || !section || size<16+50*44 || !player || player->facing>3) return 0;
    int32_t point[3];memcpy(point,player->position,sizeof(point));
    point[player->facing&1u ? 0 : 2]+=player->facing<2 ? -224 : 224;
    for(unsigned i=0;i<50;++i) {
        const uint8_t *p=section+16+i*44;
        const unsigned id=hmbtn_read_le16(p+4),facing=hmbtn_read_le16(p+8);
        if(!(hmbtn_read_le16(p)&0x4000u) || hmbtn_read_le16(p+2)!=36 || !id || id>=HMBTN_VILLAGERS
                || (facing!=5 && facing!=player->facing)) continue;
        if(point[0]<=half(p+14) || point[0]>=half(p+20) || point[2]<=half(p+18) || point[2]>=half(p+24)
                || point[1]<=half(p+16)-128 || point[1]>=half(p+22)+120) continue;
        const HmbtnIntroActor *actor=v->event_actors[id] ? v->event_actors[id] : &v->people[id].actor;
        const int32_t target[3]={half(p+26),half(p+28),half(p+30)};
        if(actor->visible && inside(target,actor->position,200)) return id;
    }
    return 0;
}
/* family_flags_update: consume yesterday's flags before rebuilding the agenda.
 * The session has no spouse; family_daily_update remains a separate boundary. */
static void daily_affinity(HmbtnVillage *v)
{
    for(unsigned id=0;id<HMBTN_VILLAGERS;++id) {
        HmbtnVillager *p=&v->people[id];
        int affection=(int)p->affection,friendship=(int)p->friendship;
        if(p->talk_flags&HMBTN_VILLAGER_DAILY_GAIN) {
            if(girl(id)) affection+=200;else ++friendship;
        }
        switch(id) {
        case 25:case 29:case 30:case 31:case 32:case 35:break;
        default:
            if(!(p->talk_flags&(HMBTN_VILLAGER_TALKED_TODAY|HMBTN_VILLAGER_DAILY_GAIN))
                    && hmbtn_psx_random_range(&v->random,10)==0) {
                if(girl(id)) affection-=200;else --friendship;
            }
            break;
        }
        p->affection=(unsigned)(affection<0 ? 0 : affection>65535 ? 65535 : affection);
        p->friendship=(unsigned)(friendship<0 ? 0 : friendship>255 ? 255 : friendship);
        p->talk_flags &= (uint16_t)~(HMBTN_VILLAGER_TALKED_TODAY|HMBTN_VILLAGER_DAILY_GAIN);
    }
    static const unsigned girls[]={3,7,9,14,20};
    static const unsigned flags[]={EVENT_FLAG_KAREN_AFFECTION_1500,EVENT_FLAG_ANN_AFFECTION_1500,
        EVENT_FLAG_ELLI_AFFECTION_1500,EVENT_FLAG_MARY_AFFECTION_1500,EVENT_FLAG_POPURI_AFFECTION_1500};
    for(unsigned i=0;i<5;++i) if(v->people[girls[i]].affection>1500) v->flags[flags[i]]=true;
}
void hmbtn_village_clock(HmbtnVillage *v,const HmbtnFarmClock *clock,unsigned changes,
    unsigned weather,bool wedding_offer_pending)
{
    if(changes&HMBTN_CLOCK_DAY) {
        daily_affinity(v);
        hmbtn_event_days_advance(&v->days,clock,weather,wedding_offer_pending);
    }
    const bool reset=(changes&HMBTN_CLOCK_DAY) || ((changes&HMBTN_CLOCK_HOUR)
        && (clock->hour==12 || clock->hour==18));
    for(unsigned id=1;id<HMBTN_VILLAGERS;++id) {
        HmbtnVillager *p=&v->people[id];
        if(reset) { p->delay=0;p->blocked_by_player=false; }
        else if((changes&HMBTN_CLOCK_MINUTE) && !v->player_running && p->actor.visible
                && p->blocked_by_player && p->delay<30) ++p->delay;
    }
}
void hmbtn_village_enter(HmbtnVillage *v)
{
    for(unsigned i=0;i<HMBTN_VILLAGERS;++i) { v->people[i].placed=false;v->people[i].actor.visible=false;v->people[i].blocked_by_player=false; }
}
static const uint8_t *cell(const uint8_t *section,size_t size,unsigned row,unsigned column)
{
    const size_t at=0x1000+(row*17+column)*12;
    return column<17 && at<=size && at+12<=0x2000 && 12<=size-at ? section+at : NULL;
}
static void stand(HmbtnVillager *p)
{
    p->standing=true;p->actor.sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->actor.facing,0};
}
static bool place(HmbtnVillager *p,const uint8_t *section,size_t size)
{
    const uint8_t *start=cell(section,size,p->row,0);
    if(!start) { p->actor.visible=false;return false; }
    for(unsigned j=0;j<3;++j) p->actor.position[j]=half(start+6+j*2);
    p->actor.facing=hmbtn_read_le16(start+4)&3u;
    p->actor.sprite=(HmbtnAnimationPlayer){0,(uint16_t)p->actor.facing,0};
    p->column=1;p->wait=hmbtn_read_le16(start)==5 ? hmbtn_read_le16(start+2) : 0;
    p->standing=false;p->placed=true;p->cell_entered=false;return true;
}
static void route_frame(HmbtnVillage *v,HmbtnVillager *p,const uint8_t *section,size_t size,bool resume)
{
    HmbtnIntroActor *a=&p->actor;
    if(p->standing || !a->visible) return;
    /* Bound control-cell chains as well as jumps, even for malformed data. */
    for(unsigned controls=0;controls<17;++controls) {
        const uint8_t *target=cell(section,size,p->row,p->column);
        if(!target) { stand(p);return; }
        const unsigned kind=hmbtn_read_le16(target),param=hmbtn_read_le16(target+2);
        const uint8_t *previous=p->column ? target-12 : target;
        a->facing=hmbtn_read_le16(previous+4)&3u;
        if(kind==0 || kind==65535) { stand(p);return; }
        if(kind==4) {
            if(p->step==29) { stand(p);return; }
            ++p->step;p->schedule[p->step][0]=(uint16_t)(v->minute%1440);
            p->location=p->schedule[p->step][3];p->row=p->schedule[p->step][1]&255u;
            a->visible=p->location==hmbtn_read_le16(section+2) && p->location!=54;
            if(!a->visible) {
                p->schedule[p->step][0]=(uint16_t)((v->minute+1440-p->delay)%1440);
                if(hmbtn_farm_clock_location_held(p->location)) p->delay=0;
            }
            p->placed=false;
            if(a->visible) place(p,section,size);
            return;
        }
        if(kind==0x8000) {
            target=cell(section,size,p->row,param);
            if(!target) { stand(p);return; }
            p->column=param;p->cell_entered=false;
            if(hmbtn_read_le16(target)==2 || hmbtn_read_le16(target)==5) {
                for(unsigned j=0;j<3;++j) a->position[j]=half(target+6+j*2);
                ++p->column;
            }
            continue;
        }
        /* Ordinary door cells resume at the next point. Their scripted door
         * animations, greetings and story cues are not executed by this module. */
        if(kind==3 || kind==9 || kind==0x11 || kind==0x12) { ++p->column;p->cell_entered=false;continue; }
        if(kind==0x10) {
            target=cell(section,size,p->row,++p->column);
            if(!target) { stand(p);return; }
            for(unsigned j=0;j<3;++j) a->position[j]=half(target+6+j*2);
            p->cell_entered=false;continue;
        }
        if(!p->cell_entered) {
            if(!resume && (kind==1 || kind==6)) p->wait=param;
            p->cell_entered=true;
        }
        if(p->wait && !resume) {
            --p->wait;a->sprite=(HmbtnAnimationPlayer){0,(uint16_t)a->facing,0};return;
        }
        const int dx=half(target+6)-a->position[0],dz=half(target+10)-a->position[2];
        const double distance=sqrt((double)dx*dx+(double)dz*dz);
        const int speed=(hmbtn_read_le16(section+8)&0x1000u) ? 16 : 48;
        if(a->sprite.animation!=a->facing+1) a->sprite=(HmbtnAnimationPlayer){(uint16_t)(a->facing+1),0,0};
        int32_t next[3]={a->position[0],a->position[1],a->position[2]};
        next[0]+=distance<=speed ? dx : (int)(dx*speed/distance);
        next[2]+=distance<=speed ? dz : (int)(dz*speed/distance);
        if(!resume && v->player && !v->player_running && p->delay<30 && approaching(a->position,next,v->player->position,218)) {
            p->blocked_by_player=true;
            a->sprite=(HmbtnAnimationPlayer){0,(uint16_t)a->facing,0};return;
        }
        if(distance<=speed) {
            a->position[0]=half(target+6);a->position[2]=half(target+10);
            ++p->column;p->cell_entered=false;
        } else {
            a->position[0]+=(int)(dx*speed/distance);a->position[2]+=(int)(dz*speed/distance);
        }
        return;
    }
    stand(p);
}
void hmbtn_village_tick(HmbtnVillage *v,const uint8_t *section,size_t size,const HmbtnModel *ground,bool held)
{
    if(!section || size<0x2000) return;
    for(unsigned id=1;id<37;++id) {
        HmbtnVillager *p=&v->people[id];HmbtnIntroActor *a=&p->actor;
        if(!held) p->blocked_by_player=false;
        a->visible=p->location==hmbtn_read_le16(section+2) && p->location!=54;
        if(!a->visible) continue;
        if(!p->placed) {
            if(!place(p,section,size)) continue;
            unsigned start=p->schedule[p->step][0];if(start<360) start+=1440;
            unsigned elapsed=v->minute>=start ? v->minute-start : 0;
            elapsed=elapsed>=p->delay ? elapsed-p->delay : 0;
            if(elapsed>1440) elapsed=1440;
            const uint8_t *entry=cell(section,size,p->row,0);
            if(hmbtn_read_le16(entry)!=5) for(unsigned t=0;t<elapsed*15 && !p->standing && a->visible;++t)
                route_frame(v,p,section,size,true);
        }
        if(!held) route_frame(v,p,section,size,false);
        HmbtnGroundHit hit;if(a->visible && hmbtn_ground_find(ground,a->position,0xC000,200,&hit)) a->position[1]=hit.height;
    }
}
static int outside(unsigned hour,unsigned start,unsigned stop) { return hour<start || hour>=stop; }
static int condition(HmbtnVillage *v,unsigned id,const HmbtnFarmClock *c)
{
    switch(id) {
    case 0:return flag(v,EVENT_FLAG_MARRIED_POPURI)||flag(v,EVENT_FLAG_POPURI_KAI_MARRIED);
    case 1:return flag(v,EVENT_FLAG_KAREN_RICK_WEDDING_TODAY)||flag(v,EVENT_FLAG_POPURI_KAI_WEDDING_TODAY);
    case 2:return flag(v,EVENT_FLAG_184)&&!flag(v,EVENT_FLAG_185)&&!flag(v,EVENT_FLAG_186);
    case 3:return flag(v,EVENT_FLAG_ANN_CLIFF_WEDDING_TODAY);
    case 4:return flag(v,EVENT_FLAG_KAREN_RICK_WEDDING_TODAY);
    case 5:return c->weekday==0;
    case 6:return flag(v,EVENT_FLAG_MARRIED_POPURI);
    case 7:return c->season==2 && c->day==4;
    case 8:return flag(v,EVENT_FLAG_DOG_PLACED)&&v->dog_held;
    case 9:case 10:case 11:case 12:case 13:
        return c->season==3 && c->day==23 && flag(v,EVENT_FLAG_SINGLE) && !flag(v,EVENT_FLAG_KAREN_RICK_MARRIED+id-9);
    case 14:return c->season==2 && c->day==2;
    case 15:return c->season==0 && c->day==0;
    case 16:return flag(v,EVENT_FLAG_MARRIED_KAREN);
    case 17:return flag(v,EVENT_FLAG_MARRIED_ANN);
    case 18:return flag(v,EVENT_FLAG_MARRIED_MARY);
    case 19:return c->season==v->ann_birthday_season && c->day==v->ann_birthday_day && c->hour<(v->invitation_available ? 10u : 18u);
    case 20:return flag(v,EVENT_FLAG_19B) && c->weekday!=1 && c->weekday!=3;
    case 21:return flag(v,EVENT_FLAG_POPURI_KAI_WEDDING_TODAY);
    default:return 0;
    }
}
static int rule(HmbtnVillage *v,unsigned command,unsigned operand,const HmbtnFarmClock *c,int *pending)
{
    const unsigned id=operand>>8,mode=operand&255u;
    if(command==0) return id<37 && v->people[id].location!=mode;
    if(command==1 && id<37) {
        const unsigned grade=girl(id) ? v->people[id].affection : v->people[id].friendship;
        const unsigned low=girl(id) ? 10000u : 100u,high=girl(id) ? 30000u : 200u;
        switch(mode) {
        case 0:return outside(c->hour,grade<=low ? 10u : grade<=high ? 8u : 6u,grade<=low ? 18u : grade<=high ? 20u : 22u);
        case 1:return grade<=low;
        case 2:return grade<=high;
        case 3:return grade<=low || outside(c->hour,18,24);
        default:return 0;
        }
    }
    if(command==2) {
        static const unsigned windows[9][2]={{9,12},{9,16},{9,17},{9,16},{13,16},{9,18},{8,21},{18,24},{11,17}};
        return operand<9 ? outside(c->hour,windows[operand][0],windows[operand][1]) : 0;
    }
    if(command==3) { int result=condition(v,operand,c);if(result) *pending=(int)operand;return result; }
    return 0;
}
int hmbtn_village_door(HmbtnVillage *v,unsigned event,const HmbtnFarmClock *c)
{
    const HmbtnDoorScript *script=NULL;
    for(unsigned i=0;i<sizeof(door_scripts)/sizeof(*door_scripts);++i) if(door_scripts[i].id==event) script=&door_scripts[i];
    if(!script) return -1;
    if(flag(v,EVENT_FLAG_DATED_EVENT_TODAY)) {
        bool exception=(c->season==0 && c->day==0 && (event==34 || event==37 || event==40 || event==41))
            || (c->season==2 && c->day==2 && event==32)
            || (c->season==3 && c->day==23 && (event==6 || event==7 || event==19 || event==20 || event==24 || event==25 || event==27));
        if(!exception) return EVENT_RESULT_DEFAULT;
    }
    int pending=-1;
    for(unsigned i=0;i<script->count;++i) {
        const HmbtnDoorRule *r=&script->rules[i];if(pending!=-1 && (unsigned)pending!=r->x) continue;
        const int a=rule(v,r->a,r->x,c,&pending),b=rule(v,r->b,r->y,c,&pending);
        if(a==1 && (r->b==255 || b==a)) return (int)r->result;
    }
    return -1;
}
bool hmbtn_village_pack(unsigned row,uint32_t *offset,size_t *size,const HmbtnVillagePack **toc)
{
    if(row>=35 || !offset || !size || !toc) return false;
    *offset=villager_pack_offsets[row];*size=villager_pack_offsets[row+1]-*offset;*toc=&village_packs[row];return true;
}
