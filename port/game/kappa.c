/* Installed event 197 owns the three-gift sequence. Adapt only the remaining
 * event_cmd_check_gift, wait_time and event_cue_issue commands; no second quest. */
#include "kappa.h"
#include "session.h"
#include "item_reaction.h"
#include <stdio.h>
#include <string.h>
bool hmbtn_kappa_bind(HmbtnSession *s,unsigned event,char *error,size_t size)
{
    if(event!=197) return true;
    HmbtnIntroActor *a=hmbtn_session_bind_event_actor(s,27,HMBTN_SESSION_ACTOR_VISIT,false);
    if(!a) { snprintf(error,size,"Cannot bind original Kappa actor 27");return false; }
    memset(a,0,sizeof(*a));return true;
}
HmbtnEventResult hmbtn_kappa_command(HmbtnSession *s,const HmbtnEventCommand *c,char *error,size_t size)
{
    if(s->visit.event!=197) return HMBTN_EVENT_BOUNDARY;
    const int a=c->command,b=c->operand,x=c->x,y=c->y;
    switch(c->type) {
    case 75:
        if(!a && b==39 && x==-1 && !y) {
            const unsigned gift=s->visit.gift;s->visit.gift=0;
            return gift && gift!=0x186 && hmbtn_item_villager_reaction(gift,39)==0 ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
        }
        break;
    case 31:
        if(a==294 && !b && !x && !y)
            return hmbtn_event_item_begin(&s->event_item,&s->intro.player,&s->game.inventory,294,true) ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
        break;
    case 36:
        if(a==-1 && !b && !x && !y)
            return s->game.clock.hour==6 && !s->game.clock.minute && (s->overnight_tick || s->intro.fade_level) ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
        break;
    default:return HMBTN_EVENT_BOUNDARY;
    }
    snprintf(error,size,"Invalid original Kappa command %u/%d/%d",c->type,a,b);return HMBTN_EVENT_ERROR;
}
