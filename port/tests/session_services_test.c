#include "session.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Session services line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static void instruction(uint8_t *p,unsigned type,unsigned yes,unsigned no,int a,int b,int x,int y)
{
    memset(p,0,16);p[0]=(uint8_t)type;p[1]=(uint8_t)yes;p[2]=(uint8_t)no;p[3]=1;
    hmbtn_write_le16(p+4,(uint16_t)a);hmbtn_write_le16(p+6,(uint16_t)b);
    hmbtn_write_le32(p+8,(uint32_t)x);hmbtn_write_le32(p+12,(uint32_t)y);
}
static HmbtnEventResult reward(void *context,const HmbtnEventCommand *command,char *error,size_t size)
{
    (void)command;(void)error;(void)size;HmbtnSession *s=context;
    ++s->game.inventory.supplies[0];s->game.economy.gold+=7;return HMBTN_EVENT_DONE;
}
int main(void)
{
    HmbtnSession *s=hmbtn_session_create(NULL,0),*other=hmbtn_session_create(NULL,0);
    uint8_t code[48];char error[256]={0};CHECK(s && other);
    instruction(code,2,1,0,1,7,3,0);
    instruction(code+16,0x51,2,1,0,0,0,0);
    instruction(code+32,255,0,0,0,0,0,0);
    CHECK(hmbtn_session_open_event(s,288,code,sizeof(code),error,sizeof(error)));
    s->intro.game_command=reward;s->intro.game_context=s;
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.pc==0);
    const HmbtnServiceResult *r=hmbtn_service_claim(&s->services,HMBTN_SERVICE_MESSAGE);CHECK(r);
    uint64_t ticket=r->ticket;
    for(unsigned i=0;i<8;++i) CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.pc==0);
    CHECK(s->game.economy.gold==500 && !s->game.inventory.supplies[0]);
    CHECK(hmbtn_service_complete(&s->services,ticket,2));
    CHECK(!hmbtn_service_complete(&s->services,ticket,1));
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.pc==1 && s->intro.slots[3]==2);
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->game.economy.gold==507 && s->game.inventory.supplies[0]==1);
    for(unsigned i=0;i<5;++i) CHECK(hmbtn_session_event_tick(s,error,sizeof(error)));
    CHECK(s->game.economy.gold==507 && s->game.inventory.supplies[0]==1 && other->game.economy.gold==500);
    CHECK(!hmbtn_service_complete(&s->services,ticket,2));
    /* Delayed resources block the issuing cursor; neither failure nor cancel
     * can be interpreted as success or repeat the next command's reward. */
    CHECK(hmbtn_session_open_event(s,288,code,sizeof(code),error,sizeof(error)));
    ticket=hmbtn_service_begin(&s->services,HMBTN_SERVICE_RESOURCE,(HmbtnServiceRequest){HMBTN_SERVICE_ACTION_DATA,12,0,0,0});
    CHECK(ticket && hmbtn_service_claim(&s->services,HMBTN_SERVICE_RESOURCE));
    for(unsigned i=0;i<5;++i) CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && !s->intro.message);
    CHECK(hmbtn_service_fail(&s->services,ticket,"injected missing data"));
    CHECK(!hmbtn_session_event_tick(s,error,sizeof(error)) && !s->intro.script.pc && s->game.economy.gold==507);
    s->intro.song_pending=s->intro.start_music=s->intro.action_pending=s->intro.xa_pending=true;
    CHECK(hmbtn_session_open_event(s,288,code,sizeof(code),error,sizeof(error)));error[0]=0;
    CHECK(!s->intro.song_pending && !s->intro.start_music && !s->intro.action_pending && !s->intro.xa_pending);
    CHECK(!hmbtn_service_complete_data(&s->services,ticket,code,sizeof(code),0));
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)));
    r=hmbtn_service_claim(&s->services,HMBTN_SERVICE_MESSAGE);CHECK(r);ticket=r->ticket;
    CHECK(hmbtn_service_cancel(&s->services,ticket));
    CHECK(!hmbtn_session_event_tick(s,error,sizeof(error)) && !s->intro.script.pc && s->game.inventory.supplies[0]==1);
    /* Receiving bytes/audio is distinct from applying them. Keep the cursor
     * parked until the consumer explicitly acknowledges the terminal result. */
    CHECK(hmbtn_session_open_event(s,288,code,sizeof(code),error,sizeof(error)));error[0]=0;
    ticket=hmbtn_service_begin(&s->services,HMBTN_SERVICE_RESOURCE,(HmbtnServiceRequest){HMBTN_SERVICE_ACTION_DATA,12,0,0,0});
    CHECK(ticket && hmbtn_service_claim(&s->services,HMBTN_SERVICE_RESOURCE));
    CHECK(hmbtn_service_complete_data(&s->services,ticket,code,sizeof(code),16));
    for(unsigned i=0;i<3;++i) CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && !s->intro.message && !s->intro.script.pc);
    CHECK(hmbtn_service_ack(&s->services,ticket));
    ticket=hmbtn_service_begin(&s->services,HMBTN_SERVICE_AUDIO,(HmbtnServiceRequest){HMBTN_SERVICE_LOAD_SONG,34,0,0,0});
    CHECK(ticket && hmbtn_service_claim(&s->services,HMBTN_SERVICE_AUDIO));
    CHECK(hmbtn_service_complete(&s->services,ticket,0));
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && !s->intro.message);
    CHECK(hmbtn_service_ack(&s->services,ticket));
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.message==7);
    /* Completion of a real animation wait is identified and consumed once. */
    instruction(code,5,1,0,17,2,0,0);
    CHECK(hmbtn_session_open_event(s,288,code,sizeof(code),error,sizeof(error)));error[0]=0;
    s->intro.game_command=reward;s->intro.game_context=s;
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)));
    r=hmbtn_service_claim(&s->services,HMBTN_SERVICE_ANIMATION);CHECK(r && r->request.actor==17 && r->request.id==2);ticket=r->ticket;
    for(unsigned i=0;i<6;++i) CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && !s->intro.script.pc);
    CHECK(hmbtn_service_complete(&s->services,ticket,0));
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.pc==1);
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->game.economy.gold==514 && s->game.inventory.supplies[0]==2);
    CHECK(!hmbtn_service_complete(&s->services,ticket,0));
    /* XA wait does not finish merely because the provider has claimed it. */
    instruction(code,0x17,1,0,4,0,0,0);
    CHECK(hmbtn_session_open_event(s,288,code,sizeof(code),error,sizeof(error)));
    s->intro.xa_active=true;
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)));
    r=hmbtn_service_claim(&s->services,HMBTN_SERVICE_AUDIO);CHECK(r && r->request.kind==HMBTN_SERVICE_WAIT_XA);ticket=r->ticket;
    for(unsigned i=0;i<5;++i) CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && !s->intro.script.pc);
    CHECK(hmbtn_service_complete(&s->services,ticket,0));
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.pc==1 && !s->intro.xa_active);
    /* Player action completion uses its own channel, independent of NPC waits. */
    instruction(code,0x3f,1,1,0,71,0,11);
    instruction(code+16,3,2,1,10,0,0,0);
    CHECK(hmbtn_session_open_event(s,288,code,sizeof(code),error,sizeof(error)));
    s->intro.action_ready=true;
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.action_active);
    r=hmbtn_service_claim(&s->services,HMBTN_SERVICE_ACTION);CHECK(r);ticket=r->ticket;
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.action_active);
    CHECK(hmbtn_service_complete(&s->services,ticket,0));
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && !s->intro.action_active);
    /* A yielded branch can point at the next sprite command before it has
     * selected that animation. Do not issue a wait for the previous sprite. */
    instruction(code,3,1,0,0,0,0,0);
    instruction(code+16,5,2,1,17,2,0,0);
    CHECK(hmbtn_session_open_event(s,288,code,sizeof(code),error,sizeof(error)));
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && !s->intro.script.pc);
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->intro.script.pc==1);
    CHECK(s->services.channels[HMBTN_SERVICE_ANIMATION].status==HMBTN_SERVICE_IDLE);
    CHECK(hmbtn_session_event_tick(s,error,sizeof(error)) && s->services.channels[HMBTN_SERVICE_ANIMATION].status==HMBTN_SERVICE_PENDING);
    hmbtn_session_destroy(other);hmbtn_session_destroy(s);
    puts("Delayed message/resource/animation/action/XA results, errors, cancellation and exactly-once inventory/economy effects passed.");return 0;
fail:
    hmbtn_session_destroy(other);hmbtn_session_destroy(s);return 1;
}
