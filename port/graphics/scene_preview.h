#ifndef HMBTN_SCENE_PREVIEW_H
#define HMBTN_SCENE_PREVIEW_H

#include <SDL3/SDL.h>
#include "new_game.h"

typedef struct HmbtnScenePreview HmbtnScenePreview;
HmbtnScenePreview *hmbtn_scene_preview_create(SDL_Renderer *renderer,
    const char *data_root, char *error, size_t error_size);
HmbtnScenePreview *hmbtn_scene_intro_create(SDL_Renderer *renderer,
    const char *data_root, char *error, size_t error_size);
HmbtnScenePreview *hmbtn_scene_childhood_create(SDL_Renderer *renderer,
    const char *data_root, char *error, size_t error_size);
HmbtnScenePreview *hmbtn_scene_house_create(SDL_Renderer *renderer,
    const char *data_root, char *error, size_t error_size);
void hmbtn_scene_preview_discard_input(HmbtnScenePreview *scene);
void hmbtn_scene_intro_registration(HmbtnScenePreview *scene, const HmbtnNewGame *registration);
void hmbtn_scene_preview_destroy(HmbtnScenePreview *scene);
bool hmbtn_scene_preview_draw(HmbtnScenePreview *scene, SDL_Renderer *renderer,
    Uint64 elapsed_ns, uint32_t pressed, uint32_t held, char *error, size_t error_size);

#endif
