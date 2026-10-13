#ifndef SOUND_REQUEST_H
#define SOUND_REQUEST_H

/* Sound requests: the master-volume fader, the sequence queue and the song table. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "sound.h"

/* The fixed sound requests: SOUND_REQUEST_RESET silences everything and restarts the
 * current sequence; the other two queue the sequences 2 and 1. Any other id queues the
 * pair song_table[id - 1]. */
enum SoundRequest {
    SOUND_REQUEST_RESET = 0,
    SOUND_REQUEST_SEQ_2 = 999,
    SOUND_REQUEST_SEQ_1 = 1000,
};

/* Song numbers for sound_request (from 1, see song_table). */
enum SoundSong {
    SONG_TITLE = 0x1A,      /* the title screen's theme */
};

s32 sound_play(s16 id);
void sound_release_voice(s32 voice);
s32 sound_seq_start_reverb(s16 id, s16 handle, s16 reverbIndex, s16 volume);
s32 sound_seq_stop_first(s16 id);
void sound_set_seq_arg(s32 unused, u8 *buffer);
void sound_request(s32 request, s32 unused);
void sound_fader_init(void);
void sound_load_tick(void);
s32 sound_fader_ready(void);
void sound_fade_in(s32 ticks);
void sound_fade_out(s32 ticks);
enum SoundFaderMode sound_fader_mode(void);
void sound_init_stereo_xa(void);

#endif /* SOUND_REQUEST_H */
