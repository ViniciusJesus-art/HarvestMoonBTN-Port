#ifndef MAIN_H
#define MAIN_H

/* The EXE entry point: library start-up, the main loop and the scene requests. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "pad.h"
#include "prim.h"
#include "sound.h"

/* Entries in each frame's main ordering table. */
#define FRAME_OT_DEPTH 2048
#define FRAME_PRIM_SIZE 0x17700
/* Offsets within the region at region_base of a frame's main OT and its primitive buffer. */
#define REGION_OT 0x290
#define REGION_PRIM 0x4290

typedef struct FrameState {
    /* The draw and display environments. */
    DRAWENV drawEnv;
    DISPENV dispEnv;
    u8  pad70[0x84 - 0x70];
    GsOT ots[4];        /* headers over the OT heads below; the fourth is unused */
    u8  padD4[0xD8 - 0xD4];
    /* The tag arrays of the four layer OTs (64, 16 and 2 entries; the fourth is
     * unused), drawn around the main OT: 3, 2, `ot`, 1, 0, so layer 0 is the
     * front. */
    u_long *otTags[4];
    u8  padE8[0xEC - 0xE8];
    s32 otTagCounts[4]; /* entries in each */
    u8  padFC[0x100 - 0xFC];
    s32 otLengths[4];   /* log2 of otTagCounts (GsOT.length) */
    s32 otDepth;        /* entries in `ot`; a depth is clamped to otDepth - 1 */
    s32 otDepthBits;    /* log2(otDepth) */
    PrimCursor primCursor;  /* the next free byte of the primitive buffer; rewound to
                             * primStart each frame */
    s32 primSize;       /* bytes in the primitive buffer */
    u_long *ot;         /* the main OT, otDepth entries indexed by depth */
    u8 *primStart;      /* this frame's primitive buffer */
    u32 workSize;       /* bytes of the GS work area */
    u8 *workBase;       /* the GS work area (gs_work_alloc allocates from it) */
    u8 *primBuffer;     /* == primStart */
} FrameState;

/* The top-level scenes. A scene is requested by number, from the debug menu's rows too.
 * SCENE_PDA is reached only from the farm's PDA. */
enum MainScene {
    SCENE_DEBUG = 0,              /* the debug menu (08_DEBUG) */
    SCENE_PDA_SAMPLE = 1,         /* the PDA sample (10_PdaSamp) */
    SCENE_FARM = 2,               /* the farm, from a new game or a wind change */
    SCENE_MG1 = 3,                /* minigame 1: the horse race */
    SCENE_MG2 = 4,                /* minigame 2: the cockfight */
    SCENE_MG3 = 5,                /* minigame 3: the tomato */
    SCENE_FARM_MINIGAME_EXIT = 6, /* the farm, back from a minigame */
    SCENE_SAVE = 7,               /* the save screen (15_saveload) */
    SCENE_LOAD = 8,               /* the load screen (15_saveload) */
    SCENE_PDA = 9,                /* the farm's PDA */
    SCENE_LIVESTOCK_TRADE = 10,   /* the livestock trade (the save transfer screen) */
    SCENE_MG4 = 11,               /* minigame 4: the dog */
    SCENE_MG5 = 12,               /* minigame 5: the swimming race */
    SCENE_TITLE = 13,             /* the title screen (27_GF_TITLE) */
    SCENE_FARM_LOADED = 14,       /* the farm, after a load */
    SCENE_FARM_SAVED = 15,        /* the farm, after a save */
    SCENE_STAFF_ROLL = 16,        /* the staff roll (28_GF_STAFF) */
    SCENE_MG_DEBUG = 17,          /* the minigame debug screen (30_GF_swind) */
    SCENE_MEMCARD = 18,           /* the memory card screen (31_gf_mcard) */
    SCENE_FARM_MENU_RETURN = 19,  /* the farm, back from the livestock trade or a cancelled load */
    SCENE_FARM_STAFF_EXIT = 20,   /* the farm, after the staff roll */
};

void main_request_scene(s32 scene);
s32 main_get_previous_scene(void);
/* The speed mode: a fade runs at half step in HALF, and the cd fade steps and the pad's
 * repeat delays scale by 2 or 3 in FAST_2 and FAST_3. */
enum MainSpeedMode {
    MAIN_SPEED_HALF = 0,
    MAIN_SPEED_NORMAL = 1,
    MAIN_SPEED_FAST_2 = 2,
    MAIN_SPEED_FAST_3 = 3,
};

u8 main_get_speed_mode(void);
void main_set_put_env(s32 value);
void main_set_clear_screen(s32 value);
void *gs_work_alloc(s32 size);
void gs_work_release(void *end);
s32 main_region_free_bytes(void);
u8 *main_region_free_start(void);
s32 main_get_region_base(void);
void main_frame_clear(void);

/* Double-buffer parity: 0 or 1. */
extern s32 frame_index;
extern InputRecord *input_pad1;
extern InputRecord *input_pad2;
/* Base of the big scratch/OT region. */
extern u8 *region_base;
/* The active one of the two OTs the frame is drawn through. */
extern u_long *active_ot;
/* The OT base for the current frame. */
extern u8 *frame_ot_base;
/* Struct definitions for the game's global data objects. Parts without a name are
 * padding arrays. */
extern FrameState *current_frame;
/* The two frame records. */
extern FrameState frame_records[];
extern VabStream vab_stream_copy;
/* The two controller input records. */
extern InputRecord input_records[2];

/* Deferred one-shot callback, called once at vsync and then cleared. */
extern void (*vsync_pending_callback)(void);

#endif /* MAIN_H */
