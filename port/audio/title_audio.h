#ifndef HMBTN_TITLE_AUDIO_H
#define HMBTN_TITLE_AUDIO_H
#include "psx_audio.h"
#include "title_flow.h"
#include <SDL3/SDL.h>

typedef struct HmbtnTitleAudio {
    HmbtnSound *sound;
    SDL_AudioStream *stream;
    bool initialized;
} HmbtnTitleAudio;
bool hmbtn_title_audio_open(HmbtnTitleAudio *audio, const char *root, char *error, size_t error_size);
void hmbtn_title_audio_close(HmbtnTitleAudio *audio);
bool hmbtn_title_audio_pump(HmbtnTitleAudio *audio, char *error, size_t error_size);
void hmbtn_title_audio_tick(HmbtnTitleAudio *audio, const HmbtnTitleFlow *flow, HmbtnTitleStep previous);
#endif
