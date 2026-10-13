#ifndef CLOCK_EVENTS_H
#define CLOCK_EVENTS_H

/* The clock's event callbacks, in the order of clock_callbacks: the ledger roll, the
 * end-of-day sequence, the season and hour refreshes and the objects shown on a date. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

s32 clock_callback_nothing(void);
s32 estate_ledger_roll(void);
s32 scene_overnight_step(void);
s32 clock_refresh_dated_event_flag(void);
void farm_overnight_finish(void);
s32 estate_season_refresh(void);
s32 farm_hourly_refresh(void);
void event_request_drain(void);
void farm_rest_request(void);
s32 clock_show_object_4(void);
s32 clock_show_object_1(void);
s32 clock_show_object_2(void);
s32 clock_show_object_0(void);

#endif /* CLOCK_EVENTS_H */
