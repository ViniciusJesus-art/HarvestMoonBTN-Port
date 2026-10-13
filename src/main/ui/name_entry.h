#ifndef NAME_ENTRY_H
#define NAME_ENTRY_H


/* The name-entry screen: the keyboard grid, its cursor and the name. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "sound.h"

void text_load_record(void *record);
void name_sound_reset(void);
void name_sound_begin(SoundSequence *sequence);
s32 name_sound_step(void);
void name_entry_reset(void);
void name_entry_start(u16 *name);
void name_entry_start_no_cancel(u16 *name);
void name_entry_start_cancellable(u16 *name);
/* What the name-entry step returns: IDLE when no entry runs (an event's name entry starts
 * one), EDITING while it runs, CANCELLED (-1, the script's slot is set to 1)
 * or ACCEPTED (1, the name is copied). */
enum NameEntryStep {
    NAME_STEP_CANCELLED = -1,
    NAME_STEP_IDLE = 0,
    NAME_STEP_ACCEPTED = 1,
    NAME_STEP_EDITING = 2,
};

s32 name_entry_step(void);


#endif /* NAME_ENTRY_H */
