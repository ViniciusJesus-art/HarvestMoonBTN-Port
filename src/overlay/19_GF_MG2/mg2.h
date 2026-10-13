#ifndef MG2_H
#define MG2_H

/* Overlay 19_GF_MG2 (minigame 2): the types of its objects. */

#include "types.h"
#include "anim.h"

/* A four-halfword vector. */
typedef struct Mg2Vector {
    s16 x;
    s16 y;
    s16 z;
    s16 w;
} Mg2Vector;

/* One of the two players. */
typedef struct Mg2Player {
    s16 index;              /* 0 or 1 */
    s16 state;              /* MG2_STATE_* (mg2.c): 4 is attacking */
    s16 dir;                /* facing, 0..3 */
    s16 timer;
    s16 skill;              /* chance to turn, out of 255; player 0's is the farm's round */
    s16 clutX;
    s32 winning;
    Mg2Vector position;
    AnimState sprite;
} Mg2Player;

/* One of the three objects. */
typedef struct Mg2Object {
    s16 id;                 /* object 1 restarts its sprite */
    s16 state;              /* picks the animation bank */
    s16 timer;
    s16 clutRow;            /* the sprite's CLUT row */
    Mg2Vector position;
    AnimState sprite;
} Mg2Object;

typedef struct Mg2State {
    s32 paused;
    s16 index;              /* an Mg2HandlerIndex (mg2.c) */
    s16 sub;                /* an Mg2SubStep (mg2.c); MG2_SUB_DONE ends the run */
    s16 lookupIndex;        /* indexes the menu id table */
    s16  frameCount;
    s16 fallenPlayer;       /* -1 when nobody has fallen */
    s16 subStep;
    Mg2Player players[2];
    Mg2Object objects[3];
    s16 picks[8];           /* eight distinct picks 0..19; laid out as 2 rows of 4 */
    s16 speedScale;         /* every speed is scaled by it */
    s16 attackAngle;        /* the sight-cone half-angle, in degrees */
    s16 attackRange;        /* rejects a pair whose separation exceeds it */
    s16  attackInterval;
    s16 moveDelay;
    s16  rollInterval;
    s16  busyLow;
    s16  busyHigh;
    s16  sideLow;
    s16  sideHigh;
    s16  frontLow;
    s16  frontHigh;
    s16  arcLow;
    s16  arcHigh;
    s16  missDuration;
    s16  hitDuration;
    s16  attackDuration;
    s16  retreatDuration;
    s16 thinkInterval;
    s16  missSpeed;
    s16  attackSpeed;
    s16  hitSpeedScale;
    s16  idleHitSpeedScale;
    s16  retreatSpeedScale;
    AnimState iconSprite;
    AnimState shadow;       /* the shadow sprite drawn under each player */
    s16 waitTimer;          /* counts frames up to the message's wait (MG2_INTRO_FRAMES, MG2_WAIT_FRAMES) */
    s16 soundVoice;
    s16 hitVoice;
    u8  pad206[0x210 - 0x206];
    AnimState scenerySprite;
} Mg2State;

void mg2_move_step(Mg2Player *obj);
void mg2_player_set_action(Mg2Player *player, s16 state, s16 dir, s16 timer);

void mg2_phase_run(void);

#endif /* MG2_H */
