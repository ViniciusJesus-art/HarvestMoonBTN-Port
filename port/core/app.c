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

#include <inttypes.h>
#include <stdio.h>

int hmbtn_app_run(const HmbtnAppOptions *options)
{
    HmbtnHost host = {0};
    uint32_t frames = 0;
    int result = 0;
    bool running = true;
    HmbtnTitlePreview *preview = NULL;
    HmbtnScenePreview *scene = NULL;
    char *save_path=NULL;
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
        if(scene && options->preview_weather_set)
            hmbtn_scene_preview_set_weather(scene,options->preview_weather,options->preview_tomorrow);
        if (!preview && !scene) {
            fprintf(stderr, "Cannot load game resources: %s\nUse --data DIR or --window-test.\n", error);
            if(!options->frame_limit) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                "Nao foi possivel carregar os recursos",error,host.window);
            SDL_free(save_path);hmbtn_host_close(&host);
            return 1;
        }
        if (!SDL_SetWindowTitle(host.window, options->area_preview ? "Harvest Moon BTN - Region gameplay preview"
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
    fflush(stdout);

    Uint64 previous = SDL_GetTicksNS();
    while (running) {
        const Uint64 started = SDL_GetTicksNS();
        const Uint64 elapsed_ns = started - previous;
        previous = started;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (preview || scene) hmbtn_game_input_event(&input, &event, window_id);
            if (preview && event.type == SDL_EVENT_WINDOW_FOCUS_LOST
                    && event.window.windowID == window_id) {
                hmbtn_update_clock_discard_input(&clock);
            }
            if (scene && event.type == SDL_EVENT_WINDOW_FOCUS_LOST && event.window.windowID == window_id)
                hmbtn_update_clock_discard_input(&clock);
            const HmbtnWindowAction action = hmbtn_window_action(&event, window_id);
            if (action == HMBTN_WINDOW_QUIT) {
                running = false;
            } else if (action == HMBTN_WINDOW_TOGGLE_FULLSCREEN) {
                const bool fullscreen = (SDL_GetWindowFlags(host.window) & SDL_WINDOW_FULLSCREEN) != 0;
                if (!SDL_SetWindowFullscreen(host.window, !fullscreen)) {
                    SDL_Log("Cannot toggle fullscreen: %s", SDL_GetError());
                }
            }
        }
        if (!running) {
            break;
        }
        if (preview || scene) hmbtn_game_input_poll(&input);
        bool drawn=true;
        hmbtn_update_clock_add(&clock,elapsed_ns,hmbtn_game_input_take_frame(&input));
        HmbtnLogicalInput tick_input;
        while(hmbtn_update_clock_take(&clock,&tick_input)) {
            if(scene) drawn=hmbtn_scene_preview_tick(scene,tick_input,error,sizeof(error));
            else if(preview) drawn=hmbtn_title_preview_tick(preview,tick_input,error,sizeof(error));
            if(!drawn || (preview && (hmbtn_title_preview_registration(preview) || hmbtn_title_preview_continue(preview)))
                || (scene && (hmbtn_scene_preview_load_pending(scene) || hmbtn_scene_preview_save_pending(scene)))) break;
        }
        if(drawn && ((preview && hmbtn_title_preview_continue(preview)) || (scene && hmbtn_scene_preview_load_pending(scene)))) {
            char *root=hmbtn_data_directory(options->data_root);
            HmbtnScenePreview *loaded=root ? hmbtn_scene_saved_create(host.renderer,root,save_path,error,sizeof(error)) : NULL;
            SDL_free(root);
            if(loaded) {
                hmbtn_scene_preview_destroy(scene);hmbtn_title_preview_destroy(preview);preview=NULL;scene=loaded;
                puts("Native save loaded; session resources rebound without replaying daily updates.");
            } else {
                fprintf(stderr,"Cannot continue: %s\n",error);
                if(!options->frame_limit) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Nao foi possivel carregar",error,host.window);
                if(preview) hmbtn_title_preview_continue_failed(preview);
                if(scene) hmbtn_scene_preview_ack_load(scene);
            }
            clock=(HmbtnUpdateClock){0};hmbtn_game_input_take_frame(&input);previous=SDL_GetTicksNS();
        }
        if(drawn && scene && hmbtn_scene_preview_take_save_request(scene)) {
            if(hmbtn_scene_preview_save(scene,save_path,error,sizeof(error))) {
                printf("Native save written: %s\n",save_path);
                if(!options->frame_limit) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,"Jogo salvo","Seu progresso foi salvo.",host.window);
            } else {
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
        if(scene && hmbtn_scene_preview_take_rumble(scene) && input.gamepad
                && !SDL_RumbleGamepad(input.gamepad,0x5000,0x5000,150))
            SDL_Log("Controller vibration unavailable: %s",SDL_GetError());
        if (drawn && preview && hmbtn_title_preview_registration(preview)) {
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
    SDL_free(save_path);
    hmbtn_host_close(&host);
    return result;
}
