/* PF-04: preserved coordinator from scene_preview.c (d3c0f38 + PF-03).
 * References: farm_main/farm_frame, event_script/commands, doors, clock_events.
 * Decisions/state live here; host callbacks only supply decoded resources and
 * presentation/audio effects. No SDL, filesystem or rendering dependencies. */
#include "runtime.h"
#include "economy.h"
#include "ground.h"
#include "world_map.h"
#include "psx_data.h"
#include <stdio.h>
#include <string.h>
#define HMBTN_TEXT_RECORD_BYTES 8192
static bool effect(HmbtnSessionFrame *s,HmbtnRuntimeEffect e)
{ return !s->effect || s->effect(s,&e,s->error,s->error_size); }
static void ui_sound(HmbtnSessionFrame *s,unsigned id)
{ (void)effect(s,(HmbtnRuntimeEffect){.kind=HMBTN_RUNTIME_SOUND,.id=id}); }
static bool key_sound(HmbtnSessionFrame *s,unsigned bank,unsigned program,unsigned tone,unsigned note,unsigned volume)
{ return effect(s,(HmbtnRuntimeEffect){HMBTN_RUNTIME_KEY_SOUND,0,bank,program,tone,note,volume,NULL,0}); }
static bool load_area(HmbtnSessionFrame *s,unsigned id,char *error,size_t size)
{ if(!s->area) { snprintf(error,size,"Missing area resource provider");return false; } return s->area(s,id,error,size); }
static bool load_event(HmbtnSessionFrame *s,unsigned id,char *error,size_t size)
{ if(!s->event) { snprintf(error,size,"Missing event resource provider");return false; } return s->event(s,id,error,size); }
static bool load_action_bank(HmbtnSessionFrame *s,unsigned id,char *error,size_t size)
{ return s->exploration.action_bank(s->exploration.context,id,error,size); }
static bool farm_song(HmbtnSessionFrame *s,unsigned id,char *error,size_t size)
{ return s->song && s->song(s->context,id,true,error,size); }
static unsigned world_music_volume(const HmbtnSessionFrame *s)
{ return hmbtn_world_music_volume_weather(s->section,16,s->session->game.clock.season,s->session->game.clock.hour,s->session->game.weather.today); }
static const HmbtnServiceResult *service_resource(HmbtnSessionFrame *s,HmbtnServiceKind kind,unsigned id,unsigned bank,char *e,size_t n)
{ if(!s->resource) { snprintf(e,n,"Missing resource provider");return NULL; } return s->resource(s->context,kind,id,bank,e,n); }
static const HmbtnServiceResult *service_audio(HmbtnSessionFrame *s,HmbtnServiceKind kind,unsigned id,char *e,size_t n)
{ if(!s->audio) { snprintf(e,n,"Missing audio provider");return NULL; } return s->audio(s->context,kind,id,e,n); }
static bool service_read(HmbtnSessionFrame *s,HmbtnServiceKind kind,unsigned id,unsigned bank,uint8_t *data,size_t bytes,uint32_t *value,char *e,size_t n)
{ if(!s->read) { snprintf(e,n,"Missing resource reader");return false; } return s->read(s->context,kind,id,bank,data,bytes,value,e,n); }
static bool message_record(HmbtnSessionFrame *s,const uint8_t *record,size_t bytes,HmbtnMessage *m,char *error,size_t size)
{ return hmbtn_message_open(m,record,bytes,error,size) && effect(s,(HmbtnRuntimeEffect){.kind=HMBTN_RUNTIME_MESSAGE_ART,.data=record,.bytes=bytes}); }
static bool editor_load(HmbtnSessionFrame *s,HmbtnNewGame *g,char *error,size_t size)
{
    if(!g->request || g->request==g->loaded) return true;
    uint8_t record[8192];
    if(!s->exploration.text(s->exploration.context,3,g->request,record,sizeof(record),error,size)
        || !message_record(s,record,sizeof(record),&g->message,error,size)) return false;
    g->loaded=g->request;return true;
}
static void service_animations(HmbtnSessionFrame *s)
{
    const HmbtnServiceChannel channels[]={HMBTN_SERVICE_ANIMATION,HMBTN_SERVICE_ACTION};
    for(unsigned i=0;i<2;++i) {
        const HmbtnServiceResult *r=hmbtn_service_poll(&s->session->services,channels[i]);
        if(r->status!=HMBTN_SERVICE_PENDING) continue;
        hmbtn_service_claim(&s->session->services,channels[i]);
        HmbtnIntroActor *actor=hmbtn_farm_intro_actor(&s->session->intro,(int32_t)r->request.actor);
        if(!actor || actor->sprite.animation!=r->request.id) { hmbtn_service_fail(&s->session->services,r->ticket,"Animation actor changed before completion");continue; }
        if(actor->animation_done) hmbtn_service_complete(&s->session->services,r->ticket,0);
    }
}
static HmbtnEventResult animal_command(void *context,const HmbtnEventCommand *c,char *error,size_t size)
{
    HmbtnSession *owner=context;
    HmbtnSessionFrame *s=owner->active_frame;
    if(!s) { snprintf(error,size,"Event command outside session tick");return HMBTN_EVENT_ERROR; }
    if(c->type==0x51 && c->command==14 && s->session->animal_event==172) {
        if(c->operand==0 && c->x>=0 && c->x<=2 && c->y>=0 && c->y<=1)
            return c->x==2 && hmbtn_poultry_dead(&s->session->poultry,c->y==0) ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
        if(c->operand==1 && !c->x && !c->y) {
            s->session->carry.discarded+=hmbtn_poultry_clear_dead(&s->session->poultry);
            if(s->session->carry.discarded>99) s->session->carry.discarded=99;
            return HMBTN_EVENT_DONE;
        }
    }
    if(c->type==0x51 && c->command==13 && s->session->animal_event==299 && !c->y) {
        if(c->operand==0 && !c->x) return HMBTN_EVENT_DONE;
        if(c->operand==1 && c->x>=0 && c->x<=3)
            return c->x>=2 && hmbtn_poultry_hatch_ready(&s->session->poultry,(unsigned)c->x-2) ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
        if(c->operand==2 && c->x>=2 && c->x<=3) {
            HmbtnHen *hen=hmbtn_poultry_hatch_ready(&s->session->poultry,(unsigned)c->x-2);
            if(!hen) { snprintf(error,size,"Chick naming lost its reserved slot");return HMBTN_EVENT_ERROR; }
            if(!s->session->animal_name_active) {
                hmbtn_name_editor_begin(&s->session->animal_name);s->session->animal_name.no_cancel=true;s->session->animal_name_active=true;
                return HMBTN_EVENT_WAIT;
            }
            if(!s->session->animal_name.finished || s->session->animal_name.message.phase!=HMBTN_MESSAGE_CLOSED) return HMBTN_EVENT_WAIT;
            memcpy(hen->name,s->session->animal_name.name,sizeof(hen->name));s->session->animal_name_active=false;return HMBTN_EVENT_DONE;
        }
        if(c->operand==3 && !c->x) { hmbtn_poultry_name_end(&s->session->poultry);return HMBTN_EVENT_DONE; }
    }
    if(c->type==0x5B && s->session->animal_event==172 && c->command==32 && c->operand==1 && c->x==-20 && c->y==-1000) {
        /* event_cmd_apply_var_table: bachelorettes use affection, others friendship. */
        for(unsigned i=1;i<HMBTN_VILLAGERS;++i) if(i!=27 && i!=33) {
            HmbtnVillager *v=&s->session->village.people[i];
            if(i==3 || i==7 || i==9 || i==14 || i==20) v->affection=v->affection>1000 ? v->affection-1000 : 0;
            else v->friendship=v->friendship>20 ? v->friendship-20 : 0;
        }
        for(unsigned i=0;i<7;++i) s->session->menu.stats.helper_affection[i]=s->session->menu.stats.helper_affection[i]>20 ? s->session->menu.stats.helper_affection[i]-20 : 0;
        return HMBTN_EVENT_DONE;
    }
    snprintf(error,size,"Unsupported animal event command %u/%d/%d",c->type,c->command,c->operand);
    return HMBTN_EVENT_ERROR;
}
static bool animal_event_begin(HmbtnSessionFrame *s,unsigned event,char *error,size_t size)
{
    if(!load_event(s,event,error,size)) return false;
    s->session->animal_event=event;s->session->control.enabled=false;s->session->control.mode=0;
    s->session->intro.game_command=animal_command;s->session->intro.game_context=s->session;s->session->intro.event_active=true;
    s->session->intro.player.pose=false;s->session->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->session->intro.player.facing,0};
    memset(s->session->intro.player.velocity,0,sizeof(s->session->intro.player.velocity));
    s->session->intro.fade_level=0;s->session->intro.fade_mode=0;s->session->intro.fade_length=0;s->session->intro.music_mode=0;
    return true;
}
static HmbtnEventResult visit_command(void *context,const HmbtnEventCommand *c,char *error,size_t size)
{
    HmbtnSession *owner=context;
    HmbtnSessionFrame *s=owner->active_frame;
    if(!s) { snprintf(error,size,"Event command outside session tick");return HMBTN_EVENT_ERROR; }
    if(s->session->visit.event==289) {
        if(c->type==7 && c->command==42 && c->operand==-1 && c->x==-1 && c->y==3) {
            for(unsigned id=1;id<HMBTN_VILLAGERS;++id) {
                if(s->session->village.people[id].location!=hmbtn_read_le16(s->section+2)) continue;
                HmbtnIntroActor *actor=hmbtn_session_bind_event_actor(s->session,id,
                    id==17 ? HMBTN_SESSION_ACTOR_PRIMARY : HMBTN_SESSION_ACTOR_VISIT,true);
                if(!actor) { snprintf(error,size,"Conflicting ingredient-event actor %u",id);return HMBTN_EVENT_ERROR; }
                memset(actor->velocity,0,sizeof(actor->velocity));actor->visible=true;
                actor->sprite=(HmbtnAnimationPlayer){0,(uint16_t)actor->facing,0};
                if(!s->animation(s->context,id,true)) {
                    snprintf(error,size,"Original village art missing for ingredient-event actor %u",id);return HMBTN_EVENT_ERROR;
                }
            }
            return HMBTN_EVENT_DONE;
        }
        if(c->type==30 && c->operand==1 && c->x==1 && c->y==1) {
            const int found=hmbtn_event_items_take(s->session->house.inventory,c->command,1,1,1);
            if(found>=0) return found ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
        }
    }
    if(c->type==7 && s->session->visit.event==130 && c->command==42 && c->operand==-1 && c->x==-1 && c->y==3) {
        const unsigned ids[]={9,23,34};
        for(unsigned i=0;i<3;++i) {
            HmbtnIntroActor *actor=hmbtn_farm_intro_actor(&s->session->intro,(int32_t)ids[i]);
            if(s->session->village.people[ids[i]].location!=hmbtn_read_le16(s->section+2)) continue;
            memset(actor->velocity,0,sizeof(actor->velocity));
            actor->sprite=(HmbtnAnimationPlayer){0,(uint16_t)actor->facing,0};actor->visible=true;
        }
        return HMBTN_EVENT_DONE;
    }
    if(c->type==30 && s->session->visit.event==130 && c->command==204 && c->operand==1 && !c->x && c->y==1) {
        const bool held=hmbtn_event_items_take(s->session->house.inventory,204,1,0,1)>0;
        return held ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
    }
    if((c->type==31 && (s->session->visit.event==129 || s->session->visit.event==130) && c->command==204 && !c->operand && !c->x && !c->y)
            || (c->type==65 && s->session->visit.event==131 && c->command==225 && !c->operand && c->x==2 && !c->y)) {
        const bool begun=hmbtn_event_item_begin(&s->session->event_item,&s->session->intro.player,s->session->house.inventory,(unsigned)c->command,c->type==65);
        return begun ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
    }
    if(c->type==69 && (s->session->visit.event==130 || s->session->visit.event==131) && c->command==1 && !c->operand && c->x==0x20000000 && !c->y)
        return s->session->event_item.phase!=HMBTN_EVENT_ITEM_IDLE ? HMBTN_EVENT_DONE : HMBTN_EVENT_WAIT;
    if(c->type==44 && s->session->visit.event==130 && !c->command && c->operand==-1 && c->x==300 && !c->y) {
        hmbtn_economy_gold_change(&s->session->game.economy,300,false);return HMBTN_EVENT_DONE;
    }
    const HmbtnEventResult result=hmbtn_farm_visit_command(&s->session->visit,&s->session->intro,&s->session->village,&s->session->game.clock,c,error,size);
    if(c->type==91 && result==HMBTN_EVENT_DONE) for(unsigned i=0;i<7;++i) {
        const int n=(int)s->session->menu.stats.helper_affection[i]+c->x;
        s->session->menu.stats.helper_affection[i]=(unsigned)(n<0 ? 0 : n>255 ? 255 : n);
    }
    return result;
}
static bool visit_begin(HmbtnSessionFrame *s,unsigned event,char *error,size_t size)
{
    if(!load_event(s,event,error,size) || !hmbtn_farm_visit_begin(&s->session->visit,&s->session->triggers,event,&s->session->village)) return false;
    s->session->visit.start_area=s->session->area;
    s->session->intro.game_command=visit_command;s->session->intro.game_context=s->session;
    if(event!=129 && event!=130 && event!=140 && !hmbtn_session_bind_event_actor(s->session,17,
            HMBTN_SESSION_ACTOR_PRIMARY,event==289)) {
        snprintf(error,size,"Conflicting visitor actor 17");return false;
    }
    if(event==140) {
        HmbtnIntroActor *actor=hmbtn_session_bind_event_actor(s->session,21,HMBTN_SESSION_ACTOR_VISIT,true);
        if(!actor || !s->animation(s->context,21,false)) {
            snprintf(error,size,"Original Duke event actor/art unavailable");return false;
        }
    }
    if(event==289) {
        s->session->visit.talk=true;s->session->visit.talk_target=17;
        if(!s->animation(s->context,17,true)) {
            snprintf(error,size,"Original village art missing for ingredient-delivery actor 17");return false;
        }
    }
    if(event==130) {
        const unsigned ids[]={9,23,34};
        for(unsigned i=0;i<3;++i) {
            HmbtnIntroActor *actor=hmbtn_session_bind_event_actor(s->session,ids[i],HMBTN_SESSION_ACTOR_INTRO,true);
            if(!actor) { snprintf(error,size,"Conflicting pie-event actor %u",ids[i]);return false; }
                        if((ids[i]!=34 || actor->visible) && !s->animation(s->context,ids[i],true)) {
                snprintf(error,size,"Original village art missing for pie-event actor %u",ids[i]);return false;
            }
        }
        s->session->intro.elli.visible=false;
    }
    s->session->intro.event_active=false;s->session->intro.fade_level=0;s->session->intro.fade_mode=0;s->session->intro.fade_length=0;
    s->session->intro.player.pose=false;s->session->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->session->intro.player.facing,0};
    s->session->dialogue.step=100;s->session->dialogue.choice=0;
    s->session->control.enabled=false;s->session->control.mode=0;
    return true;
}
static bool village_conversation(HmbtnSessionFrame *s,uint32_t pressed,uint32_t held,char *error,size_t size)
{
    HmbtnVillageTalk *t=&s->session->talk;
    if(s->session->quick_ticks && !--s->session->quick_ticks) s->session->quick_id=0;
    if(t->pending && t->loaded && !t->message_done) {
        HmbtnMessage *m=&s->session->dialogue.message;
        if(m->choices && m->phase==HMBTN_MESSAGE_WAIT_END && (pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN))) {
            s->session->dialogue.choice=(s->session->dialogue.choice+((pressed&HMBTN_PAD_UP) ? m->choices-1 : 1))%m->choices;
            ui_sound(s,5);
        }
        const int sound=hmbtn_message_tick(m,pressed,held);
        if(sound>=0) ui_sound(s,(unsigned)sound);
        if(m->phase==HMBTN_MESSAGE_CLOSED) {
            const HmbtnServiceResult *r=hmbtn_service_poll(&s->session->services,HMBTN_SERVICE_MESSAGE);
            hmbtn_service_claim(&s->session->services,HMBTN_SERVICE_MESSAGE);
            if(r->status==HMBTN_SERVICE_PENDING && !hmbtn_service_complete(&s->session->services,r->ticket,
                    m->choices ? s->session->dialogue.choice+1 : 0)) return false;
        }
    }
    if(!hmbtn_session_talk_tick(s->session,error,size)) return false;
    if(t->show_tool) {
        /* player_show_tool: use the existing base ANI 0x47 and item picture. */
        s->session->quick_tool=true;s->session->quick_id=t->show_tool;s->session->quick_ticks=t->show_frames;
        s->session->intro.player.sprite=(HmbtnAnimationPlayer){71,(uint16_t)s->session->intro.player.facing,0};s->session->intro.player.animation_done=false;
        t->show_tool=t->show_frames=0;
        const HmbtnFarmSound *fx=hmbtn_farm_sound(7);
        if(!fx || !key_sound(s,2,fx->program,fx->tone,fx->note,fx->volume)) {
            snprintf(error,size,"Original item presentation tone unavailable");return false;
        }
    }
    if(t->pending && !t->loaded && t->active) {
        uint8_t record[HMBTN_TEXT_RECORD_BYTES];
        const HmbtnServiceResult *resource=service_resource(s,HMBTN_SERVICE_TEXT_DATA,t->request,t->bank,error,size);
        if(!resource) return false;
        if(resource->status==HMBTN_SERVICE_PENDING) return true;
        if(!service_read(s,HMBTN_SERVICE_TEXT_DATA,t->request,t->bank,record,sizeof(record),NULL,error,size)
                || !hmbtn_feed_shop_expand(record,sizeof(record),hmbtn_game_registration_const(&s->session->game))
                || !message_record(s,record,sizeof(record),&s->session->dialogue.message,error,size)) return false;
        hmbtn_service_claim(&s->session->services,HMBTN_SERVICE_MESSAGE);
        s->session->dialogue.step=100;s->session->dialogue.choice=0;
        s->session->dialogue.message.suppress_cursor=s->session->dialogue.message.choices!=0;t->loaded=true;
    }
    if(t->script.boundary) {
        s->session->quick_ticks=s->session->quick_id=0;
        fprintf(stderr,"NPC %u conversation stopped at unported command %u, step %u.\n",
            t->target,t->boundary_type,t->script.pc);
    }
    s->session->control.mode=0;
    s->session->control.carrying=s->session->house.inventory->held_item.id!=0;
    const HmbtnFarmItem *carried=hmbtn_farm_item(s->session->house.inventory->held_item.id);
    s->session->control.carrying_big=s->session->control.carrying && carried && (carried->flags&0x180u);
    if(!s->session->quick_ticks) s->session->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)(s->session->control.carrying_big ? 41 : s->session->control.carrying ? 27 : 0),(uint16_t)s->session->intro.player.facing,0};
    return true;
}
static bool morning(HmbtnSessionFrame *,bool,char *,size_t);
static HmbtnExplorationFrame exploration_frame(HmbtnSessionFrame *);
static bool exploration_boundary(void *context,HmbtnSession *session,HmbtnExplorationBoundary kind,
    unsigned value,HmbtnLogicalInput input,HmbtnExplorationBoundaryResult *result,char *error,size_t error_size)
{
    HmbtnSessionFrame *s=context;
    if(s->session!=session) { snprintf(error,error_size,"Exploration adapter session mismatch");return false; }
    switch(kind) {
    case HMBTN_EXP_TALK:return village_conversation(s,input.pressed,input.held,error,error_size);
    case HMBTN_EXP_START_VISIT:return visit_begin(s,value,error,error_size);
    case HMBTN_EXP_START_TALK: {
        const unsigned id=value;
                size_t length=0;uint8_t block[8192];
                if(!s->conversation(s->context,id,block,sizeof(block),&length,error,error_size)
                        || !hmbtn_village_talk_open(&s->session->talk,id,block,length,&s->session->village,&s->session->intro.player,
                            &s->session->game.clock,hmbtn_game_registration_const(&s->session->game),&s->session->house,error,error_size)
                        || !village_conversation(s,0,0,error,error_size)) return false;
        return true;
    }
    case HMBTN_EXP_TRANSITION: {  // Sistema de Interação com portas 
        if(s->session->door_tick) {
            /* doors.c supplies swing direction and 0x420 limit. Preserve the
             * native retreat, acceleration 5 and 31-tick entry timing. */
            if(s->session->door_angle>-0x420) {
                const unsigned face = s->session->intro.player.facing;

                const unsigned opposite_face = (face + 2u) % 4u;

                const unsigned animation = opposite_face + (s->session->control.carrying_big ? 42u : s->session->control.carrying ? 15u : 1u);

                if (s->session->intro.player.sprite.animation != animation) {
                    s->session->intro.player.sprite =
                        (HmbtnAnimationPlayer){(uint16_t)animation, 0, 0};
                }

                //s->session->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)(s->session->control.carrying_big ? 41 : 0),(uint16_t)s->session->intro.player.facing,0};
                s->session->door_speed+=5;    // Velocidade de abertura portas 
                s->session->door_angle-=s->session->door_speed;
                if(s->session->door_angle<-0x420) s->session->door_angle=-0x420; // Angulo de aberturas

                s->session->intro.player.position[face&1u ? 0 : 2]+=face<2 ? 30 : -30;
                memcpy(s->session->intro.camera,s->session->intro.player.position,sizeof(s->session->intro.camera));
                //const unsigned animation=face+(s->session->control.carrying_big ? 42u : 1u);
                const int turn=(s->session->intro.player.facing==1 || s->session->intro.player.facing==2) ? s->session->door_angle : -s->session->door_angle;
                s->scene->angle[s->session->door_model][1]=(uint16_t)(s->session->door_base_angle+turn);
                if(s->session->door_twin>=0) s->scene->angle[s->session->door_twin][1]=(uint16_t)(s->session->door_twin_base-turn);
            } else {
                ++s->session->door_tick;
                const unsigned face=s->session->intro.player.facing;

                s->session->intro.player.position[face&1u ? 0 : 2]+=face<2 ? -41 : 41;
                memcpy(s->session->intro.camera,s->session->intro.player.position,sizeof(s->session->intro.camera));
                const unsigned animation=face+(s->session->control.carrying_big ? 42u : s->session->control.carrying ? 15u : 1u);
                if(s->session->intro.player.sprite.animation!=animation) s->session->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)animation,0,0};
                if(s->session->door_tick>=31) { s->session->door_tick=0;s->session->travel_tick=1; } // 11

            }
            return true;
        }
        if(s->session->travel_tick) {
            if(++s->session->travel_tick==11) {
                if(!load_area(s,s->session->travel_exit.target,error,error_size)) return false;
                s->session->intro.area=s->session->area;
                memcpy(s->session->intro.player.position,s->session->travel_exit.entry,sizeof(s->session->intro.player.position));
                HmbtnGroundHit ground;
                if(hmbtn_ground_find(&s->scene->terrain,s->session->intro.player.position,0xC080,200,&ground))
                    s->session->intro.player.position[1]=ground.height;
                memcpy(s->session->intro.camera,s->session->intro.player.position,sizeof(s->session->intro.camera));
                s->session->control.mode=s->session->control.blocked_frames=0;
                s->session->intro.player.facing=s->session->area==80 ? 3u : s->session->intro.player.facing;
                s->session->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)(s->session->control.carrying_big ? 41 : s->session->control.carrying ? 27 : 0),(uint16_t)s->session->intro.player.facing,0};
                s->session->house.tv_dirty=s->session->area==80;
                (void)effect(s,(HmbtnRuntimeEffect){.kind=HMBTN_RUNTIME_TV_CLEAR});
                printf("Native area transition: area %u, entry (%d,%d,%d). Session inventory and field retained.\n",s->session->area,
                    (int)s->session->intro.player.position[0],(int)s->session->intro.player.position[1],(int)s->session->intro.player.position[2]);fflush(stdout);
            }
            if(s->session->travel_tick>=21) {
                s->session->travel_tick=0;s->session->control.exit_pending=false;
                if(s->session->area==0 && !s->session->first_visit_started) {
                    s->session->first_visit_started=s->session->first_visit_active=true;s->session->control.enabled=false;
                    if(!load_event(s,12,error,error_size)) return false;
                    s->session->dialogue.step=100;
                    s->session->intro.finish_requested=false;s->session->intro.event_active=true;
                    s->session->intro.visit_approach=true;
                    s->session->intro.mayor.position[0]=-3888;s->session->intro.mayor.position[1]=0;s->session->intro.mayor.position[2]=7895;
                    s->session->intro.mayor.visible=true;s->session->intro.mayor.sprite=(HmbtnAnimationPlayer){1,0,0};
                }
            }
            return true;
        }
        return true;
    }
    case HMBTN_EXP_DOOR: {
            const unsigned event=hmbtn_read_le16(s->section+16+s->session->travel_exit.zone*44+10);
            const int refusal=hmbtn_village_door(&s->session->village,event,&s->session->game.clock);
            if(refusal>=0) {
                s->session->house.mode=HMBTN_HOUSE_BOOK;s->session->house.message_bank=2;s->session->house.request=(unsigned)refusal;
                s->session->house.loaded=0;s->session->house.closing=false;s->session->house.sound=3;
                s->session->house.detail=false;result->opened=true;
            } else {
            hmbtn_area_doors_claim(&s->session->area_doors,s->scene,s->session->door_model);
            s->session->control.exit_pending=true;s->session->door_tick=1;s->session->door_speed=s->session->door_angle=0;
            s->session->door_base_angle=s->scene->angle[s->session->door_model][1];s->session->door_twin=-1;
            if(s->scene->flags[s->session->door_model]&0x400u) {
                const unsigned twin=hmbtn_read_le16(s->section+0x948+s->session->door_model*44+8);
                if(twin>=HMBTN_SCENE_MODELS) { snprintf(error,error_size,"Invalid twin door model");return false; }
                s->session->door_twin=(int)twin;s->session->door_twin_base=s->scene->angle[twin][1];
            }
            if(!key_sound(s,2,1,12,60,127)) {
                snprintf(error,error_size,"Original door opening tone unavailable");return false;
            }
            s->session->intro.player.sprite=(HmbtnAnimationPlayer){(uint16_t)(s->session->control.carrying_big ? 41 : s->session->control.carrying ? 27 : 0),(uint16_t)s->session->intro.player.facing,0};
            if(hmbtn_read_le16(s->section+16+s->session->travel_exit.zone*44)==0x1000) { s->session->door_tick=0;s->session->travel_tick=1; }
            result->consumed=true;return true;
            }
        return true;
    }
    case HMBTN_EXP_SLEEP: {
        if(s->session->house.mode==HMBTN_HOUSE_SLEEP) {
            if(!s->session->sleep.active) {
                hmbtn_poultry_release(&s->session->poultry,&s->session->house,&s->session->intro.player,s->session->area,&s->scene->terrain);
                if(!load_action_bank(s,15,error,error_size)) return false;
                hmbtn_house_sleep_start(&s->session->sleep,&s->session->intro.player); s->session->intro.action_active=true;
            }
            hmbtn_house_sleep_tick(&s->session->sleep,&s->session->intro.player);
            /* scene_link_handback: eight steps to lift, then twelve to settle. */
            s->scene->flags[1]=(uint16_t)((s->scene->flags[1]&~0x1000u)|(s->session->sleep.second_bed ? 0x1000u : 0u));
            s->scene->flags[2]=(uint16_t)((s->scene->flags[2]&~0x1000u)|(s->session->sleep.second_bed ? 0u : 0x1000u));
            s->scene->models[1].morph_steps=8;
            s->scene->models[1].morph_step=s->session->sleep.second_bed ? 8 : s->session->sleep.link_step;
            s->scene->models[2].morph_steps=12;
            s->scene->models[2].morph_step=s->session->sleep.link_state==2 ? s->session->sleep.link_step : 0;
            s->session->intro.music_volume=s->session->intro.music_volume*(30-s->session->sleep.fade)/30;
            if(s->session->sleep.boundary && !morning(s,true,error,error_size)) return false;
        }
        return true;
    }
    default:snprintf(error,error_size,"Unknown MA-07 exploration boundary");return false;
    }
}
static bool morning(HmbtnSessionFrame *s,bool slept,char *error,size_t error_size)
{
    if(!s->session->morning_prepared) hmbtn_session_morning(s->session,slept);
    if(!load_area(s,80,error,error_size)) return false;
    s->session->intro.area=80;s->session->intro.camera_actor=0;s->session->intro.camera_gliding=false;
    memcpy(s->session->intro.camera,s->session->intro.player.position,sizeof(s->session->intro.camera));
    s->session->house.tv_dirty=true;(void)effect(s,(HmbtnRuntimeEffect){.kind=HMBTN_RUNTIME_TV_CLEAR});
    if(!farm_song(s,hmbtn_world_song_weather(s->section,16,s->session->game.clock.season,s->session->game.clock.hour,s->session->game.weather.today),error,error_size)) return false;
    (void)effect(s,(HmbtnRuntimeEffect){.kind=HMBTN_RUNTIME_START_MUSIC});s->session->intro.stop_music=false;s->session->intro.music_volume=0;
    printf("Morning: year %u, season %u, day %u at %02u:%02u; session state retained.\n",
        s->session->game.clock.year+1,s->session->game.clock.season,s->session->game.clock.day+1,s->session->game.clock.hour,s->session->game.clock.minute);
    hmbtn_session_morning_ready(s->session);
    return true;
}
static bool opening_tick(HmbtnSessionFrame *s, uint32_t held, char *error, size_t error_size)
{
    const HmbtnLogicalInput input={s->input.pressed,held};
    s->input.pressed=input.pressed;held=input.held;
    service_animations(s);
    /* The source's global fade keeps running after FARM_REQUEST_FINISH. */
    if(s->session->completion_reported && s->session->intro.script.finished && s->session->intro.fade_mode==1 && s->session->intro.fade_level) --s->session->intro.fade_level;
    if(s->session->morning_fade) {
        --s->session->morning_fade;s->session->intro.music_volume=world_music_volume(s)*(45-s->session->morning_fade)/45;
        goto opening_done;
    }
    if(s->session->overnight_tick) {
        if(++s->session->overnight_tick>=45 && !morning(s,false,error,error_size)) return false;
        goto opening_done;
    }
    if(s->session->completion_reported && s->session->control.enabled && s->session->area==80 && !s->session->animal_event && !s->session->visit.active
            && !s->session->intro.fade_level
            && !s->session->travel_tick && !s->session->door_tick && !s->session->sleep.active && !s->session->control.exit_pending
            && s->session->house.mode==HMBTN_HOUSE_FREE && s->session->menu.mode==HMBTN_MENU_NONE && !s->session->carry.phase && !s->session->quick_ticks) {
        unsigned event=hmbtn_poultry_dead(&s->session->poultry,false) || hmbtn_poultry_dead(&s->session->poultry,true) ? 172
            : hmbtn_poultry_hatch_ready(&s->session->poultry,0) || hmbtn_poultry_hatch_ready(&s->session->poultry,1) ? 299 : 0;
        if(event && !animal_event_begin(s,event,error,error_size)) return false;
    }
    if(s->session->completion_reported && s->session->control.enabled && s->session->first_visit_started
            && !s->session->visit.active && !s->session->animal_event && !s->session->intro.message && !s->session->intro.fade_level
            && !s->session->travel_tick && !s->session->door_tick && !s->session->sleep.active && !s->session->control.exit_pending
            && s->session->house.mode==HMBTN_HOUSE_FREE && s->session->menu.mode==HMBTN_MENU_NONE
            && !s->session->carry.phase && !s->session->feed.phase && !s->session->poultry.carry_phase && !s->session->quick_ticks
            && !s->session->talk.active && s->session->feed_shop.phase==HMBTN_SHOP_OFF
            && s->session->event_item.phase==HMBTN_EVENT_ITEM_IDLE && s->session->farm_action.phase==HMBTN_FARM_ACTION_FREE) {
        const unsigned event=hmbtn_farm_event_trigger(&s->session->triggers,&s->session->village,&s->session->game.clock,s->session->game.weather.today,
            s->session->previous_place,s->session->trigger_entry ? 2u : 0u,0,s->session->house.inventory);
        s->session->trigger_entry=false;
        if(event && !visit_begin(s,event,error,error_size)) return false;
    }
    if(s->session->animal_name_active) {
        hmbtn_new_game_tick(&s->session->animal_name,s->input.pressed,held);
        if(!editor_load(s,&s->session->animal_name,error,error_size)) return false;
        if(s->session->animal_name.sound>=0) ui_sound(s,(unsigned)s->session->animal_name.sound);
    }
    /* The tour first leaves through the farm's real north exit. Keep its
     * script cursor suspended while the fade and resource load complete. */
    if(s->session->first_exit_tick) {
        const unsigned tick=++s->session->first_exit_tick;
        s->session->intro.fade_length=24;s->session->intro.fade_mode=0;
        s->session->intro.fade_level=tick;
        if(tick==24) {
            if(!load_area(s,s->session->first_exit.target,error,error_size)) return false;
            s->session->intro.area=s->session->area;
            memcpy(s->session->intro.player.position,s->session->first_exit.entry,sizeof(s->session->intro.player.position));
            memcpy(s->session->intro.camera,s->session->intro.player.position,sizeof(s->session->intro.camera));
            memset(s->session->intro.mayor.velocity,0,sizeof(s->session->intro.mayor.velocity));
            s->session->intro.mayor.visible=false;
            /* The next script command supplies the fade in. */
            s->session->first_exit_tick=0;s->session->intro.zone_exited=true;
        }
        goto opening_done;
    }
    if (!s->session->animal_name_active && s->session->intro.message && s->session->runtime.loaded_message == s->session->intro.message) {
        hmbtn_service_claim(&s->session->services,HMBTN_SERVICE_MESSAGE);
        if((s->session->first_visit_active || s->session->visit.active) && s->session->dialogue.message.choices && s->session->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END) {
            if(s->input.pressed&(HMBTN_PAD_UP|HMBTN_PAD_DOWN)) {
                s->session->dialogue.choice^=1u;ui_sound(s,5);
            }
        }
        const int sound = hmbtn_message_tick(&s->session->dialogue.message,s->input.pressed,held);
        if (sound >= 0) ui_sound(s,(unsigned)sound);
        if(!effect(s,(HmbtnRuntimeEffect){.kind=HMBTN_RUNTIME_PORTRAIT,.data=&s->session->dialogue.message})) return false;
        s->session->intro.text_bank=s->session->dialogue.message.bank;
        if (s->session->dialogue.message.phase == HMBTN_MESSAGE_CLOSED) {
            const HmbtnServiceResult *r=hmbtn_service_poll(&s->session->services,HMBTN_SERVICE_MESSAGE);
            if(r->status==HMBTN_SERVICE_PENDING && !hmbtn_service_complete(&s->session->services,r->ticket,
                    s->session->dialogue.message.choices ? s->session->dialogue.choice+1 : 0)) {
                snprintf(error,error_size,"Cannot complete active message request");return false;
            }
        }
    }
    (void)effect(s,(HmbtnRuntimeEffect){.kind=HMBTN_RUNTIME_XA_TICK});
    if(s->session->services.channels[HMBTN_SERVICE_AUDIO].request.kind==HMBTN_SERVICE_WAIT_XA)
        (void)effect(s,(HmbtnRuntimeEffect){.kind=HMBTN_RUNTIME_AUDIO_POLL});
    hmbtn_event_item_tick(&s->session->event_item,&s->session->intro.player,s->session->house.inventory,&s->session->farm_action);
    if(s->session->event_item.phase==HMBTN_EVENT_ITEM_IDLE && !s->session->carry.phase) s->session->control.carrying=s->session->house.inventory->held_item.id!=0;
    if(s->session->event_item.sound>=0 && s->session->event_item.phase!=HMBTN_EVENT_ITEM_IDLE) {
        const HmbtnFarmSound *fx=hmbtn_farm_sound((unsigned)s->session->event_item.sound);
        if(!fx || !key_sound(s,2,fx->program,fx->tone,fx->note,fx->volume)) return false;
        s->session->event_item.sound=-1;
    }
    const unsigned previous = s->session->intro.script.pc;
    const bool visit_paused=s->session->visit.active && (s->session->menu.mode!=HMBTN_MENU_NONE || s->session->talk.active
        || s->session->house.mode!=HMBTN_HOUSE_FREE || s->session->feed_shop.phase!=HMBTN_SHOP_OFF || s->session->travel_tick || s->session->door_tick);
    if (!visit_paused && !hmbtn_session_event_tick(s->session,error,error_size)) return false;
    if(s->session->visit.active) {
        const bool enabled=s->session->visit.free && !s->session->intro.message;
        if(s->session->control.enabled && !enabled) {
            s->session->control.mode=0;s->session->intro.player.pose=false;
            s->session->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->session->intro.player.facing,0};
        }
        s->session->control.enabled=enabled;
    }
    if(s->session->first_visit_active && s->session->area==0 && !s->session->intro.zone_exited
            && s->session->intro.player.velocity[2]>0 && hmbtn_house_exit_at(s->section,
                s->scene->terrain.size+0x2000,s->session->intro.player.position,&s->session->first_exit)) {
        s->session->first_exit_tick=1;s->session->intro.fade_mode=0;s->session->intro.fade_length=24;s->session->intro.fade_level=1;
        memset(s->session->intro.player.velocity,0,sizeof(s->session->intro.player.velocity));
    }
    if (s->session->intro.script.pc != previous) {
        printf("Opening event 0x%02x step: %u\n",s->session->intro.event,s->session->intro.script.pc); fflush(stdout);
    }
    if (s->session->intro.area_pending) {
        if (!load_area(s,s->session->intro.target_area,error,error_size)) return false;
        s->session->intro.area = s->session->area; s->session->intro.area_pending = false;
        const unsigned song = s->session->area == 230 ? 32u : s->session->area == 231 || s->session->area == 232 ? 33u : s->session->runtime.audio_song;
        if (song != s->session->runtime.audio_song) {
            if(!s->song(s->context,song,false,error,error_size)) return false;
            s->session->runtime.audio_song=song;
            (void)effect(s,(HmbtnRuntimeEffect){.kind=HMBTN_RUNTIME_START_MUSIC});s->session->intro.stop_music=false;
        }
    }
    if (s->session->intro.xa_pending) {
        const HmbtnServiceResult *r=service_audio(s,HMBTN_SERVICE_START_XA,s->session->intro.xa_track,error,error_size);
        if(!r) return false;
        if(r->status==HMBTN_SERVICE_PENDING) goto opening_done;
        hmbtn_service_ack(&s->session->services,r->ticket);
        s->session->intro.xa_pending=false;s->session->intro.xa_started=s->session->intro.xa_active=true;
    }
    if (s->session->intro.action_pending) {
        const unsigned row=s->session->animal_event==172 ? 4u : 12u;
        const HmbtnServiceResult *r=service_resource(s,HMBTN_SERVICE_ACTION_DATA,row,0,error,error_size);
        if(!r) return false;
        if(r->status==HMBTN_SERVICE_PENDING) goto opening_done;
        if(!load_action_bank(s,row,error,error_size)) return false;
        s->session->intro.action_pending=false;s->session->intro.action_ready=true;
    }
    if (s->session->intro.song_pending) {
        const unsigned song=s->session->animal_event==172 ? 10 : 34;
        const HmbtnServiceResult *r=service_audio(s,HMBTN_SERVICE_LOAD_SONG,song,error,error_size);
        if(!r) return false;
        if(r->status==HMBTN_SERVICE_PENDING) goto opening_done;
        hmbtn_service_ack(&s->session->services,r->ticket);
        s->session->runtime.audio_song=s->session->intro.music_mode=song;s->session->intro.song_pending=false;
    }
    if (s->session->intro.start_music) {
        const HmbtnServiceResult *r=service_audio(s,HMBTN_SERVICE_START_SONG,s->session->runtime.audio_song,error,error_size);
        if(!r) return false;
        if(r->status==HMBTN_SERVICE_PENDING) goto opening_done;
        hmbtn_service_ack(&s->session->services,r->ticket);
        s->session->intro.start_music=s->session->intro.stop_music=false;
        s->session->intro.music_volume=127;s->session->intro.music_time=s->session->intro.music_length=0;
    }
    if (s->session->intro.event_pending) {
        const HmbtnServiceResult *r=service_resource(s,HMBTN_SERVICE_EVENT_DATA,s->session->intro.queued_event,0,error,error_size);
        if(!r) return false;
        if(r->status==HMBTN_SERVICE_PENDING) goto opening_done;
        if(!load_event(s,s->session->intro.queued_event,error,error_size)) return false;
    }
    if (s->session->intro.script.boundary) {
        printf("Opening boundary: %s. Escape closes the window.\n",error); fflush(stdout);
        error[0] = '\0';
    }
    if(s->session->animal_event && s->session->intro.script.finished && s->session->intro.finish_requested) {
        hmbtn_session_release_event_actors(s->session);
        s->session->animal_event=0;s->session->intro.game_command=NULL;s->session->intro.game_context=NULL;
        s->session->intro.carter.visible=s->session->intro.barley.visible=s->session->intro.rick.visible=false;
        s->session->intro.action_active=false;s->session->intro.action_ready=false;s->session->intro.player.pose=false;
        s->session->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->session->intro.player.facing,0};
        s->session->control.enabled=true;s->session->control.exit_pending=false;
        hmbtn_session_advance_minutes(s->session,s->session->intro.advanced_minutes);
        s->session->intro.advanced_minutes=0;hmbtn_daylight_weather_select(&s->session->daylight,&s->session->game.clock,s->session->game.weather.today);
        s->session->menu.stats.animals[0]=hmbtn_poultry_count(&s->session->poultry);
        if(!farm_song(s,hmbtn_world_song_weather(s->section,16,s->session->game.clock.season,s->session->game.clock.hour,s->session->game.weather.today),error,error_size)) return false;
        s->session->intro.stop_music=false;s->session->intro.music_volume=world_music_volume(s);s->session->runtime.house_music_ticks=100;
    }
    if(s->session->visit.active && s->session->intro.script.finished && s->session->intro.finish_requested) {
        hmbtn_session_finish_visit(s->session);
        s->session->control.enabled=true;
        s->session->control.carrying=s->session->house.inventory->held_item.id!=0;
    }
    if(s->session->first_visit_active && s->session->intro.script.finished && s->session->intro.finish_requested) {
        hmbtn_session_release_event_actors(s->session);
        s->session->first_visit_active=false;s->session->control.enabled=true;s->session->control.exit_pending=false;
        s->session->intro.mayor.visible=s->session->intro.ellen.visible=s->session->intro.zack.visible=false;
        s->session->intro.player.sprite=(HmbtnAnimationPlayer){0,(uint16_t)s->session->intro.player.facing,0};
        s->session->intro.player_marker=false;
        hmbtn_session_advance_minutes(s->session,s->session->intro.advanced_minutes);
        hmbtn_daylight_weather_select(&s->session->daylight,&s->session->game.clock,s->session->game.weather.today);
        s->session->intro.music_volume=hmbtn_farm_music_volume(s->section,16);
        s->session->runtime.house_music_ticks=100;
        puts("First morning complete: Thomas/Zack encounter; farm control restored.");
    }
    if (s->session->intro.script.finished && s->session->intro.finish_requested && !s->session->completion_reported) {
        /* Weather ambience replaces the normal song in the same native sampler. */
        const unsigned song=hmbtn_world_song_weather(s->section,16,s->session->game.clock.season,s->session->game.clock.hour,s->session->game.weather.today);
        if(!farm_song(s,song,error,error_size)) return false;
        if(!effect(s,(HmbtnRuntimeEffect){.kind=HMBTN_RUNTIME_READY})) return false;
        s->session->runtime.audio_song=song;
        (void)effect(s,(HmbtnRuntimeEffect){.kind=HMBTN_RUNTIME_START_MUSIC});
        s->session->intro.stop_music=false;s->session->intro.music_volume=0;s->session->control.enabled=true;
        if(!hmbtn_session_bind_schedules(s->session,s->schedules,s->schedule_size)) return false;
        s->session->intro.action_active=false; s->session->intro.player.pose=false;
        puts("Opening complete: farmhouse interior (area 80). Movement, house/farm interactions, clock/calendar and day/night enabled. Remaining world systems are pending; native saves require a stable gameplay state.");
        fflush(stdout); s->session->completion_reported = true;
    }
    HmbtnExplorationFrame exploration=exploration_frame(s);
    if(!hmbtn_session_exploration_controls(s->session,&exploration,(HmbtnLogicalInput){s->input.pressed,held},error,error_size)) return false;
    if(s->session->completion_reported) {
        unsigned changes;bool held_clock;
        exploration=exploration_frame(s);
        if(!hmbtn_session_exploration_world(s->session,&exploration,&changes,&held_clock,error,error_size)) return false;
        if(!s->session->first_visit_active && !s->session->animal_event && !s->session->sleep.active && !s->session->visit.active) {
            const unsigned song=hmbtn_world_song_weather(s->section,16,s->session->game.clock.season,s->session->game.clock.hour,s->session->game.weather.today);
            if(!farm_song(s,song,error,error_size)) return false;
        }
    }
    if(s->session->control.enabled && !s->session->sleep.active && !s->session->morning_fade && !s->session->overnight_tick) {
        if(s->session->runtime.house_music_ticks<100) ++s->session->runtime.house_music_ticks;
        s->session->intro.music_volume=s->session->runtime.house_music_ticks*world_music_volume(s)/100;
    }
    if (s->session->intro.message && s->session->runtime.loaded_message != s->session->intro.message) {
        const unsigned bank=(s->session->first_visit_active || s->session->animal_event || s->session->visit.active) ? s->session->intro.message_bank : 1;
        const HmbtnServiceResult *resource=service_resource(s,HMBTN_SERVICE_TEXT_DATA,s->session->intro.message,bank,error,error_size);
        if(!resource) return false;
        if(resource->status==HMBTN_SERVICE_PENDING) goto opening_done;
        if(s->session->first_visit_active || s->session->animal_event || s->session->visit.active) {
            uint8_t record[8192];uint16_t words[1024];unsigned count=0;
            if(!service_read(s,HMBTN_SERVICE_TEXT_DATA,s->session->intro.message,s->session->intro.message_bank,
                    record,sizeof(record),NULL,error,error_size)) return false;
            if(s->session->animal_event==172 && !hmbtn_poultry_expand_dead(&s->session->poultry,record,sizeof(record))) {
                snprintf(error,error_size,"Funeral names exceed message capacity");return false;
            }
            for(unsigned i=0;i<1024 && count<1024;++i) {
                uint16_t c=hmbtn_read_le16(record+0x1800+i*2);
                const uint16_t *name=c==0x400a ? s->session->game.registration.player_name : c==0x400c ? s->session->game.registration.dog_name : c==0x400d ? s->session->game.registration.farm_name : NULL;
                if(name) { for(unsigned j=0;j<8 && name[j]!=0xffff && count<1023;++j) words[count++]=name[j]; }
                else words[count++]=c;
                if(c==0xffff) break;
            }
            if(!count || words[count-1]!=0xffff) { snprintf(error,error_size,"First morning message expansion overflow");return false; }
            for(unsigned i=0;i<count;++i) hmbtn_write_le16(record+0x1800+i*2,words[i]);
            if(!message_record(s,record,sizeof(record),&s->session->dialogue.message,error,error_size)) return false;
        } else {
            uint8_t record[HMBTN_TEXT_RECORD_BYTES];
            if(!service_read(s,HMBTN_SERVICE_TEXT_DATA,s->session->intro.message,1,record,sizeof(record),NULL,error,error_size)
                    || !message_record(s,record,sizeof(record),&s->session->dialogue.message,error,error_size)) return false;
        }
        hmbtn_service_claim(&s->session->services,HMBTN_SERVICE_MESSAGE);
        s->session->runtime.loaded_message = s->session->intro.message;
        if(s->session->first_visit_active || s->session->visit.active) { s->session->dialogue.choice=0;s->session->dialogue.message.suppress_cursor=s->session->dialogue.message.choices!=0; }
        printf("Opening message: 0x%03x\n",s->session->runtime.loaded_message); fflush(stdout);
    }
    if (!s->session->intro.message) {
        s->session->runtime.loaded_message = 0;
        exploration=exploration_frame(s);
        if(!hmbtn_session_exploration_animate(s->session,&exploration,error,error_size)) return false;
        if(s->session->menu.mode==HMBTN_MENU_NONE) for (unsigned id = 1; id < 35; ++id) {
            if(s->session->visit.event==289 && s->session->village.event_actors[id]) continue;
            HmbtnIntroActor *actor = hmbtn_farm_intro_actor(&s->session->intro,(int32_t)id);
            const HmbtnAnimation *animation=s->animation(s->context,id,false);
            if (actor && actor->visible && animation && !hmbtn_actor_animation_tick(animation,actor,error,error_size)) return false;
        }
        if(s->session->visit.active && s->session->visit.event==289 && s->session->menu.mode==HMBTN_MENU_NONE)
            for(unsigned id=1;id<HMBTN_VILLAGERS;++id) if(s->session->village.event_actors[id] && s->session->visit.actors[id].visible) {
                const HmbtnAnimation *animation=s->animation(s->context,id,true);
                if(!animation || !hmbtn_actor_animation_tick(animation,&s->session->visit.actors[id],error,error_size)) return false;
            }
    }
    if (s->session->intro.cell_actor) {
        const HmbtnIntroActor *cell = hmbtn_farm_intro_actor(&s->session->intro,(int32_t)s->session->intro.cell_actor);
        if (cell && s->session->runtime.loaded_cell != cell->sprite.animation) {
            s->session->runtime.loaded_cell = cell->sprite.animation;
            printf("Opening memory image: %u\n",s->session->runtime.loaded_cell); fflush(stdout);
        }
    }
opening_done:
    service_animations(s);
    if(s->session->control.enabled && !s->session->sleep.active)
        hmbtn_world_camera(s->section,s->scene->terrain.size+0x2000,s->session->intro.camera);
    (void)effect(s,(HmbtnRuntimeEffect){.kind=HMBTN_RUNTIME_VOLUME,.id=s->session->intro.music_volume});
    if (s->session->intro.stop_music) (void)effect(s,(HmbtnRuntimeEffect){.kind=HMBTN_RUNTIME_STOP_MUSIC});
    ++s->session->runtime.ticks;
    return true;
}

static HmbtnExplorationFrame exploration_frame(HmbtnSessionFrame *s)
{
    HmbtnExplorationFrame f=s->exploration;
    f.section=s->section;f.ground=&s->scene->terrain;f.scene=s->scene;f.section_size=s->scene->terrain.size+0x2000;
    f.boundary=exploration_boundary;f.boundary_context=s;return f;
}
bool hmbtn_session_tick(HmbtnSession *session,HmbtnSessionFrame *frame,HmbtnLogicalInput input,char *error,size_t size)
{
    if(!session || !frame || !frame->scene || !frame->section || session->active_frame) { snprintf(error,size,"Missing session frame resources");return false; }
    frame->session=session;frame->input=input;frame->error=error;frame->error_size=size;session->active_frame=frame;
    const bool result=opening_tick(frame,input.held,error,size);
    session->active_frame=NULL;return result;
}
bool hmbtn_session_start_visit(HmbtnSession *session,HmbtnSessionFrame *frame,unsigned id,char *error,size_t size)
{
    frame->session=session;frame->error=error;frame->error_size=size;return visit_begin(frame,id,error,size);
}
bool hmbtn_session_prepare_morning(HmbtnSession *session,HmbtnSessionFrame *frame,bool slept,char *error,size_t size)
{
    frame->session=session;frame->error=error;frame->error_size=size;return morning(frame,slept,error,size);
}

void hmbtn_session_runtime_rebind(HmbtnSession *session)
{
    if(session->visit.active) session->intro.game_command=visit_command;
    else if(session->animal_event) session->intro.game_command=animal_command;
    else return;
    session->intro.game_context=session;
}
HmbtnEventResult hmbtn_session_visit_command(HmbtnSession *session,HmbtnSessionFrame *frame,const HmbtnEventCommand *c,char *error,size_t size)
{
    frame->session=session;frame->error=error;frame->error_size=size;session->active_frame=frame;
    const HmbtnEventResult r=visit_command(session,c,error,size);session->active_frame=NULL;return r;
}

void hmbtn_session_area_begin(HmbtnSession *s,unsigned area,unsigned previous,bool has_previous)
{
    if(has_previous) s->previous_place=previous;
    if(s->visit.active && s->area==s->visit.start_area && area!=s->area) {
        s->intro.zone_exited=true;
        if(s->visit.event==130) s->intro.elli.visible=s->intro.ellen.visible=s->intro.stu.visible=false;
        if(s->visit.event==289) for(unsigned id=1;id<HMBTN_VILLAGERS;++id) s->visit.actors[id].visible=false;
    }
    s->triggers=(HmbtnFarmTriggers){0};s->trigger_entry=true;
}
void hmbtn_session_area_ready(HmbtnSession *s,unsigned area,const HmbtnSceneData *scene)
{
    s->area_doors=(HmbtnAreaDoors){0};
    s->area=s->carry.area=area;s->intro.ground=&scene->terrain;s->refused_zone=-1;
    if(s->village_ready) hmbtn_village_enter(&s->village);
    s->village.flags[0x5E]=false;s->fishing.streak=0;
    hmbtn_daylight_weather_select(&s->daylight,&s->game.clock,s->game.weather.today);
    if(area!=0) memset(s->field_effects,0,sizeof(s->field_effects));
    else s->field.season=s->game.clock.season;
    s->intro.camera_height=-4500;s->intro.camera_distance=-9000;
}
