#include "title_preview.h"
#include "sdl_host.h"
#include "scene_inspection.h"
#include "calendar_checks.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"Title update tick %u line %d: %s; %s\n",tick,__LINE__,#x,error);goto fail; } } while(0)
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnTitlePreview *p[3]={0};char error[512]={0};unsigned tick=0;
    CHECK(hmbtn_host_open(&host));
    for(unsigned i=0;i<3;++i) { p[i]=hmbtn_title_preview_create(host.renderer,argv[1],error,sizeof(error));CHECK(p[i]); }
    bool named=false,birthday=false;
    for(;tick<2500 && !hmbtn_title_preview_registration(p[0]);++tick) {
        HmbtnTitleInspection lead=hmbtn_title_preview_inspect(p[0]);
        uint32_t pressed=lead.flow->step==HMBTN_TITLE_START_WAIT ? HMBTN_PAD_START : HMBTN_PAD_CROSS;
        const HmbtnNewGame *g=lead.new_game;
        if(lead.flow->step==HMBTN_TITLE_MENU && lead.flow->menu_row) pressed=HMBTN_PAD_UP;
        else if(hmbtn_new_game_name_editor(g) && g->message.phase==HMBTN_MESSAGE_WAIT_END && !g->queued) {
            named=true;pressed=g->confirming || !g->length ? HMBTN_PAD_CROSS : HMBTN_PAD_START;
        }
        if(g->step==18) birthday=true;
        for(unsigned i=0;i<3;++i) {
            CHECK(hmbtn_title_preview_tick(p[i],(HmbtnLogicalInput){pressed,HMBTN_PAD_CROSS},error,sizeof(error)));
            for(unsigned n=0;n<i;++n) {
                HmbtnTitleInspection before=hmbtn_title_preview_inspect(p[i]);
                const HmbtnTitleFlow flow=*before.flow;const HmbtnNewGame game=*before.new_game;
                const HmbtnGameState state=*before.game;
                HmbtnAnimationPlayer actors[3],columns[3];uint16_t timers[6];
                memcpy(actors,before.actors,sizeof(actors));memcpy(columns,before.columns,sizeof(columns));
                memcpy(timers,before.actor_timers,6);memcpy(timers+3,before.column_timers,6);
                CHECK(hmbtn_title_preview_draw(p[i],host.renderer,error,sizeof(error)));
                HmbtnTitleInspection after=hmbtn_title_preview_inspect(p[i]);
                CHECK(!memcmp(&flow,after.flow,sizeof(flow)) && !memcmp(&game,after.new_game,sizeof(game))
                    && !memcmp(&state,after.game,sizeof(state)) && !memcmp(actors,after.actors,sizeof(actors))
                    && !memcmp(columns,after.columns,sizeof(columns)) && !memcmp(timers,after.actor_timers,6)
                    && !memcmp(timers+3,after.column_timers,6) && before.random_seed==after.random_seed
                    && before.scene_frames==after.scene_frames && before.scroll_a==after.scroll_a && before.scroll_b==after.scroll_b);
            }
        }
        const HmbtnTitleInspection a=hmbtn_title_preview_inspect(p[0]);
        for(unsigned i=1;i<3;++i) {
            const HmbtnTitleInspection b=hmbtn_title_preview_inspect(p[i]);
            CHECK(!memcmp(a.flow,b.flow,sizeof(*a.flow)) && a.new_game->step==b.new_game->step
                && a.new_game->request==b.new_game->request && a.new_game->loaded==b.new_game->loaded
                && !memcmp(&a.new_game->message,&b.new_game->message,sizeof(a.new_game->message))
                && registration_equal(&a.game->registration,&b.game->registration)
                && a.random_seed==b.random_seed && a.scene_frames==b.scene_frames
                && !memcmp(a.actors,b.actors,3*sizeof(*a.actors)) && !memcmp(a.columns,b.columns,3*sizeof(*a.columns)));
        }
    }
    CHECK(tick<2500 && named && birthday);
    const HmbtnRegistration accepted=*hmbtn_title_preview_registration(p[0]);
    for(unsigned i=0;i<3;++i) {
        const HmbtnRegistration *r=hmbtn_title_preview_registration(p[i]);
        CHECK(r && r->player_name[0]==0 && r->farm_name[0]==0 && r->dog_name[0]==0
            && !r->birthday_day && !r->birthday_season);
        hmbtn_title_preview_destroy(p[i]);p[i]=NULL;
    }
    HmbtnScenePreview *scene=hmbtn_scene_intro_create(host.renderer,argv[1],error,sizeof(error));
    CHECK(scene);hmbtn_scene_intro_registration(scene,&accepted);
    const bool transferred=registration_equal(&accepted,&hmbtn_scene_preview_session(scene)->game.registration)
        && hmbtn_scene_preview_tick(scene,(HmbtnLogicalInput){0},error,sizeof(error));
    hmbtn_scene_preview_destroy(scene);CHECK(transferred);
    hmbtn_host_close(&host);puts("Title, original registration, RNG, animation and scroll match at 0/30/60 presentations; draw mutates no logical state.");return 0;
fail:
    for(unsigned i=0;i<3;++i) hmbtn_title_preview_destroy(p[i]);
    hmbtn_host_close(&host);return 1;
}
