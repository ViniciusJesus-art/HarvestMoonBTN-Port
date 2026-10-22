#ifndef HMBTN_TITLE_PREVIEW_H
#define HMBTN_TITLE_PREVIEW_H

#include <SDL3/SDL.h>
#include "new_game.h"

typedef struct HmbtnTitlePreview HmbtnTitlePreview;

HmbtnTitlePreview *hmbtn_title_preview_create(SDL_Renderer *renderer, const char *data_root,
    char *error, size_t error_size);
bool hmbtn_title_preview_draw(HmbtnTitlePreview *preview, SDL_Renderer *renderer,
    Uint64 elapsed_ns, uint32_t pressed, uint32_t held, char *error, size_t error_size);
void hmbtn_title_preview_destroy(HmbtnTitlePreview *preview);
void hmbtn_title_preview_discard_input(HmbtnTitlePreview *preview);
/* Borrowed until destroy; NULL until the original registration fade completes. */
const HmbtnNewGame *hmbtn_title_preview_registration(const HmbtnTitlePreview *preview);

#endif
