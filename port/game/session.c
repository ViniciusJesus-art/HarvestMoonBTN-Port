#include "session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "psx_data.h"

static HmbtnIntroActor *resident_event_actor(void *context,int32_t id)
{
    HmbtnSession *s=context;
    if(id<=0 || (unsigned)id>=HMBTN_VILLAGERS) return NULL;
    const HmbtnIntroActor *bound=hmbtn_village_event_actor(&s->village,(unsigned)id);
    HmbtnIntroActor *owned[]={&s->visit.actors[id],&s->intro.mayor,&s->intro.elli,
        &s->intro.ellen,&s->intro.stu,&s->intro.zack,&s->intro.carter,&s->intro.barley,&s->intro.rick};
    for(unsigned i=0;i<sizeof(owned)/sizeof(*owned);++i) if(bound==owned[i]) return owned[i];
    return NULL;
}
static void bind_actor_resolver(HmbtnSession *s)
{
    s->intro.resolve_actor=resident_event_actor;s->intro.actor_context=s;s->intro.actor_count=HMBTN_VILLAGERS;
}
HmbtnSession *hmbtn_session_create(const uint16_t *layout,size_t count)
{
    HmbtnSession *s=calloc(1,sizeof(*s));
    if(!s) return NULL;
    hmbtn_game_state_init(&s->game);bind_actor_resolver(s);
    s->daily_date=hmbtn_session_date(&s->game.clock);
    hmbtn_farm_field_init(&s->field,layout,count);
    hmbtn_house_interaction_init(&s->house,&s->game);
    hmbtn_new_game_init(&s->dialogue,hmbtn_game_registration(&s->game));
    s->village.player=&s->intro.player;s->village.weather=&s->game.weather;s->village.doors=&s->area_doors;
    hmbtn_farm_action_init(&s->farm_action);
    hmbtn_farm_menu_init(&s->menu,hmbtn_game_economy_const(&s->game));
    s->menu.poultry=&s->poultry;s->feed_shop.poultry=&s->poultry;s->feed_shop.economy=&s->game.economy;
    /* Preserve zeroed poultry and the pre-handoff village. The village's
     * existing defaults are installed only when its schedules arrive. */
    return s;
}
bool hmbtn_session_bind_schedules(HmbtnSession *s,const uint8_t *data,size_t size)
{
    if(!s || !data || !size) return false;
    if(!s->village_initialized) {
        hmbtn_village_init(&s->village,data,size);s->village_initialized=true;
        s->village.player=&s->intro.player;s->village.weather=&s->game.weather;s->village.doors=&s->area_doors;
        hmbtn_village_refresh_weather(&s->village);
    } else {
        s->village.schedule_data=data;s->village.schedule_size=size;
    }
    s->village_ready=true;
    return true;
}
void hmbtn_session_unbind_resources(HmbtnSession *s)
{
    if(!s) return;
    s->village.schedule_data=NULL;s->village.schedule_size=0;s->village_ready=false;
    s->intro.ground=NULL;s->intro.game_command=NULL;s->intro.game_context=NULL;
    hmbtn_services_rebind(&s->services);
}
void hmbtn_session_destroy(HmbtnSession *s)
{
    if(!s) return;
    hmbtn_session_unbind_resources(s);
    hmbtn_services_clear(&s->services);
    free(s);
}

void hmbtn_session_release_event_actors(HmbtnSession *s)
{
    if(!s) return;
    HmbtnVillage *v=&s->village;
    for(unsigned id=1;id<HMBTN_VILLAGERS;++id) {
        if(hmbtn_village_event_actor(v,id)) v->people[id].placed=false;
        v->event_actors[id]=NULL;
    }
    v->event_actor=NULL;v->event_actor_id=0;
}
HmbtnIntroActor *hmbtn_session_bind_event_actor(HmbtnSession *s,unsigned id,
    HmbtnSessionActorSource source,bool copy_route)
{
    if(!s || !s->village_initialized || !id || id>=HMBTN_VILLAGERS) return NULL;
    HmbtnIntroActor *actor=NULL;
    switch(source) {
    case HMBTN_SESSION_ACTOR_PRIMARY:
        if(id!=17) return NULL;
        actor=&s->intro.mayor;break;
    case HMBTN_SESSION_ACTOR_INTRO:
        /* Script-local childhood/boy/girl IDs are not resident identities. */
        switch(id) { case 9:case 10:case 15:case 17:case 19:case 23:case 26:case 34:break;
        default:return NULL; }
        actor=hmbtn_farm_intro_actor(&s->intro,(int32_t)id);break;
    case HMBTN_SESSION_ACTOR_VISIT:actor=&s->visit.actors[id];break;
    default:return NULL;
    }
    HmbtnVillage *v=&s->village;
    const HmbtnIntroActor *bound=hmbtn_village_event_actor(v,id);
    if(bound) return bound==actor ? actor : NULL;
    if(source==HMBTN_SESSION_ACTOR_PRIMARY && v->event_actor) return NULL;
    if(copy_route) *actor=v->people[id].actor;
    if(source==HMBTN_SESSION_ACTOR_PRIMARY) { v->event_actor=actor;v->event_actor_id=id; }
    else v->event_actors[id]=actor;
    return actor;
}
bool hmbtn_session_open_event(HmbtnSession *s,unsigned event,const uint8_t *code,
    size_t size,char *error,size_t error_size)
{
    HmbtnEventScript validated;
    if(!s) { snprintf(error,error_size,"Missing event session");return false; }
    if(size>sizeof(s->event_code)) { snprintf(error,error_size,"Event bytecode exceeds session storage");return false; }
    if(!hmbtn_event_script_open(&validated,code,size,error,error_size)) return false;
    /* Validation precedes mutation; the existing interpreter bounds size. */
    hmbtn_services_clear(&s->services);
    memmove(s->event_code,code,size);
    hmbtn_session_release_event_actors(s);
    if(event==116) {
        if(!hmbtn_farm_intro_open(&s->intro,s->event_code,size,error,error_size)) return false;
    } else {
        s->intro.script=validated;s->intro.script.data=s->event_code;
        memset(s->intro.childhood,0,sizeof(s->intro.childhood));
        const unsigned ids[]={9,10,15,17,19,23,26,27,33,34};
        for(unsigned i=0;i<sizeof(ids)/sizeof(*ids);++i)
            memset(hmbtn_farm_intro_actor(&s->intro,(int32_t)ids[i]),0,sizeof(HmbtnIntroActor));
    }
    bind_actor_resolver(s);
    memset(s->intro.slots,0,sizeof(s->intro.slots));
    s->intro.event=event;s->intro.event_pending=false;s->intro.queued_event=0;
    s->intro.cell_actor=0;s->intro.camera_actor=0;s->intro.camera_gliding=false;
    s->intro.message=0;s->intro.message_done=false;s->intro.message_result_slot=-1;
    s->intro.text_bank=UINT32_MAX;s->intro.zone_exited=false;s->intro.advanced_minutes=0;
    s->intro.finish_requested=false;s->intro.action_ready=s->intro.action_active=false;
    /* A replacement event cancels the old service intents as well as tickets.
     * Keep the active XA stream snapshot; the host owns its ongoing playback. */
    s->intro.song_pending=s->intro.start_music=s->intro.action_pending=false;
    s->intro.xa_pending=s->intro.xa_started=false;
    s->intro.player_free=false;s->dialogue.message.phase=HMBTN_MESSAGE_CLOSED;
    /* Arrival/childhood actors precede the village handoff and are local to
     * that script. Tour/funeral actors explicitly replace live NPC routes. */
    if(s->village_initialized && (event==12 || event==172)) {
        const unsigned tour[]={17,23,26},funeral[]={10,15,19};
        const unsigned *ids=event==12 ? tour : funeral;
        for(unsigned i=0;i<3;++i)
            hmbtn_session_bind_event_actor(s,ids[i],HMBTN_SESSION_ACTOR_INTRO,false);
    }
    return true;
}
static bool service_results(HmbtnSession *s,char *error,size_t size)
{
    for(unsigned i=0;i<HMBTN_SERVICE_CHANNELS;++i) {
        const HmbtnServiceResult *r=hmbtn_service_poll(&s->services,(HmbtnServiceChannel)i);
        if(r->status==HMBTN_SERVICE_FAILED || r->status==HMBTN_SERVICE_CANCELLED) {
            snprintf(error,size,"Service %u: %s",i,r->status==HMBTN_SERVICE_CANCELLED ? "cancelled" : r->error);return false;
        }
        if(r->status==HMBTN_SERVICE_DONE) {
            if(i==HMBTN_SERVICE_MESSAGE) {
                if(r->request.actor) {
                    if(!s->talk.active || s->talk.target!=r->request.actor || s->talk.request!=r->request.id || s->talk.bank!=r->request.bank) {
                        snprintf(error,size,"Conversation result does not match active request");return false;
                    }
                    s->talk.message_done=true;s->talk.result=r->value;
                    hmbtn_service_ack(&s->services,r->ticket);continue;
                }
                if(s->intro.message!=r->request.id || s->intro.message_bank!=r->request.bank) {
                    snprintf(error,size,"Message result does not match active event");return false;
                }
                s->intro.message_done=true;
                if(s->intro.message_result_slot>=0 && s->intro.message_result_slot<32)
                    s->intro.slots[s->intro.message_result_slot]=(int32_t)r->value;
                hmbtn_service_ack(&s->services,r->ticket);
            } else if(i==HMBTN_SERVICE_ANIMATION || i==HMBTN_SERVICE_ACTION) {
                HmbtnIntroActor *actor=hmbtn_farm_intro_actor(&s->intro,(int32_t)r->request.actor);
                if(!actor || actor->sprite.animation!=r->request.id) {
                    snprintf(error,size,"Animation result does not match active actor");return false;
                }
                actor->animation_done=true;
                if(i==HMBTN_SERVICE_ACTION) s->intro.action_finished=true;
                hmbtn_service_ack(&s->services,r->ticket);
            } else if(i==HMBTN_SERVICE_AUDIO && r->request.kind==HMBTN_SERVICE_WAIT_XA) {
                s->intro.xa_active=false;hmbtn_service_ack(&s->services,r->ticket);
            }
        } else if(r->status==HMBTN_SERVICE_PENDING) {
            if(i==HMBTN_SERVICE_MESSAGE) {
                if(r->request.actor) s->talk.message_done=false;
                else s->intro.message_done=false;
            }
            if(i==HMBTN_SERVICE_ACTION) s->intro.action_finished=false;
            if(i==HMBTN_SERVICE_ANIMATION) {
                HmbtnIntroActor *actor=hmbtn_farm_intro_actor(&s->intro,(int32_t)r->request.actor);
                if(actor) actor->animation_done=false;
            }
        }
    }
    return true;
}
static bool event_requests(HmbtnSession *s,char *error,size_t size)
{
    HmbtnFarmIntro *in=&s->intro;
    if(in->message && !in->message_done && s->services.channels[HMBTN_SERVICE_MESSAGE].status==HMBTN_SERVICE_IDLE
            && !hmbtn_service_begin(&s->services,HMBTN_SERVICE_MESSAGE,(HmbtnServiceRequest){HMBTN_SERVICE_SHOW_MESSAGE,in->message,in->message_bank,0,in->script.pc})) goto exhausted;
    if(!in->script.finished && !in->script.boundary && in->action_active && !in->action_finished && s->services.channels[HMBTN_SERVICE_ACTION].status==HMBTN_SERVICE_IDLE
            && !hmbtn_service_begin(&s->services,HMBTN_SERVICE_ACTION,(HmbtnServiceRequest){HMBTN_SERVICE_WAIT_ACTION,in->player.sprite.animation,0,41,in->script.pc})) goto exhausted;
    if(in->script.data && in->script.pc<in->script.count && !in->script.finished && !in->script.boundary) {
        const uint8_t *p=in->script.data+in->script.pc*16;
        if(p[0]==5 && !hmbtn_read_le32(p+12) && s->services.channels[HMBTN_SERVICE_ANIMATION].status==HMBTN_SERVICE_IDLE) {
            const unsigned actor=hmbtn_read_le16(p+4),animation=hmbtn_read_le16(p+6);
            HmbtnIntroActor *slot=hmbtn_farm_intro_actor(in,(int32_t)actor);
            if(slot && slot->sprite.animation==animation
                    && !hmbtn_service_begin(&s->services,HMBTN_SERVICE_ANIMATION,(HmbtnServiceRequest){HMBTN_SERVICE_WAIT_ANIMATION,animation,0,actor,in->script.pc})) goto exhausted;
        }
        if(p[0]==0x17 && !hmbtn_read_le32(p+8) && in->xa_active && s->services.channels[HMBTN_SERVICE_AUDIO].status==HMBTN_SERVICE_IDLE
                && !hmbtn_service_begin(&s->services,HMBTN_SERVICE_AUDIO,(HmbtnServiceRequest){HMBTN_SERVICE_WAIT_XA,hmbtn_read_le16(p+4),0,0,in->script.pc})) goto exhausted;
    }
    return true;
exhausted:
    snprintf(error,size,"Cannot issue event service request");return false;
}
bool hmbtn_session_event_tick(HmbtnSession *s,char *error,size_t error_size)
{
    if(!s) { snprintf(error,error_size,"Missing event session");return false; }
    if(!service_results(s,error,error_size)) return false;
    /* A delayed load is not permission to advance its issuing instruction. */
    if(s->services.channels[HMBTN_SERVICE_RESOURCE].status!=HMBTN_SERVICE_IDLE
            || (s->services.channels[HMBTN_SERVICE_AUDIO].status!=HMBTN_SERVICE_IDLE
                && s->services.channels[HMBTN_SERVICE_AUDIO].request.kind!=HMBTN_SERVICE_WAIT_XA)) return true;
    if(!hmbtn_farm_intro_tick(&s->intro,error,error_size)) return false;
    return event_requests(s,error,error_size);
}
bool hmbtn_session_talk_tick(HmbtnSession *s,char *error,size_t size)
{
    if(!s) { snprintf(error,size,"Missing conversation session");return false; }
    if(!service_results(s,error,size)) return false;
    if(s->services.channels[HMBTN_SERVICE_RESOURCE].status!=HMBTN_SERVICE_IDLE) return true;
    s->talk.shop=s->talk.village==&s->village ? &s->feed_shop : NULL;
    if(!hmbtn_village_talk_tick(&s->talk,error,size)) return false;
    if(s->talk.active && s->talk.pending && !s->talk.message_done
            && s->services.channels[HMBTN_SERVICE_MESSAGE].status==HMBTN_SERVICE_IDLE
            && !hmbtn_service_begin(&s->services,HMBTN_SERVICE_MESSAGE,(HmbtnServiceRequest){
                HMBTN_SERVICE_SHOW_MESSAGE,s->talk.request,s->talk.bank,s->talk.target,s->talk.message_pc})) {
        snprintf(error,size,"Cannot issue conversation message request");return false;
    }
    return true;
}
void hmbtn_session_finish_visit(HmbtnSession *s)
{
    if(!s || !s->visit.active) return;
    const bool ellen=s->visit.event==130;
    hmbtn_session_release_event_actors(s);
    s->visit=(HmbtnFarmVisit){0};s->intro.game_command=NULL;s->intro.game_context=NULL;
    s->intro.player_free=false;
    s->village.flags[3]=s->village.flags[4]=s->village.flags[5]=s->village.flags[0xB0]=false;
    s->village.people[17].placed=false;s->intro.mayor.visible=false;
    if(ellen) s->intro.elli.visible=s->intro.ellen.visible=s->intro.stu.visible=false;
    memset(s->intro.mayor.velocity,0,sizeof(s->intro.mayor.velocity));
}
