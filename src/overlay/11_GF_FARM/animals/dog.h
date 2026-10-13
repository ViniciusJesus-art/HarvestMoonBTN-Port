#ifndef DOG_H
#define DOG_H

/* The dog: its records, actions and cues. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "actor.h"
#include "anim.h"
#include "animal_actions.h"
#include "livestock.h"

/* The dog and the horse. */
typedef struct FarmPet {
    FarmActor actor;    /* the dog's item (ITEM_ADULT_DOG / ITEM_YOUNG_DOG), or the horse's (ITEM_PONY / ITEM_HORSE) */
    s16 training;       /* 0..255: the dog's intelligence or the horse's stamina */
    u8  padAE[0xE0 - 0xAE];
    VramPlace projectCoords[2];    /* the two places the pet's sprite parts draw from */
} FarmPet;

/* The dog's phases: the taskNo it switches to. Each phase has an enter handler and a
 * per-frame cue. */
enum DogPhase {
    DOG_PHASE_IDLE = 0,             /* the idle cue; the phase resets to it */
    DOG_PHASE_NIGHT = 2,            /* idle at night (hour outside 6..20) */
    DOG_PHASE_HIT = 3,              /* hit by the player's target */
    DOG_PHASE_TRY_ACTION = 4,       /* the farm-mode check of the dog */
    DOG_PHASE_SEEK_ITEM = 5,        /* the livestock check: seek a livestock to drive */
    DOG_PHASE_WALK_TO_ACTION = 6,   /* an event was submitted: walk to its action */
    DOG_PHASE_FETCH = 7,            /* a thrown dog ball: heads for it */
    DOG_PHASE_PLACED = 9,           /* a held dog is put down */
    DOG_PHASE_START_ACTION = 0x11,  /* the action roll passed */
    DOG_PHASE_IDLE_ACTION = 0x12,   /* a plain idle walk */
};

FarmPet *dog_start(s32 kind, s32 enabled);
s32 dog_tick(FarmPet *object);
s32 dog_update_phase(FarmPet *object);
s32 dog_set_phase(FarmPet *object, s32 mode);
s32 dog_sprite_load(FarmPet *screen);
void dog_place(s32 place);
s32 dog_pick_idle_phase(FarmPet *object);
s32 dog_try_action(FarmPet *object);
s32 dog_timer_tick(FarmPet *object);
s32 dog_accept_request(FarmPet *object);
s32 dog_seek_item(FarmPet *object);
s32 dog_walk_to_action(FarmPet *arg);
s32 dog_set_bit2_cue(FarmPet *p);
s32 dog_idle_cue(FarmActor *self);
s32 dog_cue_2(FarmActor *self);
s32 dog_cue_1(FarmActor *self);
s32 dog_cue_8(FarmActor *self);
s32 dog_cue_10(FarmActor *self);
s32 dog_cue_11(FarmActor *self);
s32 dog_cue_12(FarmActor *self);
s32 dog_cue_13(FarmActor *self);
s32 dog_script_drive_object(FarmActor *o);
s32 dog_script_follow_motion(FarmPet *o);
s32 dog_script_approach_player(FarmActor *object);
s32 dog_cue_14(FarmActor *self);
s32 dog_cue_15(FarmActor *self);
s32 dog_cue_16(FarmActor *self);
s32 dog_set_idle_action(FarmActor *p);
s32 dog_start_action(FarmActor *object);
void dog_snapshot_submit(FarmActor *source);

void pet_refresh_sprite(FarmPet *object);
u32 cue_anim_lookup(u32 entry, u32 cell, u16 *animation, u16 *frame);
void cue_pair_at_38(FarmPet *obj);

/* The rows of dog_action_table: the dog's own walks, and the walks it makes when it is brought to the player. */
enum DogActionRow {
    DOG_ROW_OWN = 0,
    DOG_ROW_TO_PLAYER = 1,
};

extern s32 dog_action_row;
extern FarmAction *dog_target_action;
/* The dog's target: an animal near it, the action found for it, and whether its flag is clear. */
extern FarmLivestock *dog_target_animal;
extern s32 dog_target_unflagged;

#endif /* DOG_H */
