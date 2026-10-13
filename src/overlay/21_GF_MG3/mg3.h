#ifndef MG3_H
#define MG3_H

/* Overlay 21_GF_MG3 (minigame 3): its record types. */

#include "types.h"
#include "anim.h"
#include "villager.h"

/* One target slot; each record holds three of them. */
typedef struct Mg3Target {
    s32 active;             /* 0 = free */
    s16 anim;               /* the animation the sprite keeps running */
    s16 timer;
    s16 start;              /* the frame the shot was launched */
    s16 duration;           /* steps */
    s16 pos[3];             /* starts at the record's pos */
    u8  pad12[0x14 - 0x12];
    s16 step[3];            /* per-step x/y/z delta */
    u8  pad1A[0x1C - 0x1A];
    AnimState sprite;
} Mg3Target;

/* A record's state (Mg3Record.state). Zero is the start state the round resets to. */
enum Mg3RecordState {
    MG3_RECORD_IDLE = 2,
    MG3_RECORD_READY = 4,       /* a shot is armed; the record goes idle when its animation ends */
    MG3_RECORD_ENGAGED = 6,     /* a rival is in range */
    MG3_RECORD_HIT = 7,         /* struck; it goes to DONE after 30 frames */
    MG3_RECORD_DONE = 9,
};

/* One record of the minigame's target array. */
typedef struct Mg3Record {
    CharacterId villager;   /* the character this record stands for */
    s16 index;            /* the record's own index; < 3 is group 0 */
    s32 inPlay;             /* 1 while in play */
    s16 state;              /* an Mg3RecordState */
    s16 pos[3];             /* x, y, z */
    u8  pad10[0x12 - 0x10];
    s16 score;
    s16 speed;
    u8  pad16[0x18 - 0x16];
    s32 ready;
    s16 aim;                /* the cursor, kept in 3..5, or the record aimed at (NPC records) */
    s16 hitTimer;
    AnimState *sprite;
    Mg3Target targets[3];
} Mg3Record;

typedef struct Mg3State {
    s16 state;              /* step id of the round state machine */
    s16 roundIndex;
    s32 paused;
    s16 roundTimer;         /* frames left in the round */
    u8  pad0A[0xC - 0xA];
    u16 *captionText;
    Mg3Record records[6];
    s16 roundParams[6];     /* indexed by round result */
    s16 roundSeconds;       /* the round's length, in seconds */
    s16  laneInterval;
    s16  stateInterval;
    s16  arcScale;          /* a shot's frames in the air: arcScale * 400 / the record's speed */
    s16 unusedA;
    s16 unusedB;
    s16 unusedC;
    s16  rivalRange;
    s16  hitRange;
    s16 unusedD;
    s16 unusedE;
    /* Roll chances: the lane picks use percent, the record-state rolls are out of 128.
     * A record is idle, engaged with a rival in range, or ready to
     * fire. "busy": its own group has two or more records in play. */
    s16  quietRivalEngageRoll;
    s16  quietRivalReadyRoll;
    s16  quietFreeReleaseRoll;
    s16  quietRivalReleaseRoll;
    s16  busyFreeReadyRoll;
    s16  busyFreeReleaseRoll;
    s16  busyRivalEngageRoll;
    s16  busyRivalIdleRoll;
    s16  busyRivalReleaseRoll;
    s16  quietFreeReadyRoll;
    s16  idleTargetChance;
    s16  playerAimChance;
    s16  playerAimScoreDrop;
    s16  idleAimChance;
    s16  fullPlayerAimChance;
    s16  fullPlayerAimScoreDrop;
    u8  pad682[0x684 - 0x682];
    AnimState sprites[16];  /* [0] the marker, [1] the pause frame, [2] the mark over a record that has gone idle */
    s16 effectVoice;
    s16 messageVoice;
    s16 subStep;
} Mg3State;

void mg3_step_dispatch(void);

#endif /* MG3_H */
