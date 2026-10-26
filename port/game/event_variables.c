/* Adapted from src/overlay/11_GF_FARM/event/event_commands.c (d3c0f38):
 * event_var_get/update/arith/compare and commands 13..22. Replace PS1 address/
 * stride/type punning with explicit native fields. Original supported getters,
 * clamping, bit rules and failed-comparison polarity are retained. Arithmetic
 * wraps explicitly at 32 bits; division overflow is defined, no MIPS dependency.
 * Family queries remain at their existing new-game values. */
#include "event_variables.h"
#include "village.h"
#include "game_state.h"
#include <stdio.h>
static bool person(int i) { return i>0 && i<HMBTN_VILLAGERS; }
static int32_t signed_word(uint32_t v)
{
    return v<=INT32_MAX ? (int32_t)v : -1-(int32_t)(UINT32_MAX-v);
}
int32_t hmbtn_event_arithmetic(int32_t a,int32_t b,int op)
{
    switch(op) {
    case 0:return b;
    case 1:return signed_word((uint32_t)a+(uint32_t)b);
    case 2:return signed_word((uint32_t)a-(uint32_t)b);
    case 3:return signed_word((uint32_t)a*(uint32_t)b);
    case 4:return !b || (a==INT32_MIN && b==-1) ? a : a/b;
    case 5:return !b ? a : (a==INT32_MIN && b==-1) ? 0 : a%b;
    default:return a; /* Original switch leaves the value unchanged. */
    }
}
int hmbtn_event_comparison(int32_t a,int32_t b,int op)
{
    switch(op) { case 0:return a>=b;case 1:return a>b;case 2:return a<=b;
    case 3:return a<b;case 4:return a==b;default:return 1; }
}
bool hmbtn_event_variable_get(const HmbtnEventVariables *t,int var,int element,int32_t *out)
{
    if(!t || !out) return false;
    HmbtnVillage *v=t->village;const HmbtnFarmClock *c=t->clock;
    HmbtnVillager *p=v && person(element) ? &v->people[element] : NULL;
    if(v && var>=49 && var-48<HMBTN_VILLAGE_FLAGS) { *out=v->flags[var-48];return true; }
    if((var==3 || var==4) && !t->registration) return false;
    if((var==10 || var==11 || var==12 || var==13 || var==14 || var==26) && !c) return false;
    if((var==15 || var==40 || var==41 || var==42) && !v) return false;
    if(((var>=21 && var<=23) || var==25) && !t->player) return false;
    switch(var) {
    case 3:*out=(int32_t)t->registration->birthday_season;break;
    case 4:*out=(int32_t)t->registration->birthday_day;break;
    case 10:*out=(int32_t)c->hour;break;
    case 11:*out=(int32_t)c->weekday;break;
    case 12:*out=(int32_t)c->year+1;break;
    case 13:*out=(int32_t)c->season;break;
    case 14:*out=(int32_t)c->day+1;break;
    case 15:if(!person((int)t->target)) return false;*out=(int32_t)v->people[t->target].location;break;
    /* Family remains at its actual new-game initial values. */
    case 16:case 17:case 18:*out=0;break;
    case 19:if(!v || !v->weather) return false;*out=(int32_t)v->weather->today;break;
    case 20:*out=-1;break;
    case 21:case 22:case 23:*out=t->player->position[var-21];break;
    case 25:*out=(int32_t)t->player->facing;break;
    case 26:*out=(int32_t)c->minute;break;
    case 27:if(!p) return false;*out=(int32_t)hmbtn_village_actor(v,(unsigned)element)->facing;break;
    case 31:if(!p) return false;*out=(int32_t)p->affection;break;
    case 32:if(!p) return false;*out=(int32_t)p->friendship;break;
    case 38:if(!p || p->step>=sizeof(p->schedule)/sizeof(*p->schedule)) return false;*out=(int32_t)p->schedule[p->step][2];break;
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
bool hmbtn_event_variable_update(HmbtnEventVariables *t,int var,int element,int32_t operand,int op)
{
    int32_t previous;
    if(!hmbtn_event_variable_get(t,var,element,&previous)) return false;
    if(var>=49) { t->village->flags[var-48]=((uint32_t)operand&0xFFFFu)!=0;return true; }
    HmbtnVillager *p=t->village && person(element) ? &t->village->people[element] : NULL;
    if(var>=44 && var<=46 && p) {
        const uint16_t mask=var==44 ? HMBTN_VILLAGER_DAILY_GAIN : var==45 ? HMBTN_VILLAGER_TALKED_TODAY : 0x10;
        if(operand) p->talk_flags |= mask;else p->talk_flags &= (uint16_t)~mask;
        return true;
    }
    if(var>=40 && var<=42) {
        int32_t result=hmbtn_event_arithmetic(previous,operand,op);
        if(result<0) result=0;
        if(result>255) result=255;
        uint8_t *cell=var==40 ? &t->village->days.count0[element]
            : var==41 ? &t->village->days.count1[element] : &t->village->days.values[element];
        *cell=(uint8_t)result;return true;
    }
    if((var==31 || var==32) && p) {
        int32_t result=hmbtn_event_arithmetic(previous,operand,op),limit=var==31 ? 65535 : 255;
        if(result<0) result=0;
        if(result>limit) result=limit;
        if(var==31) p->affection=(unsigned)result;else p->friendship=(unsigned)result;
        return true;
    }
    return false;
}
static bool slot(const HmbtnEventVariables *t,int i)
{
    return t && t->slots && i>=0 && (size_t)i<t->slot_count;
}
HmbtnEventResult hmbtn_event_variables_command(HmbtnEventVariables *t,
    const HmbtnEventCommand *c,char *error,size_t size)
{
    if(!t || !c) goto invalid;
    const int v=c->command,e=c->operand,x=c->x,y=c->y;
    int32_t a=0,b=0;bool done=true;
    switch(c->type) {
    case 13:case 14:
        if(c->type==14 && !slot(t,y)) goto invalid;
        if(!hmbtn_event_variable_get(t,v,e,&a)) goto boundary;
        b=c->type==14 ? t->slots[y] : y;
        done=c->type==13 && v==20 && b!=-1 ? false : hmbtn_event_comparison(a,b,x);break;
    case 15:case 16:
        if(!slot(t,v) || (c->type==16 && !slot(t,e))) goto invalid;
        done=hmbtn_event_comparison(t->slots[v],c->type==16 ? t->slots[e] : y,x);break;
    case 17:if(!hmbtn_event_variable_get(t,v,e,&a)) goto boundary;done=a>=x && a<=y;break;
    case 18:
        if(!slot(t,e) || ((v&1) && !slot(t,x)) || ((v&2) && !slot(t,y))) goto invalid;
        done=t->slots[e]>=(v&1 ? t->slots[x] : x) && t->slots[e]<=(v&2 ? t->slots[y] : y);break;
    case 19:case 20:
        if(c->type==20 && !slot(t,x)) goto invalid;
        if(!hmbtn_event_variable_update(t,v,e,c->type==20 ? t->slots[x] : x,y)) goto boundary;
        break;
    case 21:
        if(!slot(t,v) || (e && !slot(t,x))) goto invalid;
        t->slots[v]=hmbtn_event_arithmetic(t->slots[v],e ? t->slots[x] : x,y);break;
    case 22:
        if(!slot(t,x)) goto invalid;
        if(!hmbtn_event_variable_get(t,v,e,&a)) goto boundary;
        t->slots[x]=hmbtn_event_arithmetic(t->slots[x],a,y);break;
    default:goto boundary;
    }
    return done ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
boundary:
    return HMBTN_EVENT_BOUNDARY;
invalid:
    snprintf(error,size,"Invalid event variable/slot operand");return HMBTN_EVENT_ERROR;
}
