/* Installed scene/title resources, resized and fullscreen presentation, and
 * audio refill without simulation ticks. Timing is diagnostic, not a CI limit. */
#include "poultry_helpers.h"
#include "pause_menu.h"
#include "title_preview.h"
static bool audio_between_ticks(HmbtnScenePreview *s,char *error,size_t size)
{
    HmbtnSession *g=hmbtn_scene_preview_session(s);
    HmbtnTitleAudio *audio=hmbtn_scene_preview_resources(s).audio;REQUIRE(audio->stream);
    const unsigned ticks=g->runtime.ticks;const HmbtnFarmClock clock=g->game.clock;
    const HmbtnIntroActor player=g->intro.player;const HmbtnDog dog=g->dog;
    /* Stop only the test device so queue depletion/refill can be observed
     * deterministically, without a race with the dummy playback callback. */
    REQUIRE(SDL_PauseAudioStreamDevice(audio->stream));
    for(unsigned i=0;i<8;++i) {
        REQUIRE(SDL_ClearAudioStream(audio->stream) && hmbtn_scene_preview_pump_audio(s,error,size));
        const int queued=SDL_GetAudioStreamQueued(audio->stream);
        REQUIRE(queued>=3072*4 && queued<=3584*4);
        REQUIRE(hmbtn_scene_preview_pump_audio(s,error,size) && SDL_GetAudioStreamQueued(audio->stream)==queued);
    }
    REQUIRE(g->runtime.ticks==ticks && !memcmp(&clock,&g->game.clock,sizeof(clock))
        && !memcmp(&player,&g->intro.player,sizeof(player)) && !memcmp(&dog,&g->dog,sizeof(dog)));
    REQUIRE(hmbtn_scene_preview_preferences(s,true,40,false) && SDL_ClearAudioStream(audio->stream)
        && hmbtn_scene_preview_pump_audio(s,error,size) && !SDL_GetAudioStreamQueued(audio->stream));
    REQUIRE(hmbtn_scene_preview_preferences(s,false,100,false)
        && hmbtn_scene_preview_pump_audio(s,error,size));return true;
}
static bool present(HmbtnScenePreview *s,HmbtnHost *host,HmbtnPauseMenu *menu,HmbtnPreferences *p,
    bool paused,const char *label,char *error,size_t size)
{
    Uint64 render_ns=0,slowest=0;const unsigned ticks=hmbtn_scene_preview_session(s)->runtime.ticks;
    REQUIRE(hmbtn_scene_preview_preferences(s,paused,100,false));
    for(unsigned frame=0;frame<32;++frame) {
        SDL_PumpEvents();
        if(!paused && frame%2==0) REQUIRE(step(s,0,error,size));
        REQUIRE(hmbtn_scene_preview_pump_audio(s,error,size));
        const Uint64 start=SDL_GetTicksNS();
        REQUIRE(hmbtn_scene_preview_draw(s,host->renderer,error,size));
        if(paused) REQUIRE(hmbtn_pause_menu_draw(menu,p,hmbtn_scene_preview_state(s),host->renderer));
        /* Present flushes commands; include it so software/GPU comparisons
         * include real scaling/blending instead of measuring queue insertion. */
        REQUIRE(SDL_RenderPresent(host->renderer));const Uint64 duration=SDL_GetTicksNS()-start;
        render_ns+=duration;if(duration>slowest) slowest=duration;
        REQUIRE(hmbtn_scene_preview_pump_audio(s,error,size));
    }
    REQUIRE(hmbtn_scene_preview_session(s)->runtime.ticks==ticks+(paused ? 0u : 16u));
    int w,h;REQUIRE(SDL_GetRenderOutputSize(host->renderer,&w,&h));
    printf("Display %s %dx%d renderer=%s paused=%u: present mean %.2f ms, max %.2f ms\n",
        label,w,h,SDL_GetRendererName(host->renderer),paused,(double)render_ns/32000000.0,(double)slowest/1000000.0);
    return true;
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    HmbtnHost host={0};HmbtnScenePreview *s=NULL;char error[512]={0};HmbtnPauseMenu menu={.binding=-1};
    CHECK(hmbtn_host_open(&host));HmbtnPreferences prefs;hmbtn_preferences_defaults(&prefs);
    const HmbtnFarmClock date={0,0,4,4,10,0,0};
    s=hmbtn_scene_area_create_at(host.renderer,argv[1],0,&date,error,sizeof(error));CHECK(s);
    for(unsigned i=0;i<110;++i) CHECK(step(s,0,error,sizeof(error)));
    CHECK(audio_between_ticks(s,error,sizeof(error)));hmbtn_pause_menu_open(&menu,NULL,NULL);
    const int sizes[][2]={{640,480},{1920,1080},{2560,1440}};
    for(unsigned i=0;i<SDL_arraysize(sizes);++i) {
        CHECK(SDL_SetWindowSize(host.window,sizes[i][0],sizes[i][1]));SDL_PumpEvents();
        CHECK(present(s,&host,&menu,&prefs,false,"window",error,sizeof(error)));
        menu.page=HMBTN_PAUSE_CONTROLS;
        CHECK(present(s,&host,&menu,&prefs,true,"window",error,sizeof(error)));
    }
    CHECK(SDL_SetWindowFullscreen(host.window,true) && SDL_SyncWindow(host.window));SDL_PumpEvents();
    CHECK(present(s,&host,&menu,&prefs,true,"fullscreen",error,sizeof(error)));
    CHECK(SDL_SetWindowFullscreen(host.window,false) && SDL_SyncWindow(host.window));SDL_PumpEvents();
    CHECK(present(s,&host,&menu,&prefs,false,"restored",error,sizeof(error)));
    hmbtn_pause_menu_close(&menu);hmbtn_scene_preview_destroy(s);s=NULL;
    HmbtnTitlePreview *title=hmbtn_title_preview_create(host.renderer,argv[1],error,sizeof(error));CHECK(title);
    HmbtnTitleFlow before=*hmbtn_title_preview_inspect(title).flow;
    CHECK(hmbtn_title_preview_pump_audio(title,error,sizeof(error))
        && !memcmp(&before,hmbtn_title_preview_inspect(title).flow,sizeof(before)));
    hmbtn_title_preview_destroy(title);hmbtn_host_close(&host);
    puts("Display: resizing/fullscreen, paused simulation, bounded audio refill between ticks and independent title audio passed");return 0;
fail:hmbtn_pause_menu_close(&menu);hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);return 1;
}
