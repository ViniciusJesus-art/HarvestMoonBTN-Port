#ifndef MG5_H
#define MG5_H

/* Types for minigame 5 (the racing game). Only this overlay uses them. */

#include "types.h"
#include "anim.h"

/* A racer's id: the player, or one of the five rivals in grid order. */
enum Mg5RacerId {
    MG5_RACER_PLAYER = 0,
    MG5_RACER_RIVAL_1 = 1,
    MG5_RACER_RIVAL_2 = 2,
    MG5_RACER_RIVAL_3 = 3,
    MG5_RACER_RIVAL_4 = 4,
    MG5_RACER_RIVAL_5 = 5,
};

typedef struct Mg5Racer {
    s16 id;         /* a Mg5RacerId */
    s16 status;     /* a Mg5RacerStatus */
    s16 timer;      /* non-zero while active */
    s16 delay;
    s16 positionStep;   /* set from mg5_position_step_value */
    u8  pad0A[0x0C - 0x0A];
    s32 position;   /* 20.12 fixed point, clamped to 0..0x32000 */
    SVECTOR start;  /* the racer's place on the track (start.vx is its track x); its animations are drawn here */
    AnimState body;
    AnimState head;
} Mg5Racer;


/* The state block for the whole minigame. */
typedef struct Mg5State {
    s16 dispatch;           /* index into the dispatch table */
    s16 step;               /* the next start step (a Mg5StartStep) */
    s32 paused;
    s32 resultLatched;
    s16 raceTime;
    s16 idleFrames;
    Mg5Racer racers[6];     /* 0 is the player */
    Mg5Racer extras[2];     /* the two extra actors */
    SVECTOR marker;         /* where the marker sprite is drawn */
    AnimState stageSprite;  /* the landing splash, played through once when the screen lands */
    AnimState rowSprites[4]; /* sprite rows: backdrop, marker (two), rank */
    s16 voice;
    s16 engineVoice;
    s16 sceneTimer;
    /* Tuning values, set after the rest of the block is cleared. A stride lasts
     * strideFrames frames; the per-frame moves divide the step by it. */
    s16 idleLimit;
    s16 driftPercent;       /* a drift moves dashStep times this percent */
    s16 trackWidth;
    s16 strideFrames;
    s16 dashStep;           /* the dash's step per stride */
    s16 rollScale;          /* scales the stumble and dash rolls */
    s16 coastStep;          /* the coast's step per stride */
    s16 idleTimeout;
    s16 decayRivalStep;     /* the last rival's step base, over 30 */
    s16 decayPercent;       /* the last rival's step falls by this percent each lap */
    s16 steadyRivalStep;    /* the fourth rival's step, over 30 */
    s16 racerStepPercent;   /* the other racers' steps, in percent */
    u8  pad4E6[0x4E8 - 0x4E6];
} Mg5State;

void mg5_step_run(void);

#endif /* MG5_H */
