#ifndef ANIMAL_ACTIONS_H
#define ANIMAL_ACTIONS_H

/* The farm actions: claiming a slot and running the cues of the wandering animals. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "actor.h"

/* One of the six farm actions. */
typedef struct FarmAction {
    FarmActor actor;
    s16 countdown;      /* 0xFF when claimed, then counts down to 0 */
} FarmAction;

/* The task of a farm action: the index of its tick handler. */
enum FarmTask {
    FARM_TASK_REST = 0,
    FARM_TASK_WALK = 1,
    FARM_TASK_WANDER = 2,
    FARM_TASK_VISITOR_WALK = 3,
    FARM_TASK_VISITOR_SIT = 4,
    FARM_TASK_MESSAGE = 5,
    FARM_TASK_COUNTDOWN = 6,
    FARM_TASK_DOG_HERD = 7,
    FARM_TASK_RABBIT_A = 8,
    FARM_TASK_RABBIT_B = 9,
    FARM_TASK_SQUIRREL = 10,
    FARM_TASK_FOX_A = 11,
    FARM_TASK_FOX_B = 12,
    FARM_TASK_MONKEY = 13,
    FARM_TASK_PLACED_CICADA = 14,
    FARM_TASK_PLACED_BEETLE = 15,
    FARM_TASK_PLACED_CRICKET = 16,
    FARM_TASK_CAT = 17,
    FARM_TASK_DOG_WALK = 18,
    FARM_TASK_DOG_LEAVE = 19,
    FARM_TASK_DOG_SEEK = 20,
};

FarmAction *farm_action_claim(s32 kind);
s32 farm_action_update(FarmAction *action);
s32 farm_actor_set_task(FarmActor *action, s32 state);
void farm_action_set_countdown(s32 step, s32 start_value);
void farm_action_clear(void *arg);
s32 farm_action_pick_slot(u16 arg);
s32 farm_event_training_boost(FarmAction *event);
s32 farm_table_lookup_pair(u16 group, u16 index, u16 *first, u16 *second);
s32 insect_cue_0(FarmActor *self);
s32 rabbit_cue_8(FarmActor *self);
s32 rabbit_cue_9(FarmActor *self);
s32 fox_cue_11(FarmActor *self);
s32 fox_cue_12(FarmActor *self);
s32 squirrel_cue_10(FarmActor *self);
s32 monkey_cue_13(FarmActor *self);
s32 cat_cue_17(FarmActor *self);


#endif /* ANIMAL_ACTIONS_H */
