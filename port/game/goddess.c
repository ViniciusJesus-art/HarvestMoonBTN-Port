/* Adapted from event_commands.c: gift checks, nudge, cue_reset, pick_bride,
 * estate bits, door tests and queued events used by installed script 182.
 * The BIN bytecode owns dialogue, counts, rewards and branch ordering. */
#include "goddess.h"
#include "runtime.h"
#include "inventory.h"
#include "economy.h"
#include "psx_random.h"
#include "world_map.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
bool hmbtn_goddess_event(unsigned e)
{ return e==182 || e==185 || e==187 || e==189 || e==191 || e==193; }
bool hmbtn_goddess_bind(HmbtnSession *s,unsigned e,char *error,size_t size)
{
    if(!hmbtn_goddess_event(e)) return true;
    const unsigned bride=e==185 ? 20 : e==187 ? 7 : e==189 ? 9 : e==191 ? 3 : 14;
    const unsigned ids[2]={e==182 ? 33u : 27u,e==182 ? 24u : bride};
    for(unsigned i=0;i<2;++i) {
        HmbtnIntroActor *a=hmbtn_session_bind_event_actor(s,ids[i],HMBTN_SESSION_ACTOR_VISIT,false);
        if(!a) { snprintf(error,size,"Cannot bind original goddess actor %u",ids[i]);return false; }
        memset(a,0,sizeof(*a));
    }
    s->goddess=(HmbtnGoddess){0};s->visit.event=e;s->intro.music_mode=s->runtime.audio_song;
    return true;
}
static HmbtnEventResult song(HmbtnSession *s,unsigned id,char *error,size_t size)
{
    HmbtnSessionFrame *f=s->active_frame;
    /* Keep the farm effects bank when changing the event's music. */
    if(!f || !f->song || !f->song(f->context,id,true,error,size)) return HMBTN_EVENT_ERROR;
    s->intro.music_mode=id;s->intro.stop_music=false;s->intro.start_music=true;
    return HMBTN_EVENT_DONE;
}
HmbtnEventResult hmbtn_goddess_command(HmbtnSession *s,const HmbtnEventCommand *c,char *error,size_t size)
{
    const int a=c->command,b=c->operand,x=c->x,y=c->y;
    HmbtnFarmIntro *in=&s->intro;HmbtnSessionFrame *f=s->active_frame;
    if(!hmbtn_goddess_event(s->visit.event)) return HMBTN_EVENT_BOUNDARY;
    switch(c->type) {
    case 24:
        if((a==5 || a==6 || a==7 || a==8 || a==9) && !b && !y && (x==0 || x==-1)) return song(s,(unsigned)a,error,size);
        break;
    case 25:
        if(!a && !b && !x && !y && f) return song(s,hmbtn_world_song_weather(f->section,16,s->game.clock.season,s->game.clock.hour,s->game.weather.today),error,size);
        break;
    case 30:
        if(a==63 && b==1 && !x && y==1) return hmbtn_inventory_take(&s->game.inventory,a,b,x,1) ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
        break;
    case 31:
        if((a==103 || a==63) && !b && !x && !y)
            return (a==103 ? hmbtn_event_item_begin(&s->event_item,&in->player,&s->game.inventory,103,true)
                : hmbtn_event_item_show(&s->event_item,&in->player,&s->game.inventory,63)) ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
        break;
    case 40:case 41: {
        if((a!=46 && a!=47) || x || y || !f) break;
        for(unsigned i=0;i<50;++i) {
            const uint8_t *z=f->section+16+i*44;unsigned model=hmbtn_read_le16(z+12);
            if(hmbtn_read_le16(z+10)!=(unsigned)a || model>=HMBTN_SCENE_MODELS) continue;
            HmbtnDoorPhase phase=s->area_doors.models[model].phase;
            bool matching=s->area_doors.models[model].event==(unsigned)a;
            bool done=c->type==41 ? matching && phase!=HMBTN_DOOR_CLOSED : b==1 ? phase==HMBTN_DOOR_CLOSED : matching && b==2 && phase==HMBTN_DOOR_HELD;
            return done ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
        }
        break;
    }
    case 43:
        if(a!=41 || (b!=32 && b!=34) || x || y) break;
        if(!s->goddess.cue_active) { s->goddess.cue=(unsigned)b;s->goddess.cue_ticks=30;s->goddess.cue_active=true;return HMBTN_EVENT_WAIT; }
        if(s->goddess.cue_ticks) return HMBTN_EVENT_WAIT;
        s->goddess.cue_active=false;return HMBTN_EVENT_DONE;
    case 44:
        if(a || b || x!=1000 || y) break;
        hmbtn_economy_gold_change(&s->game.economy,1000,false);in->slots[0]=0;return HMBTN_EVENT_DONE;
    case 50:
        if(a!=10 || b!=2 || x!=1 || y || !hmbtn_goddess_event((unsigned)in->slots[10]) || in->slots[10]==182) break;
        in->queued_event=(unsigned)in->slots[10];in->event_pending=true;return HMBTN_EVENT_PENDING;
    case 69:
        if(a==1 && !b && x==0x20000000 && !y) return s->event_item.phase!=HMBTN_EVENT_ITEM_IDLE ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
        break;
    case 71:
        if(a!=42 || b!=2 || x || y) break;
        for(unsigned id=1;id<HMBTN_VILLAGERS;++id) {
            HmbtnIntroActor *actor=s->village.event_actors[id] ? hmbtn_farm_intro_actor(in,(int32_t)id) : NULL;
            if(actor) actor->visible=false;
        }
        return HMBTN_EVENT_DONE;
    case 81:
        if(a==6 && b==256 && !x && !y) { s->game.progress.goddess_berry=true;return HMBTN_EVENT_DONE; }
        if(a==2 && b==4 && x==9 && !y) { in->slots[4]=-1;in->slots[9]=-1;return HMBTN_EVENT_DONE; } /* Actual unmarried family state. */
        break;
    case 101: { /* Original candidate order; ties use the village's PS1 RNG. */
        if((a!=0 && a!=1) || b!=9 || x || y) break;
        const unsigned ids[5]={3,20,9,14,7};unsigned best=0,count=0,choices[5];
        for(unsigned i=0;i<5;++i) {
            if(i==4 && hmbtn_village_flag(&s->village,0x25)) {
                if(s->game.clock.season!=1 || (!a && (hmbtn_village_flag(&s->village,0xc9)
                        || s->village.days.count0[2]>=90))) continue;
            }
            if(!a && i<4 && hmbtn_village_flag(&s->village,0x21+i)) continue;
            const unsigned bond=s->village.people[ids[i]].affection;
            if(bond>best) { best=bond;count=0; }
            if(bond==best) choices[count++]=ids[i];
        }
        if(!count) return HMBTN_EVENT_WAIT;
        in->slots[9]=(int32_t)choices[hmbtn_psx_random_range(&s->village.random,count)];return HMBTN_EVENT_DONE;
    }
    default:return HMBTN_EVENT_BOUNDARY;
    }
    snprintf(error,size,"Invalid original goddess command %u/%d/%d",c->type,a,b);return HMBTN_EVENT_ERROR;
}
