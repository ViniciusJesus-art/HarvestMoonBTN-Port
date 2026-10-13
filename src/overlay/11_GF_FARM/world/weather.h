#ifndef WEATHER_H
#define WEATHER_H

/* Weather: the daily roll, the falling particles and the sky tint. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* A value drifting toward a target, stepped once per call. */
typedef struct FarmDrift {
    s32 red;
    s32 green;
    s32 blue;
    s32 stepRed;
    s32 stepGreen;
    s32 stepBlue;
    s32 stepsTaken;
    s32 stepsToTake;
    s32 index;          /* the entry of the weather drift table it was seeded from */
} FarmDrift;

/* FarmWorld.weather. */
enum Weather {
    WEATHER_SUNNY = 0,
    WEATHER_RAIN = 1,
    WEATHER_SNOW = 2,
    WEATHER_STORM = 3,
    WEATHER_BLIZZARD = 4,
};

/* The screen scroll the drifting particles follow. */
typedef struct FarmScroll {
    s32 z;
    s32 x;
    s32 unused8;
    s32 yaw;
    s32 unused10;
    u16 dx;
    u16 dy;
    s16 a18;
    s16 b1A;
    s16 c1C;
    u16 pad1E;
    SVECTOR centre;         /* the camera centre */
} FarmScroll;

/* One of the drifting weather particles. */
typedef struct FarmParticle {
    s16 x;
    s16 y;
    s16 sprite;                 /* 0: no particle */
    u16 frame;
    s16 delay;                  /* counts down before it moves */
    u16 age;
    u16 flags;                  /* 0x4000: moving, 0x1000: fast, 0x2000: doubled */
} FarmParticle;

/* FarmParticle.flags. */
enum FarmParticleFlag {
    PARTICLE_FLAG_FAST = 0x1000,
    PARTICLE_FLAG_DOUBLED = 0x2000,
    PARTICLE_FLAG_MOVING = 0x4000,
};

void field_reset_cells(void);
s16 weather_roll_tomorrow(void);
s16 weather_step_today(void);
void weather_fx_frame(void);
void drift_select_hour(void);
void weather_fx_init(void);
void weather_frame_update_hook(void);
void drift_reset(void);
void weather_fx_set_mode(s16 mode);
void weather_scroll_nudge(s32 unused);
void greenhouse_reset_level(void);


#endif /* WEATHER_H */
