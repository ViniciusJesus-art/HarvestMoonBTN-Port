/* Drive the real application/title with SDL events, without private renderer
 * access or a production-only test hook. Watch the actual created window ID. */
#include "app.h"
#include <SDL3/SDL.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
static SDL_AtomicInt window_id;
static bool SDLCALL watch(void *unused,SDL_Event *event)
{
    (void)unused;
    if(event->type==SDL_EVENT_WINDOW_SHOWN) SDL_SetAtomicInt(&window_id,(int)event->window.windowID);
    return true;
}
static Uint32 SDLCALL press_start(void *unused,SDL_TimerID timer,Uint32 interval)
{
    (void)unused;(void)timer;int id=SDL_GetAtomicInt(&window_id);
    if(id>0) {
        SDL_Event e={0};e.type=SDL_EVENT_KEY_DOWN;e.key.windowID=(SDL_WindowID)id;
        e.key.scancode=SDL_SCANCODE_RETURN;e.key.down=true;SDL_PushEvent(&e);
        e.type=SDL_EVENT_KEY_UP;e.key.down=false;SDL_PushEvent(&e);
    }
    return interval;
}
int main(int argc,char **argv)
{
    if(argc!=3) return 2;
    assert(SDL_Init(SDL_INIT_EVENTS));assert(SDL_AddEventWatch(watch,NULL));
    assert(SDL_AddTimer(100,press_start,NULL));
    HmbtnAppOptions options={.data_root=argv[1],.save_path=argv[2],.frame_limit=250};
    /* host_close calls SDL_Quit, which joins timers before this stack returns. */
    return hmbtn_app_run(&options);
}
