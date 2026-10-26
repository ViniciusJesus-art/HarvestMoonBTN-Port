/* Installed scripts/resources: replace host loans during an active visitor,
 * destroy another active session, then finish and resume the resident route. */
#include "scene_fixture.h"
#include "sdl_host.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Session events integration line %d: %s; %s\n",__LINE__,#x,error);goto fail; } } while(0)
static bool step(HmbtnScenePreview *s,uint32_t pressed,char *error,size_t size)
{
    return hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){pressed,pressed},error,size);
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL,*other=NULL;uint8_t *replacement=NULL;char error[512]={0};
    CHECK(hmbtn_host_open(&host));
    const HmbtnFarmClock clock={0,0,14,0,6,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(s);
    other=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&clock,error,sizeof(error));CHECK(other);
    HmbtnScenePreview *scenes[]={s,other};
    for(unsigned n=0;n<2;++n) {
        HmbtnScenePreview *v=scenes[n];
        CHECK(step(v,0,error,sizeof(error)));
        for(unsigned id=1;id<HMBTN_VILLAGERS;++id) { hmbtn_scene_preview_session(v)->village.people[id].friendship=100;hmbtn_scene_preview_session(v)->village.people[id].affection=5000; }
        CHECK(step(v,0,error,sizeof(error)) && hmbtn_scene_preview_session(v)->visit.active && hmbtn_scene_preview_session(v)->visit.event==288);
    }
    HmbtnSession *session=hmbtn_scene_preview_session(s);
    CHECK(session!=hmbtn_scene_preview_session(other) && session->intro.script.data==session->event_code);
    CHECK(session->village.player==&session->intro.player);
    HmbtnIntroActor *actor=&session->intro.mayor;
    CHECK(hmbtn_village_actor(&session->village,17)==actor);
    CHECK(hmbtn_village_actor(&hmbtn_scene_preview_session(other)->village,17)==&hmbtn_scene_preview_session(other)->intro.mayor);
    /* Keep the pending request/cursor and authoritative actor through resource
     * reattachment; adapters are explicit loans, not another execution copy. */
    for(unsigned i=0;i<1500 && !session->intro.message;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(session->intro.message && !session->intro.script.boundary);
    const unsigned pc=session->intro.script.pc,message=session->intro.message;
    const int32_t position[3]={actor->position[0],actor->position[1],actor->position[2]};
    replacement=malloc(0xA800);CHECK(replacement);memcpy(replacement,hmbtn_scene_preview_resources(s).schedules,0xA800);
    hmbtn_session_unbind_resources(session);
    CHECK(!session->intro.ground && !session->intro.game_command && !session->intro.game_context);
    CHECK(session->intro.script.pc==pc && session->intro.message==message && session->intro.script.data==session->event_code);
    CHECK(hmbtn_village_actor(&session->village,17)==actor && session->village.player==&session->intro.player);
    CHECK(hmbtn_scene_preview_rebind(s,replacement,0xA800,error,sizeof(error)));
    free(replacement);replacement=NULL;
    CHECK(actor->position[0]==position[0] && actor->position[1]==position[1] && actor->position[2]==position[2]);
    CHECK(session->intro.script.pc==pc && session->intro.message==message);
    CHECK(hmbtn_scene_preview_draw(s,host.renderer,error,sizeof(error)));
    hmbtn_scene_preview_destroy(other);other=NULL;
    CHECK(hmbtn_village_actor(&session->village,17)==actor && session->intro.script.pc==pc && session->intro.message==message);
    /* Choose No in the original request and let its real script complete. */
    bool selected=false,spoke=false;
    for(unsigned i=0;i<7000 && session->visit.active;++i) {
        uint32_t pressed=session->intro.message ? HMBTN_PAD_CROSS : 0;
        if(!selected && session->dialogue.message.choices && session->dialogue.message.phase==HMBTN_MESSAGE_WAIT_END) {
            pressed=HMBTN_PAD_DOWN;selected=true;
        }
        if(!spoke && hmbtn_scene_preview_session(s)->control.enabled && session->visit.free && !session->intro.message && actor->position[2]<=3959) {
            memcpy(session->intro.player.position,actor->position,sizeof(actor->position));
            session->intro.player.position[2]-=300;session->intro.player.facing=2;hmbtn_scene_preview_session(s)->control.mode=0;
            CHECK(hmbtn_village_target(&session->village,&session->intro.player)==17);
            pressed=HMBTN_PAD_CROSS;spoke=true;
        }
        CHECK(step(s,pressed,error,sizeof(error)) && !session->intro.script.boundary);
    }
    CHECK(selected && spoke && !session->visit.active && hmbtn_scene_preview_session(s)->control.enabled);
    CHECK(!session->village.event_actor && !session->village.event_actor_id && !session->intro.mayor.visible);
    for(unsigned id=1;id<HMBTN_VILLAGERS;++id) CHECK(!session->village.event_actors[id]);
    CHECK(hmbtn_village_actor(&session->village,17)==&session->village.people[17].actor);
    /* Real route re-placement after the event; moving areas keeps the record. */
    HmbtnVillager *resident=&session->village.people[17];const unsigned friendship=resident->friendship;
    session->game.clock.hour=9;session->game.clock.minute=0;
    CHECK(hmbtn_village_sync(&session->village,&session->game.clock,error,sizeof(error)));
    unsigned target=UINT32_MAX;
    for(unsigned area=0;area<216;area+=4) {
        unsigned logical,resource;uint8_t header[16];
        if(!hmbtn_world_area(area,session->game.clock.season,&logical,&resource)) continue;
        unsigned location;
        CHECK(hmbtn_scene_preview_area_location(s,area,&location,error,sizeof(error)));
        if(location==resident->location) { target=logical;break; }
    }
    CHECK(target!=UINT32_MAX && resident->location!=54);
    CHECK(hmbtn_scene_preview_load_area(s,target,error,sizeof(error)));session->intro.area=hmbtn_scene_preview_session(s)->area;
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(&session->village.people[17]==resident && resident->friendship==friendship);
    CHECK(hmbtn_village_actor(&session->village,17)==&resident->actor);
    CHECK(resident->placed && resident->actor.visible);
    CHECK(hmbtn_scene_preview_draw(s,host.renderer,error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    puts("Active session event survives host-resource/UI replacement and another event's destruction; original script exits and route resumes.");return 0;
fail:
    free(replacement);hmbtn_scene_preview_destroy(other);hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}
