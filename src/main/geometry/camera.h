#ifndef CAMERA_H
#define CAMERA_H


/* The camera: projection, view and the camera block. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

typedef struct ClipPlane {
    s16 x;
    s16 y;
    s16 z;
} ClipPlane;

void camera_init(void);


#endif /* CAMERA_H */
