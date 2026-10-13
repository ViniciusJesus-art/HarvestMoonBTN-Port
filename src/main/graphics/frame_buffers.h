#ifndef FRAME_BUFFERS_H
#define FRAME_BUFFERS_H

/* The frame records: linking primitives into the OT, the debug hook, and laying out
 * both frames' OTs and primitive buffers. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

void *ot_insert_prim(s32 index, s32 advance);
void debug_hook_run(s32 unused0);
void frame_buffers_layout(void);

#endif /* FRAME_BUFFERS_H */
