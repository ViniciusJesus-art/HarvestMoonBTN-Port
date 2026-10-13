#ifndef EVENT_REQUEST_H
#define EVENT_REQUEST_H

/* Event requests: the queue of events waiting to start, its handlers, and the message
 * timers. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

void scene_item_queue_reset(void);
void event_request_step(void);
struct FarmRequestSlot *event_request_claim(s32 event);
struct FarmRequestSlot *event_request_claim_any(s32 event);
void scene_message_timers_tick(void);
s32 event_queue_has_entries(void);

#endif /* EVENT_REQUEST_H */
