#include "pause_menu.h"
#include "sdl_host.h"
#include "game_input.h"
#include "title_audio.h"
#include "save_fixture.h"
#include "scene_preview.h"
#include <stdio.h>
static HmbtnPauseAction key(HmbtnPauseMenu *m,HmbtnPreferences *p,SDL_Renderer *r,SDL_WindowID id,SDL_Scancode scan)
{
    SDL_Event e={0};e.type=SDL_EVENT_KEY_DOWN;e.key.windowID=id;e.key.scancode=scan;e.key.repeat=false;
    return hmbtn_pause_menu_event(m,p,r,id,&e);
}
static void gradient_sizes(HmbtnHost *host,HmbtnPauseMenu *menu,const HmbtnPreferences *p)
{
    const int sizes[][2]={{640,480},{1280,720},{1920,1080},{2560,1440},{3440,1440}};
    const HmbtnPausePage pages[]={HMBTN_PAUSE_HOME,HMBTN_PAUSE_CONTROLS};
    for(unsigned page=0;page<SDL_arraysize(pages);++page) for(unsigned i=0;i<SDL_arraysize(sizes);++i) {
        menu->page=pages[page];menu->selected=menu->top=0;
        int target_w=sizes[i][0],target_h=sizes[i][1];
        if(strcmp(SDL_GetCurrentVideoDriver(),"dummy")) {
            SDL_Rect usable;assert(SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(host->window),&usable));
            /* A desktop WM may reject windows larger than its monitor; the
             * dummy backend still verifies every exact large resolution. */
            target_w=SDL_min(target_w,usable.w-32);target_h=SDL_min(target_h,usable.h-128);
        }
        assert(SDL_SetWindowSize(host->window,target_w,target_h) && SDL_SyncWindow(host->window));SDL_PumpEvents();
        /* A real GL drawable may resize its backbuffer at the first swap,
         * after the window manager has already acknowledged the window size. */
        assert(SDL_SetRenderDrawColor(host->renderer,200,180,160,255)
            && SDL_RenderClear(host->renderer) && SDL_RenderPresent(host->renderer));
        assert(SDL_RenderClear(host->renderer));
        assert(hmbtn_pause_menu_draw(menu,p,NULL,host->renderer));
        int w,h;SDL_RendererLogicalPresentation mode;
        assert(SDL_GetRenderLogicalPresentation(host->renderer,&w,&h,&mode));
        assert(w==320 && h==240); /* Every overlay must restore gameplay coordinates. */
        assert(SDL_SetRenderLogicalPresentation(host->renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED));
        SDL_Surface *raw=SDL_RenderReadPixels(host->renderer,NULL);assert(raw);
        SDL_Surface *rgba=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA32);assert(rgba);unsigned value[3]={0};
        for(unsigned band=0;band<3;++band) {
            const int x=rgba->w*(1+4*(int)band)/10,y=rgba->h/30;
            for(int dx=-4;dx<4;++dx) value[band]+=((const Uint8 *)rgba->pixels)[y*rgba->pitch+(x+dx)*4];
            value[band]/=8;
        }
        printf("Menu gradient %dx%d page %u: red %u/%u/%u\n",rgba->w,rgba->h,pages[page],value[0],value[1],value[2]);fflush(stdout);
        assert(value[0]<35 && value[1]>85 && value[1]<135 && value[2]>185);
        SDL_DestroySurface(rgba);SDL_DestroySurface(raw);
        assert(SDL_SetRenderLogicalPresentation(host->renderer,w,h,mode));
    }
    menu->page=HMBTN_PAUSE_HOME;
    assert(SDL_SetWindowSize(host->window,640,480) && SDL_SyncWindow(host->window));SDL_PumpEvents();
}
int main(int argc,char **argv)
{
    if(argc!=2 && argc!=3) return 2;
    HmbtnHost host={0};assert(hmbtn_host_open(&host));char error[512]={0};
    assert(hmbtn_native_font_width("WWW",24)>hmbtn_native_font_width("iii",24));
    /* Smooth glyph coverage, independent of the scene/gradient underneath. */
    HmbtnNativeFont *font=hmbtn_native_font_create(host.renderer);assert(font);
    assert(SDL_SetRenderDrawColor(host.renderer,0,0,0,255) && SDL_RenderClear(host.renderer));
    assert(hmbtn_native_font_draw(font,host.renderer,"Ação",12,12,32,240,(SDL_Color){255,255,255,255}));
    SDL_Surface *pixels=SDL_RenderReadPixels(host.renderer,NULL);assert(pixels);
    SDL_Surface *rgba=SDL_ConvertSurface(pixels,SDL_PIXELFORMAT_RGBA32);assert(rgba);unsigned edges=0;
    for(int y=0;y<rgba->h;++y) for(int x=0;x<rgba->w;++x) {
        const Uint8 *q=(const Uint8 *)rgba->pixels+y*rgba->pitch+x*4;
        edges+=q[0]>0 && q[0]<255 && q[0]==q[1] && q[1]==q[2];
    }
    assert(edges>30);SDL_DestroySurface(rgba);SDL_DestroySurface(pixels);hmbtn_native_font_destroy(font);
    char *base=NULL,*slot=NULL,*root=NULL,*settings=NULL,*bad=NULL;
    SDL_asprintf(&base,"%s-%" SDL_PRIu64,argv[1],SDL_GetTicksNS());assert(base && SDL_CreateDirectory(base));
    SDL_asprintf(&slot,"%s/latest.hms",base);root=hmbtn_save_library_root(slot);
    SDL_asprintf(&settings,"%s/settings.ini",base);
    HmbtnSession *a=save_fixture(1),*b=save_fixture(2);
    const uint16_t name[9]={0,1,2,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff};
    memcpy(a->game.registration.player_name,name,sizeof(name));memcpy(a->game.registration.farm_name,name,sizeof(name));
    memcpy(b->game.registration.player_name,name,sizeof(name));memcpy(b->game.registration.farm_name,name,sizeof(name));
    a->game.economy.gold=100;
    assert(hmbtn_save_library_write(root,slot,a,error,sizeof(error)));SDL_Delay(2);
    a->game.economy.gold=200;assert(hmbtn_save_library_write(root,slot,a,error,sizeof(error)));SDL_Delay(2);
    b->game.economy.gold=300;assert(hmbtn_save_library_write(root,slot,b,error,sizeof(error)));
    assert(memcmp(a->farm_id,b->farm_id,sizeof(a->farm_id)));
    HmbtnSaveLibrary library={0};assert(hmbtn_save_library_scan(&library,root,slot,error,sizeof(error)) && library.count==3);
    assert(!strcmp(library.entries[0].farm,"ABC") && !strcmp(library.entries[0].player,"ABC"));
    assert(library.entries[0].gold==300 && library.entries[0].berries==3);
    assert(!hmbtn_save_same_farm(&library.entries[0],&library.entries[1]) && hmbtn_save_same_farm(&library.entries[1],&library.entries[2]));
    HmbtnSession *restored=hmbtn_save_file_read(library.entries[2].path,error,sizeof(error));
    assert(restored && restored->game.economy.gold==100 && !memcmp(restored->farm_id,a->farm_id,sizeof(a->farm_id)));
    hmbtn_session_destroy(restored);
    /* Damaged snapshots cannot hide healthy saves or replace the live game. */
    SDL_asprintf(&bad,"%s/broken.hms",root);assert(SDL_SaveFile(bad,"bad",3));
    assert(hmbtn_save_library_scan(&library,root,slot,error,sizeof(error)) && library.count==3 && library.skipped==1);
    HmbtnPreferences p,loaded;hmbtn_preferences_defaults(&p);p.volume=35;p.language=1;p.smooth=true;p.integer_scale=false;p.window_scale=2;p.vsync=false;
    assert(hmbtn_preferences_bind(&p,6,SDL_SCANCODE_C) && p.keys[8]==SDL_SCANCODE_Z);
    assert(!hmbtn_preferences_bind(&p,6,SDL_SCANCODE_ESCAPE));
    assert(hmbtn_preferences_save(&p,settings,error,sizeof(error)) && hmbtn_preferences_load(&loaded,settings,error,sizeof(error)));
    assert(!memcmp(&p,&loaded,sizeof(p)));
    assert(hmbtn_preferences_apply(&p,host.window,host.renderer,error,sizeof(error)));
    assert(SDL_SyncWindow(host.window));SDL_PumpEvents();
    int w,h;SDL_GetWindowSize(host.window,&w,&h);assert(w==640 && h==480);
    HmbtnGameInput input;hmbtn_game_input_init(&input);hmbtn_game_input_bindings(&input,p.keys);
    SDL_Event e={0};e.type=SDL_EVENT_KEY_DOWN;e.key.windowID=SDL_GetWindowID(host.window);e.key.scancode=SDL_SCANCODE_C;e.key.repeat=false;
    hmbtn_game_input_event(&input,&e,e.key.windowID);HmbtnLogicalInput frame=hmbtn_game_input_take_frame(&input);
    if(frame.held!=HMBTN_PAD_CROSS) fprintf(stderr,"Remap: held=%u pressed=%u focused=%u custom=%u key6=%u code=%u type=%u window=%u\n",
        frame.held,frame.pressed,input.focused,input.custom_keys,input.keys[6],e.key.scancode,e.type,e.key.windowID);
    assert(frame.held==HMBTN_PAD_CROSS);
    hmbtn_game_input_reset(&input);assert(!hmbtn_game_input_held(&input) && !hmbtn_game_input_take_pressed(&input));hmbtn_game_input_close(&input);
    HmbtnPauseMenu menu={.binding=-1};hmbtn_pause_menu_open(&menu,root,slot);
    const SDL_WindowID id=SDL_GetWindowID(host.window);
    assert(key(&menu,&p,host.renderer,id,SDL_SCANCODE_DOWN)==HMBTN_PAUSE_NONE);
    key(&menu,&p,host.renderer,id,SDL_SCANCODE_RETURN);assert(menu.page==HMBTN_PAUSE_FARMS);
    key(&menu,&p,host.renderer,id,SDL_SCANCODE_DOWN);key(&menu,&p,host.renderer,id,SDL_SCANCODE_RETURN);
    assert(menu.page==HMBTN_PAUSE_SAVES);
    key(&menu,&p,host.renderer,id,SDL_SCANCODE_DOWN);
    assert(key(&menu,&p,host.renderer,id,SDL_SCANCODE_RETURN)==HMBTN_PAUSE_LOAD);
    restored=hmbtn_save_file_read(menu.load_path,error,sizeof(error));assert(restored && restored->game.economy.gold==100);hmbtn_session_destroy(restored);
    assert(SDL_SetRenderDrawColor(host.renderer,70,92,100,255) && SDL_RenderClear(host.renderer));
    assert(hmbtn_pause_menu_draw(&menu,&p,a,host.renderer));
    char *shot=NULL;SDL_asprintf(&shot,"%s/history.bmp",base);assert(hmbtn_host_save_frame(&host,shot));SDL_free(shot);
    assert(key(&menu,&p,host.renderer,id,SDL_SCANCODE_ESCAPE)==HMBTN_PAUSE_RESUME && !menu.open);
    hmbtn_pause_menu_open(&menu,root,slot);p.language=0;
    gradient_sizes(&host,&menu,&p);
    assert(SDL_SetRenderDrawColor(host.renderer,70,92,100,255) && SDL_RenderClear(host.renderer));
    assert(hmbtn_pause_menu_draw(&menu,&p,a,host.renderer));
    SDL_asprintf(&shot,"%s/pause.bmp",base);assert(hmbtn_host_save_frame(&host,shot));SDL_free(shot);p.language=1;
    key(&menu,&p,host.renderer,id,SDL_SCANCODE_DOWN);key(&menu,&p,host.renderer,id,SDL_SCANCODE_DOWN);
    key(&menu,&p,host.renderer,id,SDL_SCANCODE_RETURN);assert(menu.page==HMBTN_PAUSE_SETTINGS);
    key(&menu,&p,host.renderer,id,SDL_SCANCODE_DOWN);key(&menu,&p,host.renderer,id,SDL_SCANCODE_DOWN);key(&menu,&p,host.renderer,id,SDL_SCANCODE_RETURN);
    assert(menu.page==HMBTN_PAUSE_SOUND);
    e=(SDL_Event){0};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.windowID=id;e.button.button=SDL_BUTTON_LEFT;e.button.x=43;e.button.y=172;
    assert(hmbtn_pause_menu_event(&menu,&p,host.renderer,id,&e)==HMBTN_PAUSE_CHANGED && p.volume==0);
    key(&menu,&p,host.renderer,id,SDL_SCANCODE_RIGHT);assert(p.volume==5);
    key(&menu,&p,host.renderer,id,SDL_SCANCODE_BACKSPACE);
    for(unsigned i=0;i<3;++i) key(&menu,&p,host.renderer,id,SDL_SCANCODE_DOWN);
    key(&menu,&p,host.renderer,id,SDL_SCANCODE_RETURN);assert(menu.page==HMBTN_PAUSE_CONTROLS);
    e.button.x=380;e.button.y=380;
    assert(hmbtn_pause_menu_event(&menu,&p,host.renderer,id,&e)==HMBTN_PAUSE_NONE && menu.binding==6);
    assert(key(&menu,&p,host.renderer,id,SDL_SCANCODE_T)==HMBTN_PAUSE_CHANGED && p.keys[6]==SDL_SCANCODE_T);
    key(&menu,&p,host.renderer,id,SDL_SCANCODE_PAGEDOWN);assert(menu.selected==HMBTN_BINDINGS && menu.top==7);
    assert(key(&menu,&p,host.renderer,id,SDL_SCANCODE_RETURN)==HMBTN_PAUSE_CHANGED && p.keys[6]==SDL_SCANCODE_Z);
    assert(SDL_SaveFile(settings,"damaged",7));assert(!hmbtn_preferences_load(&loaded,settings,error,sizeof(error)) && loaded.volume==100);
    /* The old single-slot format is imported once, retaining the farm chosen
     * on load and its previous snapshot before replacing the latest file. */
    HmbtnSession *legacy=save_fixture(3);char *legacy_slot=NULL;SDL_asprintf(&legacy_slot,"%s/previous.hms",base);
    legacy->game.economy.gold=400;assert(hmbtn_save_file_write(legacy_slot,legacy,error,sizeof(error)));
    hmbtn_save_library_adopt(legacy,legacy_slot);legacy->game.economy.gold=500;
    assert(hmbtn_save_library_write(root,legacy_slot,legacy,error,sizeof(error)));
    assert(hmbtn_save_library_scan(&library,root,slot,error,sizeof(error)) && library.count==5);
    size_t legacy_count=0;bool previous=false;
    for(size_t i=0;i<library.count;++i) if(!memcmp(library.entries[i].farm_id,legacy->farm_id,sizeof(legacy->farm_id))) {
        ++legacy_count;previous|=library.entries[i].gold==400;
    }
    assert(legacy_count==2 && previous);hmbtn_session_destroy(legacy);SDL_free(legacy_slot);
    assert(SDL_InitSubSystem(SDL_INIT_AUDIO));HmbtnTitleAudio audio={0};
    const SDL_AudioSpec spec={SDL_AUDIO_S16,2,44100};audio.stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,NULL,NULL);assert(audio.stream);
    assert(hmbtn_title_audio_preferences(&audio,true,35) && SDL_AudioDevicePaused(SDL_GetAudioStreamDevice(audio.stream)));
    assert(SDL_GetAudioStreamGain(audio.stream)>0.34f && SDL_GetAudioStreamGain(audio.stream)<0.36f);
    assert(hmbtn_title_audio_preferences(&audio,false,0) && !SDL_AudioDevicePaused(SDL_GetAudioStreamDevice(audio.stream)) && SDL_GetAudioStreamGain(audio.stream)==0);
    hmbtn_title_audio_close(&audio);SDL_QuitSubSystem(SDL_INIT_AUDIO);
    if(argc==3) {
        const HmbtnFarmClock date={0,0,5,5,10,0,0};
        HmbtnScenePreview *scene=hmbtn_scene_area_create_at(host.renderer,argv[2],0,&date,error,sizeof(error));assert(scene);
        for(unsigned i=0;i<110;++i) assert(hmbtn_scene_preview_tick(scene,(HmbtnLogicalInput){0},error,sizeof(error)));
        assert(SDL_SetWindowSize(host.window,1280,720));p.language=0;
        hmbtn_pause_menu_open(&menu,root,slot);
        const unsigned ticks=hmbtn_scene_preview_state(scene)->runtime.ticks;
        const HmbtnPausePage pages[]={HMBTN_PAUSE_HOME,HMBTN_PAUSE_SETTINGS,HMBTN_PAUSE_GRAPHICS,HMBTN_PAUSE_LANGUAGE,HMBTN_PAUSE_SOUND,HMBTN_PAUSE_CONTROLS};
        const char *names[]={"home","settings","graphics","language","sound","controls"};
        for(unsigned i=0;i<SDL_arraysize(pages);++i) {
            menu.page=pages[i];menu.selected=menu.top=0;
            assert(hmbtn_scene_preview_draw(scene,host.renderer,error,sizeof(error)) && hmbtn_pause_menu_draw(&menu,&p,hmbtn_scene_preview_state(scene),host.renderer));
            SDL_asprintf(&shot,"%s/modern-%s.bmp",base,names[i]);assert(hmbtn_host_save_frame(&host,shot));SDL_free(shot);
        }
        menu.page=HMBTN_PAUSE_HOME;menu.selected=1;
        key(&menu,&p,host.renderer,id,SDL_SCANCODE_RETURN);
        assert(hmbtn_scene_preview_draw(scene,host.renderer,error,sizeof(error)) && hmbtn_pause_menu_draw(&menu,&p,NULL,host.renderer));
        SDL_asprintf(&shot,"%s/modern-farms.bmp",base);assert(hmbtn_host_save_frame(&host,shot));SDL_free(shot);
        key(&menu,&p,host.renderer,id,SDL_SCANCODE_RETURN);
        assert(hmbtn_scene_preview_draw(scene,host.renderer,error,sizeof(error)) && hmbtn_pause_menu_draw(&menu,&p,NULL,host.renderer));
        SDL_asprintf(&shot,"%s/modern-saves.bmp",base);assert(hmbtn_host_save_frame(&host,shot));SDL_free(shot);
        assert(hmbtn_scene_preview_state(scene)->runtime.ticks==ticks);hmbtn_scene_preview_destroy(scene);
        printf("Visual previews: %s\n",base);
    }
    hmbtn_pause_menu_close(&menu);hmbtn_save_library_close(&library);
    hmbtn_session_destroy(a);hmbtn_session_destroy(b);
    SDL_free(bad);SDL_free(settings);SDL_free(root);SDL_free(slot);SDL_free(base);hmbtn_host_close(&host);
    puts("Native menu: stable farm identities, growing history, metadata, corrupt isolation, historical load, settings persistence, key swap, mouse volume, Esc and rendering passed");return 0;
}
