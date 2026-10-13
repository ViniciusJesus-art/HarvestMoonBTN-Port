#ifndef ACTOR_H
#define ACTOR_H

/* The farm actor record every animal and actor starts with, and the axes the nudge
 * commands move an actor's limits along. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "anim.h"
#include "clock.h"
#include "game_types.h"
#include "items.h"

/* The 4-byte cue pair every FarmVillager and actor carries. */
typedef struct ItemCue {
    s16 id;             /* -1 = idle */
    s16 phase;
} ItemCue;

/* A name: NAME_LENGTH u16 slots, the characters then a 0xFFFF terminator in the
 * last slot, so NAME_CHARS characters at most. */
#define NAME_LENGTH 9
#define NAME_CHARS  8

/* FarmActor.flags. */
enum ActorFlag {
    ACTOR_FLAG_FED_TODAY       = (1 << 0),
    ACTOR_FLAG_TALKED_TODAY    = (1 << 1),
    ACTOR_FLAG_BIT2            = (1 << 2),
    ACTOR_FLAG_COW_CUE_TODAY   = (1 << 3),
    ACTOR_FLAG_FED_YESTERDAY   = (1 << 4),
    ACTOR_FLAG_TRAINED_TODAY   = (1 << 5),
    ACTOR_FLAG_PRIZE_WINNER    = (1 << 6),
    ACTOR_FLAG_DIED            = (1 << 8),
    ACTOR_FLAG_NAMING_PENDING  = (1 << 9),  /* a newborn waits to be named */
    ACTOR_FLAG_IN_EVENT        = (1 << 10),
    ACTOR_FLAG_CUE_RUNNING     = (1 << 11),
    ACTOR_FLAG_BIT12           = (1 << 12),
    ACTOR_FLAG_APPROACHING_PLAYER = (1 << 13),  /* walks to the player */
    ACTOR_FLAG_BIT14           = (1 << 14),
    ACTOR_FLAG_ADULT           = (1 << 15),
    ACTOR_FLAG_BIT16           = (1 << 16),
    ACTOR_FLAG_HELD            = (1 << 17),     /* the player holds the animal */
    ACTOR_FLAG_CONTEST_ENTRANT = (1 << 18),
    ACTOR_FLAG_DIED_OF_AGE     = (1 << 19),
    ACTOR_FLAG_BIT20           = (1 << 20),
    ACTOR_FLAG_BIT21           = (1 << 21),
    ACTOR_FLAG_BIT22           = (1 << 22),
    ACTOR_FLAG_ADULT_SPRITE    = (1 << 23),
    ACTOR_FLAG_JUMPING         = (1 << 24),
    ACTOR_FLAG_CLAIMED         = (1 << 25),
    ACTOR_FLAG_BIT26           = (1 << 26),
    ACTOR_FLAG_BIT27           = (1 << 27),
    ACTOR_FLAG_BIT28           = (1 << 28),
    ACTOR_FLAG_BIT29           = (1 << 29),
    ACTOR_FLAG_BIT30           = (1 << 30),
    ACTOR_FLAG_BIT31           = (1u << 31),
};

/* FarmActor.action: the pose an animal or helper is shown in. Each animal's numbers are
 * its own. */
enum ActorAction {
    ACTOR_ACTION_REST         = 0,    /* stopped: speed zero after a walk step */
    ACTOR_ACTION_MOVING       = 1,    /* walking: speed nonzero after the same step */
    ACTOR_ACTION_3            = 3,    /* horse: moving without a jump, or at rest with its sprite idle */
    ACTOR_ACTION_JUMPING      = 4,    /* horse: a jump (ACTOR_FLAG_JUMPING) */
    ACTOR_ACTION_5            = 5,    /* livestock: a rest-like pose, tested alongside REST */
    ACTOR_ACTION_CARRY_MOVING = 6,    /* a harvest helper walking with an item */
    ACTOR_ACTION_CARRY_REST   = 0xF,  /* a harvest helper at rest with an item */
};

/* The head every animal and actor record of the farm starts with. Each record
 * type's own fields follow it. The names are the animal pages' (動物NO,
 * 愛情度, 誕生月/日, 行動, 処理NO/ID/時間, 割込み時間, 現在地, フラグ, X/Y/Z,
 * スピード, 目的地). */
typedef struct FarmActor {
    ItemId kind;        /* 0 = a free slot; the item kind of an action */
    u16 targetIndex;    /* an action's chosen target record */
    u16 name[NAME_LENGTH];  /* 0xFFFF in [0] on a new game */
    s16 affection;      /* 0..255 */
    s16 waitTimer;      /* a walker's countdown */
    FarmDate date;      /* an animal's birthday; an action's claim date */
    u16 facing;         /* the facing the actor is shown at */
    u16 action;         /* 行動: an ActorAction, or a table pose (0..99) */
    s16 taskNo;         /* an action's handler */
    s16 taskId;         /* a helper's script step (-1: done) */
    s16 taskTime;       /* the cue step's tick count */
    s16 interruptTime;  /* an action's ticks: at 300 the event runs */
    LimitPair place;    /* .location: the area it is in */
    u16 unused32;
    u32 flags;          /* ACTOR_FLAG_* */
    VECTOR position;
    VECTOR speed;
    VECTOR destination;
    VECTOR target;      /* the cell a walker heads for */
    AnimState sprite;   /* its sprite */
    ItemCue cue;
} FarmActor;

/* The FarmActor whose position is at `p`. */
#define ACTOR_OF_POSITION(p) ((FarmActor *)((u8 *)(p) - 0x38))

/* The axis a nudge moves: a component of the object's limit vector, and the
 * direction it moves in. A move that does not happen reports the axis as its cue. */
enum NudgeAxis {
    NUDGE_Z_UP = 0,
    NUDGE_X_UP = 1,
    NUDGE_Z_DOWN = 2,
    NUDGE_X_DOWN = 3,
    NUDGE_Y_UP = -1,
    NUDGE_Y_DOWN = -2,
};

#endif /* ACTOR_H */
