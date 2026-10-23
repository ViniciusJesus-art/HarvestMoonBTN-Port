#include "sdl_host.h"
#include "app_icon.h"

bool hmbtn_host_open(HmbtnHost *host)
{
    host->window = NULL;
    host->renderer = NULL;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        return false;
    }
    host->window = SDL_CreateWindow("Harvest Moon BTN - Base nativa (M1)",
        HMBTN_SCREEN_WIDTH * 3, HMBTN_SCREEN_HEIGHT * 3,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!host->window || !SDL_SetWindowMinimumSize(host->window,
            HMBTN_SCREEN_WIDTH, HMBTN_SCREEN_HEIGHT)) {
        return false;
    }
    /* This slice uploads a single CPU-composed 320x240 image. Prefer software
     * presentation to avoid a GPU driver's startup allocation for the title.
     * An explicit SDL_RENDER_DRIVER environment hint keeps its higher priority. */
    hmbtn_app_icon(host->window);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_DEFAULT);
    host->renderer = SDL_CreateRenderer(host->window, NULL);
    if (!host->renderer || !SDL_SetRenderLogicalPresentation(host->renderer,
            HMBTN_SCREEN_WIDTH, HMBTN_SCREEN_HEIGHT,
            SDL_LOGICAL_PRESENTATION_INTEGER_SCALE)) {
        return false;
    }
    /* Some renderers lack VSync. The application also limits frames using the clock. */
    if (!SDL_SetRenderVSync(host->renderer, 1)) {
        SDL_Log("VSync unavailable; using clock pacing: %s", SDL_GetError());
    }
    return true;
}

void hmbtn_host_close(HmbtnHost *host)
{
    SDL_DestroyRenderer(host->renderer);
    SDL_DestroyWindow(host->window);
    host->renderer = NULL;
    host->window = NULL;
    SDL_Quit();
}

bool hmbtn_host_save_frame(HmbtnHost *host, const char *path)
{
    int width, height;
    SDL_RendererLogicalPresentation mode;
    if (!SDL_GetRenderLogicalPresentation(host->renderer,&width,&height,&mode)
            || !SDL_SetRenderLogicalPresentation(host->renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED)) return false;
    /* Read the complete window, including aspect-ratio margins. SDL otherwise
     * clips ReadPixels to the logical presentation viewport. */
    SDL_Surface *surface = SDL_RenderReadPixels(host->renderer, NULL);
    const bool restored = SDL_SetRenderLogicalPresentation(host->renderer,width,height,mode);
    if (!surface) {
        return false;
    }
    const bool saved = restored && SDL_SaveBMP(surface, path);
    SDL_DestroySurface(surface);
    return saved;
}
