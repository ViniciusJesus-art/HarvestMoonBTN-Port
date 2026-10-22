#include "diagnostic_screen.h"

bool hmbtn_draw_diagnostics(SDL_Renderer *renderer, Uint64 frames)
{
    const SDL_FRect header = { 0.0f, 0.0f, 320.0f, 59.0f };
    const SDL_FRect track = { 16.0f, 148.0f, 288.0f, 12.0f };
    const SDL_FRect cursor = { 16.0f + (float)(frames % 265), 148.0f, 24.0f, 12.0f };
    return SDL_SetRenderDrawColor(renderer, 16, 23, 29, 255)
        && SDL_RenderClear(renderer)
        && SDL_SetRenderDrawColor(renderer, 32, 55, 56, 255)
        && SDL_RenderFillRect(renderer, &header)
        && SDL_SetRenderDrawColor(renderer, 161, 224, 176, 255)
        && SDL_RenderDebugText(renderer, 16, 16, "HARVEST MOON: BACK TO NATURE")
        && SDL_SetRenderDrawColor(renderer, 225, 235, 230, 255)
        && SDL_RenderDebugText(renderer, 16, 36, "PORTE NATIVO / ETAPA 1")
        && SDL_RenderDebugText(renderer, 16, 80, "Janela e eventos: SDL3")
        && SDL_RenderDebugText(renderer, 16, 96, "Area logica: 320 x 240")
        && SDL_RenderDebugText(renderer, 16, 128, "Teste de apresentacao de frames")
        && SDL_SetRenderDrawColor(renderer, 41, 57, 64, 255)
        && SDL_RenderFillRect(renderer, &track)
        && SDL_SetRenderDrawColor(renderer, 161, 224, 176, 255)
        && SDL_RenderFillRect(renderer, &cursor)
        && SDL_SetRenderDrawColor(renderer, 245, 199, 122, 255)
        && SDL_RenderDebugText(renderer, 16, 180, "Ainda sem a logica do jogo.")
        && SDL_SetRenderDrawColor(renderer, 185, 198, 205, 255)
        && SDL_RenderDebugText(renderer, 16, 216, "ESC: sair  F11: tela cheia");
}
