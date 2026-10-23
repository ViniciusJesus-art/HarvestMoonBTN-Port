#ifndef HMBTN_SETUP_H
#define HMBTN_SETUP_H
#include <SDL3/SDL.h>
/* 1 imported, 0 cancelled, -1 failure. Uses a worker, keeping the UI responsive. */
int hmbtn_setup_run(SDL_Window *window,SDL_Renderer *renderer,const char *destination);
#endif
