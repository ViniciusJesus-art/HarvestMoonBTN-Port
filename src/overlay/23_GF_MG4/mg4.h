#ifndef MG4_H
#define MG4_H

/* Overlay 23_GF_MG4 (minigame 4): the types of its objects. */

#include "types.h"
#include "anim.h"

/* The world's position unit: positions and track positions are in 1/4096ths. */
#define MG4_UNIT 4096

/* A record's or player's mode: active, or past the finish and walking to its stop. */
enum Mg4Mode {
    MG4_MODE_ACTIVE = 1,
    MG4_MODE_FINISHED = 3,
};

/* One entry of either trail ring buffer. */
typedef struct Mg4Entry {
    s32 position;
    s16 x;
    s16 y;
} Mg4Entry;

/* A record's state (Mg4Record.state): it stands, walks or runs. The player's
 * record runs while PAD_CIRCLE is held. */
enum Mg4RecordState {
    MG4_RECORD_STAND = 1,
    MG4_RECORD_WALK = 2,
    MG4_RECORD_RUN = 3,
};

/* One of the six records. */
typedef struct Mg4Record {
    s16 slot;               /* its own index */
    s16 mode;               /* a Mg4Mode */
    s16 state;              /* a Mg4RecordState; 2 and 3 move it */
    s16 direction;          /* MG4_DIR_PLUS_X (2) moves it on, MG4_DIR_MINUS_X (6) back */
    s16 timer;              /* counted down, reloaded from reloadTime */
    s16 reloadTime;         /* one of mg4_reload_times */
    s32 trackX;             /* in 1/4096ths */
    AnimState sprite;
    s16 clutOffY;           /* the CLUT y offset for its slot */
    u8  pad42[0x44 - 0x42];
} Mg4Record;

/* A player's action (Mg4Player.kind). Kinds 4..10 are drawn as poses that do not move. */
enum Mg4PlayerKind {
    MG4_PLAYER_KIND_WALK = 1,       /* moves at its own speed */
    MG4_PLAYER_KIND_STAND = 2,      /* its start kind; the think sets it when it is near its record */
    MG4_PLAYER_KIND_RUN = 3,        /* moves at the faster playerSpeed */
};

/* One of the six players. */
typedef struct Mg4Player {
    s16 index;              /* its own index */
    s16 mode;               /* a Mg4Mode */
    s16 kind;               /* a Mg4PlayerKind */
    s16 direction;
    s16 timer;              /* delay, counted down to 0 */
    s16 reach;              /* the reach for y, from a table indexed by y / 50 */
    s16 x;
    s16 y;
    s32 speed;
    s32 trackX;             /* 20.12 */
    s32 depth;              /* 20.12 */
    AnimState sprite;
    s16 clutOffX;           /* the CLUT x offset */
    s16 clutOffY;           /* the CLUT y offset */
} Mg4Player;

typedef struct Mg4State {
    s16 state;              /* step id of the state machine */
    u8  pad002[0x004 - 0x002];
    s32 paused;
    s32 resultLatched;
    s32 finishing;
    s32 cameraX;            /* the scroll position, 20.12 */
    u16 frameCount;
    u16 messageIds[6];      /* one message id per player, 0..9 (mg4_shuffle_messages) */
    u8  pad22[0x24 - 0x22];
    Mg4Player players[6];
    Mg4Record records[6];
    /* Camera and viewport block, set up before the frame loop starts. */
    s16 finishX;
    s16 spacing;
    s16 recordWalkStep;     /* a record's step while it walks, in tenths of a pixel */
    s16 recordRunStep;      /* the same while it runs */
    /* The difference bands for a roll: above bandEdge0 is band 0, ..., at or below bandEdge3 is band 4. */
    s16 bandEdge0;
    s16 bandEdge1;
    s16 bandEdge2;
    s16 bandEdge3;
    s16 thinkInterval;
    s16 actionInterval;
    s16 walkScale;          /* scales each player's starting walk speed */
    s16 playerSpeed;
    AnimState backdropSprite;/* the scrolling backdrop and the finish line */
    AnimState stageSprite;  /* the staged scene sprite */
    s16 voice;              /* sound_play handle, or SOUND_VOICE_NONE */
    s16 resultFrames;       /* counts the result screen's frames to 60; 0 on its first frame */
    /* Two ring buffers, of the records' and of the players' positions, each followed by
     * its own wrapping write index. */
    Mg4Entry recordTrail[54];
    s16      recordTrailIndex;
    u8       pad5CA[0x5CC - 0x5CA];
    Mg4Entry playerTrail[12];
    s16      playerTrailIndex;
} Mg4State;

void mg4_run_step(void);

#endif /* MG4_H */
