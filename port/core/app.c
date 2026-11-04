#include "app.h"
#include "diagnostic_screen.h"
#include "sdl_host.h"
#include "window_input.h"
#include "title_preview.h"
#include "game_input.h"
#include "update_clock.h"
#include "data_directory.h"
#include "save_file.h"
#include "scene_preview.h"
#include "setup.h"
#include "app_icon.h"
#include "pause_menu.h"
#include "native_font.h"
#include <string.h>

#include <inttypes.h>
#include <stdio.h>

typedef struct SaveNotice {
    HmbtnNativeFont *font;
    Uint64 started;
    bool active;
} SaveNotice;

/* Presentation time keeps this notice independent of game ticks and input. */
static bool draw_save_notice(SaveNotice *notice,SDL_Renderer *renderer,unsigned language)
{
    if(!notice->active) return true;
    const double age=(double)(SDL_GetTicksNS()-notice->started)/SDL_NS_PER_SECOND;
    if(age>=2.8) { notice->active=false;return true; }
    if(!notice->font) notice->font=hmbtn_native_font_create(renderer);
    if(!notice->font) return false;
    float opacity=(float)(age<0.2 ? age/0.2 : age>2.4 ? (2.8-age)/0.4 : 1.0);
    opacity=opacity*opacity*(3.0f-2.0f*opacity);
    int width,height,output_width,output_height;
    SDL_RendererLogicalPresentation mode;
    SDL_BlendMode blend;
    SDL_FRect game_rect;
    if(!SDL_GetRenderLogicalPresentation(renderer,&width,&height,&mode)
            || !SDL_GetRenderOutputSize(renderer,&output_width,&output_height)
            || !SDL_GetRenderLogicalPresentationRect(renderer,&game_rect)
            || !SDL_GetRenderDrawBlendMode(renderer,&blend)) return false;
    if(output_height<=0) return true;
    const int notice_width=(int)((int64_t)output_width*720/output_height);
    const char *text=language ? "Game saved" : "Jogo salvo";
    const float scale=720.0f/output_height;
    const float x=(game_rect.x+game_rect.w)*scale-40.0f-hmbtn_native_font_width(text,26);
    const float y=(game_rect.y+game_rect.h)*scale-68.0f+12.0f*(1.0f-opacity);
    bool ok=SDL_SetRenderLogicalPresentation(renderer,notice_width,720,SDL_LOGICAL_PRESENTATION_STRETCH)
        && SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND)
        && hmbtn_native_font_draw(notice->font,renderer,text,x+2,y+2,26,0,
            (SDL_Color){0,0,0,(Uint8)(220*opacity)})
        && hmbtn_native_font_draw(notice->font,renderer,text,x,y,26,0,
            (SDL_Color){255,255,255,(Uint8)(255*opacity)});
    const bool restored=SDL_SetRenderDrawBlendMode(renderer,blend);
    return SDL_SetRenderLogicalPresentation(renderer,width,height,mode) && restored && ok;
}

int hmbtn_app_run(const HmbtnAppOptions *options)
{
    HmbtnHost host = {0};
    uint32_t frames = 0;
    int result = 0;
    bool running = true;
    HmbtnTitlePreview *preview = NULL;
    HmbtnScenePreview *scene = NULL;
    char *save_path=NULL,*library_root=NULL,*preferences_path=NULL;
    HmbtnPauseMenu menu={.binding=-1};
    SaveNotice save_notice={0};
    HmbtnPreferences preferences;
    HmbtnGameInput input = {0};
    HmbtnUpdateClock clock={0};
    char error[512] = {0};
    /* Presentation cadence for this diagnostic, not the original PS1 game timer. */
    const Uint64 frame_ns = SDL_NS_PER_SECOND / 60;

    if (!hmbtn_host_open(&host)) {
        fprintf(stderr, "Cannot open native window: %s\n", SDL_GetError());
        hmbtn_host_close(&host);
        return 1;
    }
    if (options->window_width && !SDL_SetWindowSize(host.window,
            (int)options->window_width, (int)options->window_height)) {
        fprintf(stderr,"Cannot set window size: %s\n",SDL_GetError());
        hmbtn_host_close(&host);
        return 1;
    }
    const SDL_WindowID window_id = SDL_GetWindowID(host.window);
    printf("Native window ready: %s / %s; logical size 320x240.\n",
        SDL_GetCurrentVideoDriver(), SDL_GetRendererName(host.renderer));
    if (!options->window_test) {
        char *data_root = hmbtn_data_directory(options->data_root);
        if(options->setup || (data_root && !hmbtn_data_directory_available(data_root)
                && !options->data_root && !options->frame_limit)) {
            SDL_free(data_root);
            data_root=options->data_root ? SDL_strdup(options->data_root) : hmbtn_user_data_directory();
            if(!data_root) { hmbtn_host_close(&host);return 1; }
            int prepared=1;
            if(hmbtn_data_directory_available(data_root)) {
                if(options->setup) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                    "Recursos ja instalados",data_root,host.window);
            } else prepared=hmbtn_setup_run(host.window,host.renderer,data_root);
            if(prepared==1 && options->setup && !options->data_root) hmbtn_app_shortcuts();
            if(prepared!=1 || options->setup) {
                SDL_free(data_root);hmbtn_host_close(&host);return prepared<0 ? 1 : 0;
            }
        }
        save_path=options->save_path ? SDL_strdup(options->save_path) : hmbtn_save_default_path();
        if(!save_path) { SDL_free(data_root);hmbtn_host_close(&host);return 1; }
        if (data_root) {
            printf("Game data directory: %s\n", data_root);
            if(options->load_save)
                scene=hmbtn_scene_saved_create(host.renderer,data_root,save_path,error,sizeof(error));
            else if(options->debug_start)
                scene=hmbtn_scene_debug_create(host.renderer,data_root,
                    options->preview_date_set || options->preview_time_set ? &options->preview_clock : NULL,error,sizeof(error));
            else if(options->debug_money)
                scene=hmbtn_scene_debug_money_create(host.renderer,data_root,
                    options->area_preview ? options->preview_area : 0,
                    options->preview_date_set || options->preview_time_set ? &options->preview_clock : NULL,error,sizeof(error));
            else if (options->area_preview)
                scene = hmbtn_scene_area_create_at(host.renderer,data_root,options->preview_area,
                    options->preview_date_set || options->preview_time_set ? &options->preview_clock : NULL,error,sizeof(error));
            else if (options->house_preview)
                scene = hmbtn_scene_house_create(host.renderer, data_root, error, sizeof(error));
            else if (options->childhood_preview)
                scene = hmbtn_scene_childhood_create(host.renderer, data_root, error, sizeof(error));
            else if (options->intro_preview)
                scene = hmbtn_scene_intro_create(host.renderer, data_root, error, sizeof(error));
            else if (options->scene_preview)
                scene = hmbtn_scene_preview_create(host.renderer, data_root, error, sizeof(error));
            else
                preview = hmbtn_title_preview_create(host.renderer, data_root, error, sizeof(error));
        } else {
            snprintf(error, sizeof(error), "Cannot allocate data directory path");
        }
        SDL_free(data_root);
        if(scene && options->load_save) hmbtn_scene_preview_adopt_history(scene,save_path);
        if(scene && options->preview_weather_set)
            hmbtn_scene_preview_set_weather(scene,options->preview_weather,options->preview_tomorrow);
        if (!preview && !scene) {
            fprintf(stderr, "Cannot load game resources: %s\nUse --data DIR or --window-test.\n", error);
            if(!options->frame_limit) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                "Nao foi possivel carregar os recursos",error,host.window);
            SDL_free(save_path);hmbtn_host_close(&host);
            return 1;
        }
        if (!SDL_SetWindowTitle(host.window, options->debug_start || options->debug_money ? "Harvest Moon BTN - Debug profile"
                : options->area_preview ? "Harvest Moon BTN - Region gameplay preview"
                : options->house_preview ? "Harvest Moon BTN - House movement preview"
                : options->intro_preview || options->childhood_preview ? "Harvest Moon BTN - Opening event preview"
                : scene ? "Harvest Moon BTN - Farm geometry preview" : "Harvest Moon: Back to Nature")) {
            SDL_Log("Cannot update window title: %s", SDL_GetError());
        }
        if (preview) puts("Title assets loaded: Natsume, intro, menu and registration. New Game reaches the initial house with movement; native saves use the user slot; full gameplay and demo remain pending.");
        hmbtn_game_input_init(&input);
        if(!options->frame_limit && !options->data_root) hmbtn_app_shortcuts();
    } else {
        puts("Diagnostic window only: no game assets loaded.");
    }
    preferences_path=options->preferences_path ? SDL_strdup(options->preferences_path) : hmbtn_preferences_path();
    if(!hmbtn_preferences_load(&preferences,preferences_path,error,sizeof(error))) SDL_Log("%s",error);
    if(!hmbtn_preferences_apply(&preferences,host.window,host.renderer,error,sizeof(error))) {
        SDL_Log("%s",error);
        int actual=0;SDL_GetRenderVSync(host.renderer,&actual);preferences.vsync=actual!=0;
        preferences.fullscreen=(SDL_GetWindowFlags(host.window)&SDL_WINDOW_FULLSCREEN)!=0;
    }
    if(options->window_width) SDL_SetWindowSize(host.window,(int)options->window_width,(int)options->window_height);
    library_root=hmbtn_save_library_root(save_path);
    hmbtn_game_input_bindings(&input,preferences.keys);
    fflush(stdout);

    Uint64 previous = SDL_GetTicksNS();
    while (running) {
        const Uint64 started = SDL_GetTicksNS();
        const Uint64 elapsed_ns = started - previous;
        previous = started;
        SDL_Event event;bool reset_time=false;
        while (SDL_PollEvent(&event)) {
            const bool was_paused=menu.open;
            const HmbtnWindowAction action=hmbtn_window_action(&event,window_id);
            if(action==HMBTN_WINDOW_QUIT) running=false;
            else if(action==HMBTN_WINDOW_PAUSE) {
                if(menu.open) menu.open=false;
                else {
                    if(scene) hmbtn_scene_preview_preferences(scene,true,preferences.volume,preferences.smooth);
                    if(preview) hmbtn_title_preview_preferences(preview,true,preferences.volume,preferences.smooth);
                    hmbtn_pause_menu_open(&menu,library_root,save_path);
                }
                reset_time=true;
            } else if(menu.open) {
                const HmbtnPreferences before=preferences;
                const HmbtnPauseAction selected=hmbtn_pause_menu_event(&menu,&preferences,host.renderer,window_id,&event);
                if(selected==HMBTN_PAUSE_EXIT) running=false;
                if(selected==HMBTN_PAUSE_RESUME) reset_time=true;
                if(selected==HMBTN_PAUSE_CHANGED) {
                    bool display=menu.page==HMBTN_PAUSE_GRAPHICS;
                    if(display && !hmbtn_preferences_apply(&preferences,host.window,host.renderer,menu.error,sizeof(menu.error))) {
                        preferences=before;hmbtn_preferences_apply(&preferences,host.window,host.renderer,error,sizeof(error));
                    } else {
                        menu.error[0]=0;
                        hmbtn_game_input_bindings(&input,preferences.keys);
                        hmbtn_preferences_save(&preferences,preferences_path,menu.error,sizeof(menu.error));
                    }
                }
                if(selected==HMBTN_PAUSE_LOAD) {
                    char *root=hmbtn_data_directory(options->data_root);
                    HmbtnScenePreview *loaded=root ? hmbtn_scene_saved_create(host.renderer,root,menu.load_path,menu.error,sizeof(menu.error)) : NULL;
                    SDL_free(root);
                    if(loaded) {
                        hmbtn_scene_preview_adopt_history(loaded,menu.load_path);
                        hmbtn_scene_preview_destroy(scene);hmbtn_title_preview_destroy(preview);
                        preview=NULL;scene=loaded;menu.open=false;reset_time=true;
                        save_notice.active=false;
                        puts("Save history: selected farm snapshot loaded.");
                    }
                }
            } else if(action==HMBTN_WINDOW_TOGGLE_FULLSCREEN) {
                preferences.fullscreen=!preferences.fullscreen;
                if(!hmbtn_preferences_apply(&preferences,host.window,host.renderer,error,sizeof(error))) {
                    preferences.fullscreen=!preferences.fullscreen;SDL_Log("%s",error);
                } else if(!hmbtn_preferences_save(&preferences,preferences_path,error,sizeof(error))) SDL_Log("%s",error);
            }
            /* Native menu keys never reach the simulation. Keep device/focus
             * notifications alive while paused, and clear held keys on return. */
            if((preview || scene) && ((!was_paused && !menu.open && action!=HMBTN_WINDOW_PAUSE)
                    || (event.type!=SDL_EVENT_KEY_DOWN && event.type!=SDL_EVENT_GAMEPAD_BUTTON_DOWN)))
                hmbtn_game_input_event(&input,&event,window_id);
            if(event.type==SDL_EVENT_WINDOW_FOCUS_LOST && event.window.windowID==window_id)
                hmbtn_update_clock_discard_input(&clock);
        }
        if(!running) break;
        if(reset_time) { clock=(HmbtnUpdateClock){0};hmbtn_game_input_reset(&input);previous=SDL_GetTicksNS(); }
        if(preview || scene) hmbtn_game_input_poll(&input);
        bool drawn=true;
        if(scene) drawn=hmbtn_scene_preview_preferences(scene,menu.open,preferences.volume,preferences.smooth);
        if(preview) drawn=hmbtn_title_preview_preferences(preview,menu.open,preferences.volume,preferences.smooth);
        if(menu.open) { clock=(HmbtnUpdateClock){0};hmbtn_game_input_take_frame(&input); }
        else hmbtn_update_clock_add(&clock,reset_time ? 0 : elapsed_ns,hmbtn_game_input_take_frame(&input));
        HmbtnLogicalInput tick_input;
        while(drawn && !menu.open && hmbtn_update_clock_take(&clock,&tick_input)) {
            if(scene) drawn=hmbtn_scene_preview_tick(scene,tick_input,error,sizeof(error));
            else if(preview) drawn=hmbtn_title_preview_tick(preview,tick_input,error,sizeof(error));
            if(!drawn || (preview && (hmbtn_title_preview_registration(preview) || hmbtn_title_preview_continue(preview)))
                || (scene && (hmbtn_scene_preview_load_pending(scene) || hmbtn_scene_preview_save_pending(scene)))) break;
        }
        if(drawn && !menu.open && ((preview && hmbtn_title_preview_continue(preview)) || (scene && hmbtn_scene_preview_load_pending(scene)))) {
            char *root=hmbtn_data_directory(options->data_root);
            HmbtnScenePreview *loaded=root ? hmbtn_scene_saved_create(host.renderer,root,save_path,error,sizeof(error)) : NULL;
            SDL_free(root);
            if(loaded) {
                hmbtn_scene_preview_adopt_history(loaded,save_path);
                hmbtn_scene_preview_destroy(scene);hmbtn_title_preview_destroy(preview);preview=NULL;scene=loaded;
                save_notice.active=false;
                puts("Native save loaded; session resources rebound without replaying daily updates.");
            } else {
                fprintf(stderr,"Cannot continue: %s\n",error);
                if(!options->frame_limit) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Nao foi possivel carregar",error,host.window);
                if(preview) hmbtn_title_preview_continue_failed(preview);
                if(scene) hmbtn_scene_preview_ack_load(scene);
            }
            clock=(HmbtnUpdateClock){0};hmbtn_game_input_take_frame(&input);previous=SDL_GetTicksNS();
        }
        if(drawn && !menu.open && scene && hmbtn_scene_preview_take_save_request(scene)) {
            if(hmbtn_scene_preview_save_history(scene,library_root,save_path,error,sizeof(error))) {
                printf("Native save written: %s\n",save_path);
                save_notice.started=SDL_GetTicksNS();save_notice.active=true;
            } else {
                save_notice.active=false;
                fprintf(stderr,"Save failed: %s\n",error);
                if(!options->frame_limit) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Nao foi possivel salvar",error,host.window);
            }
            previous=SDL_GetTicksNS();
        }
        if(drawn) {
            if(scene) drawn=hmbtn_scene_preview_draw(scene,host.renderer,error,sizeof(error));
            else if(preview) drawn=hmbtn_title_preview_draw(preview,host.renderer,error,sizeof(error));
            else drawn=hmbtn_draw_diagnostics(host.renderer,frames);
        }
        if(!menu.open && scene && hmbtn_scene_preview_take_rumble(scene) && input.gamepad
                && !SDL_RumbleGamepad(input.gamepad,0x5000,0x5000,150))
            SDL_Log("Controller vibration unavailable: %s",SDL_GetError());
        if (drawn && !menu.open && preview && hmbtn_title_preview_registration(preview)) {
            const HmbtnRegistration registration = *hmbtn_title_preview_registration(preview);
            char *root = hmbtn_data_directory(options->data_root);
            /* Release title atlases and audio before allocating the farm resources. */
            hmbtn_title_preview_destroy(preview); preview = NULL;
            if (root) scene = hmbtn_scene_intro_create(host.renderer,root,error,sizeof(error));
            else snprintf(error,sizeof(error),"Cannot allocate opening data path");
            SDL_free(root);
            if (!scene) drawn = false;
            else {
                hmbtn_scene_intro_registration(scene,&registration);
                drawn = hmbtn_scene_preview_draw(scene,host.renderer,error,sizeof(error));
            }
        }
        if(drawn && scene) drawn=draw_save_notice(&save_notice,host.renderer,preferences.language);
        if(drawn && menu.open) drawn=hmbtn_pause_menu_draw(&menu,&preferences,
            scene ? hmbtn_scene_preview_state(scene) : NULL,host.renderer);
        if (!drawn
                || (options->screenshot_path
                    && (options->frame_limit ? frames + 1 == options->frame_limit : frames == 0)
                    && !hmbtn_host_save_frame(&host, options->screenshot_path))
                || !SDL_RenderPresent(host.renderer)) {
            fprintf(stderr, "Cannot present native frame: %s\n", (!drawn && !options->window_test) ? error : SDL_GetError());
            result = 1;
            break;
        }
        frames++;
        if (options->frame_limit && frames >= options->frame_limit) {
            break;
        }
        const Uint64 elapsed = SDL_GetTicksNS() - started;
        if (elapsed < frame_ns) {
            SDL_DelayNS(frame_ns - elapsed);
        }
    }
    printf("Frames presented: %" PRIu32 "\n", frames);
    hmbtn_title_preview_destroy(preview);
    hmbtn_scene_preview_destroy(scene);
    hmbtn_game_input_close(&input);
    hmbtn_pause_menu_close(&menu);
    hmbtn_native_font_destroy(save_notice.font);
    SDL_free(preferences_path);SDL_free(library_root);SDL_free(save_path);
    hmbtn_host_close(&host);
    return result;
}
