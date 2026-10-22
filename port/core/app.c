#include "app.h"
#include "diagnostic_screen.h"
#include "sdl_host.h"
#include "window_input.h"
#include "title_preview.h"
#include "game_input.h"
#include "data_directory.h"

#include <inttypes.h>
#include <stdio.h>

int hmbtn_app_run(const HmbtnAppOptions *options)
{
    HmbtnHost host = {0};
    uint32_t frames = 0;
    int result = 0;
    bool running = true;
    HmbtnTitlePreview *preview = NULL;
    HmbtnGameInput input = {0};
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
        if (data_root) {
            printf("Game data directory: %s\n", data_root);
            preview = hmbtn_title_preview_create(host.renderer, data_root, error, sizeof(error));
        } else {
            snprintf(error, sizeof(error), "Cannot allocate data directory path");
        }
        SDL_free(data_root);
        if (!preview) {
            fprintf(stderr, "Cannot load title preview: %s\nUse --data DIR or --window-test.\n", error);
            hmbtn_host_close(&host);
            return 1;
        }
        if (!SDL_SetWindowTitle(host.window, "Harvest Moon: Back to Nature")) {
            SDL_Log("Cannot update window title: %s", SDL_GetError());
        }
        puts("Title assets loaded: Natsume, intro, menu and registration; farm scene/gameplay, saves and demo are not ported.");
        hmbtn_game_input_init(&input);
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
            if (preview) hmbtn_game_input_event(&input, &event, window_id);
            if (preview && event.type == SDL_EVENT_WINDOW_FOCUS_LOST
                    && event.window.windowID == window_id) {
                hmbtn_title_preview_discard_input(preview);
            }
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
        if (preview) hmbtn_game_input_poll(&input);
        const bool drawn = preview
            ? hmbtn_title_preview_draw(preview, host.renderer, elapsed_ns,
                hmbtn_game_input_take_pressed(&input), hmbtn_game_input_held(&input), error, sizeof(error))
            : hmbtn_draw_diagnostics(host.renderer, frames);
        if (!drawn
                || (options->screenshot_path
                    && (options->frame_limit ? frames + 1 == options->frame_limit : frames == 0)
                    && !hmbtn_host_save_frame(&host, options->screenshot_path))
                || !SDL_RenderPresent(host.renderer)) {
            fprintf(stderr, "Cannot present native frame: %s\n", (!drawn && preview) ? error : SDL_GetError());
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
    hmbtn_game_input_close(&input);
    hmbtn_host_close(&host);
    return result;
}
