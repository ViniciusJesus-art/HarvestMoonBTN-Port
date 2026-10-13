/* Sound requests: the master-volume fader, the sequence queue and the song table. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libsnd.h>
#include "types.h"
#include "cd_audio.h"
#include "sound.h"
#include "sound_request.h"

/* The fader's full volume: 127.0 in 16.16. */
#define SOUND_FADER_FULL 0x7F0000

static void sound_fader_step(void);

static SoundFader master_fader;
static u8 seq_buffer[0xA000];

u8 *seq_arg;

s16 sound_request_unused_8005E31C = 0;

s32 sound_play(signed short id) {
    return sound_key_on_free_voice(id, 0, SOUND_LAST_VOICE);
}

void sound_release_voice(s32 voice) {
    sound_key_off_voice(voice);
}

static int sound_last_voice_id(void) {
    return SOUND_LAST_VOICE;
}

static signed int sound_stop_last_voice(void) {
    sound_voice_stop(SOUND_LAST_VOICE);
    return 0;
}

/* Per song (from 1): its sequence id, then its attribute. */
static u8 song_table[36][2] = {
    {0x03, 0x04}, {0x05, 0x06}, {0x07, 0x08}, {0x09, 0x0A}, {0x0B, 0x0C},
    {0x0D, 0x0E}, {0x0F, 0x10}, {0x11, 0x12}, {0x13, 0x14}, {0x15, 0x16}, {0x17, 0x18},
    {0x19, 0x1A}, {0x1B, 0x1C}, {0x1D, 0x1E}, {0x1F, 0x20}, {0x21, 0x22}, {0x23, 0x24},
    {0x25, 0x26}, {0x27, 0x28}, {0x29, 0x2A}, {0x2B, 0x2C}, {0x2D, 0x2E}, {0x2F, 0x30},
    {0x31, 0x32}, {0x33, 0x34}, {0x35, 0x36}, {0x37, 0x38}, {0x39, 0x3A}, {0x3B, 0x3C},
    {0x3D, 0x3E}, {0x3F, 0x40}, {0x41, 0x42}, {0x43, 0x44}, {0x45, 0x46}, {0x47, 0x48},
    {0x49, 0x4A},
};

/* Reverb (type, depth) pairs, one per reverb index that sound_seq_start_reverb takes. */
static s16 reverb_table[72] = {
    5, 40, 1, 64, 1, 64, 5, 64, 1, 64, 1, 64, 1, 64, 1, 64, 1, 64, 5, 40, 4, 48, 4, 48, 4,
    48, 1, 64, 1, 64, 4, 64, 4, 64, 1, 64, 1, 64, 1, 64, 1, 64, 5, 64, 5, 96, 5, 80, 5, 80,
    5, 64, 1, 64, 5, 64, 5, 80, 5, 80, 1, 64, 4, 64, 4, 64, 5, 64, 5, 64, 5, 64,
};

s32 sound_seq_start_reverb(s16 id, s16 handle, s16 reverbIndex, s16 volume) {
    s16 seqHandle = handle;
    s16 reverb = reverbIndex;
    s16 vol = volume;
    sound_seq_start(id, 0, vol, vol, seqHandle);
    SsUtSetReverbType(reverb_table[reverb * 2]);
    SsUtReverbOn();
    SsUtSetReverbDepth(reverb_table[reverb * 2 + 1], reverb_table[reverb * 2 + 1]);
    return 0;
}

signed int sound_seq_stop_first(signed short id) {
    sound_seq_stop(id, 0);
    return 0;
}

void sound_set_seq_arg(signed int unused, u8 *buffer) {
    seq_arg = buffer;
}

/* Queue a sound request: 0 silences everything and restarts the current
 * sequence, 999 and 1000 queue the fixed requests 2 and 1, and any other id
 * queues the pair song_table[id - 1]. */
void sound_request(s32 request, s32 unused) {
    s16 id = request;
    u8 *entry;

    master_fader.ready = 0;
    switch (id) {
    case SOUND_REQUEST_RESET:
        if (sound_request_unused_8005E31C == 0) {
            SsUtAllKeyOff(0);
            sound_clear_stamps();
            sound_begin_load(seq_buffer, seq_arg, 0, 0);
            sound_queue_seq(0);
            sound_request_unused_8005E31C = 0;
        }
        break;
    case SOUND_REQUEST_SEQ_2:
        sound_queue_seq(2);
        break;
    case SOUND_REQUEST_SEQ_1:
        sound_queue_seq(1);
        break;
    default:
        entry = song_table[id - 1];
        sound_queue_seq(entry[0]);
        sound_queue_seq_pair(entry[1], 2);
        break;
    }
}

static void sound_request_stub_1(void) {
}

void sound_fader_init(void) {
    master_fader.unused0 = 0;
    master_fader.mode = SOUND_FADER_IDLE;
    master_fader.volume = SOUND_FADER_FULL;
    master_fader.step = 0;
    master_fader.maxVolume = SOUND_FADER_FULL;
    master_fader.ready = 1;
}

void sound_load_tick(void) {
    if (sound_current_entry() != -1) {
        sound_load_step();
    } else {
        master_fader.ready = 1;
    }
    if (master_fader.mode != SOUND_FADER_IDLE) {
        sound_fader_step();
    }
}

signed int sound_fader_ready(void) {
    return master_fader.ready != 0;
}

/* One fader tick: step the master volume, stop at silence or full, and hand
 * it to the sound library. */
static void sound_fader_step(void) {
    signed int volume;
    signed int level;

    volume = master_fader.volume + master_fader.step;
    master_fader.volume = volume;
    if (volume <= 0) {
        master_fader.volume = 0;
        master_fader.mode = SOUND_FADER_IDLE;
    }
    if (master_fader.volume > SOUND_FADER_FULL) {
        master_fader.volume = SOUND_FADER_FULL;
        master_fader.mode = SOUND_FADER_IDLE;
    }
    level = master_fader.volume / 0x10000;
    SsSetMVol(level, level);
}

/* Fade the master volume up to full over `ticks` fader steps. */
void sound_fade_in(signed int ticks) {
    master_fader.step = master_fader.maxVolume / ticks;
    master_fader.mode = SOUND_FADER_FADE_IN;
}

/* Fade the master volume down to silence over `ticks` fader steps. */
void sound_fade_out(signed int ticks) {
    master_fader.step = -master_fader.volume / ticks;
    master_fader.mode = SOUND_FADER_FADE_OUT;
}

enum SoundFaderMode sound_fader_mode(void) {
    return master_fader.mode;
}

void sound_init_stereo_xa(void) {
    sound_set_stereo();
    xa_search_files();
}
