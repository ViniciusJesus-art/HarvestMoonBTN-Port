#include "window_input.h"
#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

int main(void)
{
    SDL_Event event = {0};
    const SDL_WindowID window = 1;
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = window;
    event.key.key = SDLK_F11;
    CHECK(hmbtn_window_action(&event, window) == HMBTN_WINDOW_TOGGLE_FULLSCREEN);
    /* Holding F11 must not repeatedly switch display modes. */
    event.key.repeat = true;
    CHECK(hmbtn_window_action(&event, window) == HMBTN_WINDOW_NONE);
    event.key.repeat = false;
    event.key.key = SDLK_ESCAPE;
    CHECK(hmbtn_window_action(&event, window) == HMBTN_WINDOW_QUIT);
    event.key.windowID = 2;
    CHECK(hmbtn_window_action(&event, window) == HMBTN_WINDOW_NONE);
    event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    event.window.windowID = 2;
    CHECK(hmbtn_window_action(&event, window) == HMBTN_WINDOW_NONE);
    event.window.windowID = window;
    CHECK(hmbtn_window_action(&event, window) == HMBTN_WINDOW_QUIT);
    event.type = SDL_EVENT_QUIT;
    CHECK(hmbtn_window_action(&event, window) == HMBTN_WINDOW_QUIT);
    event.type = SDL_EVENT_KEY_UP;
    CHECK(hmbtn_window_action(&event, window) == HMBTN_WINDOW_NONE);
    return 0;
}
