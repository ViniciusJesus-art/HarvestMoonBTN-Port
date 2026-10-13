#ifndef FARM_FRAME_H
#define FARM_FRAME_H

/* The farm's frame: the per-frame mode switch and the subsystems it runs, in order, and
 * the one-shot status latch. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

void farm_mode_tick(void);
void farm_one_shot_latch_set(void);
void farm_run_frame_subsystems(void);

#endif /* FARM_FRAME_H */
