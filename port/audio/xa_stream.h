#ifndef HMBTN_XA_STREAM_H
#define HMBTN_XA_STREAM_H
#include "xa_decode.h"
#include <SDL3/SDL.h>

typedef struct HmbtnXAStream {
    SDL_IOStream *file;
    HmbtnXADecoder decoder;
    int16_t pcm[HMBTN_XA_MAX_FRAMES*2], current[2], next[2];
    size_t pcm_at, pcm_frames;
    uint32_t sector, sectors, fraction;
    unsigned track, channel, countdown, volume;
    bool active, primed, ended;
} HmbtnXAStream;

/* Only the two childhood song cues currently integrated (cd_audio_tracks 4/5).
 * One sector of PCM, one open file, no whole-track allocation. */
bool hmbtn_xa_stream_start(HmbtnXAStream *stream, const char *root, unsigned track,
    char *error, size_t error_size);
void hmbtn_xa_stream_close(HmbtnXAStream *stream);
void hmbtn_xa_stream_tick(HmbtnXAStream *stream); /* original main speed 2 */
bool hmbtn_xa_stream_mix(HmbtnXAStream *stream, int16_t *stereo, size_t frames,
    char *error, size_t error_size);
#endif
