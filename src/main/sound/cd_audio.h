#ifndef CD_AUDIO_H
#define CD_AUDIO_H


/* The CD audio (XA) player. */
#include <sys/types.h>
#include <libcd.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

void sound_stereo_mode_hook(void);
void xa_search_files(void);
void cd_audio_fade_tick(void);
void cd_audio_stop(void);
/* What cd_audio_get_state reports: nothing requested, a request waiting for its
 * seek, or the fade-out running. */
enum CdAudioState {
    CD_AUDIO_IDLE = 0,
    CD_AUDIO_REQUESTED = 1,
    CD_AUDIO_FADING = 2,
};

/* The XA tracks the game names by meaning; the others stay numeric. */
enum CdAudioTrack {
    CD_TRACK_COW_PRAISE = 0x09,      /* a cow is praised: cured, bred, talked to, or it gives a product */
    CD_TRACK_COW_NEGLECT = 0x0A,     /* a cow is neglected or hit with a tool */
    CD_TRACK_SHEEP_PRAISE = 0x0B,    /* a sheep is praised, as the cow's praise */
    CD_TRACK_SHEEP_NEGLECT = 0x0C,   /* a sheep is neglected or hit with a tool */
    CD_TRACK_HORSE_PRAISE = 0x0D,    /* the horse is praised */
    CD_TRACK_HORSE_NEGLECT = 0x0E,   /* the horse is neglected or hit with a tool */
    CD_TRACK_CHICKEN_NOTICE = 0x13,  /* the chicken notice, when a chicken is well (daily announce) */
    CD_TRACK_VISITORS = 0x14,        /* the day's visitors have arrived (daily announce) */
    CD_TRACK_CHIME = 0x15,           /* the noon and evening chimes */
    CD_TRACK_COOKING_RESULT = 0x16,  /* a cooking result is shown */
    CD_TRACK_PHOTO_JINGLE = 0x19,    /* the photo result's jingle */
};

s32 cd_audio_get_state(void);
void cd_audio_play_track(s32);


#endif /* CD_AUDIO_H */
