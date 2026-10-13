#ifndef SOUND_H
#define SOUND_H

/* The sound library: sequences, voices, the VAB and the sound loader. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* What the master-volume fader is doing. */
enum SoundFaderMode {
    SOUND_FADER_IDLE = 0,
    SOUND_FADER_FADE_IN = 1,
    SOUND_FADER_FADE_OUT = 2
};

/* The VAB stream's step: start the next chunk's read, or wait for it and transfer. */
enum VabStreamState {
    VAB_STREAM_READ_START = 0,
    VAB_STREAM_READ_WAIT = 1
};

/* Master-volume fader record. */
typedef struct SoundFader {
    s32 unused0;
    enum SoundFaderMode mode;
    s32 volume;             /* current volume << 16, clamped to [0, 0x7F0000] */
    s32 step;               /* added per tick */
    s32 maxVolume;          /* 0x7F0000 */
    s32 ready;
} SoundFader;

/* The current sequence record. */
typedef struct SoundSequence {
    u16 unused0;
    u16 icon;
    u16 day;
} SoundSequence;

/* A VAB body streamed to the SPU one chunk per call. */
typedef struct {
    s32 current;            /* the sound's entry in the offset table */
    u8 *dest;               /* chunk buffer */
    u32 chunkSize;
    u32 chunkCount;
    u32 chunkIndex;
    s32 offset;             /* file offset of the next chunk */
    s32 vabId;
    enum VabStreamState state;
} VabStream;

/* A sound slot that holds no voice. */
#define SOUND_VOICE_NONE -1
void sound_init(void);
void sound_clear_stamps(void);
s32 sound_begin_load(u8 *write_ptr, u8 *read_ptr, s32 unused2, s32 unused3);
s32 sound_queue_seq(s32 seq);
s32 sound_queue_seq_pair(s32 seq, s32 entryArg);
s32 sound_seq_start(s32 sound_id, s32 slot_id, s32 left, s32 right, s32 handle);
s32 sound_seq_volume_if_idle(s32 sound_id, s32 slot_id, s32 unused2, s32 volume, s32 unused4);
s32 sound_seq_stop(s32 sound_id, s32 slot_id);
s32 sound_voice_stop(s16 voice);
s32 sound_voice_set_volume(s16 voice, s16 voll, s16 volr);
s32 sound_update(void);
void sound_set_mono(void);
void sound_set_stereo(void);
s32 sound_release_from(s32 vabFirst, s32 seqFirst);
s32 sound_current_entry(void);
s32 sound_load_step(void);
s32 sound_key_on_free_voice(s32 id, s32 from, s32 to);
s32 sound_key_off_voice(s32 arg);

/* One sound: the key-on arguments for it, plus its priority category. */
typedef struct {
    s16 vabId;
    s16 category;
    s16 prog;
    s16 tone;
    s16 note;
    s16 fine;
    s16 vol;
} SoundDef;

typedef struct Vol {
    s16 seqHandle;      /* the sequence handle the sound calls take */
    s16 pad;
} Vol;

/* The fireworks' sounds. */
enum FireworkSound {
    SOUND_FIREWORK_LAUNCH = 0x46,
    SOUND_FIREWORK_BURST = 0x47,
};

/* The farm animals' calls: a hen's cry, a hen startled by the player and a chick's peep. */
enum AnimalSound {
    SOUND_CHICKEN_VOICE = 0x17,
    SOUND_CHICKEN_STARTLED = 0x18,
    SOUND_CHICK_VOICE = 0x1B,
};

/* The interface's sounds: the confirm, the cancel and the refusal of a menu, a screen opening or
 * closing, the cursor step and the text blip. */
enum InterfaceSound {
    SOUND_MENU_CONFIRM = 0,
    SOUND_MENU_CANCEL = 1,
    SOUND_MENU_REFUSE = 2,
    SOUND_MENU_OPEN_CLOSE = 3,
    SOUND_TEXT_BLIP = 4,
    SOUND_MENU_CURSOR = 5,
};

/* The last of the 24 voices (voices are numbered 0..23). */
#define SOUND_LAST_VOICE 0x17

#endif /* SOUND_H */
