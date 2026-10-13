/* Sound: the sound bank loader, the sequence and voice slots, and the
 * key-on/off and volume calls that drive libsnd. */

#include <sys/types.h>
#include <libcd.h>
#include <libsnd.h>
#include <libspu.h>
#include <memory.h>
#include <libetc.h>
#include "types.h"
#include "afile.h"
#include "cd_audio.h"
#include "main.h"
#include "screen_fade.h"
#include "sound.h"

/* What sound_key_on_voice last started on each voice. */
typedef struct {
    s32 vabId;
    s32 id;
    s32 voll;
    s32 volr;
    s32 arg4;
} VoicePlay;

static void sound_reset_slots(void);
static signed int sound_reserve_voice(signed int voice);
static s32 sound_release_all(void);
static void sound_schedule_load(s16 value);
static void sound_vab_stream_start(s32 sound, u8 *dest, s32 chunkSize);
static void sound_vab_stream_setup(s32 vabId);
static s32 sound_vab_stream_step(void);
static s32 sound_find_free_voice(s32 value, s32 from, s32 to, u8 *states);
static s32 sound_find_lowest_voice(s32 first, s32 last);

/* The states of sound_load_state, the sound-bank loader (sound_load_step). */
enum SoundLoadState {
    SOUND_LOAD_IDLE = 0,
    SOUND_LOAD_VAB_OPEN = 10,
    SOUND_LOAD_VAB_DONE = 20,
    SOUND_LOAD_VAB_HEADER = 30,
    SOUND_LOAD_SEQ = 40,
    SOUND_LOAD_WHOLE = 50,
    SOUND_LOAD_VAB_STREAM = 60,
};

s32 sound_load_state;
s32 sound_ss_running;
s32 vab_slot;
s32 vab_free_mark;
u8 *sound_write_ptr;
u8 *sound_read_ptr;
/* Write cursor into the parallel queued_sounds tables. */
s32 queue_cursor;
s32 current_entry;
s32 sound_begin_count;
s32 sound_stereo;
s32 seq_slot;
s16 playing_prog;
s16 next_prog;
s16 playing_tone;
s16 next_tone;
s16 playing_note;
s16 next_note;
s16 next_fine;
s16 sound_scheduled_value;
s32 sound_unused_8005E574;
s32 sound_unused_8005E578;
s32 sound_seq_bytes;

/* A buffer table entry with no buffer behind it. */
#define SOUND_BUFFER_NONE ((u8 *)-1)
/* The priority of a voice with no sound: sound_find_lowest_voice never picks it. */
#define VOICE_PRIORITY_NONE 0x01000000

static s32 vab_starts[8];
/* Sound ids queued by sound_queue_seq, consumed by the loader. */
static s32 queued_sounds[8];
/* Whether each queued sound also has a second argument. */
static s32 queued_sound_has_arg[8];
/* The second argument of each queued sound. */
static s32 entry_args[8];
/* VAB ids, -1 when free. sound_release_all closes each one still open. */
static s32 vab_ids[8];
static Vol seq_slots[8];
static u8 *vab_bodies[8];
static u8 *vab_ends[8];
static u8 *seq_data[8];
static s32 seq_offsets[8];
static s32 seq_vabs[8];
/* Per-slot state, parallel to seq_slots. */
enum SeqState {
    SEQ_FREE = -1,
    SEQ_PLAYING = 0,
    SEQ_PAUSED = 1,
};
static s32 seq_states[8];
/* Sequence handle of each slot, parallel to seq_states. */
static s32 seq_handles[8];
/* Priority category and priority of what each voice last played. sound_find_lowest_voice
 * scans them for the lowest-priority voice of each category. */
static s32 voice_categories[24];
static s32 voice_priorities[24];
static VoicePlay voice_plays[24];
/* Common SPU attributes: the master and CD-input volumes that sound_init sets. */
static SpuCommonAttr spu_attr;
static char sound_table[1056];
static VabStream vab_stream;

/* The A_FILE entry that holds the sound data (objcg\sound.stm). */
#define SOUND_FILE_ENTRY 9
/* One chunk of a VAB stream, in bytes. */
#define VAB_STREAM_CHUNK (45 * 4096)

/* The current sound's entry in the offset table sound_offsets: its header
 * starts at SOUND_OFFSET(0), its body at SOUND_OFFSET(1), and it ends at
 * SOUND_OFFSET(2). */
#define CURRENT_SOUND   queued_sounds[current_entry]
#define SOUND_OFFSET(k) sound_offsets[CURRENT_SOUND + (k)]

static s16 tone_voice = SOUND_LAST_VOICE;

/* Bring up the sound system: libsnd tables, reserved voices, SPU master and
 * CD-input volumes, then the CD mixer. */
void sound_init(void) {
    SsInit();
    SsSetTableSize(sound_table, 6, 1);
    SsSetTickMode(1);
    sound_reserve_voice(0x16);
    sound_load_state = SOUND_LOAD_IDLE;
    sound_ss_running = 0;
    sound_begin_count = 0;
    sound_stereo = 1;
    SsUtReverbOff();
    spu_attr.mask = 0x2C3;
    spu_attr.mvol.left = 0x3FFF;
    spu_attr.mvol.right = 0x3FFF;
    spu_attr.cd.volume.left = 0x1FFF;
    spu_attr.cd.volume.right = 0x1FFF;
    spu_attr.cd.mix = 1;
    SpuSetCommonAttr(&spu_attr);
    SsSetSerialAttr(0, 0, 1);
    SsSetSerialVol(0, 0, 0);
    CdControl(9, 0, 0);
    sound_reset_slots();
}

/* Sound offsets, read in adjacent pairs: an entry and the next one bound a sound's data. */
s32 sound_offsets[151] = {
    0, 4096, 24576, 34816, 251904, 258048, 468992, 477184, 608256, 1, 618496, 626688, 784384,
    1, 796672, 804864, 960512, 1, 974848, 980992, 1095680, 1, 1099776, 1107968, 1255424, 1,
    1259520, 1267712, 1417216, 1, 1423360, 1431552, 1521664, 1, 1527808, 1533952, 1615872, 1,
    1619968, 1628160, 1767424, 1, 1775616, 1783808, 1918976, 1, 1935360, 1941504, 2136064, 1,
    2146304, 2156544, 2318336, 1, 2326528, 2334720, 2498560, 1, 2508800, 2516992, 2664448, 1,
    2674688, 2682880, 2850816, 1, 2859008, 2867200, 2979840, 1, 2994176, 3000320, 3106816, 1,
    3112960, 3119104, 3252224, 1, 3260416, 3268608, 3436544, 1, 3442688, 3450880, 3545088, 1,
    3551232, 3559424, 3723264, 1, 3733504, 3739648, 3891200, 1, 3893248, 3899392, 4048896, 1,
    4063232, 4071424, 4237312, 1, 4239360, 4247552, 4407296, 1, 4411392, 4417536, 4517888, 1,
    4528128, 4536320, 4667392, 1, 4675584, 4681728, 4788224, 1, 4796416, 4802560, 4950016, 1,
    4958208, 4964352, 5048320, 1, 5058560, 5066752, 5224448, 1, 5230592, 5238784, 5376000, 1,
    5380096, 5388288, 5525504, 1, 5527552, 5535744, 5689344, 1, 5701632, 5709824, 5890048, 1,
    5900288, 5906432, 6004736, 1, 6006784,
};

static SoundDef sound_defs[78] = {
    {0, 0, 0, 0, 60, 0, 60}, {0, 0, 0, 1, 60, 0, 60}, {0, 0, 0, 2, 60, 0, 60},
    {0, 0, 0, 3, 60, 0, 60}, {0, 0, 0, 4, 60, 0, 30}, {0, 0, 0, 5, 60, 0, 60},
    {1, 0, 0, 0, 60, 0, 127}, {1, 0, 0, 1, 60, 0, 127}, {1, 0, 0, 2, 60, 0, 127},
    {1, 0, 0, 3, 60, 0, 127}, {1, 0, 1, 2, 60, 0, 127}, {1, 0, 1, 3, 60, 0, 127},
    {1, 0, 1, 0, 60, 0, 127}, {1, 0, 1, 1, 60, 0, 127}, {1, 0, 1, 4, 60, 0, 127},
    {1, 0, 1, 5, 60, 0, 127}, {1, 0, 1, 6, 60, 0, 127}, {1, 0, 1, 7, 60, 0, 127},
    {1, 0, 1, 8, 60, 0, 60}, {1, 0, 1, 9, 60, 0, 127}, {1, 0, 1, 10, 60, 0, 127},
    {1, 0, 1, 11, 60, 0, 127}, {1, 0, 1, 12, 60, 0, 127}, {1, 0, 6, 0, 60, 0, 127},
    {1, 0, 6, 1, 60, 0, 127}, {1, 0, 6, 2, 60, 0, 127}, {1, 0, 6, 3, 60, 0, 127},
    {1, 0, 6, 4, 60, 0, 127}, {1, 0, 6, 5, 60, 0, 127}, {1, 0, 7, 0, 60, 0, 127},
    {1, 0, 7, 1, 60, 0, 127}, {1, 0, 7, 2, 60, 0, 127}, {1, 0, 7, 3, 60, 0, 127},
    {1, 0, 7, 4, 60, 0, 127}, {1, 0, 2, 0, 60, 0, 127}, {1, 0, 2, 1, 60, 0, 127},
    {1, 0, 2, 2, 60, 0, 127}, {1, 0, 2, 3, 60, 0, 127}, {1, 0, 2, 4, 60, 0, 127},
    {1, 0, 4, 0, 60, 0, 127}, {1, 0, 4, 1, 60, 0, 127}, {1, 0, 4, 2, 60, 0, 127},
    {1, 0, 4, 3, 60, 0, 127}, {1, 0, 4, 4, 60, 0, 127}, {1, 0, 3, 0, 60, 0, 127},
    {1, 0, 3, 1, 60, 0, 127}, {1, 0, 3, 2, 60, 0, 127}, {1, 0, 3, 3, 60, 0, 127},
    {1, 0, 3, 4, 60, 0, 127}, {1, 0, 5, 0, 60, 0, 127}, {1, 0, 5, 1, 60, 0, 127},
    {1, 0, 5, 2, 60, 0, 127}, {1, 0, 5, 3, 60, 0, 127}, {1, 0, 5, 4, 60, 0, 127},
    {1, 0, 0, 0, 60, 0, 127}, {1, 0, 0, 1, 60, 0, 127}, {1, 0, 0, 2, 60, 0, 127},
    {1, 0, 0, 3, 60, 0, 127}, {1, 0, 0, 4, 60, 0, 127}, {1, 0, 0, 5, 60, 0, 127},
    {1, 0, 0, 6, 60, 0, 127}, {1, 0, 0, 7, 60, 0, 115}, {1, 0, 0, 8, 60, 0, 127},
    {1, 0, 0, 9, 60, 0, 127}, {1, 0, 0, 10, 60, 0, 127}, {1, 0, 0, 11, 60, 0, 127},
    {1, 0, 0, 12, 60, 0, 127}, {1, 0, 1, 0, 60, 0, 127}, {1, 0, 1, 1, 60, 0, 127},
    {1, 0, 1, 2, 60, 0, 127}, {2, 0, 6, 0, 60, 0, 127}, {2, 0, 6, 1, 60, 0, 127},
    {1, 0, 8, 0, 68, 0, 20}, {1, 0, 8, 1, 60, 0, 127}, {1, 0, 9, 0, 60, 0, 127},
    {1, 0, 9, 1, 60, 0, 127}, {1, 0, 10, 0, 60, 0, 60}, {1, 0, 2, 0, 60, 0, 127},
};

static s32 voice_stamps[24] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/* Clear all 24 voice stamps, back to front. */
void sound_clear_stamps(void) {
    s32 *entry;
    s32 i;

    i = SOUND_LAST_VOICE;
    entry = voice_stamps;
    entry += SOUND_LAST_VOICE;
    do {
        *entry = 0;
        i -= 1;
        entry -= 1;
    } while (i >= 0);
}

/* Empty every slot and the queue, and clear each voice's category and priority. */
static void sound_reset_slots(void) {
    s32 slot;
    s32 sub;

    vab_slot = 0;
    current_entry = 0;
    for (slot = 0; slot < 6; slot++) {
        queued_sounds[slot] = -1;
        queued_sound_has_arg[slot] = 0;
        entry_args[slot] = 0;
        vab_ids[slot] = -1;
        *(s32 *)&seq_slots[slot] = -1;
        seq_data[slot] = SOUND_BUFFER_NONE;
        vab_bodies[slot] = SOUND_BUFFER_NONE;
        vab_ends[slot] = SOUND_BUFFER_NONE;
        seq_vabs[slot] = -1;
        seq_offsets[slot] = -1;
        for (sub = 0; sub < 1; sub++) {
            seq_states[slot + sub] = SEQ_FREE;
            seq_handles[slot + sub] = -1;
        }
    }
    queue_cursor = 0;
    seq_slot = 0;
    for (slot = 0; slot < 24; slot++) {
        voice_categories[slot] = 0;
        voice_priorities[slot] = VOICE_PRIORITY_NONE;
    }
}

/* Start a new load into the given buffers, releasing what was loaded before. */
int sound_begin_load(u8 *write_ptr, u8 *read_ptr, int unused2, int unused3) {
    if (sound_begin_count != 0) {
        sound_release_all();
    }
    sound_write_ptr = write_ptr;
    sound_read_ptr = read_ptr;
    vab_free_mark = 0x1010;
    sound_begin_count += 1;
    sound_reset_slots();
    return 0;
}

/* The table holds two entries per sound, so the stored id is doubled. */
signed int sound_queue_seq(signed int seq) {
    queued_sounds[queue_cursor] = seq * 2;
    queue_cursor += 1;
    return 0;
}

/* Queue a sound like sound_queue_seq, with a second argument for the loader. */
signed int sound_queue_seq_pair(signed int seq, signed int entryArg) {
    queued_sounds[queue_cursor] = seq * 2;
    queued_sound_has_arg[queue_cursor] = 1;
    entry_args[queue_cursor] = entryArg;
    queue_cursor += 1;
    return 0;
}

/* Start sequence `sound_id` in slot `slot_id` at the left and right volumes, if that
 * slot is free: key off every voice, then play the sequence. */
s32 sound_seq_start(s32 sound_id, s32 slot_id, s32 left, s32 right, s32 handle) {
    s32 *handles;
    s32 index;
    s16 old_handle;

    handles = seq_states;
    index = slot_id + sound_id;
    old_handle = handle;
    if (handles[index] == SEQ_FREE) {
        SsUtAllKeyOff(0);
        SsSeqSetVol(seq_slots[sound_id].seqHandle, left, right);
        SsSeqStop(seq_slots[sound_id].seqHandle);
        SsSeqPlay(seq_slots[sound_id].seqHandle, 1, 0);
        handles[index] = SEQ_PLAYING;
        seq_handles[index] = old_handle;
    }
    return 0;
}

/* Set both volumes of a sequence that is playing; other states are left alone. */
signed int sound_seq_volume_if_idle(signed int sound_id, signed int slot_id, signed int unused2, signed int volume,
                         s32 unused4) {
    if (seq_states[slot_id + sound_id] == SEQ_PLAYING) {
        SsSeqSetVol(seq_slots[sound_id].seqHandle, volume, volume);
    }
    return 0;
}

/* Set a sequence's volume while it is playing. A level of zero or less stops the
 * sequence and frees the slot. */
static signed int sound_seq_set_level(signed int sound_id, signed int slot_id, signed int level) {
    signed short handle;
    signed short amount;

    if (seq_states[slot_id + sound_id] != SEQ_FREE) {
        if (level > 0) {
            handle = seq_slots[sound_id].seqHandle;
            amount = level;
            SsSeqSetVol(handle, amount, amount);
            return 1;
        }
        handle = seq_slots[sound_id].seqHandle;
        SsSeqStop(handle);
        seq_states[slot_id + sound_id] = SEQ_FREE;
        return 0;
    }
    return 0;
}

/* Set a sequence's volume unless its slot is free or the volume is 126 or more. */
static signed int sound_seq_set_volume(signed int sound_id, signed int slot_id, signed int volume) {
    if (seq_states[slot_id + sound_id] != SEQ_FREE && volume < 126) {
        SsSeqSetVol(seq_slots[sound_id].seqHandle, volume, volume);
        return 1;
    }
    return 0;
}

static signed int sound_stub_1(void) {
    return 0;
}

signed int sound_seq_stop(signed int sound_id, signed int slot_id) {
    if (seq_states[slot_id + sound_id] != SEQ_FREE) {
        SsSeqStop(seq_slots[sound_id].seqHandle);
        seq_states[slot_id + sound_id] = SEQ_FREE;
    }
    return 0;
}

static signed int sound_seq_pause(signed int sound_id, signed int slot_id) {
    if (seq_states[slot_id + sound_id] == SEQ_PLAYING) {
        SsSeqPause(seq_slots[sound_id].seqHandle);
        seq_states[slot_id + sound_id] = SEQ_PAUSED;
    }
    return 0;
}

static signed int sound_seq_resume(signed int sound_id, signed int slot_id) {
    if (seq_states[slot_id + sound_id] == SEQ_PAUSED) {
        SsSeqReplay(seq_slots[sound_id].seqHandle);
        seq_states[slot_id + sound_id] = SEQ_PLAYING;
    }
    return 0;
}

static signed int sound_reserve_voice(signed int voice) {
    if ((signed char)SsSetReservedVoice(voice & 0xFF) < 0) return -1;
    return 0;
}

/* Replace the playing tone: key off the previous one, key on sound `id`, and
 * remember the new voice and its program, tone and note. Returns the voice. */
static s16 sound_replace_tone(s32 vabId, s32 id, s32 voll, s32 volr) {
    s32 voice;

    next_prog = sound_defs[id].prog;
    next_tone = sound_defs[id].tone;
    next_note = sound_defs[id].note;
    next_fine = sound_defs[id].fine;
    SsUtKeyOff(tone_voice, vabId, playing_prog, playing_tone, playing_note);
    voice = SsUtKeyOn(vabId, next_prog, next_tone, next_note, next_fine, voll, volr);
    tone_voice = voice;
    playing_prog = next_prog;
    playing_tone = next_tone;
    playing_note = next_note;
    return voice;
}

/* Key a sound's program/tone/note on `voice` from bank `vabId` at the given
 * volumes, and remember the request for that voice. Returns 0. */
static s32 sound_key_on_voice(s32 vabId, s32 id, s32 voll, s32 volr, s32 arg4, s32 voice) {
    SsUtKeyOnV(voice, vabId, sound_defs[id].prog, sound_defs[id].tone, sound_defs[id].note,
               sound_defs[id].fine, voll, volr);
    voice_plays[voice].vabId = vabId;
    voice_plays[voice].id = id;
    voice_plays[voice].voll = voll;
    voice_plays[voice].volr = volr;
    voice_plays[voice].arg4 = arg4;
    return 0;
}

/* Key a voice of bank `vabId` off in the given mode. */
static signed int sound_voice_key_off(signed int vabId, signed int voice, signed int mode) {
    SsVoKeyOff(voice + (vabId << 8), mode);
    return 0;
}

/* Silence `voice` and key it off. Returns -1 if libsnd rejects the key off. */
s32 sound_voice_stop(s16 voice) {
    s32 result;
    SsUtSetVVol(voice, 0, 0);
    result = SsUtKeyOffV(voice);
    if (result < 0) return -1;
    return 0;
}

static signed int sound_stub_2(void) {
    return 0;
}

signed int sound_voice_set_volume(signed short voice, signed short voll, signed short volr) {
    SsUtSetVVol(voice, voll, volr);
    return 0;
}

/* Open the libsnd sequence of each loaded slot that is not open yet, start libsnd on
 * the first call, and set the output to stereo or mono. */
s32 sound_update(void) {
    s32 sentinel;
    s32 i;

    sentinel = -1;
    for (i = 0; i < 6; i++) {
        if ((s32)seq_data[i] != sentinel && *(s32 *)&seq_slots[i] == sentinel) {
            *(s32 *)&seq_slots[i] = SsSeqOpen((u_long *)seq_data[i], seq_vabs[i]);
        }
    }
    if (sound_ss_running == 0) {
        SsStart();
    }
    sound_ss_running = 1;
    if (sound_stereo != 0) {
        SsSetStereo();
    } else {
        SsSetMono();
    }
    return 0;
}

/* Set the reverb type and clear the reverb depth. */
static int sound_set_reverb_type(signed short type) {
    int result;
    result = SsUtSetReverbType(type);
    SsUtSetReverbDepth(0, 0);
    return result;
}

static void sound_set_reverb_depth(signed short depth) {
    SsUtSetReverbDepth(depth, depth);
}

static void sound_set_reverb_volume(signed short volume) {
    SsSetRVol(volume, volume);
}

static void sound_set_reverb_feedback(signed short feedback) {
    SsUtSetReverbFeedback(feedback);
}

static void sound_set_reverb_delay(signed short delay) {
    SsUtSetReverbDelay(delay);
}

static void sound_reverb_on(void) {
    SsUtReverbOn();
}

static void sound_reverb_off(void) {
    SsUtReverbOff();
}

void sound_set_mono(void) {
    SsSetMono();
    sound_stereo = 0;
    sound_stereo_mode_hook();
}

void sound_set_stereo(void) {
    SsSetStereo();
    sound_stereo = 1;
    sound_stereo_mode_hook();
}

static signed int sound_is_stereo(void) {
    return sound_stereo;
}

static signed int sound_get_vab_free_mark(void) {
    return vab_free_mark;
}

static u8 *sound_get_write_ptr(void) {
    return sound_write_ptr;
}

/* Mute, stop and close every sequence and VAB slot, then end libsnd. */
static s32 sound_release_all(void) {
    s32 *slotEntry;
    s32 *vabEntry;
    s32 slotId;
    s32 soundId;
    s32 slotsLeft;
    s32 vabsLeft;
    s32 sentinel;

    SsSetMVol(0, 0);
    soundId = 0;
    slotId = 0;
    for (;;) {
        sound_seq_stop(soundId, slotId);
        slotId += 1;
        if (slotId > 0) {
            slotId = 0;
            soundId += 1;
            if (soundId >= 6) {
                break;
            }
        }
    }
    sentinel = -1;
    slotEntry = (s32 *)seq_slots;
    slotsLeft = 5;
    do {
        if (*slotEntry != sentinel) {
            SsSeqClose(*slotEntry);
            *slotEntry = sentinel;
        }
        slotsLeft -= 1;
        slotEntry += 1;
    } while (slotsLeft >= 0);
    SsEnd();
    sentinel = -1;
    sound_ss_running = 0;
    vabEntry = vab_ids;
    vabsLeft = 5;
    do {
        if (*vabEntry >= 0) {
            SsVabClose(*vabEntry);
            *vabEntry = sentinel;
        }
        vabsLeft -= 1;
        vabEntry += 1;
    } while (vabsLeft >= 0);
    return 0;
}

/* Release every sequence slot from `seqFirst` on and every VAB slot from `vabFirst`
 * on, keeping the slots below them, and empty the queue. If the VAB at `vabFirst` is
 * loaded, vab_free_mark gets its vab_starts entry and sound_write_ptr gets the lower
 * (unsigned) of its vab_bodies entry and the first freed sequence's seq_data entry
 * (an empty seq_data entry is skipped). */
s32 sound_release_from(s32 vabFirst, s32 seqFirst) {
    s32 slot;
    s32 sub;
    u8 *vabMark;
    u8 *seqMark;

    vabMark = vab_bodies[vabFirst];
    vab_slot = vabFirst;
    seq_slot = seqFirst;
    if (vabMark != SOUND_BUFFER_NONE) {
        vab_free_mark = vab_starts[vabFirst];
        seqMark = seq_data[seqFirst];
        if (seqMark < vabMark && seqMark != SOUND_BUFFER_NONE) {
            sound_write_ptr = seqMark;
        } else {
            sound_write_ptr = vabMark;
        }
    }
    for (slot = seqFirst; slot < 6; slot++) {
        for (sub = 0; sub < 1; sub++) {
            SsSeqSetVol(slot, 0, 0);
            sound_seq_stop(slot, sub);
        }
    }
    for (slot = seqFirst; slot < 6; slot++) {
        if (*(s32 *)&seq_slots[slot] != -1) {
            SsSeqClose(seq_slots[slot].seqHandle);
            *(s32 *)&seq_slots[slot] = -1;
        }
        seq_data[slot] = SOUND_BUFFER_NONE;
        seq_vabs[slot] = -1;
        seq_offsets[slot] = -1;
    }
    for (slot = vabFirst; slot < 6; slot++) {
        if (vab_ids[slot] >= 0) {
            SsVabClose(vab_ids[slot]);
            vab_ids[slot] = -1;
        }
        vab_bodies[slot] = SOUND_BUFFER_NONE;
        vab_ends[slot] = SOUND_BUFFER_NONE;
        for (sub = 0; sub < 1; sub++) {
            seq_states[slot + sub] = SEQ_FREE;
            seq_handles[slot + sub] = -1;
        }
    }
    seq_slot = seqFirst;
    current_entry = 0;
    vab_slot = vabFirst;
    for (slot = 0; slot < 6; slot++) {
        queued_sounds[slot] = -1;
        queued_sound_has_arg[slot] = 0;
        entry_args[slot] = 0;
    }
    queue_cursor = 0;
    return 0;
}

signed int sound_current_entry(void) {
    return queued_sounds[current_entry];
}

/* One step of the sound-bank loader state machine (sound_load_state). It reads the
 * current sound's data, copies its header into place and opens it as a VAB,
 * streams the body to the SPU, or copies the whole entry and records where it
 * went. Returns the current sound. The cursors advance after the stores that use
 * them. */
s32 sound_load_step(void) {
    s32 entry;

    switch (sound_load_state) {
    case SOUND_LOAD_SEQ:
        seq_offsets[seq_slot] = SOUND_OFFSET(1);
        sound_seq_bytes = SOUND_OFFSET(2) - SOUND_OFFSET(0);
        memcpy(sound_write_ptr, sound_read_ptr, sound_seq_bytes);
        sound_load_state = SOUND_LOAD_IDLE;
        seq_data[seq_slot] = sound_write_ptr;
        seq_vabs[seq_slot] = vab_ids[entry_args[current_entry]];
        sound_write_ptr += SOUND_OFFSET(2) - SOUND_OFFSET(0);
        seq_slot++;
        current_entry++;
        break;
    case SOUND_LOAD_VAB_OPEN:
        memcpy(sound_write_ptr, sound_read_ptr, SOUND_OFFSET(1) - SOUND_OFFSET(0));
        vab_starts[vab_slot] = vab_free_mark;
        vab_ids[vab_slot] = SsVabOpenHead(sound_write_ptr, -1);
        sound_vab_stream_setup(vab_ids[vab_slot]);
        sound_load_state = SOUND_LOAD_VAB_STREAM;
        break;
    case SOUND_LOAD_VAB_DONE:
        SsVabTransCompleted(1);
        sound_load_state = SOUND_LOAD_IDLE;
        vab_bodies[vab_slot] = sound_write_ptr;
        sound_write_ptr += SOUND_OFFSET(1) - SOUND_OFFSET(0);
        vab_ends[vab_slot] = sound_write_ptr;
        vab_slot++;
        current_entry++;
        break;
    case SOUND_LOAD_IDLE:
        sound_schedule_load(queued_sounds[current_entry]);
        break;
    case SOUND_LOAD_VAB_HEADER:
        entry = queued_sounds[current_entry];
        if (afile_read_async(SOUND_FILE_ENTRY, sound_offsets[entry], sound_offsets[entry + 1] - sound_offsets[entry],
                          sound_read_ptr, 0) == 1) {
            sound_load_state = SOUND_LOAD_VAB_OPEN;
            sound_vab_stream_start(entry, sound_read_ptr, VAB_STREAM_CHUNK);
        }
        break;
    case SOUND_LOAD_WHOLE:
        entry = queued_sounds[current_entry];
        if (afile_read_async(SOUND_FILE_ENTRY, sound_offsets[entry], sound_offsets[entry + 2] - sound_offsets[entry],
                          sound_read_ptr, 0) == 1) {
            sound_load_state = SOUND_LOAD_SEQ;
        }
        break;
    case SOUND_LOAD_VAB_STREAM:
        if (sound_vab_stream_step() != 0) {
            sound_load_state = SOUND_LOAD_VAB_DONE;
            vab_free_mark += SOUND_OFFSET(2) - SOUND_OFFSET(1);
        }
        break;
    }
    return queued_sounds[current_entry];
}

/* Key sound `id` on a free voice in [from, to], or on the lowest-priority voice there
 * when none is free. Fireworks get a random pitch and volume. Returns the voice. */
s32 sound_key_on_free_voice(s32 id, s32 from, s32 to) {
    u8 status[24];
    s32 now;
    s32 voice;
    SoundDef *def;
    s16 vabId;
    s16 prog;
    s16 tone;
    s16 note;
    s16 fine;
    s16 voll;
    s16 volr;

    now = VSync(-1);
    SpuGetAllKeysStatus(status);
    voice = sound_find_free_voice(now, from, to, status);
    if (voice < 0) {
        voice = sound_find_lowest_voice(from, to);
    }
    def = &sound_defs[id];
    voice_categories[voice] = def->category;
    voice_priorities[voice] = VSync(-1);
    note = def->note;
    voll = volr = def->vol;
    vabId = def->vabId;
    prog = def->prog;
    tone = def->tone;
    fine = def->fine;
    if (id == SOUND_FIREWORK_LAUNCH) {
        note = rand_range(4) + 60;
        voll = rand_range(28) + 100;
        volr = rand_range(28) + 100;
    }
    if (id == SOUND_FIREWORK_BURST) {
        note = rand_range(9) + 56;
        voll = rand_range(28) + 100;
        volr = rand_range(28) + 100;
    }
    SsUtKeyOnV(voice, vabId, prog, tone, note, fine, voll, volr);
    voice_stamps[voice] = now;
    return voice;
}

/* Key a voice off unless its status byte is already 0 or 3. */
s32 sound_key_off_voice(s32 voice) {
    char status[24];

    SpuGetAllKeysStatus(status);
    if (status[voice] != 0 && status[voice] != 3) {
        voice_categories[voice] = 0;
        voice_priorities[voice] = VOICE_PRIORITY_NONE;
        SsUtKeyOffV(voice);
        return 0;
    }
    return 0;
}

/* Start reading the current sound's data. A sound with a second argument is read
 * whole (state 50). Otherwise its header comes first (state 30). With nothing
 * queued the loader goes idle. */
static void sound_schedule_load(s16 value) {
    s32 index;

    index = current_entry;
    sound_scheduled_value = value;
    if (queued_sounds[index] >= 0) {
        if (queued_sound_has_arg[index] != 0) {
            afile_read_async(SOUND_FILE_ENTRY, sound_offsets[value], sound_offsets[value + 2] - sound_offsets[value],
                          sound_read_ptr, 1);
            sound_load_state = SOUND_LOAD_WHOLE;
            return;
        }
        afile_read_async(SOUND_FILE_ENTRY, sound_offsets[value], sound_offsets[value + 1] - sound_offsets[value],
                      sound_read_ptr, 1);
        sound_load_state = SOUND_LOAD_VAB_HEADER;
        return;
    }
    sound_load_state = SOUND_LOAD_IDLE;
}

/* Start streaming sound `sound`'s VAB body through buffer `dest`, `chunkSize` bytes a
 * chunk (sound_vab_stream_setup sets the rest). */
static void sound_vab_stream_start(s32 sound, u8 *dest, s32 chunkSize) {
    vab_stream.current = sound;
    vab_stream.dest = dest;
    vab_stream.chunkSize = chunkSize;
}

/* Set up the VAB stream sound_vab_stream_start started: its body runs between the
 * sound's two offsets in sound_offsets, in chunks of VAB_STREAM_CHUNK bytes, into VAB `vabId`. */
static void sound_vab_stream_setup(s32 vabId) {
    s32 current;
    s32 previous;
    s32 future;
    u32 delta;
    u32 chunk_count;

    current = vab_stream.current;
    future = sound_offsets[current + 2];
    previous = sound_offsets[current + 1];
    delta = future - previous;
    chunk_count = delta / VAB_STREAM_CHUNK;
    vab_stream.vabId = vabId;
    vab_stream.state = VAB_STREAM_READ_START;
    vab_stream.chunkIndex = 0;
    vab_stream.offset = previous;
    vab_stream.chunkCount = chunk_count;
}

/* Advance the VAB stream by one step; returns 1 once every chunk is sent. */
static s32 sound_vab_stream_step(void) {
    VabStream *stream;

    stream = &vab_stream;
    if (stream->chunkIndex <= stream->chunkCount) {
        switch (stream->state) {
        case VAB_STREAM_READ_START:
            afile_read_async(SOUND_FILE_ENTRY, stream->offset, stream->chunkSize, stream->dest, 1);
            stream->state = VAB_STREAM_READ_WAIT;
            break;
        case VAB_STREAM_READ_WAIT:
            if (afile_read_async(SOUND_FILE_ENTRY, stream->offset, stream->chunkSize, stream->dest, 0) == 1) {
                SsVabTransBodyPartly(stream->dest, stream->chunkSize, stream->vabId);
                vab_stream_copy = *stream;
                stream->state = VAB_STREAM_READ_START;
                while (SsVabTransCompleted(1) == 0) {
                }
                vab_stream.offset += vab_stream.chunkSize;
                vab_stream.chunkIndex++;
            }
            break;
        }
        return 0;
    }
    return 1;
}

/* First index in [from, to] whose state byte is free (0) or already `busy`, and
 * whose table word is neither `value` nor `value + 1`; -1 if there is none. */
static s32 sound_find_free_voice(s32 value, s32 from, s32 to, u8 *states) {
    s32 *base;
    s32 *entry;
    s32 busy;
    u8 state;

    busy = 3;
    if (to >= from) {
        base = voice_stamps;
        entry = base + from;
        do {
            state = states[from];
            if (state == 0 || state == busy) {
                if ((u32)(*entry - value) >= 2U) return from;
            }
            from++;
            entry++;
        } while (to >= from);
    }
    return -1;
}

/* The lowest-priority voice in [first, last], trying categories 0 to 3 in turn and
 * returning the first category that has one; -1 if none has a sound. */
static s32 sound_find_lowest_voice(s32 first, s32 last) {
    s32 found;
    s32 category;
    s32 lowest;
    s32 i;

    found = -1;
    for (category = 0; category < 4; category++) {
        lowest = VOICE_PRIORITY_NONE;
        for (i = first; i <= last; i++) {
            if (voice_categories[i] == category && voice_priorities[i] < lowest) {
                found = i;
                lowest = voice_priorities[i];
            }
        }
        if (found != -1) return found;
    }
    return -1;
}
