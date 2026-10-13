#ifndef CUE_HISTORY_H
#define CUE_HISTORY_H

/* The farm's cue-history scripts: the cue each farm item plays for a day, by
 * the flags set. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

void cue_history_reset(u8 *base);
void item_select_schedule_step(s16 arg);

#endif /* CUE_HISTORY_H */
