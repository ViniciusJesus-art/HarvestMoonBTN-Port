#include "event_variables.h"
#include "session.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Event variables line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static void instruction(uint8_t *p,unsigned type,unsigned yes,unsigned no,unsigned flags,int a,int b,int x,int y)
{
    memset(p,0,16);p[0]=(uint8_t)type;p[1]=(uint8_t)yes;p[2]=(uint8_t)no;p[3]=(uint8_t)flags;
    hmbtn_write_le16(p+4,(uint16_t)a);hmbtn_write_le16(p+6,(uint16_t)b);
    hmbtn_write_le32(p+8,(uint32_t)x);hmbtn_write_le32(p+12,(uint32_t)y);
}
static HmbtnEventResult visit(void *context,const HmbtnEventCommand *c,char *error,size_t size)
{
    HmbtnSession *s=context;
    return hmbtn_farm_visit_command(&s->visit,&s->intro,&s->village,&s->game.clock,c,error,size);
}
int main(void)
{
    HmbtnSession *s=hmbtn_session_create(NULL,0);char error[256]={0};CHECK(s);
    int32_t locals[64]={0},out=0;
    HmbtnEventVariables v={&s->village,&s->game.clock,hmbtn_game_registration(&s->game),&s->intro.player,
        21,locals,sizeof(locals)/sizeof(*locals)};
    CHECK(hmbtn_event_arithmetic(INT32_MAX,1,1)==INT32_MIN);
    CHECK(hmbtn_event_arithmetic(INT32_MIN,1,2)==INT32_MAX);
    CHECK(hmbtn_event_arithmetic(INT32_MAX,2,3)==-2);
    CHECK(hmbtn_event_arithmetic(12,0,4)==12 && hmbtn_event_arithmetic(12,0,5)==12);
    CHECK(hmbtn_event_arithmetic(INT32_MIN,-1,4)==INT32_MIN && hmbtn_event_arithmetic(INT32_MIN,-1,5)==0);
    CHECK(hmbtn_event_arithmetic(9,4,4)==2 && hmbtn_event_arithmetic(9,4,5)==1 && hmbtn_event_arithmetic(9,4,99)==9);
    const int less[]={0,0,1,1,0},equal[]={1,0,1,0,1},greater[]={1,1,0,0,0};
    for(int op=0;op<5;++op) CHECK(hmbtn_event_comparison(1,2,op)==less[op]
        && hmbtn_event_comparison(2,2,op)==equal[op] && hmbtn_event_comparison(3,2,op)==greater[op]);
    s->game.clock.year=5;s->game.clock.day=13;
    CHECK(hmbtn_event_variable_get(&v,12,0,&out) && out==6);
    CHECK(hmbtn_event_variable_get(&v,14,0,&out) && out==14);
    CHECK(hmbtn_event_variable_update(&v,253,0,1,0) && s->village.flags[205]);
    CHECK(hmbtn_event_variable_update(&v,253,0,65536,0) && !s->village.flags[205]);
    CHECK(hmbtn_event_variable_update(&v,32,21,300,0) && s->village.people[21].friendship==255);
    CHECK(hmbtn_event_variable_update(&v,32,21,-300,1) && !s->village.people[21].friendship);
    CHECK(hmbtn_event_variable_update(&v,31,21,70000,0) && s->village.people[21].affection==65535);
    CHECK(hmbtn_event_variable_update(&v,40,127,500,0) && s->village.days.count0[127]==255);
    CHECK(hmbtn_event_variable_update(&v,41,7,-1,0) && !s->village.days.count1[7]);
    CHECK(hmbtn_event_variable_update(&v,42,127,99,0) && s->village.days.values[127]==99);
    CHECK(hmbtn_event_variable_update(&v,44,21,1,0) && (s->village.people[21].talk_flags&HMBTN_VILLAGER_DAILY_GAIN));
    const HmbtnEventDays days=s->village.days;
    CHECK(!hmbtn_event_variable_update(&v,40,128,5,0) && !hmbtn_event_variable_update(&v,31,-1,7,0)
        && !hmbtn_event_variable_get(&v,9999,0,&out) && !memcmp(&days,&s->village.days,sizeof(days)));
    HmbtnEventCommand c={.type=21,.command=63,.x=7,.y=0};
    CHECK(hmbtn_event_variables_command(&v,&c,error,sizeof(error))==HMBTN_EVENT_DONE && locals[63]==7);
    c.command=64;CHECK(hmbtn_event_variables_command(&v,&c,error,sizeof(error))==HMBTN_EVENT_ERROR && locals[63]==7);
    c=(HmbtnEventCommand){.type=20,.command=42,.operand=2,.x=63,.y=0};
    CHECK(hmbtn_event_variables_command(&v,&c,error,sizeof(error))==HMBTN_EVENT_DONE && s->village.days.values[2]==7);
    c=(HmbtnEventCommand){.type=22,.command=42,.operand=2,.x=62,.y=0};
    CHECK(hmbtn_event_variables_command(&v,&c,error,sizeof(error))==HMBTN_EVENT_DONE && locals[62]==7);
    c=(HmbtnEventCommand){.type=16,.command=62,.operand=63,.x=4};
    CHECK(hmbtn_event_variables_command(&v,&c,error,sizeof(error))==HMBTN_EVENT_DONE);
    c=(HmbtnEventCommand){.type=14,.command=42,.operand=2,.x=4,.y=63};
    CHECK(hmbtn_event_variables_command(&v,&c,error,sizeof(error))==HMBTN_EVENT_DONE);
    c.y=-1;CHECK(hmbtn_event_variables_command(&v,&c,error,sizeof(error))==HMBTN_EVENT_ERROR);
    c=(HmbtnEventCommand){.type=17,.command=42,.operand=2,.x=6,.y=8};
    CHECK(hmbtn_event_variables_command(&v,&c,error,sizeof(error))==HMBTN_EVENT_DONE);
    c=(HmbtnEventCommand){.type=18,.command=3,.operand=62,.x=63,.y=63};
    CHECK(hmbtn_event_variables_command(&v,&c,error,sizeof(error))==HMBTN_EVENT_DONE);
    c.x=64;CHECK(hmbtn_event_variables_command(&v,&c,error,sizeof(error))==HMBTN_EVENT_ERROR);
    c=(HmbtnEventCommand){.type=19,.command=9999,.x=5};
    CHECK(hmbtn_event_variables_command(&v,&c,error,sizeof(error))==HMBTN_EVENT_BOUNDARY);
    /* A borrowed context with only slots cannot claim unsupported world state. */
    HmbtnEventVariables no_world={.slots=locals,.slot_count=64};
    c.command=32;c.operand=21;
    CHECK(hmbtn_event_variables_command(&no_world,&c,error,sizeof(error))==HMBTN_EVENT_BOUNDARY);
    /* Visit adapter and conversation-domain context address the same fields. */
    s->visit.active=true;s->visit.event=140;
    c=(HmbtnEventCommand){.type=19,.command=42,.operand=2,.x=1,.y=1};
    CHECK(visit(s,&c,error,sizeof(error))==HMBTN_EVENT_DONE
        && hmbtn_event_variable_get(&v,42,2,&out) && out==8);
    /* Native bindings have no resource/SDL lifetime. An actor query sees the
     * same instance that movement/collision/rendering use, then route resumes. */
    uint8_t schedule[64]={0};CHECK(hmbtn_session_bind_schedules(s,schedule,sizeof(schedule)));
    HmbtnIntroActor *actor=hmbtn_session_bind_event_actor(s,21,HMBTN_SESSION_ACTOR_VISIT,true);CHECK(actor);
    actor->facing=3;s->village.people[21].actor.facing=1;
    CHECK(hmbtn_farm_intro_actor(&s->intro,21)==actor && hmbtn_event_variable_get(&v,27,21,&out) && out==3);
    hmbtn_session_unbind_resources(s);CHECK(hmbtn_farm_intro_actor(&s->intro,21)==actor);
    hmbtn_session_release_event_actors(s);CHECK(!hmbtn_farm_intro_actor(&s->intro,21));
    /* Real session interpreter + asynchronous message service: mutations before
     * and after the wait apply exactly once, not on retries or failures. */
    uint8_t code[80];
    instruction(code,19,255,255,0,42,4,1,1);
    instruction(code+16,2,255,1,1,1,418,0,0);
    instruction(code+32,19,255,255,0,32,21,-20,1);
    instruction(code+48,19,255,255,0,253,0,1,0);
    instruction(code+64,255,0,0,0,0,0,0,0);
    CHECK(hmbtn_session_open_event(s,140,code,sizeof(code),error,sizeof(error)));
    s->visit.active=true;s->visit.event=140;s->intro.game_command=visit;s->intro.game_context=s;
    s->village.people[21].friendship=100;s->village.days.values[4]=0;s->village.flags[205]=false;
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.pc==1 && s->village.days.values[4]==1);
    const HmbtnServiceResult *request=hmbtn_service_poll(&s->services,HMBTN_SERVICE_MESSAGE);
    CHECK(request->status==HMBTN_SERVICE_PENDING && request->request.id==418);
    const uint64_t ticket=request->ticket;
    for(unsigned i=0;i<20;++i) CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.pc==1
        && s->village.days.values[4]==1 && s->village.people[21].friendship==100 && !s->village.flags[205]);
    CHECK(hmbtn_service_claim(&s->services,HMBTN_SERVICE_MESSAGE));
    CHECK(hmbtn_service_complete(&s->services,ticket,1));
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.pc==2 && s->intro.slots[0]==1);
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.finished
        && s->village.people[21].friendship==80 && s->village.flags[205]);
    for(unsigned i=0;i<5;++i) CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->village.people[21].friendship==80);
    CHECK(hmbtn_session_open_event(s,140,code,sizeof(code),error,sizeof(error)));
    s->intro.game_command=visit;s->intro.game_context=s;s->village.flags[205]=false;
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)));
    request=hmbtn_service_poll(&s->services,HMBTN_SERVICE_MESSAGE);
    CHECK(hmbtn_service_fail(&s->services,request->ticket,"missing original text"));
    CHECK(!hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.pc==1 && !s->village.flags[205]
        && strstr(error,"missing original text"));
    CHECK(hmbtn_session_open_event(s,140,code,sizeof(code),error,sizeof(error)));
    s->intro.game_command=visit;s->intro.game_context=s;s->village.flags[205]=false;
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)));
    request=hmbtn_service_poll(&s->services,HMBTN_SERVICE_MESSAGE);
    CHECK(hmbtn_service_cancel(&s->services,request->ticket));
    CHECK(!hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.pc==1 && !s->village.flags[205]
        && strstr(error,"cancelled"));
    hmbtn_session_destroy(s);puts("Shared original variables, clamps, comparisons, arithmetic, slots, actor identity, pending/failing services and unique effects passed.");return 0;
fail:
    hmbtn_session_destroy(s);return 1;
}
