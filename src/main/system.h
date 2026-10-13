#ifndef SYSTEM_H
#define SYSTEM_H

/* The host poll, the display on and off, and the frame buffer layout. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

s32 host_poll(void);
void display_on(void);
void display_off(void);

#endif /* SYSTEM_H */
