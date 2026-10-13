/* The farm's action slots: claiming one for an animal or pet, and their per-frame update and cues. */
#include <memory.h>
#include <libapi.h>
#include <sys/types.h>
#include <libgte.h>
#include "types.h"
#include "actor.h"
#include "anim.h"
#include "animal_actions.h"
#include "animals.h"
#include "chicken.h"
#include "clock.h"
#include "cue.h"
#include "estate.h"
#include "event_flags.h"
#include "farm_data.h"
#include "farm_main.h"
#include "farm_probe.h"
#include "farm_state.h"
#include "items.h"
#include "livestock.h"
#include "menu_window.h"
#include "scene.h"
#include "screen_fade.h"
#include "sound_request.h"
#include "villager.h"

/* Whether the record's position is blocked. */
#define RECORD_BLOCKED(r) (farm_callback_capped(&(r)->position, (r)->place, 0, 0, 0, 0, 0, 1) != 0)

/* The start of a FarmActor, as the record scans read it. */
typedef struct FarmRecordHead {
    u16 active;         /* actor.kind: 0 when free */
    u8 pad02[0x30 - 0x02];
    s16 location;       /* actor.place.location */
    u8 pad32[0x34 - 0x32];
    s32 flags;
} FarmRecordHead;

/* The header both item tables share. */
typedef struct {
    u16 active;         /* actor.kind: 0 when free */
    u8 pad02[0x30 - 0x02];
    s16 location;       /* actor.place.location: PLACE_FARM for the farm's animals */
    u8 pad32[0x34 - 0x32];
    u32 flags; /* ACTOR_FLAG_HELD: not pickable; ACTOR_FLAG_BIT16: class 0 */
    s32 x;
    s32 y;
    s32 z;
} ItemHeader;

static int farm_item_init(FarmAction *obj, s16 arg);
static s32 farm_actor_set_kind(FarmActor *actor, s16 kind);
static s32 farm_action_kind_event(FarmAction *action);
static void farm_actor_load_sprite(FarmActor *action);
static s32 farm_task_pick_10b(FarmActor *actor);
static s32 farm_task_pick_10e(FarmActor *obj);
static s32 farm_task_pick_10d(FarmActor *actor);
static s32 farm_task_pick_10f(FarmActor *actor);
static s32 farm_task_pick_15b(FarmAction *arg);
static s32 farm_pick_random_slot(u16 mode);
static s32 farm_action_count_down(FarmAction *obj);
static s32 farm_action_none(void);
static s32 farm_message_for_kind(FarmActor *entry);
static s32 farm_action_walk_cue(FarmActor *object);
static s32 farm_action_wander(FarmActor *self);
static s32 visitor_cue_3(FarmActor *self);
static s32 visitor_cue_4(FarmActor *self);
static s32 insect_cue_14(FarmActor *self);
static s32 insect_cue_15(FarmActor *self);
static s32 insect_cue_16(FarmActor *self);
static s32 farm_action_dog_walk(FarmActor *object);
static s32 dog_leave_cue(FarmActor *object);
static s32 farm_action_seek_item(FarmActor *object);
static s32 dog_herd_cue(FarmActor *self);
static s32 cue_run_tick(FarmActor *self);

/* Claim the first free action slot for `kind`, stamp it with the current date and the
 * farm area's place, and start the two kinds that need an initial handler state.
 * Returns the slot, or 0 if all six are busy. */
FarmAction *farm_action_claim(s32 kind) {
    FarmAction *action;
    s32 i;
    FarmState *farm;

    for (i = 0; i < 6; i++) {
        action = &game_state.animals.actions[i];
        if (action->actor.kind == ITEM_NOTHING) {
            break;
        }
    }
    if (i == 6) {
        return 0;
    }
    farm_item_init(action, kind);
    farm = &game_state;
    memcpy(&action->actor.date, &farm->progress.clock.date.year, sizeof(action->actor.date));
    action->actor.place.location = farm_area.mode.place.location;
    action->countdown = 0xFF;
    if (action->actor.kind == ITEM_157) {
        farm_actor_set_task(&action->actor, FARM_TASK_VISITOR_SIT);
    }
    if (action->actor.kind == ITEM_STRAY_DOG) {
        farm_actor_set_task(&action->actor, FARM_TASK_DOG_WALK);
    }
    return action;
}

u16 farm_action_countdown_step;
u16 farm_action_start_value;

static void *const farm_action_start_handlers[21] = {
    0, 0, 0, 0, 0, farm_message_for_kind, farm_action_count_down, farm_action_none, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0,
};

/* The per-frame handler for each task state, run by farm_action_update. */
static s32 (*const farm_action_tick_handlers[21])(FarmActor *) = {
    insect_cue_0, farm_action_walk_cue, farm_action_wander, visitor_cue_3, visitor_cue_4, 0,
    cue_run_tick, dog_herd_cue, rabbit_cue_8, rabbit_cue_9, squirrel_cue_10,
    fox_cue_11, fox_cue_12, monkey_cue_13, insect_cue_14, insect_cue_15,
    insect_cue_16, cat_cue_17, farm_action_dog_walk, dog_leave_cue, farm_action_seek_item,
};

/* Advance an action slot by one frame: reload its sprite, run its task's tick handler, and
 * once no cue is running and its interruptTime reaches 300, pick its next task. */
s32 farm_action_update(FarmAction *action) {
    s32 (*const *handler_table)(FarmActor *);
    s32 (*handler)(FarmActor *);

    if (action->actor.kind == ITEM_NOTHING) {
        return -1;
    }
    farm_actor_load_sprite(&action->actor);
    if (action->actor.kind == ITEM_NOTHING) {
        return -1;
    }
    handler_table = farm_action_tick_handlers;
    handler = handler_table[(u16)action->actor.taskNo];
    if (handler != 0) {
        handler(&action->actor);
    }
    if ((action->actor.flags & ACTOR_FLAG_CUE_RUNNING) == 0 && action->actor.interruptTime >= 300) {
        farm_action_kind_event(action);
        action->actor.interruptTime = 0;
    }
    action->actor.interruptTime++;
    return 0;
}

/* Switch the actor to task `state`. A task with a start handler runs it; a repeat of the running
 * task that is still cueing is refused (-1), except FARM_TASK_COUNTDOWN. A task with a tick handler
 * then resets the task fields. Returns the start handler's result. */
s32 farm_actor_set_task(FarmActor *action, s32 state) {
    s32 result;

    result = 0;
    if (farm_action_start_handlers[state] != 0) {
        if (action->taskNo == state && (action->flags & ACTOR_FLAG_CUE_RUNNING) && state != 6) {
            result = -1;
        } else {
            result = ((s32(*)(FarmActor *))farm_action_start_handlers[state])(action);
        }
    }
    if (farm_action_tick_handlers[state] != 0 && result != -1) {
        action->taskNo = state;
        action->taskId = 0;
        action->taskTime = 0;
        action->interruptTime = 0;
        farm_vector_adjust(4, &action->speed, 0);
    }
    return result;
}

/* Set the step that farm_action_count_down takes off and the start value that cue_run_tick faces. */
void farm_action_set_countdown(s32 step, s32 start_value) {
    farm_action_countdown_step = step;
    farm_action_start_value = start_value;
}

/* Zero an actor record, which frees its slot. */
void farm_action_clear(void *arg) {
    memset(arg, 0, 0xB0);
    *(s16 *)arg = 0;
}

/* Pick a random slot and discard it. */
s32 farm_action_pick_slot(u16 arg) {
    farm_pick_random_slot(arg);
}

/* Event 0x15B: when clock_is_pending reports 1, estateFlags has ESTATE_FLAG_DOG_WALK and
 * the hour is at least 7, pick a random record (the livestock table first, the
 * chickens if it has none) and shift its affection by the dog's training / 50 - 10.
 * Slot 0 is never adjusted. Any other event returns -1. */
s32 farm_event_training_boost(FarmAction *event) {
    s32 table;
    s32 slot;
    s16 *affection;

    if (event->actor.kind != ITEM_STRAY_DOG) {
        return -1;
    }
    if (clock_is_pending() == 1 && (game_state.progress.estate.estateFlags & ESTATE_FLAG_DOG_WALK) && game_state.progress.clock.time.hm.hour >= 7) {
        table = 0;
        slot = farm_pick_random_slot(0);
        if (slot < 0) {
            table = 1;
            slot = farm_pick_random_slot(1);
        }
        if (table == 0) {
            if (slot > 0) {
                affection = &game_state.animals.livestock[slot].actor.affection;
                *affection += game_state.animals.dog.training / 50 - 10;
            }
        } else {
            if (slot > 0) {
                affection = &game_state.animals.chickens[slot].actor.affection;
                *affection += game_state.animals.dog.training / 50 - 10;
            }
        }
    }
    return 0;
}

/* Zero a free action slot, set its kind and reset its cue. Returns -1 if the slot is in use. */
static int farm_item_init(FarmAction *obj, s16 arg) {
    if (obj->actor.kind != ITEM_NOTHING) return -1;
    memset(obj, 0, sizeof(FarmAction));
    obj->actor.kind = arg;
    obj->actor.name[0] = 0xFFFF;
    farm_actor_set_kind(&obj->actor, arg);
    cue_reset(&obj->actor.cue);
    return 0;
}

/* Set the kind and load its sprite. */
static s32 farm_actor_set_kind(FarmActor *actor, s16 kind) {
    actor->kind = kind;
    farm_actor_load_sprite(actor);
    return 0;
}

typedef struct FarmTablePair {
    u16 first;
    u16 second;
} FarmTablePair;

static FarmTablePair farm_table_pairs[18][4] = {
    {{0x0000, 0x0000}, {0x0000, 0x0001}, {0x0000, 0x0002}, {0x0000, 0x0003}},
    {{0x0001, 0x0000}, {0x0002, 0x0000}, {0x0003, 0x0000}, {0x0004, 0x0000}},
    {{0x0003, 0x0000}, {0x0004, 0x0000}, {0x0005, 0x0000}, {0x0006, 0x0000}},
    {{0x0007, 0x0000}, {0x0008, 0x0000}, {0x0009, 0x0000}, {0x000A, 0x0000}},
    {{0x0000, 0x0000}, {0x0001, 0x0000}, {0x0000, 0x0000}, {0x0001, 0x0000}},
    {{0x0002, 0x0000}, {0x0003, 0x0000}, {0x0002, 0x0000}, {0x0003, 0x0000}},
    {{0x0004, 0x0000}, {0x0005, 0x0000}, {0x0004, 0x0000}, {0x0005, 0x0000}},
    {{0x0005, 0x0000}, {0x0006, 0x0000}, {0x0007, 0x0000}, {0x0008, 0x0000}},
    {{0x0009, 0x0000}, {0x0006, 0x0000}, {0x0007, 0x0000}, {0x000A, 0x0000}},
    {{0x0005, 0x0000}, {0x0006, 0x0000}, {0x0007, 0x0000}, {0x0008, 0x0000}},
    {{0x0009, 0x0000}, {0x000A, 0x0000}, {0x000B, 0x0000}, {0x000C, 0x0000}},
    {{0x0005, 0x0000}, {0x0006, 0x0000}, {0x0007, 0x0000}, {0x0008, 0x0000}},
    {{0x0005, 0x0000}, {0x0006, 0x0000}, {0x0007, 0x0000}, {0x0008, 0x0000}},
    {{0x0001, 0x0000}, {0x0001, 0x0000}, {0x0002, 0x0000}, {0x0002, 0x0000}},
    {{0x0003, 0x0000}, {0x0003, 0x0000}, {0x0004, 0x0000}, {0x0004, 0x0000}},
    {{0x0005, 0x0000}, {0x0006, 0x0000}, {0x0007, 0x0000}, {0x0008, 0x0000}},
    {{0x0005, 0x0000}, {0x0006, 0x0000}, {0x0007, 0x0000}, {0x0008, 0x0000}},
    {{0x0009, 0x0000}, {0x000A, 0x0000}, {0x000B, 0x0000}, {0x000C, 0x0000}},
};

/* Read the pair at [group][index] of farm_table_pairs into `first`, and into `second` when it is not null. */
s32 farm_table_lookup_pair(u16 group, u16 index, u16 *first, u16 *second) {
    *first = farm_table_pairs[group][index].first;
    if (second != 0) {
        *second = farm_table_pairs[group][index].second;
    }
    return 0;
}

/* Choose a finished action's next task from its kind: the mammals and birds roll among their
 * tasks, the insects rest, and the snake, cows and sheep walk. An unlisted kind is a SystemError. */
static s32 farm_action_kind_event(FarmAction *action) {
    s32 result;

    result = 0;
    switch (action->actor.kind) {
    case ITEM_VISITOR:
        result = FARM_TASK_WANDER;
        break;
    case ITEM_156:
        result = FARM_TASK_VISITOR_WALK;
        break;
    case ITEM_157:
        result = FARM_TASK_VISITOR_SIT;
        break;
    case ITEM_STRAY_DOG:
        result = farm_task_pick_15b(action);
        break;
    case ITEM_CAT:
        if (rand_range(2) != 0) {
            result = FARM_TASK_CAT;
        } else {
            result = FARM_TASK_WALK;
        }
        break;
    case ITEM_WHITE_RABBIT:
    case ITEM_BROWN_RABBIT:
        result = farm_task_pick_10b(&action->actor);
        break;
    case ITEM_SQUIRREL:
        result = farm_task_pick_10d(&action->actor);
        break;
    case ITEM_FOX:
        result = farm_task_pick_10e(&action->actor);
        break;
    case ITEM_MONKEY:
        result = farm_task_pick_10f(&action->actor);
        break;
    case ITEM_BUTTERFLY:
    case ITEM_BUTTERFLY_2:
    case ITEM_BEETLE:
    case ITEM_STAG_BEETLE:
    case ITEM_CICADA:
    case ITEM_DRAGONFLY:
    case ITEM_CRICKET:
    case ITEM_LADY_BUG:
    case ITEM_GRASSHOPPER:
    case ITEM_153:
    case ITEM_155:
        result = FARM_TASK_REST;
        break;
    case ITEM_SNAKE:
    case ITEM_159:
    case ITEM_COW:
    case ITEM_SHEEP:
    case ITEM_17A:
    case ITEM_17B:
        result = FARM_TASK_WALK;
        break;
    default:
        SystemError(0x45, 0x64);
        break;
    }
    farm_actor_set_task(&action->actor, result);
    return 0;
}

/* Attach resource `id` to the action's sprite unless it is already attached;
 * without one (or at 6:00 exactly) the action is cancelled. */
static inline void load_resource(FarmActor *action, s32 id)
{
    AnimState found;

    if (game_state.progress.clock.time.word == 6 || villager_load_request(id, &found) == 0) {
        action->kind = ITEM_NOTHING;
    } else if (found.source != action->sprite.source) {
        anim_bind_file(&action->sprite, found.source);
    }
}

/* Point the action's sprite at the graphics for its state: a fixed entry of an offset table,
 * one picked by the farm's current state (a state with no graphics for the current value is a SystemError),
 * or, for five states, the resource villager_load_request finds. Only an unattached sprite
 * takes a table entry. */
static void farm_actor_load_sprite(FarmActor *action) {
    u8 *base = 0;
    s32 offset = 0;

    switch (action->kind) {
    case ITEM_CAT:
    case ITEM_BUTTERFLY:
    case ITEM_BUTTERFLY_2:
    case ITEM_153:
    case ITEM_155:
    case ITEM_159:
    case ITEM_COW:
    case ITEM_SHEEP:
    case ITEM_17A:
    case ITEM_17B:
        break;
    case ITEM_VISITOR:
        base = farm_area.slotSixArt;
        offset = ANIM_PACK_START(farm_slot_tocs[6], 4);
        break;
    case ITEM_156:
        base = farm_area.slotSixArt;
        offset = ANIM_PACK_START(farm_slot_tocs[6], 3);
        break;
    case ITEM_157:
        base = farm_area.slotSixArt;
        offset = ANIM_PACK_START(farm_slot_tocs[6], 2);
        break;
    case ITEM_STRAY_DOG:
        base = (u8 *)farm_area.mode.resourceBuffer;
        offset = 0;
        break;
    case ITEM_WHITE_RABBIT:
    case ITEM_BROWN_RABBIT:
        base = farm_area.seasonArt;
        switch (game_state.progress.clock.date.season) {
        case SEASON_SPRING:
            offset = ANIM_PACK_START(farm_slot_tocs[2], 2);
            break;
        case SEASON_SUMMER:
            offset = ANIM_PACK_START(farm_slot_tocs[3], 1);
            break;
        case SEASON_FALL:
            offset = ANIM_PACK_START(farm_slot_tocs[4], 2);
            break;
        case SEASON_WINTER:
            offset = ANIM_PACK_START(farm_slot_tocs[5], 2);
            break;
        }
        break;
    case ITEM_SQUIRREL:
        base = farm_area.seasonArt;
        switch (game_state.progress.clock.date.season) {
        case SEASON_SPRING:
            offset = ANIM_PACK_START(farm_slot_tocs[2], 0);
            break;
        case SEASON_SUMMER:
            offset = ANIM_PACK_START(farm_slot_tocs[3], 0);
            break;
        case SEASON_FALL:
            offset = ANIM_PACK_START(farm_slot_tocs[4], 1);
            break;
        case SEASON_WINTER:
            SystemError(0x45, 1);
            break;
        }
        break;
    case ITEM_FOX:
        base = farm_area.seasonArt;
        switch (game_state.progress.clock.date.season) {
        case SEASON_SPRING:
            SystemError(0x45, 2);
            break;
        case SEASON_SUMMER:
            SystemError(0x45, 3);
            break;
        case SEASON_FALL:
            offset = ANIM_PACK_START(farm_slot_tocs[4], 0);
            break;
        case SEASON_WINTER:
            offset = ANIM_PACK_START(farm_slot_tocs[5], 0);
            break;
        }
        break;
    case ITEM_MONKEY:
        base = farm_area.seasonArt;
        if (game_state.progress.clock.date.season == SEASON_WINTER) {
            offset = ANIM_PACK_START(farm_slot_tocs[5], 1);
        } else {
            SystemError(0x45, 4);
        }
        break;
    case ITEM_SNAKE:
        base = farm_area.seasonArt;
        if (game_state.progress.clock.date.season == SEASON_SPRING) {
            offset = ANIM_PACK_START(farm_slot_tocs[2], 1);
        } else {
            SystemError(0x45, 5);
        }
        break;
    case ITEM_BEETLE:
        base = farm_area.seasonArt;
        if (game_state.progress.clock.date.season == SEASON_SUMMER) {
            offset = ANIM_PACK_START(farm_slot_tocs[3], 2);
        } else {
            SystemError(0x45, 6);
        }
        break;
    case ITEM_STAG_BEETLE:
        base = farm_area.seasonArt;
        if (game_state.progress.clock.date.season == SEASON_SUMMER) {
            offset = ANIM_PACK_START(farm_slot_tocs[3], 3);
        } else {
            SystemError(0x45, 7);
        }
        break;
    case ITEM_CICADA:
        base = farm_area.seasonArt;
        if (game_state.progress.clock.date.season == SEASON_SUMMER) {
            offset = ANIM_PACK_START(farm_slot_tocs[3], 4);
        } else {
            SystemError(0x45, 8);
        }
        break;
    case ITEM_DRAGONFLY:
        base = farm_area.seasonArt;
        if (game_state.progress.clock.date.season == SEASON_FALL) {
            offset = ANIM_PACK_START(farm_slot_tocs[4], 4);
        } else {
            SystemError(0x45, 9);
        }
        break;
    case ITEM_CRICKET:
        base = farm_area.seasonArt;
        if (game_state.progress.clock.date.season == SEASON_FALL) {
            offset = ANIM_PACK_START(farm_slot_tocs[4], 3);
        } else {
            SystemError(0x45, 8);
        }
        break;
    case ITEM_GRASSHOPPER:
        base = farm_area.seasonArt;
        if (game_state.progress.clock.date.season == SEASON_SPRING) {
            offset = ANIM_PACK_START(farm_slot_tocs[2], 4);
        } else {
            SystemError(0x45, 0xB);
        }
        break;
    case ITEM_LADY_BUG:
        base = farm_area.seasonArt;
        if (game_state.progress.clock.date.season == SEASON_SPRING) {
            offset = ANIM_PACK_START(farm_slot_tocs[2], 3);
        } else {
            SystemError(0x45, 0xC);
        }
        break;
    default:
        SystemError(0x45, 0);
        break;
    }

    switch (action->kind) {
    case ITEM_COW:
        load_resource(action, 0x28);
        break;
    case ITEM_SHEEP:
        load_resource(action, 0x29);
        break;
    case ITEM_CAT:
        load_resource(action, 0x2B);
        break;
    case ITEM_17A:
        load_resource(action, 0x2A);
        break;
    case ITEM_17B:
        load_resource(action, 0x2C);
        break;
    default:
        if (action->sprite.source == 0) {
            anim_bind_file(&action->sprite, (AnimFile *)(base + offset));
        }
        break;
    }
}

/* The walk speed of a farm item kind. Kinds not listed here walk at 12. */
static s32 farm_item_walk_speed(ItemId *kind) {
    switch (*kind) {
    case ITEM_STRAY_DOG:
        return 24;
    case ITEM_CAT:
    case ITEM_WHITE_RABBIT:
    case ITEM_BROWN_RABBIT:
    case ITEM_SQUIRREL:
    case ITEM_FOX:
    case ITEM_MONKEY:
    case ITEM_SNAKE:
    case ITEM_BUTTERFLY:
    case ITEM_BUTTERFLY_2:
    case ITEM_BEETLE:
    case ITEM_STAG_BEETLE:
    case ITEM_CICADA:
    case ITEM_DRAGONFLY:
    case ITEM_CRICKET:
    case ITEM_LADY_BUG:
    case ITEM_GRASSHOPPER:
    case ITEM_153:
    case ITEM_155:
    case ITEM_159:
    case ITEM_17A:
    case ITEM_17B:
        return 12;
    case ITEM_COW:
    case ITEM_SHEEP:
        return 6;
    }
    return 12;
}

/* Pick a rabbit's next task: A on a 0 roll, B on a 1, else walk. The roll is out of 3, or out of 8
 * when the rabbit is approaching the player. */
static s32 farm_task_pick_10b(FarmActor *actor) {
    s32 result = rand_range((actor->flags & ACTOR_FLAG_APPROACHING_PLAYER) ? 8 : 3);
    s32 ret = FARM_TASK_RABBIT_A;
    if (result != 0) {
        ret = FARM_TASK_WALK;
        if (result == ret) ret = FARM_TASK_RABBIT_B;
    }
    return ret;
}

/* Pick a fox's next task: A on a 0 roll, B on a 1, else walk. The roll is out of 3, or out of 8
 * when the fox is approaching the player. */
static s32 farm_task_pick_10e(FarmActor *obj) {
    s32 roll = rand_range((obj->flags & ACTOR_FLAG_APPROACHING_PLAYER) ? 8 : 3);
    s32 result = FARM_TASK_FOX_A;
    if (roll != 0) {
        result = FARM_TASK_WALK;
        if (roll == 1) result = FARM_TASK_FOX_B;
    }
    return result;
}

/* Pick a squirrel's next task: a squirrel on a 0 roll, else walk. The roll is out of 2, or out of 7
 * when the squirrel is approaching the player. */
static s32 farm_task_pick_10d(FarmActor *actor) {
    s32 mode, result;
    if (actor->flags & ACTOR_FLAG_APPROACHING_PLAYER)
        mode = 7;
    else
        mode = 2;
    switch (rand_range(mode)) {
    case 0:
        result = FARM_TASK_SQUIRREL;
        break;
    default:
        result = FARM_TASK_WALK;
        break;
    }
    return result;
}

/* Pick a monkey's next task: a monkey on a 0 roll, else walk. The roll is out of 2, or out of 7
 * when the monkey is approaching the player. */
static s32 farm_task_pick_10f(FarmActor *actor) {
    return !rand_range((actor->flags & ACTOR_FLAG_APPROACHING_PLAYER) ? 7 : 2) ? FARM_TASK_MONKEY : FARM_TASK_WALK;
}

/* The stray dog's next task: it leaves before 7:00 or when its countdown has run out, else it seeks. */
static s32 farm_task_pick_15b(FarmAction *arg) {
    s32 result = FARM_TASK_DOG_SEEK;
    if (game_state.progress.clock.time.hm.hour < 7) {
        result = FARM_TASK_DOG_LEAVE;
    } else if (arg->countdown <= 0) {
        result = FARM_TASK_DOG_LEAVE;
    }
    return result;
}

/* Pick a random slot of the livestock table (mode 0) or the chickens (any other mode): a live
 * record on PLACE_FARM that is neither held nor in an event. Returns -1 when none matches. */
static s32 farm_pick_random_slot(u16 mode)
{
    u8 found[MAX_LIVESTOCK];
    s32 count;
    s32 limit;
    s32 wanted;
    s32 slot;
    s32 mask;
    s32 small_offset;
    s32 large_offset;
    FarmLivestock *large;
    FarmChicken *small;
    FarmRecordHead *record;

    memset(found, 0xFF, sizeof(found));
    count = 0;
    limit = MAX_CHICKENS;
    wanted = PLACE_FARM;
    if (mode == 0) {
        limit = MAX_LIVESTOCK;
    }
    slot = 0;
    if (limit != 0) {
        mask = ACTOR_FLAG_HELD | ACTOR_FLAG_IN_EVENT;
        large = game_state.animals.livestock;
        small = game_state.animals.chickens;
        small_offset = 0;
        large_offset = 0;
        do {
            if (mode == 0) {
                record = (FarmRecordHead *)(large_offset + (s32)large);
                if ((record->flags & mask) != 0 || record->location != wanted) {
                    goto next;
                }
            } else {
                record = (FarmRecordHead *)(small_offset + (s32)small);
                if ((record->flags & mask) != 0 || record->location != wanted) {
                    goto next;
                }
            }
            if (record->active != 0) {
                found[count++] = slot;
            }
        next:
            small_offset += sizeof(FarmChicken);
            slot++;
            large_offset += sizeof(FarmLivestock);
        } while (slot < limit);
    }
    if (count == 0) {
        return -1;
    }
    return found[rand_range(count & 0xFFFF)];
}

/* Find the first live animal in `location` whose x/z lies strictly within `range` of `pos`:
 * first among the MAX_LIVESTOCK livestock records (class 0 if ACTOR_FLAG_BIT16 is set, else 1),
 * then among the MAX_CHICKENS chickens (class 2). Held and not-pickable records are skipped. Returns
 * its index, or -1, and stores its class and address. */
static s32 farm_find_item_in_range(VECTOR pos, s16 location, s32 range, s32 *class_out, void **item_out) {
    s32 found = -1;
    s32 i;
    s32 x_min = pos.vx - range;
    s32 x_max = pos.vx + range;
    s32 z_min = pos.vz - range;
    s32 z_max = pos.vz + range;
    ItemHeader *item;

    for (i = 0; i < MAX_LIVESTOCK; i++) {
        item = (ItemHeader *)&game_state.animals.livestock[i];
        if (item->active != 0 && location == item->location && !(item->flags & ACTOR_FLAG_HELD) &&
            x_min < item->x && item->x < x_max && z_min < item->z && item->z < z_max) {
            if (item->flags & ACTOR_FLAG_BIT16) {
                found = i;
                *class_out = 0;
                *item_out = item;
            } else {
                found = i;
                *class_out = 1;
                *item_out = item;
            }
            break;
        }
    }
    if (found == -1) {
        FarmChicken *record = game_state.animals.chickens;

        for (i = 0; i < MAX_CHICKENS; i++, record++) {
            item = (ItemHeader *)record;
            if (item->active != 0 && location == item->location && !(item->flags & ACTOR_FLAG_HELD) &&
                x_min < item->x && item->x < x_max && z_min < item->z && item->z < z_max) {
                found = i;
                *class_out = 2;
                *item_out = item;
                break;
            }
        }
    }
    return found;
}

/* Start handler for FARM_TASK_COUNTDOWN: take farm_action_countdown_step off the countdown, to 0. */
static s32 farm_action_count_down(FarmAction *obj) {
    obj->countdown -= farm_action_countdown_step;
    obj->countdown = obj->countdown < 0 ? 0 : obj->countdown;
    return 0;
}

/* Start handler with nothing to set up, for FARM_TASK_DOG_HERD. */
static s32 farm_action_none(void) {
    return 0;
}

/* Open the speech for an animal of this kind, through menu_request_open. Cats and the 0x17A
 * visitor pick their message by place, and a cat's first visit to a place sets its event flag.
 * Always returns -1. */
static s32 farm_message_for_kind(FarmActor *entry) {
    u16 request;
    s16 mode;
    s32 message;
    request = entry->kind;
    message = -1;
    switch (request) {
    case ITEM_CAT:
        if (entry->place.location == PLACE_1B) {
            if (event_flag_get(EVENT_FLAG_CAT_MSG_1B) != 0) message = 0x263;
            else { event_flag_set(EVENT_FLAG_CAT_MSG_1B, 1); message = 0x26B; }
        } else if (entry->place.location == PLACE_1F) {
            if (event_flag_get(EVENT_FLAG_CAT_MSG_1F) != 0) message = 0x264;
            else { event_flag_set(EVENT_FLAG_CAT_MSG_1F, 1); message = 0x26A; }
        } else message = -1;
        break;
    case ITEM_COW:
        message = 0x261;
        break;
    case ITEM_SHEEP:
        message = 0x262;
        break;
    case ITEM_17A:
        mode = entry->place.location;
        switch (mode) {
        case PLACE_9:
            message = 0x266;
            if (game_state.animals.spawnGate == mode) message = 0x277;
            break;
        case PLACE_19:
            message = 0x269;
            break;
        case PLACE_18:
            message = 0x267;
            break;
        case PLACE_35:
            message = 0x268;
            break;
        default:
            break;
        }
        break;
    case ITEM_17B:
        message = 0x265;
        break;
    default:
        message = -1;
        break;
    }
    if (message != -1) menu_request_open(2, message);
    return -1;
}

/* Cue scripts, one table per runner. */
FARM_CUE_RUNNER(insect_cue_0, {0, 30}, {-1, 0})

/* Walk the action along its heading at its kind's speed; the stray dog uses a different occupant
 * mask. Then set its action to rest or moving by whether it moved, and clear ACTOR_FLAG_CUE_RUNNING. */
static s32 farm_action_walk_cue(FarmActor *object) {
    s32 result;
    if (object->kind != ITEM_STRAY_DOG) {
        result = farm_item_walk_speed(&object->kind);
        farm_walker_move(object, result, OCCUPANT_ESTATE_RECORD | OCCUPANT_TILE | OCCUPANT_CHOICE | OCCUPANT_PLAYER);
    } else {
        result = farm_item_walk_speed(&object->kind);
        farm_walker_move(object, result, OCCUPANT_ESTATE_RECORD | OCCUPANT_SPECIAL | OCCUPANT_CHOICE);
    }
    if (object->speed.vx == 0 && object->speed.vy == 0 && object->speed.vz == 0) {
        object->action = ACTOR_ACTION_REST;
    } else {
        object->action = ACTOR_ACTION_MOVING;
    }
    object->flags &= ~ACTOR_FLAG_CUE_RUNNING;
    return 1;
}

/* The taskId steps of a wandering animal (farm_action_wander): indices into its cue steps. */
enum WanderStep {
    WANDER_STEP_WAIT = 1,       /* waits 30 frames, turns when blocked */
    WANDER_STEP_TURN = 2,       /* waits 60 frames, turns when blocked */
    WANDER_STEP_CHOOSE = 3,     /* 30% back to WANDER_STEP_WAIT, else turns and moves on */
    WANDER_STEP_WALK_SINK = 4,  /* walks along its heading, lowered by 16 */
    WANDER_STEP_WALK = 5,       /* walks along its heading */
};

/* Cue runner for a wandering animal: waits a random time, then turns to a
 * random heading other than 1 when blocked, sometimes waits again, and in
 * steps 4 and 5 walks along its heading (step 4 also lowers it by 16). */
static s32 farm_action_wander(FarmActor *self)
{
    static FarmCueStep steps[6] = {{0, 30}, {2, 60}, {3, 0}, {3, 90}, {3, 300}, {-1, 0}};
    s32 finished = 0;
    s16 step = self->taskId;

    switch (step) {
    case CUE_STEP_START:
        self->taskId = WANDER_STEP_WAIT;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        self->taskTime = rand_range(60);
        break;
    case CUE_STEP_STOP:
        farm_action_clear(self);
        finished = 1;
        break;
    case CUE_STEP_PLAY:
    default:
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime) != 0) {
            self->taskId = CUE_STEP_STOP;
            break;
        }
        switch (self->taskId) {
        case WANDER_STEP_WAIT:
        case WANDER_STEP_TURN:
            if (RECORD_BLOCKED(self)) {
                self->taskId = WANDER_STEP_WALK_SINK;
                do {
                    self->facing = rand_range(4);
                } while (self->facing == 1);
            }
            break;
        case WANDER_STEP_CHOOSE:
            if ((u32)rand_range(100) < 30 && !RECORD_BLOCKED(self)) {
                self->taskId = WANDER_STEP_WAIT;
                farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime);
            } else {
                self->taskId++;
                do {
                    self->facing = rand_range(4);
                } while (self->facing == 1);
            }
            break;
        case WANDER_STEP_WALK_SINK:
            farm_vector_adjust(self->facing, &self->speed, 60);
            self->speed.vy -= 16;
            break;
        case WANDER_STEP_WALK:
            farm_vector_adjust(self->facing, &self->speed, 60);
            break;
        }
        break;
    }
    return finished;
}

/* Cue runner for FARM_TASK_VISITOR_WALK: waits 15 frames, then walks for 45 at speed 12. */
static s32 visitor_cue_3(FarmActor *self) {
    static FarmCueStep steps[3] = {{0, 15}, {1, 45}, {-1, 0}};
    s16 step;
    s32 finished;

    step = self->taskId;
    finished = 0;
    switch (step) {
    case CUE_STEP_START:
        self->taskId = CUE_STEP_PLAY;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_STOP:
        farm_action_clear(self);
        finished = 1;
        break;
    case CUE_STEP_PLAY:
    default:
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime) != 0) {
            self->taskId = CUE_STEP_STOP;
        } else if (self->taskId == 2) {
            farm_vector_adjust(self->facing, &self->speed, 12);
        }
        break;
    }
    return finished;
}

/* Cue runner for FARM_TASK_VISITOR_SIT: plays its three poses, 5, 45 and 5 frames, then stops. */
static s32 visitor_cue_4(FarmActor *self) {
    static FarmCueStep steps[4] = {{4, 5}, {5, 45}, {6, 5}, {-1, 0}};
    s32 finished = 0;
    s16 step = self->taskId;

    switch (step) {
    case CUE_STEP_START:
        self->taskId = CUE_STEP_PLAY;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_STOP:
        farm_action_clear(self);
        finished = 1;
        break;
    case CUE_STEP_PLAY:
    default:
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime) != 0) {
            self->taskId = CUE_STEP_STOP;
        }
        break;
    }
    return finished;
}

FARM_CUE_RUNNER(rabbit_cue_8, {7, 120}, {0, 15}, {-1, 0})

FARM_CUE_RUNNER(rabbit_cue_9, {8, 60}, {0, 15}, {-1, 0})

FARM_CUE_RUNNER(fox_cue_11, {9, 120}, {0, 15}, {-1, 0})

FARM_CUE_RUNNER(fox_cue_12, {10, 60}, {0, 15}, {-1, 0})

FARM_CUE_RUNNER(squirrel_cue_10, {11, 120}, {0, 15}, {-1, 0})

FARM_CUE_RUNNER(monkey_cue_13, {12, 120}, {0, 15}, {-1, 0})

/* Cue runner for FARM_TASK_PLACED_CICADA: walks 90 frames lowered by 16 a step, then 300 frames
 * without the lowering. */
static s32 insect_cue_14(FarmActor *self) {
    static FarmCueStep steps[4] = {{13, 0}, {13, 90}, {13, 300}, {-1, 0}};
    s16 step;
    s32 finished;

    step = self->taskId;
    finished = 0;
    switch (step) {
    case CUE_STEP_START:
        self->taskId = CUE_STEP_PLAY;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_STOP:
        farm_action_clear(self);
        finished = 1;
        break;
    case CUE_STEP_PLAY:
    default:
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime) != 0) {
            self->taskId = CUE_STEP_STOP;
        } else {
            switch (self->taskId) {
            case 2:
                farm_vector_adjust(self->facing, &self->speed, 60);
                self->speed.vy -= 16;
                break;
            case 3:
                farm_vector_adjust(self->facing, &self->speed, 60);
                break;
            }
        }
        break;
    }
    return finished;
}

/* Cue runner for FARM_TASK_PLACED_BEETLE: walks 90 frames lowered by 16 a step, then 300 frames
 * without the lowering. */
static s32 insect_cue_15(FarmActor *self) {
    static FarmCueStep steps[4] = {{14, 0}, {14, 90}, {14, 300}, {-1, 0}};
    s16 step;
    s32 finished;

    step = self->taskId;
    finished = 0;
    switch (step) {
    case CUE_STEP_START:
        self->taskId = CUE_STEP_PLAY;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_STOP:
        farm_action_clear(self);
        finished = 1;
        break;
    case CUE_STEP_PLAY:
    default:
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime) != 0) {
            self->taskId = CUE_STEP_STOP;
        } else {
            switch (self->taskId) {
            case 2:
                farm_vector_adjust(self->facing, &self->speed, 60);
                self->speed.vy -= 16;
                break;
            case 3:
                farm_vector_adjust(self->facing, &self->speed, 60);
                break;
            }
        }
        break;
    }
    return finished;
}

/* Cue runner for FARM_TASK_PLACED_CRICKET: walks for 300 frames, then stops. */
static s32 insect_cue_16(FarmActor *self) {
    static FarmCueStep steps[3] = {{13, 0}, {13, 300}, {-1, 0}};
    s16 step;
    s32 finished;

    step = self->taskId;
    finished = 0;
    switch (step) {
    case CUE_STEP_START:
        self->taskId = CUE_STEP_PLAY;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_STOP:
        farm_action_clear(self);
        finished = 1;
        break;
    case CUE_STEP_PLAY:
    default:
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime) != 0) {
            self->taskId = CUE_STEP_STOP;
        } else if (self->taskId == 2) {
            farm_vector_adjust(self->facing, &self->speed, 60);
        }
        break;
    }
    return finished;
}

FARM_CUE_RUNNER(cat_cue_17, {15, 120}, {0, 15}, {-1, 0})

/* The steps of a dog's walk-in (farm_action_dog_walk). */
enum DogWalkStep {
    DOG_WALK_ENTER = 0,
    DOG_WALK_TO_DOOR = 1,
    DOG_WALK_CHOOSE = 2,
};

/* Walk the stray dog in: on its first step it is set at the gate, with the dog-walk estate flag
 * and a sound when the player is on the farm. It then walks to the door and, once that walk ends,
 * picks a target to seek: a livestock record on the farm, or a chicken (affection 1) when none, or
 * leaves when there is neither. Returns 1 on the step that hands it on. */
static s32 farm_action_dog_walk(FarmActor *object) {
    s32 done;
    s32 target;
    VECTOR start;

    done = 0;
    switch (object->taskId) {
    case DOG_WALK_ENTER:
        start.vx = -4900;
        start.vy = 0;
        start.vz = -8600;
        game_state.progress.estate.estateFlags |= ESTATE_FLAG_DOG_WALK;
        object->taskId = DOG_WALK_TO_DOOR;
        object->facing = 2;
        object->flags |= ACTOR_FLAG_CUE_RUNNING;
        farm_set_destination(object, start);
        object->position.vx = -4800;
        object->position.vy = -300;
        object->position.vz = -12000;
        object->place.location = PLACE_FARM;
        if (farm_area.mode.place.location == PLACE_FARM) {
            sound_play(0x1C);
        }
        break;
    case DOG_WALK_TO_DOOR:
        object->facing = 2;
        farm_action_walk_cue(object);
        if (!(object->flags & ACTOR_FLAG_APPROACHING_PLAYER)) {
            object->taskId = DOG_WALK_CHOOSE;
        }
        object->facing = 2;
        break;
    case DOG_WALK_CHOOSE:
        if (rand_range(2) != 0) {
            object->facing = 2;
        } else {
            object->facing = 3;
        }
        done = 1;
        object->affection = 0;
        object->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        target = farm_pick_random_slot(0);
        if (target < 0) {
            target = farm_pick_random_slot(1);
            object->affection = 1;
            if (target < 0) {
                farm_actor_set_task(object, FARM_TASK_DOG_LEAVE);
                break;
            }
        }
        object->targetIndex = target;
        farm_actor_set_task(object, FARM_TASK_DOG_SEEK);
        break;
    }
    return done;
}

/* The steps of a stray dog leaving (dog_leave_cue). */
enum DogStrayStep {
    DOG_STRAY_START = 0,
    DOG_STRAY_WALK = 1,
    DOG_STRAY_LEAVE = 2,
    DOG_STRAY_CLEAR = 3,
};

/* Walk the stray dog out: on its first step it clears the dog-walk and stray-dog estate flags and
 * heads for the gate; it then walks until its z is below -11999, and clears its slot. Returns 1
 * on the step that finishes it. */
static s32 dog_leave_cue(FarmActor *object) {
    s32 done;
    VECTOR start;

    done = 0;
    if ((u16)object->cue.phase < 2)
        cue_set(&object->cue, 0x21, 0x5A);
    switch (object->taskId) {
    case DOG_STRAY_START:
        game_state.progress.estate.estateFlags &= ~ESTATE_FLAG_DOG_WALK;
        game_state.progress.estate.estateFlags &= ~ESTATE_FLAG_STRAY_DOGS;
        object->taskId = DOG_STRAY_WALK;
        object->flags |= ACTOR_FLAG_CUE_RUNNING;
        start.vx = -4900;
        start.vy = 0;
        start.vz = -8900;
        farm_set_destination(object, start);
        break;
    case DOG_STRAY_WALK:
        farm_set_proximity(200);
        farm_action_walk_cue(object);
        if (!(object->flags & ACTOR_FLAG_APPROACHING_PLAYER))
            object->taskId = DOG_STRAY_LEAVE;
        object->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    case DOG_STRAY_LEAVE:
        object->facing = 0;
        object->flags |= ACTOR_FLAG_BIT21;
        farm_action_walk_cue(object);
        if (!(object->flags & ACTOR_FLAG_BIT21))
            farm_vector_adjust(0, &object->speed, 12);
        if (object->position.vz < -11999)
            object->taskId = DOG_STRAY_CLEAR;
        object->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    case DOG_STRAY_CLEAR:
        object->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        farm_action_clear(object);
        done = 1;
        break;
    }
    return done;
}

/* Seek the stray dog's target. Before 7:00 it leaves. Otherwise it takes the first animal on the
 * farm within 800 of its position on x and z (a chicken sets affection 1), walks there, and switches
 * to FARM_TASK_DOG_HERD once that walk ends (ACTOR_FLAG_APPROACHING_PLAYER clears). */
static s32 farm_action_seek_item(FarmActor *object) {
    s32 result;
    s32 item_class;
    void *unused;

    if (game_state.progress.clock.time.hm.hour < 7) {
        farm_actor_set_task(object, FARM_TASK_DOG_LEAVE);
    }

    result = farm_find_item_in_range(object->position, PLACE_FARM, 800, &item_class, &unused);
    if (result != -1) {
        object->targetIndex = result;
        if (item_class == 2) {
            object->affection = 1;
        } else {
            object->affection = 0;
        }
    }

    if (object->affection == 0) {
        farm_set_destination(object, game_state.animals.livestock[object->targetIndex].actor.position);
    } else {
        farm_set_destination(object, game_state.animals.chickens[object->targetIndex].actor.position);
    }

    farm_action_walk_cue(object);
    result = object->flags;
    result &= ACTOR_FLAG_APPROACHING_PLAYER;
    if (result == 0) {
        farm_actor_set_task(object, FARM_TASK_DOG_HERD);
    }
    return 1;
}

/* Cue runner for the dog herding its target. On its first step it sends the target (the livestock
 * or chicken chosen by farm_action_seek_item or on the last finish) to a point 2000 units ahead of
 * the dog, facing the dog's way, and starts the target's action 3. On finishing it picks the next
 * target with farm_pick_random_slot. */
static s32 dog_herd_cue(FarmActor *self) {
    static FarmCueStep steps[3] = {{16, 120}, {0, 15}, {-1, 0}};
    s32 finished = 0;
    s16 step = self->taskId;
    VECTOR box;
    FarmActor *target;
    s32 (*action)();
    s32 index;

    switch (step) {
    case CUE_STEP_START:
        farm_vector_adjust(self->facing, &box, 2000);
        box.vx += self->position.vx;
        box.vy += self->position.vy;
        box.vz += self->position.vz;
        if (self->affection == 0) {
            target = &game_state.animals.livestock[self->targetIndex].actor;
            if (target->flags & ACTOR_FLAG_BIT16) {
                action = livestock_action_dispatch_b;
            } else {
                action = livestock_action_dispatch;
            }
        } else {
            action = 0;
            target = &game_state.animals.chickens[self->targetIndex].actor;
        }
        if (target->place.location == PLACE_FARM) {
            farm_set_destination(target, box);
            target->facing = self->facing;
            if (action != 0) {
                action(target, 3);
            } else {
                farm_actor_dispatch_state(target, 3);
            }
        }
        self->taskId = CUE_STEP_PLAY;
        self->flags |= ACTOR_FLAG_CUE_RUNNING;
        if (farm_area.mode.place.location == self->place.location) {
            sound_play(0x1C);
        }
        break;
    case CUE_STEP_STOP:
        finished = 1;
        self->interruptTime = 300;
        self->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        self->affection = 0;
        index = farm_pick_random_slot(0);
        if (index < 0) {
            index = farm_pick_random_slot(1);
            self->affection = 1;
            if (index < 0) {
                farm_actor_set_task(self, FARM_TASK_DOG_LEAVE);
                break;
            }
        }
        self->targetIndex = index;
        farm_actor_set_task(self, FARM_TASK_DOG_SEEK);
        break;
    case CUE_STEP_PLAY:
    default:
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->taskTime) != 0) {
            self->taskId = CUE_STEP_STOP;
        }
        break;
    }
    return finished;
}

/* Cue runner for FARM_TASK_COUNTDOWN. On step 0 it shows its cue icon, faces the start value set by
 * farm_action_set_countdown, walks at 24 with a sound when the player is on the farm. Then it runs its
 * table (90 frames, then 15), using interruptTime as the frame timer. Returns 1 only on the tick that
 * finishes the cue. */
static s32 cue_run_tick(FarmActor *self) {
    static FarmCueStep steps[3] = {{17, 90}, {0, 15}, {-1, 0}};
    s16 step;
    s32 finished;
    s16 remaining;
    u16 next_remaining;

    step = self->taskId;
    finished = 0;
    switch (step) {
    case CUE_STEP_START:
        cue_set(&self->cue, CUE_ICON_NEGLECT, 0x2D);
        self->taskId = CUE_STEP_PLAY;
        self->interruptTime = 0;
        self->taskTime = 0;
        self->facing = farm_action_start_value;
        self->flags = (self->flags & ~ACTOR_FLAG_APPROACHING_PLAYER) | ACTOR_FLAG_CUE_RUNNING;
        farm_walker_move(self, 24, OCCUPANT_ESTATE_RECORD | OCCUPANT_SPECIAL | OCCUPANT_CHOICE);
        if (farm_area.mode.place.location == self->place.location) {
            sound_play(0x1A);
        }
        break;
    case CUE_STEP_STOP:
        finished = 1;
        self->interruptTime = 300;
        self->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        break;
    case CUE_STEP_PLAY:
    default:
        remaining = self->interruptTime;
        next_remaining = self->interruptTime;
        if (remaining != 0) {
            self->interruptTime = next_remaining - 1;
        }
        if (farm_cue_step_run(self, steps, &self->taskId, -1, &self->interruptTime) != 0) {
            self->taskId = CUE_STEP_STOP;
        } else {
            farm_walker_move(self, 24, OCCUPANT_ESTATE_RECORD | OCCUPANT_SPECIAL | OCCUPANT_CHOICE);
        }
        break;
    }
    return finished;
}

