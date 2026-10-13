#ifndef HORSE_H
#define HORSE_H

/* The horse: its records, actions and cues. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "actor.h"
#include "dog.h"
#include "player.h"

void horse_sprite_refresh(FarmActor *object);
void horse_pair_at_38(FarmPet *obj);
FarmPet *horse_spawn(s32 kind);
s32 horse_update_bond(FarmPet *object);
s32 horse_dispatch_tick(FarmActor *object);
s32 horse_set_state(FarmActor *object, s32 state);
void horse_clear_record(FarmPet *dst);
void horse_load_sprite(void);
s32 horse_idle_cue(FarmActor *self);
s32 horse_cue_2(FarmActor *self);
s32 horse_cue_1(FarmActor *self);
void horse_move_to_player(FarmPlayer *player);
void horse_set_anim_from_tool(FarmPlayer *player);


#endif /* HORSE_H */
