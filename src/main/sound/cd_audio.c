/* CD audio: the XA streams the game plays, found on the disc by name, and their playback with its fade-out. */
#include <sys/types.h>
#include <libcd.h>
#include <libsnd.h>
#include "types.h"
#include "cd_audio.h"
#include "event_flags.h"
#include "farm_state.h"
#include "main.h"

static s32 cd_audio_restart(s32 mode);

CdlFILE xa_search_results[4];

/* The four XA streams xa_search_files looks up on the disc. */
static char *xa_file_names[4] = {"\\STR\\01.XA;1", "\\STR\\02.XA;1", "\\STR\\03.XA;1",
                             "\\STR\\04.XA;1"};

/* Empty: the stereo mode hook does nothing. */
void sound_stereo_mode_hook(void) {
}

/* Look up the four XA files on the disc, filling xa_search_results. */
void xa_search_files(void) {
    s32 i;
    for (i = 0; i < 4; i++) {
        while (!CdSearchFile(&xa_search_results[i], xa_file_names[i])) {
        }
    }
}

s16 cd_audio_volume;
s32 cd_audio_start;
s32 cd_audio_restarts;
s32 cd_audio_pos;
s32 cd_audio_step;
s32 cd_audio_track;
s32 cd_audio_file;
s32 cd_audio_channel;
s32 cd_audio_unused_8005E5A0;
s32 cd_audio_unused_8005E5A4;

/* One-shot request to stop the CD audio: cd_audio_stop acts on it and clears it. */
s16 cd_audio_request = 0;

/* The steps of cd_audio_step. */
enum CdAudioStep {
    CD_AUDIO_COUNTDOWN_0 = 0,
    CD_AUDIO_COUNTDOWN_1 = 1,
    CD_AUDIO_COUNTDOWN_2 = 2,
    CD_AUDIO_START = 3,
    CD_AUDIO_FADE = 4,
};

/* Per-frame driver of the CD-audio fade (while cd_audio_request is set). Steps
 * 0..2 of cd_audio_step are a countdown; step 3 waits for the seek to complete
 * and starts playback; step 4 fades the volume cd_audio_volume down by 1..3 a
 * frame (by main_get_speed_mode's mode), restarting playback from the recorded
 * position if the drive stops or falls 300 sectors behind, and stops the
 * audio (cd_audio_stop) once the volume reaches 0. */
void cd_audio_fade_tick(void) {
    u8 result[8];
    s32 step;
    s32 status;
    s32 pos;
    s32 volume;

    if (cd_audio_request == 0) {
        return;
    }
    switch (main_get_speed_mode()) {
    case MAIN_SPEED_FAST_2:
        step = 2;
        break;
    case MAIN_SPEED_FAST_3:
        step = 3;
        break;
    case MAIN_SPEED_NORMAL:
    default:
        step = 1;
        break;
    }
    switch (cd_audio_step) {
    case CD_AUDIO_COUNTDOWN_0:
    case CD_AUDIO_COUNTDOWN_1:
    case CD_AUDIO_COUNTDOWN_2:
        cd_audio_step++;
        break;
    case CD_AUDIO_START:
        if (CdSync(1, result) == 2 && cd_audio_restart(CdlReadS) == 0) {
            cd_audio_step++;
        }
        break;
    case CD_AUDIO_FADE:
        status = CdSync(1, result);
        if (status == 5) {
            cd_audio_restart(CdlReadS);
            break;
        }
        if (status != 2) {
            break;
        }
        if (cd_audio_volume <= 0) {
            cd_audio_stop();
            break;
        }
        if (cd_audio_pos < cd_audio_start - 300) {
            cd_audio_restart(CdlReadS);
            break;
        }
        if (CdLastCom() == CdlGetlocP) {
            pos = CdPosToInt((CdlLOC *)&result[5]);
            if (pos > 0) {
                cd_audio_pos = pos;
            }
        }
        cd_audio_request = 2;
        CdControlF(CdlGetlocP, 0);
        volume = cd_audio_volume - step;
        if (volume < 0) {
            volume = 0;
        }
        cd_audio_volume = volume;
        if (cd_audio_volume < 33) {
            SsSetSerialVol(0, cd_audio_volume * 4, cd_audio_volume * 4);
        } else {
            SsSetSerialVol(0, 45, 45);
        }
        break;
    }
}

/* Silence the CD audio and stand the drive by, if a request is pending. */
void cd_audio_stop(void) {
    if (cd_audio_request != 0) {
        SsSetSerialVol(0, 0, 0);
        CdControl(CdlStandby, 0, 0);
        cd_audio_request = 0;
    }
}

signed int cd_audio_get_state(void) {
    signed short value;
    value = cd_audio_request;
    if (value == 0) {
        return CD_AUDIO_IDLE;
    }
    if (value == 2) {
        return CD_AUDIO_FADING;
    }
    return CD_AUDIO_REQUESTED;
}

/* One row of the XA track table: the volume, then the file and the channel. */
typedef struct {
    u16 volume;
    s16 file;
    s16 channel;
} CdAudioTrackEntry;

static CdAudioTrackEntry cd_audio_tracks[26] = {
    {0x0AF2, 0x0000, 0x0000}, {0x0B10, 0x0000, 0x0001}, {0x0108, 0x0000, 0x0002},
    {0x0126, 0x0000, 0x0003}, {0x03EA, 0x0000, 0x0004}, {0x04F8, 0x0000, 0x0005},
    {0x02EE, 0x0000, 0x0006}, {0x003B, 0x0001, 0x0000}, {0x007D, 0x0001, 0x0001},
    {0x009B, 0x0001, 0x0002}, {0x0089, 0x0001, 0x0003}, {0x007D, 0x0001, 0x0004},
    {0x0083, 0x0001, 0x0005}, {0x0089, 0x0001, 0x0006}, {0x0089, 0x0001, 0x0007},
    {0x007D, 0x0002, 0x0000}, {0x0077, 0x0002, 0x0001}, {0x007D, 0x0002, 0x0002},
    {0x007D, 0x0002, 0x0003}, {0x00FB, 0x0003, 0x0000}, {0x00E9, 0x0003, 0x0001},
    {0x01D3, 0x0003, 0x0002}, {0x00E9, 0x0003, 0x0003}, {0x0119, 0x0003, 0x0004},
    {0x00B9, 0x0003, 0x0005}, {0x01D3, 0x0003, 0x0006},
};

/* Start XA track `track`: stop the current stream, set the CD filter to the track's
 * channel, seek to the track's file and set the mode. */
void cd_audio_play_track(s32 track) {
    u8 mode[8];
    u8 filter[8];
    CdAudioTrackEntry *entry;
    u16 *value;
    s32 pos;

    if (game_state.system.menuState != MENU_STATE_IDLE) {
        return;
    }
    if (event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 1 && game_state.requests.state != FARM_REQUEST_RUN) {
        return;
    }
    entry = &cd_audio_tracks[track];
    value = &entry->volume;
    CdFlush();
    cd_audio_stop();
    SsSetSerialVol(0, 0, 0);
    cd_audio_request = 1;
    cd_audio_step = CD_AUDIO_COUNTDOWN_0;
    cd_audio_track = track;
    cd_audio_channel = entry->channel;
    cd_audio_file = entry->file;
    cd_audio_volume = *value;
    filter[0] = 1;
    filter[1] = cd_audio_channel;
    while (CdControlB(CdlSetfilter, filter, 0) != 1) {
    }
    pos = CdPosToInt(&xa_search_results[cd_audio_file].pos);
    cd_audio_start = pos;
    cd_audio_pos = pos;
    cd_audio_restarts = 0;
    mode[0] = 0xC8;
    while (CdControlB(CdlSetmode, mode, 0) != 1) {
    }
}

/* Seek back to cd_audio_start and start the track's stream again with `mode`; -1 when
 * the drive refuses the command. */
static s32 cd_audio_restart(s32 mode) {
    CdlLOC position;
    u16 *value_ptr;
    int command;
    u16 value;
    value_ptr = &cd_audio_tracks[cd_audio_track].volume;
    value = *value_ptr;
    cd_audio_request = 1;
    cd_audio_volume = value;
    command = mode & 0xFF;
    CdIntToPos(cd_audio_start, &position);
    if (CdControl(command, (u_char *)&position, 0) != 1) {
        return -1;
    }
    cd_audio_pos = cd_audio_start;
    cd_audio_restarts = cd_audio_restarts + 1;
    return 0;
}
