#include "title_audio.h"
#include "resource_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool hmbtn_title_audio_open(HmbtnTitleAudio *a, const char *root, char *error, size_t error_size)
{
    /* sound_offsets and song_table[SONG_TITLE-1]: bank 0, bank 53, SEQ 54.
     * Read bounded ranges, never the complete disc or music archive. */
    uint8_t *fx = malloc(24576), *music = malloc(106496), *seq = malloc(10240);
    a->sound = hmbtn_sound_create();
    bool ok = fx && music && seq && a->sound;
    if (!ok) snprintf(error, error_size, "Cannot allocate title audio buffers");
    if (ok) ok = hmbtn_resource_read(root, "A_FILE/09_sound.stm", 0, fx, 24576, error, error_size)
        && hmbtn_resource_read(root, "A_FILE/09_sound.stm", 4411392, music, 106496, error, error_size)
        && hmbtn_resource_read(root, "A_FILE/09_sound.stm", 4517888, seq, 10240, error, error_size)
        && hmbtn_sound_bank(a->sound, 0, fx, 4096, fx + 4096, 20480, error, error_size)
        && hmbtn_sound_bank(a->sound, 1, music, 6144, music + 6144, 100352, error, error_size)
        && hmbtn_sound_sequence(a->sound, seq, 10240, error, error_size);
    free(fx); free(music); free(seq);
    if (!ok) return false;
    a->initialized = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (a->initialized) {
        const SDL_AudioSpec spec = {SDL_AUDIO_S16, 2, HMBTN_AUDIO_RATE};
        a->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        if (a->stream && !SDL_ResumeAudioStreamDevice(a->stream)) {
            SDL_DestroyAudioStream(a->stream);
            a->stream = NULL;
        }
    }
    if (!a->stream) {
        fprintf(stderr, "Audio device unavailable; title is silent: %s\n", SDL_GetError());
    } else {
        printf("Title audio ready: %s, VAB instruments + SEQ 54, stereo 44100 Hz.\n", SDL_GetCurrentAudioDriver());
    }
    return true;
}
void hmbtn_title_audio_close(HmbtnTitleAudio *a)
{
    SDL_DestroyAudioStream(a->stream);
    hmbtn_sound_destroy(a->sound);
    if (a->initialized) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    memset(a, 0, sizeof(*a));
}
void hmbtn_title_audio_tick(HmbtnTitleAudio *a, const HmbtnTitleFlow *f, HmbtnTitleStep previous)
{
    if (previous == HMBTN_TITLE_PUBLISHER_OUT && f->step == HMBTN_TITLE_INTRO_FADE_IN)
        hmbtn_sound_start(a->sound);
    if (f->sound != HMBTN_TITLE_SOUND_NONE) {
        const unsigned tone = f->sound == HMBTN_TITLE_SOUND_CURSOR ? 5 : 0;
        hmbtn_sound_effect(a->sound, tone);
        printf("Title sound: %s (original tone %u)\n", tone ? "cursor" : "confirm", tone);
    }
    unsigned volume = 127;
    if (f->step == HMBTN_TITLE_CONTINUE_FADE_WAIT || f->step == HMBTN_TITLE_NEW_GAME_FADE_WAIT || f->step == HMBTN_TITLE_NEW_GAME_LEAVE) volume = f->brightness * 127 / HMBTN_TITLE_FADE_TICKS;
    if (f->step == HMBTN_TITLE_UNAVAILABLE && (f->destination == HMBTN_TITLE_DESTINATION_CONTINUE || f->destination == HMBTN_TITLE_DESTINATION_NEW_GAME)) volume = 0;
    hmbtn_sound_music_volume(a->sound, volume);
}
bool hmbtn_title_audio_pump(HmbtnTitleAudio *a, char *error, size_t error_size)
{
    if (!a->stream) return true;
    /* At most 46 ms of PCM queued; no accumulating song-sized buffer or thread.
     * SDL does device conversion. Every allocation/voice count is bounded. */
    int queued = SDL_GetAudioStreamQueued(a->stream);
    if (queued < 0) goto failed;
    int16_t pcm[512 * 2];
    for (unsigned i = 0; queued < 2048 * 4 && i < 4; ++i) {
        hmbtn_sound_render(a->sound, pcm, 512);
        if (!SDL_PutAudioStreamData(a->stream, pcm, sizeof(pcm))) goto failed;
        queued += (int)sizeof(pcm);
    }
    return true;
failed:
    snprintf(error, error_size, "Cannot submit title audio: %s", SDL_GetError());
    return false;
}
