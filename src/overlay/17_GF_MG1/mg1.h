#ifndef MG1_H
#define MG1_H

/* Overlay 17_GF_MG1 (minigame 1): its state and record types. */

#include "types.h"
#include "anim.h"

/* Entry index of the player's horse. */
#define MG1_PLAYER_ENTRY 2
/* Number of runners in a race. */
#define MG1_ENTRY_COUNT 6
/* Length of the Mg1State.records ring. */
#define MG1_RECORD_COUNT 90

typedef struct Mg1Slot {
    s16 code;               /* the sprite code of the runner's marker */
    s16 kind;               /* the animation set the runner draws with */
    s16 order;              /* the OT depth the runner draws at */
} Mg1Slot;

/* One of the six runners. MG1_PLAYER_ENTRY is the player's horse. */
typedef struct Mg1Entry {
    u8  flags;              /* bit 0: the player's horse */
    u8  pad01;
    s16 stamina;            /* 0..0xFF; a dash costs Mg1State.dashCost */
    s16 baseSpeed;          /* the floor `speed` is clamped to */
    s16 speed;              /* decays toward baseSpeed */
    s32 score;              /* 1 = sit this round out, else the round's score */
    s16 training;           /* the training scale: 0..0xFF, folded into the base speed */
    s16 affection;          /* the runner's affection; at 81 or more it counts as fond */
    s32 position;          /* 20.12 fixed point along the course */
    Mg1Slot *slot;
    u16 nameOffset;         /* an offset into horse_names */
    s16 chance;             /* 0..127: the weight of the step roll */
    s16 rivalChance;        /* 0..127: the weight of the roll when a rival is close */
    u8  pad1E[0x20 - 0x1E];
    AnimState sprite;
} Mg1Entry;

/* 8-byte record in the table at Mg1State.records. */
typedef struct Mg1Record {
    s32 position;
    s16 code;
    u16 pad6;
} Mg1Record;

/* One entry of the course's marker table, ended by kind 0xFF. */
typedef struct Mg1Marker {
    s16 kind;
    u16 pad2;
    s32 position;           /* 20.12 fixed point */
} Mg1Marker;

typedef struct Mg1State {
    s32 paused;             /* 1 while paused */
    u8  step;               /* an index into the screen handlers */
    u8  pad05;
    s16 updateInterval;     /* ticks between the periodic updates */
    s16 trainingBase;
    s16 trainingAddend;
    s16 trainingMultiplier;
    s16 dashBoost;          /* a dash's speed gain */
    s16 dashDivisor;
    s16 dashCost;           /* a dash's stamina cost */
    s16 decayDivisor;
    s16 staminaBound1;
    s16 staminaBound2;
    u8  pad1A[0x1C - 0x1A];
    u32 frameCount;         /* race frames, counted from the start of the race */
    s16 courseSpeed;        /* the course's scroll speed, 20.12 per frame */
    s16 courseSpeedFloor;
    s32 coursePosition;     /* 20.12 fixed point along the course */
    Mg1Entry   entries[6];
    s16        chances[3][6];
    AnimState courseSprite;
    AnimState stageSprite;
    AnimState staminaAnimation;
    s16 order[6];           /* queue of entry indices, `orderCount` long */
    s16 soundVoice;
    s16  finishVoice;
    s16 finishTimer;
    u16 startTimer;
    s16  soundPercent;      /* percent of the course at which the finish sound starts */
    s16  leaderThreshold;
    s16 orderCount;
    u8  pad2D6[0x2D8 - 0x2D6];
    Mg1Record  records[MG1_RECORD_COUNT];
    s16        recordCount;         /* wraps at MG1_RECORD_COUNT */
} Mg1State;

void mg1_run_state(void);

#endif /* MG1_H */
