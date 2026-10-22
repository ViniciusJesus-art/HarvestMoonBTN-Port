#ifndef HMBTN_WINDOW_INPUT_H
#define HMBTN_WINDOW_INPUT_H

#include <SDL3/SDL.h>

typedef enum HmbtnWindowAction {
    HMBTN_WINDOW_NONE,
    HMBTN_WINDOW_QUIT,
    HMBTN_WINDOW_TOGGLE_FULLSCREEN
} HmbtnWindowAction;

/* Window shortcuts only. PS1 button/gamepad translation is not implemented yet. */
HmbtnWindowAction hmbtn_window_action(const SDL_Event *event, SDL_WindowID window);

#endif
