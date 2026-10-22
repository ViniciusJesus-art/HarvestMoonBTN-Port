#include "window_input.h"

HmbtnWindowAction hmbtn_window_action(const SDL_Event *event, SDL_WindowID window)
{
    if (event->type == SDL_EVENT_QUIT) {
        return HMBTN_WINDOW_QUIT;
    }
    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event->window.windowID == window) {
        return HMBTN_WINDOW_QUIT;
    }
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.windowID == window && !event->key.repeat) {
        if (event->key.key == SDLK_ESCAPE) {
            return HMBTN_WINDOW_QUIT;
        }
        if (event->key.key == SDLK_F11) {
            return HMBTN_WINDOW_TOGGLE_FULLSCREEN;
        }
    }
    return HMBTN_WINDOW_NONE;
}
