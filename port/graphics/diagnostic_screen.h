#ifndef HMBTN_DIAGNOSTIC_SCREEN_H
#define HMBTN_DIAGNOSTIC_SCREEN_H

#include <SDL3/SDL.h>

/* Actual diagnostic drawing, not an implementation of the game's renderer. */
bool hmbtn_draw_diagnostics(SDL_Renderer *renderer, Uint64 frames);

#endif
