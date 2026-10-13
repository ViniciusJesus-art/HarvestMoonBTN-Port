#ifndef HARVEST_SPRITES_H
#define HARVEST_SPRITES_H

/* The harvest sprites: the seven helpers and their work. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "actor.h"
#include "anim.h"
#include "items.h"

/* A (kind, count) pair: a helper's starting work and the table rows of the
 * farm item list. */
typedef struct FarmInitPair {
    s16 kind;
    s16 count;
} FarmInitPair;

/* One of the seven harvest sprites. */
enum HelperJob {
    HELPER_HARVEST = 0x1,
    HELPER_WATER = 0x2,
    HELPER_ANIMALS = 0x4,
    HELPER_JOBS = HELPER_HARVEST | HELPER_WATER | HELPER_ANIMALS
};

#define HELPER_COUNT 7

/* Helper skills and affection saturate here. */
#define HELPER_SKILL_MAX 255
#define HELPER_AFFECTION_MAX 255
#define HELPER_START_AFFECTION 50

/* Ticks between a helper's seasonal tasks, and the wait of a fresh timer. */
#define HELPER_TASK_INTERVAL 300

/* A helper works in the hours from HELPER_WORK_START_HOUR for HELPER_WORK_HOURS. */
#define HELPER_WORK_START_HOUR 6
#define HELPER_WORK_HOURS 15

/* The work spot of a helper walking to its job: the x of the spot, and the z row of
 * helper 0 with its spacing per helper number. */
#define HELPER_SPOT_X (-3300)
#define HELPER_SPOT_Z_BASE 5800
#define HELPER_SPOT_Z_STEP 200

/* FarmHelper.flags beyond the HELPER_JOBS bits. */
enum HelperFlag {
    HELPER_BIT3 = 0x8,              /* toggled by each talk with the player */
    HELPER_MARKED = 0x10,
    HELPER_BIT5 = 0x20,             /* a helper with it set is skipped by the occupant test */
    HELPER_BIT6 = 0x40,             /* set when a job or a task is started */
    HELPER_BIT8 = 0x100,
    HELPER_BIT9 = 0x200,
    HELPER_REWARDED = 0x400,        /* the one-time affection reward is paid */
    HELPER_BIT11 = 0x800,           /* an event waits for the text window to close */
};

/* FarmActor.taskNo of a helper: the index of its handler. The job handlers 8..10 are
 * also the work the helper is set to (the skill it draws on). */
enum HelperHandler {
    HELPER_HANDLER_IDLE_CUE = 0,
    HELPER_HANDLER_CUE_1 = 1,
    HELPER_HANDLER_CUE_2 = 2,
    HELPER_HANDLER_CUE_3 = 3,
    HELPER_HANDLER_CUE_4 = 4,
    HELPER_HANDLER_SCOLD_CUE = 5,
    HELPER_HANDLER_SCRIPT = 6,
    HELPER_HANDLER_SET_WORK = 7,
    HELPER_HANDLER_HARVEST = 8,
    HELPER_HANDLER_WATER = 9,
    HELPER_HANDLER_ANIMALS = 10,
    HELPER_HANDLER_PLACE = 11,
    HELPER_HANDLER_WALK = 12,
};

typedef struct FarmHelper {
    FarmActor actor;
    u16 number;         /* 0..6 */
    u16 harvestSkill;   /* 0..0xFF, a point a day of HELPER_HARVEST */
    u16 waterSkill;     /* 0..0xFF, a point a day of HELPER_WATER */
    u16 animalSkill;    /* 0..0xFF, a point a day of HELPER_ANIMALS */
    u16 jobDays;        /* the days of work asked for, 2/4/8 */
    ItemId item;
    u16 flags;          /* the HELPER_JOBS bits */
    s16 targetAnimal;
    s16 idleCount;
    s16 savedTaskNo;    /* actor.taskNo/taskId/taskTime latched while a move */
    u16 savedTaskId;    /* script runs are restored from these; savedTaskNo 0 = none */
    u16 savedTaskTime;
    FarmInitPair pairC4;
    AnimState itemSprite;   /* the icon of the item it carries */
} FarmHelper;

typedef struct HarvestSpot {
    s32  x;
    s32  y;
    s32  z;
    s32  facing;
} HarvestSpot;

void harvest_helpers_init(void);
void harvest_helpers_update(void);
void harvest_helpers_work_all(void);
void harvest_helpers_tick_all(void);
s32 harvest_helper_set_action(FarmHelper *helper, s32 action);
s32 harvest_helper_idle_cue(FarmActor *self);
s32 harvest_helper_cue_1(FarmActor *self);
s32 harvest_helper_cue_2(FarmActor *self);
s32 harvest_helper_cue_3(FarmActor *self);
s32 harvest_helper_cue_4(FarmActor *self);
s32 harvest_helper_scold_cue(FarmActor *self);
s32 harvest_find_free_tile(s16 *column_out, s16 *row_out, s32 field, s32 direction,
                         s32 alternate);


#endif /* HARVEST_SPRITES_H */
