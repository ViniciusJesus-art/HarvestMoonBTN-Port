/* Cues: resetting, setting and counting down an actor's cue, and drawing its frame. */

#include "types.h"
#include "actor.h"
#include "anim.h"
#include "cue.h"
#include "farm_state.h"
#include "ui_icons.h"

/* Clear the cue: no icon (id -1) and no phase. */
void cue_reset(ItemCue *cue) {
    cue->id = -1;
    cue->phase = 0;
}

/* Show icon `id` and start its countdown at `phase` frames. */
void cue_set(ItemCue *cue, s16 id, s16 phase) {
    cue->id = id;
    cue->phase = phase;
}

/* Count the cue down one frame. Returns 0 while it is still running; returns 1 once it is out,
 * and also while the game is paused, since the phase does not move then. */
s32 cue_countdown(ItemCue *cue) {
    if (!(game_state.globalFlags & GLOBAL_FLAG_PAUSED) && cue->phase != 0) {
        cue->phase = cue->phase - 1;
        return 0;
    }
    return 1;
}

/* Draw the icon frame for the cue's phase, one frame per 10 phase units; nothing is drawn at phase 0. */
void cue_draw_frame(ItemCue *object, u_long *ot, s16 x, s16 y) {
    s16 tableValue;
    s32 divisor;
    s16 remainder;

    if (object->phase != 0) {
        tableValue = anim_range_frames(&ui_icon_anim, object->id);
        divisor = object->phase / 10;
        remainder = divisor % tableValue;
        anim_draw_sprt_cursor(ot, &ui_icon_anim, object->id, tableValue - remainder - 1,
                      x, y);
    }
}
