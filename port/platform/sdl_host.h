#ifndef HMBTN_SDL_HOST_H
#define HMBTN_SDL_HOST_H

#include <SDL3/SDL.h>

enum { HMBTN_SCREEN_WIDTH = 320, HMBTN_SCREEN_HEIGHT = 240 };

typedef struct HmbtnHost {
    SDL_Window *window;
    SDL_Renderer *renderer;
} HmbtnHost;

bool hmbtn_host_open(HmbtnHost *host);
void hmbtn_host_close(HmbtnHost *host);
bool hmbtn_host_save_frame(HmbtnHost *host, const char *path);

#endif
