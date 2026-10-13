/* The farm dog: its sprite, its spawn, requests, daily affection and training tick, its
 * phase machine, and its actions: the idle phase, item and action searches, its cues and
 * scripted walks. */
#include <memory.h>
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "actor.h"
#include "anim.h"
#include "animal_actions.h"
#include "animals.h"
#include "clock.h"
#include "cue.h"
#include "dog.h"
#include "estate.h"
#include "farm.h"
#include "farm_load.h"
#include "farm_shared.h"
#include "farm_state.h"
#include "items.h"
#include "livestock.h"
#include "main.h"
#include "scene.h"
#include "screen_fade.h"
#include "sound_request.h"
#include "texture_pack.h"

static s32 dog_request_start(FarmPet *request, s32 id);

/* The OT offset that farm_request_project adds to frame_ot_base for the dog's sprite. */
#define DOG_SPRITE_OT_BASE 0x2800

/* Refresh the sprite's animation and frame from the cue table entry for the action and
 * facing, unless the screen is in transition, then queue its projected draw. */
void pet_refresh_sprite(FarmPet *object) {
    u16 animation;
    u16 frame;
    if (!(game_state.globalFlags & GLOBAL_FLAG_TRANSITION)) {
        cue_anim_lookup(object->actor.action, object->actor.facing, &animation, &frame);
        if (animation != object->actor.sprite.animation) {
            anim_select_range(&object->actor.sprite, animation);
        }
        if (frame != 0 && frame != object->actor.sprite.frame) {
            anim_set_frame(&object->actor.sprite, frame);
        }
        anim_set_tick_flag(0);
        farm_request_project(&object->actor, &object->projectCoords[frame_index], DOG_SPRITE_OT_BASE);
    }
}

void cue_pair_at_38(FarmPet *obj) {
    farm_pair_at_38(&obj->actor);
}

/* Start the dog as `kind` (the adult or the young dog) when `enabled` is 1 and the
 * dog's slot is free. Returns the slot, or 0 if nothing was started. */
FarmPet *dog_start(s32 kind, s32 enabled) {
    FarmPet *slot = 0;

    if ((u32)(kind - ITEM_ADULT_DOG) >= 2) {
        return 0;
    }
    if (enabled == 1) {
        slot = &game_state.animals.dog;
        if (slot->actor.kind != ITEM_NOTHING) {
            return 0;
        }
        dog_request_start(slot, kind);
    }
    return slot;
}

/* Unused. */
static s32 dog_timed_flag_tick(FarmActor *p) {
    return farm_timed_flag_tick(p);
}

s32 (*const dog_enter_handlers[19])(FarmPet *) = {
    0, 0, 0, dog_timer_tick, dog_try_action, dog_seek_item, dog_walk_to_action, dog_accept_request, 0,
    dog_set_bit2_cue, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

s32 (*const dog_cue_handlers[19])() = {
    dog_idle_cue, dog_cue_1, dog_cue_2, 0, dog_start_action, dog_script_drive_object,
    dog_script_follow_motion, dog_script_approach_player, dog_cue_8, dog_cue_8, dog_cue_10,
    dog_cue_11, dog_cue_12, dog_cue_13, dog_cue_14, dog_cue_15,
    dog_cue_16, dog_start_action, dog_set_idle_action,
};

/* Daily update. Affection gains 2 for talking today and 2 for BIT2, loses 1 when neither
 * of those nor training happened, and loses 5 outdoors while the weather is not 0. Training
 * grows by 1 to 3 as affection passes 10 and 20. A young dog grows up in fall of year 0.
 * Returns -1 while the slot is unused, 0 otherwise. */
s32 dog_tick(FarmPet *object)
{
    s32 step;

    if (object->actor.kind == ITEM_NOTHING) {
        return -1;
    }
    if ((object->actor.flags & ACTOR_FLAG_IN_EVENT) &&
        clock_date_index(game_state.progress.clock.date.season, game_state.progress.clock.date.day) == 8) {
        return 0;
    }
    if (object->actor.flags & ACTOR_FLAG_TALKED_TODAY) {
        object->actor.affection += 2;
        object->actor.affection = object->actor.affection > 255 ? 255 : object->actor.affection;
    }
    if (object->actor.flags & ACTOR_FLAG_BIT2) {
        object->actor.affection += 2;
        object->actor.affection = object->actor.affection > 255 ? 255 : object->actor.affection;
    }
    if (!(object->actor.flags & (ACTOR_FLAG_TALKED_TODAY | ACTOR_FLAG_BIT2 | ACTOR_FLAG_TRAINED_TODAY))) {
        object->actor.affection--;
        object->actor.affection = object->actor.affection < 0 ? 0 : object->actor.affection;
    }
    if (object->actor.flags & ACTOR_FLAG_TRAINED_TODAY) {
        if (object->actor.affection < 10) {
            step = 1;
        } else if (object->actor.affection < 20) {
            step = 2;
        } else {
            step = 3;
        }
        object->training += step;
        object->training = object->training > 255 ? 255 : object->training;
    }
    if (game_state.world.weather != 0) {
        switch (object->actor.place.location) {
        case PLACE_FARM: case PLACE_8: case PLACE_C: case PLACE_D: case PLACE_E: case PLACE_F: case PLACE_14: case PLACE_1A:
        case PLACE_1D: case PLACE_22: case PLACE_27: case PLACE_2A: case PLACE_2C: case PLACE_30: case PLACE_34:
            object->actor.affection -= 5;
            object->actor.affection = object->actor.affection < 0 ? 0 : object->actor.affection;
            break;
        }
    }
    if (game_state.progress.clock.date.season == SEASON_FALL && game_state.progress.clock.date.day >= 0 && game_state.progress.clock.date.year == 0 &&
        object->actor.kind == ITEM_YOUNG_DOG) {
        object->actor.kind = ITEM_ADULT_DOG;
        object->actor.flags |= ACTOR_FLAG_ADULT;
        dog_sprite_load(object);
    }
    object->actor.flags &= ~(ACTOR_FLAG_TALKED_TODAY | ACTOR_FLAG_BIT2 | ACTOR_FLAG_TRAINED_TODAY);
    farm_actor_release_held(&object->actor);
    cue_reset(&object->actor.cue);
    if (object->actor.flags & ACTOR_FLAG_BIT20) {
        dog_snapshot_submit(&object->actor);
        object->actor.action = ACTOR_ACTION_REST;
        object->actor.taskNo = 0;
        object->actor.flags &= ~ACTOR_FLAG_BIT20;
        object->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
        dog_set_phase(object, DOG_PHASE_IDLE);
    }
    return 0;
}

/* Unused. */
static s32 dog_promote_record_flags(FarmActor *a) {
    return farm_record_promote_flag(a);
}

/* Run the current cue, re-pick the idle phase after 300 frames with no cue running, and
 * set ACTOR_FLAG_BIT20 while the action is one of the walks (0x12 to 0x15). Resets
 * dog_action_row to DOG_ROW_OWN. Returns -1 for an unused slot. */
s32 dog_update_phase(FarmPet *object) {
    s32 (*callback)();
    s32 (*const *callbacks)();
    u16 callback_index;
    u16 timer;
    u16 phase;
    u32 flags;

    if (object->actor.kind == ITEM_NOTHING) {
        return -1;
    }
    callbacks = dog_cue_handlers;
    callback_index = object->actor.taskNo;
    callback = callbacks[callback_index];
    if (callback != 0) {
        callback(object);
    }
    flags = object->actor.flags;
    if ((flags & ACTOR_FLAG_CUE_RUNNING) == 0 && object->actor.interruptTime >= 300) {
        dog_pick_idle_phase(object);
        object->actor.interruptTime = 0;
    }
    timer = object->actor.interruptTime;
    phase = object->actor.action;
    timer++;
    object->actor.interruptTime = timer;
    if (phase - 0x12 < 4U) {
        object->actor.flags |= ACTOR_FLAG_BIT20;
    } else {
        object->actor.flags &= ~ACTOR_FLAG_BIT20;
    }
    dog_action_row = DOG_ROW_OWN;
    return 0;
}

/* Switch to a new phase, running the phase's enter handler. */
s32 dog_set_phase(FarmPet *object, s32 mode) {
    s32 result;
    u16 phase;

    result = 0;
    if (object->actor.flags & ACTOR_FLAG_IN_EVENT) {
        if (clock_date_index(game_state.progress.clock.date.season, game_state.progress.clock.date.day) == 8) {
            return -1;
        }
    }
    phase = object->actor.taskNo;
    if (object->actor.taskNo == DOG_PHASE_FETCH && mode == DOG_PHASE_TRY_ACTION) {
        return 0;
    }
    if (object->actor.flags & ACTOR_FLAG_JUMPING) {
        mode = DOG_PHASE_START_ACTION;
    }
    if (dog_enter_handlers[mode] != 0) {
        if ((s16)phase == DOG_PHASE_FETCH) {
            result = -1;
        } else if ((s16)phase == DOG_PHASE_SEEK_ITEM && mode == (s16)phase) {
            result = -1;
        } else {
            result = dog_enter_handlers[mode](object);
        }
    }
    if (dog_cue_handlers[mode] != 0 && result != -1) {
        object->actor.taskNo = mode;
        object->actor.taskId = 0;
        object->actor.taskTime = 0;
        object->actor.interruptTime = 0;
        farm_vector_adjust(4, &object->actor.speed, 0);
    }
    return result;
}

/* Unused: zero a dog record and mark its item kind as ITEM_NOTHING. */
static void dog_clear_record(FarmPet *dst) {
    memset(dst, 0, sizeof(FarmPet));
    dst->actor.kind = ITEM_NOTHING;
}

/* Clear the OT, then load the sprite for the dog's kind: the young dog reads the sprite at
 * 0x1420 with the adult flags clear, the adult at 0x23C8 with them set. Then set the two
 * draw places' fixed CLUT and page coordinates. */
s32 dog_sprite_load(FarmPet *screen)
{
    s32 index;
    s32 offset;

    DrawSync(0);
    ClearOTagR(current_frame->ot, FRAME_OT_DEPTH);
    if (screen->actor.kind == ITEM_YOUNG_DOG) {
        screen->actor.flags &= ~ACTOR_FLAG_ADULT;
        screen->actor.flags &= ~ACTOR_FLAG_ADULT_SPRITE;
        index = 1;
        offset = 0x1420;
    } else {
        screen->actor.flags |= (ACTOR_FLAG_ADULT | ACTOR_FLAG_ADULT_SPRITE);
        index = 0;
        offset = 0x23C8;
    }
    farm_load_span(index);
    anim_bind_file(&screen->actor.sprite, (AnimFile *)farm_area.spanBuffer);
    anim_set_bank(&screen->actor.sprite, (ImageBank *)(farm_area.spanBuffer + offset));
    screen->projectCoords[0].clutX = 0;
    screen->projectCoords[0].clutY = 493;
    screen->projectCoords[0].pageX = 960;
    screen->projectCoords[0].pageY = 384;
    screen->projectCoords[1].clutX = 16;
    screen->projectCoords[1].clutY = 493;
    screen->projectCoords[1].pageX = 992;
    screen->projectCoords[1].pageY = 384;
    return 0;
}

/* The spots dog_place puts the dog on: the first one, and the one the placed dog keeps
 * once EVENT_FLAG_DOG_PLACED is set. */
enum DogPlace {
    DOG_PLACE_FIRST = 0,
    DOG_PLACE_PLACED = 1,
};

void dog_place(s32 place) {
    FarmPet *p = &game_state.animals.dog;
    switch (place) {
    case DOG_PLACE_FIRST:
        p->actor.place.location = PLACE_FARM;
        p->actor.position.vx = -1800;
        p->actor.position.vy = 0;
        p->actor.position.vz = 3100;
        break;
    case DOG_PLACE_PLACED:
        p->actor.place.location = PLACE_9;
        p->actor.position.vx = 230;
        p->actor.position.vy = 0;
        p->actor.position.vz = -1300;
        break;
    default:
        return;
    }
}

/* Start a request in a free slot: clear it but keep its name, stamp the item id, the
 * starting affection (10) and the clock date, and reset its cue. */
static s32 dog_request_start(FarmPet *request, s32 id) {
    s32 saved[5]; /* word-aligned room for the name */
    FarmState *farm;

    if (request->actor.kind != ITEM_NOTHING) {
        return -1;
    }
    memcpy(saved, &request->actor.name, sizeof(request->actor.name));
    memset(request, 0, sizeof(FarmPet));
    request->actor.kind = id;
    request->actor.affection = 10;
    farm = &game_state;
    memcpy(&request->actor.date, &farm->progress.clock.date.year, sizeof(request->actor.date));
    memcpy(&request->actor.name, saved, sizeof(request->actor.name));
    dog_sprite_load(request);
    cue_reset(&request->actor.cue);
    return 0;
}

typedef struct {
    u16 animation;
    u16 frame;          /* 0: keep the sprite's frame */
} CueAnimCell;
typedef struct {
    CueAnimCell cells[4];
} CueAnimEntry;

/* One entry per cue: the four (animation, frame) cells that cue_anim_lookup returns, by cell index. */
static CueAnimEntry cue_anim_table[22] = {
    {{{0, 0}, {0, 1}, {0, 2}, {0, 3}}},
    {{{1, 0}, {2, 0}, {3, 0}, {4, 0}}},
    {{{5, 0}, {6, 0}, {7, 0}, {8, 0}}},
    {{{9, 0}, {10, 0}, {11, 0}, {12, 0}}},
    {{{13, 0}, {14, 0}, {15, 0}, {16, 0}}},
    {{{17, 0}, {18, 0}, {19, 0}, {20, 0}}},
    {{{21, 0}, {22, 0}, {23, 0}, {24, 0}}},
    {{{25, 0}, {26, 0}, {27, 0}, {28, 0}}},
    {{{29, 0}, {30, 0}, {31, 0}, {32, 0}}},
    {{{33, 0}, {33, 1}, {33, 2}, {33, 3}}},
    {{{34, 0}, {35, 0}, {36, 0}, {37, 0}}},
    {{{38, 0}, {39, 1}, {40, 2}, {41, 3}}},
    {{{42, 0}, {42, 0}, {43, 0}, {43, 0}}},
    {{{46, 0}, {46, 0}, {47, 0}, {47, 0}}},
    {{{44, 0}, {44, 0}, {45, 0}, {45, 0}}},
    {{{54, 0}, {55, 0}, {56, 0}, {57, 0}}},
    {{{58, 0}, {59, 0}, {60, 0}, {61, 0}}},
    {{{62, 0}, {63, 0}, {64, 0}, {65, 0}}},
    {{{42, 0}, {43, 0}, {44, 0}, {45, 0}}},
    {{{46, 0}, {47, 0}, {48, 0}, {49, 0}}},
    {{{50, 0}, {51, 0}, {52, 0}, {53, 0}}},
    {{{66, 0}, {66, 1}, {66, 2}, {66, 3}}},
};

/* Read cell `cell` of cue `entry` into *animation and, when given, *frame. */
u32 cue_anim_lookup(u32 entry, u32 cell, u16 *animation, u16 *frame) {
    entry &= 0xFFFF;
    cell &= 0xFFFF;
    *animation = cue_anim_table[entry].cells[cell].animation;
    if (frame != 0) *frame = cue_anim_table[entry].cells[cell].frame;
    return 0;
}

/* The dog's actions: the idle phase, item and action searches, its cues and scripted walks. */

static s32 dog_start_action_search(FarmPet *object);
static u32 dog_update_position(FarmActor *object);

/* How far around the dog a ball is looked for when a request is accepted. */
#define DOG_FETCH_RADIUS 2000

/* Pick a farm object's next idle phase: 6 when an event was submitted, 2 at
 * night, otherwise a random pick weighted by the object's ACTOR_FLAG_APPROACHING_PLAYER
 * and ACTOR_FLAG_ADULT flags. Switch to it unless the object is already in that phase. */
s32 dog_pick_idle_phase(FarmPet *object) {
    s32 mode;
    s32 fallback;
    s32 range;

    if (dog_start_action_search(object) == 1) {
        mode = DOG_PHASE_WALK_TO_ACTION;
    } else if ((u32)((u16)game_state.progress.clock.time.hm.hour - FARM_DAY_HOUR_FIRST) >= FARM_DAY_HOUR_COUNT) {
        mode = DOG_PHASE_NIGHT;
    } else if (object->actor.taskNo == 0xF) {
        mode = 1;
    } else {
        if (object->actor.flags & ACTOR_FLAG_APPROACHING_PLAYER) {
            range = (object->actor.flags & ACTOR_FLAG_ADULT) ? 10 : 13;
            fallback = DOG_PHASE_START_ACTION;
        } else {
            range = (object->actor.flags & ACTOR_FLAG_ADULT) ? 6 : 8;
            fallback = DOG_PHASE_IDLE_ACTION;
        }
        switch (rand_range(range)) {
        case 1:
            mode = 8;
            break;
        case 2:
            mode = 10;
            break;
        case 3:
            mode = 12;
            break;
        case 4:
            mode = 13;
            break;
        case 5:
            mode = (object->actor.flags & ACTOR_FLAG_ADULT) == 0 ? 14 : 11;
            break;
        case 6:
            mode = 15;
            if (!(object->actor.flags & ACTOR_FLAG_ADULT)) {
                break;
            }
            /* fall through */
        case 7:
            mode = 16;
            if (!(object->actor.flags & ACTOR_FLAG_ADULT)) {
                break;
            }
            /* fall through */
        case 0:
        default:
            mode = fallback;
            break;
        }
    }
    if (mode != object->actor.taskNo) {
        dog_set_phase(object, mode);
    }
    return 0;
}

/* Find the first livestock on this map that the dog is not already walking (FARM_TASK_DOG_WALK)
 * and that lies within radius of pos in x and z. On a hit, returns 1 and reports the
 * animal and whether it lacks ACTOR_FLAG_BIT16. */
static s32 dog_find_animal(VECTOR pos, s16 map, s32 radius, s32 *unflagged, FarmLivestock **found_item) {
    FarmLivestock *item;
    s32 found;
    s32 i;
    s32 left;
    s32 right;
    s32 min_z;
    s32 max_z;

    found = 0;
    left = pos.vx - radius;
    right = pos.vx + radius;
    min_z = pos.vz - radius;
    max_z = pos.vz + radius;
    for (i = 0; i < MAX_LIVESTOCK; i++) {
        item = &game_state.animals.livestock[i];
        if (item->actor.kind == ITEM_NOTHING) {
            continue;
        }
        if (map != item->actor.place.location) {
            continue;
        }
        if (item->actor.taskNo == FARM_TASK_DOG_WALK) {
            continue;
        }
        if (left < item->actor.position.vx && item->actor.position.vx < right && min_z < item->actor.position.vz && item->actor.position.vz < max_z) {
            if (item->actor.flags & ACTOR_FLAG_BIT16) {
                *unflagged = 0;
                *found_item = item;
                found = 1;
            } else {
                *unflagged = 1;
                *found_item = item;
                found = 1;
            }
            break;
        }
    }
    return found;
}

/* Find the first farm action of kind ITEM_STRAY_DOG at `location` within
 * `radius` of `position` (x/z). */
static s32 dog_find_action(VECTOR position, s16 location, s32 radius, FarmAction **out) {
    s32 i;
    s32 found = 0;
    FarmAction *actions = game_state.animals.actions;
    s32 min_x = position.vx - radius;
    s32 max_x = position.vx + radius;
    s32 min_z = position.vz - radius;
    s32 max_z = position.vz + radius;

    for (i = 0; i < FARM_ACTIONS; i++) {
        if (actions[i].actor.kind == ITEM_STRAY_DOG && location == actions[i].actor.place.location &&
            min_x < actions[i].actor.position.vx && actions[i].actor.position.vx < max_x &&
            min_z < actions[i].actor.position.vz && actions[i].actor.position.vz < max_z) {
            *out = &actions[i];
            found = 1;
            break;
        }
    }
    return found;
}

/* Select the dog's search radius from its training and find the farm action it
 * can reach, storing that action in dog_target_action (0 when none is found). Adults only. */
static s32 dog_start_action_search(FarmPet *object) {
    s32 result;
    s32 call_result;
    s32 duration;
    s16 level;

    result = 0;
    if (object->actor.kind == ITEM_NOTHING) {
        return result;
    }
    if ((object->actor.flags & ACTOR_FLAG_ADULT) == 0) {
        return result;
    }
    level = object->training;
    duration = 1600;
    if (level >= 50) {
        duration = 2000;
        if (level >= 100) {
            duration = 2800;
            if (level >= 150) {
                duration = 6000;
                if (level < 200) {
                    duration = 4000;
                }
            }
        }
    }
    call_result = dog_find_action(object->actor.position, object->actor.place.location, duration,
                                &dog_target_action);
    result = 1;
    if (call_result != 1) {
        result = 0;
        dog_target_action = 0;
    }
    return result;
}

/* Look up the ITEM_DOG_BALL farm result; if it is at `height` and lies strictly
 * within `radius` of `center` in x and z, move `object` to it. Returns 1 on a
 * hit. */
static s32 dog_move_to_result(FarmPet *object, VECTOR center, s16 height, s32 radius) {
    VECTOR found;
    s16 found_height;
    s32 min_x;
    s32 max_x;
    s32 min_z;
    s32 max_z;
    s32 hit;

    hit = 0;
    min_x = center.vx - radius;
    max_x = center.vx + radius;
    min_z = center.vz - radius;
    max_z = center.vz + radius;
    if (estate_result_select(ITEM_DOG_BALL, &found, &found_height, 0) == 0) return 0;
    if (height != found_height) return 0;
    if (min_x < found.vx && found.vx < max_x && min_z < found.vz && found.vz < max_z) {
        farm_set_destination(&object->actor, found);
        hit = 1;
    }
    return hit;
}

/* Roll for the dog's action: it walks to the player and starts the action when a roll
 * under DOG_ACTION_ROLL is below its affection plus DOG_ACTION_AFFECTION_BONUS. */
#define DOG_ACTION_ROLL 200
#define DOG_ACTION_AFFECTION_BONUS 50
s32 dog_try_action(FarmPet *object) {
    s32 roll;
    if (object->actor.kind == ITEM_NOTHING) {
        return -1;
    }
    if (farm_area.mode.place.location != object->actor.place.location) {
        return -1;
    }
    if ((object->actor.flags & ACTOR_FLAG_IN_EVENT) && (clock_date_index(game_state.progress.clock.date.season, game_state.progress.clock.date.day) == 8)) {
        return -1;
    }
    roll = rand_range(DOG_ACTION_ROLL);
    if (roll < (object->actor.affection + DOG_ACTION_AFFECTION_BONUS)) {
        if (farm_area.mode.place.location == object->actor.place.location) {
            sound_play(0x19);
        }
        farm_set_destination(&object->actor, game_state.player.position);
        object->actor.flags |= ACTOR_FLAG_TALKED_TODAY;
        dog_set_phase(object, DOG_PHASE_START_ACTION);
        return 0;
    }
    return -1;
}

/* Scold the dog: lose 10 affection (not below 0), play its sound and set the cue
 * CUE_ICON_SCOLD/0x1E. */
s32 dog_timer_tick(FarmPet *object) {
    s32 remaining;
    s32 clamped;

    if (object->actor.kind == ITEM_NOTHING) {
        return -1;
    }
    if (farm_area.mode.place.location == object->actor.place.location) {
        sound_play(0x1A);
    }
    remaining = (u16)object->actor.affection - 10;
    object->actor.affection = remaining;
    if ((s16)remaining < 0) {
        clamped = 0;
    } else {
        clamped = remaining;
    }
    object->actor.affection = clamped;
    cue_set(&object->actor.cue, CUE_ICON_SCOLD, 0x1E);
    return 0;
}

/* A trained dog can only be sent out between 6:00 and 21:00. When a dog ball lies within
 * DOG_FETCH_RADIUS, it heads for it, counts as trained today and returns 0. */
s32 dog_accept_request(FarmPet *object) {
    s32 result;
    if (object->actor.kind == ITEM_NOTHING)
        return -1;
    if (!(object->actor.flags & ACTOR_FLAG_ADULT))
        return -1;
    if ((u16)game_state.progress.clock.time.hm.hour < FARM_DAY_HOUR_FIRST || (u16)game_state.progress.clock.time.hm.hour >= FARM_DAY_HOUR_FIRST + FARM_DAY_HOUR_COUNT)
        return -1;
    result = -1;
    if (dog_move_to_result(object, object->actor.position, object->actor.place.location, DOG_FETCH_RADIUS) == 1) {
        if (farm_area.mode.place.location == object->actor.place.location)
            sound_play(0x19);
        object->actor.flags |= ACTOR_FLAG_TRAINED_TODAY;
        result = 0;
    }
    return result;
}

/* Adults only: find a livestock within the radius that training sets and walk toward it,
 * storing it in dog_target_animal. Returns 0 when one is found, -1 otherwise. */
s32 dog_seek_item(FarmPet *object) {
    s32 limit;
    if (object->actor.kind == ITEM_NOTHING) return -1;
    if ((object->actor.flags & ACTOR_FLAG_ADULT) == 0) return -1;
    limit = 1200;
    if (object->training >= 50) {
        limit = 1600;
        if (object->training >= 100) {
            limit = 2000;
            if (object->training >= 150) {
                limit = 4000;
                if (object->training < 200) limit = 2800;
            }
        }
    }
    dog_target_animal = 0;
    if (dog_find_animal(object->actor.position, object->actor.place.location, limit, &dog_target_unflagged, &dog_target_animal) == 1) {
        farm_set_destination(&object->actor, dog_target_animal->actor.position);
        return 0;
    }
    return -1;
}

/* Walk to the action that dog_start_action_search found. Returns -1 when there is none. */
s32 dog_walk_to_action(FarmPet *object) {
    FarmAction *action;
    if (object->actor.kind == ITEM_NOTHING) return -1;
    action = dog_target_action;
    if (action != 0) {
        farm_set_destination(&object->actor, action->actor.position);
        return 0;
    }
    return -1;
}

/* Set ACTOR_FLAG_BIT2, which adds 2 affection at the daily tick. The first time, the dog
 * shows cue 0x25/0x2D. */
s32 dog_set_bit2_cue(FarmPet *object) {
    if (object->actor.kind == ITEM_NOTHING) {
        return -1;
    }
    if (!(object->actor.flags & ACTOR_FLAG_BIT2)) {
        cue_set(&object->actor.cue, 0x25, 0x2D);
    }
    object->actor.flags |= ACTOR_FLAG_BIT2;
    return 0;
}

FARM_CUE_RUNNER(dog_idle_cue, {0, 30}, {-1, 0})

FARM_CUE_RUNNER_NO_TIMER(dog_cue_2, {0, 10}, {7, 10}, {2, 0}, {-1, 0})

FARM_CUE_RUNNER(dog_cue_1, {7, 10}, {0, 10}, {-1, 0})

FARM_CUE_RUNNER(dog_cue_8, {5, 90}, {-1, 0})

FARM_CUE_RUNNER(dog_cue_10, {6, 10}, {-1, 0})

FARM_CUE_RUNNER(dog_cue_11, {17, 60}, {-1, 0})

FARM_CUE_RUNNER(dog_cue_12, {7, 10}, {8, 60}, {-1, 0})

FARM_CUE_RUNNER(dog_cue_13, {9, 30}, {10, 90}, {9, 30}, {-1, 0})

static s16 dog_drive_timer;
static s16 dog_follow_timer;
static s16 dog_approach_timer;
/* How far the dog's approach to the player has got: armed, near the player, at the player, following. */
enum DogApproachStage {
    DOG_STAGE_ARMED = 0,
    DOG_STAGE_NEAR = 1,
    DOG_STAGE_AT_PLAYER = 2,
    DOG_STAGE_FOLLOWING = 3,
};

static s16 dog_approach_stage;
FarmLivestock *dog_target_animal;
s32 dog_target_unflagged;
FarmAction *dog_target_action;
s32 dog_action_row;

static FarmCueStep dog_drive_script[3] = {
    {0x00, 0x12C}, {0x10, 0x3C}, {-1, 0x00},
};

/* One frame of the dog's drive script. Task 0 arms it, -1 tears it down and
 * reports 1 so the caller drops it. While ACTOR_FLAG_APPROACHING_PLAYER is set the dog
 * walks to the target animal; otherwise the animal is sent toward the player. A sound
 * plays on the first step. */
s32 dog_script_drive_object(FarmActor *object) {
    s16 state;
    s32 result;

    state = object->taskId;
    result = 0;
    switch (state) {
    case CUE_STEP_START:
        object->taskId = CUE_STEP_PLAY;
        object->flags |= ACTOR_FLAG_CUE_RUNNING;
        dog_drive_timer = 0;
        break;
    case CUE_STEP_STOP:
        result = 1;
        object->interruptTime = 300;
        object->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        object->flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
        break;
    case CUE_STEP_PLAY:
    default:
        if (farm_cue_step_run(object, dog_drive_script, &object->taskId, -1, &dog_drive_timer) != 0) {
            object->taskId = CUE_STEP_STOP;
        }
        if (object->flags & ACTOR_FLAG_APPROACHING_PLAYER) {
            object->destination.vx = dog_target_animal->actor.position.vx;
            object->destination.vy = dog_target_animal->actor.position.vy;
            object->destination.vz = dog_target_animal->actor.position.vz;
            object->destination.pad = dog_target_animal->actor.position.pad;
            dog_start_action(object);
            if (object->taskId != CUE_STEP_PLAY) {
                object->taskId = CUE_STEP_STOP;
            }
        } else {
            if (dog_target_unflagged != 0) {
                livestock_action_dispatch(dog_target_animal, LIVESTOCK_STATE_APPROACH_PLAYER);
            } else {
                livestock_action_dispatch_b(dog_target_animal, LIVESTOCK_STATE_APPROACH_PLAYER);
            }
            if (object->taskId == CUE_STEP_PLAY) {
                dog_drive_timer = 0;
                object->taskId = object->taskId + 1;
                if (farm_area.mode.place.location == object->place.location) {
                    sound_play(0x19);
                }
            }
            farm_mover_set_facing(object);
            farm_vector_adjust(4, &object->speed, 0);
        }
        object->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    }
    return result;
}

static FarmCueStep dog_follow_script[3] = {
    {0x00, 0x12C}, {0x10, 0x1E}, {-1, 0x00},
};

/* One frame of the dog's follow script. Task 0 arms it, -1 tears it down and
 * reports 1 so the caller drops it. While it runs, the dog either follows the
 * target action (ACTOR_FLAG_APPROACHING_PLAYER) or, on the first step, plays a sound and
 * starts the countdown on the target action. */
s32 dog_script_follow_motion(FarmPet *dog) {
    s16 state;
    s32 result;
    s32 step;

    state = dog->actor.taskId;
    result = 0;
    switch (state) {
    case CUE_STEP_START:
        dog->actor.taskId = CUE_STEP_PLAY;
        dog->actor.flags |= ACTOR_FLAG_CUE_RUNNING;
        dog_follow_timer = 0;
        break;
    case CUE_STEP_STOP:
        result = 1;
        dog->actor.interruptTime = 300;
        dog->actor.flags &= ~ACTOR_FLAG_CUE_RUNNING;
        dog->actor.flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
        dog_set_phase(dog, DOG_PHASE_IDLE);
        break;
    case CUE_STEP_PLAY:
    default:
        if (farm_cue_step_run(&dog->actor, dog_follow_script, &dog->actor.taskId, -1, &dog_follow_timer) != 0) {
            dog->actor.taskId = CUE_STEP_STOP;
        }
        if (dog->actor.flags & ACTOR_FLAG_APPROACHING_PLAYER) {
            dog->actor.destination.vx = dog_target_action->actor.position.vx;
            dog->actor.destination.vy = dog_target_action->actor.position.vy;
            dog->actor.destination.vz = dog_target_action->actor.position.vz;
            dog->actor.destination.pad = dog_target_action->actor.position.pad;
            dog_start_action(&dog->actor);
            if (dog->actor.taskId != CUE_STEP_PLAY) {
                dog->actor.taskId = CUE_STEP_STOP;
            }
        } else {
            if (dog->actor.taskId == CUE_STEP_PLAY) {
                if (farm_area.mode.place.location == dog->actor.place.location) {
                    sound_play(0x19);
                }
                step = dog->training;
                farm_action_set_countdown(((step / 10) & 0xffff), dog->actor.facing);
                farm_actor_set_task(&dog_target_action->actor, FARM_TASK_COUNTDOWN);
                dog_follow_timer = 0;
                dog->actor.taskId = dog->actor.taskId + 1;
            }
            farm_mover_set_facing(&dog->actor);
            farm_vector_adjust(4, &dog->actor.speed, 0);
        }
        dog->actor.flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    }
    return result;
}

static FarmCueStep dog_approach_script[5] = {
    {0x00, 0x12C}, {0x12, 0x12C}, {0x15, 0x12C}, {0x15, 0x5A}, {-1, 0x00},
};

/* The result of dog_update_position: 0 the dog is heading for its ball, 1 it is not
 * fetching (its speed is eased), 2 there is no ball on the farm to fetch. */
enum DogBallMove {
    DOG_BALL_HEADING = 0,
    DOG_BALL_NOT_FETCHING = 1,
    DOG_BALL_NO_BALL = 2,
};

/* The steps of the dog's approach script (dog_script_approach_player). The negative
 * steps tear it down. */
enum DogApproach {
    DOG_APPROACH_HAND_OVER = -3,
    DOG_APPROACH_SUBMIT = -2,
    DOG_APPROACH_DONE = -1,
    DOG_APPROACH_ARM = 0,
    DOG_APPROACH_WAIT_NEAR = 1,
    DOG_APPROACH_WALK_TO_PLAYER = 2,
    DOG_APPROACH_FOLLOW = 3,
    DOG_APPROACH_ARRIVE = 4,
};

/* One tick of the dog's approach script. Steps: 0 arms it; 1 waits until the dog is near
 * its ball; 2 walks to the player; 3 follows; 4 checks for arrival. -2 stamps the dog-ball
 * marker and ends; -1 ends; -3 hands over to dog_start_action while jumping, else ends. */
s32 dog_script_approach_player(FarmActor *object) {
    u32 jumping;
    s32 result;
    s16 state;

    jumping = object->flags & ACTOR_FLAG_JUMPING;
    result = 0;
    switch (object->taskId) {
    case DOG_APPROACH_ARM:
        object->taskId = DOG_APPROACH_WAIT_NEAR;
        object->flags |= ACTOR_FLAG_CUE_RUNNING;
        dog_approach_timer = 0;
        dog_approach_stage = DOG_STAGE_ARMED;
        break;
    case DOG_APPROACH_HAND_OVER:
        if (object->flags & ACTOR_FLAG_JUMPING) {
            dog_action_row = DOG_ROW_TO_PLAYER;
            dog_start_action(object);
            break;
        }
        /* fall through */
    case DOG_APPROACH_SUBMIT:
        dog_snapshot_submit(object);
        /* fall through */
    case DOG_APPROACH_DONE:
        result = 1;
        object->interruptTime = 300;
        object->flags &= ~ACTOR_FLAG_CUE_RUNNING;
        object->flags &= ~ACTOR_FLAG_APPROACHING_PLAYER;
        break;
    case DOG_APPROACH_WAIT_NEAR:
    default:
        if (farm_cue_step_run(object, dog_approach_script, &object->taskId, -1, &dog_approach_timer) != 0) {
            object->taskId = DOG_APPROACH_SUBMIT;
        }
        state = object->taskId;
        switch (state) {
        case DOG_APPROACH_WAIT_NEAR:
            switch (dog_update_position(object)) {
            case DOG_BALL_HEADING:
                break;
            case DOG_BALL_NOT_FETCHING:
                object->taskId++;
                dog_approach_stage = DOG_STAGE_NEAR;
                break;
            case DOG_BALL_NO_BALL:
                object->taskId = DOG_APPROACH_DONE;
                break;
            }
            break;
        case DOG_APPROACH_WALK_TO_PLAYER:
            if ((u16)(dog_approach_stage - 1) >= 2) {
                object->taskId = DOG_APPROACH_DONE;
                break;
            }
            dog_action_row = DOG_ROW_TO_PLAYER;
            estate_field_pickup(&game_state.runs.records[ESTATE_RUN_STAMPS][ESTATE_STAMP_DOG_BALL], 0);
            farm_set_destination(object, game_state.player.position);
            if (object->sprite.flags & 0x300) {
                if (dog_approach_stage == DOG_STAGE_AT_PLAYER) {
                    object->taskId++;
                }
                dog_approach_stage = DOG_STAGE_AT_PLAYER;
            }
            break;
        case DOG_APPROACH_FOLLOW:
            if (dog_approach_stage != DOG_STAGE_AT_PLAYER) {
                object->taskId = DOG_APPROACH_DONE;
                break;
            }
            if (object->flags & ACTOR_FLAG_APPROACHING_PLAYER) {
                farm_set_destination(object, game_state.player.position);
                dog_action_row = DOG_ROW_TO_PLAYER;
                dog_start_action(object);
                break;
            }
            if (jumping == 0) {
                farm_vector_adjust(4, &object->speed, 0);
            }
            object->taskId++;
            dog_approach_timer = 0;
            dog_approach_stage = DOG_STAGE_FOLLOWING;
            if (farm_area.mode.place.location == object->place.location) {
                sound_play(0x19);
            }
            break;
        case DOG_APPROACH_ARRIVE:
            if (dog_approach_stage != DOG_STAGE_FOLLOWING || jumping != 0) {
                object->taskId = DOG_APPROACH_HAND_OVER;
            }
            break;
        }
        object->flags |= ACTOR_FLAG_CUE_RUNNING;
        break;
    }
    return result;
}

FARM_CUE_RUNNER(dog_cue_14, {7, 30}, {14, 90}, {7, 30}, {-1, 0})

FARM_CUE_RUNNER(dog_cue_15, {0, 30}, {12, 90}, {0, 30}, {-1, 0})

FARM_CUE_RUNNER(dog_cue_16, {7, 30}, {13, 90}, {7, 30}, {-1, 0})

/* The walk speeds of a dog that is not yet adult, of an adult one, and of its idle walk. */
#define DOG_WALK_SPEED_YOUNG 24
#define DOG_WALK_SPEED_ADULT 36
#define DOG_WALK_SPEED_IDLE 12

/* Walk the dog on its idle walk at DOG_WALK_SPEED_IDLE: clear ACTOR_FLAG_ADULT_SPRITE, set
 * the rest or moving action by its speed, and end the cue. Returns 1. */
s32 dog_set_idle_action(FarmActor *object) {
    object->flags &= ~ACTOR_FLAG_ADULT_SPRITE;
    farm_walker_move(object, DOG_WALK_SPEED_IDLE, 0x31);
    if (object->speed.vx == 0 && object->speed.vy == 0 && object->speed.vz == 0)
        object->action = ACTOR_ACTION_REST;
    else
        object->action = ACTOR_ACTION_MOVING;
    object->flags &= ~ACTOR_FLAG_CUE_RUNNING;
    return 1;
}

/* Rows: the dog's own walks (DOG_ROW_OWN) and the walks it makes to the player. Columns: the
 * pose at rest (with sprite flags 0x300), the walking pose, and the pose while jumping. */
static unsigned short dog_action_table[2][3] = {{0x0000, 0x0004, 0x000F}, {0x0015, 0x0013, 0x0014}};

/* Start the dog's walk for a farm action: pick its pose from the cue row, set the walk
 * speed, and set the idle timer unless it is heading for a ball. */
s32 dog_start_action(FarmActor *object) {
    s32 flags;
    s32 mode;
    u16 *table;

    mode = DOG_WALK_SPEED_YOUNG;
    table = dog_action_table[DOG_ROW_OWN];
    if (dog_action_row != DOG_ROW_OWN) {
        table = dog_action_table[DOG_ROW_TO_PLAYER];
    }

    flags = object->flags;
    if (flags & ACTOR_FLAG_ADULT) {
        object->flags |= ACTOR_FLAG_ADULT_SPRITE;
        mode = DOG_WALK_SPEED_ADULT;
    }
    farm_walker_move(object, mode, 0x31);
    object->flags |= ACTOR_FLAG_CUE_RUNNING;

    if (object->speed.vx == 0 && object->speed.vy == 0 && object->speed.vz == 0) {
        if (object->sprite.flags & 0x300) {
            object->action = table[0];
        } else {
            object->action = table[1];
        }
    } else if (object->flags & ACTOR_FLAG_JUMPING) {
        object->action = table[2];
    } else {
        object->action = table[1];
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

/* A dog that is fetching heads for its ball. Otherwise its speed is eased. */
static u32 dog_update_position(FarmActor *object) {
    VECTOR position;
    s16 height;

    if (object->flags & ACTOR_FLAG_APPROACHING_PLAYER) {
        if (estate_result_select(ITEM_DOG_BALL, &position, &height, 0) == 0) {
            return DOG_BALL_NO_BALL;
        }
        farm_set_destination(object, position);
        dog_start_action(object);
        return DOG_BALL_HEADING;
    }
    farm_vector_adjust(4, &object->speed, 0);
    return DOG_BALL_NOT_FETCHING;
}

/* Stamp a dog-ball marker at the actor's position. */
void dog_snapshot_submit(FarmActor *source) {
    FarmActor copy;

    copy = *source;
    estate_marker_stamp(ITEM_DOG_BALL, copy.position, copy.place.location, 0);
}
