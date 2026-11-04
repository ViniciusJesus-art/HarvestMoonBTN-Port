/* Compile the actual app loop, observing its public scene boundary. The host
 * library does not supply app.o because this translation unit defines app_run.
 * No production test hooks or alternate pause loop are used. */
#define hmbtn_scene_preview_tick observed_tick
#define hmbtn_scene_saved_create observed_load
#include "../core/app.c"
#undef hmbtn_scene_preview_tick
#undef hmbtn_scene_saved_create
#include "scene_fixture.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
bool hmbtn_scene_preview_tick(HmbtnScenePreview *,HmbtnLogicalInput,char *,size_t);
HmbtnScenePreview *hmbtn_scene_saved_create(SDL_Renderer *,const char *,const char *,char *,size_t);
static SDL_AtomicInt ticks,loads,last_gold,window_id,stage,failed;
static int frozen;
bool observed_tick(HmbtnScenePreview *s,HmbtnLogicalInput input,char *error,size_t size)
{ SDL_AddAtomicInt(&ticks,1);return hmbtn_scene_preview_tick(s,input,error,size); }
HmbtnScenePreview *observed_load(SDL_Renderer *r,const char *root,const char *path,char *error,size_t size)
{
    HmbtnScenePreview *s=hmbtn_scene_saved_create(r,root,path,error,size);
    if(s) { SDL_AddAtomicInt(&loads,1);SDL_SetAtomicInt(&last_gold,(int)hmbtn_scene_preview_state(s)->game.economy.gold); }
    return s;
}
static bool SDLCALL watch(void *unused,SDL_Event *event)
{
    (void)unused;
    if(event->type==SDL_EVENT_WINDOW_SHOWN) SDL_SetAtomicInt(&window_id,(int)event->window.windowID);
    return true;
}
static void press(SDL_Scancode code)
{
    SDL_Event e={0};e.type=SDL_EVENT_KEY_DOWN;e.key.windowID=(SDL_WindowID)SDL_GetAtomicInt(&window_id);
    e.key.scancode=code;e.key.key=SDL_GetKeyFromScancode(code,SDL_KMOD_NONE,false);e.key.down=true;e.key.repeat=false;assert(SDL_PushEvent(&e));
    e.type=SDL_EVENT_KEY_UP;e.key.down=false;assert(SDL_PushEvent(&e));
}
static Uint32 SDLCALL drive(void *unused,SDL_TimerID timer,Uint32 interval)
{
    (void)unused;(void)timer;
    if(!SDL_GetAtomicInt(&window_id) || !SDL_GetAtomicInt(&ticks)) return interval;
    int at=SDL_GetAtomicInt(&stage);
    if(at==0) press(SDL_SCANCODE_ESCAPE);
    if(at==1) frozen=SDL_GetAtomicInt(&ticks);
    if(at==2) { if(SDL_GetAtomicInt(&ticks)!=frozen) SDL_SetAtomicInt(&failed,1);press(SDL_SCANCODE_ESCAPE); }
    if(at==3) {
        if(SDL_GetAtomicInt(&ticks)<=frozen) SDL_SetAtomicInt(&failed,1);
        /* Farm B is newest; choose farm A, then its older 100 G snapshot. */
        press(SDL_SCANCODE_ESCAPE);press(SDL_SCANCODE_DOWN);press(SDL_SCANCODE_RETURN);
        press(SDL_SCANCODE_DOWN);press(SDL_SCANCODE_RETURN);press(SDL_SCANCODE_DOWN);press(SDL_SCANCODE_RETURN);
    }
    if(at==4) {
        if(SDL_GetAtomicInt(&loads)!=2 || SDL_GetAtomicInt(&last_gold)!=100) SDL_SetAtomicInt(&failed,1);
        press(SDL_SCANCODE_ESCAPE);press(SDL_SCANCODE_DOWN);press(SDL_SCANCODE_DOWN);press(SDL_SCANCODE_RETURN);
        press(SDL_SCANCODE_DOWN);press(SDL_SCANCODE_DOWN);press(SDL_SCANCODE_RETURN);
        press(SDL_SCANCODE_LEFT);press(SDL_SCANCODE_LEFT);
    }
    if(at==5) {
        press(SDL_SCANCODE_ESCAPE);press(SDL_SCANCODE_ESCAPE);
        press(SDL_SCANCODE_DOWN);press(SDL_SCANCODE_DOWN);press(SDL_SCANCODE_DOWN);press(SDL_SCANCODE_RETURN);
    }
    if(at>8) { SDL_Event e={.type=SDL_EVENT_QUIT};SDL_PushEvent(&e);SDL_SetAtomicInt(&failed,1);return 0; }
    SDL_SetAtomicInt(&stage,at+1);return interval;
}
int main(int argc,char **argv)
{
    if(argc!=3) return 2;
    char *base=NULL,*slot=NULL,*root=NULL,*settings=NULL,error[512]={0};HmbtnHost host={0};assert(hmbtn_host_open(&host));
    SDL_asprintf(&base,"%s-%" SDL_PRIu64,argv[2],SDL_GetTicksNS());assert(SDL_CreateDirectory(base));
    SDL_asprintf(&slot,"%s/latest.hms",base);root=hmbtn_save_library_root(slot);SDL_asprintf(&settings,"%s/settings.ini",base);
    HmbtnScenePreview *s=hmbtn_scene_house_create(host.renderer,argv[1],error,sizeof(error));assert(s);
    for(unsigned i=0;i<110;++i) assert(hmbtn_scene_preview_tick(s,(HmbtnLogicalInput){0},error,sizeof(error)));
    HmbtnSession *g=hmbtn_scene_preview_session(s);g->game.economy.gold=100;
    assert(hmbtn_save_library_write(root,slot,g,error,sizeof(error)));SDL_Delay(2);
    g->game.economy.gold=200;assert(hmbtn_save_library_write(root,slot,g,error,sizeof(error)));SDL_Delay(2);
    memset(g->farm_id,0,sizeof(g->farm_id));g->game.economy.gold=300;
    assert(hmbtn_save_library_write(root,slot,g,error,sizeof(error)));
    hmbtn_scene_preview_destroy(s);hmbtn_host_close(&host);
    assert(SDL_Init(SDL_INIT_EVENTS));assert(SDL_AddEventWatch(watch,NULL));assert(SDL_AddTimer(250,drive,NULL));
    HmbtnAppOptions options={.data_root=argv[1],.load_save=true,.save_path=slot,.preferences_path=settings,.frame_limit=240};
    assert(hmbtn_app_run(&options)==0 && SDL_GetAtomicInt(&stage)>=6 && !SDL_GetAtomicInt(&failed));
    HmbtnPreferences prefs;assert(hmbtn_preferences_load(&prefs,settings,error,sizeof(error)) && prefs.volume==90);
    SDL_free(base);SDL_free(slot);SDL_free(root);SDL_free(settings);
    puts("Actual app loop: Esc pause stops all game ticks, Esc resumes, farm/history selection restores old gold, settings persist, menu Exit quits passed");return 0;
}
