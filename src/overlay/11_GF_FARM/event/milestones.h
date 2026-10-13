#ifndef MILESTONES_H
#define MILESTONES_H

/* Milestones: the events that mark the farm's progress. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

void milestone_result_show(void);
void milestone_timer_tick(void);
void milestone_result_begin(void);
void milestone_result_step(void);
void milestone_reset_timers(void);
void milestone_set_timer(s16 arg);
void milestone_arm_countdown(void);
void milestone_refresh(void);
void milestone_refresh_all(void);
void milestone_tick(void);
s32 milestone_status_query(s32 mode);


#endif /* MILESTONES_H */
