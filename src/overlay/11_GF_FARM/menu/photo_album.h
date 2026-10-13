#ifndef PHOTO_ALBUM_H
#define PHOTO_ALBUM_H

/* The photo album. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

void photo_viewer_start(void);
void photo_viewer_step(void);
void ending_track_reset(void);
void ending_track_request(s32 arg);
void ending_track_tick(void);
void photo_sequence_start(void);
void photo_sequence_tick(void);

#endif /* PHOTO_ALBUM_H */
