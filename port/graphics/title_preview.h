#ifndef HMBTN_TITLE_PREVIEW_H
#define HMBTN_TITLE_PREVIEW_H

#include <SDL3/SDL.h>
#include "new_game.h"
#include "services.h"
#include "title_flow.h"
#include "asset_decode.h"

typedef struct HmbtnTitlePreview HmbtnTitlePreview;

HmbtnTitlePreview *hmbtn_title_preview_create(SDL_Renderer *renderer, const char *data_root,
    char *error, size_t error_size);
bool hmbtn_title_preview_tick(HmbtnTitlePreview *preview,HmbtnLogicalInput input,char *error,size_t error_size);
bool hmbtn_title_preview_draw(HmbtnTitlePreview *preview,SDL_Renderer *renderer,char *error,size_t error_size);
void hmbtn_title_preview_destroy(HmbtnTitlePreview *preview);
/* Borrowed until destroy; NULL until the original registration fade completes. */
const HmbtnRegistration *hmbtn_title_preview_registration(const HmbtnTitlePreview *preview);

bool hmbtn_title_preview_continue(const HmbtnTitlePreview *);
void hmbtn_title_preview_continue_failed(HmbtnTitlePreview *);

/* Read-only inspection for hosts/tools; loans expire on destroy. */
typedef struct HmbtnTitleInspection {
    const HmbtnTitleFlow *flow;
    const HmbtnNewGame *new_game;
    const HmbtnGameState *game;
    const HmbtnAnimationPlayer *actors,*columns;
    const uint16_t *actor_timers,*column_timers;
    unsigned random_seed,scene_frames,scroll_a,scroll_b;
} HmbtnTitleInspection;
HmbtnTitleInspection hmbtn_title_preview_inspect(const HmbtnTitlePreview *preview);
#endif
