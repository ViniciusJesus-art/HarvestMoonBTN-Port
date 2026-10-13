/* The farm horse and pony: its sprite, its bond with the player, its cues and its actions. */
#include <memory.h>
#include "types.h"
#include "actor.h"
#include "anim.h"
#include "animals.h"
#include "cue.h"
#include "dog.h"
#include "farm.h"
#include "farm_load.h"
#include "farm_shared.h"
#include "farm_state.h"
#include "horse.h"
#include "items.h"
#include "menu_window.h"
#include "player.h"
#include "scene.h"
#include "screen_fade.h"

typedef void (*HorseActionTick)(FarmActor *);

typedef struct {
    u16 x;
    u16 y;
} HorseFrameEntry;

/* The affection and training of the horse are capped here. */
#define HORSE_STAT_CAP 255

/* The start of a new horse or pony: its affection and its place at the stable. */
#define HORSE_START_AFFECTION 10
#define HORSE_START_X         460
#define HORSE_START_Z         600

/* The affection bands of the training step: below 100 a step of 1, below 200 of 2, else 3. */
#define HORSE_BOND_BAND_LOW   100
#define HORSE_BOND_BAND_HIGH  200

/* The message horse_cue_sound opens: the normal one, and the one at the top of the bond. */
enum HorseMessage {
    HORSE_MSG_BOND = 0x258,         /* affection below 201 */
    HORSE_MSG_DEVOTED = 0x259,      /* affection 201 or more */
};

/* The horse's states (horse_set_state). */
enum HorseState {
    HORSE_STATE_WALK = 0x11,        /* walks to the player after a talk, and jumps */
};

static s32 horse_init(FarmPet *obj, s32 value);
static s32 horse_set_flags(FarmActor *obj);
static s32 horse_frame_lookup(u16 action, u16 facing, u16 *frameX, u16 *frameY);
static s32 horse_pick_state(FarmActor *object);
static s32 horse_roll_visit(FarmActor *object);
static s32 horse_start_scold_cue(FarmActor *cue);
static s32 horse_set_bit2_cue(FarmActor *object);
static s32 horse_cue_sound(FarmActor *object);
static s32 horse_mover_update(FarmActor *object);
static s32 horse_cue_affection_drop(FarmActor *object);
static s32 horse_action_check(FarmActor *obj);
static s32 horse_action_refresh(FarmActor *p);
static s32 horse_action_start(FarmActor *object);

void horse_sprite_refresh(FarmActor *object) {
    u16 animation;
    u16 frame;

    if (!(game_state.globalFlags & GLOBAL_FLAG_TRANSITION) && object->sprite.source != 0) {
        horse_frame_lookup(object->action, object->facing, &animation, &frame);
        if (animation != object->sprite.animation) {
            anim_select_range(&object->sprite, animation);
        }
        if ((frame != 0) && (frame != object->sprite.frame)) {
            anim_set_frame(&object->sprite, frame);
        }
        anim_set_tick_flag(0);
        farm_draw_actor(object);
    }
}

void horse_pair_at_38(FarmPet *obj) {
    farm_pair_at_38(&obj->actor);
}

FarmPet *horse_spawn(s32 kind) {
    if ((u32)(kind - ITEM_PONY) < 2U) {
        FarmPet *p = &game_state.animals.horse;
        if (p->actor.kind == ITEM_NOTHING) {
            horse_init(p, kind);
            return p;
        }
    }
    return 0;
}

static s32 horse_timed_flag_tick(FarmActor *p) {
    return farm_timed_flag_tick(p);
}

/* Tick the horse's bond: affection rises by 2 for each of talking and BIT2, falls by 1
 * on a day with none of them or training, and drops by 5 outdoors in bad weather. Training
 * steps up by 1 to 3 by affection on a trained day. On the year rule the pony becomes
 * a horse. Returns -1 while it is unused or in an event. */
s32 horse_update_bond(FarmPet *object) {
    s32 step;

    if (object->actor.kind == ITEM_NOTHING || (object->actor.flags & ACTOR_FLAG_IN_EVENT)) {
        return -1;
    }
    if (object->actor.flags & ACTOR_FLAG_TALKED_TODAY) {
        object->actor.affection += 2;
        object->actor.affection = object->actor.affection > HORSE_STAT_CAP ? HORSE_STAT_CAP : object->actor.affection;
    }
    if (object->actor.flags & ACTOR_FLAG_BIT2) {
        object->actor.affection += 2;
        object->actor.affection = object->actor.affection > HORSE_STAT_CAP ? HORSE_STAT_CAP : object->actor.affection;
    }
    if (!(object->actor.flags & (ACTOR_FLAG_TALKED_TODAY | ACTOR_FLAG_BIT2 | ACTOR_FLAG_TRAINED_TODAY))) {
        object->actor.affection--;
        object->actor.affection = object->actor.affection < 0 ? 0 : object->actor.affection;
    }
    if (object->actor.flags & ACTOR_FLAG_TRAINED_TODAY) {
        if (object->actor.affection < HORSE_BOND_BAND_LOW) {
            step = 1;
        } else if (object->actor.affection < HORSE_BOND_BAND_HIGH) {
            step = 2;
        } else {
            step = 3;
        }
        object->training += step;
        object->training = object->training > HORSE_STAT_CAP ? HORSE_STAT_CAP : object->training;
    }
    if (object->actor.place.location == PLACE_FARM && game_state.world.weather != 0) {
        object->actor.affection -= 5;
        object->actor.affection = object->actor.affection < 0 ? 0 : object->actor.affection;
    }
    if (object->actor.date.year != game_state.progress.clock.date.year && object->actor.date.season == game_state.progress.clock.date.season &&
        object->actor.date.day <= game_state.progress.clock.date.day) {
        object->actor.kind = ITEM_HORSE;
        object->actor.flags |= ACTOR_FLAG_ADULT;
        horse_set_flags(&object->actor);
    }
    object->actor.flags &= ~(ACTOR_FLAG_TALKED_TODAY | ACTOR_FLAG_BIT2 | ACTOR_FLAG_TRAINED_TODAY);
    cue_reset(&object->actor.cue);
    return 0;
}

static s32 horse_promote_flag(FarmActor *flags) {
    return farm_record_promote_flag(flags);
}

void *const horse_action_start_handlers[20] = {
    0, 0, 0, horse_cue_affection_drop, horse_start_scold_cue, horse_cue_sound, horse_set_bit2_cue, 0, 0, 0, 0, 0, 0,
    horse_mover_update, 0, 0, 0, 0, 0, horse_roll_visit,
};

void *const horse_action_tick_handlers[20] = {
    horse_idle_cue, horse_cue_1, horse_cue_2, 0, horse_idle_cue, horse_idle_cue,
    horse_idle_cue, 0, 0, 0, 0, 0, 0, horse_action_check, 0, 0, 0, horse_action_start, horse_action_refresh,
    0,
};

/* Advance a farm object's timer and dispatch its callback when needed. */
s32 horse_dispatch_tick(FarmActor *object) {
    HorseActionTick callback;

    if (object->kind == ITEM_NOTHING) {
        return -1;
    }
    callback = (HorseActionTick)horse_action_tick_handlers[(u16)object->taskNo];
    if (callback != 0) {
        callback(object);
    }
    if ((object->flags & ACTOR_FLAG_CUE_RUNNING) == 0 && object->interruptTime >= 300) {
        horse_pick_state(object);
        object->interruptTime = 0;
    }
    object->interruptTime++;
    return 0;
}

/* Start the horse in `state` (a jump forces HORSE_STATE_WALK): run its start handler, and when it
 * has a tick handler reset its task and stop its speed. Returns the handler's result, or -1 in an
 * event or when the handler refuses. */
s32 horse_set_state(FarmActor *object, s32 state) {
    s32 callback_result;
    s32 index;
    s32 flags;
    s32 (*callback)(FarmActor *);

    index = state;
    flags = object->flags;
    callback_result = 0;
    if (flags & ACTOR_FLAG_JUMPING) {
        index = HORSE_STATE_WALK;
    }
    if (flags & ACTOR_FLAG_IN_EVENT) {
        return -1;
    }
    callback = (s32(*)(FarmActor *))horse_action_start_handlers[index];
    if (callback != 0) {
        callback_result = callback(object);
    }
    if (horse_action_tick_handlers[index] != 0 && callback_result != -1) {
        object->taskNo = index;
        object->taskId = 0;
        object->taskTime = 0;
        object->interruptTime = 0;
        farm_vector_adjust(4, &object->speed, 0);
    }
    return callback_result;
}

void horse_clear_record(FarmPet *dst) {
    memset(dst, 0, sizeof(FarmPet));
    dst->actor.kind = ITEM_NOTHING;
}

/* Raise the horse's ACTOR_FLAG_BIT27 flag and, on the farm and at place 3, load and
 * attach its sprite for ITEM_PONY / ITEM_HORSE (plus anim_set_offsets at place 3);
 * any other kind drops the flag again. */
void horse_load_sprite(void) {
    FarmPet *object;

    object = &game_state.animals.horse;
    object->actor.flags |= ACTOR_FLAG_BIT27;
    if (farm_area.mode.place.location != PLACE_3 && farm_area.mode.place.location != PLACE_FARM) {
        object->actor.sprite.source = 0;
        return;
    }
    switch (object->actor.kind) {
    case ITEM_PONY:
        farm_load_anim8(1);
        anim_bind_file(&object->actor.sprite, (AnimFile *)farm_area.anim8);
        if (farm_area.mode.place.location == PLACE_3) {
            anim_set_offsets(&object->actor.sprite, -256, 0, 0, 0);
        }
        break;
    case ITEM_HORSE:
        farm_load_anim8(0);
        anim_bind_file(&object->actor.sprite, (AnimFile *)farm_area.anim8);
        if (farm_area.mode.place.location == PLACE_3) {
            anim_set_offsets(&object->actor.sprite, -256, 0, 0, 0);
        }
        break;
    default:
        object->actor.sprite.source = 0;
        object->actor.flags &= ~ACTOR_FLAG_BIT27;
        break;
    }
}

static s32 horse_init(FarmPet *obj, s32 value) {
    if (obj->actor.kind != ITEM_NOTHING) {
        return -1;
    }
    memset(obj, 0, sizeof(FarmPet));
    obj->actor.kind = value;
    obj->actor.affection = HORSE_START_AFFECTION;
    obj->actor.date = game_state.progress.clock.date;
    obj->actor.place.location = PLACE_3;
    obj->actor.position.vx = HORSE_START_X;
    obj->actor.position.vy = 0;
    obj->actor.position.vz = HORSE_START_Z;
    obj->actor.name[0] = 0xFFFF;
    horse_set_flags(&obj->actor);
    cue_reset(&obj->actor.cue);
    return 0;
}

static s32 horse_set_flags(FarmActor *obj) {
    FarmActor *p = obj;
    switch (p->kind) {
    case ITEM_PONY:
        p->flags &= ~ACTOR_FLAG_ADULT;
        p->flags &= ~ACTOR_FLAG_ADULT_SPRITE;
        break;
    case ITEM_HORSE:
        p->flags |= (ACTOR_FLAG_ADULT | ACTOR_FLAG_ADULT_SPRITE);
        break;
    }
    return 0;
}

static HorseFrameEntry horse_frame_table[5][4] = {
    {{0, 0}, {0, 1}, {0, 2}, {0, 3}},
    {{1, 0}, {2, 0}, {3, 0}, {4, 0}},
    {{5, 0}, {6, 0}, {7, 0}, {8, 0}},
    {{9, 0}, {10, 0}, {11, 0}, {12, 0}},
    {{13, 0}, {14, 0}, {15, 0}, {16, 0}},
};

static s32 horse_frame_lookup(u16 action, u16 facing, u16 *frameX, u16 *frameY) {
    *frameX = horse_frame_table[action][facing].x;
    if (frameY != 0) {
        *frameY = horse_frame_table[action][facing].y;
    }
    return 0;
}

/* Select the farm object's next state and dispatch it when it changes. In the farm's day
 * hours the horse idles in state 1, and a roll may send it to walk to the player (when it
 * is approaching) or to state 0x12, or to state 2; outside those hours it rests in state 2. */
static s32 horse_pick_state(FarmActor *object) {
    s32 state;
    s32 next;
    s32 roll;
    s32 flags;

    if ((u32)((u16)game_state.progress.clock.time.hm.hour - FARM_DAY_HOUR_FIRST) < FARM_DAY_HOUR_COUNT) {
        state = 1;
        if (object->taskNo != 0xF) {
            flags = object->flags;
            if (flags & ACTOR_FLAG_APPROACHING_PLAYER) {
                roll = rand_range(7);
                next = HORSE_STATE_WALK;
            } else if (flags & ACTOR_FLAG_ADULT) {
                roll = rand_range(2);
                next = 0x12;
            } else {
                roll = rand_range(2);
                next = 0x12;
            }
            switch (roll) {
            case 0:
                state = next;
                break;
            case 1:
                state = 2;
                break;
            default:
                state = next;
                break;
            }
        }
    } else {
        state = 2;
    }
    if (state != object->taskNo) {
        horse_set_state(object, state);
    }
    return 0;
}

/* Talk to the player with a chance of affection + 100 out of 200: the horse walks to the player
 * and the day's talk is marked. */
static s32 horse_roll_visit(FarmActor *object) {
    s32 roll;
    if (object->kind == ITEM_NOTHING) return -1;
    if (farm_area.mode.place.location != object->place.location) return -1;
    if (object->flags & ACTOR_FLAG_IN_EVENT) return -1;
    roll = rand_range(200);
    if (roll < object->affection + 100) {
        farm_set_destination(object, game_state.player.position);
        object->flags |= ACTOR_FLAG_TALKED_TODAY;
        if (farm_area.mode.place.location == object->place.location) farm_request_track(0xE);
        horse_set_state(object, HORSE_STATE_WALK);
        return 0;
    }
    return -1;
}

/* Scold the horse: affection -10, the scold icon and track 0xE. */
static s32 horse_start_scold_cue(FarmActor *object) {
    if (object->kind == ITEM_NOTHING) {
        return -1;
    }
    object->affection -= 10;
    object->affection = (object->affection < 0) ? 0 : object->affection;
    cue_set(&object->cue, CUE_ICON_SCOLD, 0x1E);
    if (farm_area.mode.place.location == object->place.location) {
        farm_request_track(0xE);
    }
    return 0;
}

/* Praise the horse: the praise icon, and BIT2 for the day's bond. Track 0xD plays here. */
static s32 horse_set_bit2_cue(FarmActor *object) {
    if (object->kind != ITEM_NOTHING) {
        if (!(object->flags & ACTOR_FLAG_BIT2)) {
            cue_set(&object->cue, CUE_ICON_PRAISE, 0x2D);
        }
        object->flags |= ACTOR_FLAG_BIT2;
        if (farm_area.mode.place.location != object->place.location) {
            return -1;
        }
        farm_request_track(0xD);
    }
    return -1;
}

/* Open the horse's message for its bond: the bond message below 201 affection, the devoted one
 * from 201 on. Track 0xD plays when the horse is in the same place. */
static s32 horse_cue_sound(FarmActor *object) {
    s16 affection;
    if (object->kind != ITEM_NOTHING) {
        FarmPet *horse = &game_state.animals.horse;

        affection = horse->actor.affection;
        if (affection < 9) {
            menu_request_open(2, HORSE_MSG_BOND);
        } else if (affection >= 201) {
            menu_request_open(2, HORSE_MSG_DEVOTED);
        } else {
            menu_request_open(2, HORSE_MSG_BOND);
        }
        if (farm_area.mode.place.location != object->place.location) {
            return -1;
        }
        farm_request_track(0xD);
    }
    return -1;
}

static s32 horse_mover_update(FarmActor *object) {
    return farm_mover_update(object);
}

/* Neglect the horse: affection -10 and the neglect icon. */
static s32 horse_cue_affection_drop(FarmActor *object) {
    cue_set(&object->cue, CUE_ICON_NEGLECT, 0x2D);
    object->affection -= 10;
    object->affection = (object->affection < 0) ? 0 : object->affection;
    if (farm_area.mode.place.location == object->place.location) {
        farm_request_track(0xE);
    }
    return 0;
}

FARM_CUE_RUNNER(horse_idle_cue, {0, 30}, {-1, 0})

FARM_CUE_RUNNER_NO_TIMER(horse_cue_2, {0, 10}, {2, 0}, {-1, 0})

FARM_CUE_RUNNER(horse_cue_1, {0, 10}, {-1, 0})

/* Check the horse's walk each tick: with the approach flag clear, the walk ends and the
 * idle timer is set to 300 (returns 1); otherwise its place is cleared and the walk refreshed. */
static s32 horse_action_check(FarmActor *obj) {
    u32 flags = obj->flags | ACTOR_FLAG_CUE_RUNNING;
    obj->flags = flags;
    if (!(flags & ACTOR_FLAG_APPROACHING_PLAYER)) {
        obj->flags = flags & ~ACTOR_FLAG_CUE_RUNNING;
        obj->interruptTime = 300;
        return 1;
    } else {
        obj->place.value = 0;
        horse_action_refresh(obj);
        return 0;
    }
}

/* Clear the adult sprite flag and walk the horse one step (speed 12; pose 0x8030 in state 0xD,
 * else 0x8031). Then set its action from the speed and clear the cue-running flag. Returns 1. */
static s32 horse_action_refresh(FarmActor *p) {
    s32 mode;

    p->flags &= ~ACTOR_FLAG_ADULT_SPRITE;
    mode = 0x8031;
    if (p->taskNo == 0xD) mode = 0x8030;
    farm_walker_move(p, 12, mode);
    if (p->speed.vx == 0 && p->speed.vy == 0 && p->speed.vz == 0)
        p->action = ACTOR_ACTION_REST;
    else
        p->action = ACTOR_ACTION_MOVING;
    p->flags &= ~ACTOR_FLAG_CUE_RUNNING;
    return 1;
}

/* Start the horse's walk for an action: pick its pose, set the walk speed, and set the
 * idle timer unless it is heading for the player. */
static s32 horse_action_start(FarmActor *object) {
    s32 flags;

    flags = object->flags;
    if (flags & ACTOR_FLAG_ADULT) {
        object->flags |= ACTOR_FLAG_ADULT_SPRITE;
    }
    farm_walker_move(object, 24, 0x8031);
    object->flags |= ACTOR_FLAG_CUE_RUNNING;

    if (object->speed.vx == 0 && object->speed.vy == 0 && object->speed.vz == 0) {
        object->action = ACTOR_ACTION_REST;
        if (!(object->sprite.flags & 0x300)) {
            object->action = ACTOR_ACTION_3;
        }
    } else if (object->flags & ACTOR_FLAG_JUMPING) {
        object->action = ACTOR_ACTION_JUMPING;
    } else {
        object->action = ACTOR_ACTION_3;
    }

    if (object->sprite.flags & 0x300) {
        flags = object->flags;
        if (!(flags & ACTOR_FLAG_JUMPING)) {
            object->flags = flags & ~ACTOR_FLAG_CUE_RUNNING;
        }
    }

    if (!(object->flags & ACTOR_FLAG_APPROACHING_PLAYER)) {
        object->interruptTime = 300;
        if ((object->sprite.flags & 0x300) && !(object->flags & ACTOR_FLAG_JUMPING)) {
            farm_vector_adjust(4, &object->speed, 0);
        }
    }
    return 1;
}

/* Put the horse at the player's position. */
void horse_move_to_player(FarmPlayer *player) {
    game_state.animals.horse.actor.position.vx = player->position.vx;
    game_state.animals.horse.actor.position.vy = player->position.vy;
    game_state.animals.horse.actor.position.vz = player->position.vz;
    game_state.animals.horse.actor.position.pad = player->position.pad;
}

/* Turn the horse to the player's facing and set its animation range from the player's mode. */
void horse_set_anim_from_tool(FarmPlayer *player) {
    FarmPet *horse;

    horse = &game_state.animals.horse;
    horse->actor.facing = player->facing38;
    if (player->mode == PLAYER_MODE_IDLE) {
        anim_select_range(&horse->actor.sprite, 0);
    }
    if (player->mode == PLAYER_MODE_POSE_2) {
        anim_select_range(&horse->actor.sprite, (horse->actor.facing + 1) & 0xFF);
    }
    if (player->mode == PLAYER_MODE_POSE_4) {
        anim_select_range(&horse->actor.sprite, (horse->actor.facing + 9) & 0xFF);
    }
    if (player->secondSprite.animation == player->facing38 + 0x3F) {
        anim_select_range(&horse->actor.sprite, (horse->actor.facing + 13) & 0xFF);
    } else {
        if (anim_frame_jumps(&horse->actor.sprite) != 0) {
            horse->actor.sprite.frame = horse->actor.facing;
        }
    }
}
