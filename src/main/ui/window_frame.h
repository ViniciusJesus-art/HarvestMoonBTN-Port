#ifndef WINDOW_FRAME_H
#define WINDOW_FRAME_H


/* The bevelled window frame, drawn through the primitive builders. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

void window_frame_draw(u_long *ot, s16 x, s16 y, s16 w, s16 h);

#endif /* WINDOW_FRAME_H */
