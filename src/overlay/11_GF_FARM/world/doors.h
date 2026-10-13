#ifndef DOORS_H
#define DOORS_H

/* The farm doors. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

void door_entries_update(void);
u16 door_entry_for_event(s32 arg);


#endif /* DOORS_H */
