#ifndef CUE_H
#define CUE_H

/* Cues: the cue steppers, the cue history and the cue messages. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "actor.h"

/* One entry of a message queue: what to say and who says it. `kind` is -1 in a
 * free entry, which is how the queue tests its head and releases its tail. */
typedef struct FarmMessage {
    s16 message;        /* the message the menu opens */
    s8  kind;           /* -1 = free */
    s8  timer;          /* counts down and fires the entry at <= 0, so it is signed */
} FarmMessage;

/* One step of a cue table: `pose` is shown for `frames` frames, then the next step.
 * A `pose` of CUE_POSE_END ends the table. */
typedef struct FarmCueStep {
    s16 pose;
    s16 frames;
} FarmCueStep;

enum CuePose {
    CUE_POSE_END = -1,
};

/* The icon a cue shows: the id of its cue_set. */
enum CueIcon {
    CUE_ICON_TALK = 0x22,       /* the villager greets the player */
    CUE_ICON_SCOLD = 0x1F,      /* the animal scolds the player */
    CUE_ICON_NEGLECT = 0x24,    /* the animal is neglected: affection falls */
    CUE_ICON_PRAISE = 0x25,     /* the animal is praised: affection rises */
};

/* The phase a cue_set gives its icon: how long the icon stays up. */
enum CuePhase {
    CUE_PHASE_SHORT = 0x1E,     /* the scold icon of a cow or horse */
    CUE_PHASE_LONG = 0x2D,      /* the praise and neglect icons */
};

/* The taskId of a cue runner's step: CUE_STEP_START arms the table, a positive step plays it
 * (CUE_STEP_PLAY is the first), and CUE_STEP_STOP ends it and reports the cue as finished. */
enum CueStep {
    CUE_STEP_START = 0,
    CUE_STEP_PLAY = 1,
    CUE_STEP_STOP = -1,
};

/* A cue runner: steps an actor through a cue table. Step 0 starts the table, positive
 * steps play it, and -1 stops it and reports the cue as finished. */
#define FARM_CUE_RUNNER(name, init...)                                      \
    s32 name(FarmActor *self) {                                             \
        static FarmCueStep steps[] = { init };                              \
        s32 finished = 0;                                                   \
        s16 step = self->taskId;                                            \
                                                                            \
        switch (step) {                                                     \
        case CUE_STEP_START:                                                \
            self->taskId = CUE_STEP_PLAY;                                   \
            self->flags |= ACTOR_FLAG_CUE_RUNNING;                          \
            break;                                                          \
        case CUE_STEP_STOP:                                                 \
            finished = 1;                                                   \
            self->interruptTime = 300;                                      \
            self->flags &= ~ACTOR_FLAG_CUE_RUNNING;                         \
            break;                                                          \
        case CUE_STEP_PLAY:                                                 \
        default:                                                            \
            if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime) != 0) { \
                self->taskId = CUE_STEP_STOP;                               \
            }                                                               \
            break;                                                          \
        }                                                                   \
        return finished;                                                    \
    }

/* FARM_CUE_RUNNER without the interruptTime write when the cue finishes. */
#define FARM_CUE_RUNNER_NO_TIMER(name, init...)                             \
    s32 name(FarmActor *self) {                                             \
        static FarmCueStep steps[] = { init };                              \
        s32 finished = 0;                                                   \
        s16 step = self->taskId;                                            \
                                                                            \
        switch (step) {                                                     \
        case CUE_STEP_START:                                                \
            self->taskId = CUE_STEP_PLAY;                                   \
            self->flags |= ACTOR_FLAG_CUE_RUNNING;                          \
            break;                                                          \
        case CUE_STEP_STOP:                                                 \
            finished = 1;                                                   \
            self->flags &= ~ACTOR_FLAG_CUE_RUNNING;                         \
            break;                                                          \
        case CUE_STEP_PLAY:                                                 \
        default:                                                            \
            if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime) != 0) { \
                self->taskId = CUE_STEP_STOP;                               \
            }                                                               \
            break;                                                          \
        }                                                                   \
        return finished;                                                    \
    }

/* A cue runner variant: its start path does not set the cue-running flag. */
#define FARM_CUE_RUNNER_SHARED_STORE(name, init...)                         \
    s32 name(FarmActor *self) {                                             \
        static FarmCueStep steps[] = { init };                              \
        s32 finished = 0;                                                   \
        s16 step = self->taskId;                                            \
                                                                            \
        switch (step) {                                                     \
        case CUE_STEP_START:                                                \
            self->taskId = CUE_STEP_PLAY;                                   \
            break;                                                          \
        case CUE_STEP_STOP:                                                 \
            finished = 1;                                                   \
            self->flags &= ~ACTOR_FLAG_CUE_RUNNING;                         \
            break;                                                          \
        case CUE_STEP_PLAY:                                                 \
        default:                                                            \
            if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime) != 0) { \
                self->taskId = CUE_STEP_STOP;                               \
            }                                                               \
            break;                                                          \
        }                                                                   \
        return finished;                                                    \
    }
void cue_reset(ItemCue *cue);
void cue_set(ItemCue *cue, s16 id, s16 phase);
s32 cue_countdown(ItemCue *cue);
void cue_draw_frame(ItemCue *cue, u_long *ot, s16 x, s16 y);

#endif /* CUE_H */
