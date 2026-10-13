#ifndef CHICKEN_H
#define CHICKEN_H

/* Chickens: their slots, counts, cues and the overnight update. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "actor.h"

/* One of the ten chickens. */
typedef struct FarmChicken {
    FarmActor actor;    /* kind ITEM_CHICKEN or ITEM_CHICK */
    u16 grazingTime;    /* 0..31 */
    u16 daysSick;       /* 0..31 */
    u16 pregnancy;      /* 0..31 */
    s16 unusedValue;
} FarmChicken;

void chicken_pair_at_38(FarmChicken *obj);
FarmChicken *chicken_spawn(s32 kind, s32 requested, s32 mode);
s32 chicken_weather_tick(FarmChicken *object);
s32 chicken_daily_tick(FarmChicken *chicken);
s32 chicken_timer_tick(FarmChicken *object);
s32 farm_actor_dispatch_state(FarmActor *object, s32 state);
void chicken_clear(FarmChicken *p);
s32 chicken_index(FarmChicken *p);
s32 chicken_pick_next_state(FarmChicken *object);
void chicken_place(FarmChicken *chicken);
s32 farm_chicken_follow_player(FarmActor *object);
s32 farm_chicken_tick_offset(FarmActor *object);
s32 farm_chicken_clear_sick(FarmChicken *chicken);
s32 farm_chicken_action_none(void);
s32 farm_chicken_cue_run(FarmActor *self);
s32 chicken_cue_1(FarmActor *self);
s32 chicken_cue_2(FarmActor *self);
s32 chicken_cue_walk(FarmActor *self);
s32 chicken_cue_walk_quick(FarmActor *self);
s32 chicken_cue_sound(FarmActor *self);
s32 chicken_walk_state(FarmActor *p);
s32 chicken_sick_cue(FarmActor *self);

s32 farm_anim_lookup(u16 row, u16 column, u16 *first, u16 *second);

/* Handlers that reset an object's result, by state. */
extern s32 (*const farm_actor_cue_handlers[])(FarmActor *);

#endif /* CHICKEN_H */
