/* event_commands.c: comparisons deliberately return 1 when the rule FAILS. */
#include "village_talk.h"
#include "item_reaction.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "village_talk_tables.inc"
static bool slot(int i) { return i>=0 && i<32; }
static bool person(int i) { return i>0 && i<HMBTN_VILLAGERS; }
static int32_t arithmetic(int32_t a,int32_t b,int op)
{
    switch(op) {
    case 0:return b;
    case 1:return (int32_t)((uint32_t)a+(uint32_t)b);
    case 2:return (int32_t)((uint32_t)a-(uint32_t)b);
    case 3:return (int32_t)((uint32_t)a*(uint32_t)b);
    case 4:return !b || (a==INT32_MIN && b==-1) ? a : a/b;
    case 5:return !b || (a==INT32_MIN && b==-1) ? 0 : a%b;
    default:return a; /* Original switch leaves the value unchanged. */
    }
}
static int compare(int32_t a,int32_t b,int op)
{
    switch(op) { case 0:return a>=b;case 1:return a>b;case 2:return a<=b;
    case 3:return a<b;case 4:return a==b;default:return 1; }
}
static bool value(HmbtnVillageTalk *t,int var,int element,int32_t *out)
{
    HmbtnVillage *v=t->village;const HmbtnFarmClock *c=t->clock;
    HmbtnVillager *p=person(element) ? &v->people[element] : NULL;
    if(var>=49 && var-48<HMBTN_VILLAGE_FLAGS) { *out=v->flags[var-48];return true; }
    switch(var) {
    case 3:*out=(int32_t)t->registration->season;break;
    case 4:*out=(int32_t)t->registration->day;break;
    case 10:*out=(int32_t)c->hour;break;
    case 11:*out=(int32_t)c->weekday;break;
    case 12:*out=(int32_t)c->year+1;break;
    case 13:*out=(int32_t)c->season;break;
    case 14:*out=(int32_t)c->day+1;break;
    case 15:*out=(int32_t)v->people[t->target].location;break;
    /* Family and weather remain at their actual new-game initial values. */
    case 16:case 17:case 18:case 19:*out=0;break;
    case 20:*out=-1;break;
    case 21:case 22:case 23:*out=t->player->position[var-21];break;
    case 25:*out=(int32_t)t->player->facing;break;
    case 26:*out=(int32_t)c->minute;break;
    case 27:if(!p) return false;*out=(int32_t)p->actor.facing;break;
    case 31:if(!p) return false;*out=(int32_t)p->affection;break;
    case 32:if(!p) return false;*out=(int32_t)p->friendship;break;
    case 38:if(!p) return false;*out=(int32_t)p->schedule[p->step][2];break;
    case 40:if(element<0 || element>=128) return false;*out=v->days.count0[element];break;
    case 41:if(element<0 || element>=8) return false;*out=v->days.count1[element];break;
    case 42:if(element<0 || element>=128) return false;*out=v->days.values[element];break;
    case 44:case 45:case 46:
        if(!p) return false;
        *out=(p->talk_flags&(var==44 ? HMBTN_VILLAGER_DAILY_GAIN : var==45 ? HMBTN_VILLAGER_TALKED_TODAY : 0x10u))!=0;break;
    default:return false;
    }
    return true;
}
static bool update(HmbtnVillageTalk *t,int var,int element,int32_t operand,int op)
{
    int32_t previous;
    if(!value(t,var,element,&previous)) return false;
    if(var>=49) { t->village->flags[var-48]=operand!=0;return true; }
    HmbtnVillager *p=person(element) ? &t->village->people[element] : NULL;
    if(var>=44 && var<=46 && p) {
        const uint16_t mask=var==44 ? HMBTN_VILLAGER_DAILY_GAIN : var==45 ? HMBTN_VILLAGER_TALKED_TODAY : 0x10;
        if(operand) p->talk_flags |= mask;else p->talk_flags &= (uint16_t)~mask;
        return true;
    }
    if(var>=40 && var<=42) {
        int32_t result=arithmetic(previous,operand,op);
        if(result<0) result=0;
        if(result>255) result=255;
        uint8_t *cell=var==40 ? &t->village->days.count0[element]
            : var==41 ? &t->village->days.count1[element] : &t->village->days.values[element];
        *cell=(uint8_t)result;return true;
    }
    if((var==31 || var==32) && p) {
        int32_t result=arithmetic(previous,operand,op),limit=var==31 ? 65535 : 255;
        if(result<0) result=0;
        if(result>limit) result=limit;
        if(var==31) p->affection=(unsigned)result;else p->friendship=(unsigned)result;
        return true;
    }
    return false;
}
static void birthday(const HmbtnVillageTalk *t,unsigned id,int *season,int *day)
{
    *season=*day=0;
    for(unsigned i=0;i<28;++i) if((unsigned)birthdays[i][0]==id) {
        *season=birthdays[i][1];*day=birthdays[i][2];return;
    }
    for(unsigned i=0;i<5;++i) if((unsigned)bride_birthdays[0][i][0]==id) {
        const unsigned alternate=t->registration->season==(unsigned)bride_birthdays[0][i][1]
            && t->registration->day==(unsigned)bride_birthdays[0][i][2];
        *season=bride_birthdays[alternate][i][1];*day=bride_birthdays[alternate][i][2];return;
    }
}
static HmbtnEventResult command(void *context,const HmbtnEventCommand *c,char *error,size_t size)
{
    HmbtnVillageTalk *t=context;int32_t a=0,b=0;int season,day;
    const int v=c->command,e=c->operand,x=c->x,y=c->y;
    bool done=true;
    switch(c->type) {
    case 2:
        if(!t->pending) {
            if(v<0 || v>4 || (y && !slot(e))) goto invalid;
            t->bank=(unsigned)v;t->request=(unsigned)(y ? t->slots[e] : e);
            t->message_pc=t->script.pc;t->pending=true;t->loaded=t->message_done=false;
        }
        if(!t->message_done) return HMBTN_EVENT_PENDING;
        if(x!=-1) { if(!slot(x)) goto invalid;t->slots[x]=(int32_t)t->result; }
        t->pending=false;break;
    case 12: {
        if(v!=(int)t->target || e || x || y) goto boundary;
        HmbtnIntroActor *npc=&t->village->people[v].actor;
        int dx=t->player->position[0]-npc->position[0],dz=t->player->position[2]-npc->position[2];
        npc->facing=abs(dx)>abs(dz) ? (dx<0 ? 1u : 3u) : (dz<0 ? 0u : 2u);
        npc->sprite=(HmbtnAnimationPlayer){0,(uint16_t)npc->facing,0};break;
    }
    case 13:case 14:
        if(!value(t,v,e,&a) || (c->type==14 && !slot(y))) goto boundary;
        b=c->type==14 ? t->slots[y] : y;
        done=v==20 && b!=-1 ? false : compare(a,b,x);break;
    case 15:case 16:
        if(!slot(v) || (c->type==16 && !slot(e))) goto invalid;
        done=compare(t->slots[v],c->type==16 ? t->slots[e] : y,x);break;
    case 17:if(!value(t,v,e,&a)) goto boundary;done=a>=x && a<=y;break;
    case 18:
        if(!slot(e) || ((v&1) && !slot(x)) || ((v&2) && !slot(y))) goto invalid;
        done=t->slots[e]>=(v&1 ? t->slots[x] : x) && t->slots[e]<=(v&2 ? t->slots[y] : y);break;
    case 19:case 20:
        if(c->type==20 && !slot(x)) goto invalid;
        if(!update(t,v,e,c->type==20 ? t->slots[x] : x,y)) goto boundary;
        break;
    case 21:
        if(!slot(v) || (e && !slot(x))) goto invalid;
        t->slots[v]=arithmetic(t->slots[v],e ? t->slots[x] : x,y);break;
    case 22:
        if(!slot(x) || !value(t,v,e,&a)) goto boundary;
        t->slots[x]=arithmetic(t->slots[x],a,y);break;
    case 29:case 30: {
        if(!t->inventory) goto invalid;
        int item=c->type==29 ? 0 : v,where=c->type==29 ? v : e,which=c->type==29 ? e : x;
        if(c->type==30 && (y&2)) {
            if(!slot(item)) goto invalid;
            item=(int)((uint32_t)t->slots[item]&0xFFFFu);
            if(item>=32768) item-=65536;
        }
        done=false;
        if((where!=0 && where!=1) || which<0 || which>2) goto boundary;
        for(int kind=0;kind<=which;++kind) {
            HmbtnStoredTool *items=NULL;unsigned count=0;
            if(kind==0) { items=where ? &t->inventory->held_item : &t->inventory->equipped;count=1; }
            if(kind==1) { items=where ? t->inventory->items : t->inventory->bag;
                count=t->inventory->rucksack_level<=2 ? 2u<<t->inventory->rucksack_level : 0; }
            if(kind==2 && where==0) { items=t->inventory->toolbox;count=64; }
            if(kind==2 && where==1) goto boundary; /* Fridge not represented in this session. */
            for(unsigned i=0;i<count;++i) if(items[i].id==item) {
                done=true;
                if(c->type==30 && (y&1)) items[i]=(HmbtnStoredTool){0};
            }
        }
        break;
    }
    case 32: { /* event_item_store: equipped, rucksack, then optional toolbox. */
        if(t->inventory->rucksack_level>2 || (e && !slot(v))) goto invalid;
        int item=e ? (int)((uint32_t)t->slots[v]&0xFFFFu) : v;
        if(item>=32768) item-=65536; /* script_vars[item].half */
        if(item<=0 || item>=380) goto boundary;
        HmbtnStoredTool *destination=NULL;bool spilled=false;
        if(!t->inventory->equipped.id) destination=&t->inventory->equipped;
        const unsigned capacity=2u<<t->inventory->rucksack_level;
        for(unsigned i=0;!destination && i<capacity;++i)
            if(!t->inventory->bag[i].id) destination=&t->inventory->bag[i];
        if(!x) for(unsigned i=0;!destination && i<64;++i)
            if(!t->inventory->toolbox[i].id) { destination=&t->inventory->toolbox[i];spilled=true; }
        done=destination!=NULL;
        if(destination) {
            *destination=(HmbtnStoredTool){(uint16_t)item,(uint16_t)(item<=25 ? (item-1)%5 : 0),1};
            if(y && !spilled) { t->show_tool=(unsigned)item;t->show_frames=y==1 ? 11 : 61; }
        }
        break;
    }
    case 35: /* Bind the event actor; the world route is retained for handback. */
        if(v!=(int)t->target) goto boundary;
        break;
    case 37:
        if(v || e || x || y) goto boundary;
        t->script.finished=true;break;
    case 57: {
        if(!t->inventory->held_item.id) { done=false;break; }
        if(!slot(e) || (x && !slot(v))) goto invalid;
        const int id=x ? t->slots[v] : v;
        if(!person(id)) goto invalid;
        const HmbtnStoredTool *gift=&t->inventory->held_item;
        const unsigned item=gift->id==164 && gift->modifier ? gift->modifier : gift->id;
        const int reaction=hmbtn_item_villager_reaction(item,(unsigned)id);
        t->slots[e]=reaction>=0 && reaction<7 ? reaction+1 : 0;
        break;
    }
    case 58: {
        if(e!=0 && e!=1) goto invalid;
        const HmbtnStoredTool *entry=e ? &t->inventory->held_item : &t->inventory->equipped;
        const unsigned item=x && entry->id==164 ? entry->modifier : entry->id;
        done=item!=0;
        if(done && v!=-1) { if(!slot(v)) goto invalid;t->slots[v]=(int32_t)item; }
        break;
    }
    case 66:
        if(x<0 || x>=HMBTN_VILLAGE_FLAGS) goto invalid;
        if(v==0) t->village->flags[x]=e!=0;
        else done=t->village->flags[x];
        break;
    case 71:
        if(v!=(int)t->target || e) goto boundary;
        t->released=true;break;
    case 79:if(!slot(v)) goto invalid;t->slots[v]=(int32_t)(c->next_other==255 ? t->script.pc+1 : c->next_other);break;
    case 80:
        if(!slot(v) || t->slots[v]<0 || (unsigned)t->slots[v]>=t->script.count) goto invalid;
        t->script.pc=(unsigned)t->slots[v];return HMBTN_EVENT_JUMP;
    case 81:
        if(v==22) {
            if(t->inventory->event_count1<15) ++t->inventory->event_count1;
            break;
        }
        /* Only queries of initial-state systems; shops/story mutations are boundaries. */
        if(v==2 && e==1) { done=false;break; } /* No weddings in this session. */
        if(v==18 && x==0) { done=false;break; } /* No livestock spawn gate armed. */
        if(v==20 && e==0) { done=false;break; } /* No construction order. */
        if(v==29 && slot(e)) { t->slots[e]=0;done=compare(0,x,y);break; }
        goto boundary;
    case 94:
        if(e || !person(v)) goto boundary;
        birthday(t,(unsigned)v,&season,&day);
        if(x!=-1) { if(!slot(x)) goto invalid;t->slots[x]=season; }
        if(y!=-1) { if(!slot(y)) goto invalid;t->slots[y]=day; }
        done=t->clock->season==(unsigned)season && t->clock->day==(unsigned)day;break;
    case 95:
        /* The native session has no house/barn/greenhouse upgrades yet. */
        if(v!=-1) { if(!slot(v)) goto invalid;t->slots[v]=0; }
        if(e!=-1 || x!=-1 || y!=-1) goto boundary;
        break;
    default:goto boundary;
    }
    return done ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
boundary:
    t->boundary_type=c->type;return HMBTN_EVENT_BOUNDARY;
invalid:
    snprintf(error,size,"Invalid conversation operand at NPC %u step %u",t->target,t->script.pc);
    return HMBTN_EVENT_ERROR;
}
bool hmbtn_village_talk_range(unsigned id,uint32_t *offset,size_t *size)
{
    if(!id || id>=HMBTN_VILLAGERS || !offset || !size) return false;
    *offset=talk_ranges[id][0];*size=talk_ranges[id][1]-*offset;return *size>0 && *size<=8192;
}
bool hmbtn_village_talk_open(HmbtnVillageTalk *t,unsigned id,const uint8_t *data,size_t size,
    HmbtnVillage *v,const HmbtnIntroActor *player,const HmbtnFarmClock *clock,
    const HmbtnNewGame *registration,HmbtnHouseInteraction *inventory,char *error,size_t error_size)
{
    HmbtnEventBlock block;const uint8_t *code;size_t length;
    if(!t || !person((int)id) || !v || !player || !clock || !registration || !inventory || inventory->held_item.id>=380
            || !hmbtn_event_block_open(&block,data,size,error,error_size)
            || !hmbtn_event_block_entry(&block,block.script,&code,&length,NULL)) return false;
    if(length>sizeof(t->code)) { snprintf(error,error_size,"NPC conversation exceeds native script bound");return false; }
    memset(t,0,sizeof(*t));memcpy(t->code,code,length);
    if(!hmbtn_event_script_open(&t->script,t->code,length,error,error_size)) return false;
    memcpy(t->saved_flags,v->flags,sizeof(t->saved_flags));t->saved_days=v->days;
    t->saved_equipped=inventory->equipped;
    memcpy(t->saved_bag,inventory->bag,sizeof(t->saved_bag));
    memcpy(t->saved_toolbox,inventory->toolbox,sizeof(t->saved_toolbox));
    t->saved_held_item=inventory->held_item;
    t->saved_event_count1=inventory->event_count1;
    memcpy(t->saved_items,inventory->items,sizeof(t->saved_items));
    for(unsigned i=0;i<HMBTN_VILLAGERS;++i) {
        t->saved_people[i].flags=v->people[i].talk_flags;t->saved_people[i].affection=v->people[i].affection;
        t->saved_people[i].friendship=v->people[i].friendship;
    }
    t->target=id;t->saved_facing=v->people[id].actor.facing;t->village=v;t->player=player;
    t->clock=clock;t->registration=registration;t->inventory=inventory;t->active=true;return true;
}
bool hmbtn_village_talk_tick(HmbtnVillageTalk *t,char *error,size_t size)
{
    if(!t || !t->active) return true;
    if(!hmbtn_event_tick(&t->script,command,t,error,size)) return false;
    if(t->script.finished || t->script.boundary) {
        if(t->script.boundary) {
            /* An unported action must not claim story/item progress. */
            memcpy(t->village->flags,t->saved_flags,sizeof(t->saved_flags));t->village->days=t->saved_days;
            for(unsigned i=0;i<HMBTN_VILLAGERS;++i) {
                t->village->people[i].talk_flags=t->saved_people[i].flags;
                t->village->people[i].affection=t->saved_people[i].affection;
                t->village->people[i].friendship=t->saved_people[i].friendship;
            }
            t->inventory->equipped=t->saved_equipped;
            memcpy(t->inventory->bag,t->saved_bag,sizeof(t->saved_bag));
            memcpy(t->inventory->toolbox,t->saved_toolbox,sizeof(t->saved_toolbox));
            t->inventory->held_item=t->saved_held_item;
            t->inventory->event_count1=t->saved_event_count1;
            memcpy(t->inventory->items,t->saved_items,sizeof(t->saved_items));
            t->show_tool=t->show_frames=0;
        }
        HmbtnIntroActor *a=&t->village->people[t->target].actor;
        a->facing=t->saved_facing;a->sprite=(HmbtnAnimationPlayer){0,(uint16_t)a->facing,0};
        t->active=false;
    }
    return true;
}
