#include "xa_stream.h"
#include "psx_audio.h"
#include <stdio.h>
#include <string.h>
#include "xa_tracks.inc"

void hmbtn_xa_stream_close(HmbtnXAStream *s)
{
    if (!s) return;
    if (s->file) SDL_CloseIO(s->file);
    memset(s,0,sizeof(*s));
}
static bool next_frame(HmbtnXAStream *s, int16_t frame[2], char *error, size_t error_size)
{
    if (s->pcm_at == s->pcm_frames) {
        bool found = false;
        uint8_t sector[HMBTN_XA_SECTOR_BYTES];
        /* A complete pass is bounded by the checked file length. Never spin on
         * a corrupt/missing channel, nor cache the entire interleaved file. */
        while (s->sector < s->sectors && !s->ended) {
            if (SDL_ReadIO(s->file,sector,sizeof(sector)) != sizeof(sector)) {
                snprintf(error,error_size,"Cannot read extracted XA sector: %s",SDL_GetError()); return false;
            }
            ++s->sector;
            if (sector[0] != 1 || sector[1] != s->channel || !(sector[2]&4u)) continue;
            if (!hmbtn_xa_decode(&s->decoder,sector,sizeof(sector),s->pcm,
                    HMBTN_XA_MAX_FRAMES,&s->pcm_frames,error,error_size)) return false;
            s->pcm_at = 0; s->ended = (sector[2]&0x80u) != 0; found = true; break;
        }
        if (!found) {
            if (!s->ended) { snprintf(error,error_size,"XA channel ended without its final sector"); return false; }
            frame[0] = frame[1] = 0; return true;
        }
    }
    frame[0] = s->pcm[s->pcm_at*2]; frame[1] = s->pcm[s->pcm_at*2+1]; ++s->pcm_at;
    return true;
}
bool hmbtn_xa_stream_start(HmbtnXAStream *s, const char *root, unsigned track,
    char *error, size_t error_size)
{
    if (!s || !root || !*root || (track != 4 && track != 5 && track != 0x16)) {
        snprintf(error,error_size,"Unsupported native XA track"); return false;
    }
    hmbtn_xa_stream_close(s);
    char *path = NULL;
    if (SDL_asprintf(&path,"%s/disc/STR/%02u.XA",root,xa_tracks[track][1]+1) < 0) {
        snprintf(error,error_size,"Cannot allocate XA path"); return false;
    }
    s->file = SDL_IOFromFile(path,"rb"); SDL_free(path);
    if (!s->file) { snprintf(error,error_size,"Cannot open original XA file %u: %s",xa_tracks[track][1]+1,SDL_GetError()); return false; }
    const Sint64 size = SDL_GetIOSize(s->file);
    if (size <= 0 || size%HMBTN_XA_SECTOR_BYTES || size/HMBTN_XA_SECTOR_BYTES > 32768) {
        snprintf(error,error_size,"Extracted XA file has invalid sector size/count"); goto fail;
    }
    s->sectors = (uint32_t)(size/HMBTN_XA_SECTOR_BYTES);
    s->track = track; s->channel = xa_tracks[track][2]; s->countdown = 4;
    s->volume = xa_tracks[track][0];
    if (!next_frame(s,s->current,error,error_size) || !next_frame(s,s->next,error,error_size)) goto fail;
    s->primed = s->active = true;
    printf("Native XA track %u ready: filtered channel %u, %u Hz, sector streaming.\n",track,s->channel,s->decoder.rate);
    return true;
fail:
    hmbtn_xa_stream_close(s); return false;
}
void hmbtn_xa_stream_tick(HmbtnXAStream *s)
{
    if (!s->active) return;
    if (s->countdown) { --s->countdown; return; }
    if (!s->volume) { s->active = false; return; }
    s->volume = s->volume > 2 ? s->volume-2 : 0;
}
bool hmbtn_xa_stream_mix(HmbtnXAStream *s, int16_t *pcm, size_t frames,
    char *error, size_t error_size)
{
    if (!s || (!pcm && frames)) { snprintf(error,error_size,"Invalid XA mix buffer"); return false; }
    if (!s->active || s->countdown || !s->primed) return true;
    unsigned volume = s->volume < 33 ? s->volume*4 : 45;
    if (volume > 127) volume = 127;
    for (size_t i = 0; i < frames; ++i) {
        for (unsigned channel = 0; channel < 2; ++channel) {
            /* Linear conversion to the mixer's 44100 Hz. The CD decoder's
             * hardware zigzag interpolation remains a documented fidelity gap. */
            const int32_t sample = (int32_t)(((int64_t)s->current[channel]*(HMBTN_AUDIO_RATE-s->fraction)
                +(int64_t)s->next[channel]*s->fraction)/HMBTN_AUDIO_RATE);
            int32_t mixed = pcm[i*2+channel]+sample*(int32_t)volume/127;
            if (mixed < -32768) mixed = -32768;
            if (mixed > 32767) mixed = 32767;
            pcm[i*2+channel] = (int16_t)mixed;
        }
        s->fraction += s->decoder.rate;
        while (s->fraction >= HMBTN_AUDIO_RATE) {
            s->fraction -= HMBTN_AUDIO_RATE;
            memcpy(s->current,s->next,sizeof(s->current));
            if (!next_frame(s,s->next,error,error_size)) return false;
        }
    }
    return true;
}
