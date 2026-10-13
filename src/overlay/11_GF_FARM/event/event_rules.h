#ifndef EVENT_RULES_H
#define EVENT_RULES_H

/* The rules of the farm's event scripts: the conditions an event checks before it starts
 * (time, place, affection, the day's items) and the scripts that open the doors or put the
 * farm into a timed state. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

s32 event_pending_pair_lookup(s16 id, u16 zoneValue, s32 *result, s32 process);

#endif /* EVENT_RULES_H */
