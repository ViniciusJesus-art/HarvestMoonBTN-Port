/* event_triggers_fire and the Thomas visits, with serialized LE fields, no PS1 pointers. */
#include "farm_visit.h"
#include "psx_data.h"
#include "event_item.h"
#include "event_variables.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
bool hmbtn_farm_triggers_open(HmbtnFarmTriggers *t,const uint8_t *data,size_t bytes,char *error,size_t size)
{
    if(t) *t=(HmbtnFarmTriggers){0};
    if(t && data) for(unsigned i=0;i<HMBTN_FARM_TRIGGER_LIMIT && i*36u+2<=bytes;++i) {
        if(hmbtn_read_le16(data+i*36u)==65535) { *t=(HmbtnFarmTriggers){.data=data,.count=i};return true; }
        if(i*36u+36>bytes) break;
    }
    if(t && data && bytes>=HMBTN_FARM_TRIGGER_LIMIT*36u) {
        *t=(HmbtnFarmTriggers){.data=data,.count=HMBTN_FARM_TRIGGER_LIMIT};return true;
    }
    snprintf(error,size,"Truncated farm trigger table or missing sentinel");return false;
}
static bool flags_match(const HmbtnVillage *v,uint32_t packed,unsigned base,bool set,unsigned count)
{
    for(unsigned i=0;i<count;++i) {
        const unsigned id=(packed>>(i*10u))&1023u;
        if(id!=1023u && (base+id>=HMBTN_VILLAGE_FLAGS || hmbtn_village_flag(v,base+id)!=set)) return false;
    }
    return true;
}
static bool window_match(uint32_t packed,const HmbtnFarmClock *c)
{
    unsigned w[6];for(unsigned i=0;i<6;++i) w[i]=(packed>>(25-5*i))&31u;
    const unsigned first_year=w[0]==31 ? 0 : (w[0]+65535u)&65535u;
    const unsigned last_year=w[1]==31 ? 32766 : (w[1]+65535u)&65535u;
    if(c->year<first_year || c->year>last_year) return false;
    /* Preserve the original inclusive span and its unusual day wrap. */
    if(w[3]!=31) {
        bool found=false;const unsigned span=w[3]>=30 ? 29 : w[3];
        for(unsigned i=0;i<=span;++i) { unsigned day=((w[2]+65535u)&65535u)+i;if(day>=31) day-=29;found|=day==c->day; }
        if(!found) return false;
    }
    if(w[4]==31 || w[5]==31) return true;
    if(!w[5]) return c->hour==w[4] && !c->minute;
    const unsigned count=w[5]>24 ? 24 : w[5];
    for(unsigned i=0;i<count;++i) if((w[4]+i)%24==c->hour) return true;
    return false;
}
bool hmbtn_farm_visit_supported(unsigned event) { return event==18 || event==19 || (event>=128 && event<=133) || (event>=288 && event<=298) || event==140; }
static bool available(unsigned condition,const HmbtnVillage *v,const HmbtnFarmClock *c,const HmbtnInventory *inventory,const HmbtnFarmField *field)
{
    if(condition==255) return true;
    if(condition>=84 && condition<=92) return hmbtn_village_flag(v,0x67+condition-84);
    if(condition==7) return hmbtn_farm_field_has_grown(field,0x31);
    if(!inventory) return false;
    if(condition==39) return inventory->held_item.id==204;
    if(condition!=83) return false;
    /* farm_event_available(YEAR_CYCLE_ITEMS_3) uses the internal year, unlike
     * EVENT_VAR_YEAR in the script. Preserve the extra egg/milk variants too. */
    static const unsigned items[9]={65,71,67,77,0,0,162,68,151};
    int total=0;
    const unsigned cycle=c->year%9;
    if(cycle==4) {
        for(unsigned id=290;id<=293;++id) total+=hmbtn_event_items_count(inventory,(int)id,1,1);
        total+=hmbtn_event_items_count(inventory,232,1,1);
    }
    else if(cycle==5) {
        for(unsigned id=139;id<=141;++id) total+=hmbtn_event_items_count(inventory,(int)id,1,1);
        total+=hmbtn_event_items_count(inventory,227,1,1);
    } else total=hmbtn_event_items_count(inventory,(int)items[cycle],1,1);
    return total>=3;
}
unsigned hmbtn_farm_visit_trigger(const HmbtnFarmTriggers *t,const HmbtnVillage *v,
    const HmbtnFarmClock *c,unsigned weather,unsigned previous_place,unsigned mode)
{
    return hmbtn_farm_event_trigger(t,v,c,weather,previous_place,mode,0,NULL);
}
unsigned hmbtn_farm_event_trigger(const HmbtnFarmTriggers *t,const HmbtnVillage *v,
    const HmbtnFarmClock *c,unsigned weather,unsigned previous_place,unsigned mode,
    unsigned target,const HmbtnInventory *inventory)
{
    static const uint32_t weather_bits[5]={0x80000,0x2000,0x40000,0x1000,0x800};
    if(!t || !t->data || !v || !c || c->season>3 || c->weekday>6 || weather>4) return 0;
    for(unsigned i=0;i<t->count;++i) {
        const uint8_t *p=t->data+i*36;const unsigned raw=hmbtn_read_le16(p),id=raw&1023u;
        if(!hmbtn_farm_visit_supported(id)) continue;
        const uint32_t flags=hmbtn_read_le32(p+28);
        /* event_wedding_role_match: current native family is single. Accept
         * wildcard kind enabled for single; do not pretend other roles exist. */
        if((p[34]!=1 && p[34]!=mode) || (p[12]&7)!=7 || !(p[12]&0x40) || v->flags[251+id]) continue;
        if(p[34]==3 && p[35]!=target) continue;
        if(!available(p[33],v,c,inventory,t->field)) continue;
        if(!window_match(hmbtn_read_le32(p+8),c)) continue;
        const unsigned place=raw>>10;
        if(place!=63 && ((place==previous_place)==((flags&0x80000000u)!=0))) continue;
        bool range=true;
        if(p[2]!=255 && p[3]!=255) {
            if(p[2]!=40 || p[3]>=128) continue; /* Unknown variables fail closed. */
            const unsigned n=v->days.count0[p[3]],lo=hmbtn_read_le16(p+4),hi=hmbtn_read_le16(p+6);
            range=(lo==255 || n>=lo) && (hi==255 || n<=hi);
        }
        if(flags&0x400u) range=!range;
        if(!range) continue;
        if(p[13]!=255) {
            if(p[13]>=HMBTN_VILLAGERS) continue;
            const unsigned who=p[13];
            const bool bride=who==3 || who==7 || who==9 || who==14 || who==20;
            const unsigned bond=bride ? v->people[who].affection : v->people[who].friendship,amount=hmbtn_read_le16(p+14);
            if((flags&0x4000u) ? bond>amount : bond<amount) continue;
        }
        if(!flags_match(v,hmbtn_read_le32(p+16),251,true,3)
                || !flags_match(v,hmbtn_read_le32(p+20),251,false,3)) continue;
        const uint32_t prerequisites=hmbtn_read_le32(p+24);
        if(!flags_match(v,prerequisites&1023u,200,true,1)
                || !flags_match(v,(prerequisites>>10)&1023u,200,false,1)) continue;
        if(!(flags&(0x40000000u>>c->season)) || !(flags&(0x4000000u>>c->weekday))) continue;
        if((flags&0xC3800u)!=0xC3800u && !(flags&weather_bits[weather])) continue;
        if((flags&0x10000u) && (!inventory || inventory->held_item.id)) continue;
        if((flags&0x20000u) && (!inventory || hmbtn_event_items_count(inventory,0,0,1)<=0)) continue;
        return id;
    }
    return 0;
}
bool hmbtn_farm_visit_begin(HmbtnFarmVisit *visit,const HmbtnFarmTriggers *t,unsigned event,HmbtnVillage *v)
{
    if(!visit || !t || !t->data || !v || !hmbtn_farm_visit_supported(event)) return false;
    for(unsigned i=0;i<t->count;++i) {
        const uint8_t *p=t->data+i*36;
        if((hmbtn_read_le16(p)&1023u)!=event) continue;
        /* Request args[2] suppresses the done flag for repeatable visits. */
        if(!(hmbtn_read_le32(p+28)&0x8000u)) v->flags[251+event]=true;
        *visit=(HmbtnFarmVisit){.event=event,.active=true};return true;
    }
    return false;
}
static void face(HmbtnIntroActor *actor,const HmbtnIntroActor *player)
{
    const int32_t dx=player->position[0]-actor->position[0],dz=player->position[2]-actor->position[2];
    actor->facing=(unsigned)(abs(dx)>abs(dz) ? (dx<0 ? 1 : 3) : (dz<0 ? 0 : 2));
    actor->sprite=(HmbtnAnimationPlayer){0,(uint16_t)actor->facing,0};actor->visible=true;
}
HmbtnEventResult hmbtn_farm_visit_command(HmbtnFarmVisit *visit,HmbtnFarmIntro *s,
    HmbtnVillage *v,const HmbtnFarmClock *clock,const HmbtnEventCommand *c,char *error,size_t size)
{
    const int a=c->command,b=c->operand,x=c->x,y=c->y;
    if(!visit->active) goto unsupported;
    if(c->type>=13 && c->type<=22) {
        HmbtnEventVariables variables={v,clock,NULL,&s->player,0,s->slots,sizeof(s->slots)/sizeof(*s->slots)};
        return hmbtn_event_variables_command(&variables,c,error,size);
    }
    switch(c->type) {
    case 28:
        if((a!=0 && a!=1) || (b!=0 && b!=-1 && !(visit->event==18 && !a && b==2)) || x || y) break;
        s->event_active=a==0;v->flags[3]=s->event_active;
        if(b!=-1) { v->flags[4]=false;v->flags[5]=a==0;v->flags[0xB0]=a==0; }
        return a ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
    case 10:
        if(a<0 || a>1 || (b!=0 && b!=1) || x || y) break;
        visit->free=s->player_free=a!=0;return HMBTN_EVENT_DONE;
    case 11: {
        if((a!=-1 && (a<0 || a>=32)) || (b!=0 && b!=1) || (x!=0 && x!=1) || y) break;
        const bool talked=visit->talk;visit->talk=false;visit->free=s->player_free=!talked;
        if(talked) {
            const unsigned target=visit->event==130 || visit->event==289 || visit->event==18 || visit->event==19 ? visit->talk_target : 17;
            if(target>=HMBTN_VILLAGERS || !target) break;
            if(a!=-1) s->slots[a]=(int32_t)target;
            HmbtnIntroActor *actor=hmbtn_farm_intro_actor(s,(int32_t)target);
            if(visit->event==289 && target!=17) actor=&visit->actors[target];
            if(b && actor) face(actor,&s->player);
            if(x) v->people[target].talk_flags|=HMBTN_VILLAGER_DAILY_GAIN;
        }
        return talked ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
    }
    case 36: {
        if(a<0 || a>=32 || b<0 || b>=32 || x!=3 || y) break;
        int target=s->slots[a],now=(int)clock->hour;
        if(target<6) target+=24;
        if(now<6) now+=24;
        const bool reached=now>target || (now==target && (int)clock->minute>=s->slots[b]);
        if(reached) visit->talk=false;
        return reached ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
    }
    case 78:
        if(a!=17 || b || x || y) break;
        /* Hand back through the normal current-time route on completion. */
        v->people[17].placed=false;return HMBTN_EVENT_DONE;
    case 84:
        if(a!=2 || b || x || y) break;
        v->flags[4]=v->flags[5]=v->flags[0xB0]=false;return HMBTN_EVENT_DONE;
    case 91:
        if(a!=32 || b!=1 || !((x==-20 && y==-1000) || (x==20 && y==1000))) break;
        for(unsigned id=1;id<HMBTN_VILLAGERS;++id) if(id!=27 && id!=33) {
            HmbtnVillager *p=&v->people[id];
            const bool bride=id==3 || id==7 || id==9 || id==14 || id==20;
            const int n=(int)(bride ? p->affection : p->friendship)+(bride ? y : x),limit=bride ? 65535 : 255;
            if(bride) p->affection=(unsigned)(n<0 ? 0 : n>limit ? limit : n);
            else p->friendship=(unsigned)(n<0 ? 0 : n>limit ? limit : n);
        }
        return HMBTN_EVENT_DONE;
    }
unsupported:
    snprintf(error,size,"Unsupported farm visit command %u/%d/%d",c->type,a,b);return HMBTN_EVENT_ERROR;
}
