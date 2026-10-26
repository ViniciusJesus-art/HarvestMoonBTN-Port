#ifndef HMBTN_SCENE_PREVIEW_H
#define HMBTN_SCENE_PREVIEW_H

#include <SDL3/SDL.h>
#include "new_game.h"
#include "farm_clock.h"
#include "services.h"

typedef struct HmbtnScenePreview HmbtnScenePreview;
HmbtnScenePreview *hmbtn_scene_preview_create(SDL_Renderer *renderer,
    const char *data_root, char *error, size_t error_size);
HmbtnScenePreview *hmbtn_scene_intro_create(SDL_Renderer *renderer,
    const char *data_root, char *error, size_t error_size);
HmbtnScenePreview *hmbtn_scene_childhood_create(SDL_Renderer *renderer,
    const char *data_root, char *error, size_t error_size);
HmbtnScenePreview *hmbtn_scene_house_create(SDL_Renderer *renderer,
    const char *data_root, char *error, size_t error_size);
HmbtnScenePreview *hmbtn_scene_area_create(SDL_Renderer *renderer,
    const char *data_root,unsigned area,char *error,size_t error_size);
HmbtnScenePreview *hmbtn_scene_area_create_at(SDL_Renderer *renderer,
    const char *data_root,unsigned area,const HmbtnFarmClock *clock,char *error,size_t error_size);
/* Development fixture only; preserves the RNG and does not advance a day. */
bool hmbtn_scene_preview_set_weather(HmbtnScenePreview *,unsigned today,unsigned tomorrow);
/* Native persistence: candidate creation leaves the existing scene untouched.
 * Save requests are taken between ticks; true means requested, not I/O success. */
HmbtnScenePreview *hmbtn_scene_saved_create(SDL_Renderer *,const char *data_root,
    const char *save_path,char *error,size_t error_size);
bool hmbtn_scene_preview_save(HmbtnScenePreview *,const char *path,char *,size_t);
bool hmbtn_scene_preview_save_pending(HmbtnScenePreview *);
bool hmbtn_scene_preview_take_save_request(HmbtnScenePreview *);
bool hmbtn_scene_preview_load_pending(const HmbtnScenePreview *);
void hmbtn_scene_preview_ack_load(HmbtnScenePreview *);
bool hmbtn_scene_preview_take_rumble(HmbtnScenePreview *scene);
void hmbtn_scene_intro_registration(HmbtnScenePreview *scene, const HmbtnRegistration *registration);
void hmbtn_scene_preview_destroy(HmbtnScenePreview *scene);
/* Exactly one logical tick, independent of whether a frame is drawn. */
bool hmbtn_scene_preview_tick(HmbtnScenePreview *scene,HmbtnLogicalInput input,char *error,size_t error_size);
/* Presentation/cache only; never consumes input or advances gameplay. */
bool hmbtn_scene_preview_draw(HmbtnScenePreview *scene,SDL_Renderer *renderer,char *error,size_t error_size);

#endif
