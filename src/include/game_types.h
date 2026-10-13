#ifndef GAME_TYPES_H
#define GAME_TYPES_H

/* Shared game structs that are passed by pointer or used as locals. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"

/* 3D integer vector used by the game's own math helpers. */
typedef struct Vec3 {
    s32 x;
    s32 y;
    s32 z;
} Vec3;

/* A place: the map and the area within it, copied whole. */
typedef struct LimitPair {
    s16 value;      /* the map; a walker's step count */
    s16 location;   /* the area */
} LimitPair;

/* Dispatch tables: the game's state machines are arrays of these, indexed by a state id. */
typedef void (*Func)(void);

/* The GTE scratch used by the camera rotation. */
typedef struct {
    MATRIX  rotation;
    MATRIX  translation;
    SVECTOR angle;
    VECTOR  position;
    long    flag;
} CameraWork;

#endif /* GAME_TYPES_H */
